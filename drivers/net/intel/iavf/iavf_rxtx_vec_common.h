/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_RXTX_VEC_COMMON_H_
#define _IAVF_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "../common/rx.h"
#include "iavf.h"
#include "iavf_rxtx.h"

static inline int
iavf_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	return (txq->iavf_tx_ring[idx].cmd_type_offset_bsz &
			rte_cpu_to_le_64(IAVF_TXD_QW1_DTYPE_MASK)) ==
				rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE);
}

static inline void
_iavf_rx_queue_release_mbufs_vec(struct iavf_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (!rxq->sw_ring || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i])
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->sw_ring[i])
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static inline int
iavf_rx_vec_queue_default(struct iavf_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!rte_is_power_of_2(rxq->nb_rx_desc))
		return -1;

	if (rxq->rx_free_thresh < IAVF_VPMD_RX_BURST)
		return -1;

	if (rxq->nb_rx_desc % rxq->rx_free_thresh)
		return -1;

	if (rxq->proto_xtr != IAVF_PROTO_XTR_NONE)
		return -1;

	if (rxq->offloads & IAVF_RX_VECTOR_OFFLOAD)
		return IAVF_VECTOR_OFFLOAD_PATH;

	return IAVF_VECTOR_PATH;
}

static inline int
iavf_tx_vec_queue_default(struct ci_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (txq->tx_rs_thresh < IAVF_VPMD_TX_BURST ||
	    txq->tx_rs_thresh > IAVF_VPMD_TX_MAX_FREE_BUF)
		return -1;

	if (txq->offloads & IAVF_TX_NO_VECTOR_FLAGS)
		return -1;

	if (rte_pmd_iavf_tx_lldp_dynfield_offset > 0) {
		txq->use_ctx = 1;
		return IAVF_VECTOR_CTX_PATH;
	}

	/**
	 * Vlan tci needs to be inserted via ctx desc, if the vlan_flag is L2TAG2.
	 * Tunneling parameters and other fields need be configured in ctx desc
	 * if the outer checksum offload is enabled.
	 */
	if (txq->offloads & (IAVF_TX_VECTOR_OFFLOAD | IAVF_TX_VECTOR_OFFLOAD_CTX)) {
		if (txq->offloads & IAVF_TX_VECTOR_OFFLOAD_CTX) {
			if (txq->vlan_flag == IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2) {
				txq->use_ctx = 1;
				return IAVF_VECTOR_CTX_OFFLOAD_PATH;
			} else {
				return -1;
			}
		} else {
			return IAVF_VECTOR_OFFLOAD_PATH;
		}
	} else {
		return IAVF_VECTOR_PATH;
	}
}

static inline int
iavf_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct iavf_rx_queue *rxq;
	int ret;
	int result = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		ret = iavf_rx_vec_queue_default(rxq);

		if (ret < 0)
			return -1;
		if (ret > result)
			result = ret;
	}

	return result;
}

static inline int
iavf_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_tx_queue *txq;
	int ret;
	int result = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = iavf_tx_vec_queue_default(txq);

		if (ret < 0)
			return -1;
		if (ret > result)
			result = ret;
	}

	return result;
}

/******************************************************************************
 * If user knows a specific offload is not enabled by APP,
 * the macro can be commented to save the effort of fast path.
 * Currently below 2 features are supported in TX path,
 * 1, checksum offload
 * 2, VLAN/QINQ insertion
 ******************************************************************************/
#define IAVF_TX_CSUM_OFFLOAD
#define IAVF_TX_VLAN_QINQ_OFFLOAD

static __rte_always_inline void
iavf_txd_enable_offload(__rte_unused struct rte_mbuf *tx_pkt,
			uint64_t *txd_hi)
{
#if defined(IAVF_TX_CSUM_OFFLOAD) || defined(IAVF_TX_VLAN_QINQ_OFFLOAD)
	uint64_t ol_flags = tx_pkt->ol_flags;
#endif
	uint32_t td_cmd = 0;
#ifdef IAVF_TX_CSUM_OFFLOAD
	uint32_t td_offset = 0;
#endif

#ifdef IAVF_TX_CSUM_OFFLOAD
	/* Set MACLEN */
	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		td_offset |= (tx_pkt->outer_l2_len >> 1)
			<< IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;
	else
		td_offset |= (tx_pkt->l2_len >> 1)
			<< IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;

	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		if (ol_flags & RTE_MBUF_F_TX_IPV4) {
			td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM;
			td_offset |= (tx_pkt->l3_len >> 2) <<
				     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
		}
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV6;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
		td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_SCTP;
		td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_UDP;
		td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		break;
	}

	*txd_hi |= ((uint64_t)td_offset) << IAVF_TXD_QW1_OFFSET_SHIFT;
#endif

#ifdef IAVF_TX_VLAN_QINQ_OFFLOAD
	if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		td_cmd |= IAVF_TX_DESC_CMD_IL2TAG1;
		*txd_hi |= ((uint64_t)tx_pkt->vlan_tci <<
			    IAVF_TXD_QW1_L2TAG1_SHIFT);
	}
#endif

	*txd_hi |= ((uint64_t)td_cmd) << IAVF_TXD_QW1_CMD_SHIFT;
}

#ifdef RTE_ARCH_X86
static __rte_always_inline void
iavf_rxq_rearm_common(struct iavf_rx_queue *rxq, __rte_unused bool avx512)
{
	int i;
	uint16_t rx_id;
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start];

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxp,
				 IAVF_VPMD_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + IAVF_VPMD_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IAVF_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rxq->fake_mbuf;
				_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[i].read),
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			IAVF_VPMD_RXQ_REARM_THRESH;
		return;
	}

	struct rte_mbuf *mb0, *mb1;
	__m128i dma_addr0, dma_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < IAVF_VPMD_RXQ_REARM_THRESH; i += 2, rxp += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxp[0];
		mb1 = rxp[1];

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
		vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = _mm_add_epi64(dma_addr0, hdr_room);
		dma_addr1 = _mm_add_epi64(dma_addr1, hdr_room);

		/* flush desc with pa dma_addr */
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp++->read), dma_addr0);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp++->read), dma_addr1);
	}

	rxq->rxrearm_start += IAVF_VPMD_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IAVF_VPMD_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IAVF_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
}
#endif

#endif
