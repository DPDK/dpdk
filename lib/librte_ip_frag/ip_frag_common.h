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

#ifndef _IP_FRAG_COMMON_H_
#define _IP_FRAG_COMMON_H_

#include "rte_ip_frag.h"

/* logging macros. */
#ifdef RTE_LIBRTE_IP_FRAG_DEBUG

#define	IP_FRAG_LOG(lvl, fmt, args...)	RTE_LOG(lvl, USER1, fmt, ##args)

#define	IP_FRAG_ASSERT(exp)					\
if (!(exp))	{							\
	rte_panic("function %s, line%d\tassert \"" #exp "\" failed\n",	\
		__func__, __LINE__);					\
}
#else
#define	IP_FRAG_LOG(lvl, fmt, args...)	do {} while(0)
#define IP_FRAG_ASSERT(exp)	do {} while (0)
#endif /* IP_FRAG_DEBUG */

#define IPV4_KEYLEN 1
#define IPV6_KEYLEN 4

/* helper macros */
#define	IP_FRAG_MBUF2DR(dr, mb)	((dr)->row[(dr)->cnt++] = (mb))

#define IPv6_KEY_BYTES(key) \
	(key)[0], (key)[1], (key)[2], (key)[3]
#define IPv6_KEY_BYTES_FMT \
	"%08" PRIx64 "%08" PRIx64 "%08" PRIx64 "%08" PRIx64

/* internal functions declarations */
struct rte_mbuf * ip_frag_process(struct ip_frag_pkt *fp,
		struct rte_ip_frag_death_row *dr, struct rte_mbuf *mb,
		uint16_t ofs, uint16_t len, uint16_t more_frags);

struct ip_frag_pkt * ip_frag_find(struct rte_ip_frag_tbl *tbl,
		struct rte_ip_frag_death_row *dr,
		const struct ip_frag_key *key, uint64_t tms);

struct ip_frag_pkt * ip_frag_lookup(struct rte_ip_frag_tbl *tbl,
	const struct ip_frag_key *key, uint64_t tms,
	struct ip_frag_pkt **free, struct ip_frag_pkt **stale);

/* these functions need to be declared here as ip_frag_process relies on them */
struct rte_mbuf * ipv4_frag_reassemble(const struct ip_frag_pkt *fp);
struct rte_mbuf * ipv6_frag_reassemble(const struct ip_frag_pkt *fp);



/*
 * misc frag key functions
 */

/* check if key is empty */
static inline int
ip_frag_key_is_empty(const struct ip_frag_key * key)
{
	uint32_t i;
	for (i = 0; i < RTE_MIN(key->key_len, RTE_DIM(key->src_dst)); i++)
		if (key->src_dst[i] != 0)
			return 0;
	return 1;
}

/* empty the key */
static inline void
ip_frag_key_invalidate(struct ip_frag_key * key)
{
	uint32_t i;
	for (i = 0; i < key->key_len; i++)
		key->src_dst[i] = 0;
}

/* compare two keys */
static inline int
ip_frag_key_cmp(const struct ip_frag_key * k1, const struct ip_frag_key * k2)
{
	uint32_t i, val;
	val = k1->id ^ k2->id;
	for (i = 0; i < k1->key_len; i++)
		val |= k1->src_dst[i] ^ k2->src_dst[i];
	return val;
}

/*
 * misc fragment functions
 */

/* put fragment on death row */
static inline void
ip_frag_free(struct ip_frag_pkt *fp, struct rte_ip_frag_death_row *dr)
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

/* if key is empty, mark key as in use */
static inline void
ip_frag_inuse(struct rte_ip_frag_tbl *tbl, const struct  ip_frag_pkt *fp)
{
	if (ip_frag_key_is_empty(&fp->key)) {
		TAILQ_REMOVE(&tbl->lru, fp, lru);
		tbl->use_entries--;
	}
}

/* reset the fragment */
static inline void
ip_frag_reset(struct ip_frag_pkt *fp, uint64_t tms)
{
	static const struct ip_frag zero_frag = {
		.ofs = 0,
		.len = 0,
		.mb = NULL,
	};

	fp->start = tms;
	fp->total_size = UINT32_MAX;
	fp->frag_size = 0;
	fp->last_idx = IP_MIN_FRAG_NUM;
	fp->frags[IP_LAST_FRAG_IDX] = zero_frag;
	fp->frags[IP_FIRST_FRAG_IDX] = zero_frag;
}

/* chain two mbufs */
static inline void
ip_frag_chain(struct rte_mbuf *mn, struct rte_mbuf *mp)
{
	struct rte_mbuf *ms;

	/* adjust start of the last fragment data. */
	rte_pktmbuf_adj(mp, (uint16_t)(mp->l2_len + mp->l3_len));

	/* chain two fragments. */
	ms = rte_pktmbuf_lastseg(mn);
	ms->next = mp;

	/* accumulate number of segments and total length. */
	mn->nb_segs = (uint8_t)(mn->nb_segs + mp->nb_segs);
	mn->pkt_len += mp->pkt_len;

	/* reset pkt_len and nb_segs for chained fragment. */
	mp->pkt_len = mp->data_len;
	mp->nb_segs = 1;
}


#endif /* _IP_FRAG_COMMON_H_ */
