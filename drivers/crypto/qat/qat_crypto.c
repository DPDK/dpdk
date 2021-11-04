/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "qat_qp.h"
#include "qat_crypto.h"
#include "qat_sym.h"
#include "qat_asym.h"

int
qat_cryptodev_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

int
qat_cryptodev_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

void
qat_cryptodev_stop(__rte_unused struct rte_cryptodev *dev)
{
}

int
qat_cryptodev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = dev->dev_ops->queue_pair_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void
qat_cryptodev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *info)
{
	struct qat_cryptodev_private *qat_private = dev->data->dev_private;
	struct qat_pci_device *qat_dev = qat_private->qat_dev;
	enum qat_service_type service_type = qat_private->service_type;

	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(qat_dev, service_type);
		info->feature_flags = dev->feature_flags;
		info->capabilities = qat_private->qat_dev_capabilities;
		if (service_type == QAT_SERVICE_ASYMMETRIC)
			info->driver_id = qat_asym_driver_id;

		if (service_type == QAT_SERVICE_SYMMETRIC)
			info->driver_id = qat_sym_driver_id;
		/* No limit of number of sessions */
		info->sym.max_nb_sessions = 0;
	}
}

void
qat_cryptodev_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	struct qat_common_stats qat_stats = {0};
	struct qat_cryptodev_private *qat_priv;

	if (stats == NULL || dev == NULL) {
		QAT_LOG(ERR, "invalid ptr: stats %p, dev %p", stats, dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_get(qat_priv->qat_dev, &qat_stats, qat_priv->service_type);
	stats->enqueued_count = qat_stats.enqueued_count;
	stats->dequeued_count = qat_stats.dequeued_count;
	stats->enqueue_err_count = qat_stats.enqueue_err_count;
	stats->dequeue_err_count = qat_stats.dequeue_err_count;
}

void
qat_cryptodev_stats_reset(struct rte_cryptodev *dev)
{
	struct qat_cryptodev_private *qat_priv;

	if (dev == NULL) {
		QAT_LOG(ERR, "invalid cryptodev ptr %p", dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_reset(qat_priv->qat_dev, qat_priv->service_type);

}

int
qat_cryptodev_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	struct qat_cryptodev_private *qat_private = dev->data->dev_private;
	struct qat_pci_device *qat_dev = qat_private->qat_dev;
	enum qat_device_gen qat_dev_gen = qat_dev->qat_dev_gen;
	enum qat_service_type service_type = qat_private->service_type;

	QAT_LOG(DEBUG, "Release %s qp %u on device %d",
			qat_service_get_str(service_type),
			queue_pair_id, dev->data->dev_id);

	qat_private->qat_dev->qps_in_use[service_type][queue_pair_id] = NULL;

	return qat_qp_release(qat_dev_gen, (struct qat_qp **)
			&(dev->data->queue_pairs[queue_pair_id]));
}

int
qat_cryptodev_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)
{
	struct qat_qp **qp_addr =
			(struct qat_qp **)&(dev->data->queue_pairs[qp_id]);
	struct qat_cryptodev_private *qat_private = dev->data->dev_private;
	struct qat_pci_device *qat_dev = qat_private->qat_dev;
	enum qat_service_type service_type = qat_private->service_type;
	struct qat_qp_config qat_qp_conf = {0};
	struct qat_qp *qp;
	int ret = 0;
	uint32_t i;

	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = dev->dev_ops->queue_pair_release(dev, qp_id);
		if (ret < 0)
			return -EBUSY;
	}
	if (qp_id >= qat_qps_per_service(qat_dev, service_type)) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	qat_qp_conf.hw = qat_qp_get_hw_data(qat_dev, service_type,
			qp_id);
	if (qat_qp_conf.hw == NULL) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	qat_qp_conf.cookie_size = service_type == QAT_SERVICE_SYMMETRIC ?
			sizeof(struct qat_sym_op_cookie) :
			sizeof(struct qat_asym_op_cookie);
	qat_qp_conf.nb_descriptors = qp_conf->nb_descriptors;
	qat_qp_conf.socket_id = socket_id;
	qat_qp_conf.service_str = qat_service_get_str(service_type);

	ret = qat_qp_setup(qat_dev, qp_addr, qp_id, &qat_qp_conf);
	if (ret != 0)
		return ret;

	/* store a link to the qp in the qat_pci_device */
	qat_dev->qps_in_use[service_type][qp_id] = *qp_addr;

	qp = (struct qat_qp *)*qp_addr;
	qp->min_enq_burst_threshold = qat_private->min_enq_burst_threshold;

	for (i = 0; i < qp->nb_descriptors; i++) {
		if (service_type == QAT_SERVICE_SYMMETRIC)
			qat_sym_init_op_cookie(qp->op_cookies[i]);
		else
			qat_asym_init_op_cookie(qp->op_cookies[i]);
	}

	return ret;
}
