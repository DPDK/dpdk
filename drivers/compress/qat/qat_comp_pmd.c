/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include "qat_comp_pmd.h"

void
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

void
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
