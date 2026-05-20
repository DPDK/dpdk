/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_common.h>
#include <rte_net.h>
#include <rte_vect.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <ethdev_driver.h>
#include <unistd.h>
#include "sxe2_txrx.h"
#include "sxe2_txrx_common.h"
#include "sxe2_txrx_vec.h"
#include "sxe2_txrx_poll.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"
#include "sxe2_osal.h"
#include "sxe2_cmd_chnl.h"
#if defined(RTE_ARCH_ARM64)
#include <rte_cpuflags.h>
#endif

int32_t __rte_cold
sxe2_tx_simple_batch_support_check(struct rte_eth_dev *dev,
		uint32_t *batch_flags)
{
	struct sxe2_tx_queue *txq;
	int32_t ret = 0;
	uint16_t i;

	for (i = 0; i < dev->data->nb_tx_queues; ++i) {
		txq = (struct sxe2_tx_queue *)dev->data->tx_queues[i];
		if (txq == NULL) {
			ret = -EINVAL;
			goto l_end;
		}
		if (txq->offloads != (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) ||
		     txq->rs_thresh < SXE2_TX_PKTS_BURST_BATCH_NUM) {
			ret = -ENOTSUP;
			goto l_end;
		}
	}
	*batch_flags = SXE2_TX_MODE_SIMPLE_BATCH;
l_end:
	return ret;
}

static int32_t sxe2_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct sxe2_tx_queue *txq = (struct sxe2_tx_queue *)tx_queue;
	int32_t ret;
	uint16_t desc_idx;

	if (unlikely(offset >= txq->ring_depth)) {
		ret = -EINVAL;
		goto l_end;
	}
	desc_idx = txq->next_use + offset;
	desc_idx = SXE2_DIV_ROUND_UP(desc_idx, txq->rs_thresh) * (txq->rs_thresh);
	if (desc_idx >= txq->ring_depth) {
		desc_idx -= txq->ring_depth;
		if (desc_idx >= txq->ring_depth)
			desc_idx -= txq->ring_depth;
	}
	if (desc_idx == 0)
		desc_idx = txq->rs_thresh - 1;
	else
		desc_idx -= 1;
	if (rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_DONE) ==
		(txq->desc_ring[desc_idx].wb.dd &
		rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_MASK)))
		ret = RTE_ETH_TX_DESC_DONE;
	else
		ret = RTE_ETH_TX_DESC_FULL;
l_end:
	return ret;
}

static inline int32_t sxe2_tx_mbuf_empty_check(struct rte_mbuf *mbuf)
{
	struct rte_mbuf *m_seg = mbuf;
	while (m_seg != NULL) {
		if (m_seg->data_len == 0)
			return -EINVAL;
		m_seg = m_seg->next;
	}

	return 0;
}

uint16_t sxe2_tx_pkts_prepare(void *tx_queue,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sxe2_tx_queue *txq = tx_queue;
	struct rte_mbuf *mbuf;
	uint64_t ol_flags = 0;
	int32_t ret = 0;
	int32_t i = 0;

	for (i = 0; i < nb_pkts; i++) {
		mbuf = tx_pkts[i];
		if (!mbuf)
			continue;
		ol_flags = mbuf->ol_flags;
		if (!(ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))) {
			if (mbuf->nb_segs > SXE2_TX_MTU_SEG_MAX ||
					mbuf->pkt_len > SXE2_FRAME_SIZE_MAX) {
				rte_errno = -EINVAL;
				goto l_end;
			}
		} else if ((mbuf->tso_segsz < SXE2_MIN_TSO_MSS) ||
			(mbuf->tso_segsz > SXE2_MAX_TSO_MSS) ||
			(mbuf->nb_segs   > txq->ring_depth) ||
			(mbuf->pkt_len > SXE2_TX_TSO_PKTLEN_MAX)) {
			rte_errno = -EINVAL;
			goto l_end;
		}
		if (mbuf->pkt_len < SXE2_TX_MIN_PKT_LEN) {
			rte_errno = -EINVAL;
			goto l_end;
		}
#ifdef RTE_ETHDEV_DEBUG_TX
		ret = rte_validate_tx_offload(mbuf);
		if (ret != 0) {
			rte_errno = -ret;
			goto l_end;
		}
#endif
		ret = rte_net_intel_cksum_prepare(mbuf);
		if (ret != 0) {
			rte_errno = -ret;
			goto l_end;
		}
		ret = sxe2_tx_mbuf_empty_check(mbuf);
		if (ret != 0) {
			rte_errno = -ret;
			goto l_end;
		}
	}
l_end:
	return i;
}

void sxe2_tx_mode_func_set(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint32_t tx_mode_flags;
	int32_t ret;
	uint32_t vec_flags = 0;
	uint32_t batch_flags = 0;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		tx_mode_flags = 0;
		ret = sxe2_tx_vec_support_check(dev, &vec_flags);
		if (ret == 0 &&
		    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
			tx_mode_flags = vec_flags;
			if (tx_mode_flags & SXE2_TX_MODE_VEC_SET_MASK) {
				ret = sxe2_tx_queues_vec_prepare(dev);
				if (ret != 0)
					tx_mode_flags &= ~SXE2_TX_MODE_VEC_SET_MASK;
			}
		}
		ret = sxe2_tx_simple_batch_support_check(dev, &batch_flags);
		if (ret == 0 && batch_flags == SXE2_TX_MODE_SIMPLE_BATCH)
			tx_mode_flags |= SXE2_TX_MODE_SIMPLE_BATCH;

		adapter->q_ctxt.tx_mode_flags = tx_mode_flags;
	} else {
		tx_mode_flags = adapter->q_ctxt.tx_mode_flags;
	}

#ifdef RTE_ARCH_X86
	if (tx_mode_flags & SXE2_TX_MODE_VEC_SET_MASK) {
		if (tx_mode_flags & SXE2_TX_MODE_VEC_OFFLOAD) {
			dev->tx_pkt_prepare = sxe2_tx_pkts_prepare;
			dev->tx_pkt_burst = sxe2_tx_pkts_vec_sse;
		} else {
			dev->tx_pkt_prepare = NULL;
			dev->tx_pkt_burst = sxe2_tx_pkts_vec_sse_simple;
		}
	} else {
#endif
		if (tx_mode_flags & SXE2_TX_MODE_SIMPLE_BATCH) {
			dev->tx_pkt_prepare = NULL;
			dev->tx_pkt_burst = sxe2_tx_pkts_simple;
		} else {
			dev->tx_pkt_prepare = sxe2_tx_pkts_prepare;
			dev->tx_pkt_burst = sxe2_tx_pkts;
		}
#ifdef RTE_ARCH_X86
	}
#endif
}

static const struct {
	eth_tx_burst_t tx_burst;
	const char *info;
} sxe2_tx_burst_infos[] = {
	{ sxe2_tx_pkts,   "Scalar" },
#ifdef RTE_ARCH_X86
	{ sxe2_tx_pkts_vec_sse,        "Vector SSE" },
	{ sxe2_tx_pkts_vec_sse_simple, "Vector SSE Simple" },
#endif
};

int32_t sxe2_tx_burst_mode_get(struct rte_eth_dev *dev,
		__rte_unused uint16_t queue_id, struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	int32_t ret = -EINVAL;
	uint32_t i;
	uint32_t size;

	size = RTE_DIM(sxe2_tx_burst_infos);
	for (i = 0; i < size; ++i) {
		if (pkt_burst == sxe2_tx_burst_infos[i].tx_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
					sxe2_tx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}
	return ret;
}

static int32_t sxe2_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct sxe2_rx_queue *rxq = (struct sxe2_rx_queue *)rx_queue;
	volatile union sxe2_rx_desc *desc;
	int32_t ret;

	if (unlikely(offset >= rxq->ring_depth)) {
		ret = -EINVAL;
		goto l_end;
	}
	if (offset >= rxq->ring_depth - rxq->hold_num) {
		ret = RTE_ETH_RX_DESC_UNAVAIL;
		goto l_end;
	}
	if (rxq->processing_idx + offset >= rxq->ring_depth)
		desc = &rxq->desc_ring[rxq->processing_idx + offset - rxq->ring_depth];
	else
		desc = &rxq->desc_ring[rxq->processing_idx + offset];
	if (rte_le_to_cpu_64(desc->wb.status_err_ptype_len) & SXE2_RX_DESC_STATUS_DD_MASK)
		ret = RTE_ETH_RX_DESC_DONE;
	else
		ret = RTE_ETH_RX_DESC_AVAIL;
l_end:
	PMD_LOG_DEBUG(RX, "Rx queue desc[%u] status:%d queue_id:%u port_id:%u",
				offset, ret, rxq->queue_id, rxq->port_id);
	return ret;
}

static int32_t sxe2_rx_queue_count(void *rx_queue)
{
	struct sxe2_rx_queue *rxq = (struct sxe2_rx_queue *)rx_queue;
	volatile union sxe2_rx_desc *desc;
	uint16_t done_num = 0;

	desc = &rxq->desc_ring[rxq->processing_idx];
	while ((done_num < rxq->ring_depth) &&
		(rte_le_to_cpu_64(desc->wb.status_err_ptype_len) &
		SXE2_RX_DESC_STATUS_DD_MASK)) {
		done_num += SXE2_RX_QUEUE_CHECK_INTERVAL_NUM;
		if (rxq->processing_idx + done_num >= rxq->ring_depth)
			desc = &rxq->desc_ring[rxq->processing_idx + done_num - rxq->ring_depth];
		else
			desc += SXE2_RX_QUEUE_CHECK_INTERVAL_NUM;
	}
	PMD_LOG_DEBUG(RX, "Rx queue done desc count:%u queue_id:%u port_id:%u",
				done_num, rxq->queue_id, rxq->port_id);
	return done_num;
}

void sxe2_rx_mode_func_set(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint32_t rx_mode_flags = 0;
	int32_t ret;
	uint32_t vec_flags = 0;
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = sxe2_rx_vec_support_check(dev, &vec_flags);
		if (ret == 0 &&
		    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
			rx_mode_flags = vec_flags;
			if ((rx_mode_flags & SXE2_RX_MODE_VEC_SET_MASK) != 0) {
				ret = sxe2_rx_queues_vec_prepare(dev);
				if (ret != 0)
					rx_mode_flags &= ~SXE2_RX_MODE_VEC_SET_MASK;
			}
		}
		adapter->q_ctxt.rx_mode_flags = rx_mode_flags;
	} else {
		rx_mode_flags = adapter->q_ctxt.rx_mode_flags;
	}

#ifdef RTE_ARCH_X86
	if (rx_mode_flags & SXE2_RX_MODE_VEC_SET_MASK) {
		dev->rx_pkt_burst = sxe2_rx_pkts_scattered_vec_sse_offload;
		return;
	}
#endif
	if (sxe2_rx_offload_en_check(dev, RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT))
		dev->rx_pkt_burst = sxe2_rx_pkts_scattered_split;
	else
		dev->rx_pkt_burst = sxe2_rx_pkts_scattered;
}

static const struct {
	eth_rx_burst_t rx_burst;
	const char *info;
} sxe2_rx_burst_infos[] = {
	{ sxe2_rx_pkts_scattered,          "Scalar Scattered" },
	{ sxe2_rx_pkts_scattered_split,          "Scalar Scattered split" },
#ifdef RTE_ARCH_X86
	{ sxe2_rx_pkts_scattered_vec_sse_offload,      "Vector SSE Scattered" },
#endif
};

int32_t sxe2_rx_burst_mode_get(struct rte_eth_dev *dev,
			__rte_unused uint16_t queue_id, struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int32_t ret = -EINVAL;
	uint32_t i, size;
	size = RTE_DIM(sxe2_rx_burst_infos);
	for (i = 0; i < size; ++i) {
		if (pkt_burst == sxe2_rx_burst_infos[i].rx_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 sxe2_rx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}
	return ret;
}

void sxe2_set_common_function(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	dev->rx_queue_count = sxe2_rx_queue_count;
	dev->rx_descriptor_status = sxe2_rx_descriptor_status;

	dev->tx_descriptor_status = sxe2_tx_descriptor_status;
	dev->tx_pkt_prepare = sxe2_tx_pkts_prepare;
}
