/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stddef.h>
#include <errno.h>

#include <rte_memcpy.h>

#include "ip_frag_common.h"

/**
 * @file
 * RTE IPv6 Fragmentation
 *
 * Implementation of IPv6 fragmentation.
 *
 */

static inline void
__fill_ipv6hdr_frag(struct ipv6_hdr *dst,
		const struct ipv6_hdr *src, uint16_t len, uint16_t fofs,
		uint32_t mf)
{
	struct ipv6_extension_fragment *fh;

	rte_memcpy(dst, src, sizeof(*dst));
	dst->payload_len = rte_cpu_to_be_16(len);
	dst->proto = IPPROTO_FRAGMENT;

	fh = (struct ipv6_extension_fragment *) ++dst;
	fh->next_header = src->proto;
	fh->reserved = 0;
	fh->frag_data = rte_cpu_to_be_16(RTE_IPV6_SET_FRAG_DATA(fofs, mf));
	fh->id = 0;
}

static inline void
__free_fragments(struct rte_mbuf *mb[], uint32_t num)
{
	uint32_t i;
	for (i = 0; i < num; i++)
		rte_pktmbuf_free(mb[i]);
}

/**
 * IPv6 fragmentation.
 *
 * This function implements the fragmentation of IPv6 packets.
 *
 * @param pkt_in
 *   The input packet.
 * @param pkts_out
 *   Array storing the output fragments.
 * @param mtu_size
 *   Size in bytes of the Maximum Transfer Unit (MTU) for the outgoing IPv6
 *   datagrams. This value includes the size of the IPv6 header.
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
rte_ipv6_fragment_packet(struct rte_mbuf *pkt_in,
	struct rte_mbuf **pkts_out,
	uint16_t nb_pkts_out,
	uint16_t mtu_size,
	struct rte_mempool *pool_direct,
	struct rte_mempool *pool_indirect)
{
	struct rte_mbuf *in_seg = NULL;
	struct ipv6_hdr *in_hdr;
	uint32_t out_pkt_pos, in_seg_data_pos;
	uint32_t more_in_segs;
	uint16_t fragment_offset, frag_size;

	frag_size = (uint16_t)(mtu_size - sizeof(struct ipv6_hdr));

	/* Fragment size should be a multiple of 8. */
	RTE_ASSERT((frag_size & ~RTE_IPV6_EHDR_FO_MASK) == 0);

	/* Check that pkts_out is big enough to hold all fragments */
	if (unlikely (frag_size * nb_pkts_out <
	    (uint16_t)(pkt_in->pkt_len - sizeof (struct ipv6_hdr))))
		return -EINVAL;

	in_hdr = rte_pktmbuf_mtod(pkt_in, struct ipv6_hdr *);

	in_seg = pkt_in;
	in_seg_data_pos = sizeof(struct ipv6_hdr);
	out_pkt_pos = 0;
	fragment_offset = 0;

	more_in_segs = 1;
	while (likely(more_in_segs)) {
		struct rte_mbuf *out_pkt = NULL, *out_seg_prev = NULL;
		uint32_t more_out_segs;
		struct ipv6_hdr *out_hdr;

		/* Allocate direct buffer */
		out_pkt = rte_pktmbuf_alloc(pool_direct);
		if (unlikely(out_pkt == NULL)) {
			__free_fragments(pkts_out, out_pkt_pos);
			return -ENOMEM;
		}

		/* Reserve space for the IP header that will be built later */
		out_pkt->data_len = sizeof(struct ipv6_hdr) + sizeof(struct ipv6_extension_fragment);
		out_pkt->pkt_len  = sizeof(struct ipv6_hdr) + sizeof(struct ipv6_extension_fragment);

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
			len = mtu_size - out_pkt->pkt_len;
			if (len > (in_seg->data_len - in_seg_data_pos)) {
				len = in_seg->data_len - in_seg_data_pos;
			}
			out_seg->data_off = in_seg->data_off + in_seg_data_pos;
			out_seg->data_len = (uint16_t)len;
			out_pkt->pkt_len = (uint16_t)(len +
			    out_pkt->pkt_len);
			out_pkt->nb_segs += 1;
			in_seg_data_pos += len;

			/* Current output packet (i.e. fragment) done ? */
			if (unlikely(out_pkt->pkt_len >= mtu_size)) {
				more_out_segs = 0;
			}

			/* Current input segment done ? */
			if (unlikely(in_seg_data_pos == in_seg->data_len)) {
				in_seg = in_seg->next;
				in_seg_data_pos = 0;

				if (unlikely(in_seg == NULL)) {
					more_in_segs = 0;
				}
			}
		}

		/* Build the IP header */

		out_hdr = rte_pktmbuf_mtod(out_pkt, struct ipv6_hdr *);

		__fill_ipv6hdr_frag(out_hdr, in_hdr,
		    (uint16_t) out_pkt->pkt_len - sizeof(struct ipv6_hdr),
		    fragment_offset, more_in_segs);

		fragment_offset = (uint16_t)(fragment_offset +
		    out_pkt->pkt_len - sizeof(struct ipv6_hdr)
			- sizeof(struct ipv6_extension_fragment));

		/* Write the fragment to the output list */
		pkts_out[out_pkt_pos] = out_pkt;
		out_pkt_pos ++;
	}

	return out_pkt_pos;
}
