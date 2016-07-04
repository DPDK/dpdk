/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_log.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_ring.h>
#include <rte_compat.h>

#include "rte_hash.h"
#include "rte_cuckoo_hash.h"

#if defined(RTE_ARCH_X86)
#include "rte_cuckoo_hash_x86.h"
#endif

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
	unsigned hw_trans_mem_support = 0;
	unsigned i;

	hash_list = RTE_TAILQ_CAST(rte_hash_tailq.head, rte_hash_list);

	if (params == NULL) {
		RTE_LOG(ERR, HASH, "rte_hash_create has no parameters\n");
		return NULL;
	}

	/* Check for valid parameters */
	if ((params->entries > RTE_HASH_ENTRIES_MAX) ||
			(params->entries < RTE_HASH_BUCKET_ENTRIES) ||
			!rte_is_power_of_2(RTE_HASH_BUCKET_ENTRIES) ||
			(params->key_len == 0)) {
		rte_errno = EINVAL;
		RTE_LOG(ERR, HASH, "rte_hash_create has invalid parameters\n");
		return NULL;
	}

	/* Check extra flags field to check extra options. */
	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT)
		hw_trans_mem_support = 1;

	/* Store all keys and leave the first entry as a dummy entry for lookup_bulk */
	if (hw_trans_mem_support)
		/*
		 * Increase number of slots by total number of indices
		 * that can be stored in the lcore caches
		 * except for the first cache
		 */
		num_key_slots = params->entries + (RTE_MAX_LCORE - 1) *
					LCORE_CACHE_SIZE + 1;
	else
		num_key_slots = params->entries + 1;

	snprintf(ring_name, sizeof(ring_name), "HT_%s", params->name);
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

	if (hw_trans_mem_support) {
		h->local_free_slots = rte_zmalloc_socket(NULL,
				sizeof(struct lcore_cache) * RTE_MAX_LCORE,
				RTE_CACHE_LINE_SIZE, params->socket_id);
	}

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
		DEFAULT_HASH_FUNC : params->hash_func;
	h->key_store = k;
	h->free_slots = r;
	h->hw_trans_mem_support = hw_trans_mem_support;

	/* Turn on multi-writer only with explicit flat from user and TM
	 * support.
	 */
	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD) {
		if (h->hw_trans_mem_support) {
			h->add_key = ADD_KEY_MULTIWRITER_TM;
		} else {
			h->add_key = ADD_KEY_MULTIWRITER;
			h->multiwriter_lock = rte_malloc(NULL,
							sizeof(rte_spinlock_t),
							LCORE_CACHE_SIZE);
			rte_spinlock_init(h->multiwriter_lock);
		}
	} else
		h->add_key = ADD_KEY_SINGLEWRITER;

	/* Populate free slots ring. Entry zero is reserved for key misses. */
	for (i = 1; i < params->entries + 1; i++)
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

	if (h->hw_trans_mem_support)
		rte_free(h->local_free_slots);

	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_free(h->multiwriter_lock);
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

void
rte_hash_reset(struct rte_hash *h)
{
	void *ptr;
	unsigned i;

	if (h == NULL)
		return;

	memset(h->buckets, 0, h->num_buckets * sizeof(struct rte_hash_bucket));
	memset(h->key_store, 0, h->key_entry_size * (h->entries + 1));

	/* clear the free ring */
	while (rte_ring_dequeue(h->free_slots, &ptr) == 0)
		rte_pause();

	/* Repopulate the free slots ring. Entry zero is reserved for key misses */
	for (i = 1; i < h->entries + 1; i++)
		rte_ring_sp_enqueue(h->free_slots, (void *)((uintptr_t) i));

	if (h->hw_trans_mem_support) {
		/* Reset local caches per lcore */
		for (i = 0; i < RTE_MAX_LCORE; i++)
			h->local_free_slots[i].len = 0;
	}
}

/* Search for an entry that can be pushed to its alternative location */
static inline int
make_space_bucket(const struct rte_hash *h, struct rte_hash_bucket *bkt)
{
	unsigned i, j;
	int ret;
	uint32_t next_bucket_idx;
	struct rte_hash_bucket *next_bkt[RTE_HASH_BUCKET_ENTRIES];

	/*
	 * Push existing item (search for bucket with space in
	 * alternative locations) to its alternative location
	 */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		/* Search for space in alternative locations */
		next_bucket_idx = bkt->signatures[i].alt & h->bucket_bitmask;
		next_bkt[i] = &h->buckets[next_bucket_idx];
		for (j = 0; j < RTE_HASH_BUCKET_ENTRIES; j++) {
			if (next_bkt[i]->signatures[j].sig == NULL_SIGNATURE)
				break;
		}

		if (j != RTE_HASH_BUCKET_ENTRIES)
			break;
	}

	/* Alternative location has spare room (end of recursive function) */
	if (i != RTE_HASH_BUCKET_ENTRIES) {
		next_bkt[i]->signatures[j].alt = bkt->signatures[i].current;
		next_bkt[i]->signatures[j].current = bkt->signatures[i].alt;
		next_bkt[i]->key_idx[j] = bkt->key_idx[i];
		return i;
	}

	/* Pick entry that has not been pushed yet */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++)
		if (bkt->flag[i] == 0)
			break;

	/* All entries have been pushed, so entry cannot be added */
	if (i == RTE_HASH_BUCKET_ENTRIES)
		return -ENOSPC;

	/* Set flag to indicate that this entry is going to be pushed */
	bkt->flag[i] = 1;
	/* Need room in alternative bucket to insert the pushed entry */
	ret = make_space_bucket(h, next_bkt[i]);
	/*
	 * After recursive function.
	 * Clear flags and insert the pushed entry
	 * in its alternative location if successful,
	 * or return error
	 */
	bkt->flag[i] = 0;
	if (ret >= 0) {
		next_bkt[i]->signatures[ret].alt = bkt->signatures[i].current;
		next_bkt[i]->signatures[ret].current = bkt->signatures[i].alt;
		next_bkt[i]->key_idx[ret] = bkt->key_idx[i];
		return i;
	} else
		return ret;

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
	if (h->hw_trans_mem_support) {
		cached_free_slots->objs[cached_free_slots->len] = slot_id;
		cached_free_slots->len++;
	} else
		rte_ring_sp_enqueue(h->free_slots, slot_id);
}

static inline int32_t
__rte_hash_add_key_with_hash(const struct rte_hash *h, const void *key,
						hash_sig_t sig, void *data)
{
	hash_sig_t alt_hash;
	uint32_t prim_bucket_idx, sec_bucket_idx;
	unsigned i;
	struct rte_hash_bucket *prim_bkt, *sec_bkt;
	struct rte_hash_key *new_k, *k, *keys = h->key_store;
	void *slot_id = NULL;
	uint32_t new_idx;
	int ret;
	unsigned n_slots;
	unsigned lcore_id;
	struct lcore_cache *cached_free_slots = NULL;

	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_spinlock_lock(h->multiwriter_lock);

	prim_bucket_idx = sig & h->bucket_bitmask;
	prim_bkt = &h->buckets[prim_bucket_idx];
	rte_prefetch0(prim_bkt);

	alt_hash = rte_hash_secondary_hash(sig);
	sec_bucket_idx = alt_hash & h->bucket_bitmask;
	sec_bkt = &h->buckets[sec_bucket_idx];
	rte_prefetch0(sec_bkt);

	/* Get a new slot for storing the new key */
	if (h->hw_trans_mem_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Try to get a free slot from the local cache */
		if (cached_free_slots->len == 0) {
			/* Need to get another burst of free slots from global ring */
			n_slots = rte_ring_mc_dequeue_burst(h->free_slots,
					cached_free_slots->objs, LCORE_CACHE_SIZE);
			if (n_slots == 0)
				return -ENOSPC;

			cached_free_slots->len += n_slots;
		}

		/* Get a free slot from the local cache */
		cached_free_slots->len--;
		slot_id = cached_free_slots->objs[cached_free_slots->len];
	} else {
		if (rte_ring_sc_dequeue(h->free_slots, &slot_id) != 0)
			return -ENOSPC;
	}

	new_k = RTE_PTR_ADD(keys, (uintptr_t)slot_id * h->key_entry_size);
	rte_prefetch0(new_k);
	new_idx = (uint32_t)((uintptr_t) slot_id);

	/* Check if key is already inserted in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (prim_bkt->signatures[i].current == sig &&
				prim_bkt->signatures[i].alt == alt_hash) {
			k = (struct rte_hash_key *) ((char *)keys +
					prim_bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				/* Enqueue index of free slot back in the ring. */
				enqueue_slot_back(h, cached_free_slots, slot_id);
				/* Update data */
				k->pdata = data;
				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return prim_bkt->key_idx[i] - 1;
			}
		}
	}

	/* Check if key is already inserted in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (sec_bkt->signatures[i].alt == sig &&
				sec_bkt->signatures[i].current == alt_hash) {
			k = (struct rte_hash_key *) ((char *)keys +
					sec_bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				/* Enqueue index of free slot back in the ring. */
				enqueue_slot_back(h, cached_free_slots, slot_id);
				/* Update data */
				k->pdata = data;
				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return sec_bkt->key_idx[i] - 1;
			}
		}
	}

	/* Copy key */
	rte_memcpy(new_k->key, key, h->key_len);
	new_k->pdata = data;

#if defined(RTE_ARCH_X86) /* currently only x86 support HTM */
	if (h->add_key == ADD_KEY_MULTIWRITER_TM) {
		ret = rte_hash_cuckoo_insert_mw_tm(prim_bkt,
				sig, alt_hash, new_idx);
		if (ret >= 0)
			return new_idx - 1;

		/* Primary bucket full, need to make space for new entry */
		ret = rte_hash_cuckoo_make_space_mw_tm(h, prim_bkt, sig,
							alt_hash, new_idx);

		if (ret >= 0)
			return new_idx - 1;

		/* Also search secondary bucket to get better occupancy */
		ret = rte_hash_cuckoo_make_space_mw_tm(h, sec_bkt, sig,
							alt_hash, new_idx);

		if (ret >= 0)
			return new_idx - 1;
	} else {
#endif
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			/* Check if slot is available */
			if (likely(prim_bkt->signatures[i].sig == NULL_SIGNATURE)) {
				prim_bkt->signatures[i].current = sig;
				prim_bkt->signatures[i].alt = alt_hash;
				prim_bkt->key_idx[i] = new_idx;
				break;
			}
		}

		if (i != RTE_HASH_BUCKET_ENTRIES) {
			if (h->add_key == ADD_KEY_MULTIWRITER)
				rte_spinlock_unlock(h->multiwriter_lock);
			return new_idx - 1;
		}

		/* Primary bucket full, need to make space for new entry
		 * After recursive function.
		 * Insert the new entry in the position of the pushed entry
		 * if successful or return error and
		 * store the new slot back in the ring
		 */
		ret = make_space_bucket(h, prim_bkt);
		if (ret >= 0) {
			prim_bkt->signatures[ret].current = sig;
			prim_bkt->signatures[ret].alt = alt_hash;
			prim_bkt->key_idx[ret] = new_idx;
			if (h->add_key == ADD_KEY_MULTIWRITER)
				rte_spinlock_unlock(h->multiwriter_lock);
			return new_idx - 1;
		}
#if defined(RTE_ARCH_X86)
	}
#endif
	/* Error in addition, store new slot back in the ring and return error */
	enqueue_slot_back(h, cached_free_slots, (void *)((uintptr_t) new_idx));

	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_spinlock_unlock(h->multiwriter_lock);
	return ret;
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
static inline int32_t
__rte_hash_lookup_with_hash(const struct rte_hash *h, const void *key,
					hash_sig_t sig, void **data)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	unsigned i;
	struct rte_hash_bucket *bkt;
	struct rte_hash_key *k, *keys = h->key_store;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->signatures[i].current == sig &&
				bkt->signatures[i].sig != NULL_SIGNATURE) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				if (data != NULL)
					*data = k->pdata;
				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->signatures[i].current == alt_hash &&
				bkt->signatures[i].alt == sig) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				if (data != NULL)
					*data = k->pdata;
				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

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

	bkt->signatures[i].sig = NULL_SIGNATURE;
	if (h->hw_trans_mem_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Cache full, need to free it. */
		if (cached_free_slots->len == LCORE_CACHE_SIZE) {
			/* Need to enqueue the free slots in global ring. */
			n_slots = rte_ring_mp_enqueue_burst(h->free_slots,
						cached_free_slots->objs,
						LCORE_CACHE_SIZE);
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

static inline int32_t
__rte_hash_del_key_with_hash(const struct rte_hash *h, const void *key,
						hash_sig_t sig)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	unsigned i;
	struct rte_hash_bucket *bkt;
	struct rte_hash_key *k, *keys = h->key_store;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->signatures[i].current == sig &&
				bkt->signatures[i].sig != NULL_SIGNATURE) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				remove_entry(h, bkt, i);

				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->signatures[i].current == alt_hash &&
				bkt->signatures[i].sig != NULL_SIGNATURE) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				remove_entry(h, bkt, i);

				/*
				 * Return index where key is stored,
				 * substracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

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

/* Lookup bulk stage 0: Prefetch input key */
static inline void
lookup_stage0(unsigned *idx, uint64_t *lookup_mask,
		const void * const *keys)
{
	*idx = __builtin_ctzl(*lookup_mask);
	if (*lookup_mask == 0)
		*idx = 0;

	rte_prefetch0(keys[*idx]);
	*lookup_mask &= ~(1llu << *idx);
}

/*
 * Lookup bulk stage 1: Calculate primary/secondary hashes
 * and prefetch primary/secondary buckets
 */
static inline void
lookup_stage1(unsigned idx, hash_sig_t *prim_hash, hash_sig_t *sec_hash,
		const struct rte_hash_bucket **primary_bkt,
		const struct rte_hash_bucket **secondary_bkt,
		hash_sig_t *hash_vals, const void * const *keys,
		const struct rte_hash *h)
{
	*prim_hash = rte_hash_hash(h, keys[idx]);
	hash_vals[idx] = *prim_hash;
	*sec_hash = rte_hash_secondary_hash(*prim_hash);

	*primary_bkt = &h->buckets[*prim_hash & h->bucket_bitmask];
	*secondary_bkt = &h->buckets[*sec_hash & h->bucket_bitmask];

	rte_prefetch0(*primary_bkt);
	rte_prefetch0(*secondary_bkt);
}

/*
 * Lookup bulk stage 2:  Search for match hashes in primary/secondary locations
 * and prefetch first key slot
 */
static inline void
lookup_stage2(unsigned idx, hash_sig_t prim_hash, hash_sig_t sec_hash,
		const struct rte_hash_bucket *prim_bkt,
		const struct rte_hash_bucket *sec_bkt,
		const struct rte_hash_key **key_slot, int32_t *positions,
		uint64_t *extra_hits_mask, const void *keys,
		const struct rte_hash *h)
{
	unsigned prim_hash_matches, sec_hash_matches, key_idx, i;
	unsigned total_hash_matches;

	prim_hash_matches = 1 << RTE_HASH_BUCKET_ENTRIES;
	sec_hash_matches = 1 << RTE_HASH_BUCKET_ENTRIES;
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		prim_hash_matches |= ((prim_hash == prim_bkt->signatures[i].current) << i);
		sec_hash_matches |= ((sec_hash == sec_bkt->signatures[i].current) << i);
	}

	key_idx = prim_bkt->key_idx[__builtin_ctzl(prim_hash_matches)];
	if (key_idx == 0)
		key_idx = sec_bkt->key_idx[__builtin_ctzl(sec_hash_matches)];

	total_hash_matches = (prim_hash_matches |
				(sec_hash_matches << (RTE_HASH_BUCKET_ENTRIES + 1)));
	*key_slot = (const struct rte_hash_key *) ((const char *)keys +
					key_idx * h->key_entry_size);

	rte_prefetch0(*key_slot);
	/*
	 * Return index where key is stored,
	 * substracting the first dummy index
	 */
	positions[idx] = (key_idx - 1);

	*extra_hits_mask |= (uint64_t)(__builtin_popcount(total_hash_matches) > 3) << idx;

}


/* Lookup bulk stage 3: Check if key matches, update hit mask and return data */
static inline void
lookup_stage3(unsigned idx, const struct rte_hash_key *key_slot, const void * const *keys,
		const int32_t *positions, void *data[], uint64_t *hits,
		const struct rte_hash *h)
{
	unsigned hit;
	unsigned key_idx;

	hit = !rte_hash_cmp_eq(key_slot->key, keys[idx], h);
	if (data != NULL)
		data[idx] = key_slot->pdata;

	key_idx = positions[idx] + 1;
	/*
	 * If key index is 0, force hit to be 0, in case key to be looked up
	 * is all zero (as in the dummy slot), which would result in a wrong hit
	 */
	*hits |= (uint64_t)(hit && !!key_idx)  << idx;
}

static inline void
__rte_hash_lookup_bulk(const struct rte_hash *h, const void **keys,
			uint32_t num_keys, int32_t *positions,
			uint64_t *hit_mask, void *data[])
{
	uint64_t hits = 0;
	uint64_t extra_hits_mask = 0;
	uint64_t lookup_mask, miss_mask;
	unsigned idx;
	const void *key_store = h->key_store;
	int ret;
	hash_sig_t hash_vals[RTE_HASH_LOOKUP_BULK_MAX];

	unsigned idx00, idx01, idx10, idx11, idx20, idx21, idx30, idx31;
	const struct rte_hash_bucket *primary_bkt10, *primary_bkt11;
	const struct rte_hash_bucket *secondary_bkt10, *secondary_bkt11;
	const struct rte_hash_bucket *primary_bkt20, *primary_bkt21;
	const struct rte_hash_bucket *secondary_bkt20, *secondary_bkt21;
	const struct rte_hash_key *k_slot20, *k_slot21, *k_slot30, *k_slot31;
	hash_sig_t primary_hash10, primary_hash11;
	hash_sig_t secondary_hash10, secondary_hash11;
	hash_sig_t primary_hash20, primary_hash21;
	hash_sig_t secondary_hash20, secondary_hash21;

	lookup_mask = (uint64_t) -1 >> (64 - num_keys);
	miss_mask = lookup_mask;

	lookup_stage0(&idx00, &lookup_mask, keys);
	lookup_stage0(&idx01, &lookup_mask, keys);

	idx10 = idx00, idx11 = idx01;

	lookup_stage0(&idx00, &lookup_mask, keys);
	lookup_stage0(&idx01, &lookup_mask, keys);
	lookup_stage1(idx10, &primary_hash10, &secondary_hash10,
			&primary_bkt10, &secondary_bkt10, hash_vals, keys, h);
	lookup_stage1(idx11, &primary_hash11, &secondary_hash11,
			&primary_bkt11,	&secondary_bkt11, hash_vals, keys, h);

	primary_bkt20 = primary_bkt10;
	primary_bkt21 = primary_bkt11;
	secondary_bkt20 = secondary_bkt10;
	secondary_bkt21 = secondary_bkt11;
	primary_hash20 = primary_hash10;
	primary_hash21 = primary_hash11;
	secondary_hash20 = secondary_hash10;
	secondary_hash21 = secondary_hash11;
	idx20 = idx10, idx21 = idx11;
	idx10 = idx00, idx11 = idx01;

	lookup_stage0(&idx00, &lookup_mask, keys);
	lookup_stage0(&idx01, &lookup_mask, keys);
	lookup_stage1(idx10, &primary_hash10, &secondary_hash10,
			&primary_bkt10, &secondary_bkt10, hash_vals, keys, h);
	lookup_stage1(idx11, &primary_hash11, &secondary_hash11,
			&primary_bkt11,	&secondary_bkt11, hash_vals, keys, h);
	lookup_stage2(idx20, primary_hash20, secondary_hash20, primary_bkt20,
			secondary_bkt20, &k_slot20, positions, &extra_hits_mask,
			key_store, h);
	lookup_stage2(idx21, primary_hash21, secondary_hash21, primary_bkt21,
			secondary_bkt21, &k_slot21, positions, &extra_hits_mask,
			key_store, h);

	while (lookup_mask) {
		k_slot30 = k_slot20, k_slot31 = k_slot21;
		idx30 = idx20, idx31 = idx21;
		primary_bkt20 = primary_bkt10;
		primary_bkt21 = primary_bkt11;
		secondary_bkt20 = secondary_bkt10;
		secondary_bkt21 = secondary_bkt11;
		primary_hash20 = primary_hash10;
		primary_hash21 = primary_hash11;
		secondary_hash20 = secondary_hash10;
		secondary_hash21 = secondary_hash11;
		idx20 = idx10, idx21 = idx11;
		idx10 = idx00, idx11 = idx01;

		lookup_stage0(&idx00, &lookup_mask, keys);
		lookup_stage0(&idx01, &lookup_mask, keys);
		lookup_stage1(idx10, &primary_hash10, &secondary_hash10,
			&primary_bkt10, &secondary_bkt10, hash_vals, keys, h);
		lookup_stage1(idx11, &primary_hash11, &secondary_hash11,
			&primary_bkt11,	&secondary_bkt11, hash_vals, keys, h);
		lookup_stage2(idx20, primary_hash20, secondary_hash20,
			primary_bkt20, secondary_bkt20, &k_slot20, positions,
			&extra_hits_mask, key_store, h);
		lookup_stage2(idx21, primary_hash21, secondary_hash21,
			primary_bkt21, secondary_bkt21,	&k_slot21, positions,
			&extra_hits_mask, key_store, h);
		lookup_stage3(idx30, k_slot30, keys, positions, data, &hits, h);
		lookup_stage3(idx31, k_slot31, keys, positions, data, &hits, h);
	}

	k_slot30 = k_slot20, k_slot31 = k_slot21;
	idx30 = idx20, idx31 = idx21;
	primary_bkt20 = primary_bkt10;
	primary_bkt21 = primary_bkt11;
	secondary_bkt20 = secondary_bkt10;
	secondary_bkt21 = secondary_bkt11;
	primary_hash20 = primary_hash10;
	primary_hash21 = primary_hash11;
	secondary_hash20 = secondary_hash10;
	secondary_hash21 = secondary_hash11;
	idx20 = idx10, idx21 = idx11;
	idx10 = idx00, idx11 = idx01;

	lookup_stage1(idx10, &primary_hash10, &secondary_hash10,
		&primary_bkt10, &secondary_bkt10, hash_vals, keys, h);
	lookup_stage1(idx11, &primary_hash11, &secondary_hash11,
		&primary_bkt11,	&secondary_bkt11, hash_vals, keys, h);
	lookup_stage2(idx20, primary_hash20, secondary_hash20, primary_bkt20,
		secondary_bkt20, &k_slot20, positions, &extra_hits_mask,
		key_store, h);
	lookup_stage2(idx21, primary_hash21, secondary_hash21, primary_bkt21,
		secondary_bkt21, &k_slot21, positions, &extra_hits_mask,
		key_store, h);
	lookup_stage3(idx30, k_slot30, keys, positions, data, &hits, h);
	lookup_stage3(idx31, k_slot31, keys, positions, data, &hits, h);

	k_slot30 = k_slot20, k_slot31 = k_slot21;
	idx30 = idx20, idx31 = idx21;
	primary_bkt20 = primary_bkt10;
	primary_bkt21 = primary_bkt11;
	secondary_bkt20 = secondary_bkt10;
	secondary_bkt21 = secondary_bkt11;
	primary_hash20 = primary_hash10;
	primary_hash21 = primary_hash11;
	secondary_hash20 = secondary_hash10;
	secondary_hash21 = secondary_hash11;
	idx20 = idx10, idx21 = idx11;

	lookup_stage2(idx20, primary_hash20, secondary_hash20, primary_bkt20,
		secondary_bkt20, &k_slot20, positions, &extra_hits_mask,
		key_store, h);
	lookup_stage2(idx21, primary_hash21, secondary_hash21, primary_bkt21,
		secondary_bkt21, &k_slot21, positions, &extra_hits_mask,
		key_store, h);
	lookup_stage3(idx30, k_slot30, keys, positions, data, &hits, h);
	lookup_stage3(idx31, k_slot31, keys, positions, data, &hits, h);

	k_slot30 = k_slot20, k_slot31 = k_slot21;
	idx30 = idx20, idx31 = idx21;

	lookup_stage3(idx30, k_slot30, keys, positions, data, &hits, h);
	lookup_stage3(idx31, k_slot31, keys, positions, data, &hits, h);

	/* ignore any items we have already found */
	extra_hits_mask &= ~hits;

	if (unlikely(extra_hits_mask)) {
		/* run a single search for each remaining item */
		do {
			idx = __builtin_ctzl(extra_hits_mask);
			if (data != NULL) {
				ret = rte_hash_lookup_with_hash_data(h,
						keys[idx], hash_vals[idx], &data[idx]);
				if (ret >= 0)
					hits |= 1ULL << idx;
			} else {
				positions[idx] = rte_hash_lookup_with_hash(h,
							keys[idx], hash_vals[idx]);
				if (positions[idx] >= 0)
					hits |= 1llu << idx;
			}
			extra_hits_mask &= ~(1llu << idx);
		} while (extra_hits_mask);
	}

	miss_mask &= ~hits;
	if (unlikely(miss_mask)) {
		do {
			idx = __builtin_ctzl(miss_mask);
			positions[idx] = -ENOENT;
			miss_mask &= ~(1llu << idx);
		} while (miss_mask);
	}

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
	while (h->buckets[bucket_idx].signatures[idx].sig == NULL_SIGNATURE) {
		(*next)++;
		/* End of table */
		if (*next == total_entries)
			return -ENOENT;
		bucket_idx = *next / RTE_HASH_BUCKET_ENTRIES;
		idx = *next % RTE_HASH_BUCKET_ENTRIES;
	}

	/* Get position of entry in key table */
	position = h->buckets[bucket_idx].key_idx[idx];
	next_key = (struct rte_hash_key *) ((char *)h->key_store +
				position * h->key_entry_size);
	/* Return key and data */
	*key = next_key->key;
	*data = next_key->pdata;

	/* Increment iterator */
	(*next)++;

	return position - 1;
}
