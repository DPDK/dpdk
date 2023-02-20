/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_service.h>
#include <ethdev_pci.h>

#include "../nfp_common.h"
#include "../nfp_logs.h"
#include "../nfp_ctrl.h"
#include "../nfp_rxtx.h"
#include "nfp_flower.h"
#include "nfp_flower_ctrl.h"
#include "nfp_flower_cmsg.h"

#define MAX_PKT_BURST 32

static uint16_t
nfp_flower_ctrl_vnic_recv(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	uint64_t dma_addr;
	uint16_t avail = 0;
	struct rte_mbuf *mb;
	uint16_t nb_hold = 0;
	struct nfp_net_hw *hw;
	struct nfp_net_rxq *rxq;
	struct rte_mbuf *new_mb;
	struct nfp_net_rx_buff *rxb;
	struct nfp_net_rx_desc *rxds;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured
		 */
		PMD_RX_LOG(ERR, "RX Bad queue");
		return 0;
	}

	hw = rxq->hw;
	while (avail < nb_pkts) {
		rxb = &rxq->rxbufs[rxq->rd_p];
		if (unlikely(rxb == NULL)) {
			PMD_RX_LOG(ERR, "rxb does not exist!");
			break;
		}

		rxds = &rxq->rxds[rxq->rd_p];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		/*
		 * Memory barrier to ensure that we won't do other
		 * reads before the DD bit.
		 */
		rte_rmb();

		/*
		 * We got a packet. Let's alloc a new mbuf for refilling the
		 * free descriptor ring as soon as possible
		 */
		new_mb = rte_pktmbuf_alloc(rxq->mem_pool);
		if (unlikely(new_mb == NULL)) {
			PMD_RX_LOG(ERR,
				"RX mbuf alloc failed port_id=%u queue_id=%u",
				rxq->port_id, (unsigned int)rxq->qidx);
			nfp_net_mbuf_alloc_failed(rxq);
			break;
		}

		/*
		 * Grab the mbuf and refill the descriptor with the
		 * previously allocated mbuf
		 */
		mb = rxb->mbuf;
		rxb->mbuf = new_mb;

		/* Size of this segment */
		mb->data_len = rxds->rxd.data_len - NFP_DESC_META_LEN(rxds);
		/* Size of the whole packet. We just support 1 segment */
		mb->pkt_len = mb->data_len;

		if (unlikely((mb->data_len + hw->rx_offset) > rxq->mbuf_size)) {
			/*
			 * This should not happen and the user has the
			 * responsibility of avoiding it. But we have
			 * to give some info about the error
			 */
			PMD_RX_LOG(ERR,
				"mbuf overflow likely due to the RX offset.\n"
				"\t\tYour mbuf size should have extra space for"
				" RX offset=%u bytes.\n"
				"\t\tCurrently you just have %u bytes available"
				" but the received packet is %u bytes long",
				hw->rx_offset,
				rxq->mbuf_size - hw->rx_offset,
				mb->data_len);
			rte_pktmbuf_free(mb);
			break;
		}

		/* Filling the received mbuf with packet info */
		if (hw->rx_offset)
			mb->data_off = RTE_PKTMBUF_HEADROOM + hw->rx_offset;
		else
			mb->data_off = RTE_PKTMBUF_HEADROOM + NFP_DESC_META_LEN(rxds);

		/* No scatter mode supported */
		mb->nb_segs = 1;
		mb->next = NULL;
		mb->port = rxq->port_id;

		rx_pkts[avail++] = mb;

		/* Now resetting and updating the descriptor */
		rxds->vals[0] = 0;
		rxds->vals[1] = 0;
		dma_addr = rte_cpu_to_le_64(RTE_MBUF_DMA_ADDR_DEFAULT(new_mb));
		rxds->fld.dd = 0;
		rxds->fld.dma_addr_hi = (dma_addr >> 32) & 0xffff;
		rxds->fld.dma_addr_lo = dma_addr & 0xffffffff;
		nb_hold++;

		rxq->rd_p++;
		if (unlikely(rxq->rd_p == rxq->rx_count)) /* wrapping?*/
			rxq->rd_p = 0;
	}

	if (nb_hold == 0)
		return 0;

	nb_hold += rxq->nb_rx_hold;

	/*
	 * FL descriptors needs to be written before incrementing the
	 * FL queue WR pointer
	 */
	rte_wmb();
	if (nb_hold >= rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port=%hu queue=%d nb_hold=%hu avail=%hu",
			rxq->port_id, rxq->qidx, nb_hold, avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return avail;
}

uint16_t
nfp_flower_ctrl_vnic_xmit(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf)
{
	uint16_t cnt = 0;
	uint64_t dma_addr;
	uint32_t free_descs;
	struct rte_mbuf **lmbuf;
	struct nfp_net_txq *txq;
	struct nfp_net_hw *ctrl_hw;
	struct rte_eth_dev *ctrl_dev;
	struct nfp_net_nfd3_tx_desc *txds;

	ctrl_hw = app_fw_flower->ctrl_hw;
	ctrl_dev = ctrl_hw->eth_dev;

	/* Flower ctrl vNIC only has a single tx queue */
	txq = ctrl_dev->data->tx_queues[0];
	if (unlikely(txq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured
		 */
		PMD_TX_LOG(ERR, "ctrl dev TX Bad queue");
		goto xmit_end;
	}

	txds = &txq->txds[txq->wr_p];
	txds->vals[0] = 0;
	txds->vals[1] = 0;
	txds->vals[2] = 0;
	txds->vals[3] = 0;

	if (nfp_net_nfd3_txq_full(txq))
		nfp_net_tx_free_bufs(txq);

	free_descs = nfp_net_nfd3_free_tx_desc(txq);
	if (unlikely(free_descs == 0)) {
		PMD_TX_LOG(ERR, "ctrl dev no free descs");
		goto xmit_end;
	}

	lmbuf = &txq->txbufs[txq->wr_p].mbuf;
	RTE_MBUF_PREFETCH_TO_FREE(*lmbuf);
	if (*lmbuf)
		rte_pktmbuf_free_seg(*lmbuf);

	*lmbuf = mbuf;
	dma_addr = rte_mbuf_data_iova(mbuf);

	txds->data_len = mbuf->pkt_len;
	txds->dma_len = txds->data_len;
	txds->dma_addr_hi = (dma_addr >> 32) & 0xff;
	txds->dma_addr_lo = (dma_addr & 0xffffffff);
	txds->offset_eop = FLOWER_PKT_DATA_OFFSET | PCIE_DESC_TX_EOP;

	txq->wr_p++;
	if (unlikely(txq->wr_p == txq->tx_count)) /* wrapping?*/
		txq->wr_p = 0;

	cnt++;
	app_fw_flower->ctrl_vnic_tx_count++;

xmit_end:
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, 1);

	return cnt;
}

static void
nfp_flower_cmsg_rx_stats(struct nfp_flow_priv *flow_priv,
		struct rte_mbuf *mbuf)
{
	char *msg;
	uint16_t i;
	uint16_t count;
	uint16_t msg_len;
	uint32_t ctx_id;
	struct nfp_flower_stats_frame *stats;

	msg = rte_pktmbuf_mtod(mbuf, char *) + NFP_FLOWER_CMSG_HLEN;
	msg_len = mbuf->data_len - NFP_FLOWER_CMSG_HLEN;
	count = msg_len / sizeof(struct nfp_flower_stats_frame);

	rte_spinlock_lock(&flow_priv->stats_lock);
	for (i = 0; i < count; i++) {
		stats = (struct nfp_flower_stats_frame *)msg + i;
		ctx_id = rte_be_to_cpu_32(stats->stats_con_id);
		flow_priv->stats[ctx_id].pkts  += rte_be_to_cpu_32(stats->pkt_count);
		flow_priv->stats[ctx_id].bytes += rte_be_to_cpu_64(stats->byte_count);
	}
	rte_spinlock_unlock(&flow_priv->stats_lock);
}

static void
nfp_flower_cmsg_rx_qos_stats(struct nfp_mtr_priv *mtr_priv,
		struct rte_mbuf *mbuf)
{
	char *msg;
	uint32_t profile_id;
	struct nfp_mtr *mtr;
	struct nfp_mtr_stats_reply *mtr_stats;

	msg = rte_pktmbuf_mtod(mbuf, char *) + NFP_FLOWER_CMSG_HLEN;

	mtr_stats = (struct nfp_mtr_stats_reply *)msg;
	profile_id = rte_be_to_cpu_32(mtr_stats->head.profile_id);
	mtr = nfp_mtr_find_by_profile_id(mtr_priv, profile_id);
	if (mtr == NULL)
		return;

	rte_spinlock_lock(&mtr_priv->mtr_stats_lock);
	mtr->mtr_stats.curr.pass_bytes = rte_be_to_cpu_64(mtr_stats->pass_bytes);
	mtr->mtr_stats.curr.pass_pkts = rte_be_to_cpu_64(mtr_stats->pass_pkts);
	mtr->mtr_stats.curr.drop_bytes = rte_be_to_cpu_64(mtr_stats->drop_bytes);
	mtr->mtr_stats.curr.drop_pkts = rte_be_to_cpu_64(mtr_stats->drop_pkts);
	rte_spinlock_unlock(&mtr_priv->mtr_stats_lock);
}

static void
nfp_flower_cmsg_rx(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf **pkts_burst,
		uint16_t count)
{
	uint16_t i;
	char *meta;
	uint32_t meta_type;
	uint32_t meta_info;
	struct nfp_mtr_priv *mtr_priv;
	struct nfp_flow_priv *flow_priv;
	struct nfp_flower_cmsg_hdr *cmsg_hdr;

	mtr_priv = app_fw_flower->mtr_priv;
	flow_priv = app_fw_flower->flow_priv;

	for (i = 0; i < count; i++) {
		meta = rte_pktmbuf_mtod(pkts_burst[i], char *);

		/* Free the unsupported ctrl packet */
		meta_type = rte_be_to_cpu_32(*(uint32_t *)(meta - 8));
		meta_info = rte_be_to_cpu_32(*(uint32_t *)(meta - 4));
		if (meta_type != NFP_NET_META_PORTID ||
				meta_info != NFP_META_PORT_ID_CTRL) {
			PMD_DRV_LOG(ERR, "Incorrect metadata for ctrl packet!");
			rte_pktmbuf_free(pkts_burst[i]);
			continue;
		}

		cmsg_hdr = (struct nfp_flower_cmsg_hdr *)meta;
		if (unlikely(cmsg_hdr->version != NFP_FLOWER_CMSG_VER1)) {
			PMD_DRV_LOG(ERR, "Incorrect repr control version!");
			rte_pktmbuf_free(pkts_burst[i]);
			continue;
		}

		if (cmsg_hdr->type == NFP_FLOWER_CMSG_TYPE_FLOW_STATS) {
			/* We need to deal with stats updates from HW asap */
			nfp_flower_cmsg_rx_stats(flow_priv, pkts_burst[i]);
		} else if (cmsg_hdr->type == NFP_FLOWER_CMSG_TYPE_QOS_STATS) {
			/* Handle meter stats */
			nfp_flower_cmsg_rx_qos_stats(mtr_priv, pkts_burst[i]);
		}

		rte_pktmbuf_free(pkts_burst[i]);
	}
}

static void
nfp_mtr_stats_request(struct nfp_app_fw_flower *app_fw_flower)
{
	struct nfp_mtr *mtr;

	LIST_FOREACH(mtr, &app_fw_flower->mtr_priv->mtrs, next)
		(void)nfp_flower_cmsg_qos_stats(app_fw_flower, &mtr->mtr_profile->conf.head);
}

void
nfp_flower_ctrl_vnic_poll(struct nfp_app_fw_flower *app_fw_flower)
{
	uint16_t count;
	uint64_t cur_tsc;
	uint64_t drain_tsc;
	uint64_t pre_tsc = 0;
	struct nfp_net_rxq *rxq;
	struct nfp_net_hw *ctrl_hw;
	struct rte_eth_dev *ctrl_eth_dev;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	drain_tsc = app_fw_flower->mtr_priv->drain_tsc;

	ctrl_hw = app_fw_flower->ctrl_hw;
	ctrl_eth_dev = ctrl_hw->eth_dev;

	/* ctrl vNIC only has a single Rx queue */
	rxq = ctrl_eth_dev->data->rx_queues[0];

	while (rte_service_runstate_get(app_fw_flower->ctrl_vnic_id) != 0) {
		count = nfp_flower_ctrl_vnic_recv(rxq, pkts_burst, MAX_PKT_BURST);
		if (count != 0) {
			app_fw_flower->ctrl_vnic_rx_count += count;
			/* Process cmsgs here */
			nfp_flower_cmsg_rx(app_fw_flower, pkts_burst, count);
		}

		cur_tsc = rte_rdtsc();
		if (unlikely(cur_tsc - pre_tsc > drain_tsc)) {
			nfp_mtr_stats_request(app_fw_flower);
			pre_tsc = cur_tsc;
		}
	}
}
