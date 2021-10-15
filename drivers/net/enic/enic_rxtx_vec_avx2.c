/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2018 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_vect.h>

#include "enic_compat.h"
#include "rq_enet_desc.h"
#include "enic.h"
#include "enic_rxtx_common.h"

#include <x86intrin.h>

static struct rte_mbuf *
rx_one(struct cq_enet_rq_desc *cqd, struct rte_mbuf *mb, struct enic *enic)
{
	bool tnl;

	*(uint64_t *)&mb->rearm_data = enic->mbuf_initializer;
	mb->data_len = cqd->bytes_written_flags &
		CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
	mb->pkt_len = mb->data_len;
	tnl = enic->overlay_offload && (cqd->completed_index_flags &
					CQ_ENET_RQ_DESC_FLAGS_FCOE) != 0;
	mb->packet_type =
		enic_cq_rx_flags_to_pkt_type((struct cq_desc *)cqd, tnl);
	enic_cq_rx_to_pkt_flags((struct cq_desc *)cqd, mb);
	/* Wipe the outer types set by enic_cq_rx_flags_to_pkt_type() */
	if (tnl) {
		mb->packet_type &= ~(RTE_PTYPE_L3_MASK |
				     RTE_PTYPE_L4_MASK);
	}
	return mb;
}

static uint16_t
enic_noscatter_vec_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	struct rte_mbuf **rx, **rxmb;
	uint16_t cq_idx, nb_rx, max_rx;
	struct cq_enet_rq_desc *cqd;
	struct rq_enet_desc *rqd;
	struct vnic_cq *cq;
	struct vnic_rq *rq;
	struct enic *enic;
	uint8_t color;

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

	rxmb = rq->mbuf_ring + cq_idx;
	color = cq->last_color;
	cqd = (struct cq_enet_rq_desc *)(cq->ring.descs) + cq_idx;
	rx = rx_pkts;
	if (max_rx == 0 ||
	    (cqd->type_color & CQ_DESC_COLOR_MASK_NOSHIFT) == color)
		return 0;

	/* Step 1: Process one packet to do aligned 256-bit load below */
	if (cq_idx & 0x1) {
		if (unlikely(cqd->bytes_written_flags &
			     CQ_ENET_RQ_DESC_FLAGS_TRUNCATED)) {
			rte_pktmbuf_free(*rxmb++);
			rte_atomic64_inc(&enic->soft_stats.rx_packet_errors);
		} else {
			*rx++ = rx_one(cqd, *rxmb++, enic);
		}
		cqd++;
		max_rx--;
	}

	const __m256i mask =
		_mm256_set_epi8(/* Second descriptor */
			0xff, /* type_color */
			(CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT |
			 CQ_ENET_RQ_DESC_FLAGS_IPV4 |
			 CQ_ENET_RQ_DESC_FLAGS_IPV6 |
			 CQ_ENET_RQ_DESC_FLAGS_TCP |
			 CQ_ENET_RQ_DESC_FLAGS_UDP), /* flags */
			0, 0, /* checksum_fcoe */
			0xff, 0xff, /* vlan */
			0x3f, 0xff, /* bytes_written_flags */
			0xff, 0xff, 0xff, 0xff, /* rss_hash */
			0xff, 0xff, /* q_number_rss_type_flags */
			0, 0, /* completed_index_flags */
			/* First descriptor */
			0xff, /* type_color */
			(CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT |
			 CQ_ENET_RQ_DESC_FLAGS_IPV4 |
			 CQ_ENET_RQ_DESC_FLAGS_IPV6 |
			 CQ_ENET_RQ_DESC_FLAGS_TCP |
			 CQ_ENET_RQ_DESC_FLAGS_UDP), /* flags */
			0, 0, /* checksum_fcoe */
			0xff, 0xff, /* vlan */
			0x3f, 0xff, /* bytes_written_flags */
			0xff, 0xff, 0xff, 0xff, /* rss_hash */
			0xff, 0xff, /* q_number_rss_type_flags */
			0, 0 /* completed_index_flags */
			);
	const __m256i shuffle_mask =
		_mm256_set_epi8(/* Second descriptor */
			7, 6, 5, 4,             /* rss = rss_hash */
			11, 10,                 /* vlan_tci = vlan */
			9, 8,                   /* data_len = bytes_written */
			0x80, 0x80, 9, 8,       /* pkt_len = bytes_written */
			0x80, 0x80, 0x80, 0x80, /* packet_type = 0 */
			/* First descriptor */
			7, 6, 5, 4,             /* rss = rss_hash */
			11, 10,                 /* vlan_tci = vlan */
			9, 8,                   /* data_len = bytes_written */
			0x80, 0x80, 9, 8,       /* pkt_len = bytes_written */
			0x80, 0x80, 0x80, 0x80  /* packet_type = 0 */
			);
	/* Used to collect 8 flags from 8 desc into one register */
	const __m256i flags_shuffle_mask =
		_mm256_set_epi8(/* Second descriptor */
			1, 3, 9, 14,
			1, 3, 9, 14,
			1, 3, 9, 14,
			1, 3, 9, 14,
			/* First descriptor */
			1, 3, 9, 14,
			1, 3, 9, 14,
			1, 3, 9, 14,
			/*
			 * Byte 3: upper byte of completed_index_flags
			 *         bit 5 = fcoe (tunnel)
			 * Byte 2: upper byte of q_number_rss_type_flags
			 *         bits 2,3,4,5 = rss type
			 *         bit 6 = csum_not_calc
			 * Byte 1: upper byte of bytes_written_flags
			 *         bit 6 = truncated
			 *         bit 7 = vlan stripped
			 * Byte 0: flags
			 */
			1, 3, 9, 14
			);
	/* Used to collect 8 VLAN IDs from 8 desc into one register */
	const __m256i vlan_shuffle_mask =
		_mm256_set_epi8(/* Second descriptor */
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10,
			/* First descriptor */
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10,
			0x80, 0x80, 11, 10);
	/* RTE_MBUF_F_RX_RSS_HASH is 1<<1 so fits in 8-bit integer */
	const __m256i rss_shuffle =
		_mm256_set_epi8(/* second 128 bits */
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			0, /* rss_types = 0 */
			/* first 128 bits */
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
			0 /* rss_types = 0 */);
	/*
	 * VLAN offload flags.
	 * shuffle index:
	 * vlan_stripped => bit 0
	 * vlan_id == 0  => bit 1
	 */
	const __m256i vlan_shuffle =
		_mm256_set_epi32(0, 0, 0, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, RTE_MBUF_F_RX_VLAN);
	/* Use the same shuffle index as vlan_shuffle */
	const __m256i vlan_ptype_shuffle =
		_mm256_set_epi32(0, 0, 0, 0,
				 RTE_PTYPE_L2_ETHER,
				 RTE_PTYPE_L2_ETHER,
				 RTE_PTYPE_L2_ETHER,
				 RTE_PTYPE_L2_ETHER_VLAN);
	/*
	 * CKSUM flags. Shift right so they fit int 8-bit integers.
	 * shuffle index:
	 * ipv4_csum_ok    => bit 3
	 * ip4             => bit 2
	 * tcp_or_udp      => bit 1
	 * tcp_udp_csum_ok => bit 0
	 */
	const __m256i csum_shuffle =
		_mm256_set_epi8(/* second 128 bits */
			/* 1111 ip4+ip4_ok+l4+l4_ok */
			((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
			/* 1110 ip4_ok+ip4+l4+!l4_ok */
			((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1), /* 1101 ip4+ip4_ok */
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1), /* 1100 ip4_ok+ip4 */
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1), /* 1011 l4+l4_ok */
			(RTE_MBUF_F_RX_L4_CKSUM_BAD >> 1),  /* 1010 l4+!l4_ok */
			0, /* 1001 */
			0, /* 1000 */
			/* 0111 !ip4_ok+ip4+l4+l4_ok */
			((RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
			/* 0110 !ip4_ok+ip4+l4+!l4_ok */
			((RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1),  /* 0101 !ip4_ok+ip4 */
			(RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1),  /* 0100 !ip4_ok+ip4 */
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1), /* 0011 l4+l4_ok */
			(RTE_MBUF_F_RX_L4_CKSUM_BAD >> 1),  /* 0010 l4+!l4_ok */
			0, /* 0001 */
			0, /* 0000 */
			/* first 128 bits */
			((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1),
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1),
			(RTE_MBUF_F_RX_L4_CKSUM_BAD >> 1),
			0, 0,
			((RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1),
			(RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1),
			(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1),
			(RTE_MBUF_F_RX_L4_CKSUM_BAD >> 1),
			0, 0);
	/*
	 * Non-fragment PTYPEs.
	 * Shuffle 4-bit index:
	 * ip6 => bit 0
	 * ip4 => bit 1
	 * udp => bit 2
	 * tcp => bit 3
	 *   bit
	 * 3 2 1 0
	 * -------
	 * 0 0 0 0 unknown
	 * 0 0 0 1 ip6 | nonfrag
	 * 0 0 1 0 ip4 | nonfrag
	 * 0 0 1 1 unknown
	 * 0 1 0 0 unknown
	 * 0 1 0 1 ip6 | udp
	 * 0 1 1 0 ip4 | udp
	 * 0 1 1 1 unknown
	 * 1 0 0 0 unknown
	 * 1 0 0 1 ip6 | tcp
	 * 1 0 1 0 ip4 | tcp
	 * 1 0 1 1 unknown
	 * 1 1 0 0 unknown
	 * 1 1 0 1 unknown
	 * 1 1 1 0 unknown
	 * 1 1 1 1 unknown
	 *
	 * PTYPEs do not fit in 8 bits, so shift right 4..
	 */
	const __m256i nonfrag_ptype_shuffle =
		_mm256_set_epi8(/* second 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_NONFRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_NONFRAG) >> 4,
			RTE_PTYPE_UNKNOWN,
			/* first 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_TCP) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_TCP) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN | RTE_PTYPE_L4_UDP) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN | RTE_PTYPE_L4_UDP) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_NONFRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_NONFRAG) >> 4,
			RTE_PTYPE_UNKNOWN);
	/* Fragment PTYPEs. Use the same shuffle index as above. */
	const __m256i frag_ptype_shuffle =
		_mm256_set_epi8(/* second 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN,
			/* first 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			(RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			(RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			 RTE_PTYPE_L4_FRAG) >> 4,
			RTE_PTYPE_UNKNOWN);
	/*
	 * Tunnel PTYPEs. Use the same shuffle index as above.
	 * L4 types are not part of this table. They come from non-tunnel
	 * types above.
	 */
	const __m256i tnl_l3_ptype_shuffle =
		_mm256_set_epi8(/* second 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN,
			/* first 128 bits */
			RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN, RTE_PTYPE_UNKNOWN,
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN >> 16,
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN >> 16,
			RTE_PTYPE_UNKNOWN);

	const __m256i mbuf_init = _mm256_set_epi64x(0, enic->mbuf_initializer,
						    0, enic->mbuf_initializer);

	/*
	 * --- cq desc fields ---    offset
	 * completed_index_flags    - 0   use: fcoe
	 * q_number_rss_type_flags  - 2   use: rss types, csum_not_calc
	 * rss_hash                 - 4   ==> mbuf.hash.rss
	 * bytes_written_flags      - 8   ==> mbuf.pkt_len,data_len
	 *                                use: truncated, vlan_stripped
	 * vlan                     - 10  ==> mbuf.vlan_tci
	 * checksum_fcoe            - 12  (unused)
	 * flags                    - 14  use: all bits
	 * type_color               - 15  (unused)
	 *
	 * --- mbuf fields ---       offset
	 * rearm_data              ---- 16
	 * data_off    - 0      (mbuf_init) -+
	 * refcnt      - 2      (mbuf_init)  |
	 * nb_segs     - 4      (mbuf_init)  | 16B 128b
	 * port        - 6      (mbuf_init)  |
	 * ol_flag     - 8      (from cqd)  -+
	 * rx_descriptor_fields1   ---- 32
	 * packet_type - 0      (from cqd)  -+
	 * pkt_len     - 4      (from cqd)   |
	 * data_len    - 8      (from cqd)   | 16B 128b
	 * vlan_tci    - 10     (from cqd)   |
	 * rss         - 12     (from cqd)  -+
	 */

	__m256i overlay_enabled =
		_mm256_set1_epi32((uint32_t)enic->overlay_offload);

	/* Step 2: Process 8 packets per loop using SIMD */
	while (max_rx > 7 && (((cqd + 7)->type_color &
			       CQ_DESC_COLOR_MASK_NOSHIFT) != color)) {
		/* Load 8 16B CQ descriptors */
		__m256i cqd01 = _mm256_load_si256((void *)cqd);
		__m256i cqd23 = _mm256_load_si256((void *)(cqd + 2));
		__m256i cqd45 = _mm256_load_si256((void *)(cqd + 4));
		__m256i cqd67 = _mm256_load_si256((void *)(cqd + 6));
		/* Copy 8 mbuf pointers to rx_pkts */
		_mm256_storeu_si256((void *)rx,
				    _mm256_loadu_si256((void *)rxmb));
		_mm256_storeu_si256((void *)(rx + 4),
				    _mm256_loadu_si256((void *)(rxmb + 4)));

		/*
		 * Collect 8 flags (each 32 bits) into one register.
		 * 4 shuffles, 3 blends, 1 permute for 8 desc: 1 inst/desc
		 */
		__m256i flags01 =
			_mm256_shuffle_epi8(cqd01, flags_shuffle_mask);
		/*
		 * Shuffle above produces 8 x 32-bit flags for 8 descriptors
		 * in this order: 0, 0, 0, 0, 1, 1, 1, 1
		 * The duplicates in each 128-bit lane simplifies blending
		 * below.
		 */
		__m256i flags23 =
			_mm256_shuffle_epi8(cqd23, flags_shuffle_mask);
		__m256i flags45 =
			_mm256_shuffle_epi8(cqd45, flags_shuffle_mask);
		__m256i flags67 =
			_mm256_shuffle_epi8(cqd67, flags_shuffle_mask);
		/* 1st blend produces flags for desc: 0, 2, 0, 0, 1, 3, 1, 1 */
		__m256i flags0_3 = _mm256_blend_epi32(flags01, flags23, 0x22);
		/* 2nd blend produces flags for desc: 4, 4, 4, 6, 5, 5, 5, 7 */
		__m256i flags4_7 = _mm256_blend_epi32(flags45, flags67, 0x88);
		/* 3rd blend produces flags for desc: 0, 2, 4, 6, 1, 3, 5, 7 */
		__m256i flags0_7 = _mm256_blend_epi32(flags0_3, flags4_7, 0xcc);
		/*
		 * Swap to reorder flags in this order: 1, 3, 5, 7, 0, 2, 4, 6
		 * This order simplifies blend operations way below that
		 * produce 'rearm' data for each mbuf.
		 */
		flags0_7 = _mm256_permute4x64_epi64(flags0_7,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);

		/*
		 * Check truncated bits and bail out early on.
		 * 6 avx inst, 1 or, 1 if-then-else for 8 desc: 1 inst/desc
		 */
		__m256i trunc =
			_mm256_srli_epi32(_mm256_slli_epi32(flags0_7, 17), 31);
		trunc = _mm256_add_epi64(trunc, _mm256_permute4x64_epi64(trunc,
			(1 << 6) + (0 << 4) + (3 << 2) + 2));
		/* 0:63 contains 1+3+0+2 and 64:127 contains 5+7+4+6 */
		if (_mm256_extract_epi64(trunc, 0) ||
		    _mm256_extract_epi64(trunc, 1))
			break;

		/*
		 * Compute RTE_MBUF_F_RX_RSS_HASH.
		 * Use 2 shifts and 1 shuffle for 8 desc: 0.375 inst/desc
		 * RSS types in byte 0, 4, 8, 12, 16, 20, 24, 28
		 * Everything else is zero.
		 */
		__m256i rss_types =
			_mm256_srli_epi32(_mm256_slli_epi32(flags0_7, 10), 28);
		/*
		 * RSS flags (RTE_MBUF_F_RX_RSS_HASH) are in
		 * byte 0, 4, 8, 12, 16, 20, 24, 28
		 * Everything else is zero.
		 */
		__m256i rss_flags = _mm256_shuffle_epi8(rss_shuffle, rss_types);

		/*
		 * Compute CKSUM flags. First build the index and then
		 * use it to shuffle csum_shuffle.
		 * 20 instructions including const loads: 2.5 inst/desc
		 */
		/*
		 * csum_not_calc (bit 22)
		 * csum_not_calc (0) => 0xffffffff
		 * csum_not_calc (1) => 0x0
		 */
		const __m256i zero4 = _mm256_setzero_si256();
		const __m256i mask22 = _mm256_set1_epi32(0x400000);
		__m256i csum_not_calc = _mm256_cmpeq_epi32(zero4,
			_mm256_and_si256(flags0_7, mask22));
		/*
		 * (tcp|udp) && !fragment => bit 1
		 * tcp = bit 2, udp = bit 1, frag = bit 6
		 */
		const __m256i mask1 = _mm256_set1_epi32(0x2);
		__m256i tcp_udp =
			_mm256_andnot_si256(_mm256_srli_epi32(flags0_7, 5),
				_mm256_or_si256(flags0_7,
					_mm256_srli_epi32(flags0_7, 1)));
		tcp_udp = _mm256_and_si256(tcp_udp, mask1);
		/* ipv4 (bit 5) => bit 2 */
		const __m256i mask2 = _mm256_set1_epi32(0x4);
		__m256i ipv4 = _mm256_and_si256(mask2,
			_mm256_srli_epi32(flags0_7, 3));
		/*
		 * ipv4_csum_ok (bit 3) => bit 3
		 * tcp_udp_csum_ok (bit 0) => bit 0
		 * 0x9
		 */
		const __m256i mask0_3 = _mm256_set1_epi32(0x9);
		__m256i csum_idx = _mm256_and_si256(flags0_7, mask0_3);
		csum_idx = _mm256_and_si256(csum_not_calc,
			_mm256_or_si256(_mm256_or_si256(csum_idx, ipv4),
				tcp_udp));
		__m256i csum_flags =
			_mm256_shuffle_epi8(csum_shuffle, csum_idx);
		/* Shift left to restore CKSUM flags. See csum_shuffle. */
		csum_flags = _mm256_slli_epi32(csum_flags, 1);
		/* Combine csum flags and offload flags: 0.125 inst/desc */
		rss_flags = _mm256_or_si256(rss_flags, csum_flags);

		/*
		 * Collect 8 VLAN IDs and compute vlan_id != 0 on each.
		 * 4 shuffles, 3 blends, 1 permute, 1 cmp, 1 sub for 8 desc:
		 * 1.25 inst/desc
		 */
		__m256i vlan01 = _mm256_shuffle_epi8(cqd01, vlan_shuffle_mask);
		__m256i vlan23 = _mm256_shuffle_epi8(cqd23, vlan_shuffle_mask);
		__m256i vlan45 = _mm256_shuffle_epi8(cqd45, vlan_shuffle_mask);
		__m256i vlan67 = _mm256_shuffle_epi8(cqd67, vlan_shuffle_mask);
		__m256i vlan0_3 = _mm256_blend_epi32(vlan01, vlan23, 0x22);
		__m256i vlan4_7 = _mm256_blend_epi32(vlan45, vlan67, 0x88);
		/* desc: 0, 2, 4, 6, 1, 3, 5, 7 */
		__m256i vlan0_7 = _mm256_blend_epi32(vlan0_3, vlan4_7, 0xcc);
		/* desc: 1, 3, 5, 7, 0, 2, 4, 6 */
		vlan0_7 = _mm256_permute4x64_epi64(vlan0_7,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);
		/*
		 * Compare 0 == vlan_id produces 0xffffffff (-1) if
		 * vlan 0 and 0 if vlan non-0. Then subtracting the
		 * result from 0 produces 0 - (-1) = 1 for vlan 0, and
		 * 0 - 0 = 0 for vlan non-0.
		 */
		vlan0_7 = _mm256_cmpeq_epi32(zero4, vlan0_7);
		/* vlan_id != 0 => 0, vlan_id == 0 => 1 */
		vlan0_7 = _mm256_sub_epi32(zero4, vlan0_7);

		/*
		 * Compute RTE_MBUF_F_RX_VLAN and RTE_MBUF_F_RX_VLAN_STRIPPED.
		 * Use 3 shifts, 1 or,  1 shuffle for 8 desc: 0.625 inst/desc
		 * VLAN offload flags in byte 0, 4, 8, 12, 16, 20, 24, 28
		 * Everything else is zero.
		 */
		__m256i vlan_idx =
			_mm256_or_si256(/* vlan_stripped => bit 0 */
				_mm256_srli_epi32(_mm256_slli_epi32(flags0_7,
					16), 31),
				/* (vlan_id == 0) => bit 1 */
				_mm256_slli_epi32(vlan0_7, 1));
		/*
		 * The index captures 4 cases.
		 * stripped, id = 0   ==> 11b = 3
		 * stripped, id != 0  ==> 01b = 1
		 * not strip, id == 0 ==> 10b = 2
		 * not strip, id != 0 ==> 00b = 0
		 */
		__m256i vlan_flags = _mm256_permutevar8x32_epi32(vlan_shuffle,
			vlan_idx);
		/* Combine vlan and offload flags: 0.125 inst/desc */
		rss_flags = _mm256_or_si256(rss_flags, vlan_flags);

		/*
		 * Compute non-tunnel PTYPEs.
		 * 17 inst / 8 desc = 2.125 inst/desc
		 */
		/* ETHER and ETHER_VLAN */
		__m256i vlan_ptype =
			_mm256_permutevar8x32_epi32(vlan_ptype_shuffle,
				vlan_idx);
		/* Build the ptype index from flags */
		tcp_udp = _mm256_slli_epi32(flags0_7, 29);
		tcp_udp = _mm256_slli_epi32(_mm256_srli_epi32(tcp_udp, 30), 2);
		__m256i ip4_ip6 =
			_mm256_srli_epi32(_mm256_slli_epi32(flags0_7, 26), 30);
		__m256i ptype_idx = _mm256_or_si256(tcp_udp, ip4_ip6);
		__m256i frag_bit =
			_mm256_srli_epi32(_mm256_slli_epi32(flags0_7, 25), 31);
		__m256i nonfrag_ptype =
			_mm256_shuffle_epi8(nonfrag_ptype_shuffle, ptype_idx);
		__m256i frag_ptype =
			_mm256_shuffle_epi8(frag_ptype_shuffle, ptype_idx);
		/*
		 * Zero out the unwanted types and combine the remaining bits.
		 * The effect is same as selecting non-frag or frag types
		 * depending on the frag bit.
		 */
		nonfrag_ptype = _mm256_and_si256(nonfrag_ptype,
			_mm256_cmpeq_epi32(zero4, frag_bit));
		frag_ptype = _mm256_and_si256(frag_ptype,
			_mm256_cmpgt_epi32(frag_bit, zero4));
		__m256i ptype = _mm256_or_si256(nonfrag_ptype, frag_ptype);
		ptype = _mm256_slli_epi32(ptype, 4);
		/*
		 * Compute tunnel PTYPEs.
		 * 15 inst / 8 desc = 1.875 inst/desc
		 */
		__m256i tnl_l3_ptype =
			_mm256_shuffle_epi8(tnl_l3_ptype_shuffle, ptype_idx);
		tnl_l3_ptype = _mm256_slli_epi32(tnl_l3_ptype, 16);
		/*
		 * Shift non-tunnel L4 types to make them tunnel types.
		 * RTE_PTYPE_L4_TCP << 16 == RTE_PTYPE_INNER_L4_TCP
		 */
		__m256i tnl_l4_ptype =
			_mm256_slli_epi32(_mm256_and_si256(ptype,
				_mm256_set1_epi32(RTE_PTYPE_L4_MASK)), 16);
		__m256i tnl_ptype =
			_mm256_or_si256(tnl_l3_ptype, tnl_l4_ptype);
		tnl_ptype = _mm256_or_si256(tnl_ptype,
			_mm256_set1_epi32(RTE_PTYPE_TUNNEL_GRENAT |
				RTE_PTYPE_INNER_L2_ETHER));
		/*
		 * Select non-tunnel or tunnel types by zeroing out the
		 * unwanted ones.
		 */
		__m256i tnl_flags = _mm256_and_si256(overlay_enabled,
			_mm256_srli_epi32(_mm256_slli_epi32(flags0_7, 2), 31));
		tnl_ptype = _mm256_and_si256(tnl_ptype,
			_mm256_sub_epi32(zero4, tnl_flags));
		ptype =	_mm256_and_si256(ptype,
			_mm256_cmpeq_epi32(zero4, tnl_flags));
		/*
		 * Combine types and swap to have ptypes in the same order
		 * as desc.
		 * desc: 0 2 4 6 1 3 5 7
		 * 3 inst / 8 desc = 0.375 inst/desc
		 */
		ptype = _mm256_or_si256(ptype, tnl_ptype);
		ptype = _mm256_or_si256(ptype, vlan_ptype);
		ptype = _mm256_permute4x64_epi64(ptype,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);

		/*
		 * Mask packet length.
		 * Use 4 ands: 0.5 instructions/desc
		 */
		cqd01 = _mm256_and_si256(cqd01, mask);
		cqd23 = _mm256_and_si256(cqd23, mask);
		cqd45 = _mm256_and_si256(cqd45, mask);
		cqd67 = _mm256_and_si256(cqd67, mask);
		/*
		 * Shuffle. Two 16B sets of the mbuf fields.
		 * packet_type, pkt_len, data_len, vlan_tci, rss
		 */
		__m256i rearm01 = _mm256_shuffle_epi8(cqd01, shuffle_mask);
		__m256i rearm23 = _mm256_shuffle_epi8(cqd23, shuffle_mask);
		__m256i rearm45 = _mm256_shuffle_epi8(cqd45, shuffle_mask);
		__m256i rearm67 = _mm256_shuffle_epi8(cqd67, shuffle_mask);

		/*
		 * Blend in ptypes
		 * 4 blends and 3 shuffles for 8 desc: 0.875 inst/desc
		 */
		rearm01 = _mm256_blend_epi32(rearm01, ptype, 0x11);
		rearm23 = _mm256_blend_epi32(rearm23,
			_mm256_shuffle_epi32(ptype, 1), 0x11);
		rearm45 = _mm256_blend_epi32(rearm45,
			_mm256_shuffle_epi32(ptype, 2), 0x11);
		rearm67 = _mm256_blend_epi32(rearm67,
			_mm256_shuffle_epi32(ptype, 3), 0x11);

		/*
		 * Move rss_flags into ol_flags in mbuf_init.
		 * Use 1 shift and 1 blend for each desc: 2 inst/desc
		 */
		__m256i mbuf_init4_5 = _mm256_blend_epi32(mbuf_init,
			rss_flags, 0x44);
		__m256i mbuf_init2_3 = _mm256_blend_epi32(mbuf_init,
			_mm256_slli_si256(rss_flags, 4), 0x44);
		__m256i mbuf_init0_1 = _mm256_blend_epi32(mbuf_init,
			_mm256_slli_si256(rss_flags, 8), 0x44);
		__m256i mbuf_init6_7 = _mm256_blend_epi32(mbuf_init,
			_mm256_srli_si256(rss_flags, 4), 0x44);

		/*
		 * Build rearm, one per desc.
		 * 8 blends and 4 permutes: 1.5 inst/desc
		 */
		__m256i rearm0 = _mm256_blend_epi32(rearm01,
			mbuf_init0_1, 0xf0);
		__m256i rearm1 = _mm256_blend_epi32(mbuf_init0_1,
			rearm01, 0xf0);
		__m256i rearm2 = _mm256_blend_epi32(rearm23,
			mbuf_init2_3, 0xf0);
		__m256i rearm3 = _mm256_blend_epi32(mbuf_init2_3,
			rearm23, 0xf0);
		/* Swap upper and lower 64 bits */
		rearm0 = _mm256_permute4x64_epi64(rearm0,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);
		rearm2 = _mm256_permute4x64_epi64(rearm2,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);
		/* Second set of 4 descriptors */
		__m256i rearm4 = _mm256_blend_epi32(rearm45,
			mbuf_init4_5, 0xf0);
		__m256i rearm5 = _mm256_blend_epi32(mbuf_init4_5,
			rearm45, 0xf0);
		__m256i rearm6 = _mm256_blend_epi32(rearm67,
			mbuf_init6_7, 0xf0);
		__m256i rearm7 = _mm256_blend_epi32(mbuf_init6_7,
			rearm67, 0xf0);
		rearm4 = _mm256_permute4x64_epi64(rearm4,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);
		rearm6 = _mm256_permute4x64_epi64(rearm6,
			(1 << 6) + (0 << 4) + (3 << 2) + 2);

		/*
		 * Write out 32B of mbuf fields.
		 * data_off    - off 0  (mbuf_init)
		 * refcnt      - 2      (mbuf_init)
		 * nb_segs     - 4      (mbuf_init)
		 * port        - 6      (mbuf_init)
		 * ol_flag     - 8      (from cqd)
		 * packet_type - 16     (from cqd)
		 * pkt_len     - 20     (from cqd)
		 * data_len    - 24     (from cqd)
		 * vlan_tci    - 26     (from cqd)
		 * rss         - 28     (from cqd)
		 */
		_mm256_storeu_si256((__m256i *)&rxmb[0]->rearm_data, rearm0);
		_mm256_storeu_si256((__m256i *)&rxmb[1]->rearm_data, rearm1);
		_mm256_storeu_si256((__m256i *)&rxmb[2]->rearm_data, rearm2);
		_mm256_storeu_si256((__m256i *)&rxmb[3]->rearm_data, rearm3);
		_mm256_storeu_si256((__m256i *)&rxmb[4]->rearm_data, rearm4);
		_mm256_storeu_si256((__m256i *)&rxmb[5]->rearm_data, rearm5);
		_mm256_storeu_si256((__m256i *)&rxmb[6]->rearm_data, rearm6);
		_mm256_storeu_si256((__m256i *)&rxmb[7]->rearm_data, rearm7);

		max_rx -= 8;
		cqd += 8;
		rx += 8;
		rxmb += 8;
	}

	/*
	 * Step 3: Slow path to handle a small (<8) number of packets and
	 * occasional truncated packets.
	 */
	while (max_rx && ((cqd->type_color &
			   CQ_DESC_COLOR_MASK_NOSHIFT) != color)) {
		if (unlikely(cqd->bytes_written_flags &
			     CQ_ENET_RQ_DESC_FLAGS_TRUNCATED)) {
			rte_pktmbuf_free(*rxmb++);
			rte_atomic64_inc(&enic->soft_stats.rx_packet_errors);
		} else {
			*rx++ = rx_one(cqd, *rxmb++, enic);
		}
		cqd++;
		max_rx--;
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

	/* Step 4: Restock RQ with new mbufs */
	memcpy(rxmb, rq->free_mbufs + ENIC_RX_BURST_MAX - rq->num_free_mbufs,
	       sizeof(struct rte_mbuf *) * nb_rx);
	rq->num_free_mbufs -= nb_rx;
	while (nb_rx) {
		rqd->address = (*rxmb)->buf_iova + RTE_PKTMBUF_HEADROOM;
		nb_rx--;
		rqd++;
		rxmb++;
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

bool
enic_use_vector_rx_handler(struct rte_eth_dev *eth_dev)
{
	struct enic *enic = pmd_priv(eth_dev);

	/* User needs to request for the avx2 handler */
	if (!enic->enable_avx2_rx)
		return false;
	/* Do not support scatter Rx */
	if (!(enic->rq_count > 0 && enic->rq[0].data_queue_enable == 0))
		return false;
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) &&
			rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256) {
		ENICPMD_LOG(DEBUG, " use the non-scatter avx2 Rx handler");
		eth_dev->rx_pkt_burst = &enic_noscatter_vec_recv_pkts;
		enic->use_noscatter_vec_rx_handler = 1;
		return true;
	}
	return false;
}
