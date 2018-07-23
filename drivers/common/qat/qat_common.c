/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_common.h"
#include "qat_device.h"
#include "qat_logs.h"

int
qat_sgl_fill_array(struct rte_mbuf *buf, uint64_t buf_start,
		void *list_in, uint32_t data_len,
		const uint16_t max_segs)
{
	int nr = 1;
	struct qat_sgl *list = (struct qat_sgl *)list_in;
	/* buf_start allows the first buffer to start at an address before or
	 * after the mbuf data start. It's used to either optimally align the
	 * dma to 64 or to start dma from an offset.
	 */
	uint32_t buf_len;
	uint32_t first_buf_len = rte_pktmbuf_data_len(buf) +
			(rte_pktmbuf_mtophys(buf) - buf_start);
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	uint8_t *virt_addr[max_segs];
	virt_addr[0] = rte_pktmbuf_mtod(buf, uint8_t*) +
			(rte_pktmbuf_mtophys(buf) - buf_start);
#endif

	list->buffers[0].addr = buf_start;
	list->buffers[0].resrvd = 0;
	list->buffers[0].len = first_buf_len;

	if (data_len <= first_buf_len) {
		list->num_bufs = nr;
		list->buffers[0].len = data_len;
		goto sgl_end;
	}

	buf = buf->next;
	buf_len = first_buf_len;
	while (buf) {
		if (unlikely(nr == max_segs)) {
			QAT_DP_LOG(ERR, "Exceeded max segments in QAT SGL (%u)",
					max_segs);
			return -EINVAL;
		}

		list->buffers[nr].len = rte_pktmbuf_data_len(buf);
		list->buffers[nr].resrvd = 0;
		list->buffers[nr].addr = rte_pktmbuf_mtophys(buf);
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		virt_addr[nr] = rte_pktmbuf_mtod(buf, uint8_t*);
#endif
		buf_len += list->buffers[nr].len;
		buf = buf->next;

		if (buf_len >= data_len) {
			list->buffers[nr].len -=
				buf_len - data_len;
			buf = NULL;
		}
		++nr;
	}
	list->num_bufs = nr;

sgl_end:
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	{
		uint16_t i;
		QAT_DP_LOG(INFO, "SGL with %d buffers:", list->num_bufs);
		for (i = 0; i < list->num_bufs; i++) {
			QAT_DP_LOG(INFO,
				"QAT SGL buf %d, len = %d, iova = 0x%012"PRIx64,
				i, list->buffers[i].len,
				list->buffers[i].addr);
			QAT_DP_HEXDUMP_LOG(DEBUG, "qat SGL",
					virt_addr[i], list->buffers[i].len);
		}
	}
#endif

	return 0;
}

void qat_stats_get(struct qat_pci_device *dev,
		struct qat_common_stats *stats,
		enum qat_service_type service)
{
	int i;
	struct qat_qp **qp;

	if (stats == NULL || dev == NULL || service >= QAT_SERVICE_INVALID) {
		QAT_LOG(ERR, "invalid param: stats %p, dev %p, service %d",
				stats, dev, service);
		return;
	}

	qp = dev->qps_in_use[service];
	for (i = 0; i < ADF_MAX_QPS_ON_ANY_SERVICE; i++) {
		if (qp[i] == NULL) {
			QAT_LOG(DEBUG, "Service %d Uninitialised qp %d",
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
		QAT_LOG(ERR, "invalid param: dev %p, service %d",
				dev, service);
		return;
	}

	qp = dev->qps_in_use[service];
	for (i = 0; i < ADF_MAX_QPS_ON_ANY_SERVICE; i++) {
		if (qp[i] == NULL) {
			QAT_LOG(DEBUG, "Service %d Uninitialised qp %d",
					service, i);
			continue;
		}
		memset(&(qp[i]->stats), 0, sizeof(qp[i]->stats));
	}

	QAT_LOG(DEBUG, "QAT: %d stats cleared", service);
}
