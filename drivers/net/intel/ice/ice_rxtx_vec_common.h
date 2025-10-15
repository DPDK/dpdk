/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_RXTX_VEC_COMMON_H_
#define _ICE_RXTX_VEC_COMMON_H_

#include "../common/rx.h"
#include "ice_rxtx.h"

static inline int
ice_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	return (txq->ice_tx_ring[idx].cmd_type_offset_bsz &
			rte_cpu_to_le_64(ICE_TXD_QW1_DTYPE_M)) ==
				rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE);
}

static inline void
_ice_rx_queue_release_mbufs_vec(struct ci_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (unlikely(!rxq->sw_ring)) {
		PMD_DRV_LOG(DEBUG, "sw_ring is NULL");
		return;
	}

	if (rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->sw_ring[i].mbuf)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

#define ICE_TX_NO_VECTOR_FLAGS (			\
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |		\
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |	\
		RTE_ETH_TX_OFFLOAD_TCP_TSO |	\
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |    \
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |    \
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |    \
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |    \
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)

#define ICE_TX_VECTOR_OFFLOAD (				\
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |		\
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |		\
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |		\
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |		\
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |		\
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM)

#define ICE_VECTOR_PATH		0
#define ICE_VECTOR_OFFLOAD_PATH	1

static inline int
ice_rx_vec_queue_default(struct ci_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!ci_rxq_vec_capable(rxq->nb_rx_desc, rxq->rx_free_thresh))
		return -1;

	if (rxq->proto_xtr != PROTO_XTR_NONE)
		return -1;

	return 0;
}

static inline int
ice_tx_vec_queue_default(struct ci_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (txq->tx_rs_thresh < ICE_VPMD_TX_BURST ||
	    txq->tx_rs_thresh > ICE_TX_MAX_FREE_BUF_SZ)
		return -1;

	if (txq->offloads & ICE_TX_NO_VECTOR_FLAGS)
		return -1;

	if (txq->offloads & ICE_TX_VECTOR_OFFLOAD)
		return ICE_VECTOR_OFFLOAD_PATH;

	return ICE_VECTOR_PATH;
}

static inline int
ice_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_rx_queue *rxq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		ret = (ice_rx_vec_queue_default(rxq));
		if (ret < 0)
			break;
	}

	return ret;
}

static inline int
ice_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_tx_queue *txq;
	int ret = 0;
	int result = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = ice_tx_vec_queue_default(txq);
		if (ret < 0)
			return -1;
		if (ret == ICE_VECTOR_OFFLOAD_PATH)
			result = ret;
	}

	return result;
}

static inline void
ice_txd_enable_offload(struct rte_mbuf *tx_pkt,
		       uint64_t *txd_hi)
{
	uint64_t ol_flags = tx_pkt->ol_flags;
	uint32_t td_cmd = 0;
	uint32_t td_offset = 0;

	/* Tx Checksum Offload */
	/* SET MACLEN */
	td_offset |= (tx_pkt->l2_len >> 1) <<
		ICE_TX_DESC_LEN_MACLEN_S;

	/* Enable L3 checksum offload */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV4_CSUM;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV4;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV6;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_TCP;
		td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
			ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_SCTP;
		td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
			ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_UDP;
		td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
			ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	default:
		break;
	}

	*txd_hi |= ((uint64_t)td_offset) << ICE_TXD_QW1_OFFSET_S;

	/* Tx VLAN/QINQ insertion Offload */
	if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		td_cmd |= ICE_TX_DESC_CMD_IL2TAG1;
		*txd_hi |= ((uint64_t)tx_pkt->vlan_tci <<
				ICE_TXD_QW1_L2TAG1_S);
	}

	*txd_hi |= ((uint64_t)td_cmd) << ICE_TXD_QW1_CMD_S;
}
#endif
