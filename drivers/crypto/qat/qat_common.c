/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_common.h"
#include "qat_device.h"
#include "qat_logs.h"

int
qat_sgl_fill_array(struct rte_mbuf *buf, uint64_t buf_start,
		struct qat_sgl *list, uint32_t data_len)
{
	int nr = 1;

	uint32_t buf_len = rte_pktmbuf_iova(buf) -
			buf_start + rte_pktmbuf_data_len(buf);

	list->buffers[0].addr = buf_start;
	list->buffers[0].resrvd = 0;
	list->buffers[0].len = buf_len;

	if (data_len <= buf_len) {
		list->num_bufs = nr;
		list->buffers[0].len = data_len;
		return 0;
	}

	buf = buf->next;
	while (buf) {
		if (unlikely(nr == QAT_SGL_MAX_NUMBER)) {
			PMD_DRV_LOG(ERR,
				"QAT PMD exceeded size of QAT SGL entry(%u)",
					QAT_SGL_MAX_NUMBER);
			return -EINVAL;
		}

		list->buffers[nr].len = rte_pktmbuf_data_len(buf);
		list->buffers[nr].resrvd = 0;
		list->buffers[nr].addr = rte_pktmbuf_iova(buf);

		buf_len += list->buffers[nr].len;
		buf = buf->next;

		if (buf_len > data_len) {
			list->buffers[nr].len -=
				buf_len - data_len;
			buf = NULL;
		}
		++nr;
	}
	list->num_bufs = nr;

	return 0;
}

void qat_stats_get(struct qat_pci_device *dev,
		struct qat_common_stats *stats,
		enum qat_service_type service)
{
	int i;
	struct qat_qp **qp;

	if (stats == NULL || dev == NULL || service >= QAT_SERVICE_INVALID) {
		PMD_DRV_LOG(ERR, "invalid param: stats %p, dev %p, service %d",
				stats, dev, service);
		return;
	}

	qp = dev->qps_in_use[service];
	for (i = 0; i < ADF_MAX_QPS_PER_BUNDLE; i++) {
		if (qp[i] == NULL) {
			PMD_DRV_LOG(DEBUG, "Service %d Uninitialised qp %d",
					service, i);
			continue;
		}

		stats->enqueued_count += qp[i]->stats.enqueued_count;
		stats->dequeued_count += qp[i]->stats.dequeued_count;
		stats->enqueue_err_count += qp[i]->stats.enqueue_err_count;
		stats->dequeue_err_count += qp[i]->stats.dequeue_err_count;
	}
}

void qat_stats_reset(struct qat_pci_device *dev,
		enum qat_service_type service)
{
	int i;
	struct qat_qp **qp;

	if (dev == NULL || service >= QAT_SERVICE_INVALID) {
		PMD_DRV_LOG(ERR, "invalid param: dev %p, service %d",
				dev, service);
		return;
	}

	qp = dev->qps_in_use[service];
	for (i = 0; i < ADF_MAX_QPS_PER_BUNDLE; i++) {
		if (qp[i] == NULL) {
			PMD_DRV_LOG(DEBUG, "Service %d Uninitialised qp %d",
					service, i);
			continue;
		}
		memset(&(qp[i]->stats), 0, sizeof(qp[i]->stats));
	}

	PMD_DRV_LOG(DEBUG, "QAT crypto: %d stats cleared", service);
}
