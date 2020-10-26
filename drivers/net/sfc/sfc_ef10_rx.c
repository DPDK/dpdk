/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
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

#include "sfc_debug.h"
#include "sfc_tweak.h"
#include "sfc_dp_rx.h"
#include "sfc_kvargs.h"
#include "sfc_ef10.h"

#define SFC_EF10_RX_EV_ENCAP_SUPPORT	1
#include "sfc_ef10_rx_ev.h"

#define sfc_ef10_rx_err(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, ERR, dpq, __VA_ARGS__)

#define sfc_ef10_rx_info(dpq, ...) \
	SFC_DP_LOG(SFC_KVARG_DATAPATH_EF10, INFO, dpq, __VA_ARGS__)

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
#define SFC_EF10_RXQ_FLAG_INTR_EN	0x10
	unsigned int			ptr_mask;
	unsigned int			pending;
	unsigned int			completed;
	unsigned int			evq_read_ptr;
	unsigned int			evq_read_ptr_primed;
	efx_qword_t			*evq_hw_ring;
	struct sfc_ef10_rx_sw_desc	*sw_ring;
	uint64_t			rearm_data;
	struct rte_mbuf			*scatter_pkt;
	volatile void			*evq_prime;
	uint16_t			prefix_size;

	/* Used on refill */
	uint16_t			buf_size;
	unsigned int			added;
	unsigned int			max_fill_level;
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
sfc_ef10_rx_qprime(struct sfc_ef10_rxq *rxq)
{
	sfc_ef10_ev_qprime(rxq->evq_prime, rxq->evq_read_ptr, rxq->ptr_mask);
	rxq->evq_read_ptr_primed = rxq->evq_read_ptr;
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

	RTE_BUILD_BUG_ON(SFC_RX_REFILL_BULK % SFC_EF10_RX_WPTR_ALIGN != 0);

	free_space = rxq->max_fill_level - (added - rxq->completed);

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
			rte_iova_t phys_addr;

			__rte_mbuf_raw_sanity_check(m);

			SFC_ASSERT((id & ~ptr_mask) == 0);
			rxd = &rxq->sw_ring[id];
			rxd->mbuf = m;

			/*
			 * Avoid writing to mbuf. It is cheaper to do it
			 * when we receive packet and fill in nearby
			 * structure members.
			 */

			phys_addr = rte_mbuf_data_iova_default(m);
			EFX_POPULATE_QWORD_2(rxq->rxq_hw_ring[id],
			    ESF_DZ_RX_KER_BYTE_CNT, buf_size,
			    ESF_DZ_RX_KER_BUF_ADDR, phys_addr);
		}

		added += RTE_DIM(objs);
	} while (--bulks > 0);

	SFC_ASSERT(rxq->added != added);
	rxq->added = added;
	sfc_ef10_rx_qpush(rxq->doorbell, added, ptr_mask);
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

static struct rte_mbuf **
sfc_ef10_rx_pending(struct sfc_ef10_rxq *rxq, struct rte_mbuf **rx_pkts,
		    uint16_t nb_pkts)
{
	uint16_t n_rx_pkts = RTE_MIN(nb_pkts, rxq->pending - rxq->completed);

	SFC_ASSERT(rxq->pending == rxq->completed || rxq->scatter_pkt == NULL);

	if (n_rx_pkts != 0) {
		unsigned int completed = rxq->completed;

		rxq->completed = completed + n_rx_pkts;

		do {
			*rx_pkts++ =
				rxq->sw_ring[completed++ & rxq->ptr_mask].mbuf;
		} while (completed != rxq->completed);
	}

	return rx_pkts;
}

/*
 * Below Rx pseudo-header (aka Rx prefix) accessors rely on the
 * following fields layout.
 */
static const efx_rx_prefix_layout_t sfc_ef10_rx_prefix_layout = {
	.erpl_fields	= {
		[EFX_RX_PREFIX_FIELD_RSS_HASH]	=
		    { 0, sizeof(uint32_t) * CHAR_BIT, B_FALSE },
		[EFX_RX_PREFIX_FIELD_LENGTH]	=
		    { 8 * CHAR_BIT, sizeof(uint16_t) * CHAR_BIT, B_FALSE },
	}
};
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

static struct rte_mbuf **
sfc_ef10_rx_process_event(struct sfc_ef10_rxq *rxq, efx_qword_t rx_ev,
			  struct rte_mbuf **rx_pkts,
			  struct rte_mbuf ** const rx_pkts_end)
{
	const unsigned int ptr_mask = rxq->ptr_mask;
	unsigned int pending = rxq->pending;
	unsigned int ready;
	struct sfc_ef10_rx_sw_desc *rxd;
	struct rte_mbuf *m;
	struct rte_mbuf *m0;
	const uint8_t *pseudo_hdr;
	uint16_t seg_len;

	ready = (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_DSC_PTR_LBITS) - pending) &
		EFX_MASK32(ESF_DZ_RX_DSC_PTR_LBITS);

	if (ready == 0) {
		/* Rx abort - it was no enough descriptors for Rx packet */
		rte_pktmbuf_free(rxq->scatter_pkt);
		rxq->scatter_pkt = NULL;
		return rx_pkts;
	}

	rxq->pending = pending + ready;

	if (rx_ev.eq_u64[0] &
	    rte_cpu_to_le_64((1ull << ESF_DZ_RX_ECC_ERR_LBN) |
			     (1ull << ESF_DZ_RX_ECRC_ERR_LBN))) {
		SFC_ASSERT(rxq->completed == pending);
		do {
			rxd = &rxq->sw_ring[pending++ & ptr_mask];
			rte_mbuf_raw_free(rxd->mbuf);
		} while (pending != rxq->pending);
		rxq->completed = pending;
		return rx_pkts;
	}

	/* If scattered packet is in progress */
	if (rxq->scatter_pkt != NULL) {
		/* Events for scattered packet frags are not merged */
		SFC_ASSERT(ready == 1);
		SFC_ASSERT(rxq->completed == pending);

		/* There is no pseudo-header in scatter segments. */
		seg_len = EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_BYTES);

		rxd = &rxq->sw_ring[pending++ & ptr_mask];
		m = rxd->mbuf;

		__rte_mbuf_raw_sanity_check(m);

		m->data_off = RTE_PKTMBUF_HEADROOM;
		rte_pktmbuf_data_len(m) = seg_len;
		rte_pktmbuf_pkt_len(m) = seg_len;

		rxq->scatter_pkt->nb_segs++;
		rte_pktmbuf_pkt_len(rxq->scatter_pkt) += seg_len;
		rte_pktmbuf_lastseg(rxq->scatter_pkt)->next = m;

		if (~rx_ev.eq_u64[0] &
		    rte_cpu_to_le_64(1ull << ESF_DZ_RX_CONT_LBN)) {
			*rx_pkts++ = rxq->scatter_pkt;
			rxq->scatter_pkt = NULL;
		}
		rxq->completed = pending;
		return rx_pkts;
	}

	rxd = &rxq->sw_ring[pending++ & ptr_mask];

	sfc_ef10_rx_prefetch_next(rxq, pending & ptr_mask);

	m = rxd->mbuf;

	RTE_BUILD_BUG_ON(sizeof(m->rearm_data[0]) != sizeof(rxq->rearm_data));
	m->rearm_data[0] = rxq->rearm_data;

	/* Classify packet based on Rx event */
	/* Mask RSS hash offload flag if RSS is not enabled */
	sfc_ef10_rx_ev_to_offloads(rx_ev, m,
				   (rxq->flags & SFC_EF10_RXQ_RSS_HASH) ?
				   ~0ull : ~PKT_RX_RSS_HASH);

	/* data_off already moved past pseudo header */
	pseudo_hdr = (uint8_t *)m->buf_addr + RTE_PKTMBUF_HEADROOM;

	/*
	 * Always get RSS hash from pseudo header to avoid
	 * condition/branching. If it is valid or not depends on
	 * PKT_RX_RSS_HASH in m->ol_flags.
	 */
	m->hash.rss = sfc_ef10_rx_pseudo_hdr_get_hash(pseudo_hdr);

	if (ready == 1)
		seg_len = EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_BYTES) -
			rxq->prefix_size;
	else
		seg_len = sfc_ef10_rx_pseudo_hdr_get_len(pseudo_hdr);
	SFC_ASSERT(seg_len > 0);
	rte_pktmbuf_data_len(m) = seg_len;
	rte_pktmbuf_pkt_len(m) = seg_len;

	SFC_ASSERT(m->next == NULL);

	if (~rx_ev.eq_u64[0] & rte_cpu_to_le_64(1ull << ESF_DZ_RX_CONT_LBN)) {
		*rx_pkts++ = m;
		rxq->completed = pending;
	} else {
		/* Events with CONT bit are not merged */
		SFC_ASSERT(ready == 1);
		rxq->scatter_pkt = m;
		rxq->completed = pending;
		return rx_pkts;
	}

	/* Remember mbuf to copy offload flags and packet type from */
	m0 = m;
	while (pending != rxq->pending) {
		rxd = &rxq->sw_ring[pending++ & ptr_mask];

		sfc_ef10_rx_prefetch_next(rxq, pending & ptr_mask);

		m = rxd->mbuf;

		if (rx_pkts != rx_pkts_end) {
			*rx_pkts++ = m;
			rxq->completed = pending;
		}

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

		seg_len = sfc_ef10_rx_pseudo_hdr_get_len(pseudo_hdr);
		SFC_ASSERT(seg_len > 0);
		rte_pktmbuf_data_len(m) = seg_len;
		rte_pktmbuf_pkt_len(m) = seg_len;

		SFC_ASSERT(m->next == NULL);
	}

	return rx_pkts;
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
	struct rte_mbuf ** const rx_pkts_end = &rx_pkts[nb_pkts];
	unsigned int evq_old_read_ptr;
	efx_qword_t rx_ev;

	rx_pkts = sfc_ef10_rx_pending(rxq, rx_pkts, nb_pkts);

	if (unlikely(rxq->flags &
		     (SFC_EF10_RXQ_NOT_RUNNING | SFC_EF10_RXQ_EXCEPTION)))
		goto done;

	evq_old_read_ptr = rxq->evq_read_ptr;
	while (rx_pkts != rx_pkts_end && sfc_ef10_rx_get_event(rxq, &rx_ev)) {
		/*
		 * DROP_EVENT is an internal to the NIC, software should
		 * never see it and, therefore, may ignore it.
		 */

		rx_pkts = sfc_ef10_rx_process_event(rxq, rx_ev,
						    rx_pkts, rx_pkts_end);
	}

	sfc_ef10_ev_qclear(rxq->evq_hw_ring, rxq->ptr_mask, evq_old_read_ptr,
			   rxq->evq_read_ptr);

	/* It is not a problem if we refill in the case of exception */
	sfc_ef10_rx_qrefill(rxq);

	if ((rxq->flags & SFC_EF10_RXQ_FLAG_INTR_EN) &&
	    rxq->evq_read_ptr_primed != rxq->evq_read_ptr)
		sfc_ef10_rx_qprime(rxq);

done:
	return nb_pkts - (rx_pkts_end - rx_pkts);
}

const uint32_t *
sfc_ef10_supported_ptypes_get(uint32_t tunnel_encaps)
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
	static const uint32_t ef10_overlay_ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L2_ETHER_QINQ,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	/*
	 * The function returns static set of supported packet types,
	 * so we can't build it dynamically based on supported tunnel
	 * encapsulations and should limit to known sets.
	 */
	switch (tunnel_encaps) {
	case (1u << EFX_TUNNEL_PROTOCOL_VXLAN |
	      1u << EFX_TUNNEL_PROTOCOL_GENEVE |
	      1u << EFX_TUNNEL_PROTOCOL_NVGRE):
		return ef10_overlay_ptypes;
	default:
		SFC_GENERIC_LOG(ERR,
			"Unexpected set of supported tunnel encapsulations: %#x",
			tunnel_encaps);
		/* FALLTHROUGH */
	case 0:
		return ef10_native_ptypes;
	}
}

static sfc_dp_rx_qdesc_npending_t sfc_ef10_rx_qdesc_npending;
static unsigned int
sfc_ef10_rx_qdesc_npending(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);
	efx_qword_t rx_ev;
	const unsigned int evq_old_read_ptr = rxq->evq_read_ptr;
	unsigned int pending = rxq->pending;
	unsigned int ready;

	if (unlikely(rxq->flags &
		     (SFC_EF10_RXQ_NOT_RUNNING | SFC_EF10_RXQ_EXCEPTION)))
		goto done;

	while (sfc_ef10_rx_get_event(rxq, &rx_ev)) {
		ready = (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_DSC_PTR_LBITS) -
			 pending) &
			EFX_MASK32(ESF_DZ_RX_DSC_PTR_LBITS);
		pending += ready;
	}

	/*
	 * The function does not process events, so return event queue read
	 * pointer to the original position to allow the events that were
	 * read to be processed later
	 */
	rxq->evq_read_ptr = evq_old_read_ptr;

done:
	return pending - rxq->completed;
}

static sfc_dp_rx_qdesc_status_t sfc_ef10_rx_qdesc_status;
static int
sfc_ef10_rx_qdesc_status(struct sfc_dp_rxq *dp_rxq, uint16_t offset)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);
	unsigned int npending = sfc_ef10_rx_qdesc_npending(dp_rxq);

	if (unlikely(offset > rxq->ptr_mask))
		return -EINVAL;

	if (offset < npending)
		return RTE_ETH_RX_DESC_DONE;

	if (offset < (rxq->added - rxq->completed))
		return RTE_ETH_RX_DESC_AVAIL;

	return RTE_ETH_RX_DESC_UNAVAIL;
}


static sfc_dp_rx_get_dev_info_t sfc_ef10_rx_get_dev_info;
static void
sfc_ef10_rx_get_dev_info(struct rte_eth_dev_info *dev_info)
{
	/*
	 * Number of descriptors just defines maximum number of pushed
	 * descriptors (fill level).
	 */
	dev_info->rx_desc_lim.nb_min = SFC_RX_REFILL_BULK;
	dev_info->rx_desc_lim.nb_align = SFC_RX_REFILL_BULK;
}


static sfc_dp_rx_qsize_up_rings_t sfc_ef10_rx_qsize_up_rings;
static int
sfc_ef10_rx_qsize_up_rings(uint16_t nb_rx_desc,
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
				      SFC_EF10_RXQ_LIMIT(*evq_entries));
	return 0;
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
	rxq->max_fill_level = info->max_fill_level;
	rxq->refill_threshold = info->refill_threshold;
	rxq->rearm_data =
		sfc_ef10_mk_mbuf_rearm_data(port_id, info->prefix_size);
	rxq->prefix_size = info->prefix_size;
	rxq->buf_size = info->buf_size;
	rxq->refill_mb_pool = info->refill_mb_pool;
	rxq->rxq_hw_ring = info->rxq_hw_ring;
	rxq->doorbell = (volatile uint8_t *)info->mem_bar +
			ER_DZ_RX_DESC_UPD_REG_OFST +
			(info->hw_index << info->vi_window_shift);
	rxq->evq_prime = (volatile uint8_t *)info->mem_bar +
		      ER_DZ_EVQ_RPTR_REG_OFST +
		      (info->evq_hw_index << info->vi_window_shift);

	sfc_ef10_rx_info(&rxq->dp.dpq, "RxQ doorbell is %p", rxq->doorbell);

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
sfc_ef10_rx_qstart(struct sfc_dp_rxq *dp_rxq, unsigned int evq_read_ptr,
		   const efx_rx_prefix_layout_t *pinfo)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	SFC_ASSERT(rxq->completed == 0);
	SFC_ASSERT(rxq->pending == 0);
	SFC_ASSERT(rxq->added == 0);

	if (pinfo->erpl_length != rxq->prefix_size ||
	    efx_rx_prefix_layout_check(pinfo, &sfc_ef10_rx_prefix_layout) != 0)
		return ENOTSUP;

	sfc_ef10_rx_qrefill(rxq);

	rxq->evq_read_ptr = evq_read_ptr;

	rxq->flags |= SFC_EF10_RXQ_STARTED;
	rxq->flags &= ~(SFC_EF10_RXQ_NOT_RUNNING | SFC_EF10_RXQ_EXCEPTION);

	if (rxq->flags & SFC_EF10_RXQ_FLAG_INTR_EN)
		sfc_ef10_rx_qprime(rxq);

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

	rte_pktmbuf_free(rxq->scatter_pkt);
	rxq->scatter_pkt = NULL;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_ring[i & rxq->ptr_mask];
		rte_mbuf_raw_free(rxd->mbuf);
		rxd->mbuf = NULL;
	}

	rxq->completed = rxq->pending = rxq->added = 0;

	rxq->flags &= ~SFC_EF10_RXQ_STARTED;
}

static sfc_dp_rx_intr_enable_t sfc_ef10_rx_intr_enable;
static int
sfc_ef10_rx_intr_enable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	rxq->flags |= SFC_EF10_RXQ_FLAG_INTR_EN;
	if (rxq->flags & SFC_EF10_RXQ_STARTED)
		sfc_ef10_rx_qprime(rxq);
	return 0;
}

static sfc_dp_rx_intr_disable_t sfc_ef10_rx_intr_disable;
static int
sfc_ef10_rx_intr_disable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_ef10_rxq *rxq = sfc_ef10_rxq_by_dp_rxq(dp_rxq);

	/* Cannot disarm, just disable rearm */
	rxq->flags &= ~SFC_EF10_RXQ_FLAG_INTR_EN;
	return 0;
}

struct sfc_dp_rx sfc_ef10_rx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EF10,
		.type		= SFC_DP_RX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_EF10,
	},
	.features		= SFC_DP_RX_FEAT_MULTI_PROCESS |
				  SFC_DP_RX_FEAT_INTR,
	.dev_offload_capa	= DEV_RX_OFFLOAD_CHECKSUM |
				  DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
				  DEV_RX_OFFLOAD_RSS_HASH,
	.queue_offload_capa	= DEV_RX_OFFLOAD_SCATTER,
	.get_dev_info		= sfc_ef10_rx_get_dev_info,
	.qsize_up_rings		= sfc_ef10_rx_qsize_up_rings,
	.qcreate		= sfc_ef10_rx_qcreate,
	.qdestroy		= sfc_ef10_rx_qdestroy,
	.qstart			= sfc_ef10_rx_qstart,
	.qstop			= sfc_ef10_rx_qstop,
	.qrx_ev			= sfc_ef10_rx_qrx_ev,
	.qpurge			= sfc_ef10_rx_qpurge,
	.supported_ptypes_get	= sfc_ef10_supported_ptypes_get,
	.qdesc_npending		= sfc_ef10_rx_qdesc_npending,
	.qdesc_status		= sfc_ef10_rx_qdesc_status,
	.intr_enable		= sfc_ef10_rx_intr_enable,
	.intr_disable		= sfc_ef10_rx_intr_disable,
	.pkt_burst		= sfc_ef10_recv_pkts,
};
