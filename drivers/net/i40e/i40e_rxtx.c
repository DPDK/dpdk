/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "i40e_logs.h"
#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#define DEFAULT_TX_RS_THRESH   32
#define DEFAULT_TX_FREE_THRESH 32

#define I40E_TX_MAX_BURST  32

#define I40E_DMA_MEM_ALIGN 4096

/* Base address of the HW descriptor ring should be 128B aligned. */
#define I40E_RING_BASE_ALIGN	128

#define I40E_TXD_CMD (I40E_TX_DESC_CMD_EOP | I40E_TX_DESC_CMD_RS)

#ifdef RTE_LIBRTE_IEEE1588
#define I40E_TX_IEEE1588_TMST RTE_MBUF_F_TX_IEEE1588_TMST
#else
#define I40E_TX_IEEE1588_TMST 0
#endif

#define I40E_TX_CKSUM_OFFLOAD_MASK (RTE_MBUF_F_TX_IP_CKSUM |		 \
		RTE_MBUF_F_TX_L4_MASK |		 \
		RTE_MBUF_F_TX_TCP_SEG |		 \
		RTE_MBUF_F_TX_OUTER_IP_CKSUM)

#define I40E_TX_OFFLOAD_MASK (RTE_MBUF_F_TX_OUTER_IPV4 |	\
		RTE_MBUF_F_TX_OUTER_IPV6 |	\
		RTE_MBUF_F_TX_IPV4 |		\
		RTE_MBUF_F_TX_IPV6 |		\
		RTE_MBUF_F_TX_IP_CKSUM |       \
		RTE_MBUF_F_TX_L4_MASK |        \
		RTE_MBUF_F_TX_OUTER_IP_CKSUM | \
		RTE_MBUF_F_TX_TCP_SEG |        \
		RTE_MBUF_F_TX_QINQ |       \
		RTE_MBUF_F_TX_VLAN |	\
		RTE_MBUF_F_TX_TUNNEL_MASK |	\
		RTE_MBUF_F_TX_OUTER_UDP_CKSUM |	\
		I40E_TX_IEEE1588_TMST)

#define I40E_TX_OFFLOAD_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ I40E_TX_OFFLOAD_MASK)

#define I40E_TX_OFFLOAD_SIMPLE_SUP_MASK (RTE_MBUF_F_TX_IPV4 | \
		RTE_MBUF_F_TX_IPV6 | \
		RTE_MBUF_F_TX_OUTER_IPV4 | \
		RTE_MBUF_F_TX_OUTER_IPV6)

#define I40E_TX_OFFLOAD_SIMPLE_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ I40E_TX_OFFLOAD_SIMPLE_SUP_MASK)

static int
i40e_monitor_callback(const uint64_t value,
		const uint64_t arg[RTE_POWER_MONITOR_OPAQUE_SZ] __rte_unused)
{
	const uint64_t m = rte_cpu_to_le_64(1 << I40E_RX_DESC_STATUS_DD_SHIFT);
	/*
	 * we expect the DD bit to be set to 1 if this descriptor was already
	 * written to.
	 */
	return (value & m) == m ? -1 : 0;
}

int
i40e_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	struct i40e_rx_queue *rxq = rx_queue;
	volatile union i40e_rx_desc *rxdp;
	uint16_t desc;

	desc = rxq->rx_tail;
	rxdp = &rxq->rx_ring[desc];
	/* watch for changes in status bit */
	pmc->addr = &rxdp->wb.qword1.status_error_len;

	/* comparison callback */
	pmc->fn = i40e_monitor_callback;

	/* registers are 64-bit */
	pmc->size = sizeof(uint64_t);

	return 0;
}

static inline void
i40e_rxd_to_vlan_tci(struct rte_mbuf *mb, volatile union i40e_rx_desc *rxdp)
{
	if (rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		(1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT)) {
		mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		mb->vlan_tci =
			rte_le_to_cpu_16(rxdp->wb.qword0.lo_dword.l2tag1);
		PMD_RX_LOG(DEBUG, "Descriptor l2tag1: %u",
			   rte_le_to_cpu_16(rxdp->wb.qword0.lo_dword.l2tag1));
	} else {
		mb->vlan_tci = 0;
	}
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	if (rte_le_to_cpu_16(rxdp->wb.qword2.ext_status) &
		(1 << I40E_RX_DESC_EXT_STATUS_L2TAG2P_SHIFT)) {
		mb->ol_flags |= RTE_MBUF_F_RX_QINQ_STRIPPED | RTE_MBUF_F_RX_QINQ |
			RTE_MBUF_F_RX_VLAN_STRIPPED | RTE_MBUF_F_RX_VLAN;
		mb->vlan_tci_outer = mb->vlan_tci;
		mb->vlan_tci = rte_le_to_cpu_16(rxdp->wb.qword2.l2tag2_2);
		PMD_RX_LOG(DEBUG, "Descriptor l2tag2_1: %u, l2tag2_2: %u",
			   rte_le_to_cpu_16(rxdp->wb.qword2.l2tag2_1),
			   rte_le_to_cpu_16(rxdp->wb.qword2.l2tag2_2));
	} else {
		mb->vlan_tci_outer = 0;
	}
#endif
	PMD_RX_LOG(DEBUG, "Mbuf vlan_tci: %u, vlan_tci_outer: %u",
		   mb->vlan_tci, mb->vlan_tci_outer);
}

/* Translate the rx descriptor status to pkt flags */
static inline uint64_t
i40e_rxd_status_to_pkt_flags(uint64_t qword)
{
	uint64_t flags;

	/* Check if RSS_HASH */
	flags = (((qword >> I40E_RX_DESC_STATUS_FLTSTAT_SHIFT) &
					I40E_RX_DESC_FLTSTAT_RSS_HASH) ==
			I40E_RX_DESC_FLTSTAT_RSS_HASH) ? RTE_MBUF_F_RX_RSS_HASH : 0;

	/* Check if FDIR Match */
	flags |= (qword & (1 << I40E_RX_DESC_STATUS_FLM_SHIFT) ?
							RTE_MBUF_F_RX_FDIR : 0);

	return flags;
}

static inline uint64_t
i40e_rxd_error_to_pkt_flags(uint64_t qword)
{
	uint64_t flags = 0;
	uint64_t error_bits = (qword >> I40E_RXD_QW1_ERROR_SHIFT);

#define I40E_RX_ERR_BITS 0x3f
	if (likely((error_bits & I40E_RX_ERR_BITS) == 0)) {
		flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_IPE_SHIFT)))
		flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_L4E_SHIFT)))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_EIPE_SHIFT)))
		flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

	return flags;
}

/* Function to check and set the ieee1588 timesync index and get the
 * appropriate flags.
 */
#ifdef RTE_LIBRTE_IEEE1588
static inline uint64_t
i40e_get_iee15888_flags(struct rte_mbuf *mb, uint64_t qword)
{
	uint64_t pkt_flags = 0;
	uint16_t tsyn = (qword & (I40E_RXD_QW1_STATUS_TSYNVALID_MASK
				  | I40E_RXD_QW1_STATUS_TSYNINDX_MASK))
				    >> I40E_RX_DESC_STATUS_TSYNINDX_SHIFT;

	if ((mb->packet_type & RTE_PTYPE_L2_MASK)
			== RTE_PTYPE_L2_ETHER_TIMESYNC)
		pkt_flags = RTE_MBUF_F_RX_IEEE1588_PTP;
	if (tsyn & 0x04) {
		pkt_flags |= RTE_MBUF_F_RX_IEEE1588_TMST;
		mb->timesync = tsyn & 0x03;
	}

	return pkt_flags;
}
#endif

static inline uint64_t
i40e_rxd_build_fdir(volatile union i40e_rx_desc *rxdp, struct rte_mbuf *mb)
{
	uint64_t flags = 0;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	uint16_t flexbh, flexbl;

	flexbh = (rte_le_to_cpu_32(rxdp->wb.qword2.ext_status) >>
		I40E_RX_DESC_EXT_STATUS_FLEXBH_SHIFT) &
		I40E_RX_DESC_EXT_STATUS_FLEXBH_MASK;
	flexbl = (rte_le_to_cpu_32(rxdp->wb.qword2.ext_status) >>
		I40E_RX_DESC_EXT_STATUS_FLEXBL_SHIFT) &
		I40E_RX_DESC_EXT_STATUS_FLEXBL_MASK;


	if (flexbh == I40E_RX_DESC_EXT_STATUS_FLEXBH_FD_ID) {
		mb->hash.fdir.hi =
			rte_le_to_cpu_32(rxdp->wb.qword3.hi_dword.fd_id);
		flags |= RTE_MBUF_F_RX_FDIR_ID;
	} else if (flexbh == I40E_RX_DESC_EXT_STATUS_FLEXBH_FLEX) {
		mb->hash.fdir.hi =
			rte_le_to_cpu_32(rxdp->wb.qword3.hi_dword.flex_bytes_hi);
		flags |= RTE_MBUF_F_RX_FDIR_FLX;
	}
	if (flexbl == I40E_RX_DESC_EXT_STATUS_FLEXBL_FLEX) {
		mb->hash.fdir.lo =
			rte_le_to_cpu_32(rxdp->wb.qword3.lo_dword.flex_bytes_lo);
		flags |= RTE_MBUF_F_RX_FDIR_FLX;
	}
#else
	mb->hash.fdir.hi =
		rte_le_to_cpu_32(rxdp->wb.qword0.hi_dword.fd_id);
	flags |= RTE_MBUF_F_RX_FDIR_ID;
#endif
	return flags;
}

static inline void
i40e_parse_tunneling_params(uint64_t ol_flags,
			    union i40e_tx_offload tx_offload,
			    uint32_t *cd_tunneling)
{
	/* EIPT: External (outer) IP header type */
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM)
		*cd_tunneling |= I40E_TX_CTX_EXT_IP_IPV4;
	else if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV4)
		*cd_tunneling |= I40E_TX_CTX_EXT_IP_IPV4_NO_CSUM;
	else if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6)
		*cd_tunneling |= I40E_TX_CTX_EXT_IP_IPV6;

	/* EIPLEN: External (outer) IP header length, in DWords */
	*cd_tunneling |= (tx_offload.outer_l3_len >> 2) <<
		I40E_TXD_CTX_QW0_EXT_IPLEN_SHIFT;

	/* L4TUNT: L4 Tunneling Type */
	switch (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_IPIP:
		/* for non UDP / GRE tunneling, set to 00b */
		break;
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		*cd_tunneling |= I40E_TXD_CTX_UDP_TUNNELING;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		*cd_tunneling |= I40E_TXD_CTX_GRE_TUNNELING;
		break;
	default:
		PMD_TX_LOG(ERR, "Tunnel type not supported");
		return;
	}

	/* L4TUNLEN: L4 Tunneling Length, in Words
	 *
	 * We depend on app to set rte_mbuf.l2_len correctly.
	 * For IP in GRE it should be set to the length of the GRE
	 * header;
	 * for MAC in GRE or MAC in UDP it should be set to the length
	 * of the GRE or UDP headers plus the inner MAC up to including
	 * its last Ethertype.
	 */
	*cd_tunneling |= (tx_offload.l2_len >> 1) <<
		I40E_TXD_CTX_QW0_NATLEN_SHIFT;
}

static inline void
i40e_txd_enable_checksum(uint64_t ol_flags,
			uint32_t *td_cmd,
			uint32_t *td_offset,
			union i40e_tx_offload tx_offload)
{
	/* Set MACLEN */
	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		*td_offset |= (tx_offload.outer_l2_len >> 1)
				<< I40E_TX_DESC_LENGTH_MACLEN_SHIFT;
	else
		*td_offset |= (tx_offload.l2_len >> 1)
			<< I40E_TX_DESC_LENGTH_MACLEN_SHIFT;

	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV4_CSUM;
		*td_offset |= (tx_offload.l3_len >> 2)
				<< I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV4;
		*td_offset |= (tx_offload.l3_len >> 2)
				<< I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV6;
		*td_offset |= (tx_offload.l3_len >> 2)
				<< I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	}

	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (tx_offload.l4_len >> 2)
			<< I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		return;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_SCTP;
		*td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		break;
	}
}

/* Construct the tx flags */
static inline uint64_t
i40e_build_ctob(uint32_t td_cmd,
		uint32_t td_offset,
		unsigned int size,
		uint32_t td_tag)
{
	return rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)td_cmd  << I40E_TXD_QW1_CMD_SHIFT) |
			((uint64_t)td_offset << I40E_TXD_QW1_OFFSET_SHIFT) |
			((uint64_t)size  << I40E_TXD_QW1_TX_BUF_SZ_SHIFT) |
			((uint64_t)td_tag  << I40E_TXD_QW1_L2TAG1_SHIFT));
}

static inline int
i40e_xmit_cleanup(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *sw_ring = txq->sw_ring;
	volatile struct i40e_tx_desc *txd = txq->tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if ((txd[desc_to_clean_to].cmd_type_offset_bsz &
			rte_cpu_to_le_64(I40E_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE)) {
		PMD_TX_LOG(DEBUG, "TX descriptor %4u is not done "
			   "(port=%d queue=%d)", desc_to_clean_to,
			   txq->port_id, txq->queue_id);
		return -1;
	}

	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
							desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
					last_desc_cleaned);

	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	return 0;
}

static inline int
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
check_rx_burst_bulk_alloc_preconditions(struct i40e_rx_queue *rxq)
#else
check_rx_burst_bulk_alloc_preconditions(__rte_unused struct i40e_rx_queue *rxq)
#endif
{
	int ret = 0;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (!(rxq->rx_free_thresh >= RTE_PMD_I40E_RX_MAX_BURST)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "RTE_PMD_I40E_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, RTE_PMD_I40E_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if (rxq->nb_rx_desc % rxq->rx_free_thresh != 0) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->nb_rx_desc=%d, "
			     "rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}
#else
	ret = -EINVAL;
#endif

	return ret;
}

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
#define I40E_LOOK_AHEAD 8
#if (I40E_LOOK_AHEAD != 8)
#error "PMD I40E: I40E_LOOK_AHEAD must be 8\n"
#endif
static inline int
i40e_rx_scan_hw_ring(struct i40e_rx_queue *rxq)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	uint64_t qword1;
	uint32_t rx_status;
	int32_t s[I40E_LOOK_AHEAD], var, nb_dd;
	int32_t i, j, nb_rx = 0;
	uint64_t pkt_flags;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
	rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
				I40E_RXD_QW1_STATUS_SHIFT;

	/* Make sure there is at least 1 packet to receive */
	if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/**
	 * Scan LOOK_AHEAD descriptors at a time to determine which
	 * descriptors reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_I40E_RX_MAX_BURST; i+=I40E_LOOK_AHEAD,
			rxdp += I40E_LOOK_AHEAD, rxep += I40E_LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = I40E_LOOK_AHEAD - 1; j >= 0; j--) {
			qword1 = rte_le_to_cpu_64(\
				rxdp[j].wb.qword1.status_error_len);
			s[j] = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
					I40E_RXD_QW1_STATUS_SHIFT;
		}

		/* This barrier is to order loads of different words in the descriptor */
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

		/* Compute how many status bits were set */
		for (j = 0, nb_dd = 0; j < I40E_LOOK_AHEAD; j++) {
			var = s[j] & (1 << I40E_RX_DESC_STATUS_DD_SHIFT);
#ifdef RTE_ARCH_ARM
			/* For Arm platforms, only compute continuous status bits */
			if (var)
				nb_dd += 1;
			else
				break;
#else
			nb_dd += var;
#endif
		}

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf parameters */
		for (j = 0; j < nb_dd; j++) {
			mb = rxep[j].mbuf;
			qword1 = rte_le_to_cpu_64(\
				rxdp[j].wb.qword1.status_error_len);
			pkt_len = ((qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
				I40E_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;
			mb->ol_flags = 0;
			i40e_rxd_to_vlan_tci(mb, &rxdp[j]);
			pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
			pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
			mb->packet_type =
				ptype_tbl[(uint8_t)((qword1 &
				I40E_RXD_QW1_PTYPE_MASK) >>
				I40E_RXD_QW1_PTYPE_SHIFT)];
			if (pkt_flags & RTE_MBUF_F_RX_RSS_HASH)
				mb->hash.rss = rte_le_to_cpu_32(\
					rxdp[j].wb.qword0.hi_dword.rss);
			if (pkt_flags & RTE_MBUF_F_RX_FDIR)
				pkt_flags |= i40e_rxd_build_fdir(&rxdp[j], mb);

#ifdef RTE_LIBRTE_IEEE1588
			pkt_flags |= i40e_get_iee15888_flags(mb, qword1);
#endif
			mb->ol_flags |= pkt_flags;

		}

		for (j = 0; j < I40E_LOOK_AHEAD; j++)
			rxq->rx_stage[i + j] = rxep[j].mbuf;

		if (nb_dd != I40E_LOOK_AHEAD)
			break;
	}

	/* Clear software ring entries */
	for (i = 0; i < nb_rx; i++)
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;

	return nb_rx;
}

static inline uint16_t
i40e_rx_fill_from_stage(struct i40e_rx_queue *rxq,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	uint16_t i;
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];

	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	for (i = 0; i < nb_pkts; i++)
		rx_pkts[i] = stage[i];

	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline int
i40e_rx_alloc_bufs(struct i40e_rx_queue *rxq)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx, i;
	uint64_t dma_addr;
	int diag;

	/* Allocate buffers in bulk */
	alloc_idx = (uint16_t)(rxq->rx_free_trigger -
				(rxq->rx_free_thresh - 1));
	rxep = &(rxq->sw_ring[alloc_idx]);
	diag = rte_mempool_get_bulk(rxq->mp, (void *)rxep,
					rxq->rx_free_thresh);
	if (unlikely(diag != 0)) {
		PMD_DRV_LOG(ERR, "Failed to get mbufs in bulk");
		return -ENOMEM;
	}

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; i++) {
		if (likely(i < (rxq->rx_free_thresh - 1)))
			/* Prefetch next mbuf */
			rte_prefetch0(rxep[i + 1].mbuf);

		mb = rxep[i].mbuf;
		rte_mbuf_refcnt_set(mb, 1);
		mb->next = NULL;
		mb->data_off = RTE_PKTMBUF_HEADROOM;
		mb->nb_segs = 1;
		mb->port = rxq->port_id;
		dma_addr = rte_cpu_to_le_64(\
			rte_mbuf_data_iova_default(mb));
		rxdp[i].read.hdr_addr = 0;
		rxdp[i].read.pkt_addr = dma_addr;
	}

	/* Update rx tail register */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->rx_free_trigger);

	rxq->rx_free_trigger =
		(uint16_t)(rxq->rx_free_trigger + rxq->rx_free_thresh);
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	return 0;
}

static inline uint16_t
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = (struct i40e_rx_queue *)rx_queue;
	struct rte_eth_dev *dev;
	uint16_t nb_rx = 0;

	if (!nb_pkts)
		return 0;

	if (rxq->rx_nb_avail)
		return i40e_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	nb_rx = (uint16_t)i40e_rx_scan_hw_ring(rxq);
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	if (rxq->rx_tail > rxq->rx_free_trigger) {
		if (i40e_rx_alloc_bufs(rxq) != 0) {
			uint16_t i, j;

			dev = I40E_VSI_TO_ETH_DEV(rxq->vsi);
			dev->data->rx_mbuf_alloc_failed +=
				rxq->rx_free_thresh;

			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; i++, j++)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	if (rxq->rx_nb_avail)
		return i40e_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

static uint16_t
i40e_recv_pkts_bulk_alloc(void *rx_queue,
			  struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts)
{
	uint16_t nb_rx = 0, n, count;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_I40E_RX_MAX_BURST))
		return rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	while (nb_pkts) {
		n = RTE_MIN(nb_pkts, RTE_PMD_I40E_RX_MAX_BURST);
		count = rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + count);
		nb_pkts = (uint16_t)(nb_pkts - count);
		if (count < n)
			break;
	}

	return nb_rx;
}
#else
static uint16_t
i40e_recv_pkts_bulk_alloc(void __rte_unused *rx_queue,
			  struct rte_mbuf __rte_unused **rx_pkts,
			  uint16_t __rte_unused nb_pkts)
{
	return 0;
}
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */

uint16_t
i40e_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq;
	volatile union i40e_rx_desc *rx_ring;
	volatile union i40e_rx_desc *rxdp;
	union i40e_rx_desc rxd;
	struct i40e_rx_entry *sw_ring;
	struct i40e_rx_entry *rxe;
	struct rte_eth_dev *dev;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	uint16_t nb_rx;
	uint32_t rx_status;
	uint64_t qword1;
	uint16_t rx_packet_len;
	uint16_t rx_id, nb_hold;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	uint32_t *ptype_tbl;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK)
				>> I40E_RXD_QW1_STATUS_SHIFT;

		/* Check the DD bit first */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
			break;

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			dev = I40E_VSI_TO_ETH_DEV(rxq->vsi);
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		/**
		 * Use acquire fence to ensure that qword1 which includes DD
		 * bit is loaded before loading of other descriptor words.
		 */
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

		rxd = *rxdp;
		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(sw_ring[rx_id].mbuf);

		/**
		 * When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;

		rx_packet_len = ((qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
				I40E_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(RTE_PTR_ADD(rxm->buf_addr, RTE_PKTMBUF_HEADROOM));
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = rx_packet_len;
		rxm->data_len = rx_packet_len;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;
		i40e_rxd_to_vlan_tci(rxm, &rxd);
		pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
		rxm->packet_type =
			ptype_tbl[(uint8_t)((qword1 &
			I40E_RXD_QW1_PTYPE_MASK) >> I40E_RXD_QW1_PTYPE_SHIFT)];
		if (pkt_flags & RTE_MBUF_F_RX_RSS_HASH)
			rxm->hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);
		if (pkt_flags & RTE_MBUF_F_RX_FDIR)
			pkt_flags |= i40e_rxd_build_fdir(&rxd, rxm);

#ifdef RTE_LIBRTE_IEEE1588
		pkt_flags |= i40e_get_iee15888_flags(rxm, qword1);
#endif
		rxm->ol_flags |= pkt_flags;

		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the receive tail register of queue.
	 * Update that register with the value of the last processed RX
	 * descriptor minus 1.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t) ((rx_id == 0) ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		I40E_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

uint16_t
i40e_recv_scattered_pkts(void *rx_queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = rx_queue;
	volatile union i40e_rx_desc *rx_ring = rxq->rx_ring;
	volatile union i40e_rx_desc *rxdp;
	union i40e_rx_desc rxd;
	struct i40e_rx_entry *sw_ring = rxq->sw_ring;
	struct i40e_rx_entry *rxe;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;
	struct rte_mbuf *nmb, *rxm;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0, nb_hold = 0, rx_packet_len;
	struct rte_eth_dev *dev;
	uint32_t rx_status;
	uint64_t qword1;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
					I40E_RXD_QW1_STATUS_SHIFT;

		/* Check the DD bit */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
			break;

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			dev = I40E_VSI_TO_ETH_DEV(rxq->vsi);
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		/**
		 * Use acquire fence to ensure that qword1 which includes DD
		 * bit is loaded before loading of other descriptor words.
		 */
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

		rxd = *rxdp;
		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(sw_ring[rx_id].mbuf);

		/**
		 * When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		/* Set data buffer address and data length of the mbuf */
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;
		rx_packet_len = (qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
					I40E_RXD_QW1_LENGTH_PBUF_SHIFT;
		rxm->data_len = rx_packet_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;

		/**
		 * If this is the first buffer of the received packet, set the
		 * pointer to the first mbuf of the packet and initialize its
		 * context. Otherwise, update the total length and the number
		 * of segments of the current scattered packet, and update the
		 * pointer to the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
			first_seg->pkt_len = rx_packet_len;
		} else {
			first_seg->pkt_len =
				(uint16_t)(first_seg->pkt_len +
						rx_packet_len);
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		/**
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_EOF_SHIFT))) {
			last_seg = rxm;
			continue;
		}

		/**
		 * This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (rx_packet_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len =
					(uint16_t)(last_seg->data_len -
					(RTE_ETHER_CRC_LEN - rx_packet_len));
				last_seg->next = NULL;
			} else
				rxm->data_len = (uint16_t)(rx_packet_len -
							RTE_ETHER_CRC_LEN);
		}

		first_seg->port = rxq->port_id;
		first_seg->ol_flags = 0;
		i40e_rxd_to_vlan_tci(first_seg, &rxd);
		pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
		first_seg->packet_type =
			ptype_tbl[(uint8_t)((qword1 &
			I40E_RXD_QW1_PTYPE_MASK) >> I40E_RXD_QW1_PTYPE_SHIFT)];
		if (pkt_flags & RTE_MBUF_F_RX_RSS_HASH)
			first_seg->hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);
		if (pkt_flags & RTE_MBUF_F_RX_FDIR)
			pkt_flags |= i40e_rxd_build_fdir(&rxd, first_seg);

#ifdef RTE_LIBRTE_IEEE1588
		pkt_flags |= i40e_get_iee15888_flags(first_seg, qword1);
#endif
		first_seg->ol_flags |= pkt_flags;

		/* Prefetch data of first segment, if configured to do so. */
		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr,
			first_seg->data_off));
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
	}

	/* Record index of the next RX descriptor to probe. */
	rxq->rx_tail = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register. Update the RDT with the value of the last processed RX
	 * descriptor minus 1, to guarantee that the RDT register is never
	 * equal to the RDH register, which creates a "full" ring situation
	 * from the hardware point of view.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t)(rx_id == 0 ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		I40E_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

/* Check if the context descriptor is needed for TX offloading */
static inline uint16_t
i40e_calc_context_desc(uint64_t flags)
{
	static uint64_t mask = RTE_MBUF_F_TX_OUTER_IP_CKSUM |
		RTE_MBUF_F_TX_TCP_SEG |
		RTE_MBUF_F_TX_QINQ |
		RTE_MBUF_F_TX_TUNNEL_MASK;

#ifdef RTE_LIBRTE_IEEE1588
	mask |= RTE_MBUF_F_TX_IEEE1588_TMST;
#endif

	return (flags & mask) ? 1 : 0;
}

/* set i40e TSO context descriptor */
static inline uint64_t
i40e_set_tso_ctx(struct rte_mbuf *mbuf, union i40e_tx_offload tx_offload)
{
	uint64_t ctx_desc = 0;
	uint32_t cd_cmd, hdr_len, cd_tso_len;

	if (!tx_offload.l4_len) {
		PMD_DRV_LOG(DEBUG, "L4 length set to 0");
		return ctx_desc;
	}

	hdr_len = tx_offload.l2_len + tx_offload.l3_len + tx_offload.l4_len;
	hdr_len += (mbuf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
		   tx_offload.outer_l2_len + tx_offload.outer_l3_len : 0;

	cd_cmd = I40E_TX_CTX_DESC_TSO;
	cd_tso_len = mbuf->pkt_len - hdr_len;
	ctx_desc |= ((uint64_t)cd_cmd << I40E_TXD_CTX_QW1_CMD_SHIFT) |
		((uint64_t)cd_tso_len <<
		 I40E_TXD_CTX_QW1_TSO_LEN_SHIFT) |
		((uint64_t)mbuf->tso_segsz <<
		 I40E_TXD_CTX_QW1_MSS_SHIFT);

	return ctx_desc;
}

/* HW requires that Tx buffer size ranges from 1B up to (16K-1)B. */
#define I40E_MAX_DATA_PER_TXD \
	(I40E_TXD_QW1_TX_BUF_SZ_MASK >> I40E_TXD_QW1_TX_BUF_SZ_SHIFT)
/* Calculate the number of TX descriptors needed for each pkt */
static inline uint16_t
i40e_calc_pkt_desc(struct rte_mbuf *tx_pkt)
{
	struct rte_mbuf *txd = tx_pkt;
	uint16_t count = 0;

	while (txd != NULL) {
		count += DIV_ROUND_UP(txd->data_len, I40E_MAX_DATA_PER_TXD);
		txd = txd->next;
	}

	return count;
}

uint16_t
i40e_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq;
	struct i40e_tx_entry *sw_ring;
	struct i40e_tx_entry *txe, *txn;
	volatile struct i40e_tx_desc *txd;
	volatile struct i40e_tx_desc *txr;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint32_t cd_tunneling_params;
	uint16_t tx_id;
	uint16_t nb_tx;
	uint32_t td_cmd;
	uint32_t td_offset;
	uint32_t td_tag;
	uint64_t ol_flags;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint16_t tx_last;
	uint16_t slen;
	uint64_t buf_dma_addr;
	union i40e_tx_offload tx_offload = {0};

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr = txq->tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Check if the descriptor ring needs to be cleaned. */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		(void)i40e_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		td_cmd = 0;
		td_tag = 0;
		td_offset = 0;

		tx_pkt = *tx_pkts++;
		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		ol_flags = tx_pkt->ol_flags;
		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.outer_l2_len = tx_pkt->outer_l2_len;
		tx_offload.outer_l3_len = tx_pkt->outer_l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;

		/* Calculate the number of context descriptors needed. */
		nb_ctx = i40e_calc_context_desc(ol_flags);

		/**
		 * The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus 1 context descriptor if needed.
		 * Recalculate the needed tx descs when TSO enabled in case
		 * the mbuf data size exceeds max data size that hw allows
		 * per tx desc.
		 */
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			nb_used = (uint16_t)(i40e_calc_pkt_desc(tx_pkt) +
					     nb_ctx);
		else
			nb_used = (uint16_t)(tx_pkt->nb_segs + nb_ctx);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		if (nb_used > txq->nb_tx_free) {
			if (i40e_xmit_cleanup(txq) != 0) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->tx_rs_thresh)) {
				while (nb_used > txq->nb_tx_free) {
					if (i40e_xmit_cleanup(txq) != 0) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/* Descriptor based VLAN insertion */
		if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
			td_cmd |= I40E_TX_DESC_CMD_IL2TAG1;
			td_tag = tx_pkt->vlan_tci;
		}

		/* Always enable CRC offload insertion */
		td_cmd |= I40E_TX_DESC_CMD_ICRC;

		/* Fill in tunneling parameters if necessary */
		cd_tunneling_params = 0;
		if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
			i40e_parse_tunneling_params(ol_flags, tx_offload,
						    &cd_tunneling_params);
		/* Enable checksum offloading */
		if (ol_flags & I40E_TX_CKSUM_OFFLOAD_MASK)
			i40e_txd_enable_checksum(ol_flags, &td_cmd,
						 &td_offset, tx_offload);

		if (nb_ctx) {
			/* Setup TX context descriptor if required */
			volatile struct i40e_tx_context_desc *ctx_txd =
				(volatile struct i40e_tx_context_desc *)\
							&txr[tx_id];
			uint16_t cd_l2tag2 = 0;
			uint64_t cd_type_cmd_tso_mss =
				I40E_TX_DESC_DTYPE_CONTEXT;

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf != NULL) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			/* TSO enabled means no timestamp */
			if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
				cd_type_cmd_tso_mss |=
					i40e_set_tso_ctx(tx_pkt, tx_offload);
			else {
#ifdef RTE_LIBRTE_IEEE1588
				if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
					cd_type_cmd_tso_mss |=
						((uint64_t)I40E_TX_CTX_DESC_TSYN <<
						 I40E_TXD_CTX_QW1_CMD_SHIFT);
#endif
			}

			ctx_txd->tunneling_params =
				rte_cpu_to_le_32(cd_tunneling_params);
			if (ol_flags & RTE_MBUF_F_TX_QINQ) {
				cd_l2tag2 = tx_pkt->vlan_tci_outer;
				cd_type_cmd_tso_mss |=
					((uint64_t)I40E_TX_CTX_DESC_IL2TAG2 <<
						I40E_TXD_CTX_QW1_CMD_SHIFT);
			}
			ctx_txd->l2tag2 = rte_cpu_to_le_16(cd_l2tag2);
			ctx_txd->type_cmd_tso_mss =
				rte_cpu_to_le_64(cd_type_cmd_tso_mss);

			PMD_TX_LOG(DEBUG, "mbuf: %p, TCD[%u]:\n"
				"tunneling_params: %#x;\n"
				"l2tag2: %#hx;\n"
				"rsvd: %#hx;\n"
				"type_cmd_tso_mss: %#"PRIx64";\n",
				tx_pkt, tx_id,
				ctx_txd->tunneling_params,
				ctx_txd->l2tag2,
				ctx_txd->rsvd,
				ctx_txd->type_cmd_tso_mss);

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);

			while ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) &&
				unlikely(slen > I40E_MAX_DATA_PER_TXD)) {
				txd->buffer_addr =
					rte_cpu_to_le_64(buf_dma_addr);
				txd->cmd_type_offset_bsz =
					i40e_build_ctob(td_cmd,
					td_offset, I40E_MAX_DATA_PER_TXD,
					td_tag);

				buf_dma_addr += I40E_MAX_DATA_PER_TXD;
				slen -= I40E_MAX_DATA_PER_TXD;

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
				txd = &txr[tx_id];
				txn = &sw_ring[txe->next_id];
			}
			PMD_TX_LOG(DEBUG, "mbuf: %p, TDD[%u]:\n"
				"buf_dma_addr: %#"PRIx64";\n"
				"td_cmd: %#x;\n"
				"td_offset: %#x;\n"
				"td_len: %u;\n"
				"td_tag: %#x;\n",
				tx_pkt, tx_id, buf_dma_addr,
				td_cmd, td_offset, slen, td_tag);

			txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->cmd_type_offset_bsz = i40e_build_ctob(td_cmd,
						td_offset, slen, td_tag);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg != NULL);

		/* The last packet data descriptor needs End Of Packet (EOP) */
		td_cmd |= I40E_TX_DESC_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			PMD_TX_LOG(DEBUG,
				   "Setting RS bit on TXD id="
				   "%4u (port=%d queue=%d)",
				   tx_last, txq->port_id, txq->queue_id);

			td_cmd |= I40E_TX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}

		txd->cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)td_cmd) <<
					I40E_TXD_QW1_CMD_SHIFT);
	}

end_of_tx:
	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (unsigned) txq->port_id, (unsigned) txq->queue_id,
		   (unsigned) tx_id, (unsigned) nb_tx);

	rte_io_wmb();
	I40E_PCI_REG_WC_WRITE_RELAXED(txq->qtx_tail, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

static __rte_always_inline int
i40e_tx_free_bufs(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *txep;
	uint16_t tx_rs_thresh = txq->tx_rs_thresh;
	uint16_t i = 0, j = 0;
	struct rte_mbuf *free[RTE_I40E_TX_MAX_FREE_BUF_SZ];
	const uint16_t k = RTE_ALIGN_FLOOR(tx_rs_thresh, RTE_I40E_TX_MAX_FREE_BUF_SZ);
	const uint16_t m = tx_rs_thresh % RTE_I40E_TX_MAX_FREE_BUF_SZ;

	if ((txq->tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(I40E_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	txep = &txq->sw_ring[txq->tx_next_dd - (tx_rs_thresh - 1)];

	for (i = 0; i < tx_rs_thresh; i++)
		rte_prefetch0((txep + i)->mbuf);

	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
		if (k) {
			for (j = 0; j != k; j += RTE_I40E_TX_MAX_FREE_BUF_SZ) {
				for (i = 0; i < RTE_I40E_TX_MAX_FREE_BUF_SZ; ++i, ++txep) {
					free[i] = txep->mbuf;
					txep->mbuf = NULL;
				}
				rte_mempool_put_bulk(free[0]->pool, (void **)free,
						RTE_I40E_TX_MAX_FREE_BUF_SZ);
			}
		}

		if (m) {
			for (i = 0; i < m; ++i, ++txep) {
				free[i] = txep->mbuf;
				txep->mbuf = NULL;
			}
			rte_mempool_put_bulk(free[0]->pool, (void **)free, m);
		}
	} else {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_pktmbuf_free_seg(txep->mbuf);
			txep->mbuf = NULL;
		}
	}

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile struct i40e_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;
	uint32_t i;

	for (i = 0; i < 4; i++, txdp++, pkts++) {
		dma_addr = rte_mbuf_data_iova(*pkts);
		txdp->buffer_addr = rte_cpu_to_le_64(dma_addr);
		txdp->cmd_type_offset_bsz =
			i40e_build_ctob((uint32_t)I40E_TD_CMD, 0,
					(*pkts)->data_len, 0);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct i40e_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;

	dma_addr = rte_mbuf_data_iova(*pkts);
	txdp->buffer_addr = rte_cpu_to_le_64(dma_addr);
	txdp->cmd_type_offset_bsz =
		i40e_build_ctob((uint32_t)I40E_TD_CMD, 0,
				(*pkts)->data_len, 0);
}

/* Fill hardware descriptor ring with mbuf data */
static inline void
i40e_tx_fill_hw_ring(struct i40e_tx_queue *txq,
		     struct rte_mbuf **pkts,
		     uint16_t nb_pkts)
{
	volatile struct i40e_tx_desc *txdp = &(txq->tx_ring[txq->tx_tail]);
	struct i40e_tx_entry *txep = &(txq->sw_ring[txq->tx_tail]);
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	mainpart = (nb_pkts & ((uint32_t) ~N_PER_LOOP_MASK));
	leftover = (nb_pkts & ((uint32_t)  N_PER_LOOP_MASK));
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		for (j = 0; j < N_PER_LOOP; ++j) {
			(txep + i + j)->mbuf = *(pkts + i + j);
		}
		tx4(txdp + i, pkts + i);
	}
	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(txep + mainpart + i)->mbuf = *(pkts + mainpart + i);
			tx1(txdp + mainpart + i, pkts + mainpart + i);
		}
	}
}

static inline uint16_t
tx_xmit_pkts(struct i40e_tx_queue *txq,
	     struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts)
{
	volatile struct i40e_tx_desc *txr = txq->tx_ring;
	uint16_t n = 0;

	/**
	 * Begin scanning the H/W ring for done descriptors when the number
	 * of available descriptors drops below tx_free_thresh. For each done
	 * descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs(txq);

	/* Use available descriptor only */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(!nb_pkts))
		return 0;

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		i40e_tx_fill_hw_ring(txq, tx_pkts, n);
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
		txq->tx_tail = 0;
	}

	/* Fill hardware descriptor ring with mbuf data */
	i40e_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/* Determine if RS bit needs to be set */
	if (txq->tx_tail > txq->tx_next_rs) {
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
		if (txq->tx_next_rs >= txq->nb_tx_desc)
			txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
	}

	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	/* Update the tx tail register */
	I40E_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static uint16_t
i40e_xmit_pkts_simple(void *tx_queue,
		      struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;

	if (likely(nb_pkts <= I40E_TX_MAX_BURST))
		return tx_xmit_pkts((struct i40e_tx_queue *)tx_queue,
						tx_pkts, nb_pkts);

	while (nb_pkts) {
		uint16_t ret, num = (uint16_t)RTE_MIN(nb_pkts,
						I40E_TX_MAX_BURST);

		ret = tx_xmit_pkts((struct i40e_tx_queue *)tx_queue,
						&tx_pkts[nb_tx], num);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < num)
			break;
	}

	return nb_tx;
}

static uint16_t
i40e_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		   uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = i40e_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx],
						num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

/*********************************************************************
 *
 *  TX simple prep functions
 *
 **********************************************************************/
uint16_t
i40e_simple_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	int i;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		if (m->nb_segs != 1) {
			rte_errno = EINVAL;
			return i;
		}

		if (ol_flags & I40E_TX_OFFLOAD_SIMPLE_NOTSUP_MASK) {
			rte_errno = ENOTSUP;
			return i;
		}

		/* check the size of packet */
		if (m->pkt_len < I40E_TX_MIN_PKT_LEN ||
		    m->pkt_len > I40E_FRAME_SIZE_MAX) {
			rte_errno = EINVAL;
			return i;
		}
	}
	return i;
}

/*********************************************************************
 *
 *  TX prep functions
 *
 **********************************************************************/
uint16_t
i40e_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/* Check for m->nb_segs to not exceed the limits. */
		if (!(ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			if (m->nb_segs > I40E_TX_MAX_MTU_SEG ||
			    m->pkt_len > I40E_FRAME_SIZE_MAX) {
				rte_errno = EINVAL;
				return i;
			}
		} else if (m->nb_segs > I40E_TX_MAX_SEG ||
			   m->tso_segsz < I40E_MIN_TSO_MSS ||
			   m->tso_segsz > I40E_MAX_TSO_MSS ||
			   m->pkt_len > I40E_TSO_FRAME_SIZE_MAX) {
			/* MSS outside the range (256B - 9674B) are considered
			 * malicious
			 */
			rte_errno = EINVAL;
			return i;
		}

		if (ol_flags & I40E_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = ENOTSUP;
			return i;
		}

		/* check the size of packet */
		if (m->pkt_len < I40E_TX_MIN_PKT_LEN) {
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_ETHDEV_DEBUG_TX
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
	}
	return i;
}

/*
 * Find the VSI the queue belongs to. 'queue_idx' is the queue index
 * application used, which assume having sequential ones. But from driver's
 * perspective, it's different. For example, q0 belongs to FDIR VSI, q1-q64
 * to MAIN VSI, , q65-96 to SRIOV VSIs, q97-128 to VMDQ VSIs. For application
 * running on host, q1-64 and q97-128 can be used, total 96 queues. They can
 * use queue_idx from 0 to 95 to access queues, while real queue would be
 * different. This function will do a queue mapping to find VSI the queue
 * belongs to.
 */
static struct i40e_vsi*
i40e_pf_get_vsi_by_qindex(struct i40e_pf *pf, uint16_t queue_idx)
{
	/* the queue in MAIN VSI range */
	if (queue_idx < pf->main_vsi->nb_qps)
		return pf->main_vsi;

	queue_idx -= pf->main_vsi->nb_qps;

	/* queue_idx is greater than VMDQ VSIs range */
	if (queue_idx > pf->nb_cfg_vmdq_vsi * pf->vmdq_nb_qps - 1) {
		PMD_INIT_LOG(ERR, "queue_idx out of range. VMDQ configured?");
		return NULL;
	}

	return pf->vmdq[queue_idx / pf->vmdq_nb_qps].vsi;
}

static uint16_t
i40e_get_queue_offset_by_qindex(struct i40e_pf *pf, uint16_t queue_idx)
{
	/* the queue in MAIN VSI range */
	if (queue_idx < pf->main_vsi->nb_qps)
		return queue_idx;

	/* It's VMDQ queues */
	queue_idx -= pf->main_vsi->nb_qps;

	if (pf->nb_cfg_vmdq_vsi)
		return queue_idx % pf->vmdq_nb_qps;
	else {
		PMD_INIT_LOG(ERR, "Fail to get queue offset");
		return (uint16_t)(-1);
	}
}

int
i40e_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct i40e_rx_queue *rxq;
	int err;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];
	if (!rxq || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
			    rx_queue_id);
		return -EINVAL;
	}

	if (rxq->rx_deferred_start)
		PMD_DRV_LOG(WARNING, "RX queue %u is deferred start",
			    rx_queue_id);

	err = i40e_alloc_rx_queue_mbufs(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
		return err;
	}

	/* Init the RX tail register. */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	err = i40e_switch_rx_queue(hw, rxq->reg_idx, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);

		i40e_rx_queue_release_mbufs(rxq);
		i40e_reset_rx_queue(rxq);
		return err;
	}
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
i40e_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct i40e_rx_queue *rxq;
	int err;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	rxq = dev->data->rx_queues[rx_queue_id];
	if (!rxq || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
				rx_queue_id);
		return -EINVAL;
	}

	/*
	 * rx_queue_id is queue id application refers to, while
	 * rxq->reg_idx is the real queue index.
	 */
	err = i40e_switch_rx_queue(hw, rxq->reg_idx, FALSE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}
	i40e_rx_queue_release_mbufs(rxq);
	i40e_reset_rx_queue(rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
i40e_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	int err;
	struct i40e_tx_queue *txq;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq || !txq->q_set) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available or setup",
			    tx_queue_id);
		return -EINVAL;
	}

	if (txq->tx_deferred_start)
		PMD_DRV_LOG(WARNING, "TX queue %u is deferred start",
			    tx_queue_id);

	/*
	 * tx_queue_id is queue id application refers to, while
	 * rxq->reg_idx is the real queue index.
	 */
	err = i40e_switch_tx_queue(hw, txq->reg_idx, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
		return err;
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
i40e_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct i40e_tx_queue *txq;
	int err;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq || !txq->q_set) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available or setup",
			tx_queue_id);
		return -EINVAL;
	}

	/*
	 * tx_queue_id is queue id application refers to, while
	 * txq->reg_idx is the real queue index.
	 */
	err = i40e_switch_tx_queue(hw, txq->reg_idx, FALSE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u of",
			    tx_queue_id);
		return err;
	}

	i40e_tx_queue_release_mbufs(txq);
	i40e_reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

const uint32_t *
i40e_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to i40e_rxd_pkt_type_mapping() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == i40e_recv_pkts ||
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	    dev->rx_pkt_burst == i40e_recv_pkts_bulk_alloc ||
#endif
	    dev->rx_pkt_burst == i40e_recv_scattered_pkts ||
	    dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec ||
	    dev->rx_pkt_burst == i40e_recv_pkts_vec ||
#ifdef CC_AVX512_SUPPORT
	    dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec_avx512 ||
	    dev->rx_pkt_burst == i40e_recv_pkts_vec_avx512 ||
#endif
	    dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec_avx2 ||
	    dev->rx_pkt_burst == i40e_recv_pkts_vec_avx2)
		return ptypes;
	return NULL;
}

static int
i40e_dev_first_queue(uint16_t idx, void **queues, int num)
{
	uint16_t i;

	for (i = 0; i < num; i++) {
		if (i != idx && queues[i])
			return 0;
	}

	return 1;
}

static int
i40e_dev_rx_queue_setup_runtime(struct rte_eth_dev *dev,
				struct i40e_rx_queue *rxq)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int use_def_burst_func =
		check_rx_burst_bulk_alloc_preconditions(rxq);
	uint16_t buf_size =
		(uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
			   RTE_PKTMBUF_HEADROOM);
	int use_scattered_rx =
		(rxq->max_pkt_len > buf_size);

	if (i40e_rx_queue_init(rxq) != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			    "Failed to do RX queue initialization");
		return -EINVAL;
	}

	if (i40e_dev_first_queue(rxq->queue_id,
				 dev->data->rx_queues,
				 dev->data->nb_rx_queues)) {
		/**
		 * If it is the first queue to setup,
		 * set all flags to default and call
		 * i40e_set_rx_function.
		 */
		ad->rx_bulk_alloc_allowed = true;
		ad->rx_vec_allowed = true;
		dev->data->scattered_rx = use_scattered_rx;
		if (use_def_burst_func)
			ad->rx_bulk_alloc_allowed = false;
		i40e_set_rx_function(dev);
		return 0;
	} else if (ad->rx_vec_allowed && !rte_is_power_of_2(rxq->nb_rx_desc)) {
		PMD_DRV_LOG(ERR, "Vector mode is allowed, but descriptor"
			    " number %d of queue %d isn't power of 2",
			    rxq->nb_rx_desc, rxq->queue_id);
		return -EINVAL;
	}

	/* check bulk alloc conflict */
	if (ad->rx_bulk_alloc_allowed && use_def_burst_func) {
		PMD_DRV_LOG(ERR, "Can't use default burst.");
		return -EINVAL;
	}
	/* check scattered conflict */
	if (!dev->data->scattered_rx && use_scattered_rx) {
		PMD_DRV_LOG(ERR, "Scattered rx is required.");
		return -EINVAL;
	}
	/* check vector conflict */
	if (ad->rx_vec_allowed && i40e_rxq_vec_setup(rxq)) {
		PMD_DRV_LOG(ERR, "Failed vector rx setup.");
		return -EINVAL;
	}

	return 0;
}

int
i40e_dev_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct i40e_vsi *vsi;
	struct i40e_pf *pf = NULL;
	struct i40e_rx_queue *rxq;
	const struct rte_memzone *rz;
	uint32_t ring_size;
	uint16_t len, i;
	uint16_t reg_idx, base, bsf, tc_mapping;
	int q_offset, use_def_burst_func = 1;
	uint64_t offloads;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	vsi = i40e_pf_get_vsi_by_qindex(pf, queue_idx);
	if (!vsi)
		return -EINVAL;
	q_offset = i40e_get_queue_offset_by_qindex(pf, queue_idx);
	if (q_offset < 0)
		return -EINVAL;
	reg_idx = vsi->base_queue + q_offset;

	if (nb_desc % I40E_ALIGN_RING_DESC != 0 ||
	    (nb_desc > I40E_MAX_RING_DESC) ||
	    (nb_desc < I40E_MIN_RING_DESC)) {
		PMD_DRV_LOG(ERR, "Number (%u) of receive descriptors is "
			    "invalid", nb_desc);
		return -EINVAL;
	}

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		i40e_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("i40e rx queue",
				 sizeof(struct i40e_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
			    "rx queue data structure");
		return -ENOMEM;
	}
	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = reg_idx;
	rxq->port_id = dev->data->port_id;
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->vsi = vsi;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->offloads = offloads;

	/* Allocate the maximum number of RX ring hardware descriptor. */
	len = I40E_MAX_RING_DESC;

	/**
	 * Allocating a little more memory because vectorized/bulk_alloc Rx
	 * functions doesn't check boundaries each time.
	 */
	len += RTE_PMD_I40E_RX_MAX_BURST;

	ring_size = RTE_ALIGN(len * sizeof(union i40e_rx_desc),
			      I40E_DMA_MEM_ALIGN);

	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
			      ring_size, I40E_RING_BASE_ALIGN, socket_id);
	if (!rz) {
		i40e_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX");
		return -ENOMEM;
	}

	rxq->mz = rz;
	/* Zero all the descriptors in the ring. */
	memset(rz->addr, 0, ring_size);

	rxq->rx_ring_phys_addr = rz->iova;
	rxq->rx_ring = (union i40e_rx_desc *)rz->addr;

	len = (uint16_t)(nb_desc + RTE_PMD_I40E_RX_MAX_BURST);

	/* Allocate the software ring. */
	rxq->sw_ring =
		rte_zmalloc_socket("i40e rx sw ring",
				   sizeof(struct i40e_rx_entry) * len,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!rxq->sw_ring) {
		i40e_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW ring");
		return -ENOMEM;
	}

	i40e_reset_rx_queue(rxq);
	rxq->q_set = TRUE;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (!(vsi->enabled_tc & (1 << i)))
			continue;
		tc_mapping = rte_le_to_cpu_16(vsi->info.tc_mapping[i]);
		base = (tc_mapping & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) >>
			I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT;
		bsf = (tc_mapping & I40E_AQ_VSI_TC_QUE_NUMBER_MASK) >>
			I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT;

		if (queue_idx >= base && queue_idx < (base + BIT(bsf)))
			rxq->dcb_tc = i;
	}

	if (dev->data->dev_started) {
		if (i40e_dev_rx_queue_setup_runtime(dev, rxq)) {
			i40e_rx_queue_release(rxq);
			return -EINVAL;
		}
	} else {
		use_def_burst_func =
			check_rx_burst_bulk_alloc_preconditions(rxq);
		if (!use_def_burst_func) {
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
			PMD_INIT_LOG(DEBUG,
			  "Rx Burst Bulk Alloc Preconditions are "
			  "satisfied. Rx Burst Bulk Alloc function will be "
			  "used on port=%d, queue=%d.",
			  rxq->port_id, rxq->queue_id);
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
		} else {
			PMD_INIT_LOG(DEBUG,
			  "Rx Burst Bulk Alloc Preconditions are "
			  "not satisfied, Scattered Rx is requested, "
			  "or RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC is "
			  "not enabled on port=%d, queue=%d.",
			  rxq->port_id, rxq->queue_id);
			ad->rx_bulk_alloc_allowed = false;
		}
	}

	dev->data->rx_queues[queue_idx] = rxq;
	return 0;
}

void
i40e_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	i40e_rx_queue_release(dev->data->rx_queues[qid]);
}

void
i40e_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	i40e_tx_queue_release(dev->data->tx_queues[qid]);
}

void
i40e_rx_queue_release(void *rxq)
{
	struct i40e_rx_queue *q = (struct i40e_rx_queue *)rxq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

	i40e_rx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

uint32_t
i40e_dev_rx_queue_count(void *rx_queue)
{
#define I40E_RXQ_SCAN_INTERVAL 4
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_queue *rxq;
	uint16_t desc = 0;

	rxq = rx_queue;
	rxdp = &(rxq->rx_ring[rxq->rx_tail]);
	while ((desc < rxq->nb_rx_desc) &&
		((rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		I40E_RXD_QW1_STATUS_MASK) >> I40E_RXD_QW1_STATUS_SHIFT) &
				(1 << I40E_RX_DESC_STATUS_DD_SHIFT)) {
		/**
		 * Check the DD bit of a rx descriptor of each 4 in a group,
		 * to avoid checking too frequently and downgrading performance
		 * too much.
		 */
		desc += I40E_RXQ_SCAN_INTERVAL;
		rxdp += I40E_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
					desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
i40e_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct i40e_rx_queue *rxq = rx_queue;
	volatile uint64_t *status;
	uint64_t mask;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	if (offset >= rxq->nb_rx_desc - rxq->nb_rx_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	status = &rxq->rx_ring[desc].wb.qword1.status_error_len;
	mask = rte_le_to_cpu_64((1ULL << I40E_RX_DESC_STATUS_DD_SHIFT)
		<< I40E_RXD_QW1_STATUS_SHIFT);
	if (*status & mask)
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
i40e_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct i40e_tx_queue *txq = tx_queue;
	volatile uint64_t *status;
	uint64_t mask, expect;
	uint32_t desc;

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	/* go to next desc that has the RS bit */
	desc = ((desc + txq->tx_rs_thresh - 1) / txq->tx_rs_thresh) *
		txq->tx_rs_thresh;
	if (desc >= txq->nb_tx_desc) {
		desc -= txq->nb_tx_desc;
		if (desc >= txq->nb_tx_desc)
			desc -= txq->nb_tx_desc;
	}

	status = &txq->tx_ring[desc].cmd_type_offset_bsz;
	mask = rte_le_to_cpu_64(I40E_TXD_QW1_DTYPE_MASK);
	expect = rte_cpu_to_le_64(
		I40E_TX_DESC_DTYPE_DESC_DONE << I40E_TXD_QW1_DTYPE_SHIFT);
	if ((*status & mask) == expect)
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

static int
i40e_dev_tx_queue_setup_runtime(struct rte_eth_dev *dev,
				struct i40e_tx_queue *txq)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (i40e_tx_queue_init(txq) != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			    "Failed to do TX queue initialization");
		return -EINVAL;
	}

	if (i40e_dev_first_queue(txq->queue_id,
				 dev->data->tx_queues,
				 dev->data->nb_tx_queues)) {
		/**
		 * If it is the first queue to setup,
		 * set all flags and call
		 * i40e_set_tx_function.
		 */
		i40e_set_tx_function_flag(dev, txq);
		i40e_set_tx_function(dev);
		return 0;
	}

	/* check vector conflict */
	if (ad->tx_vec_allowed) {
		if (txq->tx_rs_thresh > RTE_I40E_TX_MAX_FREE_BUF_SZ ||
		    i40e_txq_vec_setup(txq)) {
			PMD_DRV_LOG(ERR, "Failed vector tx setup.");
			return -EINVAL;
		}
	}
	/* check simple tx conflict */
	if (ad->tx_simple_allowed) {
		if ((txq->offloads & ~RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) != 0 ||
				txq->tx_rs_thresh < RTE_PMD_I40E_TX_MAX_BURST) {
			PMD_DRV_LOG(ERR, "No-simple tx is required.");
			return -EINVAL;
		}
	}

	return 0;
}

int
i40e_dev_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf)
{
	struct i40e_vsi *vsi;
	struct i40e_pf *pf = NULL;
	struct i40e_tx_queue *txq;
	const struct rte_memzone *tz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;
	uint16_t reg_idx, i, base, bsf, tc_mapping;
	int q_offset;
	uint64_t offloads;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	vsi = i40e_pf_get_vsi_by_qindex(pf, queue_idx);
	if (!vsi)
		return -EINVAL;
	q_offset = i40e_get_queue_offset_by_qindex(pf, queue_idx);
	if (q_offset < 0)
		return -EINVAL;
	reg_idx = vsi->base_queue + q_offset;

	if (nb_desc % I40E_ALIGN_RING_DESC != 0 ||
	    (nb_desc > I40E_MAX_RING_DESC) ||
	    (nb_desc < I40E_MIN_RING_DESC)) {
		PMD_DRV_LOG(ERR, "Number (%u) of transmit descriptors is "
			    "invalid", nb_desc);
		return -EINVAL;
	}

	/**
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors. TX descriptors will have their RS bit set
	 * after txq->tx_rs_thresh descriptors have been used. The TX
	 * descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required to
	 * transmit a packet is greater than the number of free TX descriptors.
	 *
	 * The following constraints must be satisfied:
	 *  - tx_rs_thresh must be greater than 0.
	 *  - tx_rs_thresh must be less than the size of the ring minus 2.
	 *  - tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  - tx_rs_thresh must be a divisor of the ring size.
	 *  - tx_free_thresh must be greater than 0.
	 *  - tx_free_thresh must be less than the size of the ring minus 3.
	 *  - tx_free_thresh + tx_rs_thresh must not exceed nb_desc.
	 *
	 * One descriptor in the TX ring is used as a sentinel to avoid a H/W
	 * race condition, hence the maximum threshold constraints. When set
	 * to zero use default values.
	 */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
		tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	/* force tx_rs_thresh to adapt an aggressive tx_free_thresh */
	tx_rs_thresh = (DEFAULT_TX_RS_THRESH + tx_free_thresh > nb_desc) ?
		nb_desc - tx_free_thresh : DEFAULT_TX_RS_THRESH;
	if (tx_conf->tx_rs_thresh > 0)
		tx_rs_thresh = tx_conf->tx_rs_thresh;
	if (tx_rs_thresh + tx_free_thresh > nb_desc) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh + tx_free_thresh must not "
				"exceed nb_desc. (tx_rs_thresh=%u "
				"tx_free_thresh=%u nb_desc=%u port=%d queue=%d)",
				(unsigned int)tx_rs_thresh,
				(unsigned int)tx_free_thresh,
				(unsigned int)nb_desc,
				(int)dev->data->port_id,
				(int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if (tx_rs_thresh >= (nb_desc - 2)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than the "
			     "number of TX descriptors minus 2. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_free_thresh must be less than the "
			     "number of TX descriptors minus 3. "
			     "(tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than or "
			     "equal to tx_free_thresh. (tx_free_thresh=%u"
			     " tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be a divisor of the "
			     "number of TX descriptors. (tx_rs_thresh=%u"
			     " port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if ((tx_rs_thresh > 1) && (tx_conf->tx_thresh.wthresh != 0)) {
		PMD_INIT_LOG(ERR, "TX WTHRESH must be set to 0 if "
			     "tx_rs_thresh is greater than 1. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return I40E_ERR_PARAM;
	}

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		i40e_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("i40e tx queue",
				  sizeof(struct i40e_tx_queue),
				  RTE_CACHE_LINE_SIZE,
				  socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
			    "tx queue structure");
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct i40e_tx_desc) * I40E_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, I40E_DMA_MEM_ALIGN);
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
			      ring_size, I40E_RING_BASE_ALIGN, socket_id);
	if (!tz) {
		i40e_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX");
		return -ENOMEM;
	}

	txq->mz = tz;
	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->reg_idx = reg_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->vsi = vsi;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	txq->tx_ring_phys_addr = tz->iova;
	txq->tx_ring = (struct i40e_tx_desc *)tz->addr;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("i40e tx sw ring",
				   sizeof(struct i40e_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		i40e_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW TX ring");
		return -ENOMEM;
	}

	i40e_reset_tx_queue(txq);
	txq->q_set = TRUE;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (!(vsi->enabled_tc & (1 << i)))
			continue;
		tc_mapping = rte_le_to_cpu_16(vsi->info.tc_mapping[i]);
		base = (tc_mapping & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) >>
			I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT;
		bsf = (tc_mapping & I40E_AQ_VSI_TC_QUE_NUMBER_MASK) >>
			I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT;

		if (queue_idx >= base && queue_idx < (base + BIT(bsf)))
			txq->dcb_tc = i;
	}

	if (dev->data->dev_started) {
		if (i40e_dev_tx_queue_setup_runtime(dev, txq)) {
			i40e_tx_queue_release(txq);
			return -EINVAL;
		}
	} else {
		/**
		 * Use a simple TX queue without offloads or
		 * multi segs if possible
		 */
		i40e_set_tx_function_flag(dev, txq);
	}
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

void
i40e_tx_queue_release(void *txq)
{
	struct i40e_tx_queue *q = (struct i40e_tx_queue *)txq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to TX queue is NULL");
		return;
	}

	i40e_tx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

const struct rte_memzone *
i40e_memzone_reserve(const char *name, uint32_t len, int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz)
		return mz;

	mz = rte_memzone_reserve_aligned(name, len, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, I40E_RING_BASE_ALIGN);
	return mz;
}

void
i40e_rx_queue_release_mbufs(struct i40e_rx_queue *rxq)
{
	uint16_t i;

	/* SSE Vector driver has a different way of releasing mbufs. */
	if (rxq->rx_using_sse) {
		i40e_rx_queue_release_mbufs_vec(rxq);
		return;
	}

	if (!rxq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to sw_ring is NULL");
		return;
	}

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
			rxq->sw_ring[i].mbuf = NULL;
		}
	}
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (rxq->rx_nb_avail == 0)
		return;
	for (i = 0; i < rxq->rx_nb_avail; i++) {
		struct rte_mbuf *mbuf;

		mbuf = rxq->rx_stage[rxq->rx_next_avail + i];
		rte_pktmbuf_free_seg(mbuf);
	}
	rxq->rx_nb_avail = 0;
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
}

void
i40e_reset_rx_queue(struct i40e_rx_queue *rxq)
{
	unsigned i;
	uint16_t len;

	if (!rxq) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (check_rx_burst_bulk_alloc_preconditions(rxq) == 0)
		len = (uint16_t)(rxq->nb_rx_desc + RTE_PMD_I40E_RX_MAX_BURST);
	else
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
		len = rxq->nb_rx_desc;

	for (i = 0; i < len * sizeof(union i40e_rx_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < RTE_PMD_I40E_RX_MAX_BURST; ++i)
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;

	rte_pktmbuf_free(rxq->pkt_first_seg);

	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;

	rxq->rxrearm_start = 0;
	rxq->rxrearm_nb = 0;
}

void
i40e_tx_queue_release_mbufs(struct i40e_tx_queue *txq)
{
	struct rte_eth_dev *dev;
	uint16_t i;

	if (!txq || !txq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq or sw_ring is NULL");
		return;
	}

	dev = &rte_eth_devices[txq->port_id];

	/**
	 *  vPMD tx will not set sw_ring's mbuf to NULL after free,
	 *  so need to free remains more carefully.
	 */
#ifdef CC_AVX512_SUPPORT
	if (dev->tx_pkt_burst == i40e_xmit_pkts_vec_avx512) {
		struct i40e_vec_tx_entry *swr = (void *)txq->sw_ring;

		i = txq->tx_next_dd - txq->tx_rs_thresh + 1;
		if (txq->tx_tail < i) {
			for (; i < txq->nb_tx_desc; i++) {
				rte_pktmbuf_free_seg(swr[i].mbuf);
				swr[i].mbuf = NULL;
			}
			i = 0;
		}
		for (; i < txq->tx_tail; i++) {
			rte_pktmbuf_free_seg(swr[i].mbuf);
			swr[i].mbuf = NULL;
		}
		return;
	}
#endif
	if (dev->tx_pkt_burst == i40e_xmit_pkts_vec_avx2 ||
			dev->tx_pkt_burst == i40e_xmit_pkts_vec) {
		i = txq->tx_next_dd - txq->tx_rs_thresh + 1;
		if (txq->tx_tail < i) {
			for (; i < txq->nb_tx_desc; i++) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
			i = 0;
		}
		for (; i < txq->tx_tail; i++) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	} else {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static int
i40e_tx_done_cleanup_full(struct i40e_tx_queue *txq,
			uint32_t free_cnt)
{
	struct i40e_tx_entry *swr_ring = txq->sw_ring;
	uint16_t i, tx_last, tx_id;
	uint16_t nb_tx_free_last;
	uint16_t nb_tx_to_clean;
	uint32_t pkt_cnt;

	/* Start free mbuf from the next of tx_tail */
	tx_last = txq->tx_tail;
	tx_id  = swr_ring[tx_last].next_id;

	if (txq->nb_tx_free == 0 && i40e_xmit_cleanup(txq))
		return 0;

	nb_tx_to_clean = txq->nb_tx_free;
	nb_tx_free_last = txq->nb_tx_free;
	if (!free_cnt)
		free_cnt = txq->nb_tx_desc;

	/* Loop through swr_ring to count the amount of
	 * freeable mubfs and packets.
	 */
	for (pkt_cnt = 0; pkt_cnt < free_cnt; ) {
		for (i = 0; i < nb_tx_to_clean &&
			pkt_cnt < free_cnt &&
			tx_id != tx_last; i++) {
			if (swr_ring[tx_id].mbuf != NULL) {
				rte_pktmbuf_free_seg(swr_ring[tx_id].mbuf);
				swr_ring[tx_id].mbuf = NULL;

				/*
				 * last segment in the packet,
				 * increment packet count
				 */
				pkt_cnt += (swr_ring[tx_id].last_id == tx_id);
			}

			tx_id = swr_ring[tx_id].next_id;
		}

		if (txq->tx_rs_thresh > txq->nb_tx_desc -
			txq->nb_tx_free || tx_id == tx_last)
			break;

		if (pkt_cnt < free_cnt) {
			if (i40e_xmit_cleanup(txq))
				break;

			nb_tx_to_clean = txq->nb_tx_free - nb_tx_free_last;
			nb_tx_free_last = txq->nb_tx_free;
		}
	}

	return (int)pkt_cnt;
}

static int
i40e_tx_done_cleanup_simple(struct i40e_tx_queue *txq,
			uint32_t free_cnt)
{
	int i, n, cnt;

	if (free_cnt == 0 || free_cnt > txq->nb_tx_desc)
		free_cnt = txq->nb_tx_desc;

	cnt = free_cnt - free_cnt % txq->tx_rs_thresh;

	for (i = 0; i < cnt; i += n) {
		if (txq->nb_tx_desc - txq->nb_tx_free < txq->tx_rs_thresh)
			break;

		n = i40e_tx_free_bufs(txq);

		if (n == 0)
			break;
	}

	return i;
}

static int
i40e_tx_done_cleanup_vec(struct i40e_tx_queue *txq __rte_unused,
			uint32_t free_cnt __rte_unused)
{
	return -ENOTSUP;
}
int
i40e_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	struct i40e_tx_queue *q = (struct i40e_tx_queue *)txq;
	struct rte_eth_dev *dev = &rte_eth_devices[q->port_id];
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (ad->tx_simple_allowed) {
		if (ad->tx_vec_allowed)
			return i40e_tx_done_cleanup_vec(q, free_cnt);
		else
			return i40e_tx_done_cleanup_simple(q, free_cnt);
	} else {
		return i40e_tx_done_cleanup_full(q, free_cnt);
	}
}

void
i40e_reset_tx_queue(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *txe;
	uint16_t i, prev, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct i40e_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile struct i40e_tx_desc *txd = &txq->tx_ring[i];

		txd->cmd_type_offset_bsz =
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;

	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
}

/* Init the TX queue in hardware */
int
i40e_tx_queue_init(struct i40e_tx_queue *txq)
{
	enum i40e_status_code err = I40E_SUCCESS;
	struct i40e_vsi *vsi = txq->vsi;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t pf_q = txq->reg_idx;
	struct i40e_hmc_obj_txq tx_ctx;
	uint32_t qtx_ctl;

	/* clear the context structure first */
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	tx_ctx.new_context = 1;
	tx_ctx.base = txq->tx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->nb_tx_desc;

#ifdef RTE_LIBRTE_IEEE1588
	tx_ctx.timesync_ena = 1;
#endif
	tx_ctx.rdylist = rte_le_to_cpu_16(vsi->info.qs_handle[txq->dcb_tc]);
	if (vsi->type == I40E_VSI_FDIR)
		tx_ctx.fd_ena = TRUE;

	err = i40e_clear_lan_tx_queue_context(hw, pf_q);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failure of clean lan tx queue context");
		return err;
	}

	err = i40e_set_lan_tx_queue_context(hw, pf_q, &tx_ctx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failure of set lan tx queue context");
		return err;
	}

	/* Now associate this queue with this PCI function */
	qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
	qtx_ctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
					I40E_QTX_CTL_PF_INDX_MASK);
	I40E_WRITE_REG(hw, I40E_QTX_CTL(pf_q), qtx_ctl);
	I40E_WRITE_FLUSH(hw);

	txq->qtx_tail = hw->hw_addr + I40E_QTX_TAIL(pf_q);

	return err;
}

int
i40e_alloc_rx_queue_mbufs(struct i40e_rx_queue *rxq)
{
	struct i40e_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union i40e_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mp);

		if (unlikely(!mbuf)) {
			PMD_DRV_LOG(ERR, "Failed to allocate mbuf for RX");
			return -ENOMEM;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->next = NULL;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rxd = &rxq->rx_ring[i];
		rxd->read.pkt_addr = dma_addr;
		rxd->read.hdr_addr = 0;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif /* RTE_LIBRTE_I40E_16BYTE_RX_DESC */

		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/*
 * Calculate the buffer length, and check the jumbo frame
 * and maximum packet length.
 */
static int
i40e_rx_queue_config(struct i40e_rx_queue *rxq)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(rxq->vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->vsi);
	struct rte_eth_dev_data *data = pf->dev_data;
	uint16_t buf_size;

	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
		RTE_PKTMBUF_HEADROOM);

	switch (pf->flags & (I40E_FLAG_HEADER_SPLIT_DISABLED |
			I40E_FLAG_HEADER_SPLIT_ENABLED)) {
	case I40E_FLAG_HEADER_SPLIT_ENABLED: /* Not supported */
		rxq->rx_hdr_len = RTE_ALIGN(I40E_RXBUF_SZ_1024,
				(1 << I40E_RXQ_CTX_HBUFF_SHIFT));
		rxq->rx_buf_len = RTE_ALIGN(I40E_RXBUF_SZ_2048,
				(1 << I40E_RXQ_CTX_DBUFF_SHIFT));
		rxq->hs_mode = i40e_header_split_enabled;
		break;
	case I40E_FLAG_HEADER_SPLIT_DISABLED:
	default:
		rxq->rx_hdr_len = 0;
		rxq->rx_buf_len = RTE_ALIGN_FLOOR(buf_size,
			(1 << I40E_RXQ_CTX_DBUFF_SHIFT));
		rxq->hs_mode = i40e_header_split_none;
		break;
	}

	rxq->max_pkt_len =
		RTE_MIN(hw->func_caps.rx_buf_chain_len * rxq->rx_buf_len,
				data->mtu + I40E_ETH_OVERHEAD);
	if (rxq->max_pkt_len < RTE_ETHER_MIN_LEN ||
		rxq->max_pkt_len > I40E_FRAME_SIZE_MAX) {
		PMD_DRV_LOG(ERR, "maximum packet length must be "
			    "larger than %u and smaller than %u",
			    (uint32_t)RTE_ETHER_MIN_LEN,
			    (uint32_t)I40E_FRAME_SIZE_MAX);
		return I40E_ERR_CONFIG;
	}

	return 0;
}

/* Init the RX queue in hardware */
int
i40e_rx_queue_init(struct i40e_rx_queue *rxq)
{
	int err = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->vsi);
	struct rte_eth_dev_data *dev_data = I40E_VSI_TO_DEV_DATA(rxq->vsi);
	uint16_t pf_q = rxq->reg_idx;
	uint16_t buf_size;
	struct i40e_hmc_obj_rxq rx_ctx;

	err = i40e_rx_queue_config(rxq);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Failed to config RX queue");
		return err;
	}

	/* Clear the context structure first */
	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));
	rx_ctx.dbuff = rxq->rx_buf_len >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.hbuff = rxq->rx_hdr_len >> I40E_RXQ_CTX_HBUFF_SHIFT;

	rx_ctx.base = rxq->rx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	rx_ctx.dsize = 1;
#endif
	rx_ctx.dtype = rxq->hs_mode;
	if (rxq->hs_mode)
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_ALL;
	else
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_NONE;
	rx_ctx.rxmax = rxq->max_pkt_len;
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = (rxq->crc_len == 0) ? 1 : 0;
	rx_ctx.l2tsel = 1;
	/* showiv indicates if inner VLAN is stripped inside of tunnel
	 * packet. When set it to 1, vlan information is stripped from
	 * the inner header, but the hardware does not put it in the
	 * descriptor. So set it zero by default.
	 */
	rx_ctx.showiv = 0;
	rx_ctx.prefena = 1;

	err = i40e_clear_lan_rx_queue_context(hw, pf_q);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to clear LAN RX queue context");
		return err;
	}
	err = i40e_set_lan_rx_queue_context(hw, pf_q, &rx_ctx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to set LAN RX queue context");
		return err;
	}

	rxq->qrx_tail = hw->hw_addr + I40E_QRX_TAIL(pf_q);

	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
		RTE_PKTMBUF_HEADROOM);

	/* Check if scattered RX needs to be used. */
	if (rxq->max_pkt_len > buf_size)
		dev_data->scattered_rx = 1;

	/* Init the RX tail register. */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	return 0;
}

void
i40e_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!dev->data->tx_queues[i])
			continue;
		i40e_tx_queue_release_mbufs(dev->data->tx_queues[i]);
		i40e_reset_tx_queue(dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!dev->data->rx_queues[i])
			continue;
		i40e_rx_queue_release_mbufs(dev->data->rx_queues[i]);
		i40e_reset_rx_queue(dev->data->rx_queues[i]);
	}
}

void
i40e_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!dev->data->rx_queues[i])
			continue;
		i40e_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!dev->data->tx_queues[i])
			continue;
		i40e_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
}

enum i40e_status_code
i40e_fdir_setup_tx_resources(struct i40e_pf *pf)
{
	struct i40e_tx_queue *txq;
	const struct rte_memzone *tz = NULL;
	struct rte_eth_dev *dev;
	uint32_t ring_size;

	if (!pf) {
		PMD_DRV_LOG(ERR, "PF is not available");
		return I40E_ERR_BAD_PTR;
	}

	dev = &rte_eth_devices[pf->dev_data->port_id];

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("i40e fdir tx queue",
				  sizeof(struct i40e_tx_queue),
				  RTE_CACHE_LINE_SIZE,
				  SOCKET_ID_ANY);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
					"tx queue structure.");
		return I40E_ERR_NO_MEMORY;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct i40e_tx_desc) * I40E_FDIR_NUM_TX_DESC;
	ring_size = RTE_ALIGN(ring_size, I40E_DMA_MEM_ALIGN);

	tz = rte_eth_dma_zone_reserve(dev, "fdir_tx_ring",
				      I40E_FDIR_QUEUE_ID, ring_size,
				      I40E_RING_BASE_ALIGN, SOCKET_ID_ANY);
	if (!tz) {
		i40e_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX.");
		return I40E_ERR_NO_MEMORY;
	}

	txq->mz = tz;
	txq->nb_tx_desc = I40E_FDIR_NUM_TX_DESC;
	txq->queue_id = I40E_FDIR_QUEUE_ID;
	txq->reg_idx = pf->fdir.fdir_vsi->base_queue;
	txq->vsi = pf->fdir.fdir_vsi;

	txq->tx_ring_phys_addr = tz->iova;
	txq->tx_ring = (struct i40e_tx_desc *)tz->addr;

	/*
	 * don't need to allocate software ring and reset for the fdir
	 * program queue just set the queue has been configured.
	 */
	txq->q_set = TRUE;
	pf->fdir.txq = txq;
	pf->fdir.txq_available_buf_count = I40E_FDIR_PRG_PKT_CNT;

	return I40E_SUCCESS;
}

enum i40e_status_code
i40e_fdir_setup_rx_resources(struct i40e_pf *pf)
{
	struct i40e_rx_queue *rxq;
	const struct rte_memzone *rz = NULL;
	uint32_t ring_size;
	struct rte_eth_dev *dev;

	if (!pf) {
		PMD_DRV_LOG(ERR, "PF is not available");
		return I40E_ERR_BAD_PTR;
	}

	dev = &rte_eth_devices[pf->dev_data->port_id];

	/* Allocate the RX queue data structure. */
	rxq = rte_zmalloc_socket("i40e fdir rx queue",
				  sizeof(struct i40e_rx_queue),
				  RTE_CACHE_LINE_SIZE,
				  SOCKET_ID_ANY);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
					"rx queue structure.");
		return I40E_ERR_NO_MEMORY;
	}

	/* Allocate RX hardware ring descriptors. */
	ring_size = sizeof(union i40e_rx_desc) * I40E_FDIR_NUM_RX_DESC;
	ring_size = RTE_ALIGN(ring_size, I40E_DMA_MEM_ALIGN);

	rz = rte_eth_dma_zone_reserve(dev, "fdir_rx_ring",
				      I40E_FDIR_QUEUE_ID, ring_size,
				      I40E_RING_BASE_ALIGN, SOCKET_ID_ANY);
	if (!rz) {
		i40e_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX.");
		return I40E_ERR_NO_MEMORY;
	}

	rxq->mz = rz;
	rxq->nb_rx_desc = I40E_FDIR_NUM_RX_DESC;
	rxq->queue_id = I40E_FDIR_QUEUE_ID;
	rxq->reg_idx = pf->fdir.fdir_vsi->base_queue;
	rxq->vsi = pf->fdir.fdir_vsi;

	rxq->rx_ring_phys_addr = rz->iova;
	memset(rz->addr, 0, I40E_FDIR_NUM_RX_DESC * sizeof(union i40e_rx_desc));
	rxq->rx_ring = (union i40e_rx_desc *)rz->addr;

	/*
	 * Don't need to allocate software ring and reset for the fdir
	 * rx queue, just set the queue has been configured.
	 */
	rxq->q_set = TRUE;
	pf->fdir.rxq = rxq;

	return I40E_SUCCESS;
}

void
i40e_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct i40e_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mp;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = rxq->drop_en;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
	qinfo->conf.offloads = rxq->offloads;
}

void
i40e_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct i40e_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;

	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_rs_thresh = txq->tx_rs_thresh;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
	qinfo->conf.offloads = txq->offloads;
}

#ifdef RTE_ARCH_X86
static inline bool
get_avx_supported(bool request_avx512)
{
	if (request_avx512) {
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1)
#ifdef CC_AVX512_SUPPORT
			return true;
#else
		PMD_DRV_LOG(NOTICE,
			"AVX512 is not supported in build env");
		return false;
#endif
	} else {
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1)
#ifdef CC_AVX2_SUPPORT
			return true;
#else
		PMD_DRV_LOG(NOTICE,
			"AVX2 is not supported in build env");
		return false;
#endif
	}

	return false;
}
#endif /* RTE_ARCH_X86 */


void __rte_cold
i40e_set_rx_function(struct rte_eth_dev *dev)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	uint16_t rx_using_sse, i;
	/* In order to allow Vector Rx there are a few configuration
	 * conditions to be met and Rx Bulk Allocation should be allowed.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
#ifdef RTE_ARCH_X86
		ad->rx_use_avx512 = false;
		ad->rx_use_avx2 = false;
#endif
		if (i40e_rx_vec_dev_conf_condition_check(dev) ||
		    !ad->rx_bulk_alloc_allowed) {
			PMD_INIT_LOG(DEBUG, "Port[%d] doesn't meet"
				     " Vector Rx preconditions",
				     dev->data->port_id);

			ad->rx_vec_allowed = false;
		}
		if (ad->rx_vec_allowed) {
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				struct i40e_rx_queue *rxq =
					dev->data->rx_queues[i];

				if (rxq && i40e_rxq_vec_setup(rxq)) {
					ad->rx_vec_allowed = false;
					break;
				}
			}
#ifdef RTE_ARCH_X86
			ad->rx_use_avx512 = get_avx_supported(1);

			if (!ad->rx_use_avx512)
				ad->rx_use_avx2 = get_avx_supported(0);
#endif
		}
	}

	if (ad->rx_vec_allowed  &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
#ifdef RTE_ARCH_X86
		if (dev->data->scattered_rx) {
			if (ad->rx_use_avx512) {
#ifdef CC_AVX512_SUPPORT
				PMD_DRV_LOG(NOTICE,
					"Using AVX512 Vector Scattered Rx (port %d).",
					dev->data->port_id);
				dev->rx_pkt_burst =
					i40e_recv_scattered_pkts_vec_avx512;
#endif
			} else {
				PMD_INIT_LOG(DEBUG,
					"Using %sVector Scattered Rx (port %d).",
					ad->rx_use_avx2 ? "avx2 " : "",
					dev->data->port_id);
				dev->rx_pkt_burst = ad->rx_use_avx2 ?
					i40e_recv_scattered_pkts_vec_avx2 :
					i40e_recv_scattered_pkts_vec;
			}
		} else {
			if (ad->rx_use_avx512) {
#ifdef CC_AVX512_SUPPORT
				PMD_DRV_LOG(NOTICE,
					"Using AVX512 Vector Rx (port %d).",
					dev->data->port_id);
				dev->rx_pkt_burst =
					i40e_recv_pkts_vec_avx512;
#endif
			} else {
				PMD_INIT_LOG(DEBUG,
					"Using %sVector Rx (port %d).",
					ad->rx_use_avx2 ? "avx2 " : "",
					dev->data->port_id);
				dev->rx_pkt_burst = ad->rx_use_avx2 ?
					i40e_recv_pkts_vec_avx2 :
					i40e_recv_pkts_vec;
			}
		}
#else /* RTE_ARCH_X86 */
		if (dev->data->scattered_rx) {
			PMD_INIT_LOG(DEBUG,
				     "Using Vector Scattered Rx (port %d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = i40e_recv_scattered_pkts_vec;
		} else {
			PMD_INIT_LOG(DEBUG, "Using Vector Rx (port %d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = i40e_recv_pkts_vec;
		}
#endif /* RTE_ARCH_X86 */
	} else if (!dev->data->scattered_rx && ad->rx_bulk_alloc_allowed) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
				    "satisfied. Rx Burst Bulk Alloc function "
				    "will be used on port=%d.",
			     dev->data->port_id);

		dev->rx_pkt_burst = i40e_recv_pkts_bulk_alloc;
	} else {
		/* Simple Rx Path. */
		PMD_INIT_LOG(DEBUG, "Simple Rx path will be used on port=%d.",
			     dev->data->port_id);
		dev->rx_pkt_burst = dev->data->scattered_rx ?
					i40e_recv_scattered_pkts :
					i40e_recv_pkts;
	}

	/* Propagate information about RX function choice through all queues. */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rx_using_sse =
			(dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec ||
			 dev->rx_pkt_burst == i40e_recv_pkts_vec ||
#ifdef CC_AVX512_SUPPORT
			 dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec_avx512 ||
			 dev->rx_pkt_burst == i40e_recv_pkts_vec_avx512 ||
#endif
			 dev->rx_pkt_burst == i40e_recv_scattered_pkts_vec_avx2 ||
			 dev->rx_pkt_burst == i40e_recv_pkts_vec_avx2);

		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			struct i40e_rx_queue *rxq = dev->data->rx_queues[i];

			if (rxq)
				rxq->rx_using_sse = rx_using_sse;
		}
	}
}

static const struct {
	eth_rx_burst_t pkt_burst;
	const char *info;
} i40e_rx_burst_infos[] = {
	{ i40e_recv_scattered_pkts,          "Scalar Scattered" },
	{ i40e_recv_pkts_bulk_alloc,         "Scalar Bulk Alloc" },
	{ i40e_recv_pkts,                    "Scalar" },
#ifdef RTE_ARCH_X86
#ifdef CC_AVX512_SUPPORT
	{ i40e_recv_scattered_pkts_vec_avx512, "Vector AVX512 Scattered" },
	{ i40e_recv_pkts_vec_avx512,           "Vector AVX512" },
#endif
	{ i40e_recv_scattered_pkts_vec_avx2, "Vector AVX2 Scattered" },
	{ i40e_recv_pkts_vec_avx2,           "Vector AVX2" },
	{ i40e_recv_scattered_pkts_vec,      "Vector SSE Scattered" },
	{ i40e_recv_pkts_vec,                "Vector SSE" },
#elif defined(RTE_ARCH_ARM64)
	{ i40e_recv_scattered_pkts_vec,      "Vector Neon Scattered" },
	{ i40e_recv_pkts_vec,                "Vector Neon" },
#elif defined(RTE_ARCH_PPC_64)
	{ i40e_recv_scattered_pkts_vec,      "Vector AltiVec Scattered" },
	{ i40e_recv_pkts_vec,                "Vector AltiVec" },
#endif
};

int
i40e_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		       struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(i40e_rx_burst_infos); ++i) {
		if (pkt_burst == i40e_rx_burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 i40e_rx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

void __rte_cold
i40e_set_tx_function_flag(struct rte_eth_dev *dev, struct i40e_tx_queue *txq)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	/* Use a simple Tx queue if possible (only fast free is allowed) */
	ad->tx_simple_allowed =
		(txq->offloads ==
		 (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) &&
		 txq->tx_rs_thresh >= RTE_PMD_I40E_TX_MAX_BURST);
	ad->tx_vec_allowed = (ad->tx_simple_allowed &&
			txq->tx_rs_thresh <= RTE_I40E_TX_MAX_FREE_BUF_SZ);

	if (ad->tx_vec_allowed)
		PMD_INIT_LOG(DEBUG, "Vector Tx can be enabled on Tx queue %u.",
				txq->queue_id);
	else if (ad->tx_simple_allowed)
		PMD_INIT_LOG(DEBUG, "Simple Tx can be enabled on Tx queue %u.",
				txq->queue_id);
	else
		PMD_INIT_LOG(DEBUG,
				"Neither simple nor vector Tx enabled on Tx queue %u\n",
				txq->queue_id);
}

void __rte_cold
i40e_set_tx_function(struct rte_eth_dev *dev)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int i;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
#ifdef RTE_ARCH_X86
		ad->tx_use_avx2 = false;
		ad->tx_use_avx512 = false;
#endif
		if (ad->tx_vec_allowed) {
			for (i = 0; i < dev->data->nb_tx_queues; i++) {
				struct i40e_tx_queue *txq =
					dev->data->tx_queues[i];

				if (txq && i40e_txq_vec_setup(txq)) {
					ad->tx_vec_allowed = false;
					break;
				}
			}
#ifdef RTE_ARCH_X86
			ad->tx_use_avx512 = get_avx_supported(1);

			if (!ad->tx_use_avx512)
				ad->tx_use_avx2 = get_avx_supported(0);
#endif
		}
	}

	if (ad->tx_simple_allowed) {
		if (ad->tx_vec_allowed &&
		    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
#ifdef RTE_ARCH_X86
			if (ad->tx_use_avx512) {
#ifdef CC_AVX512_SUPPORT
				PMD_DRV_LOG(NOTICE, "Using AVX512 Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst = i40e_xmit_pkts_vec_avx512;
#endif
			} else {
				PMD_INIT_LOG(DEBUG, "Using %sVector Tx (port %d).",
					     ad->tx_use_avx2 ? "avx2 " : "",
					     dev->data->port_id);
				dev->tx_pkt_burst = ad->tx_use_avx2 ?
						    i40e_xmit_pkts_vec_avx2 :
						    i40e_xmit_pkts_vec;
			}
#else /* RTE_ARCH_X86 */
			PMD_INIT_LOG(DEBUG, "Using Vector Tx (port %d).",
				     dev->data->port_id);
			dev->tx_pkt_burst = i40e_xmit_pkts_vec;
#endif /* RTE_ARCH_X86 */
		} else {
			PMD_INIT_LOG(DEBUG, "Simple tx finally be used.");
			dev->tx_pkt_burst = i40e_xmit_pkts_simple;
		}
		dev->tx_pkt_prepare = i40e_simple_prep_pkts;
	} else {
		PMD_INIT_LOG(DEBUG, "Xmit tx finally be used.");
		dev->tx_pkt_burst = i40e_xmit_pkts;
		dev->tx_pkt_prepare = i40e_prep_pkts;
	}
}

static const struct {
	eth_tx_burst_t pkt_burst;
	const char *info;
} i40e_tx_burst_infos[] = {
	{ i40e_xmit_pkts_simple,   "Scalar Simple" },
	{ i40e_xmit_pkts,          "Scalar" },
#ifdef RTE_ARCH_X86
#ifdef CC_AVX512_SUPPORT
	{ i40e_xmit_pkts_vec_avx512, "Vector AVX512" },
#endif
	{ i40e_xmit_pkts_vec_avx2, "Vector AVX2" },
	{ i40e_xmit_pkts_vec,      "Vector SSE" },
#elif defined(RTE_ARCH_ARM64)
	{ i40e_xmit_pkts_vec,      "Vector Neon" },
#elif defined(RTE_ARCH_PPC_64)
	{ i40e_xmit_pkts_vec,      "Vector AltiVec" },
#endif
};

int
i40e_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		       struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(i40e_tx_burst_infos); ++i) {
		if (pkt_burst == i40e_tx_burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 i40e_tx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

void __rte_cold
i40e_set_default_ptype_table(struct rte_eth_dev *dev)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int i;

	for (i = 0; i < I40E_MAX_PKT_TYPE; i++)
		ad->ptype_tbl[i] = i40e_get_default_pkt_type(i);
}

void __rte_cold
i40e_set_default_pctype_table(struct rte_eth_dev *dev)
{
	struct i40e_adapter *ad =
			I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int i;

	for (i = 0; i < I40E_FLOW_TYPE_MAX; i++)
		ad->pctypes_tbl[i] = 0ULL;
	ad->flow_types_mask = 0ULL;
	ad->pctypes_mask = 0ULL;

	ad->pctypes_tbl[RTE_ETH_FLOW_FRAG_IPV4] =
				(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_SCTP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_OTHER] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER);
	ad->pctypes_tbl[RTE_ETH_FLOW_FRAG_IPV6] =
				(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_SCTP] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP);
	ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_OTHER] =
				(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER);
	ad->pctypes_tbl[RTE_ETH_FLOW_L2_PAYLOAD] =
				(1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD);

	if (hw->mac.type == I40E_MAC_X722 ||
		hw->mac.type == I40E_MAC_X722_VF) {
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP);
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP);
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK);
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP);
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP);
		ad->pctypes_tbl[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] |=
			(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK);
	}

	for (i = 0; i < I40E_FLOW_TYPE_MAX; i++) {
		if (ad->pctypes_tbl[i])
			ad->flow_types_mask |= (1ULL << i);
		ad->pctypes_mask |= ad->pctypes_tbl[i];
	}
}

#ifndef CC_AVX2_SUPPORT
uint16_t
i40e_recv_pkts_vec_avx2(void __rte_unused *rx_queue,
			struct rte_mbuf __rte_unused **rx_pkts,
			uint16_t __rte_unused nb_pkts)
{
	return 0;
}

uint16_t
i40e_recv_scattered_pkts_vec_avx2(void __rte_unused *rx_queue,
			struct rte_mbuf __rte_unused **rx_pkts,
			uint16_t __rte_unused nb_pkts)
{
	return 0;
}

uint16_t
i40e_xmit_pkts_vec_avx2(void __rte_unused * tx_queue,
			  struct rte_mbuf __rte_unused **tx_pkts,
			  uint16_t __rte_unused nb_pkts)
{
	return 0;
}
#endif /* ifndef CC_AVX2_SUPPORT */
