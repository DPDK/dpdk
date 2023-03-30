/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_io.h>
#include <rte_ip.h>
#include <rte_tcp.h>

#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"

#include "sfc_debug.h"
#include "sfc_dp_tx.h"
#include "sfc_tweak.h"
#include "sfc_kvargs.h"
#include "sfc_ef10.h"
#include "sfc_tso.h"

#define sfc_ef10_tx_err(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, ERR, dpq, __VA_ARGS__)

#define sfc_ef10_tx_info(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, INFO, dpq, __VA_ARGS__)

/** Maximum length of the DMA descriptor data */
#define SFC_EF10_TX_DMA_DESC_LEN_MAX \
	((1u << ESF_DZ_TX_KER_BYTE_CNT_WIDTH) - 1)

/**
 * Maximum number of descriptors/buffers in the Tx ring.
 * It should guarantee that corresponding event queue never overfill.
 * EF10 native datapath uses event queue of the same size as Tx queue.
 * Maximum number of events on datapath can be estimated as number of
 * Tx queue entries (one event per Tx buffer in the worst case) plus
 * Tx error and flush events.
 */
#define SFC_EF10_TXQ_LIMIT(_ndesc) \
	((_ndesc) - 1 /* head must not step on tail */ - \
	 (SFC_EF10_EV_PER_CACHE_LINE - 1) /* max unused EvQ entries */ - \
	 1 /* Rx error */ - 1 /* flush */)

struct sfc_ef10_tx_sw_desc {
	struct rte_mbuf			*mbuf;
};

struct sfc_ef10_txq {
	unsigned int			flags;
#define SFC_EF10_TXQ_STARTED		0x1
#define SFC_EF10_TXQ_NOT_RUNNING	0x2
#define SFC_EF10_TXQ_EXCEPTION		0x4

	unsigned int			ptr_mask;
	unsigned int			added;
	unsigned int			completed;
	unsigned int			max_fill_level;
	unsigned int			free_thresh;
	unsigned int			evq_read_ptr;
	struct sfc_ef10_tx_sw_desc	*sw_ring;
	efx_qword_t			*txq_hw_ring;
	volatile void			*doorbell;
	efx_qword_t			*evq_hw_ring;
	uint8_t				*tsoh;
	rte_iova_t			tsoh_iova;
	uint16_t			tso_tcp_header_offset_limit;

	/* Datapath transmit queue anchor */
	struct sfc_dp_txq		dp;
};

static inline struct sfc_ef10_txq *
sfc_ef10_txq_by_dp_txq(struct sfc_dp_txq *dp_txq)
{
	return container_of(dp_txq, struct sfc_ef10_txq, dp);
}

static bool
sfc_ef10_tx_get_event(struct sfc_ef10_txq *txq, efx_qword_t *tx_ev)
{
	volatile efx_qword_t *evq_hw_ring = txq->evq_hw_ring;

	/*
	 * Exception flag is set when reap is done.
	 * It is never done twice per packet burst get and absence of
	 * the flag is checked on burst get entry.
	 */
	SFC_ASSERT((txq->flags & SFC_EF10_TXQ_EXCEPTION) == 0);

	*tx_ev = evq_hw_ring[txq->evq_read_ptr & txq->ptr_mask];

	if (!sfc_ef10_ev_present(*tx_ev))
		return false;

	if (unlikely(EFX_QWORD_FIELD(*tx_ev, FSF_AZ_EV_CODE) !=
		     FSE_AZ_EV_CODE_TX_EV)) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling by the control path.
		 */
		txq->flags |= SFC_EF10_TXQ_EXCEPTION;
		sfc_ef10_tx_err(&txq->dp.dpq,
				"TxQ exception at EvQ read ptr %#x",
				txq->evq_read_ptr);
		return false;
	}

	txq->evq_read_ptr++;
	return true;
}

static unsigned int
sfc_ef10_tx_process_events(struct sfc_ef10_txq *txq)
{
	const unsigned int curr_done = txq->completed - 1;
	unsigned int anew_done = curr_done;
	efx_qword_t tx_ev;

	while (sfc_ef10_tx_get_event(txq, &tx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */

		/* Update the latest done descriptor */
		anew_done = EFX_QWORD_FIELD(tx_ev, ESF_DZ_TX_DESCR_INDX);
	}
	return (anew_done - curr_done) & txq->ptr_mask;
}

static void
sfc_ef10_tx_reap(struct sfc_ef10_txq *txq)
{
	const unsigned int old_read_ptr = txq->evq_read_ptr;
	const unsigned int ptr_mask = txq->ptr_mask;
	unsigned int completed = txq->completed;
	unsigned int pending = completed;

	pending += sfc_ef10_tx_process_events(txq);

	if (pending != completed) {
		struct rte_mbuf *bulk[SFC_TX_REAP_BULK_SIZE];
		unsigned int nb = 0;

		do {
			struct sfc_ef10_tx_sw_desc *txd;
			struct rte_mbuf *m;

			txd = &txq->sw_ring[completed & ptr_mask];
			if (txd->mbuf == NULL)
				continue;

			m = rte_pktmbuf_prefree_seg(txd->mbuf);
			txd->mbuf = NULL;
			if (m == NULL)
				continue;

			if ((nb == RTE_DIM(bulk)) ||
			    ((nb != 0) && (m->pool != bulk[0]->pool))) {
				rte_mempool_put_bulk(bulk[0]->pool,
						     (void *)bulk, nb);
				nb = 0;
			}

			bulk[nb++] = m;
		} while (++completed != pending);

		if (nb != 0)
			rte_mempool_put_bulk(bulk[0]->pool, (void *)bulk, nb);

		txq->completed = completed;
	}

	sfc_ef10_ev_qclear(txq->evq_hw_ring, ptr_mask, old_read_ptr,
			   txq->evq_read_ptr);
}

static void
sfc_ef10_tx_qdesc_dma_create(rte_iova_t addr, uint16_t size, bool eop,
			     efx_qword_t *edp)
{
	EFX_POPULATE_QWORD_4(*edp,
			     ESF_DZ_TX_KER_TYPE, 0,
			     ESF_DZ_TX_KER_CONT, !eop,
			     ESF_DZ_TX_KER_BYTE_CNT, size,
			     ESF_DZ_TX_KER_BUF_ADDR, addr);
}

static void
sfc_ef10_tx_qdesc_tso2_create(struct sfc_ef10_txq * const txq,
			      unsigned int added, uint16_t ipv4_id,
			      uint16_t outer_ipv4_id, uint32_t tcp_seq,
			      uint16_t tcp_mss)
{
	EFX_POPULATE_QWORD_5(txq->txq_hw_ring[added & txq->ptr_mask],
			    ESF_DZ_TX_DESC_IS_OPT, 1,
			    ESF_DZ_TX_OPTION_TYPE,
			    ESE_DZ_TX_OPTION_DESC_TSO,
			    ESF_DZ_TX_TSO_OPTION_TYPE,
			    ESE_DZ_TX_TSO_OPTION_DESC_FATSO2A,
			    ESF_DZ_TX_TSO_IP_ID, ipv4_id,
			    ESF_DZ_TX_TSO_TCP_SEQNO, tcp_seq);
	EFX_POPULATE_QWORD_5(txq->txq_hw_ring[(added + 1) & txq->ptr_mask],
			    ESF_DZ_TX_DESC_IS_OPT, 1,
			    ESF_DZ_TX_OPTION_TYPE,
			    ESE_DZ_TX_OPTION_DESC_TSO,
			    ESF_DZ_TX_TSO_OPTION_TYPE,
			    ESE_DZ_TX_TSO_OPTION_DESC_FATSO2B,
			    ESF_DZ_TX_TSO_TCP_MSS, tcp_mss,
			    ESF_DZ_TX_TSO_OUTER_IPID, outer_ipv4_id);
}

static inline void
sfc_ef10_tx_qpush(struct sfc_ef10_txq *txq, unsigned int added,
		  unsigned int pushed)
{
	efx_qword_t desc;
	efx_oword_t oword;

	/*
	 * This improves performance by pushing a TX descriptor at the same
	 * time as the doorbell. The descriptor must be added to the TXQ,
	 * so that can be used if the hardware decides not to use the pushed
	 * descriptor.
	 */
	desc.eq_u64[0] = txq->txq_hw_ring[pushed & txq->ptr_mask].eq_u64[0];
	EFX_POPULATE_OWORD_3(oword,
		ERF_DZ_TX_DESC_WPTR, added & txq->ptr_mask,
		ERF_DZ_TX_DESC_HWORD, EFX_QWORD_FIELD(desc, EFX_DWORD_1),
		ERF_DZ_TX_DESC_LWORD, EFX_QWORD_FIELD(desc, EFX_DWORD_0));

	/* DMA sync to device is not required */

	/*
	 * rte_io_wmb() which guarantees that the STORE operations
	 * (i.e. Tx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_io_wmb();

	*(volatile efsys_uint128_t *)txq->doorbell = oword.eo_u128[0];
	txq->dp.dpq.dbells++;
}

static unsigned int
sfc_ef10_tx_pkt_descs_max(const struct rte_mbuf *m)
{
	unsigned int extra_descs_per_seg;
	unsigned int extra_descs_per_pkt;

	/*
	 * VLAN offload is not supported yet, so no extra descriptors
	 * are required for VLAN option descriptor.
	 */

/** Maximum length of the mbuf segment data */
#define SFC_MBUF_SEG_LEN_MAX		UINT16_MAX
	RTE_BUILD_BUG_ON(sizeof(m->data_len) != 2);

	/*
	 * Each segment is already counted once below.  So, calculate
	 * how many extra DMA descriptors may be required per segment in
	 * the worst case because of maximum DMA descriptor length limit.
	 * If maximum segment length is less or equal to maximum DMA
	 * descriptor length, no extra DMA descriptors are required.
	 */
	extra_descs_per_seg =
		(SFC_MBUF_SEG_LEN_MAX - 1) / SFC_EF10_TX_DMA_DESC_LEN_MAX;

/** Maximum length of the packet */
#define SFC_MBUF_PKT_LEN_MAX		UINT32_MAX
	RTE_BUILD_BUG_ON(sizeof(m->pkt_len) != 4);

	/*
	 * One more limitation on maximum number of extra DMA descriptors
	 * comes from slicing entire packet because of DMA descriptor length
	 * limit taking into account that there is at least one segment
	 * which is already counted below (so division of the maximum
	 * packet length minus one with round down).
	 * TSO is not supported yet, so packet length is limited by
	 * maximum PDU size.
	 */
	extra_descs_per_pkt =
		(RTE_MIN((unsigned int)EFX_MAC_PDU_MAX,
			 SFC_MBUF_PKT_LEN_MAX) - 1) /
		SFC_EF10_TX_DMA_DESC_LEN_MAX;

	return m->nb_segs + RTE_MIN(m->nb_segs * extra_descs_per_seg,
				    extra_descs_per_pkt);
}

static bool
sfc_ef10_try_reap(struct sfc_ef10_txq * const txq, unsigned int added,
		  unsigned int needed_desc, unsigned int *dma_desc_space,
		  bool *reap_done)
{
	if (*reap_done)
		return false;

	if (added != txq->added) {
		sfc_ef10_tx_qpush(txq, added, txq->added);
		txq->added = added;
	}

	sfc_ef10_tx_reap(txq);
	*reap_done = true;

	/*
	 * Recalculate DMA descriptor space since Tx reap may change
	 * the number of completed descriptors
	 */
	*dma_desc_space = txq->max_fill_level -
		(added - txq->completed);

	return (needed_desc <= *dma_desc_space);
}

static uint16_t
sfc_ef10_prepare_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	struct sfc_ef10_txq * const txq = sfc_ef10_txq_by_dp_txq(tx_queue);
	uint16_t i;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *m = tx_pkts[i];
		int ret;

#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
		/*
		 * In non-TSO case, check that a packet segments do not exceed
		 * the size limit. Perform the check in debug mode since MTU
		 * more than 9k is not supported, but the limit here is 16k-1.
		 */
		if (!(m->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			struct rte_mbuf *m_seg;

			for (m_seg = m; m_seg != NULL; m_seg = m_seg->next) {
				if (m_seg->data_len >
				    SFC_EF10_TX_DMA_DESC_LEN_MAX) {
					rte_errno = EINVAL;
					break;
				}
			}
		}
#endif
		ret = sfc_dp_tx_prepare_pkt(m, 0, SFC_TSOH_STD_LEN,
				txq->tso_tcp_header_offset_limit,
				txq->max_fill_level,
				SFC_EF10_TSO_OPT_DESCS_NUM, 0);
		if (unlikely(ret != 0)) {
			rte_errno = ret;
			break;
		}
	}

	return i;
}

static int
sfc_ef10_xmit_tso_pkt(struct sfc_ef10_txq * const txq, struct rte_mbuf *m_seg,
		      unsigned int *added, unsigned int *dma_desc_space,
		      bool *reap_done)
{
	size_t iph_off = ((m_seg->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
			  m_seg->outer_l2_len + m_seg->outer_l3_len : 0) +
			 m_seg->l2_len;
	size_t tcph_off = iph_off + m_seg->l3_len;
	size_t header_len = tcph_off + m_seg->l4_len;
	/* Offset of the payload in the last segment that contains the header */
	size_t in_off = 0;
	const struct rte_tcp_hdr *th;
	uint16_t packet_id = 0;
	uint16_t outer_packet_id = 0;
	uint32_t sent_seq;
	uint8_t *hdr_addr;
	rte_iova_t hdr_iova;
	struct rte_mbuf *first_m_seg = m_seg;
	unsigned int pkt_start = *added;
	unsigned int needed_desc;
	struct rte_mbuf *m_seg_to_free_up_to = first_m_seg;
	bool eop;

	/*
	 * Preliminary estimation of required DMA descriptors, including extra
	 * descriptor for TSO header that is needed when the header is
	 * separated from payload in one segment. It does not include
	 * extra descriptors that may appear when a big segment is split across
	 * several descriptors.
	 */
	needed_desc = m_seg->nb_segs +
			(unsigned int)SFC_EF10_TSO_OPT_DESCS_NUM +
			(unsigned int)SFC_EF10_TSO_HDR_DESCS_NUM;

	if (needed_desc > *dma_desc_space &&
	    !sfc_ef10_try_reap(txq, pkt_start, needed_desc,
			       dma_desc_space, reap_done)) {
		/*
		 * If a future Tx reap may increase available DMA descriptor
		 * space, do not try to send the packet.
		 */
		if (txq->completed != pkt_start)
			return ENOSPC;
		/*
		 * Do not allow to send packet if the maximum DMA
		 * descriptor space is not sufficient to hold TSO
		 * descriptors, header descriptor and at least 1
		 * segment descriptor.
		 */
		if (*dma_desc_space < SFC_EF10_TSO_OPT_DESCS_NUM +
				SFC_EF10_TSO_HDR_DESCS_NUM + 1)
			return EMSGSIZE;
	}

	/* Check if the header is not fragmented */
	if (rte_pktmbuf_data_len(m_seg) >= header_len) {
		hdr_addr = rte_pktmbuf_mtod(m_seg, uint8_t *);
		hdr_iova = rte_mbuf_data_iova(m_seg);
		if (rte_pktmbuf_data_len(m_seg) == header_len) {
			/* Cannot send a packet that consists only of header */
			if (unlikely(m_seg->next == NULL))
				return EMSGSIZE;
			/*
			 * Associate header mbuf with header descriptor
			 * which is located after TSO descriptors.
			 */
			txq->sw_ring[(pkt_start + SFC_EF10_TSO_OPT_DESCS_NUM) &
				     txq->ptr_mask].mbuf = m_seg;
			m_seg = m_seg->next;
			in_off = 0;

			/*
			 * If there is no payload offset (payload starts at the
			 * beginning of a segment) then an extra descriptor for
			 * separated header is not needed.
			 */
			needed_desc--;
		} else {
			in_off = header_len;
		}
	} else {
		unsigned int copied_segs;
		unsigned int hdr_addr_off = (*added & txq->ptr_mask) *
				SFC_TSOH_STD_LEN;

		/*
		 * Discard a packet if header linearization is needed but
		 * the header is too big.
		 * Duplicate Tx prepare check here to avoid spoil of
		 * memory if Tx prepare is skipped.
		 */
		if (unlikely(header_len > SFC_TSOH_STD_LEN))
			return EMSGSIZE;

		hdr_addr = txq->tsoh + hdr_addr_off;
		hdr_iova = txq->tsoh_iova + hdr_addr_off;
		copied_segs = sfc_tso_prepare_header(hdr_addr, header_len,
						     &m_seg, &in_off);

		/* Cannot send a packet that consists only of header */
		if (unlikely(m_seg == NULL))
			return EMSGSIZE;

		m_seg_to_free_up_to = m_seg;
		/*
		 * Reduce the number of needed descriptors by the number of
		 * segments that entirely consist of header data.
		 */
		needed_desc -= copied_segs;

		/* Extra descriptor for separated header is not needed */
		if (in_off == 0)
			needed_desc--;
	}

	/*
	 * 8000-series EF10 hardware requires that innermost IP length
	 * be greater than or equal to the value which each segment is
	 * supposed to have; otherwise, TCP checksum will be incorrect.
	 *
	 * The same concern applies to outer UDP datagram length field.
	 */
	switch (m_seg->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
		/* FALLTHROUGH */
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		sfc_tso_outer_udp_fix_len(first_m_seg, hdr_addr);
		break;
	default:
		break;
	}

	sfc_tso_innermost_ip_fix_len(first_m_seg, hdr_addr, iph_off);

	/*
	 * Tx prepare has debug-only checks that offload flags are correctly
	 * filled in TSO mbuf. Use zero IPID if there is no IPv4 flag.
	 * If the packet is still IPv4, HW will simply start from zero IPID.
	 */
	if (first_m_seg->ol_flags & RTE_MBUF_F_TX_IPV4)
		packet_id = sfc_tso_ip4_get_ipid(hdr_addr, iph_off);

	if (first_m_seg->ol_flags & RTE_MBUF_F_TX_OUTER_IPV4)
		outer_packet_id = sfc_tso_ip4_get_ipid(hdr_addr,
						first_m_seg->outer_l2_len);

	th = (const struct rte_tcp_hdr *)(hdr_addr + tcph_off);
	rte_memcpy(&sent_seq, &th->sent_seq, sizeof(uint32_t));
	sent_seq = rte_be_to_cpu_32(sent_seq);

	sfc_ef10_tx_qdesc_tso2_create(txq, *added, packet_id, outer_packet_id,
			sent_seq, first_m_seg->tso_segsz);
	(*added) += SFC_EF10_TSO_OPT_DESCS_NUM;

	sfc_ef10_tx_qdesc_dma_create(hdr_iova, header_len, false,
			&txq->txq_hw_ring[(*added) & txq->ptr_mask]);
	(*added)++;

	do {
		rte_iova_t next_frag = rte_mbuf_data_iova(m_seg);
		unsigned int seg_len = rte_pktmbuf_data_len(m_seg);
		unsigned int id;

		next_frag += in_off;
		seg_len -= in_off;
		in_off = 0;

		do {
			rte_iova_t frag_addr = next_frag;
			size_t frag_len;

			frag_len = RTE_MIN(seg_len,
					   SFC_EF10_TX_DMA_DESC_LEN_MAX);

			next_frag += frag_len;
			seg_len -= frag_len;

			eop = (seg_len == 0 && m_seg->next == NULL);

			id = (*added) & txq->ptr_mask;
			(*added)++;

			/*
			 * Initially we assume that one DMA descriptor is needed
			 * for every segment. When the segment is split across
			 * several DMA descriptors, increase the estimation.
			 */
			needed_desc += (seg_len != 0);

			/*
			 * When no more descriptors can be added, but not all
			 * segments are processed.
			 */
			if (*added - pkt_start == *dma_desc_space &&
			    !eop &&
			    !sfc_ef10_try_reap(txq, pkt_start, needed_desc,
						dma_desc_space, reap_done)) {
				struct rte_mbuf *m;
				struct rte_mbuf *m_next;

				if (txq->completed != pkt_start) {
					unsigned int i;

					/*
					 * Reset mbuf associations with added
					 * descriptors.
					 */
					for (i = pkt_start; i != *added; i++) {
						id = i & txq->ptr_mask;
						txq->sw_ring[id].mbuf = NULL;
					}
					return ENOSPC;
				}

				/* Free the segments that cannot be sent */
				for (m = m_seg->next; m != NULL; m = m_next) {
					m_next = m->next;
					rte_pktmbuf_free_seg(m);
				}
				eop = true;
				/* Ignore the rest of the segment */
				seg_len = 0;
			}

			sfc_ef10_tx_qdesc_dma_create(frag_addr, frag_len,
					eop, &txq->txq_hw_ring[id]);

		} while (seg_len != 0);

		txq->sw_ring[id].mbuf = m_seg;

		m_seg = m_seg->next;
	} while (!eop);

	/*
	 * Free segments which content was entirely copied to the TSO header
	 * memory space of Tx queue
	 */
	for (m_seg = first_m_seg; m_seg != m_seg_to_free_up_to;) {
		struct rte_mbuf *seg_to_free = m_seg;

		m_seg = m_seg->next;
		rte_pktmbuf_free_seg(seg_to_free);
	}

	return 0;
}

static uint16_t
sfc_ef10_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sfc_ef10_txq * const txq = sfc_ef10_txq_by_dp_txq(tx_queue);
	unsigned int added;
	unsigned int dma_desc_space;
	bool reap_done;
	struct rte_mbuf **pktp;
	struct rte_mbuf **pktp_end;

	if (unlikely(txq->flags &
		     (SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION)))
		return 0;

	added = txq->added;
	dma_desc_space = txq->max_fill_level - (added - txq->completed);

	reap_done = (dma_desc_space < txq->free_thresh);
	if (reap_done) {
		sfc_ef10_tx_reap(txq);
		dma_desc_space = txq->max_fill_level - (added - txq->completed);
	}

	for (pktp = &tx_pkts[0], pktp_end = &tx_pkts[nb_pkts];
	     pktp != pktp_end;
	     ++pktp) {
		struct rte_mbuf *m_seg = *pktp;
		unsigned int pkt_start = added;
		uint32_t pkt_len;

		if (likely(pktp + 1 != pktp_end))
			rte_mbuf_prefetch_part1(pktp[1]);

		if (m_seg->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
			int rc;

			rc = sfc_ef10_xmit_tso_pkt(txq, m_seg, &added,
					&dma_desc_space, &reap_done);
			if (rc != 0) {
				added = pkt_start;

				/* Packet can be sent in following xmit calls */
				if (likely(rc == ENOSPC))
					break;

				/*
				 * Packet cannot be sent, tell RTE that
				 * it is sent, but actually drop it and
				 * continue with another packet
				 */
				rte_pktmbuf_free(*pktp);
				continue;
			}

			goto dma_desc_space_update;
		}

		if (sfc_ef10_tx_pkt_descs_max(m_seg) > dma_desc_space) {
			if (reap_done)
				break;

			/* Push already prepared descriptors before polling */
			if (added != txq->added) {
				sfc_ef10_tx_qpush(txq, added, txq->added);
				txq->added = added;
			}

			sfc_ef10_tx_reap(txq);
			reap_done = true;
			dma_desc_space = txq->max_fill_level -
				(added - txq->completed);
			if (sfc_ef10_tx_pkt_descs_max(m_seg) > dma_desc_space)
				break;
		}

		pkt_len = m_seg->pkt_len;
		do {
			rte_iova_t seg_addr = rte_mbuf_data_iova(m_seg);
			unsigned int seg_len = rte_pktmbuf_data_len(m_seg);
			unsigned int id = added & txq->ptr_mask;

			SFC_ASSERT(seg_len <= SFC_EF10_TX_DMA_DESC_LEN_MAX);

			pkt_len -= seg_len;

			sfc_ef10_tx_qdesc_dma_create(seg_addr,
				seg_len, (pkt_len == 0),
				&txq->txq_hw_ring[id]);

			/*
			 * rte_pktmbuf_free() is commonly used in DPDK for
			 * recycling packets - the function checks every
			 * segment's reference counter and returns the
			 * buffer to its pool whenever possible;
			 * nevertheless, freeing mbuf segments one by one
			 * may entail some performance decline;
			 * from this point, sfc_efx_tx_reap() does the same job
			 * on its own and frees buffers in bulks (all mbufs
			 * within a bulk belong to the same pool);
			 * from this perspective, individual segment pointers
			 * must be associated with the corresponding SW
			 * descriptors independently so that only one loop
			 * is sufficient on reap to inspect all the buffers
			 */
			txq->sw_ring[id].mbuf = m_seg;

			++added;

		} while ((m_seg = m_seg->next) != 0);

dma_desc_space_update:
		dma_desc_space -= (added - pkt_start);
	}

	if (likely(added != txq->added)) {
		sfc_ef10_tx_qpush(txq, added, txq->added);
		txq->added = added;
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_ef10_tx_reap(txq);
#endif

	return pktp - &tx_pkts[0];
}

static void
sfc_ef10_simple_tx_reap(struct sfc_ef10_txq *txq)
{
	const unsigned int old_read_ptr = txq->evq_read_ptr;
	const unsigned int ptr_mask = txq->ptr_mask;
	unsigned int completed = txq->completed;
	unsigned int pending = completed;

	pending += sfc_ef10_tx_process_events(txq);

	if (pending != completed) {
		struct rte_mbuf *bulk[SFC_TX_REAP_BULK_SIZE];
		unsigned int nb = 0;

		do {
			struct sfc_ef10_tx_sw_desc *txd;

			txd = &txq->sw_ring[completed & ptr_mask];

			if (nb == RTE_DIM(bulk)) {
				rte_mempool_put_bulk(bulk[0]->pool,
						     (void *)bulk, nb);
				nb = 0;
			}

			bulk[nb++] = txd->mbuf;
		} while (++completed != pending);

		rte_mempool_put_bulk(bulk[0]->pool, (void *)bulk, nb);

		txq->completed = completed;
	}

	sfc_ef10_ev_qclear(txq->evq_hw_ring, ptr_mask, old_read_ptr,
			   txq->evq_read_ptr);
}

#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
static uint16_t
sfc_ef10_simple_prepare_pkts(__rte_unused void *tx_queue,
			     struct rte_mbuf **tx_pkts,
			     uint16_t nb_pkts)
{
	uint16_t i;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *m = tx_pkts[i];
		int ret;

		ret = rte_validate_tx_offload(m);
		if (unlikely(ret != 0)) {
			/*
			 * Negative error code is returned by
			 * rte_validate_tx_offload(), but positive are used
			 * inside net/sfc PMD.
			 */
			SFC_ASSERT(ret < 0);
			rte_errno = -ret;
			break;
		}

		/* ef10_simple does not support TSO and VLAN insertion */
		if (unlikely(m->ol_flags &
			     (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_VLAN))) {
			rte_errno = ENOTSUP;
			break;
		}

		/* ef10_simple does not support scattered packets */
		if (unlikely(m->nb_segs != 1)) {
			rte_errno = ENOTSUP;
			break;
		}

		/*
		 * ef10_simple requires fast-free which ignores reference
		 * counters
		 */
		if (unlikely(rte_mbuf_refcnt_read(m) != 1)) {
			rte_errno = ENOTSUP;
			break;
		}

		/* ef10_simple requires single pool for all packets */
		if (unlikely(m->pool != tx_pkts[0]->pool)) {
			rte_errno = ENOTSUP;
			break;
		}
	}

	return i;
}
#endif

static uint16_t
sfc_ef10_simple_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct sfc_ef10_txq * const txq = sfc_ef10_txq_by_dp_txq(tx_queue);
	unsigned int ptr_mask;
	unsigned int added;
	unsigned int dma_desc_space;
	bool reap_done;
	struct rte_mbuf **pktp;
	struct rte_mbuf **pktp_end;

	if (unlikely(txq->flags &
		     (SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION)))
		return 0;

	ptr_mask = txq->ptr_mask;
	added = txq->added;
	dma_desc_space = txq->max_fill_level - (added - txq->completed);

	reap_done = (dma_desc_space < RTE_MAX(txq->free_thresh, nb_pkts));
	if (reap_done) {
		sfc_ef10_simple_tx_reap(txq);
		dma_desc_space = txq->max_fill_level - (added - txq->completed);
	}

	pktp_end = &tx_pkts[MIN(nb_pkts, dma_desc_space)];
	for (pktp = &tx_pkts[0]; pktp != pktp_end; ++pktp) {
		struct rte_mbuf *pkt = *pktp;
		unsigned int id = added & ptr_mask;

		SFC_ASSERT(rte_pktmbuf_data_len(pkt) <=
			   SFC_EF10_TX_DMA_DESC_LEN_MAX);

		sfc_ef10_tx_qdesc_dma_create(rte_mbuf_data_iova(pkt),
					     rte_pktmbuf_data_len(pkt),
					     true, &txq->txq_hw_ring[id]);

		txq->sw_ring[id].mbuf = pkt;

		++added;
	}

	if (likely(added != txq->added)) {
		sfc_ef10_tx_qpush(txq, added, txq->added);
		txq->added = added;
	}

#if SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE
	if (!reap_done)
		sfc_ef10_simple_tx_reap(txq);
#endif

	return pktp - &tx_pkts[0];
}

static sfc_dp_tx_get_dev_info_t sfc_ef10_get_dev_info;
static void
sfc_ef10_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->tx_desc_lim.nb_min = 1;
	dev_info->tx_desc_lim.nb_align = 1;
}

static sfc_dp_tx_qsize_up_rings_t sfc_ef10_tx_qsize_up_rings;
static int
sfc_ef10_tx_qsize_up_rings(uint16_t nb_tx_desc,
			   struct sfc_dp_tx_hw_limits *limits,
			   unsigned int *txq_entries,
			   unsigned int *evq_entries,
			   unsigned int *txq_max_fill_level)
{
	/*
	 * rte_ethdev API guarantees that the number meets min, max and
	 * alignment requirements.
	 */
	if (nb_tx_desc <= limits->txq_min_entries)
		*txq_entries = limits->txq_min_entries;
	else
		*txq_entries = rte_align32pow2(nb_tx_desc);

	*evq_entries = *txq_entries;

	*txq_max_fill_level = RTE_MIN(nb_tx_desc,
				      SFC_EF10_TXQ_LIMIT(*evq_entries));
	return 0;
}

static sfc_dp_tx_qcreate_t sfc_ef10_tx_qcreate;
static int
sfc_ef10_tx_qcreate(uint16_t port_id, uint16_t queue_id,
		    const struct rte_pci_addr *pci_addr, int socket_id,
		    const struct sfc_dp_tx_qcreate_info *info,
		    struct sfc_dp_txq **dp_txqp)
{
	struct sfc_ef10_txq *txq;
	int rc;

	rc = EINVAL;
	if (info->txq_entries != info->evq_entries)
		goto fail_bad_args;

	rc = ENOTSUP;
	if (info->nic_dma_info->nb_regions > 0)
		goto fail_nic_dma;

	rc = ENOMEM;
	txq = rte_zmalloc_socket("sfc-ef10-txq", sizeof(*txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		goto fail_txq_alloc;

	sfc_dp_queue_init(&txq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	txq->sw_ring = rte_calloc_socket("sfc-ef10-txq-sw_ring",
					 info->txq_entries,
					 sizeof(*txq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL)
		goto fail_sw_ring_alloc;

	if (info->offloads & (RTE_ETH_TX_OFFLOAD_TCP_TSO |
			      RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
			      RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO)) {
		txq->tsoh = rte_calloc_socket("sfc-ef10-txq-tsoh",
					      info->txq_entries,
					      SFC_TSOH_STD_LEN,
					      RTE_CACHE_LINE_SIZE,
					      socket_id);
		if (txq->tsoh == NULL)
			goto fail_tsoh_alloc;

		txq->tsoh_iova = rte_malloc_virt2iova(txq->tsoh);
	}

	txq->flags = SFC_EF10_TXQ_NOT_RUNNING;
	txq->ptr_mask = info->txq_entries - 1;
	txq->max_fill_level = info->max_fill_level;
	txq->free_thresh = info->free_thresh;
	txq->txq_hw_ring = info->txq_hw_ring;
	txq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_DZ_TX_DESC_UPD_REG_OFST +
			(info->hw_index << info->vi_window_shift);
	txq->evq_hw_ring = info->evq_hw_ring;
	txq->tso_tcp_header_offset_limit = info->tso_tcp_header_offset_limit;

	sfc_ef10_tx_info(&txq->dp.dpq, "TxQ doorbell is %p", txq->doorbell);

	*dp_txqp = &txq->dp;
	return 0;

fail_tsoh_alloc:
	rte_free(txq->sw_ring);

fail_sw_ring_alloc:
	rte_free(txq);

fail_txq_alloc:
fail_nic_dma:
fail_bad_args:
	return rc;
}

static sfc_dp_tx_qdestroy_t sfc_ef10_tx_qdestroy;
static void
sfc_ef10_tx_qdestroy(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	rte_free(txq->tsoh);
	rte_free(txq->sw_ring);
	rte_free(txq);
}

static sfc_dp_tx_qstart_t sfc_ef10_tx_qstart;
static int
sfc_ef10_tx_qstart(struct sfc_dp_txq *dp_txq, unsigned int evq_read_ptr,
		   unsigned int txq_desc_index)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	txq->evq_read_ptr = evq_read_ptr;
	txq->added = txq->completed = txq_desc_index;

	txq->flags |= SFC_EF10_TXQ_STARTED;
	txq->flags &= ~(SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION);

	return 0;
}

static sfc_dp_tx_qstop_t sfc_ef10_tx_qstop;
static void
sfc_ef10_tx_qstop(struct sfc_dp_txq *dp_txq, unsigned int *evq_read_ptr)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	txq->flags |= SFC_EF10_TXQ_NOT_RUNNING;

	*evq_read_ptr = txq->evq_read_ptr;
}

static sfc_dp_tx_qtx_ev_t sfc_ef10_tx_qtx_ev;
static bool
sfc_ef10_tx_qtx_ev(struct sfc_dp_txq *dp_txq, __rte_unused unsigned int id)
{
	__rte_unused struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);

	SFC_ASSERT(txq->flags & SFC_EF10_TXQ_NOT_RUNNING);

	/*
	 * It is safe to ignore Tx event since we reap all mbufs on
	 * queue purge anyway.
	 */

	return false;
}

static sfc_dp_tx_qreap_t sfc_ef10_tx_qreap;
static void
sfc_ef10_tx_qreap(struct sfc_dp_txq *dp_txq)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);
	unsigned int completed;

	for (completed = txq->completed; completed != txq->added; ++completed) {
		struct sfc_ef10_tx_sw_desc *txd;

		txd = &txq->sw_ring[completed & txq->ptr_mask];
		if (txd->mbuf != NULL) {
			rte_pktmbuf_free_seg(txd->mbuf);
			txd->mbuf = NULL;
		}
	}

	txq->flags &= ~SFC_EF10_TXQ_STARTED;
}

static unsigned int
sfc_ef10_tx_qdesc_npending(struct sfc_ef10_txq *txq)
{
	const unsigned int curr_done = txq->completed - 1;
	unsigned int anew_done = curr_done;
	efx_qword_t tx_ev;
	const unsigned int evq_old_read_ptr = txq->evq_read_ptr;

	if (unlikely(txq->flags &
		     (SFC_EF10_TXQ_NOT_RUNNING | SFC_EF10_TXQ_EXCEPTION)))
		return 0;

	while (sfc_ef10_tx_get_event(txq, &tx_ev))
		anew_done = EFX_QWORD_FIELD(tx_ev, ESF_DZ_TX_DESCR_INDX);

	/*
	 * The function does not process events, so return event queue read
	 * pointer to the original position to allow the events that were
	 * read to be processed later
	 */
	txq->evq_read_ptr = evq_old_read_ptr;

	return (anew_done - curr_done) & txq->ptr_mask;
}

static sfc_dp_tx_qdesc_status_t sfc_ef10_tx_qdesc_status;
static int
sfc_ef10_tx_qdesc_status(struct sfc_dp_txq *dp_txq,
			 uint16_t offset)
{
	struct sfc_ef10_txq *txq = sfc_ef10_txq_by_dp_txq(dp_txq);
	unsigned int npending = sfc_ef10_tx_qdesc_npending(txq);

	if (unlikely(offset > txq->ptr_mask))
		return -EINVAL;

	if (unlikely(offset >= txq->max_fill_level))
		return RTE_ETH_TX_DESC_UNAVAIL;

	if (unlikely(offset < npending))
		return RTE_ETH_TX_DESC_FULL;

	return RTE_ETH_TX_DESC_DONE;
}

struct sfc_dp_tx sfc_ef10_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10,
		.type		= SFC_DP_TX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF10,
	},
	.features		= SFC_DP_TX_FEAT_MULTI_PROCESS,
	.dev_offload_capa	= RTE_ETH_TX_OFFLOAD_MULTI_SEGS,
	.queue_offload_capa	= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				  RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				  RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
				  RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				  RTE_ETH_TX_OFFLOAD_TCP_TSO |
				  RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
				  RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO,
	.get_dev_info		= sfc_ef10_get_dev_info,
	.qsize_up_rings		= sfc_ef10_tx_qsize_up_rings,
	.qcreate		= sfc_ef10_tx_qcreate,
	.qdestroy		= sfc_ef10_tx_qdestroy,
	.qstart			= sfc_ef10_tx_qstart,
	.qtx_ev			= sfc_ef10_tx_qtx_ev,
	.qstop			= sfc_ef10_tx_qstop,
	.qreap			= sfc_ef10_tx_qreap,
	.qdesc_status		= sfc_ef10_tx_qdesc_status,
	.pkt_prepare		= sfc_ef10_prepare_pkts,
	.pkt_burst		= sfc_ef10_xmit_pkts,
};

struct sfc_dp_tx sfc_ef10_simple_tx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10_SIMPLE,
		.type		= SFC_DP_TX,
	},
	.features		= SFC_DP_TX_FEAT_MULTI_PROCESS,
	.dev_offload_capa	= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE,
	.queue_offload_capa	= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				  RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				  RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
				  RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM,
	.get_dev_info		= sfc_ef10_get_dev_info,
	.qsize_up_rings		= sfc_ef10_tx_qsize_up_rings,
	.qcreate		= sfc_ef10_tx_qcreate,
	.qdestroy		= sfc_ef10_tx_qdestroy,
	.qstart			= sfc_ef10_tx_qstart,
	.qtx_ev			= sfc_ef10_tx_qtx_ev,
	.qstop			= sfc_ef10_tx_qstop,
	.qreap			= sfc_ef10_tx_qreap,
	.qdesc_status		= sfc_ef10_tx_qdesc_status,
#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
	.pkt_prepare		= sfc_ef10_simple_prepare_pkts,
#endif
	.pkt_burst		= sfc_ef10_simple_xmit_pkts,
};
