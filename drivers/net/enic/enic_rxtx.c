/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_net.h>
#include <rte_prefetch.h>

#include "enic_compat.h"
#include "rq_enet_desc.h"
#include "enic.h"
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>

#define RTE_PMD_USE_PREFETCH

#ifdef RTE_PMD_USE_PREFETCH
/*Prefetch a cache line into all cache levels. */
#define rte_enic_prefetch(p) rte_prefetch0(p)
#else
#define rte_enic_prefetch(p) do {} while (0)
#endif

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p) rte_prefetch1(p)
#else
#define rte_packet_prefetch(p) do {} while (0)
#endif

static inline uint16_t
enic_cq_rx_desc_ciflags(struct cq_enet_rq_desc *crd)
{
	return le16_to_cpu(crd->completed_index_flags) & ~CQ_DESC_COMP_NDX_MASK;
}

static inline uint16_t
enic_cq_rx_desc_bwflags(struct cq_enet_rq_desc *crd)
{
	return le16_to_cpu(crd->bytes_written_flags) &
			   ~CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
}

static inline uint8_t
enic_cq_rx_desc_packet_error(uint16_t bwflags)
{
	return (bwflags & CQ_ENET_RQ_DESC_FLAGS_TRUNCATED) ==
		CQ_ENET_RQ_DESC_FLAGS_TRUNCATED;
}

static inline uint8_t
enic_cq_rx_desc_eop(uint16_t ciflags)
{
	return (ciflags & CQ_ENET_RQ_DESC_FLAGS_EOP)
		== CQ_ENET_RQ_DESC_FLAGS_EOP;
}

static inline uint8_t
enic_cq_rx_desc_csum_not_calc(struct cq_enet_rq_desc *cqrd)
{
	return (le16_to_cpu(cqrd->q_number_rss_type_flags) &
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC) ==
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC;
}

static inline uint8_t
enic_cq_rx_desc_ipv4_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return (cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK;
}

static inline uint8_t
enic_cq_rx_desc_tcp_udp_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return (cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK;
}

static inline uint8_t
enic_cq_rx_desc_rss_type(struct cq_enet_rq_desc *cqrd)
{
	return (uint8_t)((le16_to_cpu(cqrd->q_number_rss_type_flags) >>
		CQ_DESC_Q_NUM_BITS) & CQ_ENET_RQ_DESC_RSS_TYPE_MASK);
}

static inline uint32_t
enic_cq_rx_desc_rss_hash(struct cq_enet_rq_desc *cqrd)
{
	return le32_to_cpu(cqrd->rss_hash);
}

static inline uint16_t
enic_cq_rx_desc_vlan(struct cq_enet_rq_desc *cqrd)
{
	return le16_to_cpu(cqrd->vlan);
}

static inline uint16_t
enic_cq_rx_desc_n_bytes(struct cq_desc *cqd)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	return le16_to_cpu(cqrd->bytes_written_flags) &
		CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
}


static inline uint8_t
enic_cq_rx_check_err(struct cq_desc *cqd)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t bwflags;

	bwflags = enic_cq_rx_desc_bwflags(cqrd);
	if (unlikely(enic_cq_rx_desc_packet_error(bwflags)))
		return 1;
	return 0;
}

/* Lookup table to translate RX CQ flags to mbuf flags. */
static inline uint32_t
enic_cq_rx_flags_to_pkt_type(struct cq_desc *cqd, uint8_t tnl)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint8_t cqrd_flags = cqrd->flags;
	/*
	 * Odd-numbered entries are for tunnel packets. All packet type info
	 * applies to the inner packet, and there is no info on the outer
	 * packet. The outer flags in these entries exist only to avoid
	 * changing enic_cq_rx_to_pkt_flags(). They are cleared from mbuf
	 * afterwards.
	 *
	 * Also, as there is no tunnel type info (VXLAN, NVGRE, or GENEVE), set
	 * RTE_PTYPE_TUNNEL_GRENAT..
	 */
	static const uint32_t cq_type_table[128] __rte_cache_aligned = {
		[0x00] = RTE_PTYPE_UNKNOWN,
		[0x20] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG,
		[0x21] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_NONFRAG,
		[0x22] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		[0x23] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_UDP,
		[0x24] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		[0x25] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_TCP,
		[0x60] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG,
		[0x61] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_FRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_FRAG,
		[0x62] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		[0x63] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_UDP,
		[0x64] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		[0x65] = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_TCP,
		[0x10] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG,
		[0x11] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_NONFRAG |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_NONFRAG,
		[0x12] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP,
		[0x13] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_UDP,
		[0x14] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP,
		[0x15] = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP |
			 RTE_PTYPE_TUNNEL_GRENAT |
			 RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_INNER_L4_TCP,
		/* All others reserved */
	};
	cqrd_flags &= CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT
		| CQ_ENET_RQ_DESC_FLAGS_IPV4 | CQ_ENET_RQ_DESC_FLAGS_IPV6
		| CQ_ENET_RQ_DESC_FLAGS_TCP | CQ_ENET_RQ_DESC_FLAGS_UDP;
	return cq_type_table[cqrd_flags + tnl];
}

static inline void
enic_cq_rx_to_pkt_flags(struct cq_desc *cqd, struct rte_mbuf *mbuf)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t bwflags, pkt_flags = 0, vlan_tci;
	bwflags = enic_cq_rx_desc_bwflags(cqrd);
	vlan_tci = enic_cq_rx_desc_vlan(cqrd);

	/* VLAN STRIPPED flag. The L2 packet type updated here also */
	if (bwflags & CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED) {
		pkt_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mbuf->packet_type |= RTE_PTYPE_L2_ETHER;
	} else {
		if (vlan_tci != 0)
			mbuf->packet_type |= RTE_PTYPE_L2_ETHER_VLAN;
		else
			mbuf->packet_type |= RTE_PTYPE_L2_ETHER;
	}
	mbuf->vlan_tci = vlan_tci;

	if ((cqd->type_color & CQ_DESC_TYPE_MASK) == CQ_DESC_TYPE_CLASSIFIER) {
		struct cq_enet_rq_clsf_desc *clsf_cqd;
		uint16_t filter_id;
		clsf_cqd = (struct cq_enet_rq_clsf_desc *)cqd;
		filter_id = clsf_cqd->filter_id;
		if (filter_id) {
			pkt_flags |= PKT_RX_FDIR;
			if (filter_id != ENIC_MAGIC_FILTER_ID) {
				mbuf->hash.fdir.hi = clsf_cqd->filter_id;
				pkt_flags |= PKT_RX_FDIR_ID;
			}
		}
	} else if (enic_cq_rx_desc_rss_type(cqrd)) {
		/* RSS flag */
		pkt_flags |= PKT_RX_RSS_HASH;
		mbuf->hash.rss = enic_cq_rx_desc_rss_hash(cqrd);
	}

	/* checksum flags */
	if (mbuf->packet_type & (RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV6)) {
		if (!enic_cq_rx_desc_csum_not_calc(cqrd)) {
			uint32_t l4_flags;
			l4_flags = mbuf->packet_type & RTE_PTYPE_L4_MASK;

			/*
			 * When overlay offload is enabled, the NIC may
			 * set ipv4_csum_ok=1 if the inner packet is IPv6..
			 * So, explicitly check for IPv4 before checking
			 * ipv4_csum_ok.
			 */
			if (mbuf->packet_type & RTE_PTYPE_L3_IPV4) {
				if (enic_cq_rx_desc_ipv4_csum_ok(cqrd))
					pkt_flags |= PKT_RX_IP_CKSUM_GOOD;
				else
					pkt_flags |= PKT_RX_IP_CKSUM_BAD;
			}

			if (l4_flags == RTE_PTYPE_L4_UDP ||
			    l4_flags == RTE_PTYPE_L4_TCP) {
				if (enic_cq_rx_desc_tcp_udp_csum_ok(cqrd))
					pkt_flags |= PKT_RX_L4_CKSUM_GOOD;
				else
					pkt_flags |= PKT_RX_L4_CKSUM_BAD;
			}
		}
	}

	mbuf->ol_flags = pkt_flags;
}

/* dummy receive function to replace actual function in
 * order to do safe reconfiguration operations.
 */
uint16_t
enic_dummy_recv_pkts(__rte_unused void *rx_queue,
		     __rte_unused struct rte_mbuf **rx_pkts,
		     __rte_unused uint16_t nb_pkts)
{
	return 0;
}

uint16_t
enic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	       uint16_t nb_pkts)
{
	struct vnic_rq *sop_rq = rx_queue;
	struct vnic_rq *data_rq;
	struct vnic_rq *rq;
	struct enic *enic = vnic_dev_priv(sop_rq->vdev);
	uint16_t cq_idx;
	uint16_t rq_idx;
	uint16_t rq_num;
	struct rte_mbuf *nmb, *rxmb;
	uint16_t nb_rx = 0;
	struct vnic_cq *cq;
	volatile struct cq_desc *cqd_ptr;
	uint8_t color;
	uint8_t tnl;
	uint16_t seg_length;
	struct rte_mbuf *first_seg = sop_rq->pkt_first_seg;
	struct rte_mbuf *last_seg = sop_rq->pkt_last_seg;

	cq = &enic->cq[enic_cq_rq(enic, sop_rq->index)];
	cq_idx = cq->to_clean;		/* index of cqd, rqd, mbuf_table */
	cqd_ptr = (struct cq_desc *)(cq->ring.descs) + cq_idx;

	data_rq = &enic->rq[sop_rq->data_queue_idx];

	while (nb_rx < nb_pkts) {
		volatile struct rq_enet_desc *rqd_ptr;
		struct cq_desc cqd;
		uint8_t packet_error;
		uint16_t ciflags;

		/* Check for pkts available */
		color = (cqd_ptr->type_color >> CQ_DESC_COLOR_SHIFT)
			& CQ_DESC_COLOR_MASK;
		if (color == cq->last_color)
			break;

		/* Get the cq descriptor and extract rq info from it */
		cqd = *cqd_ptr;
		rq_num = cqd.q_number & CQ_DESC_Q_NUM_MASK;
		rq_idx = cqd.completed_index & CQ_DESC_COMP_NDX_MASK;

		rq = &enic->rq[rq_num];
		rqd_ptr = ((struct rq_enet_desc *)rq->ring.descs) + rq_idx;

		/* allocate a new mbuf */
		nmb = rte_mbuf_raw_alloc(rq->mp);
		if (nmb == NULL) {
			rte_atomic64_inc(&enic->soft_stats.rx_nombuf);
			break;
		}

		/* A packet error means descriptor and data are untrusted */
		packet_error = enic_cq_rx_check_err(&cqd);

		/* Get the mbuf to return and replace with one just allocated */
		rxmb = rq->mbuf_ring[rq_idx];
		rq->mbuf_ring[rq_idx] = nmb;

		/* Increment cqd, rqd, mbuf_table index */
		cq_idx++;
		if (unlikely(cq_idx == cq->ring.desc_count)) {
			cq_idx = 0;
			cq->last_color = cq->last_color ? 0 : 1;
		}

		/* Prefetch next mbuf & desc while processing current one */
		cqd_ptr = (struct cq_desc *)(cq->ring.descs) + cq_idx;
		rte_enic_prefetch(cqd_ptr);

		ciflags = enic_cq_rx_desc_ciflags(
			(struct cq_enet_rq_desc *)&cqd);

		/* Push descriptor for newly allocated mbuf */
		nmb->data_off = RTE_PKTMBUF_HEADROOM;
		/*
		 * Only the address needs to be refilled. length_type of the
		 * descriptor it set during initialization
		 * (enic_alloc_rx_queue_mbufs) and does not change.
		 */
		rqd_ptr->address = rte_cpu_to_le_64(nmb->buf_iova +
						    RTE_PKTMBUF_HEADROOM);

		/* Fill in the rest of the mbuf */
		seg_length = enic_cq_rx_desc_n_bytes(&cqd);

		if (rq->is_sop) {
			first_seg = rxmb;
			first_seg->pkt_len = seg_length;
		} else {
			first_seg->pkt_len = (uint16_t)(first_seg->pkt_len
							+ seg_length);
			first_seg->nb_segs++;
			last_seg->next = rxmb;
		}

		rxmb->port = enic->port_id;
		rxmb->data_len = seg_length;

		rq->rx_nb_hold++;

		if (!(enic_cq_rx_desc_eop(ciflags))) {
			last_seg = rxmb;
			continue;
		}

		/*
		 * When overlay offload is enabled, CQ.fcoe indicates the
		 * packet is tunnelled.
		 */
		tnl = enic->overlay_offload &&
			(ciflags & CQ_ENET_RQ_DESC_FLAGS_FCOE) != 0;
		/* cq rx flags are only valid if eop bit is set */
		first_seg->packet_type =
			enic_cq_rx_flags_to_pkt_type(&cqd, tnl);
		enic_cq_rx_to_pkt_flags(&cqd, first_seg);
		/* Wipe the outer types set by enic_cq_rx_flags_to_pkt_type() */
		if (tnl) {
			first_seg->packet_type &= ~(RTE_PTYPE_L3_MASK |
						    RTE_PTYPE_L4_MASK);
		}
		if (unlikely(packet_error)) {
			rte_pktmbuf_free(first_seg);
			rte_atomic64_inc(&enic->soft_stats.rx_packet_errors);
			continue;
		}


		/* prefetch mbuf data for caller */
		rte_packet_prefetch(RTE_PTR_ADD(first_seg->buf_addr,
				    RTE_PKTMBUF_HEADROOM));

		/* store the mbuf address into the next entry of the array */
		rx_pkts[nb_rx++] = first_seg;
	}

	sop_rq->pkt_first_seg = first_seg;
	sop_rq->pkt_last_seg = last_seg;

	cq->to_clean = cq_idx;

	if ((sop_rq->rx_nb_hold + data_rq->rx_nb_hold) >
	    sop_rq->rx_free_thresh) {
		if (data_rq->in_use) {
			data_rq->posted_index =
				enic_ring_add(data_rq->ring.desc_count,
					      data_rq->posted_index,
					      data_rq->rx_nb_hold);
			data_rq->rx_nb_hold = 0;
		}
		sop_rq->posted_index = enic_ring_add(sop_rq->ring.desc_count,
						     sop_rq->posted_index,
						     sop_rq->rx_nb_hold);
		sop_rq->rx_nb_hold = 0;

		rte_mb();
		if (data_rq->in_use)
			iowrite32_relaxed(data_rq->posted_index,
					  &data_rq->ctrl->posted_index);
		rte_compiler_barrier();
		iowrite32_relaxed(sop_rq->posted_index,
				  &sop_rq->ctrl->posted_index);
	}


	return nb_rx;
}

static inline void enic_free_wq_bufs(struct vnic_wq *wq, u16 completed_index)
{
	struct vnic_wq_buf *buf;
	struct rte_mbuf *m, *free[ENIC_MAX_WQ_DESCS];
	unsigned int nb_to_free, nb_free = 0, i;
	struct rte_mempool *pool;
	unsigned int tail_idx;
	unsigned int desc_count = wq->ring.desc_count;

	nb_to_free = enic_ring_sub(desc_count, wq->tail_idx, completed_index)
				   + 1;
	tail_idx = wq->tail_idx;
	buf = &wq->bufs[tail_idx];
	pool = ((struct rte_mbuf *)buf->mb)->pool;
	for (i = 0; i < nb_to_free; i++) {
		buf = &wq->bufs[tail_idx];
		m = rte_pktmbuf_prefree_seg((struct rte_mbuf *)(buf->mb));
		buf->mb = NULL;

		if (unlikely(m == NULL)) {
			tail_idx = enic_ring_incr(desc_count, tail_idx);
			continue;
		}

		if (likely(m->pool == pool)) {
			RTE_ASSERT(nb_free < ENIC_MAX_WQ_DESCS);
			free[nb_free++] = m;
		} else {
			rte_mempool_put_bulk(pool, (void *)free, nb_free);
			free[0] = m;
			nb_free = 1;
			pool = m->pool;
		}
		tail_idx = enic_ring_incr(desc_count, tail_idx);
	}

	if (nb_free > 0)
		rte_mempool_put_bulk(pool, (void **)free, nb_free);

	wq->tail_idx = tail_idx;
	wq->ring.desc_avail += nb_to_free;
}

unsigned int enic_cleanup_wq(__rte_unused struct enic *enic, struct vnic_wq *wq)
{
	u16 completed_index;

	completed_index = *((uint32_t *)wq->cqmsg_rz->addr) & 0xffff;

	if (wq->last_completed_index != completed_index) {
		enic_free_wq_bufs(wq, completed_index);
		wq->last_completed_index = completed_index;
	}
	return 0;
}

uint16_t enic_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct vnic_wq *wq = (struct vnic_wq *)tx_queue;
	int32_t ret;
	uint16_t i;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i != nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;
		if (ol_flags & wq->tx_offload_notsup_mask) {
			rte_errno = ENOTSUP;
			return i;
		}
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
	}

	return i;
}

uint16_t enic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	uint16_t index;
	unsigned int pkt_len, data_len;
	unsigned int nb_segs;
	struct rte_mbuf *tx_pkt;
	struct vnic_wq *wq = (struct vnic_wq *)tx_queue;
	struct enic *enic = vnic_dev_priv(wq->vdev);
	unsigned short vlan_id;
	uint64_t ol_flags;
	uint64_t ol_flags_mask;
	unsigned int wq_desc_avail;
	int head_idx;
	struct vnic_wq_buf *buf;
	unsigned int desc_count;
	struct wq_enet_desc *descs, *desc_p, desc_tmp;
	uint16_t mss;
	uint8_t vlan_tag_insert;
	uint8_t eop;
	uint64_t bus_addr;
	uint8_t offload_mode;
	uint16_t header_len;
	uint64_t tso;
	rte_atomic64_t *tx_oversized;

	enic_cleanup_wq(enic, wq);
	wq_desc_avail = vnic_wq_desc_avail(wq);
	head_idx = wq->head_idx;
	desc_count = wq->ring.desc_count;
	ol_flags_mask = PKT_TX_VLAN_PKT | PKT_TX_IP_CKSUM | PKT_TX_L4_MASK;
	tx_oversized = &enic->soft_stats.tx_oversized;

	nb_pkts = RTE_MIN(nb_pkts, ENIC_TX_XMIT_MAX);

	for (index = 0; index < nb_pkts; index++) {
		tx_pkt = *tx_pkts++;
		pkt_len = tx_pkt->pkt_len;
		data_len = tx_pkt->data_len;
		ol_flags = tx_pkt->ol_flags;
		nb_segs = tx_pkt->nb_segs;
		tso = ol_flags & PKT_TX_TCP_SEG;

		/* drop packet if it's too big to send */
		if (unlikely(!tso && pkt_len > ENIC_TX_MAX_PKT_SIZE)) {
			rte_pktmbuf_free(tx_pkt);
			rte_atomic64_inc(tx_oversized);
			continue;
		}

		if (nb_segs > wq_desc_avail) {
			if (index > 0)
				goto post;
			goto done;
		}

		mss = 0;
		vlan_id = tx_pkt->vlan_tci;
		vlan_tag_insert = !!(ol_flags & PKT_TX_VLAN_PKT);
		bus_addr = (dma_addr_t)
			   (tx_pkt->buf_iova + tx_pkt->data_off);

		descs = (struct wq_enet_desc *)wq->ring.descs;
		desc_p = descs + head_idx;

		eop = (data_len == pkt_len);
		offload_mode = WQ_ENET_OFFLOAD_MODE_CSUM;
		header_len = 0;

		if (tso) {
			header_len = tx_pkt->l2_len + tx_pkt->l3_len +
				     tx_pkt->l4_len;

			/* Drop if non-TCP packet or TSO seg size is too big */
			if (unlikely(header_len == 0 || ((tx_pkt->tso_segsz +
			    header_len) > ENIC_TX_MAX_PKT_SIZE))) {
				rte_pktmbuf_free(tx_pkt);
				rte_atomic64_inc(tx_oversized);
				continue;
			}

			offload_mode = WQ_ENET_OFFLOAD_MODE_TSO;
			mss = tx_pkt->tso_segsz;
			/* For tunnel, need the size of outer+inner headers */
			if (ol_flags & PKT_TX_TUNNEL_MASK) {
				header_len += tx_pkt->outer_l2_len +
					tx_pkt->outer_l3_len;
			}
		}

		if ((ol_flags & ol_flags_mask) && (header_len == 0)) {
			if (ol_flags & PKT_TX_IP_CKSUM)
				mss |= ENIC_CALC_IP_CKSUM;

			/* Nic uses just 1 bit for UDP and TCP */
			switch (ol_flags & PKT_TX_L4_MASK) {
			case PKT_TX_TCP_CKSUM:
			case PKT_TX_UDP_CKSUM:
				mss |= ENIC_CALC_TCP_UDP_CKSUM;
				break;
			}
		}


		wq_enet_desc_enc(&desc_tmp, bus_addr, data_len, mss, header_len,
				 offload_mode, eop, eop, 0, vlan_tag_insert,
				 vlan_id, 0);

		*desc_p = desc_tmp;
		buf = &wq->bufs[head_idx];
		buf->mb = (void *)tx_pkt;
		head_idx = enic_ring_incr(desc_count, head_idx);
		wq_desc_avail--;

		if (!eop) {
			for (tx_pkt = tx_pkt->next; tx_pkt; tx_pkt =
			    tx_pkt->next) {
				data_len = tx_pkt->data_len;

				if (tx_pkt->next == NULL)
					eop = 1;
				desc_p = descs + head_idx;
				bus_addr = (dma_addr_t)(tx_pkt->buf_iova
					   + tx_pkt->data_off);
				wq_enet_desc_enc((struct wq_enet_desc *)
						 &desc_tmp, bus_addr, data_len,
						 mss, 0, offload_mode, eop, eop,
						 0, vlan_tag_insert, vlan_id,
						 0);

				*desc_p = desc_tmp;
				buf = &wq->bufs[head_idx];
				buf->mb = (void *)tx_pkt;
				head_idx = enic_ring_incr(desc_count, head_idx);
				wq_desc_avail--;
			}
		}
	}
 post:
	rte_wmb();
	iowrite32_relaxed(head_idx, &wq->ctrl->posted_index);
 done:
	wq->ring.desc_avail = wq_desc_avail;
	wq->head_idx = head_idx;

	return index;
}


