/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>         /* for definition of RTE_CACHE_LINE_SIZE */
#include <rte_log.h>
#include <rte_memcpy.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_ring.h>
#include <rte_compat.h>
#include <rte_pause.h>

#include "rte_hash.h"
#include "rte_cuckoo_hash.h"


TAILQ_HEAD(rte_hash_list, rte_tailq_entry);

static struct rte_tailq_elem rte_hash_tailq = {
	.name = "RTE_HASH",
};
EAL_REGISTER_TAILQ(rte_hash_tailq)

struct rte_hash *
rte_hash_find_existing(const char *name)
{
	struct rte_hash *h = NULL;
	struct rte_tailq_entry *te;
	struct rte_hash_list *hash_list;

	hash_list = RTE_TAILQ_CAST(rte_hash_tailq.head, rte_hash_list);

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(te, hash_list, next) {
		h = (struct rte_hash *) te->data;
		if (strncmp(name, h->name, RTE_HASH_NAMESIZE) == 0)
			break;
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}
	return h;
}

void rte_hash_set_cmp_func(struct rte_hash *h, rte_hash_cmp_eq_t func)
{
	h->cmp_jump_table_idx = KEY_CUSTOM;
	h->rte_hash_custom_cmp_eq = func;
}

static inline int
rte_hash_cmp_eq(const void *key1, const void *key2, const struct rte_hash *h)
{
	if (h->cmp_jump_table_idx == KEY_CUSTOM)
		return h->rte_hash_custom_cmp_eq(key1, key2, h->key_len);
	else
		return cmp_jump_table[h->cmp_jump_table_idx](key1, key2, h->key_len);
}

struct rte_hash *
rte_hash_create(const struct rte_hash_parameters *params)
{
	struct rte_hash *h = NULL;
	struct rte_tailq_entry *te = NULL;
	struct rte_hash_list *hash_list;
	struct rte_ring *r = NULL;
	char hash_name[RTE_HASH_NAMESIZE];
	void *k = NULL;
	void *buckets = NULL;
	char ring_name[RTE_RING_NAMESIZE];
	unsigned num_key_slots;
	unsigned i;
	unsigned int hw_trans_mem_support = 0, multi_writer_support = 0;
	unsigned int readwrite_concur_support = 0;

	rte_hash_function default_hash_func = (rte_hash_function)rte_jhash;

	hash_list = RTE_TAILQ_CAST(rte_hash_tailq.head, rte_hash_list);

	if (params == NULL) {
		RTE_LOG(ERR, HASH, "rte_hash_create has no parameters\n");
		return NULL;
	}

	/* Check for valid parameters */
	if ((params->entries > RTE_HASH_ENTRIES_MAX) ||
			(params->entries < RTE_HASH_BUCKET_ENTRIES) ||
			(params->key_len == 0)) {
		rte_errno = EINVAL;
		RTE_LOG(ERR, HASH, "rte_hash_create has invalid parameters\n");
		return NULL;
	}

	/* Check extra flags field to check extra options. */
	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT)
		hw_trans_mem_support = 1;

	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD)
		multi_writer_support = 1;

	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY) {
		readwrite_concur_support = 1;
		multi_writer_support = 1;
	}

	/* Store all keys and leave the first entry as a dummy entry for lookup_bulk */
	if (multi_writer_support)
		/*
		 * Increase number of slots by total number of indices
		 * that can be stored in the lcore caches
		 * except for the first cache
		 */
		num_key_slots = params->entries + (RTE_MAX_LCORE - 1) *
					(LCORE_CACHE_SIZE - 1) + 1;
	else
		num_key_slots = params->entries + 1;

	snprintf(ring_name, sizeof(ring_name), "HT_%s", params->name);
	/* Create ring (Dummy slot index is not enqueued) */
	r = rte_ring_create(ring_name, rte_align32pow2(num_key_slots),
			params->socket_id, 0);
	if (r == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err;
	}

	snprintf(hash_name, sizeof(hash_name), "HT_%s", params->name);

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* guarantee there's no existing: this is normally already checked
	 * by ring creation above */
	TAILQ_FOREACH(te, hash_list, next) {
		h = (struct rte_hash *) te->data;
		if (strncmp(params->name, h->name, RTE_HASH_NAMESIZE) == 0)
			break;
	}
	h = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		te = NULL;
		goto err_unlock;
	}

	te = rte_zmalloc("HASH_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, HASH, "tailq entry allocation failed\n");
		goto err_unlock;
	}

	h = (struct rte_hash *)rte_zmalloc_socket(hash_name, sizeof(struct rte_hash),
					RTE_CACHE_LINE_SIZE, params->socket_id);

	if (h == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}

	const uint32_t num_buckets = rte_align32pow2(params->entries)
					/ RTE_HASH_BUCKET_ENTRIES;

	buckets = rte_zmalloc_socket(NULL,
				num_buckets * sizeof(struct rte_hash_bucket),
				RTE_CACHE_LINE_SIZE, params->socket_id);

	if (buckets == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}

	const uint32_t key_entry_size = sizeof(struct rte_hash_key) + params->key_len;
	const uint64_t key_tbl_size = (uint64_t) key_entry_size * num_key_slots;

	k = rte_zmalloc_socket(NULL, key_tbl_size,
			RTE_CACHE_LINE_SIZE, params->socket_id);

	if (k == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}

/*
 * If x86 architecture is used, select appropriate compare function,
 * which may use x86 intrinsics, otherwise use memcmp
 */
#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	/* Select function to compare keys */
	switch (params->key_len) {
	case 16:
		h->cmp_jump_table_idx = KEY_16_BYTES;
		break;
	case 32:
		h->cmp_jump_table_idx = KEY_32_BYTES;
		break;
	case 48:
		h->cmp_jump_table_idx = KEY_48_BYTES;
		break;
	case 64:
		h->cmp_jump_table_idx = KEY_64_BYTES;
		break;
	case 80:
		h->cmp_jump_table_idx = KEY_80_BYTES;
		break;
	case 96:
		h->cmp_jump_table_idx = KEY_96_BYTES;
		break;
	case 112:
		h->cmp_jump_table_idx = KEY_112_BYTES;
		break;
	case 128:
		h->cmp_jump_table_idx = KEY_128_BYTES;
		break;
	default:
		/* If key is not multiple of 16, use generic memcmp */
		h->cmp_jump_table_idx = KEY_OTHER_BYTES;
	}
#else
	h->cmp_jump_table_idx = KEY_OTHER_BYTES;
#endif

	if (multi_writer_support) {
		h->local_free_slots = rte_zmalloc_socket(NULL,
				sizeof(struct lcore_cache) * RTE_MAX_LCORE,
				RTE_CACHE_LINE_SIZE, params->socket_id);
	}

	/* Default hash function */
#if defined(RTE_ARCH_X86)
	default_hash_func = (rte_hash_function)rte_hash_crc;
#elif defined(RTE_ARCH_ARM64)
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_CRC32))
		default_hash_func = (rte_hash_function)rte_hash_crc;
#endif
	/* Setup hash context */
	snprintf(h->name, sizeof(h->name), "%s", params->name);
	h->entries = params->entries;
	h->key_len = params->key_len;
	h->key_entry_size = key_entry_size;
	h->hash_func_init_val = params->hash_func_init_val;

	h->num_buckets = num_buckets;
	h->bucket_bitmask = h->num_buckets - 1;
	h->buckets = buckets;
	h->hash_func = (params->hash_func == NULL) ?
		default_hash_func : params->hash_func;
	h->key_store = k;
	h->free_slots = r;
	h->hw_trans_mem_support = hw_trans_mem_support;
	h->multi_writer_support = multi_writer_support;
	h->readwrite_concur_support = readwrite_concur_support;

#if defined(RTE_ARCH_X86)
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		h->sig_cmp_fn = RTE_HASH_COMPARE_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE2))
		h->sig_cmp_fn = RTE_HASH_COMPARE_SSE;
	else
#endif
		h->sig_cmp_fn = RTE_HASH_COMPARE_SCALAR;

	/* Turn on multi-writer only with explicit flag from user and TM
	 * support.
	 */
	if (h->multi_writer_support) {
		h->readwrite_lock = rte_malloc(NULL, sizeof(rte_rwlock_t),
						RTE_CACHE_LINE_SIZE);
		if (h->readwrite_lock == NULL)
			goto err_unlock;

		rte_rwlock_init(h->readwrite_lock);
	}

	/* Populate free slots ring. Entry zero is reserved for key misses. */
	for (i = 1; i < num_key_slots; i++)
		rte_ring_sp_enqueue(r, (void *)((uintptr_t) i));

	te->data = (void *) h;
	TAILQ_INSERT_TAIL(hash_list, te, next);
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	return h;
err_unlock:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
err:
	rte_ring_free(r);
	rte_free(te);
	rte_free(h);
	rte_free(buckets);
	rte_free(k);
	return NULL;
}

void
rte_hash_free(struct rte_hash *h)
{
	struct rte_tailq_entry *te;
	struct rte_hash_list *hash_list;

	if (h == NULL)
		return;

	hash_list = RTE_TAILQ_CAST(rte_hash_tailq.head, rte_hash_list);

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* find out tailq entry */
	TAILQ_FOREACH(te, hash_list, next) {
		if (te->data == (void *) h)
			break;
	}

	if (te == NULL) {
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
		return;
	}

	TAILQ_REMOVE(hash_list, te, next);

	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (h->multi_writer_support) {
		rte_free(h->local_free_slots);
		rte_free(h->readwrite_lock);
	}
	rte_ring_free(h->free_slots);
	rte_free(h->key_store);
	rte_free(h->buckets);
	rte_free(h);
	rte_free(te);
}

hash_sig_t
rte_hash_hash(const struct rte_hash *h, const void *key)
{
	/* calc hash result by key */
	return h->hash_func(key, h->key_len, h->hash_func_init_val);
}

/* Calc the secondary hash value from the primary hash value of a given key */
static inline hash_sig_t
rte_hash_secondary_hash(const hash_sig_t primary_hash)
{
	static const unsigned all_bits_shift = 12;
	static const unsigned alt_bits_xor = 0x5bd1e995;

	uint32_t tag = primary_hash >> all_bits_shift;

	return primary_hash ^ ((tag + 1) * alt_bits_xor);
}

int32_t
rte_hash_count(const struct rte_hash *h)
{
	uint32_t tot_ring_cnt, cached_cnt = 0;
	uint32_t i, ret;

	if (h == NULL)
		return -EINVAL;

	if (h->multi_writer_support) {
		tot_ring_cnt = h->entries + (RTE_MAX_LCORE - 1) *
					(LCORE_CACHE_SIZE - 1);
		for (i = 0; i < RTE_MAX_LCORE; i++)
			cached_cnt += h->local_free_slots[i].len;

		ret = tot_ring_cnt - rte_ring_count(h->free_slots) -
								cached_cnt;
	} else {
		tot_ring_cnt = h->entries;
		ret = tot_ring_cnt - rte_ring_count(h->free_slots);
	}
	return ret;
}

/* Read write locks implemented using rte_rwlock */
static inline void
__hash_rw_writer_lock(const struct rte_hash *h)
{
	if (h->multi_writer_support && h->hw_trans_mem_support)
		rte_rwlock_write_lock_tm(h->readwrite_lock);
	else if (h->multi_writer_support)
		rte_rwlock_write_lock(h->readwrite_lock);
}


static inline void
__hash_rw_reader_lock(const struct rte_hash *h)
{
	if (h->readwrite_concur_support && h->hw_trans_mem_support)
		rte_rwlock_read_lock_tm(h->readwrite_lock);
	else if (h->readwrite_concur_support)
		rte_rwlock_read_lock(h->readwrite_lock);
}

static inline void
__hash_rw_writer_unlock(const struct rte_hash *h)
{
	if (h->multi_writer_support && h->hw_trans_mem_support)
		rte_rwlock_write_unlock_tm(h->readwrite_lock);
	else if (h->multi_writer_support)
		rte_rwlock_write_unlock(h->readwrite_lock);
}

static inline void
__hash_rw_reader_unlock(const struct rte_hash *h)
{
	if (h->readwrite_concur_support && h->hw_trans_mem_support)
		rte_rwlock_read_unlock_tm(h->readwrite_lock);
	else if (h->readwrite_concur_support)
		rte_rwlock_read_unlock(h->readwrite_lock);
}

void
rte_hash_reset(struct rte_hash *h)
{
	void *ptr;
	uint32_t tot_ring_cnt, i;

	if (h == NULL)
		return;

	__hash_rw_writer_lock(h);
	memset(h->buckets, 0, h->num_buckets * sizeof(struct rte_hash_bucket));
	memset(h->key_store, 0, h->key_entry_size * (h->entries + 1));

	/* clear the free ring */
	while (rte_ring_dequeue(h->free_slots, &ptr) == 0)
		rte_pause();

	/* Repopulate the free slots ring. Entry zero is reserved for key misses */
	if (h->multi_writer_support)
		tot_ring_cnt = h->entries + (RTE_MAX_LCORE - 1) *
					(LCORE_CACHE_SIZE - 1);
	else
		tot_ring_cnt = h->entries;

	for (i = 1; i < tot_ring_cnt + 1; i++)
		rte_ring_sp_enqueue(h->free_slots, (void *)((uintptr_t) i));

	if (h->multi_writer_support) {
		/* Reset local caches per lcore */
		for (i = 0; i < RTE_MAX_LCORE; i++)
			h->local_free_slots[i].len = 0;
	}
	__hash_rw_writer_unlock(h);
}

/*
 * Function called to enqueue back an index in the cache/ring,
 * as slot has not being used and it can be used in the
 * next addition attempt.
 */
static inline void
enqueue_slot_back(const struct rte_hash *h,
		struct lcore_cache *cached_free_slots,
		void *slot_id)
{
	if (h->multi_writer_support) {
		cached_free_slots->objs[cached_free_slots->len] = slot_id;
		cached_free_slots->len++;
	} else
		rte_ring_sp_enqueue(h->free_slots, slot_id);
}

/* Search a key from bucket and update its data */
static inline int32_t
search_and_update(const struct rte_hash *h, void *data, const void *key,
	struct rte_hash_bucket *bkt, hash_sig_t sig, hash_sig_t alt_hash)
{
	int i;
	struct rte_hash_key *k, *keys = h->key_store;

	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == sig &&
				bkt->sig_alt[i] == alt_hash) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				/* Update data */
				k->pdata = data;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}
	return -1;
}

/* Only tries to insert at one bucket (@prim_bkt) without trying to push
 * buckets around.
 * return 1 if matching existing key, return 0 if succeeds, return -1 for no
 * empty entry.
 */
static inline int32_t
rte_hash_cuckoo_insert_mw(const struct rte_hash *h,
		struct rte_hash_bucket *prim_bkt,
		struct rte_hash_bucket *sec_bkt,
		const struct rte_hash_key *key, void *data,
		hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx,
		int32_t *ret_val)
{
	unsigned int i;
	struct rte_hash_bucket *cur_bkt = prim_bkt;
	int32_t ret;

	__hash_rw_writer_lock(h);
	/* Check if key was inserted after last check but before this
	 * protected region in case of inserting duplicated keys.
	 */
	ret = search_and_update(h, data, key, cur_bkt, sig, alt_hash);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		*ret_val = ret;
		return 1;
	}
	ret = search_and_update(h, data, key, sec_bkt, alt_hash, sig);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		*ret_val = ret;
		return 1;
	}

	/* Insert new entry if there is room in the primary
	 * bucket.
	 */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		/* Check if slot is available */
		if (likely(prim_bkt->key_idx[i] == EMPTY_SLOT)) {
			prim_bkt->sig_current[i] = sig;
			prim_bkt->sig_alt[i] = alt_hash;
			prim_bkt->key_idx[i] = new_idx;
			break;
		}
	}
	__hash_rw_writer_unlock(h);

	if (i != RTE_HASH_BUCKET_ENTRIES)
		return 0;

	/* no empty entry */
	return -1;
}

/* Shift buckets along provided cuckoo_path (@leaf and @leaf_slot) and fill
 * the path head with new entry (sig, alt_hash, new_idx)
 * return 1 if matched key found, return -1 if cuckoo path invalided and fail,
 * return 0 if succeeds.
 */
static inline int
rte_hash_cuckoo_move_insert_mw(const struct rte_hash *h,
			struct rte_hash_bucket *bkt,
			struct rte_hash_bucket *alt_bkt,
			const struct rte_hash_key *key, void *data,
			struct queue_node *leaf, uint32_t leaf_slot,
			hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx,
			int32_t *ret_val)
{
	uint32_t prev_alt_bkt_idx;
	struct rte_hash_bucket *cur_bkt = bkt;
	struct queue_node *prev_node, *curr_node = leaf;
	struct rte_hash_bucket *prev_bkt, *curr_bkt = leaf->bkt;
	uint32_t prev_slot, curr_slot = leaf_slot;
	int32_t ret;

	__hash_rw_writer_lock(h);

	/* In case empty slot was gone before entering protected region */
	if (curr_bkt->key_idx[curr_slot] != EMPTY_SLOT) {
		__hash_rw_writer_unlock(h);
		return -1;
	}

	/* Check if key was inserted after last check but before this
	 * protected region.
	 */
	ret = search_and_update(h, data, key, cur_bkt, sig, alt_hash);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		*ret_val = ret;
		return 1;
	}

	ret = search_and_update(h, data, key, alt_bkt, alt_hash, sig);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		*ret_val = ret;
		return 1;
	}

	while (likely(curr_node->prev != NULL)) {
		prev_node = curr_node->prev;
		prev_bkt = prev_node->bkt;
		prev_slot = curr_node->prev_slot;

		prev_alt_bkt_idx =
			prev_bkt->sig_alt[prev_slot] & h->bucket_bitmask;

		if (unlikely(&h->buckets[prev_alt_bkt_idx]
				!= curr_bkt)) {
			/* revert it to empty, otherwise duplicated keys */
			curr_bkt->key_idx[curr_slot] = EMPTY_SLOT;
			__hash_rw_writer_unlock(h);
			return -1;
		}

		/* Need to swap current/alt sig to allow later
		 * Cuckoo insert to move elements back to its
		 * primary bucket if available
		 */
		curr_bkt->sig_alt[curr_slot] =
			 prev_bkt->sig_current[prev_slot];
		curr_bkt->sig_current[curr_slot] =
			prev_bkt->sig_alt[prev_slot];
		curr_bkt->key_idx[curr_slot] =
			prev_bkt->key_idx[prev_slot];

		curr_slot = prev_slot;
		curr_node = prev_node;
		curr_bkt = curr_node->bkt;
	}

	curr_bkt->sig_current[curr_slot] = sig;
	curr_bkt->sig_alt[curr_slot] = alt_hash;
	curr_bkt->key_idx[curr_slot] = new_idx;

	__hash_rw_writer_unlock(h);

	return 0;

}

/*
 * Make space for new key, using bfs Cuckoo Search and Multi-Writer safe
 * Cuckoo
 */
static inline int
rte_hash_cuckoo_make_space_mw(const struct rte_hash *h,
			struct rte_hash_bucket *bkt,
			struct rte_hash_bucket *sec_bkt,
			const struct rte_hash_key *key, void *data,
			hash_sig_t sig, hash_sig_t alt_hash,
			uint32_t new_idx, int32_t *ret_val)
{
	unsigned int i;
	struct queue_node queue[RTE_HASH_BFS_QUEUE_MAX_LEN];
	struct queue_node *tail, *head;
	struct rte_hash_bucket *curr_bkt, *alt_bkt;

	tail = queue;
	head = queue + 1;
	tail->bkt = bkt;
	tail->prev = NULL;
	tail->prev_slot = -1;

	/* Cuckoo bfs Search */
	while (likely(tail != head && head <
					queue + RTE_HASH_BFS_QUEUE_MAX_LEN -
					RTE_HASH_BUCKET_ENTRIES)) {
		curr_bkt = tail->bkt;
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			if (curr_bkt->key_idx[i] == EMPTY_SLOT) {
				int32_t ret = rte_hash_cuckoo_move_insert_mw(h,
						bkt, sec_bkt, key, data,
						tail, i, sig, alt_hash,
						new_idx, ret_val);
				if (likely(ret != -1))
					return ret;
			}

			/* Enqueue new node and keep prev node info */
			alt_bkt = &(h->buckets[curr_bkt->sig_alt[i]
						    & h->bucket_bitmask]);
			head->bkt = alt_bkt;
			head->prev = tail;
			head->prev_slot = i;
			head++;
		}
		tail++;
	}

	return -ENOSPC;
}

static inline int32_t
__rte_hash_add_key_with_hash(const struct rte_hash *h, const void *key,
						hash_sig_t sig, void *data)
{
	hash_sig_t alt_hash;
	uint32_t prim_bucket_idx, sec_bucket_idx;
	struct rte_hash_bucket *prim_bkt, *sec_bkt;
	struct rte_hash_key *new_k, *keys = h->key_store;
	void *slot_id = NULL;
	uint32_t new_idx;
	int ret;
	unsigned n_slots;
	unsigned lcore_id;
	struct lcore_cache *cached_free_slots = NULL;
	int32_t ret_val;

	prim_bucket_idx = sig & h->bucket_bitmask;
	prim_bkt = &h->buckets[prim_bucket_idx];
	rte_prefetch0(prim_bkt);

	alt_hash = rte_hash_secondary_hash(sig);
	sec_bucket_idx = alt_hash & h->bucket_bitmask;
	sec_bkt = &h->buckets[sec_bucket_idx];
	rte_prefetch0(sec_bkt);

	/* Check if key is already inserted in primary location */
	__hash_rw_writer_lock(h);
	ret = search_and_update(h, data, key, prim_bkt, sig, alt_hash);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		return ret;
	}

	/* Check if key is already inserted in secondary location */
	ret = search_and_update(h, data, key, sec_bkt, alt_hash, sig);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		return ret;
	}
	__hash_rw_writer_unlock(h);

	/* Did not find a match, so get a new slot for storing the new key */
	if (h->multi_writer_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Try to get a free slot from the local cache */
		if (cached_free_slots->len == 0) {
			/* Need to get another burst of free slots from global ring */
			n_slots = rte_ring_mc_dequeue_burst(h->free_slots,
					cached_free_slots->objs,
					LCORE_CACHE_SIZE, NULL);
			if (n_slots == 0) {
				return -ENOSPC;
			}

			cached_free_slots->len += n_slots;
		}

		/* Get a free slot from the local cache */
		cached_free_slots->len--;
		slot_id = cached_free_slots->objs[cached_free_slots->len];
	} else {
		if (rte_ring_sc_dequeue(h->free_slots, &slot_id) != 0) {
			return -ENOSPC;
		}
	}

	new_k = RTE_PTR_ADD(keys, (uintptr_t)slot_id * h->key_entry_size);
	new_idx = (uint32_t)((uintptr_t) slot_id);
	/* Copy key */
	rte_memcpy(new_k->key, key, h->key_len);
	new_k->pdata = data;


	/* Find an empty slot and insert */
	ret = rte_hash_cuckoo_insert_mw(h, prim_bkt, sec_bkt, key, data,
					sig, alt_hash, new_idx, &ret_val);
	if (ret == 0)
		return new_idx - 1;
	else if (ret == 1) {
		enqueue_slot_back(h, cached_free_slots, slot_id);
		return ret_val;
	}

	/* Primary bucket full, need to make space for new entry */
	ret = rte_hash_cuckoo_make_space_mw(h, prim_bkt, sec_bkt, key, data,
					sig, alt_hash, new_idx, &ret_val);
	if (ret == 0)
		return new_idx - 1;
	else if (ret == 1) {
		enqueue_slot_back(h, cached_free_slots, slot_id);
		return ret_val;
	}

	/* Also search secondary bucket to get better occupancy */
	ret = rte_hash_cuckoo_make_space_mw(h, sec_bkt, prim_bkt, key, data,
					alt_hash, sig, new_idx, &ret_val);

	if (ret == 0)
		return new_idx - 1;
	else if (ret == 1) {
		enqueue_slot_back(h, cached_free_slots, slot_id);
		return ret_val;
	} else {
		enqueue_slot_back(h, cached_free_slots, slot_id);
		return ret;
	}
}

int32_t
rte_hash_add_key_with_hash(const struct rte_hash *h,
			const void *key, hash_sig_t sig)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_add_key_with_hash(h, key, sig, 0);
}

int32_t
rte_hash_add_key(const struct rte_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_add_key_with_hash(h, key, rte_hash_hash(h, key), 0);
}

int
rte_hash_add_key_with_hash_data(const struct rte_hash *h,
			const void *key, hash_sig_t sig, void *data)
{
	int ret;

	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	ret = __rte_hash_add_key_with_hash(h, key, sig, data);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

int
rte_hash_add_key_data(const struct rte_hash *h, const void *key, void *data)
{
	int ret;

	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);

	ret = __rte_hash_add_key_with_hash(h, key, rte_hash_hash(h, key), data);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

/* Search one bucket to find the match key */
static inline int32_t
search_one_bucket(const struct rte_hash *h, const void *key, hash_sig_t sig,
			void **data, const struct rte_hash_bucket *bkt)
{
	int i;
	struct rte_hash_key *k, *keys = h->key_store;

	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == sig &&
				bkt->key_idx[i] != EMPTY_SLOT) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				if (data != NULL)
					*data = k->pdata;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}
	return -1;
}

static inline int32_t
__rte_hash_lookup_with_hash(const struct rte_hash *h, const void *key,
					hash_sig_t sig, void **data)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	struct rte_hash_bucket *bkt;
	int ret;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	__hash_rw_reader_lock(h);

	/* Check if key is in primary location */
	ret = search_one_bucket(h, key, sig, data, bkt);
	if (ret != -1) {
		__hash_rw_reader_unlock(h);
		return ret;
	}
	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in secondary location */
	ret = search_one_bucket(h, key, alt_hash, data, bkt);
	if (ret != -1) {
		__hash_rw_reader_unlock(h);
		return ret;
	}
	__hash_rw_reader_unlock(h);
	return -ENOENT;
}

int32_t
rte_hash_lookup_with_hash(const struct rte_hash *h,
			const void *key, hash_sig_t sig)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, sig, NULL);
}

int32_t
rte_hash_lookup(const struct rte_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, rte_hash_hash(h, key), NULL);
}

int
rte_hash_lookup_with_hash_data(const struct rte_hash *h,
			const void *key, hash_sig_t sig, void **data)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, sig, data);
}

int
rte_hash_lookup_data(const struct rte_hash *h, const void *key, void **data)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, rte_hash_hash(h, key), data);
}

static inline void
remove_entry(const struct rte_hash *h, struct rte_hash_bucket *bkt, unsigned i)
{
	unsigned lcore_id, n_slots;
	struct lcore_cache *cached_free_slots;

	bkt->sig_current[i] = NULL_SIGNATURE;
	bkt->sig_alt[i] = NULL_SIGNATURE;
	if (h->multi_writer_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Cache full, need to free it. */
		if (cached_free_slots->len == LCORE_CACHE_SIZE) {
			/* Need to enqueue the free slots in global ring. */
			n_slots = rte_ring_mp_enqueue_burst(h->free_slots,
						cached_free_slots->objs,
						LCORE_CACHE_SIZE, NULL);
			cached_free_slots->len -= n_slots;
		}
		/* Put index of new free slot in cache. */
		cached_free_slots->objs[cached_free_slots->len] =
				(void *)((uintptr_t)bkt->key_idx[i]);
		cached_free_slots->len++;
	} else {
		rte_ring_sp_enqueue(h->free_slots,
				(void *)((uintptr_t)bkt->key_idx[i]));
	}
}

/* Search one bucket and remove the matched key */
static inline int32_t
search_and_remove(const struct rte_hash *h, const void *key,
			struct rte_hash_bucket *bkt, hash_sig_t sig)
{
	struct rte_hash_key *k, *keys = h->key_store;
	unsigned int i;
	int32_t ret;

	/* Check if key is in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == sig &&
				bkt->key_idx[i] != EMPTY_SLOT) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				remove_entry(h, bkt, i);

				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				ret = bkt->key_idx[i] - 1;
				bkt->key_idx[i] = EMPTY_SLOT;
				return ret;
			}
		}
	}
	return -1;
}

static inline int32_t
__rte_hash_del_key_with_hash(const struct rte_hash *h, const void *key,
						hash_sig_t sig)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	struct rte_hash_bucket *bkt;
	int32_t ret;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	__hash_rw_writer_lock(h);
	/* look for key in primary bucket */
	ret = search_and_remove(h, key, bkt, sig);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		return ret;
	}

	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* look for key in secondary bucket */
	ret = search_and_remove(h, key, bkt, alt_hash);
	if (ret != -1) {
		__hash_rw_writer_unlock(h);
		return ret;
	}

	__hash_rw_writer_unlock(h);
	return -ENOENT;
}

int32_t
rte_hash_del_key_with_hash(const struct rte_hash *h,
			const void *key, hash_sig_t sig)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_del_key_with_hash(h, key, sig);
}

int32_t
rte_hash_del_key(const struct rte_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_del_key_with_hash(h, key, rte_hash_hash(h, key));
}

int
rte_hash_get_key_with_position(const struct rte_hash *h, const int32_t position,
			       void **key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);

	struct rte_hash_key *k, *keys = h->key_store;
	k = (struct rte_hash_key *) ((char *) keys + (position + 1) *
				     h->key_entry_size);
	*key = k->key;

	if (position !=
	    __rte_hash_lookup_with_hash(h, *key, rte_hash_hash(h, *key),
					NULL)) {
		return -ENOENT;
	}

	return 0;
}

static inline void
compare_signatures(uint32_t *prim_hash_matches, uint32_t *sec_hash_matches,
			const struct rte_hash_bucket *prim_bkt,
			const struct rte_hash_bucket *sec_bkt,
			hash_sig_t prim_hash, hash_sig_t sec_hash,
			enum rte_hash_sig_compare_function sig_cmp_fn)
{
	unsigned int i;

	switch (sig_cmp_fn) {
#ifdef RTE_MACHINE_CPUFLAG_AVX2
	case RTE_HASH_COMPARE_AVX2:
		*prim_hash_matches = _mm256_movemask_ps((__m256)_mm256_cmpeq_epi32(
				_mm256_load_si256(
					(__m256i const *)prim_bkt->sig_current),
				_mm256_set1_epi32(prim_hash)));
		*sec_hash_matches = _mm256_movemask_ps((__m256)_mm256_cmpeq_epi32(
				_mm256_load_si256(
					(__m256i const *)sec_bkt->sig_current),
				_mm256_set1_epi32(sec_hash)));
		break;
#endif
#ifdef RTE_MACHINE_CPUFLAG_SSE2
	case RTE_HASH_COMPARE_SSE:
		/* Compare the first 4 signatures in the bucket */
		*prim_hash_matches = _mm_movemask_ps((__m128)_mm_cmpeq_epi16(
				_mm_load_si128(
					(__m128i const *)prim_bkt->sig_current),
				_mm_set1_epi32(prim_hash)));
		*prim_hash_matches |= (_mm_movemask_ps((__m128)_mm_cmpeq_epi16(
				_mm_load_si128(
					(__m128i const *)&prim_bkt->sig_current[4]),
				_mm_set1_epi32(prim_hash)))) << 4;
		/* Compare the first 4 signatures in the bucket */
		*sec_hash_matches = _mm_movemask_ps((__m128)_mm_cmpeq_epi16(
				_mm_load_si128(
					(__m128i const *)sec_bkt->sig_current),
				_mm_set1_epi32(sec_hash)));
		*sec_hash_matches |= (_mm_movemask_ps((__m128)_mm_cmpeq_epi16(
				_mm_load_si128(
					(__m128i const *)&sec_bkt->sig_current[4]),
				_mm_set1_epi32(sec_hash)))) << 4;
		break;
#endif
	default:
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			*prim_hash_matches |=
				((prim_hash == prim_bkt->sig_current[i]) << i);
			*sec_hash_matches |=
				((sec_hash == sec_bkt->sig_current[i]) << i);
		}
	}

}

#define PREFETCH_OFFSET 4
static inline void
__rte_hash_lookup_bulk(const struct rte_hash *h, const void **keys,
			int32_t num_keys, int32_t *positions,
			uint64_t *hit_mask, void *data[])
{
	uint64_t hits = 0;
	int32_t i;
	uint32_t prim_hash[RTE_HASH_LOOKUP_BULK_MAX];
	uint32_t sec_hash[RTE_HASH_LOOKUP_BULK_MAX];
	const struct rte_hash_bucket *primary_bkt[RTE_HASH_LOOKUP_BULK_MAX];
	const struct rte_hash_bucket *secondary_bkt[RTE_HASH_LOOKUP_BULK_MAX];
	uint32_t prim_hitmask[RTE_HASH_LOOKUP_BULK_MAX] = {0};
	uint32_t sec_hitmask[RTE_HASH_LOOKUP_BULK_MAX] = {0};

	/* Prefetch first keys */
	for (i = 0; i < PREFETCH_OFFSET && i < num_keys; i++)
		rte_prefetch0(keys[i]);

	/*
	 * Prefetch rest of the keys, calculate primary and
	 * secondary bucket and prefetch them
	 */
	for (i = 0; i < (num_keys - PREFETCH_OFFSET); i++) {
		rte_prefetch0(keys[i + PREFETCH_OFFSET]);

		prim_hash[i] = rte_hash_hash(h, keys[i]);
		sec_hash[i] = rte_hash_secondary_hash(prim_hash[i]);

		primary_bkt[i] = &h->buckets[prim_hash[i] & h->bucket_bitmask];
		secondary_bkt[i] = &h->buckets[sec_hash[i] & h->bucket_bitmask];

		rte_prefetch0(primary_bkt[i]);
		rte_prefetch0(secondary_bkt[i]);
	}

	/* Calculate and prefetch rest of the buckets */
	for (; i < num_keys; i++) {
		prim_hash[i] = rte_hash_hash(h, keys[i]);
		sec_hash[i] = rte_hash_secondary_hash(prim_hash[i]);

		primary_bkt[i] = &h->buckets[prim_hash[i] & h->bucket_bitmask];
		secondary_bkt[i] = &h->buckets[sec_hash[i] & h->bucket_bitmask];

		rte_prefetch0(primary_bkt[i]);
		rte_prefetch0(secondary_bkt[i]);
	}

	__hash_rw_reader_lock(h);
	/* Compare signatures and prefetch key slot of first hit */
	for (i = 0; i < num_keys; i++) {
		compare_signatures(&prim_hitmask[i], &sec_hitmask[i],
				primary_bkt[i], secondary_bkt[i],
				prim_hash[i], sec_hash[i], h->sig_cmp_fn);

		if (prim_hitmask[i]) {
			uint32_t first_hit = __builtin_ctzl(prim_hitmask[i]);
			uint32_t key_idx = primary_bkt[i]->key_idx[first_hit];
			const struct rte_hash_key *key_slot =
				(const struct rte_hash_key *)(
				(const char *)h->key_store +
				key_idx * h->key_entry_size);
			rte_prefetch0(key_slot);
			continue;
		}

		if (sec_hitmask[i]) {
			uint32_t first_hit = __builtin_ctzl(sec_hitmask[i]);
			uint32_t key_idx = secondary_bkt[i]->key_idx[first_hit];
			const struct rte_hash_key *key_slot =
				(const struct rte_hash_key *)(
				(const char *)h->key_store +
				key_idx * h->key_entry_size);
			rte_prefetch0(key_slot);
		}
	}

	/* Compare keys, first hits in primary first */
	for (i = 0; i < num_keys; i++) {
		positions[i] = -ENOENT;
		while (prim_hitmask[i]) {
			uint32_t hit_index = __builtin_ctzl(prim_hitmask[i]);

			uint32_t key_idx = primary_bkt[i]->key_idx[hit_index];
			const struct rte_hash_key *key_slot =
				(const struct rte_hash_key *)(
				(const char *)h->key_store +
				key_idx * h->key_entry_size);
			/*
			 * If key index is 0, do not compare key,
			 * as it is checking the dummy slot
			 */
			if (!!key_idx & !rte_hash_cmp_eq(key_slot->key, keys[i], h)) {
				if (data != NULL)
					data[i] = key_slot->pdata;

				hits |= 1ULL << i;
				positions[i] = key_idx - 1;
				goto next_key;
			}
			prim_hitmask[i] &= ~(1 << (hit_index));
		}

		while (sec_hitmask[i]) {
			uint32_t hit_index = __builtin_ctzl(sec_hitmask[i]);

			uint32_t key_idx = secondary_bkt[i]->key_idx[hit_index];
			const struct rte_hash_key *key_slot =
				(const struct rte_hash_key *)(
				(const char *)h->key_store +
				key_idx * h->key_entry_size);
			/*
			 * If key index is 0, do not compare key,
			 * as it is checking the dummy slot
			 */

			if (!!key_idx & !rte_hash_cmp_eq(key_slot->key, keys[i], h)) {
				if (data != NULL)
					data[i] = key_slot->pdata;

				hits |= 1ULL << i;
				positions[i] = key_idx - 1;
				goto next_key;
			}
			sec_hitmask[i] &= ~(1 << (hit_index));
		}

next_key:
		continue;
	}

	__hash_rw_reader_unlock(h);

	if (hit_mask != NULL)
		*hit_mask = hits;
}

int
rte_hash_lookup_bulk(const struct rte_hash *h, const void **keys,
		      uint32_t num_keys, int32_t *positions)
{
	RETURN_IF_TRUE(((h == NULL) || (keys == NULL) || (num_keys == 0) ||
			(num_keys > RTE_HASH_LOOKUP_BULK_MAX) ||
			(positions == NULL)), -EINVAL);

	__rte_hash_lookup_bulk(h, keys, num_keys, positions, NULL, NULL);
	return 0;
}

int
rte_hash_lookup_bulk_data(const struct rte_hash *h, const void **keys,
		      uint32_t num_keys, uint64_t *hit_mask, void *data[])
{
	RETURN_IF_TRUE(((h == NULL) || (keys == NULL) || (num_keys == 0) ||
			(num_keys > RTE_HASH_LOOKUP_BULK_MAX) ||
			(hit_mask == NULL)), -EINVAL);

	int32_t positions[num_keys];

	__rte_hash_lookup_bulk(h, keys, num_keys, positions, hit_mask, data);

	/* Return number of hits */
	return __builtin_popcountl(*hit_mask);
}

int32_t
rte_hash_iterate(const struct rte_hash *h, const void **key, void **data, uint32_t *next)
{
	uint32_t bucket_idx, idx, position;
	struct rte_hash_key *next_key;

	RETURN_IF_TRUE(((h == NULL) || (next == NULL)), -EINVAL);

	const uint32_t total_entries = h->num_buckets * RTE_HASH_BUCKET_ENTRIES;
	/* Out of bounds */
	if (*next >= total_entries)
		return -ENOENT;

	/* Calculate bucket and index of current iterator */
	bucket_idx = *next / RTE_HASH_BUCKET_ENTRIES;
	idx = *next % RTE_HASH_BUCKET_ENTRIES;

	/* If current position is empty, go to the next one */
	while (h->buckets[bucket_idx].key_idx[idx] == EMPTY_SLOT) {
		(*next)++;
		/* End of table */
		if (*next == total_entries)
			return -ENOENT;
		bucket_idx = *next / RTE_HASH_BUCKET_ENTRIES;
		idx = *next % RTE_HASH_BUCKET_ENTRIES;
	}
	__hash_rw_reader_lock(h);
	/* Get position of entry in key table */
	position = h->buckets[bucket_idx].key_idx[idx];
	next_key = (struct rte_hash_key *) ((char *)h->key_store +
				position * h->key_entry_size);
	/* Return key and data */
	*key = next_key->key;
	*data = next_key->pdata;

	__hash_rw_reader_unlock(h);

	/* Increment iterator */
	(*next)++;

	return position - 1;
}
