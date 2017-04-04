/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2013-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_common.h>
#include <rte_net.h>
#include "fm10k.h"
#include "base/fm10k_type.h"

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while (0)
#endif

#ifdef RTE_LIBRTE_FM10K_DEBUG_RX
static inline void dump_rxd(union fm10k_rx_desc *rxd)
{
	PMD_RX_LOG(DEBUG, "+----------------|----------------+");
	PMD_RX_LOG(DEBUG, "|     GLORT      | PKT HDR & TYPE |");
	PMD_RX_LOG(DEBUG, "|   0x%08x   |   0x%08x   |", rxd->d.glort,
			rxd->d.data);
	PMD_RX_LOG(DEBUG, "+----------------|----------------+");
	PMD_RX_LOG(DEBUG, "|   VLAN & LEN   |     STATUS     |");
	PMD_RX_LOG(DEBUG, "|   0x%08x   |   0x%08x   |", rxd->d.vlan_len,
			rxd->d.staterr);
	PMD_RX_LOG(DEBUG, "+----------------|----------------+");
	PMD_RX_LOG(DEBUG, "|    RESERVED    |    RSS_HASH    |");
	PMD_RX_LOG(DEBUG, "|   0x%08x   |   0x%08x   |", 0, rxd->d.rss);
	PMD_RX_LOG(DEBUG, "+----------------|----------------+");
	PMD_RX_LOG(DEBUG, "|            TIME TAG             |");
	PMD_RX_LOG(DEBUG, "|       0x%016"PRIx64"        |", rxd->q.timestamp);
	PMD_RX_LOG(DEBUG, "+----------------|----------------+");
}
#endif

#define FM10K_TX_OFFLOAD_MASK (  \
		PKT_TX_VLAN_PKT |        \
		PKT_TX_IP_CKSUM |        \
		PKT_TX_L4_MASK |         \
		PKT_TX_TCP_SEG)

#define FM10K_TX_OFFLOAD_NOTSUP_MASK \
		(PKT_TX_OFFLOAD_MASK ^ FM10K_TX_OFFLOAD_MASK)

/* @note: When this function is changed, make corresponding change to
 * fm10k_dev_supported_ptypes_get()
 */
static inline void
rx_desc_to_ol_flags(struct rte_mbuf *m, const union fm10k_rx_desc *d)
{
	static const uint32_t
		ptype_table[FM10K_RXD_PKTTYPE_MASK >> FM10K_RXD_PKTTYPE_SHIFT]
			__rte_cache_aligned = {
		[FM10K_PKTTYPE_OTHER] = RTE_PTYPE_L2_ETHER,
		[FM10K_PKTTYPE_IPV4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4,
		[FM10K_PKTTYPE_IPV4_EX] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT,
		[FM10K_PKTTYPE_IPV6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6,
		[FM10K_PKTTYPE_IPV6_EX] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT,
		[FM10K_PKTTYPE_IPV4 | FM10K_PKTTYPE_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
		[FM10K_PKTTYPE_IPV6 | FM10K_PKTTYPE_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
		[FM10K_PKTTYPE_IPV4 | FM10K_PKTTYPE_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
		[FM10K_PKTTYPE_IPV6 | FM10K_PKTTYPE_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	};

	m->packet_type = ptype_table[(d->w.pkt_info & FM10K_RXD_PKTTYPE_MASK)
						>> FM10K_RXD_PKTTYPE_SHIFT];

	if (d->w.pkt_info & FM10K_RXD_RSSTYPE_MASK)
		m->ol_flags |= PKT_RX_RSS_HASH;

	if (unlikely((d->d.staterr &
		(FM10K_RXD_STATUS_IPCS | FM10K_RXD_STATUS_IPE)) ==
		(FM10K_RXD_STATUS_IPCS | FM10K_RXD_STATUS_IPE)))
		m->ol_flags |= PKT_RX_IP_CKSUM_BAD;
	else
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD;

	if (unlikely((d->d.staterr &
		(FM10K_RXD_STATUS_L4CS | FM10K_RXD_STATUS_L4E)) ==
		(FM10K_RXD_STATUS_L4CS | FM10K_RXD_STATUS_L4E)))
		m->ol_flags |= PKT_RX_L4_CKSUM_BAD;
	else
		m->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
}

uint16_t
fm10k_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	struct rte_mbuf *mbuf;
	union fm10k_rx_desc desc;
	struct fm10k_rx_queue *q = rx_queue;
	uint16_t count = 0;
	int alloc = 0;
	uint16_t next_dd;
	int ret;

	next_dd = q->next_dd;

	nb_pkts = RTE_MIN(nb_pkts, q->alloc_thresh);
	for (count = 0; count < nb_pkts; ++count) {
		if (!(q->hw_ring[next_dd].d.staterr & FM10K_RXD_STATUS_DD))
			break;
		mbuf = q->sw_ring[next_dd];
		desc = q->hw_ring[next_dd];
#ifdef RTE_LIBRTE_FM10K_DEBUG_RX
		dump_rxd(&desc);
#endif
		rte_pktmbuf_pkt_len(mbuf) = desc.w.length;
		rte_pktmbuf_data_len(mbuf) = desc.w.length;

		mbuf->ol_flags = 0;
#ifdef RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE
		rx_desc_to_ol_flags(mbuf, &desc);
#endif

		mbuf->hash.rss = desc.d.rss;
		/**
		 * Packets in fm10k device always carry at least one VLAN tag.
		 * For those packets coming in without VLAN tag,
		 * the port default VLAN tag will be used.
		 * So, always PKT_RX_VLAN_PKT flag is set and vlan_tci
		 * is valid for each RX packet's mbuf.
		 */
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;
		mbuf->vlan_tci = desc.w.vlan;
		/**
		 * mbuf->vlan_tci_outer is an idle field in fm10k driver,
		 * so it can be selected to store sglort value.
		 */
		if (q->rx_ftag_en)
			mbuf->vlan_tci_outer = rte_le_to_cpu_16(desc.w.sglort);

		rx_pkts[count] = mbuf;
		if (++next_dd == q->nb_desc) {
			next_dd = 0;
			alloc = 1;
		}

		/* Prefetch next mbuf while processing current one. */
		rte_prefetch0(q->sw_ring[next_dd]);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((next_dd & 0x3) == 0) {
			rte_prefetch0(&q->hw_ring[next_dd]);
			rte_prefetch0(&q->sw_ring[next_dd]);
		}
	}

	q->next_dd = next_dd;

	if ((q->next_dd > q->next_trigger) || (alloc == 1)) {
		ret = rte_mempool_get_bulk(q->mp,
					(void **)&q->sw_ring[q->next_alloc],
					q->alloc_thresh);

		if (unlikely(ret != 0)) {
			uint8_t port = q->port_id;
			PMD_RX_LOG(ERR, "Failed to alloc mbuf");
			/*
			 * Need to restore next_dd if we cannot allocate new
			 * buffers to replenish the old ones.
			 */
			q->next_dd = (q->next_dd + q->nb_desc - count) %
								q->nb_desc;
			rte_eth_devices[port].data->rx_mbuf_alloc_failed++;
			return 0;
		}

		for (; q->next_alloc <= q->next_trigger; ++q->next_alloc) {
			mbuf = q->sw_ring[q->next_alloc];

			/* setup static mbuf fields */
			fm10k_pktmbuf_reset(mbuf, q->port_id);

			/* write descriptor */
			desc.q.pkt_addr = MBUF_DMA_ADDR_DEFAULT(mbuf);
			desc.q.hdr_addr = MBUF_DMA_ADDR_DEFAULT(mbuf);
			q->hw_ring[q->next_alloc] = desc;
		}
		FM10K_PCI_REG_WRITE(q->tail_ptr, q->next_trigger);
		q->next_trigger += q->alloc_thresh;
		if (q->next_trigger >= q->nb_desc) {
			q->next_trigger = q->alloc_thresh - 1;
			q->next_alloc = 0;
		}
	}

	return count;
}

uint16_t
fm10k_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	struct rte_mbuf *mbuf;
	union fm10k_rx_desc desc;
	struct fm10k_rx_queue *q = rx_queue;
	uint16_t count = 0;
	uint16_t nb_rcv, nb_seg;
	int alloc = 0;
	uint16_t next_dd;
	struct rte_mbuf *first_seg = q->pkt_first_seg;
	struct rte_mbuf *last_seg = q->pkt_last_seg;
	int ret;

	next_dd = q->next_dd;
	nb_rcv = 0;

	nb_seg = RTE_MIN(nb_pkts, q->alloc_thresh);
	for (count = 0; count < nb_seg; count++) {
		if (!(q->hw_ring[next_dd].d.staterr & FM10K_RXD_STATUS_DD))
			break;
		mbuf = q->sw_ring[next_dd];
		desc = q->hw_ring[next_dd];
#ifdef RTE_LIBRTE_FM10K_DEBUG_RX
		dump_rxd(&desc);
#endif

		if (++next_dd == q->nb_desc) {
			next_dd = 0;
			alloc = 1;
		}

		/* Prefetch next mbuf while processing current one. */
		rte_prefetch0(q->sw_ring[next_dd]);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((next_dd & 0x3) == 0) {
			rte_prefetch0(&q->hw_ring[next_dd]);
			rte_prefetch0(&q->sw_ring[next_dd]);
		}

		/* Fill data length */
		rte_pktmbuf_data_len(mbuf) = desc.w.length;

		/*
		 * If this is the first buffer of the received packet,
		 * set the pointer to the first mbuf of the packet and
		 * initialize its context.
		 * Otherwise, update the total length and the number of segments
		 * of the current scattered packet, and update the pointer to
		 * the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = mbuf;
			first_seg->pkt_len = desc.w.length;
		} else {
			first_seg->pkt_len =
					(uint16_t)(first_seg->pkt_len +
					rte_pktmbuf_data_len(mbuf));
			first_seg->nb_segs++;
			last_seg->next = mbuf;
		}

		/*
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(desc.d.staterr & FM10K_RXD_STATUS_EOP)) {
			last_seg = mbuf;
			continue;
		}

		first_seg->ol_flags = 0;
#ifdef RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE
		rx_desc_to_ol_flags(first_seg, &desc);
#endif
		first_seg->hash.rss = desc.d.rss;
		/**
		 * Packets in fm10k device always carry at least one VLAN tag.
		 * For those packets coming in without VLAN tag,
		 * the port default VLAN tag will be used.
		 * So, always PKT_RX_VLAN_PKT flag is set and vlan_tci
		 * is valid for each RX packet's mbuf.
		 */
		first_seg->ol_flags |= PKT_RX_VLAN_PKT;
		first_seg->vlan_tci = desc.w.vlan;
		/**
		 * mbuf->vlan_tci_outer is an idle field in fm10k driver,
		 * so it can be selected to store sglort value.
		 */
		if (q->rx_ftag_en)
			first_seg->vlan_tci_outer =
				rte_le_to_cpu_16(desc.w.sglort);

		/* Prefetch data of first segment, if configured to do so. */
		rte_packet_prefetch((char *)first_seg->buf_addr +
			first_seg->data_off);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rcv++] = first_seg;

		/*
		 * Setup receipt context for a new packet.
		 */
		first_seg = NULL;
	}

	q->next_dd = next_dd;

	if ((q->next_dd > q->next_trigger) || (alloc == 1)) {
		ret = rte_mempool_get_bulk(q->mp,
					(void **)&q->sw_ring[q->next_alloc],
					q->alloc_thresh);

		if (unlikely(ret != 0)) {
			uint8_t port = q->port_id;
			PMD_RX_LOG(ERR, "Failed to alloc mbuf");
			/*
			 * Need to restore next_dd if we cannot allocate new
			 * buffers to replenish the old ones.
			 */
			q->next_dd = (q->next_dd + q->nb_desc - count) %
								q->nb_desc;
			rte_eth_devices[port].data->rx_mbuf_alloc_failed++;
			return 0;
		}

		for (; q->next_alloc <= q->next_trigger; ++q->next_alloc) {
			mbuf = q->sw_ring[q->next_alloc];

			/* setup static mbuf fields */
			fm10k_pktmbuf_reset(mbuf, q->port_id);

			/* write descriptor */
			desc.q.pkt_addr = MBUF_DMA_ADDR_DEFAULT(mbuf);
			desc.q.hdr_addr = MBUF_DMA_ADDR_DEFAULT(mbuf);
			q->hw_ring[q->next_alloc] = desc;
		}
		FM10K_PCI_REG_WRITE(q->tail_ptr, q->next_trigger);
		q->next_trigger += q->alloc_thresh;
		if (q->next_trigger >= q->nb_desc) {
			q->next_trigger = q->alloc_thresh - 1;
			q->next_alloc = 0;
		}
	}

	q->pkt_first_seg = first_seg;
	q->pkt_last_seg = last_seg;

	return nb_rcv;
}

int
fm10k_dev_rx_descriptor_done(void *rx_queue, uint16_t offset)
{
	volatile union fm10k_rx_desc *rxdp;
	struct fm10k_rx_queue *rxq = rx_queue;
	uint16_t desc;
	int ret;

	if (unlikely(offset >= rxq->nb_desc)) {
		PMD_DRV_LOG(ERR, "Invalid RX descriptor offset %u", offset);
		return 0;
	}

	desc = rxq->next_dd + offset;
	if (desc >= rxq->nb_desc)
		desc -= rxq->nb_desc;

	rxdp = &rxq->hw_ring[desc];

	ret = !!(rxdp->w.status &
			rte_cpu_to_le_16(FM10K_RXD_STATUS_DD));

	return ret;
}

/*
 * Free multiple TX mbuf at a time if they are in the same pool
 *
 * @txep: software desc ring index that starts to free
 * @num: number of descs to free
 *
 */
static inline void tx_free_bulk_mbuf(struct rte_mbuf **txep, int num)
{
	struct rte_mbuf *m, *free[RTE_FM10K_TX_MAX_FREE_BUF_SZ];
	int i;
	int nb_free = 0;

	if (unlikely(num == 0))
		return;

	m = rte_pktmbuf_prefree_seg(txep[0]);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < num; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i]);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool))
					free[nb_free++] = m;
				else {
					rte_mempool_put_bulk(free[0]->pool,
							(void *)free, nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
			txep[i] = NULL;
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
	} else {
		for (i = 1; i < num; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i]);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
			txep[i] = NULL;
		}
	}
}

static inline void tx_free_descriptors(struct fm10k_tx_queue *q)
{
	uint16_t next_rs, count = 0;

	next_rs = fifo_peek(&q->rs_tracker);
	if (!(q->hw_ring[next_rs].flags & FM10K_TXD_FLAG_DONE))
		return;

	/* the DONE flag is set on this descriptor so remove the ID
	 * from the RS bit tracker and free the buffers */
	fifo_remove(&q->rs_tracker);

	/* wrap around? if so, free buffers from last_free up to but NOT
	 * including nb_desc */
	if (q->last_free > next_rs) {
		count = q->nb_desc - q->last_free;
		tx_free_bulk_mbuf(&q->sw_ring[q->last_free], count);
		q->last_free = 0;
	}

	/* adjust free descriptor count before the next loop */
	q->nb_free += count + (next_rs + 1 - q->last_free);

	/* free buffers from last_free, up to and including next_rs */
	if (q->last_free <= next_rs) {
		count = next_rs - q->last_free + 1;
		tx_free_bulk_mbuf(&q->sw_ring[q->last_free], count);
		q->last_free += count;
	}

	if (q->last_free == q->nb_desc)
		q->last_free = 0;
}

static inline void tx_xmit_pkt(struct fm10k_tx_queue *q, struct rte_mbuf *mb)
{
	uint16_t last_id;
	uint8_t flags, hdrlen;

	/* always set the LAST flag on the last descriptor used to
	 * transmit the packet */
	flags = FM10K_TXD_FLAG_LAST;
	last_id = q->next_free + mb->nb_segs - 1;
	if (last_id >= q->nb_desc)
		last_id = last_id - q->nb_desc;

	/* but only set the RS flag on the last descriptor if rs_thresh
	 * descriptors will be used since the RS flag was last set */
	if ((q->nb_used + mb->nb_segs) >= q->rs_thresh) {
		flags |= FM10K_TXD_FLAG_RS;
		fifo_insert(&q->rs_tracker, last_id);
		q->nb_used = 0;
	} else {
		q->nb_used = q->nb_used + mb->nb_segs;
	}

	q->nb_free -= mb->nb_segs;

	q->hw_ring[q->next_free].flags = 0;
	if (q->tx_ftag_en)
		q->hw_ring[q->next_free].flags |= FM10K_TXD_FLAG_FTAG;
	/* set checksum flags on first descriptor of packet. SCTP checksum
	 * offload is not supported, but we do not explicitly check for this
	 * case in favor of greatly simplified processing. */
	if (mb->ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK | PKT_TX_TCP_SEG))
		q->hw_ring[q->next_free].flags |= FM10K_TXD_FLAG_CSUM;

	/* set vlan if requested */
	if (mb->ol_flags & PKT_TX_VLAN_PKT)
		q->hw_ring[q->next_free].vlan = mb->vlan_tci;

	q->sw_ring[q->next_free] = mb;
	q->hw_ring[q->next_free].buffer_addr =
			rte_cpu_to_le_64(MBUF_DMA_ADDR(mb));
	q->hw_ring[q->next_free].buflen =
			rte_cpu_to_le_16(rte_pktmbuf_data_len(mb));

	if (mb->ol_flags & PKT_TX_TCP_SEG) {
		hdrlen = mb->outer_l2_len + mb->outer_l3_len + mb->l2_len +
			mb->l3_len + mb->l4_len;
		if (q->hw_ring[q->next_free].flags & FM10K_TXD_FLAG_FTAG)
			hdrlen += sizeof(struct fm10k_ftag);

		if (likely((hdrlen >= FM10K_TSO_MIN_HEADERLEN) &&
				(hdrlen <= FM10K_TSO_MAX_HEADERLEN) &&
				(mb->tso_segsz >= FM10K_TSO_MINMSS))) {
			q->hw_ring[q->next_free].mss = mb->tso_segsz;
			q->hw_ring[q->next_free].hdrlen = hdrlen;
		}
	}

	if (++q->next_free == q->nb_desc)
		q->next_free = 0;

	/* fill up the rings */
	for (mb = mb->next; mb != NULL; mb = mb->next) {
		q->sw_ring[q->next_free] = mb;
		q->hw_ring[q->next_free].buffer_addr =
				rte_cpu_to_le_64(MBUF_DMA_ADDR(mb));
		q->hw_ring[q->next_free].buflen =
				rte_cpu_to_le_16(rte_pktmbuf_data_len(mb));
		q->hw_ring[q->next_free].flags = 0;
		if (++q->next_free == q->nb_desc)
			q->next_free = 0;
	}

	q->hw_ring[last_id].flags |= flags;
}

uint16_t
fm10k_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	struct fm10k_tx_queue *q = tx_queue;
	struct rte_mbuf *mb;
	uint16_t count;

	for (count = 0; count < nb_pkts; ++count) {
		mb = tx_pkts[count];

		/* running low on descriptors? try to free some... */
		if (q->nb_free < q->free_thresh)
			tx_free_descriptors(q);

		/* make sure there are enough free descriptors to transmit the
		 * entire packet before doing anything */
		if (q->nb_free < mb->nb_segs)
			break;

		/* sanity check to make sure the mbuf is valid */
		if ((mb->nb_segs == 0) ||
		    ((mb->nb_segs > 1) && (mb->next == NULL)))
			break;

		/* process the packet */
		tx_xmit_pkt(q, mb);
	}

	/* update the tail pointer if any packets were processed */
	if (likely(count > 0))
		FM10K_PCI_REG_WRITE(q->tail_ptr, q->next_free);

	return count;
}

uint16_t
fm10k_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i, ret;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];

		if ((m->ol_flags & PKT_TX_TCP_SEG) &&
				(m->tso_segsz < FM10K_TSO_MINMSS)) {
			rte_errno = -EINVAL;
			return i;
		}

		if (m->ol_flags & FM10K_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
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
