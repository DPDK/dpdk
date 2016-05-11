/*
 * Copyright 2008-2014 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>

#include "enic_compat.h"
#include "rq_enet_desc.h"
#include "enic.h"

#define RTE_PMD_USE_PREFETCH

#ifdef RTE_PMD_USE_PREFETCH
/*
 * Prefetch a cache line into all cache levels.
 */
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
	return(le16_to_cpu(crd->bytes_written_flags) &
		~CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK);
}

static inline uint8_t
enic_cq_rx_desc_packet_error(uint16_t bwflags)
{
	return((bwflags & CQ_ENET_RQ_DESC_FLAGS_TRUNCATED) ==
		CQ_ENET_RQ_DESC_FLAGS_TRUNCATED);
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
	return ((le16_to_cpu(cqrd->q_number_rss_type_flags) &
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC) ==
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC);
}

static inline uint8_t
enic_cq_rx_desc_ipv4_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return ((cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK);
}

static inline uint8_t
enic_cq_rx_desc_tcp_udp_csum_ok(struct cq_enet_rq_desc *cqrd)
{
	return((cqrd->flags & CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK) ==
		CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK);
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
enic_cq_rx_to_pkt_err_flags(struct cq_desc *cqd, uint64_t *pkt_err_flags_out)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t bwflags;
	int ret = 0;
	uint64_t pkt_err_flags = 0;

	bwflags = enic_cq_rx_desc_bwflags(cqrd);
	if (unlikely(enic_cq_rx_desc_packet_error(bwflags))) {
		pkt_err_flags = PKT_RX_MAC_ERR;
		ret = 1;
	}
	*pkt_err_flags_out = pkt_err_flags;
	return ret;
}

/*
 * Lookup table to translate RX CQ flags to mbuf flags.
 */
static inline uint32_t
enic_cq_rx_flags_to_pkt_type(struct cq_desc *cqd)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint8_t cqrd_flags = cqrd->flags;
	static const uint32_t cq_type_table[128] __rte_cache_aligned = {
		[32] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4,
		[34] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
			| RTE_PTYPE_L4_UDP,
		[36] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
			| RTE_PTYPE_L4_TCP,
		[96] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4
			| RTE_PTYPE_L4_FRAG,
		[16] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6,
		[18] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6
			| RTE_PTYPE_L4_UDP,
		[20] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6
			| RTE_PTYPE_L4_TCP,
		[80] =  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6
			| RTE_PTYPE_L4_FRAG,
		/* All others reserved */
	};
	cqrd_flags &= CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT
		| CQ_ENET_RQ_DESC_FLAGS_IPV4 | CQ_ENET_RQ_DESC_FLAGS_IPV6
		| CQ_ENET_RQ_DESC_FLAGS_TCP | CQ_ENET_RQ_DESC_FLAGS_UDP;
	return cq_type_table[cqrd_flags];
}

static inline void
enic_cq_rx_to_pkt_flags(struct cq_desc *cqd, struct rte_mbuf *mbuf)
{
	struct cq_enet_rq_desc *cqrd = (struct cq_enet_rq_desc *)cqd;
	uint16_t ciflags, bwflags, pkt_flags = 0;
	ciflags = enic_cq_rx_desc_ciflags(cqrd);
	bwflags = enic_cq_rx_desc_bwflags(cqrd);

	mbuf->ol_flags = 0;

	/* flags are meaningless if !EOP */
	if (unlikely(!enic_cq_rx_desc_eop(ciflags)))
		goto mbuf_flags_done;

	/* VLAN stripping */
	if (bwflags & CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED) {
		pkt_flags |= PKT_RX_VLAN_PKT;
		mbuf->vlan_tci = enic_cq_rx_desc_vlan(cqrd);
	} else {
		mbuf->vlan_tci = 0;
	}

	/* RSS flag */
	if (enic_cq_rx_desc_rss_type(cqrd)) {
		pkt_flags |= PKT_RX_RSS_HASH;
		mbuf->hash.rss = enic_cq_rx_desc_rss_hash(cqrd);
	}

	/* checksum flags */
	if (!enic_cq_rx_desc_csum_not_calc(cqrd) &&
		(mbuf->packet_type & RTE_PTYPE_L3_IPV4)) {
		if (unlikely(!enic_cq_rx_desc_ipv4_csum_ok(cqrd)))
			pkt_flags |= PKT_RX_IP_CKSUM_BAD;
		if (mbuf->packet_type & (RTE_PTYPE_L4_UDP | RTE_PTYPE_L4_TCP)) {
			if (unlikely(!enic_cq_rx_desc_tcp_udp_csum_ok(cqrd)))
				pkt_flags |= PKT_RX_L4_CKSUM_BAD;
		}
	}

 mbuf_flags_done:
	mbuf->ol_flags = pkt_flags;
}

static inline uint32_t
enic_ring_add(uint32_t n_descriptors, uint32_t i0, uint32_t i1)
{
	uint32_t d = i0 + i1;
	RTE_ASSERT(i0 < n_descriptors);
	RTE_ASSERT(i1 < n_descriptors);
	d -= (d >= n_descriptors) ? n_descriptors : 0;
	return d;
}


uint16_t
enic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	       uint16_t nb_pkts)
{
	struct vnic_rq *rq = rx_queue;
	struct enic *enic = vnic_dev_priv(rq->vdev);
	unsigned int rx_id;
	struct rte_mbuf *nmb, *rxmb;
	uint16_t nb_rx = 0;
	uint16_t nb_hold;
	struct vnic_cq *cq;
	volatile struct cq_desc *cqd_ptr;
	uint8_t color;

	cq = &enic->cq[enic_cq_rq(enic, rq->index)];
	rx_id = cq->to_clean;		/* index of cqd, rqd, mbuf_table */
	cqd_ptr = (struct cq_desc *)(cq->ring.descs) + rx_id;

	nb_hold = rq->rx_nb_hold;	/* mbufs held by software */

	while (nb_rx < nb_pkts) {
		volatile struct rq_enet_desc *rqd_ptr;
		dma_addr_t dma_addr;
		struct cq_desc cqd;
		uint64_t ol_err_flags;
		uint8_t packet_error;

		/* Check for pkts available */
		color = (cqd_ptr->type_color >> CQ_DESC_COLOR_SHIFT)
			& CQ_DESC_COLOR_MASK;
		if (color == cq->last_color)
			break;

		/* Get the cq descriptor and rq pointer */
		cqd = *cqd_ptr;
		rqd_ptr = (struct rq_enet_desc *)(rq->ring.descs) + rx_id;

		/* allocate a new mbuf */
		nmb = rte_mbuf_raw_alloc(rq->mp);
		if (nmb == NULL) {
			dev_err(enic, "RX mbuf alloc failed port=%u qid=%u",
			enic->port_id, (unsigned)rq->index);
			rte_eth_devices[enic->port_id].
					data->rx_mbuf_alloc_failed++;
			break;
		}

		/* A packet error means descriptor and data are untrusted */
		packet_error = enic_cq_rx_to_pkt_err_flags(&cqd, &ol_err_flags);

		/* Get the mbuf to return and replace with one just allocated */
		rxmb = rq->mbuf_ring[rx_id];
		rq->mbuf_ring[rx_id] = nmb;

		/* Increment cqd, rqd, mbuf_table index */
		rx_id++;
		if (unlikely(rx_id == rq->ring.desc_count)) {
			rx_id = 0;
			cq->last_color = cq->last_color ? 0 : 1;
		}

		/* Prefetch next mbuf & desc while processing current one */
		cqd_ptr = (struct cq_desc *)(cq->ring.descs) + rx_id;
		rte_enic_prefetch(cqd_ptr);
		rte_enic_prefetch(rq->mbuf_ring[rx_id]);
		rte_enic_prefetch((struct rq_enet_desc *)(rq->ring.descs)
				 + rx_id);

		/* Push descriptor for newly allocated mbuf */
		dma_addr = (dma_addr_t)(nmb->buf_physaddr
			   + RTE_PKTMBUF_HEADROOM);
		rqd_ptr->address = rte_cpu_to_le_64(dma_addr);
		rqd_ptr->length_type = cpu_to_le16(nmb->buf_len
				       - RTE_PKTMBUF_HEADROOM);

		/* Fill in the rest of the mbuf */
		rxmb->data_off = RTE_PKTMBUF_HEADROOM;
		rxmb->nb_segs = 1;
		rxmb->next = NULL;
		rxmb->port = enic->port_id;
		if (!packet_error) {
			rxmb->pkt_len = enic_cq_rx_desc_n_bytes(&cqd);
			rxmb->packet_type = enic_cq_rx_flags_to_pkt_type(&cqd);
			enic_cq_rx_to_pkt_flags(&cqd, rxmb);
		} else {
			rxmb->pkt_len = 0;
			rxmb->packet_type = 0;
			rxmb->ol_flags = 0;
		}
		rxmb->data_len = rxmb->pkt_len;

		/* prefetch mbuf data for caller */
		rte_packet_prefetch(RTE_PTR_ADD(rxmb->buf_addr,
				    RTE_PKTMBUF_HEADROOM));

		/* store the mbuf address into the next entry of the array */
		rx_pkts[nb_rx++] = rxmb;
	}

	nb_hold += nb_rx;
	cq->to_clean = rx_id;

	if (nb_hold > rq->rx_free_thresh) {
		rq->posted_index = enic_ring_add(rq->ring.desc_count,
				rq->posted_index, nb_hold);
		nb_hold = 0;
		rte_mb();
		iowrite32(rq->posted_index, &rq->ctrl->posted_index);
	}

	rq->rx_nb_hold = nb_hold;

	return nb_rx;
}
