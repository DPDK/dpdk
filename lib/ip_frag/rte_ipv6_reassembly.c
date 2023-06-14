/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stddef.h>

#include <rte_memcpy.h>

#include "ip_frag_common.h"

/**
 * @file
 * IPv6 reassemble
 *
 * Implementation of IPv6 reassembly.
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
ipv6_frag_reassemble(struct ip_frag_pkt *fp)
{
	struct rte_ipv6_hdr *ip_hdr;
	struct rte_ipv6_fragment_ext *frag_hdr;
	struct rte_mbuf *m, *prev;
	uint32_t i, n, ofs, first_len;
	uint32_t last_len, move_len, payload_len;
	uint32_t curr_idx = 0;

	first_len = fp->frags[IP_FIRST_FRAG_IDX].len;
	n = fp->last_idx - 1;

	/*start from the last fragment. */
	m = fp->frags[IP_LAST_FRAG_IDX].mb;
	ofs = fp->frags[IP_LAST_FRAG_IDX].ofs;
	last_len = fp->frags[IP_LAST_FRAG_IDX].len;
	curr_idx = IP_LAST_FRAG_IDX;

	payload_len = ofs + last_len;

	while (ofs != first_len) {

		prev = m;

		for (i = n; i != IP_FIRST_FRAG_IDX && ofs != first_len; i--) {

			/* previous fragment found. */
			if (fp->frags[i].ofs + fp->frags[i].len == ofs) {

				RTE_ASSERT(curr_idx != i);

				/* adjust start of the last fragment data. */
				rte_pktmbuf_adj(m,
					(uint16_t)(m->l2_len + m->l3_len));
				rte_pktmbuf_chain(fp->frags[i].mb, m);

				/* this mbuf should not be accessed directly */
				fp->frags[curr_idx].mb = NULL;
				curr_idx = i;

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
	rte_pktmbuf_adj(m, (uint16_t)(m->l2_len + m->l3_len));
	rte_pktmbuf_chain(fp->frags[IP_FIRST_FRAG_IDX].mb, m);
	fp->frags[curr_idx].mb = NULL;
	m = fp->frags[IP_FIRST_FRAG_IDX].mb;
	fp->frags[IP_FIRST_FRAG_IDX].mb = NULL;

	/* update ipv6 header for the reassembled datagram */
	ip_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *, m->l2_len);

	ip_hdr->payload_len = rte_cpu_to_be_16(payload_len);

	/*
	 * remove fragmentation header. note that per RFC2460, we need to update
	 * the last non-fragmentable header with the "next header" field to contain
	 * type of the first fragmentable header, but we currently don't support
	 * other headers, so we assume there are no other headers and thus update
	 * the main IPv6 header instead.
	 */
	move_len = m->l2_len + m->l3_len - sizeof(*frag_hdr);
	frag_hdr = (struct rte_ipv6_fragment_ext *) (ip_hdr + 1);
	ip_hdr->proto = frag_hdr->next_header;

	ip_frag_memmove(rte_pktmbuf_mtod_offset(m, char *, sizeof(*frag_hdr)),
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
 *   - an error occurred.
 *   - not all fragments of the packet are collected yet.
 */
#define MORE_FRAGS(x) (((x) & 0x100) >> 8)
#define FRAG_OFFSET(x) (rte_cpu_to_be_16(x) >> 3)
struct rte_mbuf *
rte_ipv6_frag_reassemble_packet(struct rte_ip_frag_tbl *tbl,
	struct rte_ip_frag_death_row *dr, struct rte_mbuf *mb, uint64_t tms,
	struct rte_ipv6_hdr *ip_hdr, struct rte_ipv6_fragment_ext *frag_hdr)
{
	struct ip_frag_pkt *fp;
	struct ip_frag_key key;
	uint16_t ip_ofs;
	int32_t ip_len;
	int32_t trim;

	rte_memcpy(&key.src_dst[0], ip_hdr->src_addr, 16);
	rte_memcpy(&key.src_dst[2], ip_hdr->dst_addr, 16);

	key.id = frag_hdr->id;
	key.key_len = IPV6_KEYLEN;

	ip_ofs = FRAG_OFFSET(frag_hdr->frag_data) * 8;

	/*
	 * as per RFC2460, payload length contains all extension headers
	 * as well.
	 * since we don't support anything but frag headers,
	 * this is what we remove from the payload len.
	 */
	ip_len = rte_be_to_cpu_16(ip_hdr->payload_len) - sizeof(*frag_hdr);
	trim = mb->pkt_len - (ip_len + mb->l3_len + mb->l2_len);

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p, tms: %" PRIu64
		", key: <" IPv6_KEY_BYTES_FMT ", %#x>, "
		"ofs: %u, len: %d, padding: %d, flags: %#x\n"
		"tbl: %p, max_cycles: %" PRIu64 ", entry_mask: %#x, "
		"max_entries: %u, use_entries: %u\n\n",
		__func__, __LINE__,
		mb, tms, IPv6_KEY_BYTES(key.src_dst), key.id, ip_ofs, ip_len,
		trim, RTE_IPV6_GET_MF(frag_hdr->frag_data),
		tbl, tbl->max_cycles, tbl->entry_mask, tbl->max_entries,
		tbl->use_entries);

	/* check that fragment length is greater then zero. */
	if (ip_len <= 0) {
		IP_FRAG_MBUF2DR(dr, mb);
		return NULL;
	}

	if (unlikely(trim > 0))
		rte_pktmbuf_trim(mb, trim);

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
