/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stddef.h>
#include <errno.h>

#include <rte_memcpy.h>
#include <rte_mempool.h>
#include <rte_debug.h>

#include "ip_frag_common.h"

/* Fragment Offset */
#define	IPV4_HDR_DF_SHIFT			14
#define	IPV4_HDR_MF_SHIFT			13
#define	IPV4_HDR_FO_SHIFT			3

#define	IPV4_HDR_DF_MASK			(1 << IPV4_HDR_DF_SHIFT)
#define	IPV4_HDR_MF_MASK			(1 << IPV4_HDR_MF_SHIFT)

#define	IPV4_HDR_FO_ALIGN			(1 << IPV4_HDR_FO_SHIFT)

static inline void __fill_ipv4hdr_frag(struct ipv4_hdr *dst,
		const struct ipv4_hdr *src, uint16_t len, uint16_t fofs,
		uint16_t dofs, uint32_t mf)
{
	rte_memcpy(dst, src, sizeof(*dst));
	fofs = (uint16_t)(fofs + (dofs >> IPV4_HDR_FO_SHIFT));
	fofs = (uint16_t)(fofs | mf << IPV4_HDR_MF_SHIFT);
	dst->fragment_offset = rte_cpu_to_be_16(fofs);
	dst->total_length = rte_cpu_to_be_16(len);
	dst->hdr_checksum = 0;
}

static inline void __free_fragments(struct rte_mbuf *mb[], uint32_t num)
{
	uint32_t i;
	for (i = 0; i != num; i++)
		rte_pktmbuf_free(mb[i]);
}

/**
 * IPv4 fragmentation.
 *
 * This function implements the fragmentation of IPv4 packets.
 *
 * @param pkt_in
 *   The input packet.
 * @param pkts_out
 *   Array storing the output fragments.
 * @param mtu_size
 *   Size in bytes of the Maximum Transfer Unit (MTU) for the outgoing IPv4
 *   datagrams. This value includes the size of the IPv4 header.
 * @param pool_direct
 *   MBUF pool used for allocating direct buffers for the output fragments.
 * @param pool_indirect
 *   MBUF pool used for allocating indirect buffers for the output fragments.
 * @return
 *   Upon successful completion - number of output fragments placed
 *   in the pkts_out array.
 *   Otherwise - (-1) * <errno>.
 */
int32_t
rte_ipv4_fragment_packet(struct rte_mbuf *pkt_in,
	struct rte_mbuf **pkts_out,
	uint16_t nb_pkts_out,
	uint16_t mtu_size,
	struct rte_mempool *pool_direct,
	struct rte_mempool *pool_indirect)
{
	struct rte_mbuf *in_seg = NULL;
	struct ipv4_hdr *in_hdr;
	uint32_t out_pkt_pos, in_seg_data_pos;
	uint32_t more_in_segs;
	uint16_t fragment_offset, flag_offset, frag_size;
	uint16_t frag_bytes_remaining;

	/*
	 * Ensure the IP payload length of all fragments is aligned to a
	 * multiple of 8 bytes as per RFC791 section 2.3.
	 */
	frag_size = RTE_ALIGN_FLOOR((mtu_size - sizeof(struct ipv4_hdr)),
				    IPV4_HDR_FO_ALIGN);

	in_hdr = rte_pktmbuf_mtod(pkt_in, struct ipv4_hdr *);
	flag_offset = rte_cpu_to_be_16(in_hdr->fragment_offset);

	/* If Don't Fragment flag is set */
	if (unlikely ((flag_offset & IPV4_HDR_DF_MASK) != 0))
		return -ENOTSUP;

	/* Check that pkts_out is big enough to hold all fragments */
	if (unlikely(frag_size * nb_pkts_out <
	    (uint16_t)(pkt_in->pkt_len - sizeof (struct ipv4_hdr))))
		return -EINVAL;

	in_seg = pkt_in;
	in_seg_data_pos = sizeof(struct ipv4_hdr);
	out_pkt_pos = 0;
	fragment_offset = 0;

	more_in_segs = 1;
	while (likely(more_in_segs)) {
		struct rte_mbuf *out_pkt = NULL, *out_seg_prev = NULL;
		uint32_t more_out_segs;
		struct ipv4_hdr *out_hdr;

		/* Allocate direct buffer */
		out_pkt = rte_pktmbuf_alloc(pool_direct);
		if (unlikely(out_pkt == NULL)) {
			__free_fragments(pkts_out, out_pkt_pos);
			return -ENOMEM;
		}

		/* Reserve space for the IP header that will be built later */
		out_pkt->data_len = sizeof(struct ipv4_hdr);
		out_pkt->pkt_len = sizeof(struct ipv4_hdr);
		frag_bytes_remaining = frag_size;

		out_seg_prev = out_pkt;
		more_out_segs = 1;
		while (likely(more_out_segs && more_in_segs)) {
			struct rte_mbuf *out_seg = NULL;
			uint32_t len;

			/* Allocate indirect buffer */
			out_seg = rte_pktmbuf_alloc(pool_indirect);
			if (unlikely(out_seg == NULL)) {
				rte_pktmbuf_free(out_pkt);
				__free_fragments(pkts_out, out_pkt_pos);
				return -ENOMEM;
			}
			out_seg_prev->next = out_seg;
			out_seg_prev = out_seg;

			/* Prepare indirect buffer */
			rte_pktmbuf_attach(out_seg, in_seg);
			len = frag_bytes_remaining;
			if (len > (in_seg->data_len - in_seg_data_pos)) {
				len = in_seg->data_len - in_seg_data_pos;
			}
			out_seg->data_off = in_seg->data_off + in_seg_data_pos;
			out_seg->data_len = (uint16_t)len;
			out_pkt->pkt_len = (uint16_t)(len +
			    out_pkt->pkt_len);
			out_pkt->nb_segs += 1;
			in_seg_data_pos += len;
			frag_bytes_remaining -= len;

			/* Current output packet (i.e. fragment) done ? */
			if (unlikely(frag_bytes_remaining == 0))
				more_out_segs = 0;

			/* Current input segment done ? */
			if (unlikely(in_seg_data_pos == in_seg->data_len)) {
				in_seg = in_seg->next;
				in_seg_data_pos = 0;

				if (unlikely(in_seg == NULL))
					more_in_segs = 0;
			}
		}

		/* Build the IP header */

		out_hdr = rte_pktmbuf_mtod(out_pkt, struct ipv4_hdr *);

		__fill_ipv4hdr_frag(out_hdr, in_hdr,
		    (uint16_t)out_pkt->pkt_len,
		    flag_offset, fragment_offset, more_in_segs);

		fragment_offset = (uint16_t)(fragment_offset +
		    out_pkt->pkt_len - sizeof(struct ipv4_hdr));

		out_pkt->ol_flags |= PKT_TX_IP_CKSUM;
		out_pkt->l3_len = sizeof(struct ipv4_hdr);

		/* Write the fragment to the output list */
		pkts_out[out_pkt_pos] = out_pkt;
		out_pkt_pos ++;
	}

	return out_pkt_pos;
}
