/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <rte_malloc.h>

#include "zsda_logs.h"
#include "zsda_qp_common.h"
#include "zsda_comp_pmd.h"

static int
zsda_comp_xform_size(void)
{
	return RTE_ALIGN_CEIL(sizeof(struct zsda_comp_xform), 8);
}

static struct rte_mempool *
zsda_comp_xform_pool_create(struct zsda_comp_dev_private *comp_dev,
			    struct rte_compressdev_config *config,
			    uint32_t num_elements)
{
	char xform_pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(xform_pool_name, RTE_MEMPOOL_NAMESIZE, "%s_xforms",
		 comp_dev->zsda_pci_dev->name);

	ZSDA_LOG(DEBUG, "xformpool: %s", xform_pool_name);
	mp = rte_mempool_lookup(xform_pool_name);

	if (mp != NULL) {
		ZSDA_LOG(DEBUG, "xformpool already created");
		if (mp->size != num_elements) {
			ZSDA_LOG(DEBUG, "xformpool wrong size - delete it");
			rte_mempool_free(mp);
			mp = NULL;
			comp_dev->xformpool = NULL;
		}
	} else {
		mp = rte_mempool_create(xform_pool_name, num_elements,
					zsda_comp_xform_size(), 0, 0, NULL,
					NULL, NULL, NULL, config->socket_id, 0);
		if (mp == NULL) {
			ZSDA_LOG(ERR, "Failed! mp is NULL");
			return NULL;
		}
	}

	return mp;
}

static int
zsda_comp_dev_config(struct rte_compressdev *dev,
		     struct rte_compressdev_config *config)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	if (config->max_nb_priv_xforms) {
		comp_dev->xformpool = zsda_comp_xform_pool_create(
			comp_dev, config, config->max_nb_priv_xforms);
		if (comp_dev->xformpool == NULL)
			return -ENOMEM;
	} else
		comp_dev->xformpool = NULL;

	return ZSDA_SUCCESS;
}

static int
zsda_comp_dev_start(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;
	int ret;

	ret = zsda_queue_start(comp_dev->zsda_pci_dev->pci_dev);

	if (ret)
		ZSDA_LOG(ERR, "Failed! zsda_queue_start.");

	return ret;
}

static void
zsda_comp_dev_stop(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	zsda_queue_stop(comp_dev->zsda_pci_dev->pci_dev);
}

static int
zsda_comp_dev_close(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	rte_mempool_free(comp_dev->xformpool);
	comp_dev->xformpool = NULL;

	return ZSDA_SUCCESS;
}

static uint16_t
zsda_comp_max_nb_qps(void)
{
	uint16_t comp = zsda_nb_qps.encomp;
	uint16_t decomp = zsda_nb_qps.decomp;
	uint16_t min = 0;

	if ((comp == MAX_QPS_ON_FUNCTION) ||
		(decomp == MAX_QPS_ON_FUNCTION))
		min = MAX_QPS_ON_FUNCTION;
	else
		min = (comp < decomp) ? comp : decomp;
	if (min == 0)
		return MAX_QPS_ON_FUNCTION;
	return min;
}

static void
zsda_comp_dev_info_get(struct rte_compressdev *dev,
		       struct rte_compressdev_info *info)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	if (info != NULL) {
		info->max_nb_queue_pairs = zsda_comp_max_nb_qps();
		info->feature_flags = dev->feature_flags;
		info->capabilities = comp_dev->zsda_dev_capabilities;
	}
}

static void
zsda_comp_stats_get(struct rte_compressdev *dev,
		    struct rte_compressdev_stats *stats)
{
	struct zsda_qp_stat stats_info = {0};

	zsda_stats_get(dev->data->queue_pairs, dev->data->nb_queue_pairs,
		       &stats_info);
	stats->enqueued_count = stats_info.enqueued_count;
	stats->dequeued_count = stats_info.dequeued_count;
	stats->enqueue_err_count = stats_info.enqueue_err_count;
	stats->dequeue_err_count = stats_info.dequeue_err_count;
}

static void
zsda_comp_stats_reset(struct rte_compressdev *dev)
{
	zsda_stats_reset(dev->data->queue_pairs, dev->data->nb_queue_pairs);
}

static struct rte_compressdev_ops compress_zsda_ops = {

	.dev_configure = zsda_comp_dev_config,
	.dev_start = zsda_comp_dev_start,
	.dev_stop = zsda_comp_dev_stop,
	.dev_close = zsda_comp_dev_close,
	.dev_infos_get = zsda_comp_dev_info_get,

	.stats_get = zsda_comp_stats_get,
	.stats_reset = zsda_comp_stats_reset,
	.queue_pair_setup = NULL,
	.queue_pair_release = NULL,

	.private_xform_create = NULL,
	.private_xform_free = NULL
};

/* An rte_driver is needed in the registration of the device with compressdev.
 * The actual zsda pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the compression part of the pci device.
 */
static const char zsda_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_ZSDA_PMD);
static const struct rte_driver compdev_zsda_driver = {
	.name = zsda_comp_drv_name, .alias = zsda_comp_drv_name};

int
zsda_comp_dev_create(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_device_info *dev_info =
		&zsda_devs[zsda_pci_dev->zsda_dev_id];

	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = (int)rte_socket_id(),
	};

	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct zsda_comp_dev_private *comp_dev;

	snprintf(name, RTE_COMPRESSDEV_NAME_MAX_LEN, "%s_%s",
		 zsda_pci_dev->name, "comp");

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev_info->comp_rte_dev.driver = &compdev_zsda_driver;
	dev_info->comp_rte_dev.numa_node = dev_info->pci_dev->device.numa_node;
	dev_info->comp_rte_dev.devargs = NULL;

	compressdev = rte_compressdev_pmd_create(
		name, &(dev_info->comp_rte_dev),
		sizeof(struct zsda_comp_dev_private), &init_params);

	if (compressdev == NULL)
		return -ENODEV;

	compressdev->dev_ops = &compress_zsda_ops;

	compressdev->enqueue_burst = NULL;
	compressdev->dequeue_burst = NULL;

	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	comp_dev = compressdev->data->dev_private;
	comp_dev->zsda_pci_dev = zsda_pci_dev;
	comp_dev->compressdev = compressdev;

	zsda_pci_dev->comp_dev = comp_dev;

	return ZSDA_SUCCESS;
}

int
zsda_comp_dev_destroy(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_comp_dev_private *comp_dev;

	if (zsda_pci_dev == NULL)
		return -ENODEV;

	comp_dev = zsda_pci_dev->comp_dev;
	if (comp_dev == NULL)
		return ZSDA_SUCCESS;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(zsda_pci_dev->comp_dev->capa_mz);

	zsda_comp_dev_close(comp_dev->compressdev);

	rte_compressdev_pmd_destroy(comp_dev->compressdev);
	zsda_pci_dev->comp_dev = NULL;

	return ZSDA_SUCCESS;
}
