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

#include "rte_hash.h"

TAILQ_HEAD(rte_hash_list, rte_tailq_entry);

static struct rte_tailq_elem rte_hash_tailq = {
	.name = "RTE_HASH",
};
EAL_REGISTER_TAILQ(rte_hash_tailq)

/* Macro to enable/disable run-time checking of function parameters */
#if defined(RTE_LIBRTE_HASH_DEBUG)
#define RETURN_IF_TRUE(cond, retval) do { \
	if (cond) return (retval); \
} while (0)
#else
#define RETURN_IF_TRUE(cond, retval)
#endif

/* Hash function used if none is specified */
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif

/* Signature bucket size is a multiple of this value */
#define SIG_BUCKET_ALIGNMENT    16

/* Stoered key size is a multiple of this value */
#define KEY_ALIGNMENT           16

/* The high bit is always set in real signatures */
#define NULL_SIGNATURE          0

/* Returns a pointer to the first signature in specified bucket. */
static inline hash_sig_t *
get_sig_tbl_bucket(const struct rte_hash *h, uint32_t bucket_index)
{
	return (hash_sig_t *)
			&(h->sig_tbl[bucket_index * h->sig_tbl_bucket_size]);
}

/* Returns a pointer to the first key in specified bucket. */
static inline uint8_t *
get_key_tbl_bucket(const struct rte_hash *h, uint32_t bucket_index)
{
	return (uint8_t *) &(h->key_tbl[bucket_index * h->bucket_entries *
				     h->key_tbl_key_size]);
}

/* Returns a pointer to a key at a specific position in a specified bucket. */
static inline void *
get_key_from_bucket(const struct rte_hash *h, uint8_t *bkt, uint32_t pos)
{
	return (void *) &bkt[pos * h->key_tbl_key_size];
}

/* Does integer division with rounding-up of result. */
static inline uint32_t
div_roundup(uint32_t numerator, uint32_t denominator)
{
	return (numerator + denominator - 1) / denominator;
}

/* Increases a size (if needed) to a multiple of alignment. */
static inline uint32_t
align_size(uint32_t val, uint32_t alignment)
{
	return alignment * div_roundup(val, alignment);
}

/* Returns the index into the bucket of the first occurrence of a signature. */
static inline int
find_first(uint32_t sig, const uint32_t *sig_bucket, uint32_t num_sigs)
{
	uint32_t i;
	for (i = 0; i < num_sigs; i++) {
		if (sig == sig_bucket[i])
			return i;
	}
	return -1;
}

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

struct rte_hash *
rte_hash_create(const struct rte_hash_parameters *params)
{
	struct rte_hash *h = NULL;
	struct rte_tailq_entry *te;
	uint32_t num_buckets, sig_bucket_size, key_size,
		hash_tbl_size, sig_tbl_size, key_tbl_size, mem_size;
	char hash_name[RTE_HASH_NAMESIZE];
	struct rte_hash_list *hash_list;

	hash_list = RTE_TAILQ_CAST(rte_hash_tailq.head, rte_hash_list);

	/* Check for valid parameters */
	if ((params == NULL) ||
			(params->entries > RTE_HASH_ENTRIES_MAX) ||
			(params->bucket_entries > RTE_HASH_BUCKET_ENTRIES_MAX) ||
			(params->entries < params->bucket_entries) ||
			!rte_is_power_of_2(params->entries) ||
			!rte_is_power_of_2(params->bucket_entries) ||
			(params->key_len == 0) ||
			(params->key_len > RTE_HASH_KEY_LENGTH_MAX)) {
		rte_errno = EINVAL;
		RTE_LOG(ERR, HASH, "rte_hash_create has invalid parameters\n");
		return NULL;
	}

	snprintf(hash_name, sizeof(hash_name), "HT_%s", params->name);

	/* Calculate hash dimensions */
	num_buckets = params->entries / params->bucket_entries;
	sig_bucket_size = align_size(params->bucket_entries *
				     sizeof(hash_sig_t), SIG_BUCKET_ALIGNMENT);
	key_size =  align_size(params->key_len, KEY_ALIGNMENT);

	hash_tbl_size = align_size(sizeof(struct rte_hash), RTE_CACHE_LINE_SIZE);
	sig_tbl_size = align_size(num_buckets * sig_bucket_size,
				  RTE_CACHE_LINE_SIZE);
	key_tbl_size = align_size(num_buckets * key_size *
				  params->bucket_entries, RTE_CACHE_LINE_SIZE);

	/* Total memory required for hash context */
	mem_size = hash_tbl_size + sig_tbl_size + key_tbl_size;

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, hash_list, next) {
		h = (struct rte_hash *) te->data;
		if (strncmp(params->name, h->name, RTE_HASH_NAMESIZE) == 0)
			break;
	}
	if (te != NULL)
		goto exit;

	te = rte_zmalloc("HASH_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, HASH, "tailq entry allocation failed\n");
		goto exit;
	}

	h = (struct rte_hash *)rte_zmalloc_socket(hash_name, mem_size,
					   RTE_CACHE_LINE_SIZE, params->socket_id);
	if (h == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		rte_free(te);
		goto exit;
	}

	/* Setup hash context */
	snprintf(h->name, sizeof(h->name), "%s", params->name);
	h->entries = params->entries;
	h->bucket_entries = params->bucket_entries;
	h->key_len = params->key_len;
	h->hash_func_init_val = params->hash_func_init_val;
	h->num_buckets = num_buckets;
	h->bucket_bitmask = h->num_buckets - 1;
	h->sig_msb = 1 << (sizeof(hash_sig_t) * 8 - 1);
	h->sig_tbl = (uint8_t *)h + hash_tbl_size;
	h->sig_tbl_bucket_size = sig_bucket_size;
	h->key_tbl = h->sig_tbl + sig_tbl_size;
	h->key_tbl_key_size = key_size;
	h->hash_func = (params->hash_func == NULL) ?
		DEFAULT_HASH_FUNC : params->hash_func;

	te->data = (void *) h;

	TAILQ_INSERT_TAIL(hash_list, te, next);

exit:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	return h;
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

	rte_free(h);
	rte_free(te);
}

static inline int32_t
__rte_hash_add_key_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig)
{
	hash_sig_t *sig_bucket;
	uint8_t *key_bucket;
	uint32_t bucket_index, i;
	int32_t pos;

	/* Get the hash signature and bucket index */
	sig |= h->sig_msb;
	bucket_index = sig & h->bucket_bitmask;
	sig_bucket = get_sig_tbl_bucket(h, bucket_index);
	key_bucket = get_key_tbl_bucket(h, bucket_index);

	/* Check if key is already present in the hash */
	for (i = 0; i < h->bucket_entries; i++) {
		if ((sig == sig_bucket[i]) &&
		    likely(memcmp(key, get_key_from_bucket(h, key_bucket, i),
				  h->key_len) == 0)) {
			return bucket_index * h->bucket_entries + i;
		}
	}

	/* Check if any free slot within the bucket to add the new key */
	pos = find_first(NULL_SIGNATURE, sig_bucket, h->bucket_entries);

	if (unlikely(pos < 0))
		return -ENOSPC;

	/* Add the new key to the bucket */
	sig_bucket[pos] = sig;
	rte_memcpy(get_key_from_bucket(h, key_bucket, pos), key, h->key_len);
	return bucket_index * h->bucket_entries + pos;
}

int32_t
rte_hash_add_key_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_add_key_with_hash(h, key, sig);
}

int32_t
rte_hash_add_key(const struct rte_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_add_key_with_hash(h, key, rte_hash_hash(h, key));
}

static inline int32_t
__rte_hash_del_key_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig)
{
	hash_sig_t *sig_bucket;
	uint8_t *key_bucket;
	uint32_t bucket_index, i;

	/* Get the hash signature and bucket index */
	sig = sig | h->sig_msb;
	bucket_index = sig & h->bucket_bitmask;
	sig_bucket = get_sig_tbl_bucket(h, bucket_index);
	key_bucket = get_key_tbl_bucket(h, bucket_index);

	/* Check if key is already present in the hash */
	for (i = 0; i < h->bucket_entries; i++) {
		if ((sig == sig_bucket[i]) &&
		    likely(memcmp(key, get_key_from_bucket(h, key_bucket, i),
				  h->key_len) == 0)) {
			sig_bucket[i] = NULL_SIGNATURE;
			return bucket_index * h->bucket_entries + i;
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

static inline int32_t
__rte_hash_lookup_with_hash(const struct rte_hash *h,
			const void *key, hash_sig_t sig)
{
	hash_sig_t *sig_bucket;
	uint8_t *key_bucket;
	uint32_t bucket_index, i;

	/* Get the hash signature and bucket index */
	sig |= h->sig_msb;
	bucket_index = sig & h->bucket_bitmask;
	sig_bucket = get_sig_tbl_bucket(h, bucket_index);
	key_bucket = get_key_tbl_bucket(h, bucket_index);

	/* Check if key is already present in the hash */
	for (i = 0; i < h->bucket_entries; i++) {
		if ((sig == sig_bucket[i]) &&
		    likely(memcmp(key, get_key_from_bucket(h, key_bucket, i),
				  h->key_len) == 0)) {
			return bucket_index * h->bucket_entries + i;
		}
	}

	return -ENOENT;
}

int32_t
rte_hash_lookup_with_hash(const struct rte_hash *h,
			const void *key, hash_sig_t sig)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, sig);
}

int32_t
rte_hash_lookup(const struct rte_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	return __rte_hash_lookup_with_hash(h, key, rte_hash_hash(h, key));
}

int
rte_hash_lookup_bulk(const struct rte_hash *h, const void **keys,
		      uint32_t num_keys, int32_t *positions)
{
	uint32_t i, j, bucket_index;
	hash_sig_t sigs[RTE_HASH_LOOKUP_BULK_MAX];

	RETURN_IF_TRUE(((h == NULL) || (keys == NULL) || (num_keys == 0) ||
			(num_keys > RTE_HASH_LOOKUP_BULK_MAX) ||
			(positions == NULL)), -EINVAL);

	/* Get the hash signature and bucket index */
	for (i = 0; i < num_keys; i++) {
		sigs[i] = h->hash_func(keys[i], h->key_len,
				h->hash_func_init_val) | h->sig_msb;
		bucket_index = sigs[i] & h->bucket_bitmask;

		/* Pre-fetch relevant buckets */
		rte_prefetch1((void *) get_sig_tbl_bucket(h, bucket_index));
		rte_prefetch1((void *) get_key_tbl_bucket(h, bucket_index));
	}

	/* Check if key is already present in the hash */
	for (i = 0; i < num_keys; i++) {
		bucket_index = sigs[i] & h->bucket_bitmask;
		hash_sig_t *sig_bucket = get_sig_tbl_bucket(h, bucket_index);
		uint8_t *key_bucket = get_key_tbl_bucket(h, bucket_index);

		positions[i] = -ENOENT;

		for (j = 0; j < h->bucket_entries; j++) {
			if ((sigs[i] == sig_bucket[j]) &&
			    likely(memcmp(keys[i],
					  get_key_from_bucket(h, key_bucket, j),
					  h->key_len) == 0)) {
				positions[i] = bucket_index *
					h->bucket_entries + j;
				break;
			}
		}
	}

	return 0;
}
