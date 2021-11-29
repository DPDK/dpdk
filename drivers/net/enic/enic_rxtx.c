/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_prefetch.h>

#include "enic_compat.h"
#include "rq_enet_desc.h"
#include "enic.h"
#include "enic_rxtx_common.h"
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

static inline uint16_t
enic_recv_pkts_common(void *rx_queue, struct rte_mbuf **rx_pkts,
		      uint16_t nb_pkts, const bool use_64b_desc)
{
	struct vnic_rq *sop_rq = rx_queue;
	struct vnic_rq *data_rq;
	struct vnic_rq *rq;
	struct enic *enic = vnic_dev_priv(sop_rq->vdev);
	uint16_t cq_idx;
	uint16_t rq_idx, max_rx;
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
	const int desc_size = use_64b_desc ?
		sizeof(struct cq_enet_rq_desc_64) :
		sizeof(struct cq_enet_rq_desc);
	RTE_BUILD_BUG_ON(sizeof(struct cq_enet_rq_desc_64) != 64);

	cq = &enic->cq[enic_cq_rq(enic, sop_rq->index)];
	cq_idx = cq->to_clean;		/* index of cqd, rqd, mbuf_table */
	cqd_ptr = (struct cq_desc *)((uintptr_t)(cq->ring.descs) +
				     (uintptr_t)cq_idx * desc_size);
	color = cq->last_color;

	data_rq = &enic->rq[sop_rq->data_queue_idx];

	/* Receive until the end of the ring, at most. */
	max_rx = RTE_MIN(nb_pkts, cq->ring.desc_count - cq_idx);

	while (max_rx) {
		volatile struct rq_enet_desc *rqd_ptr;
		struct cq_desc cqd;
		uint8_t packet_error;
		uint16_t ciflags;
		uint8_t tc;

		max_rx--;

		tc = *(volatile uint8_t *)((uintptr_t)cqd_ptr + desc_size - 1);
		/* Check for pkts available */
		if ((tc & CQ_DESC_COLOR_MASK_NOSHIFT) == color)
			break;

		/* Get the cq descriptor and extract rq info from it */
		cqd = *cqd_ptr;
		/*
		 * The first 16B of 64B descriptor is identical to the
		 * 16B descriptor, except type_color. Copy type_color
		 * from the 64B descriptor into the 16B descriptor's
		 * field, so the code below can assume the 16B
		 * descriptor format.
		 */
		if (use_64b_desc)
			cqd.type_color = tc;
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
		cq_idx++;

		/* Prefetch next mbuf & desc while processing current one */
		cqd_ptr = (struct cq_desc *)((uintptr_t)(cq->ring.descs) +
					     (uintptr_t)cq_idx * desc_size);
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
	if (unlikely(cq_idx == cq->ring.desc_count)) {
		cq_idx = 0;
		cq->last_color ^= CQ_DESC_COLOR_MASK_NOSHIFT;
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

uint16_t
enic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return enic_recv_pkts_common(rx_queue, rx_pkts, nb_pkts, false);
}

uint16_t
enic_recv_pkts_64(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return enic_recv_pkts_common(rx_queue, rx_pkts, nb_pkts, true);
}

uint16_t
enic_noscatter_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct rte_mbuf *mb, **rx, **rxmb;
	uint16_t cq_idx, nb_rx, max_rx;
	struct cq_enet_rq_desc *cqd;
	struct rq_enet_desc *rqd;
	unsigned int port_id;
	struct vnic_cq *cq;
	struct vnic_rq *rq;
	struct enic *enic;
	uint8_t color;
	bool overlay;
	bool tnl;

	rq = rx_queue;
	enic = vnic_dev_priv(rq->vdev);
	cq = &enic->cq[enic_cq_rq(enic, rq->index)];
	cq_idx = cq->to_clean;

	/*
	 * Fill up the reserve of free mbufs. Below, we restock the receive
	 * ring with these mbufs to avoid allocation failures.
	 */
	if (rq->num_free_mbufs == 0) {
		if (rte_mempool_get_bulk(rq->mp, (void **)rq->free_mbufs,
					 ENIC_RX_BURST_MAX))
			return 0;
		rq->num_free_mbufs = ENIC_RX_BURST_MAX;
	}

	/* Receive until the end of the ring, at most. */
	max_rx = RTE_MIN(nb_pkts, rq->num_free_mbufs);
	max_rx = RTE_MIN(max_rx, cq->ring.desc_count - cq_idx);

	cqd = (struct cq_enet_rq_desc *)(cq->ring.descs) + cq_idx;
	color = cq->last_color;
	rxmb = rq->mbuf_ring + cq_idx;
	port_id = enic->port_id;
	overlay = enic->overlay_offload;

	rx = rx_pkts;
	while (max_rx) {
		max_rx--;
		if ((cqd->type_color & CQ_DESC_COLOR_MASK_NOSHIFT) == color)
			break;
		if (unlikely(cqd->bytes_written_flags &
			     CQ_ENET_RQ_DESC_FLAGS_TRUNCATED)) {
			rte_pktmbuf_free(*rxmb++);
			rte_atomic64_inc(&enic->soft_stats.rx_packet_errors);
			cqd++;
			continue;
		}

		mb = *rxmb++;
		/* prefetch mbuf data for caller */
		rte_packet_prefetch(RTE_PTR_ADD(mb->buf_addr,
				    RTE_PKTMBUF_HEADROOM));
		mb->data_len = cqd->bytes_written_flags &
			CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
		mb->pkt_len = mb->data_len;
		mb->port = port_id;
		tnl = overlay && (cqd->completed_index_flags &
				  CQ_ENET_RQ_DESC_FLAGS_FCOE) != 0;
		mb->packet_type =
			enic_cq_rx_flags_to_pkt_type((struct cq_desc *)cqd,
						     tnl);
		enic_cq_rx_to_pkt_flags((struct cq_desc *)cqd, mb);
		/* Wipe the outer types set by enic_cq_rx_flags_to_pkt_type() */
		if (tnl) {
			mb->packet_type &= ~(RTE_PTYPE_L3_MASK |
					     RTE_PTYPE_L4_MASK);
		}
		cqd++;
		*rx++ = mb;
	}
	/* Number of descriptors visited */
	nb_rx = cqd - (struct cq_enet_rq_desc *)(cq->ring.descs) - cq_idx;
	if (nb_rx == 0)
		return 0;
	rqd = ((struct rq_enet_desc *)rq->ring.descs) + cq_idx;
	rxmb = rq->mbuf_ring + cq_idx;
	cq_idx += nb_rx;
	rq->rx_nb_hold += nb_rx;
	if (unlikely(cq_idx == cq->ring.desc_count)) {
		cq_idx = 0;
		cq->last_color ^= CQ_DESC_COLOR_MASK_NOSHIFT;
	}
	cq->to_clean = cq_idx;

	memcpy(rxmb, rq->free_mbufs + ENIC_RX_BURST_MAX - rq->num_free_mbufs,
	       sizeof(struct rte_mbuf *) * nb_rx);
	rq->num_free_mbufs -= nb_rx;
	while (nb_rx) {
		nb_rx--;
		mb = *rxmb++;
		mb->data_off = RTE_PKTMBUF_HEADROOM;
		rqd->address = mb->buf_iova + RTE_PKTMBUF_HEADROOM;
		rqd++;
	}
	if (rq->rx_nb_hold > rq->rx_free_thresh) {
		rq->posted_index = enic_ring_add(rq->ring.desc_count,
						 rq->posted_index,
						 rq->rx_nb_hold);
		rq->rx_nb_hold = 0;
		rte_wmb();
		iowrite32_relaxed(rq->posted_index,
				  &rq->ctrl->posted_index);
	}

	return rx - rx_pkts;
}

static inline void enic_free_wq_bufs(struct vnic_wq *wq,
				     uint16_t completed_index)
{
	struct rte_mbuf *buf;
	struct rte_mbuf *m, *free[ENIC_MAX_WQ_DESCS];
	unsigned int nb_to_free, nb_free = 0, i;
	struct rte_mempool *pool;
	unsigned int tail_idx;
	unsigned int desc_count = wq->ring.desc_count;

	nb_to_free = enic_ring_sub(desc_count, wq->tail_idx, completed_index)
				   + 1;
	tail_idx = wq->tail_idx;
	pool = wq->bufs[tail_idx]->pool;
	for (i = 0; i < nb_to_free; i++) {
		buf = wq->bufs[tail_idx];
		m = rte_pktmbuf_prefree_seg(buf);
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
	uint16_t completed_index;

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
		if (!(ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			if (unlikely(m->pkt_len > ENIC_TX_MAX_PKT_SIZE)) {
				rte_errno = EINVAL;
				return i;
			}
		} else {
			uint16_t header_len;

			header_len = m->l2_len + m->l3_len + m->l4_len;
			if (m->tso_segsz + header_len > ENIC_TX_MAX_PKT_SIZE) {
				rte_errno = EINVAL;
				return i;
			}
		}

		if (ol_flags & wq->tx_offload_notsup_mask) {
			rte_errno = ENOTSUP;
			return i;
		}
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
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
	unsigned int desc_count;
	struct wq_enet_desc *descs, *desc_p, desc_tmp;
	uint16_t mss;
	uint8_t vlan_tag_insert;
	uint8_t eop, cq;
	uint64_t bus_addr;
	uint8_t offload_mode;
	uint16_t header_len;
	uint64_t tso;
	rte_atomic64_t *tx_oversized;

	enic_cleanup_wq(enic, wq);
	wq_desc_avail = vnic_wq_desc_avail(wq);
	head_idx = wq->head_idx;
	desc_count = wq->ring.desc_count;
	ol_flags_mask = RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK;
	tx_oversized = &enic->soft_stats.tx_oversized;

	nb_pkts = RTE_MIN(nb_pkts, ENIC_TX_XMIT_MAX);

	for (index = 0; index < nb_pkts; index++) {
		tx_pkt = *tx_pkts++;
		pkt_len = tx_pkt->pkt_len;
		data_len = tx_pkt->data_len;
		ol_flags = tx_pkt->ol_flags;
		nb_segs = tx_pkt->nb_segs;
		tso = ol_flags & RTE_MBUF_F_TX_TCP_SEG;

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
		vlan_tag_insert = !!(ol_flags & RTE_MBUF_F_TX_VLAN);
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
			if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
				header_len += tx_pkt->outer_l2_len +
					tx_pkt->outer_l3_len;
			}
		}

		if ((ol_flags & ol_flags_mask) && (header_len == 0)) {
			if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
				mss |= ENIC_CALC_IP_CKSUM;

			/* Nic uses just 1 bit for UDP and TCP */
			switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
			case RTE_MBUF_F_TX_TCP_CKSUM:
			case RTE_MBUF_F_TX_UDP_CKSUM:
				mss |= ENIC_CALC_TCP_UDP_CKSUM;
				break;
			}
		}
		wq->cq_pend++;
		cq = 0;
		if (eop && wq->cq_pend >= ENIC_WQ_CQ_THRESH) {
			cq = 1;
			wq->cq_pend = 0;
		}
		wq_enet_desc_enc(&desc_tmp, bus_addr, data_len, mss, header_len,
				 offload_mode, eop, cq, 0, vlan_tag_insert,
				 vlan_id, 0);

		*desc_p = desc_tmp;
		wq->bufs[head_idx] = tx_pkt;
		head_idx = enic_ring_incr(desc_count, head_idx);
		wq_desc_avail--;

		if (!eop) {
			for (tx_pkt = tx_pkt->next; tx_pkt; tx_pkt =
			    tx_pkt->next) {
				data_len = tx_pkt->data_len;

				wq->cq_pend++;
				cq = 0;
				if (tx_pkt->next == NULL) {
					eop = 1;
					if (wq->cq_pend >= ENIC_WQ_CQ_THRESH) {
						cq = 1;
						wq->cq_pend = 0;
					}
				}
				desc_p = descs + head_idx;
				bus_addr = (dma_addr_t)(tx_pkt->buf_iova
					   + tx_pkt->data_off);
				wq_enet_desc_enc((struct wq_enet_desc *)
						 &desc_tmp, bus_addr, data_len,
						 mss, 0, offload_mode, eop, cq,
						 0, vlan_tag_insert, vlan_id,
						 0);

				*desc_p = desc_tmp;
				wq->bufs[head_idx] = tx_pkt;
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

static void enqueue_simple_pkts(struct rte_mbuf **pkts,
				struct wq_enet_desc *desc,
				uint16_t n,
				struct enic *enic)
{
	struct rte_mbuf *p;
	uint16_t mss;

	while (n) {
		n--;
		p = *pkts++;
		desc->address = p->buf_iova + p->data_off;
		desc->length = p->pkt_len;
		/* VLAN insert */
		desc->vlan_tag = p->vlan_tci;
		desc->header_length_flags &=
			((1 << WQ_ENET_FLAGS_EOP_SHIFT) |
			 (1 << WQ_ENET_FLAGS_CQ_ENTRY_SHIFT));
		if (p->ol_flags & RTE_MBUF_F_TX_VLAN) {
			desc->header_length_flags |=
				1 << WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT;
		}
		/*
		 * Checksum offload. We use WQ_ENET_OFFLOAD_MODE_CSUM, which
		 * is 0, so no need to set offload_mode.
		 */
		mss = 0;
		if (p->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			mss |= ENIC_CALC_IP_CKSUM << WQ_ENET_MSS_SHIFT;
		if (p->ol_flags & RTE_MBUF_F_TX_L4_MASK)
			mss |= ENIC_CALC_TCP_UDP_CKSUM << WQ_ENET_MSS_SHIFT;
		desc->mss_loopback = mss;

		/*
		 * The app should not send oversized
		 * packets. tx_pkt_prepare includes a check as
		 * well. But some apps ignore the device max size and
		 * tx_pkt_prepare. Oversized packets cause WQ errors
		 * and the NIC ends up disabling the whole WQ. So
		 * truncate packets..
		 */
		if (unlikely(p->pkt_len > ENIC_TX_MAX_PKT_SIZE)) {
			desc->length = ENIC_TX_MAX_PKT_SIZE;
			rte_atomic64_inc(&enic->soft_stats.tx_oversized);
		}
		desc++;
	}
}

uint16_t enic_simple_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts)
{
	unsigned int head_idx, desc_count;
	struct wq_enet_desc *desc;
	struct vnic_wq *wq;
	struct enic *enic;
	uint16_t rem, n;

	wq = (struct vnic_wq *)tx_queue;
	enic = vnic_dev_priv(wq->vdev);
	enic_cleanup_wq(enic, wq);
	/* Will enqueue this many packets in this call */
	nb_pkts = RTE_MIN(nb_pkts, wq->ring.desc_avail);
	if (nb_pkts == 0)
		return 0;

	head_idx = wq->head_idx;
	desc_count = wq->ring.desc_count;

	/* Descriptors until the end of the ring */
	n = desc_count - head_idx;
	n = RTE_MIN(nb_pkts, n);

	/* Save mbuf pointers to free later */
	memcpy(wq->bufs + head_idx, tx_pkts, sizeof(struct rte_mbuf *) * n);

	/* Enqueue until the ring end */
	rem = nb_pkts - n;
	desc = ((struct wq_enet_desc *)wq->ring.descs) + head_idx;
	enqueue_simple_pkts(tx_pkts, desc, n, enic);

	/* Wrap to the start of the ring */
	if (rem) {
		tx_pkts += n;
		memcpy(wq->bufs, tx_pkts, sizeof(struct rte_mbuf *) * rem);
		desc = (struct wq_enet_desc *)wq->ring.descs;
		enqueue_simple_pkts(tx_pkts, desc, rem, enic);
	}
	rte_wmb();

	/* Update head_idx and desc_avail */
	wq->ring.desc_avail -= nb_pkts;
	head_idx += nb_pkts;
	if (head_idx >= desc_count)
		head_idx -= desc_count;
	wq->head_idx = head_idx;
	iowrite32_relaxed(head_idx, &wq->ctrl->posted_index);
	return nb_pkts;
}
