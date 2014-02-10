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

#ifndef _IPV4_FRAG_TBL_H_
#define _IPV4_FRAG_TBL_H_

/**
 * @file
 * IPv4 fragments table.
 *
 * Implementation of IPv4 fragment table create/destroy/find/update.
 *
 */

/*
 * The ipv4_frag_tbl is a simple hash table:
 * The basic idea is to use two hash functions and <bucket_entries>
 * associativity. This provides 2 * <bucket_entries> possible locations in
 * the hash table for each key. Sort of simplified Cuckoo hashing,
 * when the collision occurs and all 2 * <bucket_entries> are occupied,
 * instead of resinserting existing keys into alternative locations, we just
 * return a faiure.
 * Another thing timing: entries that resides in the table longer then
 * <max_cycles> are considered as invalid, and could be removed/replaced
 * byt the new ones. 
 * <key, data> pair is stored together, all add/update/lookup opearions are not
 * MT safe.
 */

#include <rte_jhash.h>
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

#define	PRIME_VALUE	0xeaad8405

TAILQ_HEAD(ipv4_pkt_list, ipv4_frag_pkt);

struct ipv4_frag_tbl_stat {
	uint64_t find_num;      /* total # of find/insert attempts. */
	uint64_t add_num;       /* # of add ops. */
	uint64_t del_num;       /* # of del ops. */
	uint64_t reuse_num;     /* # of reuse (del/add) ops. */
	uint64_t fail_total;    /* total # of add failures. */
	uint64_t fail_nospace;  /* # of 'no space' add failures. */
} __rte_cache_aligned;

struct ipv4_frag_tbl {
	uint64_t             max_cycles;      /* ttl for table entries. */
	uint32_t             entry_mask;      /* hash value mask. */
	uint32_t             max_entries;     /* max entries allowed. */
	uint32_t             use_entries;     /* entries in use. */
	uint32_t             bucket_entries;  /* hash assocaitivity. */
	uint32_t             nb_entries;      /* total size of the table. */
	uint32_t             nb_buckets;      /* num of associativity lines. */
	struct ipv4_frag_pkt *last;           /* last used entry. */
	struct ipv4_pkt_list lru;             /* LRU list for table entries. */
	struct ipv4_frag_tbl_stat stat;       /* statistics counters. */
	struct ipv4_frag_pkt pkt[0];          /* hash table. */
};

#define	IPV4_FRAG_TBL_POS(tbl, sig)	\
	((tbl)->pkt + ((sig) & (tbl)->entry_mask))

#define	IPV4_FRAG_HASH_FNUM	2

#ifdef IPV4_FRAG_TBL_STAT
#define	IPV4_FRAG_TBL_STAT_UPDATE(s, f, v)	((s)->f += (v))
#else
#define	IPV4_FRAG_TBL_STAT_UPDATE(s, f, v)	do {} while (0)
#endif /* IPV4_FRAG_TBL_STAT */

static inline void
ipv4_frag_hash(const struct ipv4_frag_key *key, uint32_t *v1, uint32_t *v2)
{
	uint32_t v;
	const uint32_t *p;

	p = (const uint32_t *)&key->src_dst;

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
	v = rte_hash_crc_4byte(p[0], PRIME_VALUE);
	v = rte_hash_crc_4byte(p[1], v);
	v = rte_hash_crc_4byte(key->id, v);
#else

	v = rte_jhash_3words(p[0], p[1], key->id, PRIME_VALUE);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

	*v1 =  v;
	*v2 = (v << 7) + (v >> 14);
}

/*
 * Update the table, after we finish processing it's entry.
 */
static inline void
ipv4_frag_inuse(struct ipv4_frag_tbl *tbl, const struct  ipv4_frag_pkt *fp)
{
	if (IPV4_FRAG_KEY_EMPTY(&fp->key)) {
		TAILQ_REMOVE(&tbl->lru, fp, lru);
		tbl->use_entries--;
	}
}

/*
 * For the given key, try to find an existing entry.
 * If such entry doesn't exist, will return free and/or timed-out entry,
 * that can be used for that key.
 */
static inline struct  ipv4_frag_pkt *
ipv4_frag_lookup(struct ipv4_frag_tbl *tbl,
	const struct ipv4_frag_key *key, uint64_t tms,
	struct ipv4_frag_pkt **free, struct ipv4_frag_pkt **stale)
{
	struct ipv4_frag_pkt *p1, *p2;
	struct ipv4_frag_pkt *empty, *old;
	uint64_t max_cycles;
	uint32_t i, assoc, sig1, sig2;

	empty = NULL;
	old = NULL;

	max_cycles = tbl->max_cycles;
	assoc = tbl->bucket_entries;

	if (tbl->last != NULL && IPV4_FRAG_KEY_CMP(&tbl->last->key, key) == 0)
		return (tbl->last);

	ipv4_frag_hash(key, &sig1, &sig2);
	p1 = IPV4_FRAG_TBL_POS(tbl, sig1);
	p2 = IPV4_FRAG_TBL_POS(tbl, sig2);

	for (i = 0; i != assoc; i++) {

		IPV4_FRAG_LOG(DEBUG, "%s:%d:\n"
                "tbl: %p, max_entries: %u, use_entries: %u\n"
                "ipv4_frag_pkt line0: %p, index: %u from %u\n"
		"key: <%" PRIx64 ", %#x>, start: %" PRIu64 "\n",
                __func__, __LINE__,
                tbl, tbl->max_entries, tbl->use_entries,
                p1, i, assoc,
		p1[i].key.src_dst, p1[i].key.id, p1[i].start);

		if (IPV4_FRAG_KEY_CMP(&p1[i].key, key) == 0)
			return (p1 + i);
		else if (IPV4_FRAG_KEY_EMPTY(&p1[i].key))
			empty = (empty == NULL) ? (p1 + i) : empty;
		else if (max_cycles + p1[i].start < tms)
			old = (old == NULL) ? (p1 + i) : old;

		IPV4_FRAG_LOG(DEBUG, "%s:%d:\n"
                "tbl: %p, max_entries: %u, use_entries: %u\n"
                "ipv4_frag_pkt line1: %p, index: %u from %u\n"
		"key: <%" PRIx64 ", %#x>, start: %" PRIu64 "\n",
                __func__, __LINE__,
                tbl, tbl->max_entries, tbl->use_entries,
                p2, i, assoc,
		p2[i].key.src_dst, p2[i].key.id, p2[i].start);

		if (IPV4_FRAG_KEY_CMP(&p2[i].key, key) == 0)
			return (p2 + i);
		else if (IPV4_FRAG_KEY_EMPTY(&p2[i].key))
			empty = (empty == NULL) ?( p2 + i) : empty;
		else if (max_cycles + p2[i].start < tms)
			old = (old == NULL) ? (p2 + i) : old;
	}

	*free = empty;
	*stale = old;
	return (NULL);
}

static inline void
ipv4_frag_tbl_del(struct ipv4_frag_tbl *tbl, struct ipv4_frag_death_row *dr,
	struct ipv4_frag_pkt *fp)
{
	ipv4_frag_free(fp, dr);
	IPV4_FRAG_KEY_INVALIDATE(&fp->key);
	TAILQ_REMOVE(&tbl->lru, fp, lru);
	tbl->use_entries--;
	IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat, del_num, 1);
}

static inline void
ipv4_frag_tbl_add(struct ipv4_frag_tbl *tbl,  struct ipv4_frag_pkt *fp,
	const struct ipv4_frag_key *key, uint64_t tms)
{
	fp->key = key[0];
	ipv4_frag_reset(fp, tms);
	TAILQ_INSERT_TAIL(&tbl->lru, fp, lru);
	tbl->use_entries++;
	IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat, add_num, 1);
}

static inline void
ipv4_frag_tbl_reuse(struct ipv4_frag_tbl *tbl, struct ipv4_frag_death_row *dr,
	struct ipv4_frag_pkt *fp, uint64_t tms)
{
	ipv4_frag_free(fp, dr);
	ipv4_frag_reset(fp, tms);
	TAILQ_REMOVE(&tbl->lru, fp, lru);
	TAILQ_INSERT_TAIL(&tbl->lru, fp, lru);
	IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat, reuse_num, 1);
}

/*
 * Find an entry in the table for the corresponding fragment.
 * If such entry is not present, then allocate a new one.
 * If the entry is stale, then free and reuse it.
 */
static inline struct ipv4_frag_pkt *
ipv4_frag_find(struct ipv4_frag_tbl *tbl, struct ipv4_frag_death_row *dr,
	const struct ipv4_frag_key *key, uint64_t tms)
{
	struct ipv4_frag_pkt *pkt, *free, *stale, *lru;
	uint64_t max_cycles;

	/*
	 * Actually the two line below are totally redundant.
	 * they are here, just to make gcc 4.6 happy.
	 */
	free = NULL;
	stale = NULL;
	max_cycles = tbl->max_cycles;

	IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat, find_num, 1);

	if ((pkt = ipv4_frag_lookup(tbl, key, tms, &free, &stale)) == NULL) {

		/*timed-out entry, free and invalidate it*/
		if (stale != NULL) {
			ipv4_frag_tbl_del(tbl, dr, stale);
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
				ipv4_frag_tbl_del(tbl, dr, lru);
			} else {
				free = NULL;
				IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat,
					fail_nospace, 1);
			}
		}

		/* found a free entry to reuse. */
		if (free != NULL) {
			ipv4_frag_tbl_add(tbl,  free, key, tms);
			pkt = free;
		}

	/*
	 * we found the flow, but it is already timed out,
	 * so free associated resources, reposition it in the LRU list,
	 * and reuse it.
	 */
	} else if (max_cycles + pkt->start < tms) {
		ipv4_frag_tbl_reuse(tbl, dr, pkt, tms);
	}

	IPV4_FRAG_TBL_STAT_UPDATE(&tbl->stat, fail_total, (pkt == NULL));

	tbl->last = pkt;
	return (pkt);
}

/*
 * Create a new IPV4 Frag table.
 * @param bucket_num
 *  Number of buckets in the hash table.
 * @param bucket_entries
 *  Number of entries per bucket (e.g. hash associativity).
 *  Should be power of two.
 * @param max_entries
 *   Maximum number of entries that could be stored in the table.
 *   The value should be less or equal then bucket_num * bucket_entries.
 * @param max_cycles
 *   Maximum TTL in cycles for each fragmented packet.
 * @param socket_id
 *  The *socket_id* argument is the socket identifier in the case of
 *  NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA constraints.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error.
 */
static struct ipv4_frag_tbl *
ipv4_frag_tbl_create(uint32_t bucket_num, uint32_t bucket_entries,
	uint32_t max_entries, uint64_t max_cycles, int socket_id)
{
	struct ipv4_frag_tbl *tbl;
	size_t sz;
	uint64_t nb_entries;

	nb_entries = rte_align32pow2(bucket_num);
	nb_entries *= bucket_entries;
	nb_entries *= IPV4_FRAG_HASH_FNUM;

	/* check input parameters. */
	if (rte_is_power_of_2(bucket_entries) == 0 ||
			nb_entries > UINT32_MAX || nb_entries == 0 ||
			nb_entries < max_entries) {
		RTE_LOG(ERR, USER1, "%s: invalid input parameter\n", __func__);
		return (NULL);
	}

	sz = sizeof (*tbl) + nb_entries * sizeof (tbl->pkt[0]);
	if ((tbl = rte_zmalloc_socket(__func__, sz, CACHE_LINE_SIZE,
			socket_id)) == NULL) {
		RTE_LOG(ERR, USER1,
			"%s: allocation of %zu bytes at socket %d failed do\n",
			__func__, sz, socket_id);
		return (NULL);
	}

	RTE_LOG(INFO, USER1, "%s: allocated of %zu bytes at socket %d\n",
		__func__, sz, socket_id); 

	tbl->max_cycles = max_cycles;
	tbl->max_entries = max_entries;
	tbl->nb_entries = (uint32_t)nb_entries;
	tbl->nb_buckets = bucket_num;
	tbl->bucket_entries = bucket_entries;
	tbl->entry_mask = (tbl->nb_entries - 1) & ~(tbl->bucket_entries  - 1);

	TAILQ_INIT(&(tbl->lru));
	return (tbl);
}

static inline void
ipv4_frag_tbl_destroy( struct ipv4_frag_tbl *tbl)
{
	rte_free(tbl);
}

static void
ipv4_frag_tbl_dump_stat(FILE *f, const struct ipv4_frag_tbl *tbl)
{
	uint64_t fail_total, fail_nospace;

	fail_total = tbl->stat.fail_total;
	fail_nospace = tbl->stat.fail_nospace;

	fprintf(f, "max entries:\t%u;\n"
		"entries in use:\t%u;\n"
		"finds/inserts:\t%" PRIu64 ";\n"
		"entries added:\t%" PRIu64 ";\n"
		"entries deleted by timeout:\t%" PRIu64 ";\n"
		"entries reused by timeout:\t%" PRIu64 ";\n"
		"total add failures:\t%" PRIu64 ";\n"
		"add no-space failures:\t%" PRIu64 ";\n"
		"add hash-collisions failures:\t%" PRIu64 ";\n",
		tbl->max_entries,
		tbl->use_entries,
		tbl->stat.find_num,
		tbl->stat.add_num,
		tbl->stat.del_num,
		tbl->stat.reuse_num,
		fail_total,
		fail_nospace,
		fail_total - fail_nospace);
}


#endif /* _IPV4_FRAG_TBL_H_ */
