/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <bus_vdev_driver.h>
#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_errno.h>

#include "ipsec_mb_private.h"

RTE_DEFINE_PER_LCORE(IMB_MGR *, mb_mgr);

struct ipsec_mb_internals ipsec_mb_pmds[IPSEC_MB_N_PMD_TYPES];
int ipsec_mb_logtype_driver;
enum ipsec_mb_vector_mode vector_mode;

/**
 * Generic burst enqueue, place crypto operations on ingress queue for
 * processing.
 *
 * @param __qp         Queue Pair to process
 * @param ops          Crypto operations for processing
 * @param nb_ops       Number of crypto operations for processing
 *
 * @return
 * - Number of crypto operations enqueued
 */
static uint16_t
ipsec_mb_enqueue_burst(void *__qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct ipsec_mb_qp *qp = __qp;

	unsigned int nb_enqueued;

	nb_enqueued = rte_ring_enqueue_burst(qp->ingress_queue,
			(void **)ops, nb_ops, NULL);

	qp->stats.enqueued_count += nb_enqueued;
	qp->stats.enqueue_err_count += nb_ops - nb_enqueued;

	return nb_enqueued;
}

static int
ipsec_mb_mp_request_register(void)
{
	RTE_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	return rte_mp_action_register(IPSEC_MB_MP_MSG,
				ipsec_mb_ipc_request);
}

static void
ipsec_mb_mp_request_unregister(void)
{
	RTE_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_mp_action_unregister(IPSEC_MB_MP_MSG);
}

int
ipsec_mb_create(struct rte_vdev_device *vdev,
	enum ipsec_mb_pmd_types pmd_type)
{
	struct rte_cryptodev *dev;
	struct ipsec_mb_dev_private *internals;
	struct ipsec_mb_internals *pmd_data = &ipsec_mb_pmds[pmd_type];
	struct rte_cryptodev_pmd_init_params init_params = {};
	const char *name, *args;
	int retval;

#if defined(RTE_ARCH_ARM)
	if ((pmd_type != IPSEC_MB_PMD_TYPE_SNOW3G) &&
		(pmd_type != IPSEC_MB_PMD_TYPE_ZUC))
		return -ENOTSUP;
#endif

#if defined(RTE_ARCH_ARM64)
	vector_mode = IPSEC_MB_ARM64;
#elif defined(RTE_ARCH_X86_64)
	if (vector_mode == IPSEC_MB_NOT_SUPPORTED) {
		/* Check CPU for supported vector instruction set */
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F))
			vector_mode = IPSEC_MB_AVX512;
		else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
			vector_mode = IPSEC_MB_AVX2;
		else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
			vector_mode = IPSEC_MB_AVX;
		else
			vector_mode = IPSEC_MB_SSE;
	}
#else
	/* Unsupported architecture */
	return -ENOTSUP;
#endif

	init_params.private_data_size = sizeof(struct ipsec_mb_dev_private) +
		pmd_data->internals_priv_size;
	init_params.max_nb_queue_pairs =
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS;
	init_params.socket_id = rte_socket_id();

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	args = rte_vdev_device_args(vdev);

	retval = rte_cryptodev_pmd_parse_input_args(&init_params, args);
	if (retval) {
		IPSEC_MB_LOG(
		    ERR, "Failed to parse initialisation arguments[%s]", args);
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_create(name, &vdev->device, &init_params);
	if (dev == NULL) {
		IPSEC_MB_LOG(ERR, "driver %s: create failed",
			     init_params.name);
		return -ENODEV;
	}

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;
	internals->pmd_type = pmd_type;
	internals->max_nb_queue_pairs = init_params.max_nb_queue_pairs;

	dev->driver_id = ipsec_mb_get_driver_id(pmd_type);
	if (dev->driver_id == UINT8_MAX) {
		IPSEC_MB_LOG(ERR, "driver %s: create failed",
			     init_params.name);
		return -ENODEV;
	}
	dev->dev_ops = ipsec_mb_pmds[pmd_type].ops;
	dev->enqueue_burst = ipsec_mb_enqueue_burst;
	dev->dequeue_burst = ipsec_mb_pmds[pmd_type].dequeue_burst;
	dev->feature_flags = pmd_data->feature_flags;

	if (pmd_data->dev_config) {
		retval = (*pmd_data->dev_config)(dev);
		if (retval < 0) {
			IPSEC_MB_LOG(ERR,
				"Failed to configure device %s", name);
			rte_cryptodev_pmd_destroy(dev);
			return retval;
		}
	}

	switch (vector_mode) {
	case IPSEC_MB_AVX512:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX512;
		break;
	case IPSEC_MB_AVX2:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX2;
		break;
	case IPSEC_MB_AVX:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX;
		break;
	case IPSEC_MB_SSE:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
		break;
	default:
		break;
	}

	rte_cryptodev_pmd_probing_finish(dev);

	IPSEC_MB_LOG(INFO, "IPSec Multi-buffer library version used: %s\n",
		     imb_get_version_str());

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		retval = ipsec_mb_mp_request_register();
		if (retval && ((rte_errno == EEXIST) || (rte_errno == ENOTSUP)))
			/* Safe to proceed, return 0 */
			return 0;

		if (retval)
			IPSEC_MB_LOG(ERR,
				"IPSec Multi-buffer register MP request failed.\n");
	}
	return retval;
}

int
ipsec_mb_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;
	int qp_id;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	if (RTE_PER_LCORE(mb_mgr)) {
		free_mb_mgr(RTE_PER_LCORE(mb_mgr));
		RTE_PER_LCORE(mb_mgr) = NULL;
	}

	if (cryptodev->security_ctx) {
		rte_free(cryptodev->security_ctx);
		cryptodev->security_ctx = NULL;
	}

	for (qp_id = 0; qp_id < cryptodev->data->nb_queue_pairs; qp_id++)
		ipsec_mb_qp_release(cryptodev, qp_id);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		ipsec_mb_mp_request_unregister();

	return rte_cryptodev_pmd_destroy(cryptodev);
}
