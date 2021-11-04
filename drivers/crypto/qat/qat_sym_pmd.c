/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <cryptodev_pmd.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security_driver.h>
#endif

#include "qat_logs.h"
#include "qat_crypto.h"
#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_sym_pmd.h"

#define MIXED_CRYPTO_MIN_FW_VER 0x04090000

uint8_t qat_sym_driver_id;

static const struct rte_cryptodev_capabilities qat_gen1_sym_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities qat_gen2_sym_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	QAT_EXTRA_GEN2_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities qat_gen3_sym_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	QAT_EXTRA_GEN2_SYM_CAPABILITIES,
	QAT_EXTRA_GEN3_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities qat_gen4_sym_capabilities[] = {
	QAT_BASE_GEN4_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

#ifdef RTE_LIB_SECURITY
static const struct rte_cryptodev_capabilities
					qat_security_sym_capabilities[] = {
	QAT_SECURITY_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability qat_security_capabilities[] = {
	QAT_SECURITY_CAPABILITIES(qat_security_sym_capabilities),
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};
#endif

static struct rte_cryptodev_ops crypto_qat_ops = {

		/* Device related operations */
		.dev_configure		= qat_cryptodev_config,
		.dev_start		= qat_cryptodev_start,
		.dev_stop		= qat_cryptodev_stop,
		.dev_close		= qat_cryptodev_close,
		.dev_infos_get		= qat_cryptodev_info_get,

		.stats_get		= qat_cryptodev_stats_get,
		.stats_reset		= qat_cryptodev_stats_reset,
		.queue_pair_setup	= qat_cryptodev_qp_setup,
		.queue_pair_release	= qat_cryptodev_qp_release,

		/* Crypto related operations */
		.sym_session_get_size	= qat_sym_session_get_private_size,
		.sym_session_configure	= qat_sym_session_configure,
		.sym_session_clear	= qat_sym_session_clear,

		/* Raw data-path API related operations */
		.sym_get_raw_dp_ctx_size = qat_sym_get_dp_ctx_size,
		.sym_configure_raw_dp_ctx = qat_sym_configure_dp_ctx,
};

#ifdef RTE_LIB_SECURITY
static const struct rte_security_capability *
qat_security_cap_get(void *device __rte_unused)
{
	return qat_security_capabilities;
}

static struct rte_security_ops security_qat_ops = {

		.session_create = qat_security_session_create,
		.session_update = NULL,
		.session_stats_get = NULL,
		.session_destroy = qat_security_session_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = qat_security_cap_get
};
#endif

void
qat_sym_init_op_cookie(void *op_cookie)
{
	struct qat_sym_op_cookie *cookie = op_cookie;

	cookie->qat_sgl_src_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_src);

	cookie->qat_sgl_dst_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_dst);

	cookie->opt.spc_gmac.cd_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			opt.spc_gmac.cd_cipher);
}

static uint16_t
qat_sym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, (void **)ops, nb_ops);
}

static uint16_t
qat_sym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops, nb_ops);
}

/* An rte_driver is needed in the registration of both the device and the driver
 * with cryptodev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the crypto part of the pci device.
 */
static const char qat_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD);
static const struct rte_driver cryptodev_qat_sym_driver = {
	.name = qat_sym_drv_name,
	.alias = qat_sym_drv_name
};

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param __rte_unused)
{
	int i = 0, ret = 0;
	struct qat_device_info *qat_dev_instance =
			&qat_pci_devs[qat_pci_dev->qat_dev_id];

	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_cryptodev_private)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_cryptodev_private *internals;
	const struct rte_cryptodev_capabilities *capabilities;
	uint64_t capa_size;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "sym");
	QAT_LOG(DEBUG, "Creating QAT SYM device %s", name);

	/*
	 * All processes must use same driver id so they can share sessions.
	 * Store driver_id so we can validate that all processes have the same
	 * value, typically they have, but could differ if binaries built
	 * separately.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		qat_pci_dev->qat_sym_driver_id =
				qat_sym_driver_id;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (qat_pci_dev->qat_sym_driver_id !=
				qat_sym_driver_id) {
			QAT_LOG(ERR,
				"Device %s have different driver id than corresponding device in primary process",
				name);
			return -(EFAULT);
		}
	}

	/* Populate subset device to use in cryptodev device creation */
	qat_dev_instance->sym_rte_dev.driver = &cryptodev_qat_sym_driver;
	qat_dev_instance->sym_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->sym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_dev_instance->sym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_dev_instance->sym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = qat_sym_driver_id;
	cryptodev->dev_ops = &crypto_qat_ops;

	cryptodev->enqueue_burst = qat_sym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_sym_pmd_dequeue_op_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED;

	if (qat_pci_dev->qat_dev_gen < QAT_GEN4)
		cryptodev->feature_flags |= RTE_CRYPTODEV_FF_SYM_RAW_DP;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	snprintf(capa_memz_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"QAT_SYM_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

#ifdef RTE_LIB_SECURITY
	struct rte_security_ctx *security_instance;
	security_instance = rte_malloc("qat_sec",
				sizeof(struct rte_security_ctx),
				RTE_CACHE_LINE_SIZE);
	if (security_instance == NULL) {
		QAT_LOG(ERR, "rte_security_ctx memory alloc failed");
		ret = -ENOMEM;
		goto error;
	}

	security_instance->device = (void *)cryptodev;
	security_instance->ops = &security_qat_ops;
	security_instance->sess_cnt = 0;
	cryptodev->security_ctx = security_instance;
	cryptodev->feature_flags |= RTE_CRYPTODEV_FF_SECURITY;
#endif

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	internals->service_type = QAT_SERVICE_SYMMETRIC;

	internals->dev_id = cryptodev->data->dev_id;
	switch (qat_pci_dev->qat_dev_gen) {
	case QAT_GEN1:
		capabilities = qat_gen1_sym_capabilities;
		capa_size = sizeof(qat_gen1_sym_capabilities);
		break;
	case QAT_GEN2:
		capabilities = qat_gen2_sym_capabilities;
		capa_size = sizeof(qat_gen2_sym_capabilities);
		break;
	case QAT_GEN3:
		capabilities = qat_gen3_sym_capabilities;
		capa_size = sizeof(qat_gen3_sym_capabilities);
		break;
	case QAT_GEN4:
		capabilities = qat_gen4_sym_capabilities;
		capa_size = sizeof(qat_gen4_sym_capabilities);
		break;
	default:
		QAT_LOG(DEBUG,
			"QAT gen %d capabilities unknown",
			qat_pci_dev->qat_dev_gen);
		ret = -(EINVAL);
		goto error;
	}

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
		capa_size,
		rte_socket_id(), 0);
	}
	if (internals->capa_mz == NULL) {
		QAT_LOG(DEBUG,
			"Error allocating memzone for capabilities, destroying "
			"PMD for %s",
			name);
		ret = -EFAULT;
		goto error;
	}

	memcpy(internals->capa_mz->addr, capabilities, capa_size);
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	while (1) {
		if (qat_dev_cmd_param[i].name == NULL)
			break;
		if (!strcmp(qat_dev_cmd_param[i].name, SYM_ENQ_THRESHOLD_NAME))
			internals->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		i++;
	}

	qat_pci_dev->sym_dev = internals;
	QAT_LOG(DEBUG, "Created QAT SYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);

	rte_cryptodev_pmd_probing_finish(cryptodev);

	return 0;

error:
#ifdef RTE_LIB_SECURITY
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
#endif
	rte_cryptodev_pmd_destroy(cryptodev);
	memset(&qat_dev_instance->sym_rte_dev, 0,
		sizeof(qat_dev_instance->sym_rte_dev));

	return ret;
}

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->sym_dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->sym_dev->capa_mz);

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(qat_pci_dev->sym_dev->dev_id);
#ifdef RTE_LIB_SECURITY
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
#endif
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].sym_rte_dev.name = NULL;
	qat_pci_dev->sym_dev = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_sym_driver,
		qat_sym_driver_id);
