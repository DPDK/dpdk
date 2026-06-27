/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>
#include <rte_malloc.h>
#include <rte_memzone.h>

#include "sxe2_ethdev.h"
#include "sxe2_queue.h"
#include "sxe2_rx.h"
#include "sxe2_cmd_chnl.h"

#include "sxe2_osal.h"
#include "sxe2_common_log.h"

static void *sxe2_rx_doorbell_tail_addr_get(struct sxe2_adapter *adapter, uint16_t queue_id)
{
	return sxe2_pci_map_addr_get(adapter, SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL,
				     queue_id);
}

static void sxe2_rx_head_tail_init(struct sxe2_adapter *adapter, struct sxe2_rx_queue *rxq)
{
	rxq->rdt_reg_addr = sxe2_rx_doorbell_tail_addr_get(adapter, rxq->queue_id);
	SXE2_PCI_REG_WRITE_WC(rxq->rdt_reg_addr, 0);
}

static void __rte_cold sxe2_rx_queue_reset(struct sxe2_rx_queue *rxq)
{
	uint16_t i = 0;
	uint16_t len = 0;
	static const union sxe2_rx_desc zeroed_desc = {{0}};

	len = rxq->ring_depth + SXE2_RX_PKTS_BURST_BATCH_NUM;
	for (i = 0; i < len; ++i)
		rxq->desc_ring[i] = zeroed_desc;

	memset(&rxq->fake_mbuf, 0, sizeof(rxq->fake_mbuf));
	for (i = rxq->ring_depth; i < len; i++)
		rxq->buffer_ring[i] = &rxq->fake_mbuf;

	rxq->hold_num            = 0;
	rxq->next_ret_pkt        = 0;
	rxq->processing_idx      = 0;
	rxq->completed_pkts_num  = 0;
	rxq->batch_alloc_trigger = rxq->rx_free_thresh - 1;

	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg  = NULL;

	rxq->realloc_num   = 0;
	rxq->realloc_start = 0;
}

void __rte_cold sxe2_rx_queue_mbufs_release(struct sxe2_rx_queue *rxq)
{
	uint16_t i;

	if (rxq->buffer_ring != NULL) {
		for (i = 0; i < rxq->ring_depth; i++) {
			if (rxq->buffer_ring[i] != NULL) {
				rte_pktmbuf_free(rxq->buffer_ring[i]);
				rxq->buffer_ring[i] = NULL;
			}
		}
	}

	if (rxq->completed_pkts_num) {
		for (i = 0; i < rxq->completed_pkts_num; ++i) {
			if (rxq->completed_buf[rxq->next_ret_pkt + i] != NULL) {
				rte_pktmbuf_free(rxq->completed_buf[rxq->next_ret_pkt + i]);
				rxq->completed_buf[rxq->next_ret_pkt + i] = NULL;
			}
		}
		rxq->completed_pkts_num = 0;
	}
}

const struct sxe2_rxq_ops sxe2_default_rxq_ops = {
	.queue_reset      = sxe2_rx_queue_reset,
	.mbufs_release    = sxe2_rx_queue_mbufs_release,
};

static struct sxe2_rxq_ops sxe2_rx_default_ops_get(void)
{
	return sxe2_default_rxq_ops;
}

void __rte_cold sxe2_rx_queue_info_get(struct rte_eth_dev *dev,
		uint16_t queue_id, struct rte_eth_rxq_info *qinfo)
{
	struct sxe2_rx_queue *rxq = NULL;

	if (queue_id >= dev->data->nb_rx_queues) {
		PMD_LOG_ERR(RX, "rx queue:%u is out of range:%u",
			queue_id, dev->data->nb_rx_queues);
		goto end;
	}

	rxq = dev->data->rx_queues[queue_id];
	if (rxq == NULL) {
		PMD_LOG_ERR(RX, "rx queue:%u is NULL", queue_id);
		goto end;
	}

	qinfo->mp           = rxq->mb_pool;
	qinfo->nb_desc      = rxq->ring_depth;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->conf.rx_free_thresh    = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en        = rxq->drop_en;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;

end:
	return;
}

int32_t __rte_cold sxe2_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rx_queue *rxq;
	int32_t ret;
	PMD_INIT_FUNC_TRACE();

	if (dev->data->rx_queue_state[rx_queue_id] ==
			RTE_ETH_QUEUE_STATE_STOPPED) {
		ret = 0;
		goto l_end;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	if (rxq == NULL) {
		ret = 0;
		goto l_end;
	}
	ret = sxe2_drv_rxq_switch(adapter, rxq, false);
	if (ret) {
		PMD_LOG_ERR(RX, "Failed to switch rx queue %u off, ret = %d",
				rx_queue_id, ret);
		if (ret == -EPERM)
			goto l_free;
		goto l_end;
	}

l_free:
	rxq->ops.mbufs_release(rxq);
	rxq->ops.queue_reset(rxq);
	dev->data->rx_queue_state[rx_queue_id] =
		RTE_ETH_QUEUE_STATE_STOPPED;
l_end:
	return ret;
}

static void __rte_cold sxe2_rx_queue_free(struct sxe2_rx_queue *rxq)
{
	if (rxq != NULL) {
		rxq->ops.mbufs_release(rxq);
		if (rxq->buffer_ring != NULL) {
			rte_free(rxq->buffer_ring);
			rxq->buffer_ring = NULL;
		}
		rte_memzone_free(rxq->mz);
		rte_free(rxq);
	}
}

void __rte_cold sxe2_rx_queue_release(struct rte_eth_dev *dev,
					uint16_t queue_idx)
{
	(void)sxe2_rx_queue_stop(dev, queue_idx);
	sxe2_rx_queue_free(dev->data->rx_queues[queue_idx]);
	dev->data->rx_queues[queue_idx] = NULL;
}

void __rte_cold sxe2_all_rxqs_release(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	uint16_t nb_rxq;

	for (nb_rxq = 0; nb_rxq < data->nb_rx_queues; nb_rxq++) {
		if (data->rx_queues[nb_rxq] == NULL)
			continue;
		sxe2_rx_queue_release(dev, nb_rxq);
		data->rx_queues[nb_rxq] = NULL;
	}
	data->nb_rx_queues = 0;
}

static struct sxe2_rx_queue *sxe2_rx_queue_alloc(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t ring_depth, uint32_t socket_id)
{
	struct sxe2_rx_queue *rxq;
	const struct rte_memzone *tz;
	uint16_t len;

	if (dev->data->rx_queues[queue_idx] != NULL) {
		sxe2_rx_queue_release(dev, queue_idx);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	rxq = rte_zmalloc_socket("rx_queue", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);

	if (rxq == NULL) {
		PMD_LOG_ERR(RX, "rx queue[%d] alloc failed", queue_idx);
		goto l_end;
	}

	rxq->ring_depth = ring_depth;
	len = rxq->ring_depth + SXE2_RX_PKTS_BURST_BATCH_NUM;

	rxq->buffer_ring = rte_zmalloc_socket("rx_buffer_ring",
					  sizeof(struct rte_mbuf *) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);

	if (!rxq->buffer_ring) {
		PMD_LOG_ERR(RX, "Rxq malloc mbuf mem failed");
		rte_free(rxq);
		rxq = NULL;
		goto l_end;
	}

	tz = rte_eth_dma_zone_reserve(dev, "rx_dma", queue_idx,
					SXE2_RX_RING_SIZE, SXE2_DESC_ADDR_ALIGN, socket_id);
	if (tz == NULL) {
		PMD_LOG_ERR(RX, "Rxq malloc desc mem failed");
		rte_free(rxq->buffer_ring);
		rxq->buffer_ring = NULL;
		rte_free(rxq);
		rxq = NULL;
		goto l_end;
	}

	rxq->mz = tz;
	memset(tz->addr, 0, SXE2_RX_RING_SIZE);
	rxq->base_addr = tz->iova;
	rxq->desc_ring = (union sxe2_rx_desc *)tz->addr;

l_end:
	return rxq;
}

int32_t __rte_cold sxe2_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx, uint16_t nb_desc, uint32_t socket_id,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi *vsi = adapter->vsi_ctxt.main_vsi;
	struct sxe2_rx_queue *rxq;
	uint64_t offloads;
	int32_t ret;
	uint16_t rx_nseg;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	if (nb_desc % SXE2_RX_DESC_RING_ALIGN != 0 ||
		nb_desc > SXE2_MAX_RING_DESC ||
		nb_desc < SXE2_MIN_RING_DESC) {
		PMD_LOG_ERR(RX, "param desc num:%u is invalid", nb_desc);
		ret = -EINVAL;
		goto l_end;
	}

	if (mp != NULL)
		rx_nseg = 1;
	else
		rx_nseg = rx_conf->rx_nseg;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	if (rx_nseg > 1 && !(offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
		PMD_LOG_ERR(RX, "Port %u queue %u Buffer split offload not configured, but rx_nseg is %u",
					dev->data->port_id, queue_idx, rx_nseg);
		ret = -EINVAL;
		goto l_end;
	}

	if ((offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) && !(rx_nseg > 1)) {
		PMD_LOG_ERR(RX, "Port %u queue %u Buffer split offload configured, but rx_nseg is %u",
					dev->data->port_id, queue_idx, rx_nseg);
		ret = -EINVAL;
		goto l_end;
	}

	if ((offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) &&
		(offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)) {
		PMD_LOG_ERR(RX, "port_id %u queue %u, LRO can't be configure with Keep crc.",
					dev->data->port_id, queue_idx);
		ret = -EINVAL;
		goto l_end;
	}

	if (!sxe2_ipsec_valid_rx_offloads(offloads)) {
		ret = -EINVAL;
		goto l_end;
	}

	rxq = sxe2_rx_queue_alloc(dev, queue_idx, nb_desc, socket_id);
	if (rxq == NULL) {
		PMD_LOG_ERR(RX, "rx queue[%d] resource alloc failed", queue_idx);
		ret = -ENOMEM;
		goto l_end;
	}

	if (offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		dev->data->lro = 1;

	if (rx_nseg > 1) {
		for (i = 0; i < rx_nseg; i++) {
			rte_memcpy(&rxq->rx_seg[i], &rx_conf->rx_seg[i].split,
					sizeof(struct rte_eth_rxseg_split));
		}
		rxq->mb_pool = rxq->rx_seg[0].mp;
	} else {
		rxq->mb_pool = mp;
	}

	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->port_id = dev->data->port_id;
	rxq->offloads = offloads;
	if (offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	rxq->queue_id = queue_idx;
	rxq->idx_in_func = vsi->rxqs.base_idx_in_func + queue_idx;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->vsi = vsi;
	rxq->ops = sxe2_rx_default_ops_get();
	rxq->ops.queue_reset(rxq);
	dev->data->rx_queues[queue_idx] = rxq;

	ret = 0;
l_end:
	return ret;
}

static int32_t __rte_cold sxe2_rx_queue_mbufs_alloc(struct sxe2_rx_queue *rxq)
{
	struct rte_mbuf **buf_ring = rxq->buffer_ring;
	struct rte_mbuf *mbuf = NULL;
	struct rte_mbuf *mbuf_pay;
	volatile union sxe2_rx_desc *desc;
	uint64_t dma_addr;
	int32_t ret;
	uint16_t i, j;

	for (i = 0; i < rxq->ring_depth; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (mbuf == NULL) {
			PMD_LOG_ERR(RX, "Rx queue is not available or setup");
			ret = -ENOMEM;
			goto l_err_free_mbuf;
		}

		buf_ring[i] = mbuf;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		desc = &rxq->desc_ring[i];
		if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			mbuf->next = NULL;
			desc->read.hdr_addr = 0;
			desc->read.pkt_addr = dma_addr;
		} else {
			mbuf_pay = rte_mbuf_raw_alloc(rxq->rx_seg[1].mp);
			if (unlikely(!mbuf_pay)) {
				PMD_LOG_ERR(RX, "Failed to allocate payload mbuf for RX");
				ret = -ENOMEM;
				goto l_err_free_mbuf;
			}

			mbuf_pay->next = NULL;
			mbuf_pay->data_off = RTE_PKTMBUF_HEADROOM;
			mbuf_pay->nb_segs = 1;
			mbuf_pay->port = rxq->port_id;
			mbuf->next = mbuf_pay;

			desc->read.hdr_addr = dma_addr;
			desc->read.pkt_addr =
				rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf_pay));
		}

#ifndef RTE_LIBRTE_SXE2_16BYTE_RX_DESC
		desc->read.rsvd1 = 0;
		desc->read.rsvd2 = 0;
#endif
	}

	ret = 0;
	goto l_end;

l_err_free_mbuf:
	for (j = 0; j <= i; j++) {
		if (buf_ring[j] != NULL && buf_ring[j]->next != NULL) {
			rte_pktmbuf_free(buf_ring[j]->next);
			buf_ring[j]->next = NULL;
		}

		if (buf_ring[j] != NULL) {
			rte_pktmbuf_free(buf_ring[j]);
			buf_ring[j] = NULL;
		}
	}

l_end:
	return ret;
}

int32_t __rte_cold sxe2_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sxe2_rx_queue *rxq;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret;
	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];
	if (rxq == NULL) {
		PMD_LOG_ERR(RX, "Rx queue %u is not available or setup",
				rx_queue_id);
		ret = -EINVAL;
		goto l_end;
	}

	if (dev->data->rx_queue_state[rx_queue_id] ==
			RTE_ETH_QUEUE_STATE_STARTED) {
		ret = 0;
		goto l_end;
	}

	ret = sxe2_rx_queue_mbufs_alloc(rxq);
	if (ret) {
		PMD_LOG_ERR(RX, "Rx queue %u apply desc ring fail",
			rx_queue_id);
		ret =  -ENOMEM;
		goto l_end;
	}

	sxe2_rx_head_tail_init(adapter, rxq);

	ret = sxe2_drv_rxq_ctxt_cfg(adapter, rxq, 1);
	if (ret) {
		PMD_LOG_ERR(RX, "Rx queue %u config ctxt fail, ret=%d",
			rx_queue_id, ret);

		(void)sxe2_drv_rxq_switch(adapter, rxq, false);
		rxq->ops.mbufs_release(rxq);
		rxq->ops.queue_reset(rxq);
		goto l_end;
	}

	SXE2_PCI_REG_WRITE_WC(rxq->rdt_reg_addr, rxq->ring_depth - 1);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

l_end:
	return  ret;
}

int32_t __rte_cold sxe2_rxqs_all_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct sxe2_rx_queue *rxq;
	uint16_t nb_rxq;
	uint16_t nb_started_rxq;
	int32_t ret;
	PMD_INIT_FUNC_TRACE();

	for (nb_rxq = 0; nb_rxq < data->nb_rx_queues; nb_rxq++) {
		rxq = dev->data->rx_queues[nb_rxq];
		if (!rxq || rxq->rx_deferred_start)
			continue;

		ret = sxe2_rx_queue_start(dev, nb_rxq);
		if (ret) {
			PMD_LOG_ERR(RX, "Fail to start rx queue %u", nb_rxq);
			goto l_free_started_queue;
		}

		rxq->sw_stats.pkts = 0;
		rxq->sw_stats.bytes = 0;
		rxq->sw_stats.drop_pkts = 0;
		rxq->sw_stats.drop_bytes = 0;
		rxq->sw_stats.unicast_pkts = 0;
		rxq->sw_stats.broadcast_pkts = 0;
		rxq->sw_stats.multicast_pkts = 0;
	}
	ret = 0;
	goto l_end;

l_free_started_queue:
	for (nb_started_rxq = 0; nb_started_rxq <= nb_rxq; nb_started_rxq++)
		(void)sxe2_rx_queue_stop(dev, nb_started_rxq);
l_end:
	return ret;
}

void __rte_cold sxe2_rxqs_all_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct sxe2_adapter *adapter  = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi     *vsi      = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats   *sw_stats_prev = &vsi->vsi_stats.vsi_sw_stats_prev;
	struct sxe2_rx_queue *rxq = NULL;
	int32_t ret;
	uint16_t nb_rxq;
	PMD_INIT_FUNC_TRACE();

	for (nb_rxq = 0; nb_rxq < data->nb_rx_queues; nb_rxq++) {
		ret = sxe2_rx_queue_stop(dev, nb_rxq);
		if (ret) {
			PMD_LOG_ERR(RX, "Fail to stop rx queue %u", nb_rxq);
			continue;
		}

		rxq = dev->data->rx_queues[nb_rxq];
		if (rxq) {
			sw_stats_prev->ipackets += rxq->sw_stats.pkts;
			sw_stats_prev->ierrors += rxq->sw_stats.drop_pkts;
			sw_stats_prev->ibytes += rxq->sw_stats.bytes;

			sw_stats_prev->rx_sw_unicast_packets += rxq->sw_stats.unicast_pkts;
			sw_stats_prev->rx_sw_broadcast_packets += rxq->sw_stats.broadcast_pkts;
			sw_stats_prev->rx_sw_multicast_packets += rxq->sw_stats.multicast_pkts;
			sw_stats_prev->rx_sw_drop_packets += rxq->sw_stats.drop_pkts;
			sw_stats_prev->rx_sw_drop_bytes += rxq->sw_stats.drop_bytes;
		}
	}
}
