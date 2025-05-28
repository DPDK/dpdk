/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <rte_malloc.h>

#include "zsda_logs.h"
#include "zsda_qp_common.h"
#include "zsda_qp.h"
#include "zsda_comp_pmd.h"
#include "zsda_comp.h"

static const struct rte_compressdev_capabilities zsda_comp_capabilities[] = {
	{
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = RTE_COMP_FF_HUFFMAN_DYNAMIC |
				RTE_COMP_FF_OOP_SGL_IN_SGL_OUT |
				RTE_COMP_FF_OOP_SGL_IN_LB_OUT |
				RTE_COMP_FF_OOP_LB_IN_SGL_OUT |
				RTE_COMP_FF_CRC32_CHECKSUM |
				RTE_COMP_FF_ADLER32_CHECKSUM |
				RTE_COMP_FF_SHAREABLE_PRIV_XFORM,
		.window_size = {.min = 15, .max = 15, .increment = 0},
	},
};

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
zsda_dev_config(struct rte_compressdev *dev,
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
zsda_dev_start(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;
	int ret;

	ret = zsda_queue_start(comp_dev->zsda_pci_dev->pci_dev);

	if (ret)
		ZSDA_LOG(ERR, "Failed! zsda_queue_start.");

	return ret;
}

static void
zsda_dev_stop(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	zsda_queue_stop(comp_dev->zsda_pci_dev->pci_dev);
}

static int
zsda_qp_release(struct rte_compressdev *dev, uint16_t queue_pair_id)
{
	return zsda_queue_pair_release(
		(struct zsda_qp **)&(dev->data->queue_pairs[queue_pair_id]));
}

static int
zsda_dev_close(struct rte_compressdev *dev)
{
	struct zsda_comp_dev_private *comp_dev = dev->data->dev_private;

	for (int i = 0; i < dev->data->nb_queue_pairs; i++)
		zsda_qp_release(dev, i);

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
zsda_dev_info_get(struct rte_compressdev *dev,
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

static int
zsda_private_xform_create(struct rte_compressdev *dev,
			const struct rte_comp_xform *xform,
			void **private_xform)
{
	struct zsda_comp_dev_private *zsda = dev->data->dev_private;

	if (unlikely(private_xform == NULL)) {
		ZSDA_LOG(ERR, "Failed! private_xform is NULL");
		return -EINVAL;
	}
	if (unlikely(zsda->xformpool == NULL)) {
		ZSDA_LOG(ERR, "Failed! zsda->xformpool is NULL");
		return -ENOMEM;
	}
	if (rte_mempool_get(zsda->xformpool, private_xform)) {
		ZSDA_LOG(ERR, "Failed! zsda->xformpool is NULL");
		return -ENOMEM;
	}

	struct zsda_comp_xform *zsda_xform = *private_xform;
	zsda_xform->type = xform->type;

	if (zsda_xform->type == RTE_COMP_COMPRESS)
		zsda_xform->checksum_type = xform->compress.chksum;
	else
		zsda_xform->checksum_type = xform->decompress.chksum;

	if (zsda_xform->checksum_type == RTE_COMP_CHECKSUM_CRC32_ADLER32)
		return -EINVAL;

	return ZSDA_SUCCESS;
}

static int
zsda_private_xform_free(struct rte_compressdev *dev __rte_unused,
			void *private_xform)
{
	struct zsda_comp_xform *zsda_xform = private_xform;

	if (zsda_xform) {
		memset(zsda_xform, 0, zsda_comp_xform_size());
		struct rte_mempool *mp = rte_mempool_from_obj(zsda_xform);

		rte_mempool_put(mp, zsda_xform);
		return ZSDA_SUCCESS;
	}
	return -EINVAL;
}

static int
zsda_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	int ret = ZSDA_SUCCESS;
	struct zsda_qp *qp_new;

	struct zsda_qp **qp_addr =
		(struct zsda_qp **)&(dev->data->queue_pairs[qp_id]);
	struct zsda_comp_dev_private *comp_priv = dev->data->dev_private;
	struct zsda_pci_device *zsda_pci_dev = comp_priv->zsda_pci_dev;
	uint16_t nb_des = max_inflight_ops & 0xffff;
	struct task_queue_info task_q_info;

	nb_des = (nb_des == NB_DES) ? nb_des : NB_DES;

	if (*qp_addr != NULL) {
		ret = zsda_qp_release(dev, qp_id);
		if (ret)
			return ret;
	}

	qp_new = rte_zmalloc_socket("zsda PMD qp metadata", sizeof(*qp_new),
				    RTE_CACHE_LINE_SIZE, socket_id);
	if (qp_new == NULL) {
		ZSDA_LOG(ERR, "Failed! qp_new is NULL");
		return -ENOMEM;
	}

	task_q_info.nb_des = nb_des;
	task_q_info.socket_id = socket_id;
	task_q_info.qp_id = qp_id;
	task_q_info.rx_cb = zsda_comp_callback;

	task_q_info.type = ZSDA_SERVICE_COMPRESSION;
	task_q_info.service_str = "comp";
	task_q_info.tx_cb = zsda_comp_wqe_build;
	task_q_info.match = zsda_comp_match;
	ret = zsda_task_queue_setup(zsda_pci_dev, qp_new, &task_q_info);

	task_q_info.type = ZSDA_SERVICE_DECOMPRESSION;
	task_q_info.service_str = "decomp";
	task_q_info.tx_cb = zsda_decomp_request_build;
	task_q_info.match = zsda_decomp_match;
	ret |= zsda_task_queue_setup(zsda_pci_dev, qp_new, &task_q_info);

	if (ret) {
		rte_free(qp_new);
		return ret;
	}

	*qp_addr = qp_new;

	return ret;
}

static struct rte_compressdev_ops compress_zsda_ops = {
	.dev_configure = zsda_dev_config,
	.dev_start = zsda_dev_start,
	.dev_stop = zsda_dev_stop,
	.dev_close = zsda_dev_close,
	.dev_infos_get = zsda_dev_info_get,

	.stats_get = zsda_comp_stats_get,
	.stats_reset = zsda_comp_stats_reset,
	.queue_pair_setup = zsda_qp_setup,
	.queue_pair_release = zsda_qp_release,

	.private_xform_create = zsda_private_xform_create,
	.private_xform_free = zsda_private_xform_free,
};

/* An rte_driver is needed in the registration of the device with compressdev.
 * The actual zsda pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the compression part of the pci device.
 */
static const char zsda_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_ZSDA_PMD);
static const struct rte_driver compdev_zsda_driver = {
	.name = zsda_comp_drv_name,
	.alias = zsda_comp_drv_name
};

static uint16_t
zsda_comp_pmd_enqueue_op_burst(void *qp, struct rte_comp_op **ops,
			       uint16_t nb_ops)
{
	return zsda_enqueue_burst((struct zsda_qp *)qp, (void **)ops,
				     nb_ops);
}

static uint16_t
zsda_comp_pmd_dequeue_op_burst(void *qp, struct rte_comp_op **ops,
			       uint16_t nb_ops)
{
	return zsda_dequeue_burst((struct zsda_qp *)qp, (void **)ops,
				     nb_ops);
}

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
	char capa_memz_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct zsda_comp_dev_private *comp_dev;
	const struct rte_compressdev_capabilities *capabilities;
	uint16_t capa_size = sizeof(struct rte_compressdev_capabilities);

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

	compressdev->enqueue_burst = zsda_comp_pmd_enqueue_op_burst;
	compressdev->dequeue_burst = zsda_comp_pmd_dequeue_op_burst;

	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	comp_dev = compressdev->data->dev_private;
	comp_dev->zsda_pci_dev = zsda_pci_dev;
	comp_dev->compressdev = compressdev;

	capabilities = zsda_comp_capabilities;

	snprintf(capa_memz_name, RTE_COMPRESSDEV_NAME_MAX_LEN,
		 "ZSDA_COMP_CAPA");
	comp_dev->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (comp_dev->capa_mz == NULL)
		comp_dev->capa_mz = rte_memzone_reserve(
			capa_memz_name, capa_size, rte_socket_id(), 0);

	if (comp_dev->capa_mz == NULL) {
		ZSDA_LOG(DEBUG, "Failed! comp_dev->capa_mz is NULL");
		memset(&dev_info->comp_rte_dev, 0,
		       sizeof(dev_info->comp_rte_dev));
		rte_compressdev_pmd_destroy(compressdev);
		return -EFAULT;
	}

	memcpy(comp_dev->capa_mz->addr, capabilities, capa_size);
	comp_dev->zsda_dev_capabilities = comp_dev->capa_mz->addr;

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

	zsda_dev_close(comp_dev->compressdev);

	rte_compressdev_pmd_destroy(comp_dev->compressdev);
	zsda_pci_dev->comp_dev = NULL;

	return ZSDA_SUCCESS;
}
