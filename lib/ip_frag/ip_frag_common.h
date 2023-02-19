/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _IP_FRAG_COMMON_H_
#define _IP_FRAG_COMMON_H_

#include <sys/queue.h>

#include "rte_ip_frag.h"
#include "ip_reassembly.h"

/* logging macros. */
#ifdef RTE_LIBRTE_IP_FRAG_DEBUG
#define	IP_FRAG_LOG(lvl, fmt, args...)	RTE_LOG(lvl, USER1, fmt, ##args)
#else
#define	IP_FRAG_LOG(lvl, fmt, args...)	do {} while(0)
#endif /* IP_FRAG_DEBUG */

#define IPV4_KEYLEN 1
#define IPV6_KEYLEN 4

/* helper macros */
#define	IP_FRAG_MBUF2DR(dr, mb)	((dr)->row[(dr)->cnt++] = (mb))

#define IPv6_KEY_BYTES(key) \
	(key)[0], (key)[1], (key)[2], (key)[3]
#define IPv6_KEY_BYTES_FMT \
	"%08" PRIx64 "%08" PRIx64 "%08" PRIx64 "%08" PRIx64

#ifdef RTE_LIBRTE_IP_FRAG_TBL_STAT
#define	IP_FRAG_TBL_STAT_UPDATE(s, f, v)	((s)->f += (v))
#else
#define	IP_FRAG_TBL_STAT_UPDATE(s, f, v)	do {} while (0)
#endif /* IP_FRAG_TBL_STAT */

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
struct rte_mbuf *ipv4_frag_reassemble(struct ip_frag_pkt *fp);
struct rte_mbuf *ipv6_frag_reassemble(struct ip_frag_pkt *fp);



/*
 * misc frag key functions
 */

/* check if key is empty */
static inline int
ip_frag_key_is_empty(const struct ip_frag_key * key)
{
	return (key->key_len == 0);
}

/* invalidate the key */
static inline void
ip_frag_key_invalidate(struct ip_frag_key * key)
{
	key->key_len = 0;
}

/* compare two keys */
static inline uint64_t
ip_frag_key_cmp(const struct ip_frag_key * k1, const struct ip_frag_key * k2)
{
	uint32_t i;
	uint64_t val;
	val = k1->id_key_len ^ k2->id_key_len;
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

/* delete fragment's mbufs immediately instead of using death row */
static inline void
ip_frag_free_immediate(struct ip_frag_pkt *fp)
{
	uint32_t i;

	for (i = 0; i < fp->last_idx; i++) {
		if (fp->frags[i].mb != NULL) {
			IP_FRAG_LOG(DEBUG, "%s:%d\n"
			    "mbuf: %p, tms: %" PRIu64", key: <%" PRIx64 ", %#x>\n",
			    __func__, __LINE__, fp->frags[i].mb, fp->start,
			    fp->key.src_dst[0], fp->key.id);
			rte_pktmbuf_free(fp->frags[i].mb);
			fp->frags[i].mb = NULL;
		}
	}

	fp->last_idx = 0;
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

/* local frag table helper functions */
static inline void
ip_frag_tbl_del(struct rte_ip_frag_tbl *tbl, struct rte_ip_frag_death_row *dr,
	struct ip_frag_pkt *fp)
{
	ip_frag_free(fp, dr);
	ip_frag_key_invalidate(&fp->key);
	TAILQ_REMOVE(&tbl->lru, fp, lru);
	tbl->use_entries--;
	IP_FRAG_TBL_STAT_UPDATE(&tbl->stat, del_num, 1);
}

#endif /* _IP_FRAG_COMMON_H_ */
