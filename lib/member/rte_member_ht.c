/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_log.h>
#include <rte_vect.h>

#include "member.h"
#include "rte_member.h"
#include "rte_member_ht.h"

#if defined(RTE_ARCH_X86)
#include "rte_member_x86.h"
#endif

/* Search bucket for entry with tmp_sig and update set_id */
static inline int
update_entry_search(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		member_set_t set_id)
{
	uint32_t i;

	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		if (buckets[bucket_id].sigs[i] == tmp_sig) {
			buckets[bucket_id].sets[i] = set_id;
			return 1;
		}
	}
	return 0;
}

static inline int
search_bucket_single(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		member_set_t *set_id)
{
	uint32_t iter;

	for (iter = 0; iter < RTE_MEMBER_BUCKET_ENTRIES; iter++) {
		if (tmp_sig == buckets[bucket_id].sigs[iter] &&
				buckets[bucket_id].sets[iter] !=
				RTE_MEMBER_NO_MATCH) {
			*set_id = buckets[bucket_id].sets[iter];
			return 1;
		}
	}
	return 0;
}

static inline void
search_bucket_multi(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		uint32_t *counter,
		uint32_t matches_per_key,
		member_set_t *set_id)
{
	uint32_t iter;

	for (iter = 0; iter < RTE_MEMBER_BUCKET_ENTRIES; iter++) {
		if (tmp_sig == buckets[bucket_id].sigs[iter] &&
				buckets[bucket_id].sets[iter] !=
				RTE_MEMBER_NO_MATCH) {
			set_id[*counter] = buckets[bucket_id].sets[iter];
			(*counter)++;
			if (*counter >= matches_per_key)
				return;
		}
	}
}

int
rte_member_create_ht(struct rte_member_setsum *ss,
		const struct rte_member_parameters *params)
{
	uint32_t i, j;
	uint32_t size_bucket_t;
	uint32_t num_entries = rte_align32pow2(params->num_keys);

	if ((num_entries > RTE_MEMBER_ENTRIES_MAX) ||
			!rte_is_power_of_2(RTE_MEMBER_BUCKET_ENTRIES) ||
			num_entries < RTE_MEMBER_BUCKET_ENTRIES) {
		rte_errno = EINVAL;
		MEMBER_LOG(ERR,
			"Membership HT create with invalid parameters");
		return -EINVAL;
	}

	uint32_t num_buckets = num_entries / RTE_MEMBER_BUCKET_ENTRIES;

	size_bucket_t = sizeof(struct member_ht_bucket);

	struct member_ht_bucket *buckets = rte_zmalloc_socket(NULL,
			num_buckets * size_bucket_t,
			RTE_CACHE_LINE_SIZE, ss->socket_id);

	if (buckets == NULL) {
		MEMBER_LOG(ERR, "memory allocation failed for HT "
						"setsummary");
		return -ENOMEM;
	}

	ss->table = buckets;
	ss->bucket_cnt = num_buckets;
	ss->bucket_mask = num_buckets - 1;
	ss->cache = params->is_cache;

	for (i = 0; i < num_buckets; i++) {
		for (j = 0; j < RTE_MEMBER_BUCKET_ENTRIES; j++)
			buckets[i].sets[j] = RTE_MEMBER_NO_MATCH;
	}
#if defined(RTE_ARCH_X86)
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) &&
			RTE_MEMBER_BUCKET_ENTRIES == 16 &&
			rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256)
		ss->sig_cmp_fn = RTE_MEMBER_COMPARE_AVX2;
	else
#endif
		ss->sig_cmp_fn = RTE_MEMBER_COMPARE_SCALAR;

	MEMBER_LOG(DEBUG, "Hash table based filter created, "
			"the table has %u entries, %u buckets",
			num_entries, num_buckets);
	return 0;
}

static inline void
get_buckets_index(const struct rte_member_setsum *ss, const void *key,
		uint32_t *prim_bkt, uint32_t *sec_bkt, member_sig_t *sig)
{
	uint32_t first_hash = MEMBER_HASH_FUNC(key, ss->key_len,
						ss->prim_hash_seed);
	uint32_t sec_hash = MEMBER_HASH_FUNC(&first_hash, sizeof(uint32_t),
						ss->sec_hash_seed);
	/*
	 * We use the first hash value for the signature, and the second hash
	 * value to derive the primary and secondary bucket locations.
	 *
	 * For non-cache mode, we use the lower bits for the primary bucket
	 * location. Then we xor primary bucket location and the signature
	 * to get the secondary bucket location. This is called "partial-key
	 * cuckoo hashing" proposed by B. Fan, et al's paper
	 * "Cuckoo Filter: Practically Better Than Bloom". The benefit to use
	 * xor is that one could derive the alternative bucket location
	 * by only using the current bucket location and the signature. This is
	 * generally required by non-cache mode's eviction and deletion
	 * process without the need to store alternative hash value nor the full
	 * key.
	 *
	 * For cache mode, we use the lower bits for the primary bucket
	 * location and the higher bits for the secondary bucket location. In
	 * cache mode, keys are simply overwritten if bucket is full. We do not
	 * use xor since lower/higher bits are more independent hash values thus
	 * should provide slightly better table load.
	 */
	*sig = first_hash;
	if (ss->cache) {
		*prim_bkt = sec_hash & ss->bucket_mask;
		*sec_bkt =  (sec_hash >> 16) & ss->bucket_mask;
	} else {
		*prim_bkt = sec_hash & ss->bucket_mask;
		*sec_bkt =  (*prim_bkt ^ *sig) & ss->bucket_mask;
	}
}

int
rte_member_lookup_ht(const struct rte_member_setsum *ss,
		const void *key, member_set_t *set_id)
{
	uint32_t prim_bucket, sec_bucket;
	member_sig_t tmp_sig;
	struct member_ht_bucket *buckets = ss->table;

	*set_id = RTE_MEMBER_NO_MATCH;
	get_buckets_index(ss, key, &prim_bucket, &sec_bucket, &tmp_sig);

	switch (ss->sig_cmp_fn) {
#if defined(RTE_ARCH_X86) && defined(__AVX2__)
	case RTE_MEMBER_COMPARE_AVX2:
		if (search_bucket_single_avx(prim_bucket, tmp_sig, buckets,
				set_id) ||
				search_bucket_single_avx(sec_bucket, tmp_sig,
					buckets, set_id))
			return 1;
		break;
#endif
	default:
		if (search_bucket_single(prim_bucket, tmp_sig, buckets,
				set_id) ||
				search_bucket_single(sec_bucket, tmp_sig,
					buckets, set_id))
			return 1;
	}

	return 0;
}

uint32_t
rte_member_lookup_bulk_ht(const struct rte_member_setsum *ss,
		const void **keys, uint32_t num_keys, member_set_t *set_id)
{
	uint32_t i;
	uint32_t num_matches = 0;
	struct member_ht_bucket *buckets = ss->table;
	member_sig_t tmp_sig[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t prim_buckets[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t sec_buckets[RTE_MEMBER_LOOKUP_BULK_MAX];

	for (i = 0; i < num_keys; i++) {
		get_buckets_index(ss, keys[i], &prim_buckets[i],
				&sec_buckets[i], &tmp_sig[i]);
		rte_prefetch0(&buckets[prim_buckets[i]]);
		rte_prefetch0(&buckets[sec_buckets[i]]);
	}

	for (i = 0; i < num_keys; i++) {
		switch (ss->sig_cmp_fn) {
#if defined(RTE_ARCH_X86) && defined(__AVX2__)
		case RTE_MEMBER_COMPARE_AVX2:
			if (search_bucket_single_avx(prim_buckets[i],
					tmp_sig[i], buckets, &set_id[i]) ||
				search_bucket_single_avx(sec_buckets[i],
					tmp_sig[i], buckets, &set_id[i]))
				num_matches++;
			else
				set_id[i] = RTE_MEMBER_NO_MATCH;
			break;
#endif
		default:
			if (search_bucket_single(prim_buckets[i], tmp_sig[i],
					buckets, &set_id[i]) ||
					search_bucket_single(sec_buckets[i],
					tmp_sig[i], buckets, &set_id[i]))
				num_matches++;
			else
				set_id[i] = RTE_MEMBER_NO_MATCH;
		}
	}
	return num_matches;
}

uint32_t
rte_member_lookup_multi_ht(const struct rte_member_setsum *ss,
		const void *key, uint32_t match_per_key,
		member_set_t *set_id)
{
	uint32_t num_matches = 0;
	uint32_t prim_bucket, sec_bucket;
	member_sig_t tmp_sig;
	struct member_ht_bucket *buckets = ss->table;

	get_buckets_index(ss, key, &prim_bucket, &sec_bucket, &tmp_sig);

	switch (ss->sig_cmp_fn) {
#if defined(RTE_ARCH_X86) && defined(__AVX2__)
	case RTE_MEMBER_COMPARE_AVX2:
		search_bucket_multi_avx(prim_bucket, tmp_sig, buckets,
			&num_matches, match_per_key, set_id);
		if (num_matches < match_per_key)
			search_bucket_multi_avx(sec_bucket, tmp_sig,
				buckets, &num_matches, match_per_key, set_id);
		return num_matches;
#endif
	default:
		search_bucket_multi(prim_bucket, tmp_sig, buckets, &num_matches,
				 match_per_key, set_id);
		if (num_matches < match_per_key)
			search_bucket_multi(sec_bucket, tmp_sig,
				buckets, &num_matches, match_per_key, set_id);
		return num_matches;
	}
}

uint32_t
rte_member_lookup_multi_bulk_ht(const struct rte_member_setsum *ss,
		const void **keys, uint32_t num_keys, uint32_t match_per_key,
		uint32_t *match_count,
		member_set_t *set_ids)
{
	uint32_t i;
	uint32_t num_matches = 0;
	struct member_ht_bucket *buckets = ss->table;
	uint32_t match_cnt_tmp;
	member_sig_t tmp_sig[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t prim_buckets[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t sec_buckets[RTE_MEMBER_LOOKUP_BULK_MAX];

	for (i = 0; i < num_keys; i++) {
		get_buckets_index(ss, keys[i], &prim_buckets[i],
				&sec_buckets[i], &tmp_sig[i]);
		rte_prefetch0(&buckets[prim_buckets[i]]);
		rte_prefetch0(&buckets[sec_buckets[i]]);
	}
	for (i = 0; i < num_keys; i++) {
		match_cnt_tmp = 0;

		switch (ss->sig_cmp_fn) {
#if defined(RTE_ARCH_X86) && defined(__AVX2__)
		case RTE_MEMBER_COMPARE_AVX2:
			search_bucket_multi_avx(prim_buckets[i], tmp_sig[i],
				buckets, &match_cnt_tmp, match_per_key,
				&set_ids[i*match_per_key]);
			if (match_cnt_tmp < match_per_key)
				search_bucket_multi_avx(sec_buckets[i],
					tmp_sig[i], buckets, &match_cnt_tmp,
					match_per_key,
					&set_ids[i*match_per_key]);
			match_count[i] = match_cnt_tmp;
			if (match_cnt_tmp != 0)
				num_matches++;
			break;
#endif
		default:
			search_bucket_multi(prim_buckets[i], tmp_sig[i],
				buckets, &match_cnt_tmp, match_per_key,
				&set_ids[i*match_per_key]);
			if (match_cnt_tmp < match_per_key)
				search_bucket_multi(sec_buckets[i], tmp_sig[i],
					buckets, &match_cnt_tmp, match_per_key,
					&set_ids[i*match_per_key]);
			match_count[i] = match_cnt_tmp;
			if (match_cnt_tmp != 0)
				num_matches++;
		}
	}
	return num_matches;
}

static inline int
try_insert(struct member_ht_bucket *buckets, uint32_t prim, uint32_t sec,
		member_sig_t sig, member_set_t set_id)
{
	int i;
	/* If not full then insert into one slot */
	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		if (buckets[prim].sets[i] == RTE_MEMBER_NO_MATCH) {
			buckets[prim].sigs[i] = sig;
			buckets[prim].sets[i] = set_id;
			return 0;
		}
	}
	/* If prim failed, we need to access second bucket */
	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		if (buckets[sec].sets[i] == RTE_MEMBER_NO_MATCH) {
			buckets[sec].sigs[i] = sig;
			buckets[sec].sets[i] = set_id;
			return 0;
		}
	}
	return -1;
}

static inline int
try_update(struct member_ht_bucket *buckets, uint32_t prim, uint32_t sec,
		member_sig_t sig, member_set_t set_id,
		enum rte_member_sig_compare_function cmp_fn)
{
	switch (cmp_fn) {
#if defined(RTE_ARCH_X86) && defined(__AVX2__)
	case RTE_MEMBER_COMPARE_AVX2:
		if (update_entry_search_avx(prim, sig, buckets, set_id) ||
				update_entry_search_avx(sec, sig, buckets,
					set_id))
			return 0;
		break;
#endif
	default:
		if (update_entry_search(prim, sig, buckets, set_id) ||
				update_entry_search(sec, sig, buckets,
					set_id))
			return 0;
	}
	return -1;
}

static inline int
evict_from_bucket(void)
{
	/* For now, we randomly pick one entry to evict */
	return rte_rand() & (RTE_MEMBER_BUCKET_ENTRIES - 1);
}

/*
 * This function is similar to the cuckoo hash make_space function in hash
 * library
 */
static inline int
make_space_bucket(const struct rte_member_setsum *ss, uint32_t bkt_idx,
			unsigned int *nr_pushes)
{
	unsigned int i, j;
	int ret;
	struct member_ht_bucket *buckets = ss->table;
	uint32_t next_bucket_idx;
	struct member_ht_bucket *next_bkt[RTE_MEMBER_BUCKET_ENTRIES];
	struct member_ht_bucket *bkt = &buckets[bkt_idx];
	/* MSB is set to indicate if an entry has been already pushed */
	member_set_t flag_mask = 1U << (sizeof(member_set_t) * 8 - 1);

	/*
	 * Push existing item (search for bucket with space in
	 * alternative locations) to its alternative location
	 */
	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		/* Search for space in alternative locations */
		next_bucket_idx = (bkt->sigs[i] ^ bkt_idx) & ss->bucket_mask;
		next_bkt[i] = &buckets[next_bucket_idx];
		for (j = 0; j < RTE_MEMBER_BUCKET_ENTRIES; j++) {
			if (next_bkt[i]->sets[j] == RTE_MEMBER_NO_MATCH)
				break;
		}

		if (j != RTE_MEMBER_BUCKET_ENTRIES)
			break;
	}

	/* Alternative location has spare room (end of recursive function) */
	if (i != RTE_MEMBER_BUCKET_ENTRIES) {
		next_bkt[i]->sigs[j] = bkt->sigs[i];
		next_bkt[i]->sets[j] = bkt->sets[i];
		return i;
	}

	/* Pick entry that has not been pushed yet */
	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++)
		if ((bkt->sets[i] & flag_mask) == 0)
			break;

	/* All entries have been pushed, so entry cannot be added */
	if (i == RTE_MEMBER_BUCKET_ENTRIES ||
			++(*nr_pushes) > RTE_MEMBER_MAX_PUSHES)
		return -ENOSPC;

	next_bucket_idx = (bkt->sigs[i] ^ bkt_idx) & ss->bucket_mask;
	/* Set flag to indicate that this entry is going to be pushed */
	bkt->sets[i] |= flag_mask;

	/* Need room in alternative bucket to insert the pushed entry */
	ret = make_space_bucket(ss, next_bucket_idx, nr_pushes);
	/*
	 * After recursive function.
	 * Clear flags and insert the pushed entry
	 * in its alternative location if successful,
	 * or return error
	 */
	bkt->sets[i] &= ~flag_mask;
	if (ret >= 0) {
		next_bkt[i]->sigs[ret] = bkt->sigs[i];
		next_bkt[i]->sets[ret] = bkt->sets[i];
		return i;
	} else
		return ret;
}

int
rte_member_add_ht(const struct rte_member_setsum *ss,
		const void *key, member_set_t set_id)
{
	int ret;
	unsigned int nr_pushes = 0;
	uint32_t prim_bucket, sec_bucket;
	member_sig_t tmp_sig;
	struct member_ht_bucket *buckets = ss->table;
	member_set_t flag_mask = 1U << (sizeof(member_set_t) * 8 - 1);

	if (set_id == RTE_MEMBER_NO_MATCH || (set_id & flag_mask) != 0)
		return -EINVAL;

	get_buckets_index(ss, key, &prim_bucket, &sec_bucket, &tmp_sig);

	/*
	 * If it is cache based setsummary, we try overwriting (updating)
	 * existing entry with the same signature first. In cache mode, we allow
	 * false negatives and only cache the most recent keys.
	 *
	 * For non-cache mode, we do not update existing entry with the same
	 * signature. This is because if two keys with same signature update
	 * each other, false negative may happen, which is not the expected
	 * behavior for non-cache setsummary.
	 */
	if (ss->cache) {
		ret = try_update(buckets, prim_bucket, sec_bucket, tmp_sig,
					set_id, ss->sig_cmp_fn);
		if (ret != -1)
			return ret;
	}
	/* If not full then insert into one slot */
	ret = try_insert(buckets, prim_bucket, sec_bucket, tmp_sig, set_id);
	if (ret != -1)
		return ret;

	/* Random pick prim or sec for recursive displacement */
	uint32_t select_bucket = (tmp_sig && 1U) ? prim_bucket : sec_bucket;
	if (ss->cache) {
		ret = evict_from_bucket();
		buckets[select_bucket].sigs[ret] = tmp_sig;
		buckets[select_bucket].sets[ret] = set_id;
		return 1;
	}

	ret = make_space_bucket(ss, select_bucket, &nr_pushes);
	if (ret >= 0) {
		buckets[select_bucket].sigs[ret] = tmp_sig;
		buckets[select_bucket].sets[ret] = set_id;
		ret = 1;
	}

	return ret;
}

void
rte_member_free_ht(struct rte_member_setsum *ss)
{
	rte_free(ss->table);
}

int
rte_member_delete_ht(const struct rte_member_setsum *ss, const void *key,
		member_set_t set_id)
{
	int i;
	uint32_t prim_bucket, sec_bucket;
	member_sig_t tmp_sig;
	struct member_ht_bucket *buckets = ss->table;

	get_buckets_index(ss, key, &prim_bucket, &sec_bucket, &tmp_sig);

	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		if (tmp_sig == buckets[prim_bucket].sigs[i] &&
				set_id == buckets[prim_bucket].sets[i]) {
			buckets[prim_bucket].sets[i] = RTE_MEMBER_NO_MATCH;
			return 0;
		}
	}

	for (i = 0; i < RTE_MEMBER_BUCKET_ENTRIES; i++) {
		if (tmp_sig == buckets[sec_bucket].sigs[i] &&
				set_id == buckets[sec_bucket].sets[i]) {
			buckets[sec_bucket].sets[i] = RTE_MEMBER_NO_MATCH;
			return 0;
		}
	}
	return -ENOENT;
}

void
rte_member_reset_ht(const struct rte_member_setsum *ss)
{
	uint32_t i, j;
	struct member_ht_bucket *buckets = ss->table;

	for (i = 0; i < ss->bucket_cnt; i++) {
		for (j = 0; j < RTE_MEMBER_BUCKET_ENTRIES; j++)
			buckets[i].sets[j] = RTE_MEMBER_NO_MATCH;
	}
}
