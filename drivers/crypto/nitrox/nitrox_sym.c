/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>

#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>

#include "nitrox_sym.h"
#include "nitrox_device.h"
#include "nitrox_logs.h"

#define CRYPTODEV_NAME_NITROX_PMD crypto_nitrox_sym

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

static int
nitrox_sym_dev_qp_release(struct rte_cryptodev *cdev, uint16_t qp_id)
{
	RTE_SET_USED(cdev);
	RTE_SET_USED(qp_id);
	return 0;
}

static struct rte_cryptodev_ops nitrox_cryptodev_ops = {
	.dev_configure		= nitrox_sym_dev_config,
	.dev_start		= nitrox_sym_dev_start,
	.dev_stop		= nitrox_sym_dev_stop,
	.dev_close		= nitrox_sym_dev_close,
	.dev_infos_get		= nitrox_sym_dev_info_get,
	.stats_get		= NULL,
	.stats_reset		= NULL,
	.queue_pair_setup	= NULL,
	.queue_pair_release     = NULL,
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
