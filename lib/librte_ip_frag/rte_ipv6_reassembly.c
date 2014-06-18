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

#include <rte_memcpy.h>

#include "ip_frag_common.h"

/**
 * @file
 * IPv6 reassemble
 *
 * Implementation of IPv6 reassembly.
 *
 */

static inline void
ip_frag_memmove(char *dst, char *src, int len)
{
	int i;

	/* go backwards to make sure we don't overwrite anything important */
	for (i = len - 1; i >= 0; i--)
		dst[i] = src[i];
}

/*
 * Reassemble fragments into one packet.
 */
struct rte_mbuf *
ipv6_frag_reassemble(const struct ip_frag_pkt *fp)
{
	struct ipv6_hdr *ip_hdr;
	struct ipv6_extension_fragment *frag_hdr;
	struct rte_mbuf *m, *prev;
	uint32_t i, n, ofs, first_len;
	uint32_t last_len, move_len, payload_len;

	first_len = fp->frags[IP_FIRST_FRAG_IDX].len;
	n = fp->last_idx - 1;

	/*start from the last fragment. */
	m = fp->frags[IP_LAST_FRAG_IDX].mb;
	ofs = fp->frags[IP_LAST_FRAG_IDX].ofs;
	last_len = fp->frags[IP_LAST_FRAG_IDX].len;

	payload_len = ofs + last_len;

	while (ofs != first_len) {

		prev = m;

		for (i = n; i != IP_FIRST_FRAG_IDX && ofs != first_len; i--) {

			/* previous fragment found. */
			if (fp->frags[i].ofs + fp->frags[i].len == ofs) {

				ip_frag_chain(fp->frags[i].mb, m);

				/* update our last fragment and offset. */
				m = fp->frags[i].mb;
				ofs = fp->frags[i].ofs;
			}
		}

		/* error - hole in the packet. */
		if (m == prev) {
			return NULL;
		}
	}

	/* chain with the first fragment. */
	ip_frag_chain(fp->frags[IP_FIRST_FRAG_IDX].mb, m);
	m = fp->frags[IP_FIRST_FRAG_IDX].mb;

	/* update mbuf fields for reassembled packet. */
	m->ol_flags |= PKT_TX_IP_CKSUM;

	/* update ipv6 header for the reassembled datagram */
	ip_hdr = (struct ipv6_hdr *) (rte_pktmbuf_mtod(m, uint8_t *) +
								  m->pkt.vlan_macip.f.l2_len);

	ip_hdr->payload_len = rte_cpu_to_be_16(payload_len);

	/*
	 * remove fragmentation header. note that per RFC2460, we need to update
	 * the last non-fragmentable header with the "next header" field to contain
	 * type of the first fragmentable header, but we currently don't support
	 * other headers, so we assume there are no other headers and thus update
	 * the main IPv6 header instead.
	 */
	move_len = m->pkt.vlan_macip.f.l2_len + m->pkt.vlan_macip.f.l3_len -
			sizeof(*frag_hdr);
	frag_hdr = (struct ipv6_extension_fragment *) (ip_hdr + 1);
	ip_hdr->proto = frag_hdr->next_header;

	ip_frag_memmove(rte_pktmbuf_mtod(m, char*) + sizeof(*frag_hdr),
			rte_pktmbuf_mtod(m, char*), move_len);

	rte_pktmbuf_adj(m, sizeof(*frag_hdr));

	return m;
}

/*
 * Process new mbuf with fragment of IPV6 datagram.
 * Incoming mbuf should have its l2_len/l3_len fields setup correctly.
 * @param tbl
 *   Table where to lookup/add the fragmented packet.
 * @param mb
 *   Incoming mbuf with IPV6 fragment.
 * @param tms
 *   Fragment arrival timestamp.
 * @param ip_hdr
 *   Pointer to the IPV6 header.
 * @param frag_hdr
 *   Pointer to the IPV6 fragment extension header.
 * @return
 *   Pointer to mbuf for reassembled packet, or NULL if:
 *   - an error occured.
 *   - not all fragments of the packet are collected yet.
 */
#define MORE_FRAGS(x) (((x) & 0x100) >> 8)
#define FRAG_OFFSET(x) (rte_cpu_to_be_16(x) >> 3)
struct rte_mbuf *
rte_ipv6_frag_reassemble_packet(struct rte_ip_frag_tbl *tbl,
		struct rte_ip_frag_death_row *dr, struct rte_mbuf *mb, uint64_t tms,
		struct ipv6_hdr *ip_hdr, struct ipv6_extension_fragment *frag_hdr)
{
	struct ip_frag_pkt *fp;
	struct ip_frag_key key;
	uint16_t ip_len, ip_ofs;

	rte_memcpy(&key.src_dst[0], ip_hdr->src_addr, 16);
	rte_memcpy(&key.src_dst[2], ip_hdr->dst_addr, 16);

	key.id = frag_hdr->id;
	key.key_len = IPV6_KEYLEN;

	ip_ofs = FRAG_OFFSET(frag_hdr->frag_data) * 8;

	/*
	 * as per RFC2460, payload length contains all extension headers as well.
	 * since we don't support anything but frag headers, this is what we remove
	 * from the payload len.
	 */
	ip_len = rte_be_to_cpu_16(ip_hdr->payload_len) - sizeof(*frag_hdr);

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p, tms: %" PRIu64
		", key: <" IPv6_KEY_BYTES_FMT ", %#x>, ofs: %u, len: %u, flags: %#x\n"
		"tbl: %p, max_cycles: %" PRIu64 ", entry_mask: %#x, "
		"max_entries: %u, use_entries: %u\n\n",
		__func__, __LINE__,
		mb, tms, IPv6_KEY_BYTES(key.src_dst), key.id, ip_ofs, ip_len, frag_hdr->more_frags,
		tbl, tbl->max_cycles, tbl->entry_mask, tbl->max_entries,
		tbl->use_entries);

	/* try to find/add entry into the fragment's table. */
	fp = ip_frag_find(tbl, dr, &key, tms);
	if (fp == NULL) {
		IP_FRAG_MBUF2DR(dr, mb);
		return NULL;
	}

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv6_frag_pkt: %p, key: <" IPv6_KEY_BYTES_FMT ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, IPv6_KEY_BYTES(fp->key.src_dst), fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);


	/* process the fragmented packet. */
	mb = ip_frag_process(fp, dr, mb, ip_ofs, ip_len,
			MORE_FRAGS(frag_hdr->frag_data));
	ip_frag_inuse(tbl, fp);

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv6_frag_pkt: %p, key: <" IPv6_KEY_BYTES_FMT ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__, mb,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, IPv6_KEY_BYTES(fp->key.src_dst), fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);

	return mb;
}
