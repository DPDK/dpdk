/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_common.h"
#include "qat_device.h"
#include "qat_logs.h"

const char *
qat_service_get_str(enum qat_service_type type)
{
	switch (type) {
	case QAT_SERVICE_SYMMETRIC:
		return "sym";
	case QAT_SERVICE_ASYMMETRIC:
		return "asym";
	case QAT_SERVICE_COMPRESSION:
		return "comp";
	default:
		return "invalid";
	}
}

int
qat_sgl_fill_array(struct rte_mbuf *buf, int64_t offset,
		void *list_in, uint32_t data_len,
		const uint16_t max_segs)
{
	int res = -EINVAL;
	uint32_t buf_len, nr;
	struct qat_sgl *list = (struct qat_sgl *)list_in;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	uint8_t *virt_addr[max_segs];
#endif

	for (nr = buf_len = 0; buf &&  nr < max_segs; buf = buf->next)  {
		if (offset >= rte_pktmbuf_data_len(buf)) {
			offset -= rte_pktmbuf_data_len(buf);
			continue;
		}

		list->buffers[nr].len = rte_pktmbuf_data_len(buf) - offset;
		list->buffers[nr].resrvd = 0;
		list->buffers[nr].addr = rte_pktmbuf_iova_offset(buf, offset);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		virt_addr[nr] = rte_pktmbuf_mtod_offset(buf, uint8_t*, offset);
#endif
		offset = 0;
		buf_len += list->buffers[nr].len;

		if (buf_len >= data_len) {
			list->buffers[nr].len -= buf_len - data_len;
			res = 0;
			break;
		}
		++nr;
	}

	if (unlikely(res != 0)) {
		if (nr == max_segs) {
			QAT_DP_LOG(ERR, "Exceeded max segments in QAT SGL (%u)",
				   max_segs);
		} else {
			QAT_DP_LOG(ERR, "Mbuf chain is too short");
		}
	} else {

		list->num_bufs = ++nr;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_LOG(INFO, "SGL with %d buffers:", list->num_bufs);
		for (nr = 0; nr < list->num_bufs; nr++) {
			QAT_DP_LOG(INFO,
				"QAT SGL buf %d, len = %d, iova = 0x%012"PRIx64,
				 nr, list->buffers[nr].len,
				 list->buffers[nr].addr);
			QAT_DP_HEXDUMP_LOG(DEBUG, "qat SGL",
					   virt_addr[nr],
					   list->buffers[nr].len);
		}
#endif
	}

	return res;
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
		stats->threshold_hit_count += qp[i]->stats.threshold_hit_count;
		QAT_LOG(DEBUG, "Threshold was used for qp %d %"PRIu64" times",
				i, stats->threshold_hit_count);
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
