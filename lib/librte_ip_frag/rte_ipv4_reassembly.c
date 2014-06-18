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

#include <rte_debug.h>
#include <rte_tailq.h>

#include "ip_frag_common.h"

/*
 * Reassemble fragments into one packet.
 */
struct rte_mbuf *
ipv4_frag_reassemble(const struct ip_frag_pkt *fp)
{
	struct ipv4_hdr *ip_hdr;
	struct rte_mbuf *m, *prev;
	uint32_t i, n, ofs, first_len;

	first_len = fp->frags[IP_FIRST_FRAG_IDX].len;
	n = fp->last_idx - 1;

	/*start from the last fragment. */
	m = fp->frags[IP_LAST_FRAG_IDX].mb;
	ofs = fp->frags[IP_LAST_FRAG_IDX].ofs;

	while (ofs != first_len) {

		prev = m;

		for (i = n; i != IP_FIRST_FRAG_IDX && ofs != first_len; i--) {

			/* previous fragment found. */
			if(fp->frags[i].ofs + fp->frags[i].len == ofs) {

				ip_frag_chain(fp->frags[i].mb, m);

				/* update our last fragment and offset. */
				m = fp->frags[i].mb;
				ofs = fp->frags[i].ofs;
			}
		}

		/* error - hole in the packet. */
		if (m == prev) {
			return (NULL);
		}
	}

	/* chain with the first fragment. */
	ip_frag_chain(fp->frags[IP_FIRST_FRAG_IDX].mb, m);
	m = fp->frags[IP_FIRST_FRAG_IDX].mb;

	/* update mbuf fields for reassembled packet. */
	m->ol_flags |= PKT_TX_IP_CKSUM;

	/* update ipv4 header for the reassmebled packet */
	ip_hdr = (struct ipv4_hdr*)(rte_pktmbuf_mtod(m, uint8_t *) +
		m->pkt.vlan_macip.f.l2_len);

	ip_hdr->total_length = rte_cpu_to_be_16((uint16_t)(fp->total_size +
		m->pkt.vlan_macip.f.l3_len));
	ip_hdr->fragment_offset = (uint16_t)(ip_hdr->fragment_offset &
		rte_cpu_to_be_16(IPV4_HDR_DF_FLAG));
	ip_hdr->hdr_checksum = 0;

	return (m);
}

/*
 * Process new mbuf with fragment of IPV4 packet.
 * Incoming mbuf should have it's l2_len/l3_len fields setuped correclty.
 * @param tbl
 *   Table where to lookup/add the fragmented packet.
 * @param mb
 *   Incoming mbuf with IPV4 fragment.
 * @param tms
 *   Fragment arrival timestamp.
 * @param ip_hdr
 *   Pointer to the IPV4 header inside the fragment.
 * @return
 *   Pointer to mbuf for reassebled packet, or NULL if:
 *   - an error occured.
 *   - not all fragments of the packet are collected yet.
 */
struct rte_mbuf *
rte_ipv4_frag_reassemble_packet(struct rte_ip_frag_tbl *tbl,
		struct rte_ip_frag_death_row *dr, struct rte_mbuf *mb, uint64_t tms,
		struct ipv4_hdr *ip_hdr)
{
	struct ip_frag_pkt *fp;
	struct ip_frag_key key;
	const uint64_t *psd;
	uint16_t ip_len;
	uint16_t flag_offset, ip_ofs, ip_flag;

	flag_offset = rte_be_to_cpu_16(ip_hdr->fragment_offset);
	ip_ofs = (uint16_t)(flag_offset & IPV4_HDR_OFFSET_MASK);
	ip_flag = (uint16_t)(flag_offset & IPV4_HDR_MF_FLAG);

	psd = (uint64_t *)&ip_hdr->src_addr;
	/* use first 8 bytes only */
	key.src_dst[0] = psd[0];
	key.id = ip_hdr->packet_id;
	key.key_len = IPV4_KEYLEN;

	ip_ofs *= IPV4_HDR_OFFSET_UNITS;
	ip_len = (uint16_t)(rte_be_to_cpu_16(ip_hdr->total_length) -
		mb->pkt.vlan_macip.f.l3_len);

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p, tms: %" PRIu64
		", key: <%" PRIx64 ", %#x>, ofs: %u, len: %u, flags: %#x\n"
		"tbl: %p, max_cycles: %" PRIu64 ", entry_mask: %#x, "
		"max_entries: %u, use_entries: %u\n\n",
		__func__, __LINE__,
		mb, tms, key.src_dst[0], key.id, ip_ofs, ip_len, ip_flag,
		tbl, tbl->max_cycles, tbl->entry_mask, tbl->max_entries,
		tbl->use_entries);

	/* try to find/add entry into the fragment's table. */
	if ((fp = ip_frag_find(tbl, dr, &key, tms)) == NULL) {
		IP_FRAG_MBUF2DR(dr, mb);
		return (NULL);
	}

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, fp->key.src_dst[0], fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);


	/* process the fragmented packet. */
	mb = ip_frag_process(fp, dr, mb, ip_ofs, ip_len, ip_flag);
	ip_frag_inuse(tbl, fp);

	IP_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__, mb,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, fp->key.src_dst[0], fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);

	return (mb);
}
