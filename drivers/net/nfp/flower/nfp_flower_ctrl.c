/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_flower_ctrl.h"

#include <rte_service.h>

#include "../nfd3/nfp_nfd3.h"
#include "../nfdk/nfp_nfdk.h"
#include "../nfp_logs.h"
#include "nfp_flower_representor.h"
#include "nfp_mtr.h"

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
	struct nfp_net_dp_buf *rxb;
	struct nfp_net_rx_desc *rxds;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured.
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
		 * free descriptor ring as soon as possible.
		 */
		new_mb = rte_pktmbuf_alloc(rxq->mem_pool);
		if (unlikely(new_mb == NULL)) {
			PMD_RX_LOG(ERR, "RX mbuf alloc failed port_id=%u queue_id=%hu",
					rxq->port_id, rxq->qidx);
			nfp_net_mbuf_alloc_failed(rxq);
			break;
		}

		/*
		 * Grab the mbuf and refill the descriptor with the
		 * previously allocated mbuf.
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
			 * to give some info about the error.
			 */
			PMD_RX_LOG(ERR, "mbuf overflow likely due to the RX offset.");
			rte_pktmbuf_free(mb);
			break;
		}

		/* Filling the received mbuf with packet info */
		if (hw->rx_offset != 0)
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
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mb));
		rxds->fld.dd = 0;
		rxds->fld.dma_addr_hi = (dma_addr >> 32) & 0xffff;
		rxds->fld.dma_addr_lo = dma_addr & 0xffffffff;
		nb_hold++;

		rxq->rd_p++;
		if (unlikely(rxq->rd_p == rxq->rx_count)) /* Wrapping */
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
		PMD_RX_LOG(DEBUG, "port=%hu queue=%hu nb_hold=%hu avail=%hu",
				rxq->port_id, rxq->qidx, nb_hold, avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return avail;
}

static uint16_t
nfp_flower_ctrl_vnic_nfd3_xmit(struct nfp_app_fw_flower *app_fw_flower,
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
		 * enabled. But the queue needs to be configured.
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
	if (*lmbuf != NULL)
		rte_pktmbuf_free_seg(*lmbuf);

	*lmbuf = mbuf;
	dma_addr = rte_mbuf_data_iova(mbuf);

	txds->data_len = mbuf->pkt_len;
	txds->dma_len = txds->data_len;
	txds->dma_addr_hi = (dma_addr >> 32) & 0xff;
	txds->dma_addr_lo = (dma_addr & 0xffffffff);
	txds->offset_eop = FLOWER_PKT_DATA_OFFSET | NFD3_DESC_TX_EOP;

	txq->wr_p++;
	if (unlikely(txq->wr_p == txq->tx_count)) /* Wrapping */
		txq->wr_p = 0;

	cnt++;
	app_fw_flower->ctrl_vnic_tx_count++;

xmit_end:
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, 1);

	return cnt;
}

static uint16_t
nfp_flower_ctrl_vnic_nfdk_xmit(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf)
{
	int nop_descs;
	uint32_t type;
	uint32_t dma_len;
	uint32_t tmp_dlen;
	uint64_t dma_addr;
	uint32_t dlen_type;
	uint32_t used_descs;
	uint32_t free_descs;
	struct rte_mbuf **lmbuf;
	struct nfp_net_txq *txq;
	uint32_t issued_descs = 0;
	struct rte_eth_dev *ctrl_dev;
	struct nfp_net_nfdk_tx_desc *ktxds;

	ctrl_dev = app_fw_flower->ctrl_hw->eth_dev;

	/* Flower ctrl vNIC only has a single tx queue */
	txq = ctrl_dev->data->tx_queues[0];

	if (unlikely(mbuf->nb_segs > 1)) {
		PMD_TX_LOG(ERR, "Multisegment packet not supported");
		return 0;
	}

	if (nfp_net_nfdk_free_tx_desc(txq) < NFDK_TX_DESC_PER_SIMPLE_PKT ||
			nfp_net_nfdk_txq_full(txq))
		nfp_net_tx_free_bufs(txq);

	free_descs = nfp_net_nfdk_free_tx_desc(txq);
	if (unlikely(free_descs < NFDK_TX_DESC_PER_SIMPLE_PKT)) {
		PMD_TX_LOG(ERR, "ctrl dev no free descs");
		return 0;
	}

	nop_descs = nfp_net_nfdk_tx_maybe_close_block(txq, mbuf);
	if (nop_descs < 0)
		return 0;

	issued_descs += nop_descs;
	ktxds = &txq->ktxds[txq->wr_p];

	/*
	 * Checksum and VLAN flags just in the first descriptor for a
	 * multisegment packet, but TSO info needs to be in all of them.
	 */
	dma_len = mbuf->data_len;
	if (dma_len <= NFDK_TX_MAX_DATA_PER_HEAD)
		type = NFDK_DESC_TX_TYPE_SIMPLE;
	else
		type = NFDK_DESC_TX_TYPE_GATHER;

	/* Implicitly truncates to chunk in below logic */
	dma_len -= 1;

	/*
	 * We will do our best to pass as much data as we can in descriptor
	 * and we need to make sure the first descriptor includes whole
	 * head since there is limitation in firmware side. Sometimes the
	 * value of 'dma_len & NFDK_DESC_TX_DMA_LEN_HEAD' will be less
	 * than packet head len.
	 */
	if (dma_len > NFDK_DESC_TX_DMA_LEN_HEAD)
		dma_len = NFDK_DESC_TX_DMA_LEN_HEAD;
	dlen_type = dma_len | (NFDK_DESC_TX_TYPE_HEAD & (type << 12));
	ktxds->dma_len_type = rte_cpu_to_le_16(dlen_type);
	dma_addr = rte_mbuf_data_iova(mbuf);
	ktxds->dma_addr_hi = rte_cpu_to_le_16(dma_addr >> 32);
	ktxds->dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
	ktxds++;

	/*
	 * Preserve the original dlen_type, this way below the EOP logic
	 * can use dlen_type.
	 */
	tmp_dlen = dlen_type & NFDK_DESC_TX_DMA_LEN_HEAD;
	dma_len -= tmp_dlen;
	dma_addr += tmp_dlen + 1;

	/*
	 * The rest of the data (if any) will be in larger DMA descriptors
	 * and is handled with the dma_len loop.
	 */
	lmbuf = &txq->txbufs[txq->wr_p].mbuf;
	if (*lmbuf != NULL)
		rte_pktmbuf_free_seg(*lmbuf);
	*lmbuf = mbuf;
	while (dma_len > 0) {
		dma_len -= 1;
		dlen_type = NFDK_DESC_TX_DMA_LEN & dma_len;

		ktxds->dma_len_type = rte_cpu_to_le_16(dlen_type);
		ktxds->dma_addr_hi = rte_cpu_to_le_16(dma_addr >> 32);
		ktxds->dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
		ktxds++;

		dma_len -= dlen_type;
		dma_addr += dlen_type + 1;
	}

	(ktxds - 1)->dma_len_type = rte_cpu_to_le_16(dlen_type | NFDK_DESC_TX_EOP);

	ktxds->raw = rte_cpu_to_le_64(NFDK_DESC_TX_CHAIN_META);
	ktxds++;

	used_descs = ktxds - txq->ktxds - txq->wr_p;
	if (RTE_ALIGN_FLOOR(txq->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
			RTE_ALIGN_FLOOR(txq->wr_p + used_descs - 1, NFDK_TX_DESC_BLOCK_CNT)) {
		PMD_TX_LOG(INFO, "Used descs cross block boundary");
		return 0;
	}

	txq->wr_p = D_IDX(txq, txq->wr_p + used_descs);
	if (txq->wr_p % NFDK_TX_DESC_BLOCK_CNT != 0)
		txq->data_pending += mbuf->pkt_len;
	else
		txq->data_pending = 0;

	issued_descs += used_descs;

	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, issued_descs);

	return 1;
}

void
nfp_flower_ctrl_vnic_xmit_register(struct nfp_app_fw_flower *app_fw_flower)
{
	struct nfp_net_hw *hw;
	struct nfp_flower_nfd_func *nfd_func;

	hw = app_fw_flower->pf_hw;
	nfd_func = &app_fw_flower->nfd_func;

	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		nfd_func->ctrl_vnic_xmit_t = nfp_flower_ctrl_vnic_nfd3_xmit;
	else
		nfd_func->ctrl_vnic_xmit_t = nfp_flower_ctrl_vnic_nfdk_xmit;
}

uint16_t
nfp_flower_ctrl_vnic_xmit(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf)
{
	return app_fw_flower->nfd_func.ctrl_vnic_xmit_t(app_fw_flower, mbuf);
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

	msg = rte_pktmbuf_mtod_offset(mbuf, char *, NFP_FLOWER_CMSG_HLEN);
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

	msg = rte_pktmbuf_mtod_offset(mbuf, char *, NFP_FLOWER_CMSG_HLEN);

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

static int
nfp_flower_cmsg_port_mod_rx(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *pkt_burst)
{
	uint32_t port;
	struct nfp_flower_representor *repr;
	struct nfp_flower_cmsg_port_mod *msg;

	msg = rte_pktmbuf_mtod_offset(pkt_burst, struct nfp_flower_cmsg_port_mod *,
			NFP_FLOWER_CMSG_HLEN);
	port = rte_be_to_cpu_32(msg->portnum);

	switch (NFP_FLOWER_CMSG_PORT_TYPE(port)) {
	case NFP_FLOWER_CMSG_PORT_TYPE_PHYS_PORT:
		repr = app_fw_flower->phy_reprs[NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(port)];
		break;
	case NFP_FLOWER_CMSG_PORT_TYPE_PCIE_PORT:
		if (NFP_FLOWER_CMSG_PORT_VNIC_TYPE(port) == NFP_FLOWER_CMSG_PORT_VNIC_TYPE_VF)
			repr =  app_fw_flower->vf_reprs[NFP_FLOWER_CMSG_PORT_VNIC(port)];
		else
			repr = app_fw_flower->pf_repr;
		break;
	default:
		PMD_DRV_LOG(ERR, "ctrl msg for unknown port %#x", port);
		return -EINVAL;
	}

	repr->link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	if ((msg->info & NFP_FLOWER_CMSG_PORT_MOD_INFO_LINK) != 0)
		repr->link.link_status = RTE_ETH_LINK_UP;
	else
		repr->link.link_status = RTE_ETH_LINK_DOWN;

	return 0;
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
		} else if (cmsg_hdr->type == NFP_FLOWER_CMSG_TYPE_PORT_MOD) {
			/* Handle changes to port configuration/status */
			nfp_flower_cmsg_port_mod_rx(app_fw_flower, pkts_burst[i]);
		}

		rte_pktmbuf_free(pkts_burst[i]);
	}
}

void
nfp_flower_ctrl_vnic_poll(struct nfp_app_fw_flower *app_fw_flower)
{
	uint16_t count;
	struct nfp_net_rxq *rxq;
	struct nfp_net_hw *ctrl_hw;
	struct rte_eth_dev *ctrl_eth_dev;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	ctrl_hw = app_fw_flower->ctrl_hw;
	ctrl_eth_dev = ctrl_hw->eth_dev;

	/* Ctrl vNIC only has a single Rx queue */
	rxq = ctrl_eth_dev->data->rx_queues[0];

	while (rte_service_runstate_get(app_fw_flower->ctrl_vnic_id) != 0) {
		count = nfp_flower_ctrl_vnic_recv(rxq, pkts_burst, MAX_PKT_BURST);
		if (count != 0) {
			app_fw_flower->ctrl_vnic_rx_count += count;
			/* Process cmsgs here */
			nfp_flower_cmsg_rx(app_fw_flower, pkts_burst, count);
		}
	}
}
