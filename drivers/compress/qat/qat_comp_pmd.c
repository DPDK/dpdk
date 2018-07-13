/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include "qat_comp.h"
#include "qat_comp_pmd.h"

static void
qat_comp_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	struct qat_common_stats qat_stats = {0};
	struct qat_comp_dev_private *qat_priv;

	if (stats == NULL || dev == NULL) {
		QAT_LOG(ERR, "invalid ptr: stats %p, dev %p", stats, dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_get(qat_priv->qat_dev, &qat_stats, QAT_SERVICE_COMPRESSION);
	stats->enqueued_count = qat_stats.enqueued_count;
	stats->dequeued_count = qat_stats.dequeued_count;
	stats->enqueue_err_count = qat_stats.enqueue_err_count;
	stats->dequeue_err_count = qat_stats.dequeue_err_count;
}

static void
qat_comp_stats_reset(struct rte_compressdev *dev)
{
	struct qat_comp_dev_private *qat_priv;

	if (dev == NULL) {
		QAT_LOG(ERR, "invalid compressdev ptr %p", dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_reset(qat_priv->qat_dev, QAT_SERVICE_COMPRESSION);

}

static int
qat_comp_qp_release(struct rte_compressdev *dev, uint16_t queue_pair_id)
{
	struct qat_comp_dev_private *qat_private = dev->data->dev_private;

	QAT_LOG(DEBUG, "Release comp qp %u on device %d",
				queue_pair_id, dev->data->dev_id);

	qat_private->qat_dev->qps_in_use[QAT_SERVICE_COMPRESSION][queue_pair_id]
						= NULL;

	return qat_qp_release((struct qat_qp **)
			&(dev->data->queue_pairs[queue_pair_id]));
}

static int
qat_comp_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		  uint32_t max_inflight_ops, int socket_id)
{
	int ret = 0;
	struct qat_qp_config qat_qp_conf;

	struct qat_qp **qp_addr =
			(struct qat_qp **)&(dev->data->queue_pairs[qp_id]);
	struct qat_comp_dev_private *qat_private = dev->data->dev_private;
	const struct qat_qp_hw_data *comp_hw_qps =
			qat_gen_config[qat_private->qat_dev->qat_dev_gen]
				      .qp_hw_data[QAT_SERVICE_COMPRESSION];
	const struct qat_qp_hw_data *qp_hw_data = comp_hw_qps + qp_id;

	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = qat_comp_qp_release(dev, qp_id);
		if (ret < 0)
			return ret;
	}
	if (qp_id >= qat_qps_per_service(comp_hw_qps,
					 QAT_SERVICE_COMPRESSION)) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	qat_qp_conf.hw = qp_hw_data;
	qat_qp_conf.build_request = qat_comp_build_request;
	qat_qp_conf.cookie_size = sizeof(struct qat_comp_op_cookie);
	qat_qp_conf.nb_descriptors = max_inflight_ops;
	qat_qp_conf.socket_id = socket_id;
	qat_qp_conf.service_str = "comp";

	ret = qat_qp_setup(qat_private->qat_dev, qp_addr, qp_id, &qat_qp_conf);
	if (ret != 0)
		return ret;

	/* store a link to the qp in the qat_pci_device */
	qat_private->qat_dev->qps_in_use[QAT_SERVICE_COMPRESSION][qp_id]
							= *qp_addr;

	return ret;
}

static struct rte_mempool *
qat_comp_create_xform_pool(struct qat_comp_dev_private *comp_dev,
			      uint32_t num_elements)
{
	char xform_pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(xform_pool_name, RTE_MEMPOOL_NAMESIZE,
			"%s_xforms", comp_dev->qat_dev->name);

	QAT_LOG(DEBUG, "xformpool: %s", xform_pool_name);
	mp = rte_mempool_lookup(xform_pool_name);

	if (mp != NULL) {
		QAT_LOG(DEBUG, "xformpool already created");
		if (mp->size != num_elements) {
			QAT_LOG(DEBUG, "xformpool wrong size - delete it");
			rte_mempool_free(mp);
			mp = NULL;
			comp_dev->xformpool = NULL;
		}
	}

	if (mp == NULL)
		mp = rte_mempool_create(xform_pool_name,
				num_elements,
				qat_comp_xform_size(), 0, 0,
				NULL, NULL, NULL, NULL, rte_socket_id(),
				0);
	if (mp == NULL) {
		QAT_LOG(ERR, "Err creating mempool %s w %d elements of size %d",
			xform_pool_name, num_elements, qat_comp_xform_size());
		return NULL;
	}

	return mp;
}

static void
_qat_comp_dev_config_clear(struct qat_comp_dev_private *comp_dev)
{
	/* Free private_xform pool */
	if (comp_dev->xformpool) {
		/* Free internal mempool for private xforms */
		rte_mempool_free(comp_dev->xformpool);
		comp_dev->xformpool = NULL;
	}
}

static int
qat_comp_dev_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;
	int ret = 0;

	if (config->max_nb_streams != 0) {
		QAT_LOG(ERR,
	"QAT device does not support STATEFUL so max_nb_streams must be 0");
		return -EINVAL;
	}

	comp_dev->xformpool = qat_comp_create_xform_pool(comp_dev,
					config->max_nb_priv_xforms);
	if (comp_dev->xformpool == NULL) {

		ret = -ENOMEM;
		goto error_out;
	}
	return 0;

error_out:
	_qat_comp_dev_config_clear(comp_dev);
	return ret;
}

static int
qat_comp_dev_start(struct rte_compressdev *dev __rte_unused)
{
	return 0;
}

static void
qat_comp_dev_stop(struct rte_compressdev *dev __rte_unused)
{

}

static int
qat_comp_dev_close(struct rte_compressdev *dev)
{
	int i;
	int ret = 0;
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_comp_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	_qat_comp_dev_config_clear(comp_dev);

	return ret;
}


static void
qat_comp_dev_info_get(struct rte_compressdev *dev,
			struct rte_compressdev_info *info)
{
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;
	const struct qat_qp_hw_data *comp_hw_qps =
		qat_gen_config[comp_dev->qat_dev->qat_dev_gen]
			      .qp_hw_data[QAT_SERVICE_COMPRESSION];

	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(comp_hw_qps,
					    QAT_SERVICE_COMPRESSION);
		info->feature_flags = dev->feature_flags;
		info->capabilities = comp_dev->qat_dev_capabilities;
	}
}

uint16_t
qat_comp_pmd_enqueue_op_burst(void *qp, struct rte_comp_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, (void **)ops, nb_ops);
}

uint16_t
qat_comp_pmd_dequeue_op_burst(void *qp, struct rte_comp_op **ops,
			      uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops, nb_ops);
}


struct rte_compressdev_ops compress_qat_ops = {

	/* Device related operations */
	.dev_configure		= qat_comp_dev_config,
	.dev_start		= qat_comp_dev_start,
	.dev_stop		= qat_comp_dev_stop,
	.dev_close		= qat_comp_dev_close,
	.dev_infos_get		= qat_comp_dev_info_get,

	.stats_get		= qat_comp_stats_get,
	.stats_reset		= qat_comp_stats_reset,
	.queue_pair_setup	= qat_comp_qp_setup,
	.queue_pair_release	= qat_comp_qp_release,

	/* Compression related operations */
	.private_xform_create	= qat_comp_private_xform_create,
	.private_xform_free	= qat_comp_private_xform_free
};
