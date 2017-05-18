/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* EF10 native datapath implementation */

#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_mbuf_ptype.h>
#include <rte_mbuf.h>
#include <rte_io.h>

#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"

#include "sfc_tweak.h"
#include "sfc_dp_rx.h"
#include "sfc_kvargs.h"
#include "sfc_ef10.h"

#define sfc_ef10_rx_err(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, ERR, dpq, __VA_ARGS__)

/**
 * Alignment requirement for value written to RX WPTR:
 * the WPTR must be aligned to an 8 descriptor boundary.
 */
#define SFC_EF10_RX_WPTR_ALIGN	8

/**
 * Maximum number of descriptors/buffers in the Rx ring.
 * It should guarantee that corresponding event queue never overfill.
 * EF10 native datapath uses event queue of the same size as Rx queue.
 * Maximum number of events on datapath can be estimated as number of
 * Rx queue entries (one event per Rx buffer in the worst case) plus
 * Rx error and flush events.
 */
#define SFC_EF10_RXQ_LIMIT(_ndesc) \
	((_ndesc) - 1 /* head must not step on tail */ - \
	 (SFC_EF10_EV_PER_CACHE_LINE - 1) /* max unused EvQ entries */ - \
	 1 /* Rx error */ - 1 /* flush */)

struct sfc_ef10_rx_sw_desc {
	struct rte_mbuf			*mbuf;
};

struct sfc_ef10_rxq {
	/* Used on data path */
	unsigned int			flags;
#define SFC_EF10_RXQ_STARTED		0x1
#define SFC_EF10_RXQ_NOT_RUNNING	0x2
#define SFC_EF10_RXQ_EXCEPTION		0x4
#define SFC_EF10_RXQ_RSS_HASH		0x8
	unsigned int			ptr_mask;
	unsigned int			prepared;
	unsigned int			completed;
	unsigned int			evq_read_ptr;
	efx_qword_t			*evq_hw_ring;
	struct sfc_ef10_rx_sw_desc	*sw_ring;
	uint64_t			rearm_data;
	uint16_t			prefix_size;

	/* Used on refill */
	uint16_t			buf_size;
	unsigned int			added;
	unsigned int			refill_threshold;
	struct rte_mempool		*refill_mb_pool;
	efx_qword_t			*rxq_hw_ring;
	volatile void			*doorbell;

	/* Datapath receive queue anchor */
	struct sfc_dp_rxq		dp;
};

static inline struct sfc_ef10_rxq *
sfc_ef10_rxq_by_dp_rxq(struct sfc_dp_rxq *dp_rxq)
{
	return container_of(dp_rxq, struct sfc_ef10_rxq, dp);
}

static void
sfc_ef10_rx_qpush(struct sfc_ef10_rxq *rxq)
{
	efx_dword_t dword;

	/* Hardware has alignment restriction for WPTR */
	RTE_BUILD_BUG_ON(SFC_RX_REFILL_BULK % SFC_EF10_RX_WPTR_ALIGN != 0);
	SFC_ASSERT(RTE_ALIGN(rxq->added, SFC_EF10_RX_WPTR_ALIGN) == rxq->added);

	EFX_POPULATE_DWORD_1(dword, ERF_DZ_RX_DESC_WPTR,
			     rxq->added & rxq->ptr_mask);

	/* DMA sync to device is not required */

	/*
	 * rte_write32() has rte_io_wmb() which guarantees that the STORE
	 * operations (i.e. Rx and event descriptor updates) that precede
	 * the rte_io_wmb() call are visible to NIC before the STORE
	 * operations that follow it (i.e. doorbell write).
	 */
	rte_write32(dword.ed_u32[0], rxq->doorbell);
}

static void
sfc_ef10_rx_qrefill(struct sfc_ef10_rxq *rxq)
{
	const unsigned int ptr_mask = rxq->ptr_mask;
	const uint32_t buf_size = rxq->buf_size;
	unsigned int free_space;
	unsigned int bulks;
	void *objs[SFC_RX_REFILL_BULK];
	unsigned int added = rxq->added;

	free_space = SFC_EF10_RXQ_LIMIT(ptr_mask + 1) -
		(added - rxq->completed);

	if (free_space < rxq->refill_threshold)
		return;

	bulks = free_space / RTE_DIM(objs);
	/* refill_threshold guarantees that bulks is positive */
	SFC_ASSERT(bulks > 0);

	do {
		unsigned int id;
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

		for (i = 0, id = added & ptr_mask;
		     i < RTE_DIM(objs);
		     ++i, ++id) {
			struct rte_mbuf *m = objs[i];
			struct sfc_ef10_rx_sw_desc *rxd;
			phys_addr_t phys_addr;

			SFC_ASSERT((id & ~ptr_mask) == 0);
			rxd = &rxq->sw_ring[id];
			rxd->mbuf = m;

			/*
			 * Avoid writing to mbuf. It is cheaper to do it
			 * when we receive packet and fill in nearby
			 * structure members.
			 */

			phys_addr = rte_mbuf_data_dma_addr_default(m);
			EFX_POPULATE_QWORD_2(rxq->rxq_hw_ring[id],
			    ESF_DZ_RX_KER_BYTE_CNT, buf_size,
			    ESF_DZ_RX_KER_BUF_ADDR, phys_addr);
		}

		added += RTE_DIM(objs);
	} while (--bulks > 0);

	SFC_ASSERT(rxq->added != added);
	rxq->added = added;
	sfc_ef10_rx_qpush(rxq);
}

static void
sfc_ef10_rx_prefetch_next(struct sfc_ef10_rxq *rxq, unsigned int next_id)
{
	struct rte_mbuf *next_mbuf;

	/* Prefetch next bunch of software descriptors */
	if ((next_id % (RTE_CACHE_LINE_SIZE / sizeof(rxq->sw_ring[0]))) == 0)
		rte_prefetch0(&rxq->sw_ring[next_id]);

	/*
	 * It looks strange to prefetch depending on previous prefetch
	 * data, but measurements show that it is really efficient and
	 * increases packet rate.
	 */
	next_mbuf = rxq->sw_ring[next_id].mbuf;
	if (likely(next_mbuf != NULL)) {
		/* Prefetch the next mbuf structure */
		rte_mbuf_prefetch_part1(next_mbuf);

		/* Prefetch pseudo header of the next packet */
		/* data_off is not filled in yet */
		/* Yes, data could be not ready yet, but we hope */
		rte_prefetch0((uint8_t *)next_mbuf->buf_addr +
			      RTE_PKTMBUF_HEADROOM);
	}
}

static uint16_t
sfc_ef10_rx_prepared(struct sfc_ef10_rxq *rxq, struct rte_mbuf **rx_pkts,
		     uint16_t nb_pkts)
{
	uint16_t n_rx_pkts = RTE_MIN(nb_pkts, rxq->prepared);
	unsigned int completed = rxq->completed;
	unsigned int i;

	rxq->prepared -= n_rx_pkts;
	rxq->completed = completed + n_rx_pkts;

	for (i = 0; i < n_rx_pkts; ++i, ++completed)
		rx_pkts[i] = rxq->sw_ring[completed & rxq->ptr_mask].mbuf;

	return n_rx_pkts;
}

static void
sfc_ef10_rx_ev_to_offloads(struct sfc_ef10_rxq *rxq, const efx_qword_t rx_ev,
			   struct rte_mbuf *m)
{
	uint32_t l2_ptype = 0;
	uint32_t l3_ptype = 0;
	uint32_t l4_ptype = 0;
	uint64_t ol_flags = 0;

	if (unlikely(EFX_TEST_QWORD_BIT(rx_ev, ESF_DZ_RX_PARSE_INCOMPLETE_LBN)))
		goto done;

	switch (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_ETH_TAG_CLASS)) {
	case ESE_DZ_ETH_TAG_CLASS_NONE:
		l2_ptype = RTE_PTYPE_L2_ETHER;
		break;
	case ESE_DZ_ETH_TAG_CLASS_VLAN1:
		l2_ptype = RTE_PTYPE_L2_ETHER_VLAN;
		break;
	case ESE_DZ_ETH_TAG_CLASS_VLAN2:
		l2_ptype = RTE_PTYPE_L2_ETHER_QINQ;
		break;
	default:
		/* Unexpected Eth tag class */
		SFC_ASSERT(false);
	}

	switch (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_L3_CLASS)) {
	case ESE_DZ_L3_CLASS_IP4_FRAG:
		l4_ptype = RTE_PTYPE_L4_FRAG;
		/* FALLTHROUGH */
	case ESE_DZ_L3_CLASS_IP4:
		l3_ptype = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
		ol_flags |= PKT_RX_RSS_HASH |
			((EFX_TEST_QWORD_BIT(rx_ev,
					     ESF_DZ_RX_IPCKSUM_ERR_LBN)) ?
			 PKT_RX_IP_CKSUM_BAD : PKT_RX_IP_CKSUM_GOOD);
		break;
	case ESE_DZ_L3_CLASS_IP6_FRAG:
		l4_ptype |= RTE_PTYPE_L4_FRAG;
		/* FALLTHROUGH */
	case ESE_DZ_L3_CLASS_IP6:
		l3_ptype |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
		ol_flags |= PKT_RX_RSS_HASH;
		break;
	case ESE_DZ_L3_CLASS_ARP:
		/* Override Layer 2 packet type */
		l2_ptype = RTE_PTYPE_L2_ETHER_ARP;
		break;
	default:
		/* Unexpected Layer 3 class */
		SFC_ASSERT(false);
	}

	switch (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_L4_CLASS)) {
	case ESE_DZ_L4_CLASS_TCP:
		l4_ptype = RTE_PTYPE_L4_TCP;
		ol_flags |=
			(EFX_TEST_QWORD_BIT(rx_ev,
					    ESF_DZ_RX_TCPUDP_CKSUM_ERR_LBN)) ?
			PKT_RX_L4_CKSUM_BAD : PKT_RX_L4_CKSUM_GOOD;
		break;
	case ESE_DZ_L4_CLASS_UDP:
		l4_ptype = RTE_PTYPE_L4_UDP;
		ol_flags |=
			(EFX_TEST_QWORD_BIT(rx_ev,
					    ESF_DZ_RX_TCPUDP_CKSUM_ERR_LBN)) ?
			PKT_RX_L4_CKSUM_BAD : PKT_RX_L4_CKSUM_GOOD;
		break;
	case ESE_DZ_L4_CLASS_UNKNOWN:
		break;
	default:
		/* Unexpected Layer 4 class */
		SFC_ASSERT(false);
	}

	/* Remove RSS hash offload flag if RSS is not enabled */
	if (~rxq->flags & SFC_EF10_RXQ_RSS_HASH)
		ol_flags &= ~PKT_RX_RSS_HASH;

done:
	m->ol_flags = ol_flags;
	m->packet_type = l2_ptype | l3_ptype | l4_ptype;
}

static uint16_t
sfc_ef10_rx_pseudo_hdr_get_len(const uint8_t *pseudo_hdr)
{
	return rte_le_to_cpu_16(*(const uint16_t *)&pseudo_hdr[8]);
}

static uint32_t
sfc_ef10_rx_pseudo_hdr_get_hash(const uint8_t *pseudo_hdr)
{
	return rte_le_to_cpu_32(*(const uint32_t *)pseudo_hdr);
}

static uint16_t
sfc_ef10_rx_process_event(struct sfc_ef10_rxq *rxq, efx_qword_t rx_ev,
			  struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	const unsigned int ptr_mask = rxq->ptr_mask;
	unsigned int completed = rxq->completed;
	unsigned int ready;
	struct sfc_ef10_rx_sw_desc *rxd;
	struct rte_mbuf *m;
	struct rte_mbuf *m0;
	uint16_t n_rx_pkts;
	const uint8_t *pseudo_hdr;
	uint16_t pkt_len;

	ready = (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_DSC_PTR_LBITS) - completed) &
		EFX_MASK32(ESF_DZ_RX_DSC_PTR_LBITS);
	SFC_ASSERT(ready > 0);

	if (rx_ev.eq_u64[0] &
	    rte_cpu_to_le_64((1ull << ESF_DZ_RX_ECC_ERR_LBN) |
			     (1ull << ESF_DZ_RX_ECRC_ERR_LBN))) {
		SFC_ASSERT(rxq->prepared == 0);
		rxq->completed += ready;
		while (ready-- > 0) {
			rxd = &rxq->sw_ring[completed++ & ptr_mask];
			rte_mempool_put(rxq->refill_mb_pool, rxd->mbuf);
		}
		return 0;
	}

	n_rx_pkts = RTE_MIN(ready, nb_pkts);
	rxq->prepared = ready - n_rx_pkts;
	rxq->completed += n_rx_pkts;

	rxd = &rxq->sw_ring[completed++ & ptr_mask];

	sfc_ef10_rx_prefetch_next(rxq, completed & ptr_mask);

	m = rxd->mbuf;

	*rx_pkts++ = m;

	RTE_BUILD_BUG_ON(sizeof(m->rearm_data[0]) != sizeof(rxq->rearm_data));
	m->rearm_data[0] = rxq->rearm_data;

	/* Classify packet based on Rx event */
	sfc_ef10_rx_ev_to_offloads(rxq, rx_ev, m);

	/* data_off already moved past pseudo header */
	pseudo_hdr = (uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM;

	/*
	 * Always get RSS hash from pseudo header to avoid
	 * condition/branching. If it is valid or not depends on
	 * PKT_RX_RSS_HASH in m->ol_flags.
	 */
	m->hash.rss = sfc_ef10_rx_pseudo_hdr_get_hash(pseudo_hdr);

	if (ready == 1)
		pkt_len = EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_BYTES) -
			rxq->prefix_size;
	else
		pkt_len = sfc_ef10_rx_pseudo_hdr_get_len(pseudo_hdr);
	SFC_ASSERT(pkt_len > 0);
	rte_pktmbuf_data_len(m) = pkt_len;
	rte_pktmbuf_pkt_len(m) = pkt_len;

	SFC_ASSERT(m->next == NULL);

	/* Remember mbuf to copy offload flags and packet type from */
	m0 = m;
	for (--ready; ready > 0; --ready) {
		rxd = &rxq->sw_ring[completed++ & ptr_mask];

		sfc_ef10_rx_prefetch_next(rxq, completed & ptr_mask);

		m = rxd->mbuf;

		if (ready > rxq->prepared)
			*rx_pkts++ = m;

		RTE_BUILD_BUG_ON(sizeof(m->rearm_data[0]) !=
				 sizeof(rxq->rearm_data));
		m->rearm_data[0] = rxq->rearm_data;

		/* Event-dependent information is the same */
		m->ol_flags = m0->ol_flags;
		m->packet_type = m0->packet_type;

		/* data_off already moved past pseudo header */
		pseudo_hdr = (uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM;

		/*
		 * Always get RSS hash from pseudo header to avoid
		 * condition/branching. If it is valid or not depends on
		 * PKT_RX_RSS_HASH in m->ol_flags.
		 */
		m->hash.rss = sfc_ef10_rx_pseudo_hdr_get_hash(pseudo_hdr);

		pkt_len = sfc_ef10_rx_pseudo_hdr_get_len(pseudo_hdr);
		SFC_ASSERT(pkt_len > 0);
		rte_pktmbuf_data_len(m) = pkt_len;
		rte_pktmbuf_pkt_len(m) = pkt_len;

		SFC_ASSERT(m->next == NULL);
	}

	return n_rx_pkts;
}

static bool
sfc_ef10_rx_get_event(struct sfc_ef10_rxq *rxq, efx_qword_t *rx_ev)
{
	*rx_ev = rxq->evq_hw_ring[rxq->evq_read_ptr & rxq->ptr_mask];

	if (!sfc_ef10_ev_present(*rx_ev))
		return false;

	if (unlikely(EFX_QWORD_FIELD(*rx_ev, FSF_AZ_EV_CODE) !=
		     FSE_AZ_EV_CODE_RX_EV)) {
		/*
		 * Do not move read_ptr to keep the event for exception
		 * handling by the control path.
		 */
		rxq->flags |= SFC_EF10_RXQ_EXCEPTION;
		sfc_ef10_rx_err(&rxq->dp.dpq,
				"RxQ exception at EvQ read ptr %#x",
				rxq->evq_read_ptr);
		return false;
	}

	rxq->evq_read_ptr++;
	return true;
}

static uint16_t
sfc_ef10_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(rx_queue);
	unsigned int evq_old_read_ptr;
	uint16_t n_rx_pkts;
	efx_qword_t rx_ev;

	if (unlikely(rxq->flags &
		     (SFC_EF10_RXQ_NOT_RUNNING | SFC_EF10_RXQ_EXCEPTION)))
		return 0;

	n_rx_pkts = sfc_ef10_rx_prepared(rxq, rx_pkts, nb_pkts);

	evq_old_read_ptr = rxq->evq_read_ptr;
	while (n_rx_pkts != nb_pkts && sfc_ef10_rx_get_event(rxq, &rx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */

		n_rx_pkts += sfc_ef10_rx_process_event(rxq, rx_ev,
						       rx_pkts + n_rx_pkts,
						       nb_pkts - n_rx_pkts);
	}

	sfc_ef10_ev_qclear(rxq->evq_hw_ring, rxq->ptr_mask, evq_old_read_ptr,
			   rxq->evq_read_ptr);

	/* It is not a problem if we refill in the case of exception */
	sfc_ef10_rx_qrefill(rxq);

	return n_rx_pkts;
}

static const uint32_t *
sfc_ef10_supported_ptypes_get(void)
{
	static const uint32_t ef10_native_ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ef10_native_ptypes;
}

static sfc_dp_rx_qdesc_npending_t sfc_ef10_rx_qdesc_npending;
static unsigned int
sfc_ef10_rx_qdesc_npending(__rte_unused struct sfc_dp_rxq *dp_rxq)
{
	/*
	 * Correct implementation requires EvQ polling and events
	 * processing (keeping all ready mbufs in prepared).
	 */
	return -ENOTSUP;
}


static uint64_t
sfc_ef10_mk_mbuf_rearm_data(uint16_t port_id, uint16_t prefix_size)
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

static sfc_dp_rx_qcreate_t sfc_ef10_rx_qcreate;
static int
sfc_ef10_rx_qcreate(uint16_t port_id, uint16_t queue_id,
		    const struct rte_pci_addr *pci_addr, int socket_id,
		    const struct sfc_dp_rx_qcreate_info *info,
		    struct sfc_dp_rxq **dp_rxqp)
{
	struct sfc_ef10_rxq *rxq;
	int rc;

	rc = EINVAL;
	if (info->rxq_entries != info->evq_entries)
		goto fail_rxq_args;

	rc = ENOMEM;
	rxq = rte_zmalloc_socket("sfc-ef10-rxq", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		goto fail_rxq_alloc;

	sfc_dp_queue_init(&rxq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	rxq->sw_ring = rte_calloc_socket("sfc-ef10-rxq-sw_ring",
					 info->rxq_entries,
					 sizeof(*rxq->sw_ring),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL)
		goto fail_desc_alloc;

	rxq->flags |= SFC_EF10_RXQ_NOT_RUNNING;
	if (info->flags & SFC_RXQ_FLAG_RSS_HASH)
		rxq->flags |= SFC_EF10_RXQ_RSS_HASH;
	rxq->ptr_mask = info->rxq_entries - 1;
	rxq->evq_hw_ring = info->evq_hw_ring;
	rxq->refill_threshold = info->refill_threshold;
	rxq->rearm_data =
		sfc_ef10_mk_mbuf_rearm_data(port_id, info->prefix_size);
	rxq->prefix_size = info->prefix_size;
	rxq->buf_size = info->buf_size;
	rxq->refill_mb_pool = info->refill_mb_pool;
	rxq->rxq_hw_ring = info->rxq_hw_ring;
	rxq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_DZ_RX_DESC_UPD_REG_OFST +
			info->hw_index * ER_DZ_RX_DESC_UPD_REG_STEP;

	*dp_rxqp = &rxq->dp;
	return 0;

fail_desc_alloc:
	rte_free(rxq);

fail_rxq_alloc:
fail_rxq_args:
	return rc;
}

static sfc_dp_rx_qdestroy_t sfc_ef10_rx_qdestroy;
static void
sfc_ef10_rx_qdestroy(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	rte_free(rxq->sw_ring);
	rte_free(rxq);
}

static sfc_dp_rx_qstart_t sfc_ef10_rx_qstart;
static int
sfc_ef10_rx_qstart(struct sfc_dp_rxq *dp_rxq, unsigned int evq_read_ptr)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	rxq->prepared = 0;
	rxq->completed = rxq->added = 0;

	sfc_ef10_rx_qrefill(rxq);

	rxq->evq_read_ptr = evq_read_ptr;

	rxq->flags |= SFC_EF10_RXQ_STARTED;
	rxq->flags &= ~(SFC_EF10_RXQ_NOT_RUNNING | SFC_EF10_RXQ_EXCEPTION);

	return 0;
}

static sfc_dp_rx_qstop_t sfc_ef10_rx_qstop;
static void
sfc_ef10_rx_qstop(struct sfc_dp_rxq *dp_rxq, unsigned int *evq_read_ptr)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	rxq->flags |= SFC_EF10_RXQ_NOT_RUNNING;

	*evq_read_ptr = rxq->evq_read_ptr;
}

static sfc_dp_rx_qrx_ev_t sfc_ef10_rx_qrx_ev;
static bool
sfc_ef10_rx_qrx_ev(struct sfc_dp_rxq *dp_rxq, __rte_unused unsigned int id)
{
	__rte_unused struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	SFC_ASSERT(rxq->flags & SFC_EF10_RXQ_NOT_RUNNING);

	/*
	 * It is safe to ignore Rx event since we free all mbufs on
	 * queue purge anyway.
	 */

	return false;
}

static sfc_dp_rx_qpurge_t sfc_ef10_rx_qpurge;
static void
sfc_ef10_rx_qpurge(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);
	unsigned int i;
	struct sfc_ef10_rx_sw_desc *rxd;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_ring[i & rxq->ptr_mask];
		rte_mempool_put(rxq->refill_mb_pool, rxd->mbuf);
		rxd->mbuf = NULL;
	}

	rxq->flags &= ~SFC_EF10_RXQ_STARTED;
}

struct sfc_dp_rx sfc_ef10_rx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10,
		.type		= SFC_DP_RX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF10,
	},
	.features		= SFC_DP_RX_FEAT_MULTI_PROCESS,
	.qcreate		= sfc_ef10_rx_qcreate,
	.qdestroy		= sfc_ef10_rx_qdestroy,
	.qstart			= sfc_ef10_rx_qstart,
	.qstop			= sfc_ef10_rx_qstop,
	.qrx_ev			= sfc_ef10_rx_qrx_ev,
	.qpurge			= sfc_ef10_rx_qpurge,
	.supported_ptypes_get	= sfc_ef10_supported_ptypes_get,
	.qdesc_npending		= sfc_ef10_rx_qdesc_npending,
	.pkt_burst		= sfc_ef10_recv_pkts,
};
