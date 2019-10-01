/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>

#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>

#include "nitrox_sym.h"
#include "nitrox_device.h"
#include "nitrox_qp.h"
#include "nitrox_sym_reqmgr.h"
#include "nitrox_logs.h"

#define CRYPTODEV_NAME_NITROX_PMD crypto_nitrox_sym
#define NPS_PKT_IN_INSTR_SIZE 64

struct nitrox_sym_device {
	struct rte_cryptodev *cdev;
	struct nitrox_device *ndev;
};

uint8_t nitrox_sym_drv_id;
static const char nitrox_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_NITROX_PMD);
static const struct rte_driver nitrox_rte_sym_drv = {
	.name = nitrox_sym_drv_name,
	.alias = nitrox_sym_drv_name
};

static int nitrox_sym_dev_qp_release(struct rte_cryptodev *cdev,
				     uint16_t qp_id);

static int
nitrox_sym_dev_config(struct rte_cryptodev *cdev,
		      struct rte_cryptodev_config *config)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;

	if (config->nb_queue_pairs > ndev->nr_queues) {
		NITROX_LOG(ERR, "Invalid queue pairs, max supported %d\n",
			   ndev->nr_queues);
		return -EINVAL;
	}

	return 0;
}

static int
nitrox_sym_dev_start(struct rte_cryptodev *cdev)
{
	/* SE cores initialization is done in PF */
	RTE_SET_USED(cdev);
	return 0;
}

static void
nitrox_sym_dev_stop(struct rte_cryptodev *cdev)
{
	/* SE cores cleanup is done in PF */
	RTE_SET_USED(cdev);
}

static int
nitrox_sym_dev_close(struct rte_cryptodev *cdev)
{
	int i, ret;

	for (i = 0; i < cdev->data->nb_queue_pairs; i++) {
		ret = nitrox_sym_dev_qp_release(cdev, i);
		if (ret)
			return ret;
	}

	return 0;
}

static void
nitrox_sym_dev_info_get(struct rte_cryptodev *cdev,
			struct rte_cryptodev_info *info)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;

	if (!info)
		return;

	info->max_nb_queue_pairs = ndev->nr_queues;
	info->feature_flags = cdev->feature_flags;
	info->driver_id = nitrox_sym_drv_id;
	info->sym.max_nb_sessions = 0;
}

static void
nitrox_sym_dev_stats_get(struct rte_cryptodev *cdev,
			 struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = cdev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void
nitrox_sym_dev_stats_reset(struct rte_cryptodev *cdev)
{
	int qp_id;

	for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = cdev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static int
nitrox_sym_dev_qp_setup(struct rte_cryptodev *cdev, uint16_t qp_id,
			const struct rte_cryptodev_qp_conf *qp_conf,
			int socket_id)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;
	struct nitrox_qp *qp = NULL;
	int err;

	NITROX_LOG(DEBUG, "queue %d\n", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG(ERR, "queue %u invalid, max queues supported %d\n",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	if (cdev->data->queue_pairs[qp_id]) {
		err = nitrox_sym_dev_qp_release(cdev, qp_id);
		if (err)
			return err;
	}

	qp = rte_zmalloc_socket("nitrox PMD qp", sizeof(*qp),
				RTE_CACHE_LINE_SIZE,
				socket_id);
	if (!qp) {
		NITROX_LOG(ERR, "Failed to allocate nitrox qp\n");
		return -ENOMEM;
	}

	qp->qno = qp_id;
	err = nitrox_qp_setup(qp, ndev->bar_addr, cdev->data->name,
			      qp_conf->nb_descriptors, NPS_PKT_IN_INSTR_SIZE,
			      socket_id);
	if (unlikely(err))
		goto qp_setup_err;

	qp->sr_mp = nitrox_sym_req_pool_create(cdev, qp->count, qp_id,
					       socket_id);
	if (unlikely(!qp->sr_mp))
		goto req_pool_err;

	cdev->data->queue_pairs[qp_id] = qp;
	NITROX_LOG(DEBUG, "queue %d setup done\n", qp_id);
	return 0;

req_pool_err:
	nitrox_qp_release(qp, ndev->bar_addr);
qp_setup_err:
	rte_free(qp);
	return err;
}

static int
nitrox_sym_dev_qp_release(struct rte_cryptodev *cdev, uint16_t qp_id)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;
	struct nitrox_qp *qp;
	int err;

	NITROX_LOG(DEBUG, "queue %d\n", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG(ERR, "queue %u invalid, max queues supported %d\n",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	qp = cdev->data->queue_pairs[qp_id];
	if (!qp) {
		NITROX_LOG(DEBUG, "queue %u already freed\n", qp_id);
		return 0;
	}

	if (!nitrox_qp_is_empty(qp)) {
		NITROX_LOG(ERR, "queue %d not empty\n", qp_id);
		return -EAGAIN;
	}

	cdev->data->queue_pairs[qp_id] = NULL;
	err = nitrox_qp_release(qp, ndev->bar_addr);
	nitrox_sym_req_pool_free(qp->sr_mp);
	rte_free(qp);
	NITROX_LOG(DEBUG, "queue %d release done\n", qp_id);
	return err;
}

static struct rte_cryptodev_ops nitrox_cryptodev_ops = {
	.dev_configure		= nitrox_sym_dev_config,
	.dev_start		= nitrox_sym_dev_start,
	.dev_stop		= nitrox_sym_dev_stop,
	.dev_close		= nitrox_sym_dev_close,
	.dev_infos_get		= nitrox_sym_dev_info_get,
	.stats_get		= nitrox_sym_dev_stats_get,
	.stats_reset		= nitrox_sym_dev_stats_reset,
	.queue_pair_setup	= nitrox_sym_dev_qp_setup,
	.queue_pair_release     = nitrox_sym_dev_qp_release,
	.sym_session_get_size   = NULL,
	.sym_session_configure  = NULL,
	.sym_session_clear      = NULL
};

int
nitrox_sym_pmd_create(struct nitrox_device *ndev)
{
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev_pmd_init_params init_params = {
			.name = "",
			.socket_id = ndev->pdev->device.numa_node,
			.private_data_size = sizeof(struct nitrox_sym_device)
	};
	struct rte_cryptodev *cdev;

	rte_pci_device_name(&ndev->pdev->addr, name, sizeof(name));
	snprintf(name + strlen(name), RTE_CRYPTODEV_NAME_MAX_LEN, "_n5sym");
	ndev->rte_sym_dev.driver = &nitrox_rte_sym_drv;
	ndev->rte_sym_dev.numa_node = ndev->pdev->device.numa_node;
	ndev->rte_sym_dev.devargs = NULL;
	cdev = rte_cryptodev_pmd_create(name, &ndev->rte_sym_dev,
					&init_params);
	if (!cdev) {
		NITROX_LOG(ERR, "Cryptodev '%s' creation failed\n", name);
		return -ENODEV;
	}

	ndev->rte_sym_dev.name = cdev->data->name;
	cdev->driver_id = nitrox_sym_drv_id;
	cdev->dev_ops = &nitrox_cryptodev_ops;
	cdev->enqueue_burst = NULL;
	cdev->dequeue_burst = NULL;
	cdev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_HW_ACCELERATED;

	ndev->sym_dev = cdev->data->dev_private;
	ndev->sym_dev->cdev = cdev;
	ndev->sym_dev->ndev = ndev;
	NITROX_LOG(DEBUG, "Created cryptodev '%s', dev_id %d, drv_id %d\n",
		   cdev->data->name, cdev->data->dev_id, nitrox_sym_drv_id);
	return 0;
}

int
nitrox_sym_pmd_destroy(struct nitrox_device *ndev)
{
	return rte_cryptodev_pmd_destroy(ndev->sym_dev->cdev);
}

static struct cryptodev_driver nitrox_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(nitrox_crypto_drv,
		nitrox_rte_sym_drv,
		nitrox_sym_drv_id);
