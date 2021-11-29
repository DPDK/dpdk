/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

/* EF100 native datapath implementation */

#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_mbuf_ptype.h>
#include <rte_mbuf.h>
#include <rte_io.h>

#include "efx_types.h"
#include "efx_regs_ef100.h"
#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_flow_tunnel.h"
#include "sfc_tweak.h"
#include "sfc_dp_rx.h"
#include "sfc_kvargs.h"
#include "sfc_ef100.h"
#include "sfc_nic_dma_dp.h"


#define sfc_ef100_rx_err(_rxq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF100, ERR, &(_rxq)->dp.dpq, __VA_ARGS__)

#define sfc_ef100_rx_debug(_rxq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF100, DEBUG, &(_rxq)->dp.dpq, \
		   __VA_ARGS__)

/**
 * Maximum number of descriptors/buffers in the Rx ring.
 * It should guarantee that corresponding event queue never overfill.
 * EF10 native datapath uses event queue of the same size as Rx queue.
 * Maximum number of events on datapath can be estimated as number of
 * Rx queue entries (one event per Rx buffer in the worst case) plus
 * Rx error and flush events.
 */
#define SFC_EF100_RXQ_LIMIT(_ndesc) \
	((_ndesc) - 1 /* head must not step on tail */ - \
	 1 /* Rx error */ - 1 /* flush */)

/** Invalid user mark value when the mark should be treated as unset */
#define SFC_EF100_USER_MARK_INVALID	0

struct sfc_ef100_rx_sw_desc {
	struct rte_mbuf			*mbuf;
};

struct sfc_ef100_rxq {
	/* Used on data path */
	unsigned int			flags;
#define SFC_EF100_RXQ_STARTED		0x1
#define SFC_EF100_RXQ_NOT_RUNNING	0x2
#define SFC_EF100_RXQ_EXCEPTION		0x4
#define SFC_EF100_RXQ_RSS_HASH		0x10
#define SFC_EF100_RXQ_USER_MARK		0x20
#define SFC_EF100_RXQ_FLAG_INTR_EN	0x40
#define SFC_EF100_RXQ_INGRESS_MPORT	0x80
#define SFC_EF100_RXQ_USER_FLAG		0x100
#define SFC_EF100_RXQ_NIC_DMA_MAP	0x200
	unsigned int			ptr_mask;
	unsigned int			evq_phase_bit_shift;
	unsigned int			ready_pkts;
	unsigned int			completed;
	unsigned int			evq_read_ptr;
	unsigned int			evq_read_ptr_primed;
	volatile efx_qword_t		*evq_hw_ring;
	struct sfc_ef100_rx_sw_desc	*sw_ring;
	uint64_t			rearm_data;
	uint16_t			buf_size;
	uint16_t			prefix_size;
	uint32_t			user_mark_mask;

	unsigned int			evq_hw_index;
	volatile void			*evq_prime;

	/* Used on refill */
	unsigned int			added;
	unsigned int			max_fill_level;
	unsigned int			refill_threshold;
	struct rte_mempool		*refill_mb_pool;
	efx_qword_t			*rxq_hw_ring;
	volatile void			*doorbell;

	/* Datapath receive queue anchor */
	struct sfc_dp_rxq		dp;

	const struct sfc_nic_dma_info	*nic_dma_info;
};

static inline struct sfc_ef100_rxq *
sfc_ef100_rxq_by_dp_rxq(struct sfc_dp_rxq *dp_rxq)
{
	return container_of(dp_rxq, struct sfc_ef100_rxq, dp);
}

static void
sfc_ef100_rx_qprime(struct sfc_ef100_rxq *rxq)
{
	sfc_ef100_evq_prime(rxq->evq_prime, rxq->evq_hw_index,
			    rxq->evq_read_ptr & rxq->ptr_mask);
	rxq->evq_read_ptr_primed = rxq->evq_read_ptr;
}

static inline void
sfc_ef100_rx_qpush(struct sfc_ef100_rxq *rxq, unsigned int added)
{
	efx_dword_t dword;

	EFX_POPULATE_DWORD_1(dword, ERF_GZ_RX_RING_PIDX, added & rxq->ptr_mask);

	/* DMA sync to device is not required */

	/*
	 * rte_write32() has rte_io_wmb() which guarantees that the STORE
	 * operations (i.e. Rx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_write32(dword.ed_u32[0], rxq->doorbell);
	rxq->dp.dpq.dbells++;

	sfc_ef100_rx_debug(rxq, "RxQ pushed doorbell at pidx %u (added=%u)",
			   EFX_DWORD_FIELD(dword, ERF_GZ_RX_RING_PIDX),
			   added);
}

static void
sfc_ef100_rx_qrefill(struct sfc_ef100_rxq *rxq)
{
	const unsigned int ptr_mask = rxq->ptr_mask;
	unsigned int free_space;
	unsigned int bulks;
	void *objs[SFC_RX_REFILL_BULK];
	unsigned int added = rxq->added;

	free_space = rxq->max_fill_level - (added - rxq->completed);

	if (free_space < rxq->refill_threshold)
		return;

	bulks = free_space / RTE_DIM(objs);
	/* refill_threshold guarantees that bulks is positive */
	SFC_ASSERT(bulks > 0);

	do {
		unsigned int i;

		if (unlikely(rte_mempool_get_bulk(rxq->refill_mb_pool, objs,
						  RTE_DIM(objs)) < 0)) {
			struct rte_eth_dev_data *dev_data =
				rte_eth_devices[rxq->dp.dpq.port_id].data;

			/*
			 * It is hardly a safe way to increment counter
			 * from different contexts, but all PMDs do it.
			 */
			dev_data->rx_mbuf_alloc_failed += RTE_DIM(objs);
			/* Return if we have posted nothing yet */
			if (added == rxq->added)
				return;
			/* Push posted */
			break;
		}

		for (i = 0; i < RTE_DIM(objs); ++i) {
			struct rte_mbuf *m = objs[i];
			struct sfc_ef100_rx_sw_desc *rxd;
			rte_iova_t dma_addr;

			__rte_mbuf_raw_sanity_check(m);

			dma_addr = rte_mbuf_data_iova_default(m);
			if (rxq->flags & SFC_EF100_RXQ_NIC_DMA_MAP) {
				dma_addr = sfc_nic_dma_map(rxq->nic_dma_info,
						dma_addr,
						rte_pktmbuf_data_len(m));
				if (unlikely(dma_addr == RTE_BAD_IOVA)) {
					sfc_ef100_rx_err(rxq,
						"failed to map DMA address on Rx");
					/* Just skip buffer and try to continue */
					rte_mempool_put(rxq->refill_mb_pool, m);
					continue;
				}
			}

			rxd = &rxq->sw_ring[added & ptr_mask];
			rxd->mbuf = m;

			/*
			 * Avoid writing to mbuf. It is cheaper to do it
			 * when we receive packet and fill in nearby
			 * structure members.
			 */

			EFX_POPULATE_QWORD_1(rxq->rxq_hw_ring[added & ptr_mask],
			    ESF_GZ_RX_BUF_ADDR, dma_addr);
			added++;
		}
	} while (--bulks > 0);

	SFC_ASSERT(rxq->added != added);
	rxq->added = added;
	sfc_ef100_rx_qpush(rxq, added);
}

static inline uint64_t
sfc_ef100_rx_nt_or_inner_l4_csum(const efx_word_t class)
{
	return EFX_WORD_FIELD(class,
			      ESF_GZ_RX_PREFIX_HCLASS_NT_OR_INNER_L4_CSUM) ==
		ESE_GZ_RH_HCLASS_L4_CSUM_GOOD ?
		RTE_MBUF_F_RX_L4_CKSUM_GOOD : RTE_MBUF_F_RX_L4_CKSUM_BAD;
}

static inline uint64_t
sfc_ef100_rx_tun_outer_l4_csum(const efx_word_t class)
{
	return EFX_WORD_FIELD(class,
			      ESF_GZ_RX_PREFIX_HCLASS_TUN_OUTER_L4_CSUM) ==
		ESE_GZ_RH_HCLASS_L4_CSUM_GOOD ?
		RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD : RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
}

static uint32_t
sfc_ef100_rx_class_decode(const efx_word_t class, uint64_t *ol_flags)
{
	uint32_t ptype;
	bool no_tunnel = false;

	if (unlikely(EFX_WORD_FIELD(class, ESF_GZ_RX_PREFIX_HCLASS_L2_CLASS) !=
		     ESE_GZ_RH_HCLASS_L2_CLASS_E2_0123VLAN))
		return 0;

	switch (EFX_WORD_FIELD(class, ESF_GZ_RX_PREFIX_HCLASS_L2_N_VLAN)) {
	case 0:
		ptype = RTE_PTYPE_L2_ETHER;
		break;
	case 1:
		ptype = RTE_PTYPE_L2_ETHER_VLAN;
		break;
	default:
		ptype = RTE_PTYPE_L2_ETHER_QINQ;
		break;
	}

	switch (EFX_WORD_FIELD(class, ESF_GZ_RX_PREFIX_HCLASS_TUNNEL_CLASS)) {
	case ESE_GZ_RH_HCLASS_TUNNEL_CLASS_NONE:
		no_tunnel = true;
		break;
	case ESE_GZ_RH_HCLASS_TUNNEL_CLASS_VXLAN:
		ptype |= RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L4_UDP;
		*ol_flags |= sfc_ef100_rx_tun_outer_l4_csum(class);
		break;
	case ESE_GZ_RH_HCLASS_TUNNEL_CLASS_NVGRE:
		ptype |= RTE_PTYPE_TUNNEL_NVGRE;
		break;
	case ESE_GZ_RH_HCLASS_TUNNEL_CLASS_GENEVE:
		ptype |= RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L4_UDP;
		*ol_flags |= sfc_ef100_rx_tun_outer_l4_csum(class);
		break;
	default:
		/*
		 * Driver does not know the tunnel, but it is
		 * still a tunnel and NT_OR_INNER refer to inner
		 * frame.
		 */
		no_tunnel = false;
	}

	if (no_tunnel) {
		bool l4_valid = true;

		switch (EFX_WORD_FIELD(class,
			ESF_GZ_RX_PREFIX_HCLASS_NT_OR_INNER_L3_CLASS)) {
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4GOOD:
			ptype |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
			*ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4BAD:
			ptype |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
			*ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP6:
			ptype |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
			break;
		default:
			l4_valid = false;
		}

		if (l4_valid) {
			switch (EFX_WORD_FIELD(class,
				ESF_GZ_RX_PREFIX_HCLASS_NT_OR_INNER_L4_CLASS)) {
			case ESE_GZ_RH_HCLASS_L4_CLASS_TCP:
				ptype |= RTE_PTYPE_L4_TCP;
				*ol_flags |=
					sfc_ef100_rx_nt_or_inner_l4_csum(class);
				break;
			case ESE_GZ_RH_HCLASS_L4_CLASS_UDP:
				ptype |= RTE_PTYPE_L4_UDP;
				*ol_flags |=
					sfc_ef100_rx_nt_or_inner_l4_csum(class);
				break;
			case ESE_GZ_RH_HCLASS_L4_CLASS_FRAG:
				ptype |= RTE_PTYPE_L4_FRAG;
				break;
			}
		}
	} else {
		bool l4_valid = true;

		switch (EFX_WORD_FIELD(class,
			ESF_GZ_RX_PREFIX_HCLASS_TUN_OUTER_L3_CLASS)) {
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4GOOD:
			ptype |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4BAD:
			ptype |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
			*ol_flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP6:
			ptype |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
			break;
		}

		switch (EFX_WORD_FIELD(class,
			ESF_GZ_RX_PREFIX_HCLASS_NT_OR_INNER_L3_CLASS)) {
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4GOOD:
			ptype |= RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
			*ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP4BAD:
			ptype |= RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
			*ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
			break;
		case ESE_GZ_RH_HCLASS_L3_CLASS_IP6:
			ptype |= RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;
			break;
		default:
			l4_valid = false;
			break;
		}

		if (l4_valid) {
			switch (EFX_WORD_FIELD(class,
				ESF_GZ_RX_PREFIX_HCLASS_NT_OR_INNER_L4_CLASS)) {
			case ESE_GZ_RH_HCLASS_L4_CLASS_TCP:
				ptype |= RTE_PTYPE_INNER_L4_TCP;
				*ol_flags |=
					sfc_ef100_rx_nt_or_inner_l4_csum(class);
				break;
			case ESE_GZ_RH_HCLASS_L4_CLASS_UDP:
				ptype |= RTE_PTYPE_INNER_L4_UDP;
				*ol_flags |=
					sfc_ef100_rx_nt_or_inner_l4_csum(class);
				break;
			case ESE_GZ_RH_HCLASS_L4_CLASS_FRAG:
				ptype |= RTE_PTYPE_INNER_L4_FRAG;
				break;
			}
		}
	}

	return ptype;
}

/*
 * Below function relies on the following fields in Rx prefix.
 * Some fields are mandatory, some fields are optional.
 * See sfc_ef100_rx_qstart() below.
 */
static const efx_rx_prefix_layout_t sfc_ef100_rx_prefix_layout = {
	.erpl_fields	= {
#define	SFC_EF100_RX_PREFIX_FIELD(_name, _big_endian) \
	EFX_RX_PREFIX_FIELD(_name, ESF_GZ_RX_PREFIX_ ## _name, _big_endian)

		SFC_EF100_RX_PREFIX_FIELD(LENGTH, B_FALSE),
		SFC_EF100_RX_PREFIX_FIELD(RSS_HASH_VALID, B_FALSE),
		SFC_EF100_RX_PREFIX_FIELD(CLASS, B_FALSE),
		EFX_RX_PREFIX_FIELD(INGRESS_MPORT,
				    ESF_GZ_RX_PREFIX_INGRESS_MPORT, B_FALSE),
		SFC_EF100_RX_PREFIX_FIELD(RSS_HASH, B_FALSE),
		SFC_EF100_RX_PREFIX_FIELD(USER_FLAG, B_FALSE),
		SFC_EF100_RX_PREFIX_FIELD(USER_MARK, B_FALSE),

#undef	SFC_EF100_RX_PREFIX_FIELD
	}
};

static bool
sfc_ef100_rx_prefix_to_offloads(const struct sfc_ef100_rxq *rxq,
				const efx_xword_t *rx_prefix,
				struct rte_mbuf *m)
{
	const efx_word_t *class;
	uint64_t ol_flags = 0;

	RTE_BUILD_BUG_ON(EFX_LOW_BIT(ESF_GZ_RX_PREFIX_CLASS) % CHAR_BIT != 0);
	RTE_BUILD_BUG_ON(EFX_WIDTH(ESF_GZ_RX_PREFIX_CLASS) % CHAR_BIT != 0);
	RTE_BUILD_BUG_ON(EFX_WIDTH(ESF_GZ_RX_PREFIX_CLASS) / CHAR_BIT !=
			 sizeof(*class));
	class = (const efx_word_t *)((const uint8_t *)rx_prefix +
		EFX_LOW_BIT(ESF_GZ_RX_PREFIX_CLASS) / CHAR_BIT);
	if (unlikely(EFX_WORD_FIELD(*class,
				    ESF_GZ_RX_PREFIX_HCLASS_L2_STATUS) !=
		     ESE_GZ_RH_HCLASS_L2_STATUS_OK))
		return false;

	m->packet_type = sfc_ef100_rx_class_decode(*class, &ol_flags);

	if ((rxq->flags & SFC_EF100_RXQ_RSS_HASH) &&
	    EFX_TEST_XWORD_BIT(rx_prefix[0],
			       ESF_GZ_RX_PREFIX_RSS_HASH_VALID_LBN)) {
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		/* EFX_XWORD_FIELD converts little-endian to CPU */
		m->hash.rss = EFX_XWORD_FIELD(rx_prefix[0],
					      ESF_GZ_RX_PREFIX_RSS_HASH);
	}

	if (rxq->flags & SFC_EF100_RXQ_USER_FLAG) {
		uint32_t user_flag;

		user_flag = EFX_XWORD_FIELD(rx_prefix[0],
					    ESF_GZ_RX_PREFIX_USER_FLAG);
		if (user_flag != 0)
			ol_flags |= RTE_MBUF_F_RX_FDIR;
	}

	if (rxq->flags & SFC_EF100_RXQ_USER_MARK) {
		uint8_t tunnel_mark;
		uint32_t user_mark;
		uint32_t mark;

		/* EFX_XWORD_FIELD converts little-endian to CPU */
		mark = EFX_XWORD_FIELD(rx_prefix[0],
				       ESF_GZ_RX_PREFIX_USER_MARK);

		user_mark = mark & rxq->user_mark_mask;
		if (user_mark != SFC_EF100_USER_MARK_INVALID) {
			ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
			m->hash.fdir.hi = user_mark;
		}

		tunnel_mark = SFC_FT_GET_TUNNEL_MARK(mark);
		if (tunnel_mark != SFC_FT_TUNNEL_MARK_INVALID) {
			sfc_ft_id_t ft_id;

			ft_id = SFC_FT_TUNNEL_MARK_TO_ID(tunnel_mark);

			ol_flags |= sfc_dp_ft_id_valid;
			*RTE_MBUF_DYNFIELD(m, sfc_dp_ft_id_offset,
					   sfc_ft_id_t *) = ft_id;
		}
	}

	if (rxq->flags & SFC_EF100_RXQ_INGRESS_MPORT) {
		ol_flags |= sfc_dp_mport_override;
		*RTE_MBUF_DYNFIELD(m,
			sfc_dp_mport_offset,
			typeof(&((efx_mport_id_t *)0)->id)) =
				EFX_XWORD_FIELD(rx_prefix[0],
						ESF_GZ_RX_PREFIX_INGRESS_MPORT);
	}

	m->ol_flags = ol_flags;
	return true;
}

static const uint8_t *
sfc_ef100_rx_pkt_prefix(const struct rte_mbuf *m)
{
	return (const uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM;
}

static struct rte_mbuf *
sfc_ef100_rx_next_mbuf(struct sfc_ef100_rxq *rxq)
{
	struct rte_mbuf *m;
	unsigned int id;

	/* mbuf associated with current Rx descriptor */
	m = rxq->sw_ring[rxq->completed++ & rxq->ptr_mask].mbuf;

	/* completed is already moved to the next one */
	if (unlikely(rxq->completed == rxq->added))
		goto done;

	/*
	 * Prefetch Rx prefix of the next packet.
	 * Current packet is scattered and the next mbuf is its fragment
	 * it simply prefetches some data - no harm since packet rate
	 * should not be high if scatter is used.
	 */
	id = rxq->completed & rxq->ptr_mask;
	rte_prefetch0(sfc_ef100_rx_pkt_prefix(rxq->sw_ring[id].mbuf));

	if (unlikely(rxq->completed + 1 == rxq->added))
		goto done;

	/*
	 * Prefetch mbuf control structure of the next after next Rx
	 * descriptor.
	 */
	id = (id == rxq->ptr_mask) ? 0 : (id + 1);
	rte_mbuf_prefetch_part1(rxq->sw_ring[id].mbuf);

	/*
	 * If the next time we'll need SW Rx descriptor from the next
	 * cache line, try to make sure that we have it in cache.
	 */
	if ((id & 0x7) == 0x7)
		rte_prefetch0(&rxq->sw_ring[(id + 1) & rxq->ptr_mask]);

done:
	return m;
}

static struct rte_mbuf **
sfc_ef100_rx_process_ready_pkts(struct sfc_ef100_rxq *rxq,
				struct rte_mbuf **rx_pkts,
				struct rte_mbuf ** const rx_pkts_end)
{
	while (rxq->ready_pkts > 0 && rx_pkts != rx_pkts_end) {
		struct rte_mbuf *pkt;
		struct rte_mbuf *lastseg;
		const efx_xword_t *rx_prefix;
		uint16_t pkt_len;
		uint16_t seg_len;
		bool deliver;

		rxq->ready_pkts--;

		pkt = sfc_ef100_rx_next_mbuf(rxq);
		__rte_mbuf_raw_sanity_check(pkt);

		RTE_BUILD_BUG_ON(sizeof(pkt->rearm_data[0]) !=
				 sizeof(rxq->rearm_data));
		pkt->rearm_data[0] = rxq->rearm_data;

		/* data_off already moved past Rx prefix */
		rx_prefix = (const efx_xword_t *)sfc_ef100_rx_pkt_prefix(pkt);

		pkt_len = EFX_XWORD_FIELD(rx_prefix[0],
					  ESF_GZ_RX_PREFIX_LENGTH);
		SFC_ASSERT(pkt_len > 0);
		rte_pktmbuf_pkt_len(pkt) = pkt_len;

		seg_len = RTE_MIN(pkt_len, rxq->buf_size - rxq->prefix_size);
		rte_pktmbuf_data_len(pkt) = seg_len;

		deliver = sfc_ef100_rx_prefix_to_offloads(rxq, rx_prefix, pkt);

		lastseg = pkt;
		while ((pkt_len -= seg_len) > 0) {
			struct rte_mbuf *seg;

			seg = sfc_ef100_rx_next_mbuf(rxq);
			__rte_mbuf_raw_sanity_check(seg);

			seg->data_off = RTE_PKTMBUF_HEADROOM;

			seg_len = RTE_MIN(pkt_len, rxq->buf_size);
			rte_pktmbuf_data_len(seg) = seg_len;
			rte_pktmbuf_pkt_len(seg) = seg_len;

			pkt->nb_segs++;
			lastseg->next = seg;
			lastseg = seg;
		}

		if (likely(deliver)) {
			*rx_pkts++ = pkt;
			sfc_pkts_bytes_add(&rxq->dp.dpq.stats, 1,
					   rte_pktmbuf_pkt_len(pkt));
		} else {
			rte_pktmbuf_free(pkt);
		}
	}

	return rx_pkts;
}

static bool
sfc_ef100_rx_get_event(struct sfc_ef100_rxq *rxq, efx_qword_t *ev)
{
	*ev = rxq->evq_hw_ring[rxq->evq_read_ptr & rxq->ptr_mask];

	if (!sfc_ef100_ev_present(ev,
			(rxq->evq_read_ptr >> rxq->evq_phase_bit_shift) & 1))
		return false;

	if (unlikely(!sfc_ef100_ev_type_is(ev, ESE_GZ_EF100_EV_RX_PKTS))) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling by the control path.
		 */
		rxq->flags |= SFC_EF100_RXQ_EXCEPTION;
		sfc_ef100_rx_err(rxq,
			"RxQ exception at EvQ ptr %u(%#x), event %08x:%08x",
			rxq->evq_read_ptr, rxq->evq_read_ptr & rxq->ptr_mask,
			EFX_QWORD_FIELD(*ev, EFX_DWORD_1),
			EFX_QWORD_FIELD(*ev, EFX_DWORD_0));
		return false;
	}

	sfc_ef100_rx_debug(rxq, "RxQ got event %08x:%08x at %u (%#x)",
			   EFX_QWORD_FIELD(*ev, EFX_DWORD_1),
			   EFX_QWORD_FIELD(*ev, EFX_DWORD_0),
			   rxq->evq_read_ptr,
			   rxq->evq_read_ptr & rxq->ptr_mask);

	rxq->evq_read_ptr++;
	return true;
}

static uint16_t
sfc_ef100_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(rx_queue);
	struct rte_mbuf ** const rx_pkts_end = &rx_pkts[nb_pkts];
	efx_qword_t rx_ev;

	rx_pkts = sfc_ef100_rx_process_ready_pkts(rxq, rx_pkts, rx_pkts_end);

	if (unlikely(rxq->flags &
		     (SFC_EF100_RXQ_NOT_RUNNING | SFC_EF100_RXQ_EXCEPTION)))
		goto done;

	while (rx_pkts != rx_pkts_end && sfc_ef100_rx_get_event(rxq, &rx_ev)) {
		rxq->ready_pkts =
			EFX_QWORD_FIELD(rx_ev, ESF_GZ_EV_RXPKTS_NUM_PKT);
		rx_pkts = sfc_ef100_rx_process_ready_pkts(rxq, rx_pkts,
							  rx_pkts_end);
	}

	/* It is not a problem if we refill in the case of exception */
	sfc_ef100_rx_qrefill(rxq);

	if ((rxq->flags & SFC_EF100_RXQ_FLAG_INTR_EN) &&
	    rxq->evq_read_ptr_primed != rxq->evq_read_ptr)
		sfc_ef100_rx_qprime(rxq);

done:
	return nb_pkts - (rx_pkts_end - rx_pkts);
}

static const uint32_t *
sfc_ef100_supported_ptypes_get(__rte_unused uint32_t tunnel_encaps)
{
	static const uint32_t ef100_native_ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_UNKNOWN
	};

	return ef100_native_ptypes;
}

static sfc_dp_rx_qdesc_npending_t sfc_ef100_rx_qdesc_npending;
static unsigned int
sfc_ef100_rx_qdesc_npending(__rte_unused struct sfc_dp_rxq *dp_rxq)
{
	return 0;
}

static sfc_dp_rx_qdesc_status_t sfc_ef100_rx_qdesc_status;
static int
sfc_ef100_rx_qdesc_status(__rte_unused struct sfc_dp_rxq *dp_rxq,
			  __rte_unused uint16_t offset)
{
	return -ENOTSUP;
}


static sfc_dp_rx_get_dev_info_t sfc_ef100_rx_get_dev_info;
static void
sfc_ef100_rx_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->rx_desc_lim.nb_min = SFC_RX_REFILL_BULK;
	dev_info->rx_desc_lim.nb_align = SFC_RX_REFILL_BULK;
}


static sfc_dp_rx_qsize_up_rings_t sfc_ef100_rx_qsize_up_rings;
static int
sfc_ef100_rx_qsize_up_rings(uint16_t nb_rx_desc,
			   struct sfc_dp_rx_hw_limits *limits,
			   __rte_unused struct rte_mempool *mb_pool,
			   unsigned int *rxq_entries,
			   unsigned int *evq_entries,
			   unsigned int *rxq_max_fill_level)
{
	/*
	 * rte_ethdev API guarantees that the number meets min, max and
	 * alignment requirements.
	 */
	if (nb_rx_desc <= limits->rxq_min_entries)
		*rxq_entries = limits->rxq_min_entries;
	else
		*rxq_entries = rte_align32pow2(nb_rx_desc);

	*evq_entries = *rxq_entries;

	*rxq_max_fill_level = RTE_MIN(nb_rx_desc,
				      SFC_EF100_RXQ_LIMIT(*evq_entries));
	return 0;
}


static uint64_t
sfc_ef100_mk_mbuf_rearm_data(uint16_t port_id, uint16_t prefix_size)
{
	struct rte_mbuf m;

	memset(&m, 0, sizeof(m));

	rte_mbuf_refcnt_set(&m, 1);
	m.data_off = RTE_PKTMBUF_HEADROOM + prefix_size;
	m.nb_segs = 1;
	m.port = port_id;

	/* rearm_data covers structure members filled in above */
	rte_compiler_barrier();
	RTE_BUILD_BUG_ON(sizeof(m.rearm_data[0]) != sizeof(uint64_t));
	return m.rearm_data[0];
}

static sfc_dp_rx_qcreate_t sfc_ef100_rx_qcreate;
static int
sfc_ef100_rx_qcreate(uint16_t port_id, uint16_t queue_id,
		    const struct rte_pci_addr *pci_addr, int socket_id,
		    const struct sfc_dp_rx_qcreate_info *info,
		    struct sfc_dp_rxq **dp_rxqp)
{
	struct sfc_ef100_rxq *rxq;
	int rc;

	rc = EINVAL;
	if (info->rxq_entries != info->evq_entries)
		goto fail_rxq_args;

	rc = ENOMEM;
	rxq = rte_zmalloc_socket("sfc-ef100-rxq", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		goto fail_rxq_alloc;

	sfc_dp_queue_init(&rxq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	rxq->sw_ring = rte_calloc_socket("sfc-ef100-rxq-sw_ring",
					 info->rxq_entries,
					 sizeof(*rxq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL)
		goto fail_desc_alloc;

	rxq->flags |= SFC_EF100_RXQ_NOT_RUNNING;
	rxq->ptr_mask = info->rxq_entries - 1;
	rxq->evq_phase_bit_shift = rte_bsf32(info->evq_entries);
	rxq->evq_hw_ring = info->evq_hw_ring;
	rxq->max_fill_level = info->max_fill_level;
	rxq->refill_threshold = info->refill_threshold;
	rxq->prefix_size = info->prefix_size;

	SFC_ASSERT(info->user_mark_mask != 0);
	rxq->user_mark_mask = info->user_mark_mask;

	rxq->buf_size = info->buf_size;
	rxq->refill_mb_pool = info->refill_mb_pool;
	rxq->rxq_hw_ring = info->rxq_hw_ring;
	rxq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_GZ_RX_RING_DOORBELL_OFST +
			(info->hw_index << info->vi_window_shift);

	rxq->evq_hw_index = info->evq_hw_index;
	rxq->evq_prime = (volatile uint8_t *)info->mem_bar +
			 info->fcw_offset +
			 ER_GZ_EVQ_INT_PRIME_OFST;

	rxq->nic_dma_info = info->nic_dma_info;
	if (rxq->nic_dma_info->nb_regions > 0)
		rxq->flags |= SFC_EF100_RXQ_NIC_DMA_MAP;

	sfc_ef100_rx_debug(rxq, "RxQ doorbell is %p", rxq->doorbell);

	*dp_rxqp = &rxq->dp;
	return 0;

fail_desc_alloc:
	rte_free(rxq);

fail_rxq_alloc:
fail_rxq_args:
	return rc;
}

static sfc_dp_rx_qdestroy_t sfc_ef100_rx_qdestroy;
static void
sfc_ef100_rx_qdestroy(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	rte_free(rxq->sw_ring);
	rte_free(rxq);
}

static sfc_dp_rx_qstart_t sfc_ef100_rx_qstart;
static int
sfc_ef100_rx_qstart(struct sfc_dp_rxq *dp_rxq, unsigned int evq_read_ptr,
		    const efx_rx_prefix_layout_t *pinfo)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);
	uint32_t unsup_rx_prefix_fields;

	SFC_ASSERT(rxq->completed == 0);
	SFC_ASSERT(rxq->added == 0);

	/* Prefix must fit into reserved Rx buffer space */
	if (pinfo->erpl_length > rxq->prefix_size)
		return ENOTSUP;

	unsup_rx_prefix_fields =
		efx_rx_prefix_layout_check(pinfo, &sfc_ef100_rx_prefix_layout);

	/* LENGTH and CLASS fields must always be present */
	if ((unsup_rx_prefix_fields &
	     ((1U << EFX_RX_PREFIX_FIELD_LENGTH) |
	      (1U << EFX_RX_PREFIX_FIELD_CLASS))) != 0)
		return ENOTSUP;

	if ((unsup_rx_prefix_fields &
	     ((1U << EFX_RX_PREFIX_FIELD_RSS_HASH_VALID) |
	      (1U << EFX_RX_PREFIX_FIELD_RSS_HASH))) == 0)
		rxq->flags |= SFC_EF100_RXQ_RSS_HASH;
	else
		rxq->flags &= ~SFC_EF100_RXQ_RSS_HASH;

	if ((unsup_rx_prefix_fields &
	     (1U << EFX_RX_PREFIX_FIELD_USER_FLAG)) == 0)
		rxq->flags |= SFC_EF100_RXQ_USER_FLAG;
	else
		rxq->flags &= ~SFC_EF100_RXQ_USER_FLAG;

	if ((unsup_rx_prefix_fields &
	     (1U << EFX_RX_PREFIX_FIELD_USER_MARK)) == 0)
		rxq->flags |= SFC_EF100_RXQ_USER_MARK;
	else
		rxq->flags &= ~SFC_EF100_RXQ_USER_MARK;

	if ((unsup_rx_prefix_fields &
	     (1U << EFX_RX_PREFIX_FIELD_INGRESS_MPORT)) == 0)
		rxq->flags |= SFC_EF100_RXQ_INGRESS_MPORT;
	else
		rxq->flags &= ~SFC_EF100_RXQ_INGRESS_MPORT;

	rxq->prefix_size = pinfo->erpl_length;
	rxq->rearm_data = sfc_ef100_mk_mbuf_rearm_data(rxq->dp.dpq.port_id,
						       rxq->prefix_size);

	sfc_ef100_rx_qrefill(rxq);

	rxq->evq_read_ptr = evq_read_ptr;

	rxq->flags |= SFC_EF100_RXQ_STARTED;
	rxq->flags &= ~(SFC_EF100_RXQ_NOT_RUNNING | SFC_EF100_RXQ_EXCEPTION);

	if (rxq->flags & SFC_EF100_RXQ_FLAG_INTR_EN)
		sfc_ef100_rx_qprime(rxq);

	return 0;
}

static sfc_dp_rx_qstop_t sfc_ef100_rx_qstop;
static void
sfc_ef100_rx_qstop(struct sfc_dp_rxq *dp_rxq, unsigned int *evq_read_ptr)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	rxq->flags |= SFC_EF100_RXQ_NOT_RUNNING;

	*evq_read_ptr = rxq->evq_read_ptr;
}

static sfc_dp_rx_qrx_ev_t sfc_ef100_rx_qrx_ev;
static bool
sfc_ef100_rx_qrx_ev(struct sfc_dp_rxq *dp_rxq, __rte_unused unsigned int id)
{
	__rte_unused struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	SFC_ASSERT(rxq->flags & SFC_EF100_RXQ_NOT_RUNNING);

	/*
	 * It is safe to ignore Rx event since we free all mbufs on
	 * queue purge anyway.
	 */

	return false;
}

static sfc_dp_rx_qpurge_t sfc_ef100_rx_qpurge;
static void
sfc_ef100_rx_qpurge(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);
	unsigned int i;
	struct sfc_ef100_rx_sw_desc *rxd;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_ring[i & rxq->ptr_mask];
		rte_mbuf_raw_free(rxd->mbuf);
		rxd->mbuf = NULL;
	}

	rxq->completed = rxq->added = 0;
	rxq->ready_pkts = 0;

	rxq->flags &= ~SFC_EF100_RXQ_STARTED;
}

static sfc_dp_rx_intr_enable_t sfc_ef100_rx_intr_enable;
static int
sfc_ef100_rx_intr_enable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	rxq->flags |= SFC_EF100_RXQ_FLAG_INTR_EN;
	if (rxq->flags & SFC_EF100_RXQ_STARTED)
		sfc_ef100_rx_qprime(rxq);
	return 0;
}

static sfc_dp_rx_intr_disable_t sfc_ef100_rx_intr_disable;
static int
sfc_ef100_rx_intr_disable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	/* Cannot disarm, just disable rearm */
	rxq->flags &= ~SFC_EF100_RXQ_FLAG_INTR_EN;
	return 0;
}

static sfc_dp_rx_get_pushed_t sfc_ef100_rx_get_pushed;
static unsigned int
sfc_ef100_rx_get_pushed(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef100_rxq *rxq = sfc_ef100_rxq_by_dp_rxq(dp_rxq);

	/*
	 * The datapath keeps track only of added descriptors, since
	 * the number of pushed descriptors always equals the number
	 * of added descriptors due to enforced alignment.
	 */
	return rxq->added;
}

struct sfc_dp_rx sfc_ef100_rx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF100,
		.type		= SFC_DP_RX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF100,
	},
	.features		= SFC_DP_RX_FEAT_MULTI_PROCESS |
				  SFC_DP_RX_FEAT_FLOW_FLAG |
				  SFC_DP_RX_FEAT_FLOW_MARK |
				  SFC_DP_RX_FEAT_INTR |
				  SFC_DP_RX_FEAT_STATS,
	.dev_offload_capa	= 0,
	.queue_offload_capa	= RTE_ETH_RX_OFFLOAD_CHECKSUM |
				  RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
				  RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM |
				  RTE_ETH_RX_OFFLOAD_SCATTER |
				  RTE_ETH_RX_OFFLOAD_RSS_HASH,
	.get_dev_info		= sfc_ef100_rx_get_dev_info,
	.qsize_up_rings		= sfc_ef100_rx_qsize_up_rings,
	.qcreate		= sfc_ef100_rx_qcreate,
	.qdestroy		= sfc_ef100_rx_qdestroy,
	.qstart			= sfc_ef100_rx_qstart,
	.qstop			= sfc_ef100_rx_qstop,
	.qrx_ev			= sfc_ef100_rx_qrx_ev,
	.qpurge			= sfc_ef100_rx_qpurge,
	.supported_ptypes_get	= sfc_ef100_supported_ptypes_get,
	.qdesc_npending		= sfc_ef100_rx_qdesc_npending,
	.qdesc_status		= sfc_ef100_rx_qdesc_status,
	.intr_enable		= sfc_ef100_rx_intr_enable,
	.intr_disable		= sfc_ef100_rx_intr_disable,
	.get_pushed		= sfc_ef100_rx_get_pushed,
	.pkt_burst		= sfc_ef100_recv_pkts,
};
