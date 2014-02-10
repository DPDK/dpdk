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

#ifndef _IPV4_RSMBL_H_
#define _IPV4_RSMBL_H_

/**
 * @file
 * IPv4 reassemble
 *
 * Implementation of IPv4 reassemble.
 *
 */

enum {
	LAST_FRAG_IDX,
	FIRST_FRAG_IDX,
	MIN_FRAG_NUM,
	MAX_FRAG_NUM = 4,
};

struct ipv4_frag {
	uint16_t ofs;
	uint16_t len;
	struct rte_mbuf *mb;
};

/*
 * Use <src addr, dst_addr, id> to uniquely indetify fragmented datagram.
 */
struct ipv4_frag_key {
	uint64_t  src_dst;
	uint32_t  id;
};

#define	IPV4_FRAG_KEY_INVALIDATE(k)	((k)->src_dst = 0)
#define	IPV4_FRAG_KEY_EMPTY(k)		((k)->src_dst == 0)

#define	IPV4_FRAG_KEY_CMP(k1, k2)	\
	(((k1)->src_dst ^ (k2)->src_dst) | ((k1)->id ^ (k2)->id))


/*
 * Fragmented packet to reassemble.
 * First two entries in the frags[] array are for the last and first fragments.
 */
struct ipv4_frag_pkt {
	TAILQ_ENTRY(ipv4_frag_pkt) lru;   /* LRU list */
	struct ipv4_frag_key key;
	uint64_t             start;       /* creation timestamp */
	uint32_t             total_size;  /* expected reassembled size */
	uint32_t             frag_size;   /* size of fragments received */
	uint32_t             last_idx;    /* index of next entry to fill */
	struct ipv4_frag     frags[MAX_FRAG_NUM];
} __rte_cache_aligned;


struct ipv4_frag_death_row {
	uint32_t cnt;
	struct rte_mbuf *row[MAX_PKT_BURST * (MAX_FRAG_NUM + 1)];
};

#define	IPV4_FRAG_MBUF2DR(dr, mb)	((dr)->row[(dr)->cnt++] = (mb))

/* logging macros. */

#ifdef IPV4_FRAG_DEBUG
#define	IPV4_FRAG_LOG(lvl, fmt, args...)	RTE_LOG(lvl, USER1, fmt, ##args)
#else
#define	IPV4_FRAG_LOG(lvl, fmt, args...)	do {} while(0)
#endif /* IPV4_FRAG_DEBUG */


static inline void
ipv4_frag_reset(struct ipv4_frag_pkt *fp, uint64_t tms)
{
	static const struct ipv4_frag zero_frag = {
		.ofs = 0,
		.len = 0,
		.mb = NULL,
	};

	fp->start = tms;
	fp->total_size = UINT32_MAX;
	fp->frag_size = 0;
	fp->last_idx = MIN_FRAG_NUM;
	fp->frags[LAST_FRAG_IDX] = zero_frag;
	fp->frags[FIRST_FRAG_IDX] = zero_frag;
}

static inline void
ipv4_frag_free(struct ipv4_frag_pkt *fp, struct ipv4_frag_death_row *dr)
{
	uint32_t i, k;

	k = dr->cnt;
	for (i = 0; i != fp->last_idx; i++) {
		if (fp->frags[i].mb != NULL) {
			dr->row[k++] = fp->frags[i].mb;
			fp->frags[i].mb = NULL;
		}
	}

	fp->last_idx = 0;
	dr->cnt = k;
}

static inline void
ipv4_frag_free_death_row(struct ipv4_frag_death_row *dr, uint32_t prefetch)
{
	uint32_t i, k, n;

	k = RTE_MIN(prefetch, dr->cnt);
	n = dr->cnt;

	for (i = 0; i != k; i++) 
		rte_prefetch0(dr->row[i]);

	for (i = 0; i != n - k; i++) {
		rte_prefetch0(dr->row[i + k]);
		rte_pktmbuf_free(dr->row[i]);
	}

	for (; i != n; i++)
		rte_pktmbuf_free(dr->row[i]);

	dr->cnt = 0;
}

/*
 * Helper function.
 * Takes 2 mbufs that represents two framents of the same packet and
 * chains them into one mbuf.
 */
static inline void
ipv4_frag_chain(struct rte_mbuf *mn, struct rte_mbuf *mp)
{
	struct rte_mbuf *ms;

	/* adjust start of the last fragment data. */
	rte_pktmbuf_adj(mp, (uint16_t)(mp->pkt.vlan_macip.f.l2_len +
		mp->pkt.vlan_macip.f.l3_len));
				
	/* chain two fragments. */
	ms = rte_pktmbuf_lastseg(mn);
	ms->pkt.next = mp;

	/* accumulate number of segments and total length. */
	mn->pkt.nb_segs = (uint8_t)(mn->pkt.nb_segs + mp->pkt.nb_segs);
	mn->pkt.pkt_len += mp->pkt.pkt_len;
					
	/* reset pkt_len and nb_segs for chained fragment. */
	mp->pkt.pkt_len = mp->pkt.data_len;
	mp->pkt.nb_segs = 1;
}

/*
 * Reassemble fragments into one packet.
 */
static inline struct rte_mbuf *
ipv4_frag_reassemble(const struct ipv4_frag_pkt *fp)
{
	struct ipv4_hdr *ip_hdr;
	struct rte_mbuf *m, *prev;
	uint32_t i, n, ofs, first_len;

	first_len = fp->frags[FIRST_FRAG_IDX].len;
	n = fp->last_idx - 1;

	/*start from the last fragment. */
	m = fp->frags[LAST_FRAG_IDX].mb;
	ofs = fp->frags[LAST_FRAG_IDX].ofs;

	while (ofs != first_len) {

		prev = m;

		for (i = n; i != FIRST_FRAG_IDX && ofs != first_len; i--) {

			/* previous fragment found. */
			if(fp->frags[i].ofs + fp->frags[i].len == ofs) {

				ipv4_frag_chain(fp->frags[i].mb, m);

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
	ipv4_frag_chain(fp->frags[FIRST_FRAG_IDX].mb, m);
	m = fp->frags[FIRST_FRAG_IDX].mb;

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

static inline struct rte_mbuf *
ipv4_frag_process(struct ipv4_frag_pkt *fp, struct ipv4_frag_death_row *dr,
	struct rte_mbuf *mb, uint16_t ofs, uint16_t len, uint16_t more_frags)
{
	uint32_t idx;

	fp->frag_size += len;

	/* this is the first fragment. */
	if (ofs == 0) {
		idx = (fp->frags[FIRST_FRAG_IDX].mb == NULL) ?
			FIRST_FRAG_IDX : UINT32_MAX;

	/* this is the last fragment. */
	} else if (more_frags == 0) {
		fp->total_size = ofs + len;
		idx = (fp->frags[LAST_FRAG_IDX].mb == NULL) ?
			LAST_FRAG_IDX : UINT32_MAX;

	/* this is the intermediate fragment. */
	} else if ((idx = fp->last_idx) <
		sizeof (fp->frags) / sizeof (fp->frags[0])) {
		fp->last_idx++;
	}

	/*
	 * errorneous packet: either exceeed max allowed number of fragments,
	 * or duplicate first/last fragment encountered.
	 */
	if (idx >= sizeof (fp->frags) / sizeof (fp->frags[0])) {

		/* report an error. */
		IPV4_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
			"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, "
			"total_size: %u, frag_size: %u, last_idx: %u\n"
			"first fragment: ofs: %u, len: %u\n"
			"last fragment: ofs: %u, len: %u\n\n",
			__func__, __LINE__,
			fp, fp->key.src_dst, fp->key.id,
			fp->total_size, fp->frag_size, fp->last_idx,
			fp->frags[FIRST_FRAG_IDX].ofs,
			fp->frags[FIRST_FRAG_IDX].len,
			fp->frags[LAST_FRAG_IDX].ofs,
			fp->frags[LAST_FRAG_IDX].len);

		/* free all fragments, invalidate the entry. */
		ipv4_frag_free(fp, dr);
		IPV4_FRAG_KEY_INVALIDATE(&fp->key);
		IPV4_FRAG_MBUF2DR(dr, mb);

		return (NULL);
	}

	fp->frags[idx].ofs = ofs;
	fp->frags[idx].len = len;
	fp->frags[idx].mb = mb;

	mb = NULL;

	/* not all fragments are collected yet. */
	if (likely (fp->frag_size < fp->total_size)) {
		return (mb);

	/* if we collected all fragments, then try to reassemble. */
	} else if (fp->frag_size == fp->total_size &&
			fp->frags[FIRST_FRAG_IDX].mb != NULL) {
		mb = ipv4_frag_reassemble(fp);
	}

	/* errorenous set of fragments. */
	if (mb == NULL) {

		/* report an error. */
		IPV4_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
			"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, "
			"total_size: %u, frag_size: %u, last_idx: %u\n"
			"first fragment: ofs: %u, len: %u\n"
			"last fragment: ofs: %u, len: %u\n\n",
			__func__, __LINE__,
			fp, fp->key.src_dst, fp->key.id,
			fp->total_size, fp->frag_size, fp->last_idx,
			fp->frags[FIRST_FRAG_IDX].ofs,
			fp->frags[FIRST_FRAG_IDX].len,
			fp->frags[LAST_FRAG_IDX].ofs,
			fp->frags[LAST_FRAG_IDX].len);

		/* free associated resources. */
		ipv4_frag_free(fp, dr);
	}

	/* we are done with that entry, invalidate it. */
	IPV4_FRAG_KEY_INVALIDATE(&fp->key);
	return (mb);
}

#include "ipv4_frag_tbl.h"

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
 * @param ip_ofs
 *   Fragment's offset (as extracted from the header).
 * @param ip_flag
 *   Fragment's MF flag.
 * @return  
 *   Pointer to mbuf for reassebled packet, or NULL if:
 *   - an error occured.
 *   - not all fragments of the packet are collected yet.
 */
static inline struct rte_mbuf *
ipv4_frag_mbuf(struct ipv4_frag_tbl *tbl, struct ipv4_frag_death_row *dr,
	struct rte_mbuf *mb, uint64_t tms, struct ipv4_hdr *ip_hdr,
	uint16_t ip_ofs, uint16_t ip_flag)
{
	struct ipv4_frag_pkt *fp;
	struct ipv4_frag_key key;
	const uint64_t *psd;
	uint16_t ip_len;

	psd = (uint64_t *)&ip_hdr->src_addr;
	key.src_dst = psd[0];
	key.id = ip_hdr->packet_id;

	ip_ofs *= IPV4_HDR_OFFSET_UNITS;
	ip_len = (uint16_t)(rte_be_to_cpu_16(ip_hdr->total_length) -
		mb->pkt.vlan_macip.f.l3_len);

	IPV4_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p, tms: %" PRIu64
		", key: <%" PRIx64 ", %#x>, ofs: %u, len: %u, flags: %#x\n"
		"tbl: %p, max_cycles: %" PRIu64 ", entry_mask: %#x, "
		"max_entries: %u, use_entries: %u\n\n",
		__func__, __LINE__,
		mb, tms, key.src_dst, key.id, ip_ofs, ip_len, ip_flag,
		tbl, tbl->max_cycles, tbl->entry_mask, tbl->max_entries,
		tbl->use_entries);

	/* try to find/add entry into the fragment's table. */
	if ((fp = ipv4_frag_find(tbl, dr, &key, tms)) == NULL) {
		IPV4_FRAG_MBUF2DR(dr, mb);
		return (NULL);
	}

	IPV4_FRAG_LOG(DEBUG, "%s:%d:\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, fp->key.src_dst, fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);
		

	/* process the fragmented packet. */
	mb = ipv4_frag_process(fp, dr, mb, ip_ofs, ip_len, ip_flag);
	ipv4_frag_inuse(tbl, fp);

	IPV4_FRAG_LOG(DEBUG, "%s:%d:\n"
		"mbuf: %p\n"
		"tbl: %p, max_entries: %u, use_entries: %u\n"
		"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, start: %" PRIu64
		", total_size: %u, frag_size: %u, last_idx: %u\n\n",
		__func__, __LINE__, mb,
		tbl, tbl->max_entries, tbl->use_entries,
		fp, fp->key.src_dst, fp->key.id, fp->start,
		fp->total_size, fp->frag_size, fp->last_idx);

	return (mb);
}

#endif /* _IPV4_RSMBL_H_ */
