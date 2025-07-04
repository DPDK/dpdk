/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stddef.h>

#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include "ip_frag_common.h"

#define	PRIME_VALUE	0xeaad8405

#define	IP_FRAG_TBL_POS(tbl, sig)	\
	((tbl)->pkt + ((sig) & (tbl)->entry_mask))

static inline void
ip_frag_tbl_add(struct rte_ip_frag_tbl *tbl,  struct ip_frag_pkt *fp,
	const struct ip_frag_key *key, uint64_t tms)
{
	fp->key = key[0];
	ip_frag_reset(fp, tms);
	TAILQ_INSERT_TAIL(&tbl->lru, fp, lru);
	tbl->use_entries++;
	IP_FRAG_TBL_STAT_UPDATE(&tbl->stat, add_num, 1);
}

static inline void
ip_frag_tbl_reuse(struct rte_ip_frag_tbl *tbl, struct rte_ip_frag_death_row *dr,
	struct ip_frag_pkt *fp, uint64_t tms)
{
	ip_frag_free(fp, dr);
	ip_frag_reset(fp, tms);
	TAILQ_REMOVE(&tbl->lru, fp, lru);
	TAILQ_INSERT_TAIL(&tbl->lru, fp, lru);
	IP_FRAG_TBL_STAT_UPDATE(&tbl->stat, reuse_num, 1);
}


static inline void
ipv4_frag_hash(const struct ip_frag_key *key, uint32_t *v1, uint32_t *v2)
{
	uint32_t v;
	const uint32_t *p;

	p = (const uint32_t *)&key->src_dst;

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	v = rte_hash_crc_4byte(p[0], PRIME_VALUE);
	v = rte_hash_crc_4byte(p[1], v);
	v = rte_hash_crc_4byte(key->id, v);
#else

	v = rte_jhash_3words(p[0], p[1], key->id, PRIME_VALUE);
#endif /* RTE_ARCH_X86 */

	*v1 =  v;
	*v2 = (v << 7) + (v >> 14);
}

static inline void
ipv6_frag_hash(const struct ip_frag_key *key, uint32_t *v1, uint32_t *v2)
{
	uint32_t v;
	const uint32_t *p;

	p = (const uint32_t *) &key->src_dst;

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	v = rte_hash_crc_4byte(p[0], PRIME_VALUE);
	v = rte_hash_crc_4byte(p[1], v);
	v = rte_hash_crc_4byte(p[2], v);
	v = rte_hash_crc_4byte(p[3], v);
	v = rte_hash_crc_4byte(p[4], v);
	v = rte_hash_crc_4byte(p[5], v);
	v = rte_hash_crc_4byte(p[6], v);
	v = rte_hash_crc_4byte(p[7], v);
	v = rte_hash_crc_4byte(key->id, v);
#else

	v = rte_jhash_3words(p[0], p[1], p[2], PRIME_VALUE);
	v = rte_jhash_3words(p[3], p[4], p[5], v);
	v = rte_jhash_3words(p[6], p[7], key->id, v);
#endif /* RTE_ARCH_X86 */

	*v1 =  v;
	*v2 = (v << 7) + (v >> 14);
}

struct rte_mbuf *
ip_frag_process(struct ip_frag_pkt *fp, struct rte_ip_frag_death_row *dr,
	struct rte_mbuf *mb, uint16_t ofs, uint16_t len, uint16_t more_frags)
{
	uint32_t idx;

	fp->frag_size += len;

	/* this is the first fragment. */
	if (ofs == 0) {
		idx = (fp->frags[IP_FIRST_FRAG_IDX].mb == NULL) ?
				IP_FIRST_FRAG_IDX : UINT32_MAX;

	/* this is the last fragment. */
	} else if (more_frags == 0) {
		fp->total_size = ofs + len;
		idx = (fp->frags[IP_LAST_FRAG_IDX].mb == NULL) ?
				IP_LAST_FRAG_IDX : UINT32_MAX;

	/* this is the intermediate fragment. */
	} else if ((idx = fp->last_idx) < RTE_DIM(fp->frags)) {
		fp->last_idx++;
	}

	/*
	 * erroneous packet: either exceed max allowed number of fragments,
	 * or duplicate first/last fragment encountered.
	 */
	if (idx >= RTE_DIM(fp->frags)) {

		/* report an error. */
		if (fp->key.key_len == IPV4_KEYLEN)
			IP_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
				"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, "
				"total_size: %u, frag_size: %u, last_idx: %u\n"
				"first fragment: ofs: %u, len: %u\n"
				"last fragment: ofs: %u, len: %u\n\n",
				__func__, __LINE__,
				fp, fp->key.src_dst[0], fp->key.id,
				fp->total_size, fp->frag_size, fp->last_idx,
				fp->frags[IP_FIRST_FRAG_IDX].ofs,
				fp->frags[IP_FIRST_FRAG_IDX].len,
				fp->frags[IP_LAST_FRAG_IDX].ofs,
				fp->frags[IP_LAST_FRAG_IDX].len);
		else
			IP_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
				"ipv6_frag_pkt: %p, key: <" IPv6_KEY_BYTES_FMT ", %#x>, "
				"total_size: %u, frag_size: %u, last_idx: %u\n"
				"first fragment: ofs: %u, len: %u\n"
				"last fragment: ofs: %u, len: %u\n\n",
				__func__, __LINE__,
				fp, IPv6_KEY_BYTES(fp->key.src_dst), fp->key.id,
				fp->total_size, fp->frag_size, fp->last_idx,
				fp->frags[IP_FIRST_FRAG_IDX].ofs,
				fp->frags[IP_FIRST_FRAG_IDX].len,
				fp->frags[IP_LAST_FRAG_IDX].ofs,
				fp->frags[IP_LAST_FRAG_IDX].len);

		/* free all fragments, invalidate the entry. */
		ip_frag_free(fp, dr);
		ip_frag_key_invalidate(&fp->key);
		IP_FRAG_MBUF2DR(dr, mb);

		return NULL;
	}

	fp->frags[idx].ofs = ofs;
	fp->frags[idx].len = len;
	fp->frags[idx].mb = mb;

	mb = NULL;

	/* not all fragments are collected yet. */
	if (likely (fp->frag_size < fp->total_size)) {
		return mb;

	/* if we collected all fragments, then try to reassemble. */
	} else if (fp->frag_size == fp->total_size &&
			fp->frags[IP_FIRST_FRAG_IDX].mb != NULL) {
		if (fp->key.key_len == IPV4_KEYLEN)
			mb = ipv4_frag_reassemble(fp);
		else
			mb = ipv6_frag_reassemble(fp);
	}

	/* errorenous set of fragments. */
	if (mb == NULL) {

		/* report an error. */
		if (fp->key.key_len == IPV4_KEYLEN)
			IP_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
				"ipv4_frag_pkt: %p, key: <%" PRIx64 ", %#x>, "
				"total_size: %u, frag_size: %u, last_idx: %u\n"
				"first fragment: ofs: %u, len: %u\n"
				"last fragment: ofs: %u, len: %u\n\n",
				__func__, __LINE__,
				fp, fp->key.src_dst[0], fp->key.id,
				fp->total_size, fp->frag_size, fp->last_idx,
				fp->frags[IP_FIRST_FRAG_IDX].ofs,
				fp->frags[IP_FIRST_FRAG_IDX].len,
				fp->frags[IP_LAST_FRAG_IDX].ofs,
				fp->frags[IP_LAST_FRAG_IDX].len);
		else
			IP_FRAG_LOG(DEBUG, "%s:%d invalid fragmented packet:\n"
				"ipv6_frag_pkt: %p, key: <" IPv6_KEY_BYTES_FMT ", %#x>, "
				"total_size: %u, frag_size: %u, last_idx: %u\n"
				"first fragment: ofs: %u, len: %u\n"
				"last fragment: ofs: %u, len: %u\n\n",
				__func__, __LINE__,
				fp, IPv6_KEY_BYTES(fp->key.src_dst), fp->key.id,
				fp->total_size, fp->frag_size, fp->last_idx,
				fp->frags[IP_FIRST_FRAG_IDX].ofs,
				fp->frags[IP_FIRST_FRAG_IDX].len,
				fp->frags[IP_LAST_FRAG_IDX].ofs,
				fp->frags[IP_LAST_FRAG_IDX].len);

		/* free associated resources. */
		ip_frag_free(fp, dr);
	}

	/* we are done with that entry, invalidate it. */
	ip_frag_key_invalidate(&fp->key);
	return mb;
}


/*
 * Find an entry in the table for the corresponding fragment.
 * If such entry is not present, then allocate a new one.
 * If the entry is stale, then free and reuse it.
 */
struct ip_frag_pkt *
ip_frag_find(struct rte_ip_frag_tbl *tbl, struct rte_ip_frag_death_row *dr,
	const struct ip_frag_key *key, uint64_t tms)
{
	struct ip_frag_pkt *pkt, *free, *stale, *lru;
	uint64_t max_cycles;

	/*
	 * Actually the two line below are totally redundant.
	 * they are here, just to make gcc 4.6 happy.
	 */
	free = NULL;
	stale = NULL;
	max_cycles = tbl->max_cycles;

	IP_FRAG_TBL_STAT_UPDATE(&tbl->stat, find_num, 1);

	if ((pkt = ip_frag_lookup(tbl, key, tms, &free, &stale)) == NULL) {

		/*timed-out entry, free and invalidate it*/
		if (stale != NULL) {
			ip_frag_tbl_del(tbl, dr, stale);
			free = stale;

		/*
		 * we found a free entry, check if we can use it.
		 * If we run out of free entries in the table, then
		 * check if we have a timed out entry to delete.
		 */
		} else if (free != NULL &&
				tbl->max_entries <= tbl->use_entries) {
			lru = TAILQ_FIRST(&tbl->lru);
			if (max_cycles + lru->start < tms) {
				ip_frag_tbl_del(tbl, dr, lru);
			} else {
				free = NULL;
				IP_FRAG_TBL_STAT_UPDATE(&tbl->stat,
					fail_nospace, 1);
			}
		}

		/* found a free entry to reuse. */
		if (free != NULL) {
			ip_frag_tbl_add(tbl,  free, key, tms);
			pkt = free;
		}

	/*
	 * we found the flow, but it is already timed out,
	 * so free associated resources, reposition it in the LRU list,
	 * and reuse it.
	 */
	} else if (max_cycles + pkt->start < tms) {
		ip_frag_tbl_reuse(tbl, dr, pkt, tms);
	}

	IP_FRAG_TBL_STAT_UPDATE(&tbl->stat, fail_total, (pkt == NULL));

	tbl->last = pkt;
	return pkt;
}

struct ip_frag_pkt *
ip_frag_lookup(struct rte_ip_frag_tbl *tbl,
	const struct ip_frag_key *key, uint64_t tms,
	struct ip_frag_pkt **free, struct ip_frag_pkt **stale)
{
	struct ip_frag_pkt *p1, *p2;
	struct ip_frag_pkt *empty, *old;
	uint64_t max_cycles;
	uint32_t i, assoc, sig1, sig2;

	empty = NULL;
	old = NULL;

	max_cycles = tbl->max_cycles;
	assoc = tbl->bucket_entries;

	if (tbl->last != NULL && ip_frag_key_cmp(key, &tbl->last->key) == 0)
		return tbl->last;

	/* different hashing methods for IPv4 and IPv6 */
	if (key->key_len == IPV4_KEYLEN)
		ipv4_frag_hash(key, &sig1, &sig2);
	else
		ipv6_frag_hash(key, &sig1, &sig2);

	p1 = IP_FRAG_TBL_POS(tbl, sig1);
	p2 = IP_FRAG_TBL_POS(tbl, sig2);

	for (i = 0; i != assoc; i++) {
		if (p1->key.key_len == IPV4_KEYLEN)
			IP_FRAG_LOG(DEBUG, "%s:%d:\n"
					"tbl: %p, max_entries: %u, use_entries: %u\n"
					"ipv4_frag_pkt line0: %p, index: %u from %u\n"
			"key: <%" PRIx64 ", %#x>, start: %" PRIu64 "\n",
					__func__, __LINE__,
					tbl, tbl->max_entries, tbl->use_entries,
					p1, i, assoc,
			p1[i].key.src_dst[0], p1[i].key.id, p1[i].start);
		else
			IP_FRAG_LOG(DEBUG, "%s:%d:\n"
					"tbl: %p, max_entries: %u, use_entries: %u\n"
					"ipv6_frag_pkt line0: %p, index: %u from %u\n"
			"key: <" IPv6_KEY_BYTES_FMT ", %#x>, start: %" PRIu64 "\n",
					__func__, __LINE__,
					tbl, tbl->max_entries, tbl->use_entries,
					p1, i, assoc,
			IPv6_KEY_BYTES(p1[i].key.src_dst), p1[i].key.id, p1[i].start);

		if (ip_frag_key_cmp(key, &p1[i].key) == 0)
			return p1 + i;
		else if (ip_frag_key_is_empty(&p1[i].key))
			empty = (empty == NULL) ? (p1 + i) : empty;
		else if (max_cycles + p1[i].start < tms)
			old = (old == NULL) ? (p1 + i) : old;

		if (p2->key.key_len == IPV4_KEYLEN)
			IP_FRAG_LOG(DEBUG, "%s:%d:\n"
					"tbl: %p, max_entries: %u, use_entries: %u\n"
					"ipv4_frag_pkt line1: %p, index: %u from %u\n"
			"key: <%" PRIx64 ", %#x>, start: %" PRIu64 "\n",
					__func__, __LINE__,
					tbl, tbl->max_entries, tbl->use_entries,
					p2, i, assoc,
			p2[i].key.src_dst[0], p2[i].key.id, p2[i].start);
		else
			IP_FRAG_LOG(DEBUG, "%s:%d:\n"
					"tbl: %p, max_entries: %u, use_entries: %u\n"
					"ipv6_frag_pkt line1: %p, index: %u from %u\n"
			"key: <" IPv6_KEY_BYTES_FMT ", %#x>, start: %" PRIu64 "\n",
					__func__, __LINE__,
					tbl, tbl->max_entries, tbl->use_entries,
					p2, i, assoc,
			IPv6_KEY_BYTES(p2[i].key.src_dst), p2[i].key.id, p2[i].start);

		if (ip_frag_key_cmp(key, &p2[i].key) == 0)
			return p2 + i;
		else if (ip_frag_key_is_empty(&p2[i].key))
			empty = (empty == NULL) ?( p2 + i) : empty;
		else if (max_cycles + p2[i].start < tms)
			old = (old == NULL) ? (p2 + i) : old;
	}

	*free = empty;
	*stale = old;
	return NULL;
}
