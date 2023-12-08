/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */
#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_hash.h"
#include "rte_lru.h"

#include "table_log.h"

#define KEY_SIZE						16

#define KEYS_PER_BUCKET					4

#define RTE_BUCKET_ENTRY_VALID						0x1LLU

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_HASH_KEY16_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_HASH_KEY16_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_HASH_KEY16_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_HASH_KEY16_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

#ifdef RTE_ARCH_64
struct rte_bucket_4_16 {
	/* Cache line 0 */
	uint64_t signature[4 + 1];
	uint64_t lru_list;
	struct rte_bucket_4_16 *next;
	uint64_t next_valid;

	/* Cache line 1 */
	uint64_t key[4][2];

	/* Cache line 2 */
	uint8_t data[];
};
#else
struct rte_bucket_4_16 {
	/* Cache line 0 */
	uint64_t signature[4 + 1];
	uint64_t lru_list;
	struct rte_bucket_4_16 *next;
	uint32_t pad;
	uint64_t next_valid;

	/* Cache line 1 */
	uint64_t key[4][2];

	/* Cache line 2 */
	uint8_t data[];
};
#endif

struct rte_table_hash {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t n_buckets;
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t bucket_size;
	uint32_t key_offset;
	uint64_t key_mask[2];
	rte_table_hash_op_hash f_hash;
	uint64_t seed;

	/* Extendible buckets */
	uint32_t n_buckets_ext;
	uint32_t stack_pos;
	uint32_t *stack;

	/* Lookup table */
	uint8_t memory[0] __rte_cache_aligned;
};

static int
keycmp(void *a, void *b, void *b_mask)
{
	uint64_t *a64 = a, *b64 = b, *b_mask64 = b_mask;

	return (a64[0] != (b64[0] & b_mask64[0])) ||
		(a64[1] != (b64[1] & b_mask64[1]));
}

static void
keycpy(void *dst, void *src, void *src_mask)
{
	uint64_t *dst64 = dst, *src64 = src, *src_mask64 = src_mask;

	dst64[0] = src64[0] & src_mask64[0];
	dst64[1] = src64[1] & src_mask64[1];
}

static int
check_params_create(struct rte_table_hash_params *params)
{
	/* name */
	if (params->name == NULL) {
		TABLE_LOG(ERR, "%s: name invalid value", __func__);
		return -EINVAL;
	}

	/* key_size */
	if (params->key_size != KEY_SIZE) {
		TABLE_LOG(ERR, "%s: key_size invalid value", __func__);
		return -EINVAL;
	}

	/* n_keys */
	if (params->n_keys == 0) {
		TABLE_LOG(ERR, "%s: n_keys is zero", __func__);
		return -EINVAL;
	}

	/* n_buckets */
	if ((params->n_buckets == 0) ||
		(!rte_is_power_of_2(params->n_buckets))) {
		TABLE_LOG(ERR, "%s: n_buckets invalid value", __func__);
		return -EINVAL;
	}

	/* f_hash */
	if (params->f_hash == NULL) {
		TABLE_LOG(ERR, "%s: f_hash function pointer is NULL",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_create_key16_lru(void *params,
		int socket_id,
		uint32_t entry_size)
{
	struct rte_table_hash_params *p = params;
	struct rte_table_hash *f;
	uint64_t bucket_size, total_size;
	uint32_t n_buckets, i;

	/* Check input parameters */
	if ((check_params_create(p) != 0) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		((sizeof(struct rte_bucket_4_16) % 64) != 0))
		return NULL;

	/*
	 * Table dimensioning
	 *
	 * Objective: Pick the number of buckets (n_buckets) so that there a chance
	 * to store n_keys keys in the table.
	 *
	 * Note: Since the buckets do not get extended, it is not possible to
	 * guarantee that n_keys keys can be stored in the table at any time. In the
	 * worst case scenario when all the n_keys fall into the same bucket, only
	 * a maximum of KEYS_PER_BUCKET keys will be stored in the table. This case
	 * defeats the purpose of the hash table. It indicates unsuitable f_hash or
	 * n_keys to n_buckets ratio.
	 *
	 * MIN(n_buckets) = (n_keys + KEYS_PER_BUCKET - 1) / KEYS_PER_BUCKET
	 */
	n_buckets = rte_align32pow2(
		(p->n_keys + KEYS_PER_BUCKET - 1) / KEYS_PER_BUCKET);
	n_buckets = RTE_MAX(n_buckets, p->n_buckets);

	/* Memory allocation */
	bucket_size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct rte_bucket_4_16) +
		KEYS_PER_BUCKET * entry_size);
	total_size = sizeof(struct rte_table_hash) + n_buckets * bucket_size;

	if (total_size > SIZE_MAX) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes "
		"for hash table %s",
		__func__, total_size, p->name);
		return NULL;
	}

	f = rte_zmalloc_socket(p->name,
		(size_t)total_size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (f == NULL) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes "
		"for hash table %s",
		__func__, total_size, p->name);
		return NULL;
	}
	TABLE_LOG(INFO, "%s: Hash table %s memory footprint "
		"is %" PRIu64 " bytes",
		__func__, p->name, total_size);

	/* Memory initialization */
	f->n_buckets = n_buckets;
	f->key_size = KEY_SIZE;
	f->entry_size = entry_size;
	f->bucket_size = bucket_size;
	f->key_offset = p->key_offset;
	f->f_hash = p->f_hash;
	f->seed = p->seed;

	if (p->key_mask != NULL) {
		f->key_mask[0] = ((uint64_t *)p->key_mask)[0];
		f->key_mask[1] = ((uint64_t *)p->key_mask)[1];
	} else {
		f->key_mask[0] = 0xFFFFFFFFFFFFFFFFLLU;
		f->key_mask[1] = 0xFFFFFFFFFFFFFFFFLLU;
	}

	for (i = 0; i < n_buckets; i++) {
		struct rte_bucket_4_16 *bucket;

		bucket = (struct rte_bucket_4_16 *) &f->memory[i *
			f->bucket_size];
		lru_init(bucket);
	}

	return f;
}

static int
rte_table_hash_free_key16_lru(void *table)
{
	struct rte_table_hash *f = table;

	/* Check input parameters */
	if (f == NULL) {
		TABLE_LOG(ERR, "%s: table parameter is NULL", __func__);
		return -EINVAL;
	}

	rte_free(f);
	return 0;
}

static int
rte_table_hash_entry_add_key16_lru(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_hash *f = table;
	struct rte_bucket_4_16 *bucket;
	uint64_t signature, pos;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_mask, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket = (struct rte_bucket_4_16 *)
		&f->memory[bucket_index * f->bucket_size];
	signature |= RTE_BUCKET_ENTRY_VALID;

	/* Key is present in the bucket */
	for (i = 0; i < 4; i++) {
		uint64_t bucket_signature = bucket->signature[i];
		uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

		if ((bucket_signature == signature) &&
			(keycmp(bucket_key, key, f->key_mask) == 0)) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			memcpy(bucket_data, entry, f->entry_size);
			lru_update(bucket, i);
			*key_found = 1;
			*entry_ptr = (void *) bucket_data;
			return 0;
		}
	}

	/* Key is not present in the bucket */
	for (i = 0; i < 4; i++) {
		uint64_t bucket_signature = bucket->signature[i];
		uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

		if (bucket_signature == 0) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			bucket->signature[i] = signature;
			keycpy(bucket_key, key, f->key_mask);
			memcpy(bucket_data, entry, f->entry_size);
			lru_update(bucket, i);
			*key_found = 0;
			*entry_ptr = (void *) bucket_data;

			return 0;
		}
	}

	/* Bucket full: replace LRU entry */
	pos = lru_pos(bucket);
	bucket->signature[pos] = signature;
	keycpy(&bucket->key[pos], key, f->key_mask);
	memcpy(&bucket->data[pos * f->entry_size], entry, f->entry_size);
	lru_update(bucket, pos);
	*key_found = 0;
	*entry_ptr = (void *) &bucket->data[pos * f->entry_size];

	return 0;
}

static int
rte_table_hash_entry_delete_key16_lru(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_hash *f = table;
	struct rte_bucket_4_16 *bucket;
	uint64_t signature;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_mask, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket = (struct rte_bucket_4_16 *)
		&f->memory[bucket_index * f->bucket_size];
	signature |= RTE_BUCKET_ENTRY_VALID;

	/* Key is present in the bucket */
	for (i = 0; i < 4; i++) {
		uint64_t bucket_signature = bucket->signature[i];
		uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

		if ((bucket_signature == signature) &&
			(keycmp(bucket_key, key, f->key_mask) == 0)) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			bucket->signature[i] = 0;
			*key_found = 1;
			if (entry)
				memcpy(entry, bucket_data, f->entry_size);
			return 0;
		}
	}

	/* Key is not present in the bucket */
	*key_found = 0;
	return 0;
}

static void *
rte_table_hash_create_key16_ext(void *params,
		int socket_id,
		uint32_t entry_size)
{
	struct rte_table_hash_params *p = params;
	struct rte_table_hash *f;
	uint64_t bucket_size, stack_size, total_size;
	uint32_t n_buckets_ext, i;

	/* Check input parameters */
	if ((check_params_create(p) != 0) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		((sizeof(struct rte_bucket_4_16) % 64) != 0))
		return NULL;

	/*
	 * Table dimensioning
	 *
	 * Objective: Pick the number of bucket extensions (n_buckets_ext) so that
	 * it is guaranteed that n_keys keys can be stored in the table at any time.
	 *
	 * The worst case scenario takes place when all the n_keys keys fall into
	 * the same bucket. Actually, due to the KEYS_PER_BUCKET scheme, the worst
	 * case takes place when (n_keys - KEYS_PER_BUCKET + 1) keys fall into the
	 * same bucket, while the remaining (KEYS_PER_BUCKET - 1) keys each fall
	 * into a different bucket. This case defeats the purpose of the hash table.
	 * It indicates unsuitable f_hash or n_keys to n_buckets ratio.
	 *
	 * n_buckets_ext = n_keys / KEYS_PER_BUCKET + KEYS_PER_BUCKET - 1
	 */
	n_buckets_ext = p->n_keys / KEYS_PER_BUCKET + KEYS_PER_BUCKET - 1;

	/* Memory allocation */
	bucket_size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct rte_bucket_4_16) +
		KEYS_PER_BUCKET * entry_size);
	stack_size = RTE_CACHE_LINE_ROUNDUP(n_buckets_ext * sizeof(uint32_t));
	total_size = sizeof(struct rte_table_hash) +
		(p->n_buckets + n_buckets_ext) * bucket_size + stack_size;
	if (total_size > SIZE_MAX) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes "
			"for hash table %s",
			__func__, total_size, p->name);
		return NULL;
	}

	f = rte_zmalloc_socket(p->name,
		(size_t)total_size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (f == NULL) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes "
			"for hash table %s",
			__func__, total_size, p->name);
		return NULL;
	}
	TABLE_LOG(INFO, "%s: Hash table %s memory footprint "
		"is %" PRIu64 " bytes",
		__func__, p->name, total_size);

	/* Memory initialization */
	f->n_buckets = p->n_buckets;
	f->key_size = KEY_SIZE;
	f->entry_size = entry_size;
	f->bucket_size = bucket_size;
	f->key_offset = p->key_offset;
	f->f_hash = p->f_hash;
	f->seed = p->seed;

	f->n_buckets_ext = n_buckets_ext;
	f->stack_pos = n_buckets_ext;
	f->stack = (uint32_t *)
		&f->memory[(p->n_buckets + n_buckets_ext) * f->bucket_size];

	if (p->key_mask != NULL) {
		f->key_mask[0] = (((uint64_t *)p->key_mask)[0]);
		f->key_mask[1] = (((uint64_t *)p->key_mask)[1]);
	} else {
		f->key_mask[0] = 0xFFFFFFFFFFFFFFFFLLU;
		f->key_mask[1] = 0xFFFFFFFFFFFFFFFFLLU;
	}

	for (i = 0; i < n_buckets_ext; i++)
		f->stack[i] = i;

	return f;
}

static int
rte_table_hash_free_key16_ext(void *table)
{
	struct rte_table_hash *f = table;

	/* Check input parameters */
	if (f == NULL) {
		TABLE_LOG(ERR, "%s: table parameter is NULL", __func__);
		return -EINVAL;
	}

	rte_free(f);
	return 0;
}

static int
rte_table_hash_entry_add_key16_ext(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_hash *f = table;
	struct rte_bucket_4_16 *bucket0, *bucket, *bucket_prev;
	uint64_t signature;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_mask, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket0 = (struct rte_bucket_4_16 *)
		&f->memory[bucket_index * f->bucket_size];
	signature |= RTE_BUCKET_ENTRY_VALID;

	/* Key is present in the bucket */
	for (bucket = bucket0; bucket != NULL; bucket = bucket->next)
		for (i = 0; i < 4; i++) {
			uint64_t bucket_signature = bucket->signature[i];
			uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

			if ((bucket_signature == signature) &&
				(keycmp(bucket_key, key, f->key_mask) == 0)) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				memcpy(bucket_data, entry, f->entry_size);
				*key_found = 1;
				*entry_ptr = (void *) bucket_data;
				return 0;
			}
		}

	/* Key is not present in the bucket */
	for (bucket_prev = NULL, bucket = bucket0; bucket != NULL;
		bucket_prev = bucket, bucket = bucket->next)
		for (i = 0; i < 4; i++) {
			uint64_t bucket_signature = bucket->signature[i];
			uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

			if (bucket_signature == 0) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				bucket->signature[i] = signature;
				keycpy(bucket_key, key, f->key_mask);
				memcpy(bucket_data, entry, f->entry_size);
				*key_found = 0;
				*entry_ptr = (void *) bucket_data;

				return 0;
			}
		}

	/* Bucket full: extend bucket */
	if (f->stack_pos > 0) {
		bucket_index = f->stack[--f->stack_pos];

		bucket = (struct rte_bucket_4_16 *) &f->memory[(f->n_buckets +
			bucket_index) * f->bucket_size];
		bucket_prev->next = bucket;
		bucket_prev->next_valid = 1;

		bucket->signature[0] = signature;
		keycpy(&bucket->key[0], key, f->key_mask);
		memcpy(&bucket->data[0], entry, f->entry_size);
		*key_found = 0;
		*entry_ptr = (void *) &bucket->data[0];
		return 0;
	}

	return -ENOSPC;
}

static int
rte_table_hash_entry_delete_key16_ext(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_hash *f = table;
	struct rte_bucket_4_16 *bucket0, *bucket, *bucket_prev;
	uint64_t signature;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_mask, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket0 = (struct rte_bucket_4_16 *)
		&f->memory[bucket_index * f->bucket_size];
	signature |= RTE_BUCKET_ENTRY_VALID;

	/* Key is present in the bucket */
	for (bucket_prev = NULL, bucket = bucket0; bucket != NULL;
		bucket_prev = bucket, bucket = bucket->next)
		for (i = 0; i < 4; i++) {
			uint64_t bucket_signature = bucket->signature[i];
			uint8_t *bucket_key = (uint8_t *) &bucket->key[i];

			if ((bucket_signature == signature) &&
				(keycmp(bucket_key, key, f->key_mask) == 0)) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				bucket->signature[i] = 0;
				*key_found = 1;
				if (entry)
					memcpy(entry, bucket_data, f->entry_size);

				if ((bucket->signature[0] == 0) &&
					(bucket->signature[1] == 0) &&
					(bucket->signature[2] == 0) &&
					(bucket->signature[3] == 0) &&
					(bucket_prev != NULL)) {
					bucket_prev->next = bucket->next;
					bucket_prev->next_valid =
						bucket->next_valid;

					memset(bucket, 0,
						sizeof(struct rte_bucket_4_16));
					bucket_index = (((uint8_t *)bucket -
						(uint8_t *)f->memory)/f->bucket_size) - f->n_buckets;
					f->stack[f->stack_pos++] = bucket_index;
				}

				return 0;
			}
		}

	/* Key is not present in the bucket */
	*key_found = 0;
	return 0;
}

#define lookup_key16_cmp(key_in, bucket, pos, f)			\
{								\
	uint64_t xor[4][2], or[4], signature[4], k[2];		\
								\
	k[0] = key_in[0] & f->key_mask[0];				\
	k[1] = key_in[1] & f->key_mask[1];				\
	signature[0] = (~bucket->signature[0]) & 1;		\
	signature[1] = (~bucket->signature[1]) & 1;		\
	signature[2] = (~bucket->signature[2]) & 1;		\
	signature[3] = (~bucket->signature[3]) & 1;		\
								\
	xor[0][0] = k[0] ^ bucket->key[0][0];			\
	xor[0][1] = k[1] ^ bucket->key[0][1];			\
								\
	xor[1][0] = k[0] ^ bucket->key[1][0];			\
	xor[1][1] = k[1] ^ bucket->key[1][1];			\
								\
	xor[2][0] = k[0] ^ bucket->key[2][0];			\
	xor[2][1] = k[1] ^ bucket->key[2][1];			\
								\
	xor[3][0] = k[0] ^ bucket->key[3][0];			\
	xor[3][1] = k[1] ^ bucket->key[3][1];			\
								\
	or[0] = xor[0][0] | xor[0][1] | signature[0];		\
	or[1] = xor[1][0] | xor[1][1] | signature[1];		\
	or[2] = xor[2][0] | xor[2][1] | signature[2];		\
	or[3] = xor[3][0] | xor[3][1] | signature[3];		\
								\
	pos = 4;						\
	if (or[0] == 0)						\
		pos = 0;					\
	if (or[1] == 0)						\
		pos = 1;					\
	if (or[2] == 0)						\
		pos = 2;					\
	if (or[3] == 0)						\
		pos = 3;					\
}

#define lookup1_stage0(pkt0_index, mbuf0, pkts, pkts_mask, f)	\
{								\
	uint64_t pkt_mask;					\
	uint32_t key_offset = f->key_offset;\
								\
	pkt0_index = rte_ctz64(pkts_mask);		\
	pkt_mask = 1LLU << pkt0_index;				\
	pkts_mask &= ~pkt_mask;					\
								\
	mbuf0 = pkts[pkt0_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf0, key_offset));\
}

#define lookup1_stage1(mbuf1, bucket1, f)			\
{								\
	uint64_t *key;						\
	uint64_t signature = 0;				\
	uint32_t bucket_index;				\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf1, f->key_offset);\
	signature = f->f_hash(key, f->key_mask, KEY_SIZE, f->seed);	\
								\
	bucket_index = signature & (f->n_buckets - 1);		\
	bucket1 = (struct rte_bucket_4_16 *)			\
		&f->memory[bucket_index * f->bucket_size];	\
	rte_prefetch0(bucket1);					\
	rte_prefetch0((void *)(((uintptr_t) bucket1) + RTE_CACHE_LINE_SIZE));\
}

#define lookup1_stage2_lru(pkt2_index, mbuf2, bucket2,		\
		pkts_mask_out, entries, f)			\
{								\
	void *a;						\
	uint64_t pkt_mask;					\
	uint64_t *key;						\
	uint32_t pos;						\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf2, f->key_offset);\
	lookup_key16_cmp(key, bucket2, pos, f);			\
								\
	pkt_mask = (bucket2->signature[pos] & 1LLU) << pkt2_index;\
	pkts_mask_out |= pkt_mask;				\
								\
	a = (void *) &bucket2->data[pos * f->entry_size];	\
	rte_prefetch0(a);					\
	entries[pkt2_index] = a;				\
	lru_update(bucket2, pos);				\
}

#define lookup1_stage2_ext(pkt2_index, mbuf2, bucket2, pkts_mask_out, entries, \
	buckets_mask, buckets, keys, f)				\
{								\
	struct rte_bucket_4_16 *bucket_next;			\
	void *a;						\
	uint64_t pkt_mask, bucket_mask;				\
	uint64_t *key;						\
	uint32_t pos;						\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf2, f->key_offset);\
	lookup_key16_cmp(key, bucket2, pos, f);			\
								\
	pkt_mask = (bucket2->signature[pos] & 1LLU) << pkt2_index;\
	pkts_mask_out |= pkt_mask;				\
								\
	a = (void *) &bucket2->data[pos * f->entry_size];	\
	rte_prefetch0(a);					\
	entries[pkt2_index] = a;				\
								\
	bucket_mask = (~pkt_mask) & (bucket2->next_valid << pkt2_index);\
	buckets_mask |= bucket_mask;				\
	bucket_next = bucket2->next;				\
	buckets[pkt2_index] = bucket_next;			\
	keys[pkt2_index] = key;					\
}

#define lookup_grinder(pkt_index, buckets, keys, pkts_mask_out, entries,\
	buckets_mask, f)					\
{								\
	struct rte_bucket_4_16 *bucket, *bucket_next;		\
	void *a;						\
	uint64_t pkt_mask, bucket_mask;				\
	uint64_t *key;						\
	uint32_t pos;						\
								\
	bucket = buckets[pkt_index];				\
	key = keys[pkt_index];					\
	lookup_key16_cmp(key, bucket, pos, f);			\
								\
	pkt_mask = (bucket->signature[pos] & 1LLU) << pkt_index;\
	pkts_mask_out |= pkt_mask;				\
								\
	a = (void *) &bucket->data[pos * f->entry_size];	\
	rte_prefetch0(a);					\
	entries[pkt_index] = a;					\
								\
	bucket_mask = (~pkt_mask) & (bucket->next_valid << pkt_index);\
	buckets_mask |= bucket_mask;				\
	bucket_next = bucket->next;				\
	rte_prefetch0(bucket_next);				\
	rte_prefetch0((void *)(((uintptr_t) bucket_next) + RTE_CACHE_LINE_SIZE));\
	buckets[pkt_index] = bucket_next;			\
	keys[pkt_index] = key;					\
}

#define lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01,\
		pkts, pkts_mask, f)				\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	uint32_t key_offset = f->key_offset;		\
								\
	pkt00_index = rte_ctz64(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
								\
	mbuf00 = pkts[pkt00_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));\
								\
	pkt01_index = rte_ctz64(pkts_mask);		\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
								\
	mbuf01 = pkts[pkt01_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));\
}

#define lookup2_stage0_with_odd_support(pkt00_index, pkt01_index,\
		mbuf00, mbuf01, pkts, pkts_mask, f)		\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	uint32_t key_offset = f->key_offset;		\
								\
	pkt00_index = rte_ctz64(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
								\
	mbuf00 = pkts[pkt00_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));	\
								\
	pkt01_index = rte_ctz64(pkts_mask);		\
	if (pkts_mask == 0)					\
		pkt01_index = pkt00_index;			\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
								\
	mbuf01 = pkts[pkt01_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));	\
}

#define lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f)	\
{								\
	uint64_t *key10, *key11;					\
	uint64_t signature10, signature11;			\
	uint32_t bucket10_index, bucket11_index;	\
								\
	key10 = RTE_MBUF_METADATA_UINT64_PTR(mbuf10, f->key_offset);\
	signature10 = f->f_hash(key10, f->key_mask,	 KEY_SIZE, f->seed);\
	bucket10_index = signature10 & (f->n_buckets - 1);	\
	bucket10 = (struct rte_bucket_4_16 *)				\
		&f->memory[bucket10_index * f->bucket_size];	\
	rte_prefetch0(bucket10);				\
	rte_prefetch0((void *)(((uintptr_t) bucket10) + RTE_CACHE_LINE_SIZE));\
								\
	key11 = RTE_MBUF_METADATA_UINT64_PTR(mbuf11, f->key_offset);\
	signature11 = f->f_hash(key11, f->key_mask,	 KEY_SIZE, f->seed);\
	bucket11_index = signature11 & (f->n_buckets - 1);	\
	bucket11 = (struct rte_bucket_4_16 *)			\
		&f->memory[bucket11_index * f->bucket_size];	\
	rte_prefetch0(bucket11);				\
	rte_prefetch0((void *)(((uintptr_t) bucket11) + RTE_CACHE_LINE_SIZE));\
}

#define lookup2_stage2_lru(pkt20_index, pkt21_index, mbuf20, mbuf21,\
		bucket20, bucket21, pkts_mask_out, entries, f)	\
{								\
	void *a20, *a21;					\
	uint64_t pkt20_mask, pkt21_mask;			\
	uint64_t *key20, *key21;				\
	uint32_t pos20, pos21;					\
								\
	key20 = RTE_MBUF_METADATA_UINT64_PTR(mbuf20, f->key_offset);\
	key21 = RTE_MBUF_METADATA_UINT64_PTR(mbuf21, f->key_offset);\
								\
	lookup_key16_cmp(key20, bucket20, pos20, f);		\
	lookup_key16_cmp(key21, bucket21, pos21, f);		\
								\
	pkt20_mask = (bucket20->signature[pos20] & 1LLU) << pkt20_index;\
	pkt21_mask = (bucket21->signature[pos21] & 1LLU) << pkt21_index;\
	pkts_mask_out |= pkt20_mask | pkt21_mask;			\
								\
	a20 = (void *) &bucket20->data[pos20 * f->entry_size];	\
	a21 = (void *) &bucket21->data[pos21 * f->entry_size];	\
	rte_prefetch0(a20);					\
	rte_prefetch0(a21);					\
	entries[pkt20_index] = a20;				\
	entries[pkt21_index] = a21;				\
	lru_update(bucket20, pos20);				\
	lru_update(bucket21, pos21);				\
}

#define lookup2_stage2_ext(pkt20_index, pkt21_index, mbuf20, mbuf21, bucket20, \
	bucket21, pkts_mask_out, entries, buckets_mask, buckets, keys, f) \
{								\
	struct rte_bucket_4_16 *bucket20_next, *bucket21_next;	\
	void *a20, *a21;					\
	uint64_t pkt20_mask, pkt21_mask, bucket20_mask, bucket21_mask;\
	uint64_t *key20, *key21;				\
	uint32_t pos20, pos21;					\
								\
	key20 = RTE_MBUF_METADATA_UINT64_PTR(mbuf20, f->key_offset);\
	key21 = RTE_MBUF_METADATA_UINT64_PTR(mbuf21, f->key_offset);\
								\
	lookup_key16_cmp(key20, bucket20, pos20, f);	\
	lookup_key16_cmp(key21, bucket21, pos21, f);	\
								\
	pkt20_mask = (bucket20->signature[pos20] & 1LLU) << pkt20_index;\
	pkt21_mask = (bucket21->signature[pos21] & 1LLU) << pkt21_index;\
	pkts_mask_out |= pkt20_mask | pkt21_mask;		\
								\
	a20 = (void *) &bucket20->data[pos20 * f->entry_size];	\
	a21 = (void *) &bucket21->data[pos21 * f->entry_size];	\
	rte_prefetch0(a20);					\
	rte_prefetch0(a21);					\
	entries[pkt20_index] = a20;				\
	entries[pkt21_index] = a21;				\
								\
	bucket20_mask = (~pkt20_mask) & (bucket20->next_valid << pkt20_index);\
	bucket21_mask = (~pkt21_mask) & (bucket21->next_valid << pkt21_index);\
	buckets_mask |= bucket20_mask | bucket21_mask;		\
	bucket20_next = bucket20->next;				\
	bucket21_next = bucket21->next;				\
	buckets[pkt20_index] = bucket20_next;			\
	buckets[pkt21_index] = bucket21_next;			\
	keys[pkt20_index] = key20;				\
	keys[pkt21_index] = key21;				\
}

static int
rte_table_hash_lookup_key16_lru(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_16 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index;
	uint32_t pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0;

	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);

	RTE_TABLE_HASH_KEY16_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (rte_popcount64(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_16 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1(mbuf, bucket, f);
			lookup1_stage2_lru(pkt_index, mbuf, bucket,
				pkts_mask_out, entries, f);
		}

		*lookup_hit_mask = pkts_mask_out;
		RTE_TABLE_HASH_KEY16_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in -
			rte_popcount64(pkts_mask_out));
		return 0;
	}

	/*
	 * Pipeline fill
	 *
	 */
	/* Pipeline stage 0 */
	lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01, pkts,
		pkts_mask, f);

	/* Pipeline feed */
	mbuf10 = mbuf00;
	mbuf11 = mbuf01;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01, pkts,
		pkts_mask, f);

	/* Pipeline stage 1 */
	lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

	/*
	 * Pipeline run
	 *
	 */
	for ( ; pkts_mask; ) {
		/* Pipeline feed */
		bucket20 = bucket10;
		bucket21 = bucket11;
		mbuf20 = mbuf10;
		mbuf21 = mbuf11;
		mbuf10 = mbuf00;
		mbuf11 = mbuf01;
		pkt20_index = pkt10_index;
		pkt21_index = pkt11_index;
		pkt10_index = pkt00_index;
		pkt11_index = pkt01_index;

		/* Pipeline stage 0 */
		lookup2_stage0_with_odd_support(pkt00_index, pkt01_index,
			mbuf00, mbuf01, pkts, pkts_mask, f);

		/* Pipeline stage 1 */
		lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

		/* Pipeline stage 2 */
		lookup2_stage2_lru(pkt20_index, pkt21_index, mbuf20, mbuf21,
			bucket20, bucket21, pkts_mask_out, entries, f);
	}

	/*
	 * Pipeline flush
	 *
	 */
	/* Pipeline feed */
	bucket20 = bucket10;
	bucket21 = bucket11;
	mbuf20 = mbuf10;
	mbuf21 = mbuf11;
	mbuf10 = mbuf00;
	mbuf11 = mbuf01;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 1 */
	lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

	/* Pipeline stage 2 */
	lookup2_stage2_lru(pkt20_index, pkt21_index, mbuf20, mbuf21,
		bucket20, bucket21, pkts_mask_out, entries, f);

	/* Pipeline feed */
	bucket20 = bucket10;
	bucket21 = bucket11;
	mbuf20 = mbuf10;
	mbuf21 = mbuf11;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;

	/* Pipeline stage 2 */
	lookup2_stage2_lru(pkt20_index, pkt21_index, mbuf20, mbuf21,
		bucket20, bucket21, pkts_mask_out, entries, f);

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_KEY16_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in -
		rte_popcount64(pkts_mask_out));
	return 0;
} /* lookup LRU */

static int
rte_table_hash_lookup_key16_ext(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_16 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index;
	uint32_t pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0, buckets_mask = 0;
	struct rte_bucket_4_16 *buckets[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t *keys[RTE_PORT_IN_BURST_SIZE_MAX];

	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);

	RTE_TABLE_HASH_KEY16_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (rte_popcount64(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_16 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1(mbuf, bucket, f);
			lookup1_stage2_ext(pkt_index, mbuf, bucket,
				pkts_mask_out, entries, buckets_mask,
				buckets, keys, f);
		}

		goto grind_next_buckets;
	}

	/*
	 * Pipeline fill
	 *
	 */
	/* Pipeline stage 0 */
	lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01, pkts,
		pkts_mask, f);

	/* Pipeline feed */
	mbuf10 = mbuf00;
	mbuf11 = mbuf01;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01, pkts,
		pkts_mask, f);

	/* Pipeline stage 1 */
	lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

	/*
	 * Pipeline run
	 *
	 */
	for ( ; pkts_mask; ) {
		/* Pipeline feed */
		bucket20 = bucket10;
		bucket21 = bucket11;
		mbuf20 = mbuf10;
		mbuf21 = mbuf11;
		mbuf10 = mbuf00;
		mbuf11 = mbuf01;
		pkt20_index = pkt10_index;
		pkt21_index = pkt11_index;
		pkt10_index = pkt00_index;
		pkt11_index = pkt01_index;

		/* Pipeline stage 0 */
		lookup2_stage0_with_odd_support(pkt00_index, pkt01_index,
			mbuf00, mbuf01, pkts, pkts_mask, f);

		/* Pipeline stage 1 */
		lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

		/* Pipeline stage 2 */
		lookup2_stage2_ext(pkt20_index, pkt21_index, mbuf20, mbuf21,
			bucket20, bucket21, pkts_mask_out, entries,
			buckets_mask, buckets, keys, f);
	}

	/*
	 * Pipeline flush
	 *
	 */
	/* Pipeline feed */
	bucket20 = bucket10;
	bucket21 = bucket11;
	mbuf20 = mbuf10;
	mbuf21 = mbuf11;
	mbuf10 = mbuf00;
	mbuf11 = mbuf01;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 1 */
	lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f);

	/* Pipeline stage 2 */
	lookup2_stage2_ext(pkt20_index, pkt21_index, mbuf20, mbuf21,
		bucket20, bucket21, pkts_mask_out, entries,
		buckets_mask, buckets, keys, f);

	/* Pipeline feed */
	bucket20 = bucket10;
	bucket21 = bucket11;
	mbuf20 = mbuf10;
	mbuf21 = mbuf11;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;

	/* Pipeline stage 2 */
	lookup2_stage2_ext(pkt20_index, pkt21_index, mbuf20, mbuf21,
		bucket20, bucket21, pkts_mask_out, entries,
		buckets_mask, buckets, keys, f);

grind_next_buckets:
	/* Grind next buckets */
	for ( ; buckets_mask; ) {
		uint64_t buckets_mask_next = 0;

		for ( ; buckets_mask; ) {
			uint64_t pkt_mask;
			uint32_t pkt_index;

			pkt_index = rte_ctz64(buckets_mask);
			pkt_mask = 1LLU << pkt_index;
			buckets_mask &= ~pkt_mask;

			lookup_grinder(pkt_index, buckets, keys, pkts_mask_out,
				entries, buckets_mask_next, f);
		}

		buckets_mask = buckets_mask_next;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_KEY16_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in -
		rte_popcount64(pkts_mask_out));
	return 0;
} /* lookup EXT */

static int
rte_table_hash_key16_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_hash *t = table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_hash_key16_lru_ops = {
	.f_create = rte_table_hash_create_key16_lru,
	.f_free = rte_table_hash_free_key16_lru,
	.f_add = rte_table_hash_entry_add_key16_lru,
	.f_delete = rte_table_hash_entry_delete_key16_lru,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key16_lru,
	.f_stats = rte_table_hash_key16_stats_read,
};

struct rte_table_ops rte_table_hash_key16_ext_ops = {
	.f_create = rte_table_hash_create_key16_ext,
	.f_free = rte_table_hash_free_key16_ext,
	.f_add = rte_table_hash_entry_add_key16_ext,
	.f_delete = rte_table_hash_entry_delete_key16_ext,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key16_ext,
	.f_stats = rte_table_hash_key16_stats_read,
};
