/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_hash.h"

#include "table_log.h"

#define KEYS_PER_BUCKET	4

struct bucket {
	union {
		uintptr_t next;
		uint64_t lru_list;
	};
	uint16_t sig[KEYS_PER_BUCKET];
	uint32_t key_pos[KEYS_PER_BUCKET];
};

#define BUCKET_NEXT(bucket)						\
	((void *) ((bucket)->next & (~1LU)))

#define BUCKET_NEXT_VALID(bucket)					\
	((bucket)->next & 1LU)

#define BUCKET_NEXT_SET(bucket, bucket_next)				\
do									\
	(bucket)->next = (((uintptr_t) ((void *) (bucket_next))) | 1LU);\
while (0)

#define BUCKET_NEXT_SET_NULL(bucket)					\
do									\
	(bucket)->next = 0;						\
while (0)

#define BUCKET_NEXT_COPY(bucket, bucket2)				\
do									\
	(bucket)->next = (bucket2)->next;				\
while (0)

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_HASH_EXT_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_HASH_EXT_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_HASH_EXT_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_HASH_EXT_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct grinder {
	struct bucket *bkt;
	uint64_t sig;
	uint64_t match;
	uint32_t key_index;
};

struct rte_table_hash {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t n_keys;
	uint32_t n_buckets;
	uint32_t n_buckets_ext;
	rte_table_hash_op_hash f_hash;
	uint64_t seed;
	uint32_t key_offset;

	/* Internal */
	uint64_t bucket_mask;
	uint32_t key_size_shl;
	uint32_t data_size_shl;
	uint32_t key_stack_tos;
	uint32_t bkt_ext_stack_tos;

	/* Grinder */
	struct grinder grinders[RTE_PORT_IN_BURST_SIZE_MAX];

	/* Tables */
	uint64_t *key_mask;
	struct bucket *buckets;
	struct bucket *buckets_ext;
	uint8_t *key_mem;
	uint8_t *data_mem;
	uint32_t *key_stack;
	uint32_t *bkt_ext_stack;

	/* Table memory */
	uint8_t memory[0] __rte_cache_aligned;
};

static int
keycmp(void *a, void *b, void *b_mask, uint32_t n_bytes)
{
	uint64_t *a64 = a, *b64 = b, *b_mask64 = b_mask;
	uint32_t i;

	for (i = 0; i < n_bytes / sizeof(uint64_t); i++)
		if (a64[i] != (b64[i] & b_mask64[i]))
			return 1;

	return 0;
}

static void
keycpy(void *dst, void *src, void *src_mask, uint32_t n_bytes)
{
	uint64_t *dst64 = dst, *src64 = src, *src_mask64 = src_mask;
	uint32_t i;

	for (i = 0; i < n_bytes / sizeof(uint64_t); i++)
		dst64[i] = src64[i] & src_mask64[i];
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
	if ((params->key_size < sizeof(uint64_t)) ||
		(!rte_is_power_of_2(params->key_size))) {
		TABLE_LOG(ERR, "%s: key_size invalid value", __func__);
		return -EINVAL;
	}

	/* n_keys */
	if (params->n_keys == 0) {
		TABLE_LOG(ERR, "%s: n_keys invalid value", __func__);
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
		TABLE_LOG(ERR, "%s: f_hash invalid value", __func__);
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_ext_create(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_hash_params *p = params;
	struct rte_table_hash *t;
	uint64_t table_meta_sz, key_mask_sz, bucket_sz, bucket_ext_sz, key_sz;
	uint64_t key_stack_sz, bkt_ext_stack_sz, data_sz, total_size;
	uint64_t key_mask_offset, bucket_offset, bucket_ext_offset, key_offset;
	uint64_t key_stack_offset, bkt_ext_stack_offset, data_offset;
	uint32_t n_buckets_ext, i;

	/* Check input parameters */
	if ((check_params_create(p) != 0) ||
		(!rte_is_power_of_2(entry_size)) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		(sizeof(struct bucket) != (RTE_CACHE_LINE_SIZE / 2)))
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
	table_meta_sz = RTE_CACHE_LINE_ROUNDUP(sizeof(struct rte_table_hash));
	key_mask_sz = RTE_CACHE_LINE_ROUNDUP(p->key_size);
	bucket_sz = RTE_CACHE_LINE_ROUNDUP(p->n_buckets * sizeof(struct bucket));
	bucket_ext_sz =
		RTE_CACHE_LINE_ROUNDUP(n_buckets_ext * sizeof(struct bucket));
	key_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * p->key_size);
	key_stack_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * sizeof(uint32_t));
	bkt_ext_stack_sz =
		RTE_CACHE_LINE_ROUNDUP(n_buckets_ext * sizeof(uint32_t));
	data_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * entry_size);
	total_size = table_meta_sz + key_mask_sz + bucket_sz + bucket_ext_sz +
		key_sz + key_stack_sz + bkt_ext_stack_sz + data_sz;

	if (total_size > SIZE_MAX) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes"
			" for hash table %s",
			__func__, total_size, p->name);
		return NULL;
	}

	t = rte_zmalloc_socket(p->name,
		(size_t)total_size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (t == NULL) {
		TABLE_LOG(ERR, "%s: Cannot allocate %" PRIu64 " bytes"
			" for hash table %s",
			__func__, total_size, p->name);
		return NULL;
	}
	TABLE_LOG(INFO, "%s (%u-byte key): Hash table %s memory "
		"footprint is %" PRIu64 " bytes",
		__func__, p->key_size, p->name, total_size);

	/* Memory initialization */
	t->key_size = p->key_size;
	t->entry_size = entry_size;
	t->n_keys = p->n_keys;
	t->n_buckets = p->n_buckets;
	t->n_buckets_ext = n_buckets_ext;
	t->f_hash = p->f_hash;
	t->seed = p->seed;
	t->key_offset = p->key_offset;

	/* Internal */
	t->bucket_mask = t->n_buckets - 1;
	t->key_size_shl = rte_ctz32(p->key_size);
	t->data_size_shl = rte_ctz32(entry_size);

	/* Tables */
	key_mask_offset = 0;
	bucket_offset = key_mask_offset + key_mask_sz;
	bucket_ext_offset = bucket_offset + bucket_sz;
	key_offset = bucket_ext_offset + bucket_ext_sz;
	key_stack_offset = key_offset + key_sz;
	bkt_ext_stack_offset = key_stack_offset + key_stack_sz;
	data_offset = bkt_ext_stack_offset + bkt_ext_stack_sz;

	t->key_mask = (uint64_t *) &t->memory[key_mask_offset];
	t->buckets = (struct bucket *) &t->memory[bucket_offset];
	t->buckets_ext = (struct bucket *) &t->memory[bucket_ext_offset];
	t->key_mem = &t->memory[key_offset];
	t->key_stack = (uint32_t *) &t->memory[key_stack_offset];
	t->bkt_ext_stack = (uint32_t *) &t->memory[bkt_ext_stack_offset];
	t->data_mem = &t->memory[data_offset];

	/* Key mask */
	if (p->key_mask == NULL)
		memset(t->key_mask, 0xFF, p->key_size);
	else
		memcpy(t->key_mask, p->key_mask, p->key_size);

	/* Key stack */
	for (i = 0; i < t->n_keys; i++)
		t->key_stack[i] = t->n_keys - 1 - i;
	t->key_stack_tos = t->n_keys;

	/* Bucket ext stack */
	for (i = 0; i < t->n_buckets_ext; i++)
		t->bkt_ext_stack[i] = t->n_buckets_ext - 1 - i;
	t->bkt_ext_stack_tos = t->n_buckets_ext;

	return t;
}

static int
rte_table_hash_ext_free(void *table)
{
	struct rte_table_hash *t = table;

	/* Check input parameters */
	if (t == NULL)
		return -EINVAL;

	rte_free(t);
	return 0;
}

static int
rte_table_hash_ext_entry_add(void *table, void *key, void *entry,
	int *key_found, void **entry_ptr)
{
	struct rte_table_hash *t = table;
	struct bucket *bkt0, *bkt, *bkt_prev;
	uint64_t sig;
	uint32_t bkt_index, i;

	sig = t->f_hash(key, t->key_mask, t->key_size, t->seed);
	bkt_index = sig & t->bucket_mask;
	bkt0 = &t->buckets[bkt_index];
	sig = (sig >> 16) | 1LLU;

	/* Key is present in the bucket */
	for (bkt = bkt0; bkt != NULL; bkt = BUCKET_NEXT(bkt))
		for (i = 0; i < KEYS_PER_BUCKET; i++) {
			uint64_t bkt_sig = (uint64_t) bkt->sig[i];
			uint32_t bkt_key_index = bkt->key_pos[i];
			uint8_t *bkt_key =
				&t->key_mem[bkt_key_index << t->key_size_shl];

			if ((sig == bkt_sig) && (keycmp(bkt_key, key, t->key_mask,
				t->key_size) == 0)) {
				uint8_t *data = &t->data_mem[bkt_key_index <<
					t->data_size_shl];

				memcpy(data, entry, t->entry_size);
				*key_found = 1;
				*entry_ptr = (void *) data;
				return 0;
			}
		}

	/* Key is not present in the bucket */
	for (bkt_prev = NULL, bkt = bkt0; bkt != NULL; bkt_prev = bkt,
		bkt = BUCKET_NEXT(bkt))
		for (i = 0; i < KEYS_PER_BUCKET; i++) {
			uint64_t bkt_sig = (uint64_t) bkt->sig[i];

			if (bkt_sig == 0) {
				uint32_t bkt_key_index;
				uint8_t *bkt_key, *data;

				/* Allocate new key */
				if (t->key_stack_tos == 0) /* No free keys */
					return -ENOSPC;

				bkt_key_index = t->key_stack[
					--t->key_stack_tos];

				/* Install new key */
				bkt_key = &t->key_mem[bkt_key_index <<
					t->key_size_shl];
				data = &t->data_mem[bkt_key_index <<
					t->data_size_shl];

				bkt->sig[i] = (uint16_t) sig;
				bkt->key_pos[i] = bkt_key_index;
				keycpy(bkt_key, key, t->key_mask, t->key_size);
				memcpy(data, entry, t->entry_size);

				*key_found = 0;
				*entry_ptr = (void *) data;
				return 0;
			}
		}

	/* Bucket full: extend bucket */
	if ((t->bkt_ext_stack_tos > 0) && (t->key_stack_tos > 0)) {
		uint32_t bkt_key_index;
		uint8_t *bkt_key, *data;

		/* Allocate new bucket ext */
		bkt_index = t->bkt_ext_stack[--t->bkt_ext_stack_tos];
		bkt = &t->buckets_ext[bkt_index];

		/* Chain the new bucket ext */
		BUCKET_NEXT_SET(bkt_prev, bkt);
		BUCKET_NEXT_SET_NULL(bkt);

		/* Allocate new key */
		bkt_key_index = t->key_stack[--t->key_stack_tos];
		bkt_key = &t->key_mem[bkt_key_index << t->key_size_shl];

		data = &t->data_mem[bkt_key_index << t->data_size_shl];

		/* Install new key into bucket */
		bkt->sig[0] = (uint16_t) sig;
		bkt->key_pos[0] = bkt_key_index;
		keycpy(bkt_key, key, t->key_mask, t->key_size);
		memcpy(data, entry, t->entry_size);

		*key_found = 0;
		*entry_ptr = (void *) data;
		return 0;
	}

	return -ENOSPC;
}

static int
rte_table_hash_ext_entry_delete(void *table, void *key, int *key_found,
void *entry)
{
	struct rte_table_hash *t = table;
	struct bucket *bkt0, *bkt, *bkt_prev;
	uint64_t sig;
	uint32_t bkt_index, i;

	sig = t->f_hash(key, t->key_mask, t->key_size, t->seed);
	bkt_index = sig & t->bucket_mask;
	bkt0 = &t->buckets[bkt_index];
	sig = (sig >> 16) | 1LLU;

	/* Key is present in the bucket */
	for (bkt_prev = NULL, bkt = bkt0; bkt != NULL; bkt_prev = bkt,
		bkt = BUCKET_NEXT(bkt))
		for (i = 0; i < KEYS_PER_BUCKET; i++) {
			uint64_t bkt_sig = (uint64_t) bkt->sig[i];
			uint32_t bkt_key_index = bkt->key_pos[i];
			uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
				t->key_size_shl];

			if ((sig == bkt_sig) && (keycmp(bkt_key, key, t->key_mask,
				t->key_size) == 0)) {
				uint8_t *data = &t->data_mem[bkt_key_index <<
					t->data_size_shl];

				/* Uninstall key from bucket */
				bkt->sig[i] = 0;
				*key_found = 1;
				if (entry)
					memcpy(entry, data, t->entry_size);

				/* Free key */
				t->key_stack[t->key_stack_tos++] =
					bkt_key_index;

				/*Check if bucket is unused */
				if ((bkt_prev != NULL) &&
				    (bkt->sig[0] == 0) && (bkt->sig[1] == 0) &&
				    (bkt->sig[2] == 0) && (bkt->sig[3] == 0)) {
					/* Unchain bucket */
					BUCKET_NEXT_COPY(bkt_prev, bkt);

					/* Clear bucket */
					memset(bkt, 0, sizeof(struct bucket));

					/* Free bucket back to buckets ext */
					bkt_index = bkt - t->buckets_ext;
					t->bkt_ext_stack[t->bkt_ext_stack_tos++]
						= bkt_index;
				}

				return 0;
			}
		}

	/* Key is not present in the bucket */
	*key_found = 0;
	return 0;
}

static int rte_table_hash_ext_lookup_unoptimized(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;
	uint64_t pkts_mask_out = 0;

	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);

	for ( ; pkts_mask; ) {
		struct bucket *bkt0, *bkt;
		struct rte_mbuf *pkt;
		uint8_t *key;
		uint64_t pkt_mask, sig;
		uint32_t pkt_index, bkt_index, i;

		pkt_index = rte_ctz64(pkts_mask);
		pkt_mask = 1LLU << pkt_index;
		pkts_mask &= ~pkt_mask;

		pkt = pkts[pkt_index];
		key = RTE_MBUF_METADATA_UINT8_PTR(pkt, t->key_offset);
		sig = (uint64_t) t->f_hash(key, t->key_mask, t->key_size, t->seed);

		bkt_index = sig & t->bucket_mask;
		bkt0 = &t->buckets[bkt_index];
		sig = (sig >> 16) | 1LLU;

		/* Key is present in the bucket */
		for (bkt = bkt0; bkt != NULL; bkt = BUCKET_NEXT(bkt))
			for (i = 0; i < KEYS_PER_BUCKET; i++) {
				uint64_t bkt_sig = (uint64_t) bkt->sig[i];
				uint32_t bkt_key_index = bkt->key_pos[i];
				uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
					t->key_size_shl];

				if ((sig == bkt_sig) && (keycmp(bkt_key, key,
					t->key_mask, t->key_size) == 0)) {
					uint8_t *data = &t->data_mem[
					bkt_key_index << t->data_size_shl];

					pkts_mask_out |= pkt_mask;
					entries[pkt_index] = (void *) data;
					break;
				}
			}
	}

	*lookup_hit_mask = pkts_mask_out;
	return 0;
}

/*
 * mask = match bitmask
 * match = at least one match
 * match_many = more than one match
 * match_pos = position of first match
 *
 *----------------------------------------
 * mask		 match	 match_many	  match_pos
 *----------------------------------------
 * 0000		 0		 0			  00
 * 0001		 1		 0			  00
 * 0010		 1		 0			  01
 * 0011		 1		 1			  00
 *----------------------------------------
 * 0100		 1		 0			  10
 * 0101		 1		 1			  00
 * 0110		 1		 1			  01
 * 0111		 1		 1			  00
 *----------------------------------------
 * 1000		 1		 0			  11
 * 1001		 1		 1			  00
 * 1010		 1		 1			  01
 * 1011		 1		 1			  00
 *----------------------------------------
 * 1100		 1		 1			  10
 * 1101		 1		 1			  00
 * 1110		 1		 1			  01
 * 1111		 1		 1			  00
 *----------------------------------------
 *
 * match = 1111_1111_1111_1110
 * match_many = 1111_1110_1110_1000
 * match_pos = 0001_0010_0001_0011__0001_0010_0001_0000
 *
 * match = 0xFFFELLU
 * match_many = 0xFEE8LLU
 * match_pos = 0x12131210LLU
 */

#define LUT_MATCH						0xFFFELLU
#define LUT_MATCH_MANY						0xFEE8LLU
#define LUT_MATCH_POS						0x12131210LLU

#define lookup_cmp_sig(mbuf_sig, bucket, match, match_many, match_pos)	\
{									\
	uint64_t bucket_sig[4], mask[4], mask_all;			\
									\
	bucket_sig[0] = bucket->sig[0];					\
	bucket_sig[1] = bucket->sig[1];					\
	bucket_sig[2] = bucket->sig[2];					\
	bucket_sig[3] = bucket->sig[3];					\
									\
	bucket_sig[0] ^= mbuf_sig;					\
	bucket_sig[1] ^= mbuf_sig;					\
	bucket_sig[2] ^= mbuf_sig;					\
	bucket_sig[3] ^= mbuf_sig;					\
									\
	mask[0] = 0;							\
	mask[1] = 0;							\
	mask[2] = 0;							\
	mask[3] = 0;							\
									\
	if (bucket_sig[0] == 0)						\
		mask[0] = 1;						\
	if (bucket_sig[1] == 0)						\
		mask[1] = 2;						\
	if (bucket_sig[2] == 0)						\
		mask[2] = 4;						\
	if (bucket_sig[3] == 0)						\
		mask[3] = 8;						\
									\
	mask_all = (mask[0] | mask[1]) | (mask[2] | mask[3]);		\
									\
	match = (LUT_MATCH >> mask_all) & 1;				\
	match_many = (LUT_MATCH_MANY >> mask_all) & 1;			\
	match_pos = (LUT_MATCH_POS >> (mask_all << 1)) & 3;		\
}

#define lookup_cmp_key(mbuf, key, match_key, f)				\
{									\
	uint64_t *pkt_key = RTE_MBUF_METADATA_UINT64_PTR(mbuf, f->key_offset);\
	uint64_t *bkt_key = (uint64_t *) key;				\
	uint64_t *key_mask = f->key_mask;					\
									\
	switch (f->key_size) {						\
	case 8:								\
	{								\
		uint64_t xor = (pkt_key[0] & key_mask[0]) ^ bkt_key[0];	\
		match_key = 0;						\
		if (xor == 0)						\
			match_key = 1;					\
	}								\
	break;								\
									\
	case 16:							\
	{								\
		uint64_t xor[2], or;					\
									\
		xor[0] = (pkt_key[0] & key_mask[0]) ^ bkt_key[0];		\
		xor[1] = (pkt_key[1] & key_mask[1]) ^ bkt_key[1];		\
		or = xor[0] | xor[1];					\
		match_key = 0;						\
		if (or == 0)						\
			match_key = 1;					\
	}								\
	break;								\
									\
	case 32:							\
	{								\
		uint64_t xor[4], or;					\
									\
		xor[0] = (pkt_key[0] & key_mask[0]) ^ bkt_key[0];		\
		xor[1] = (pkt_key[1] & key_mask[1]) ^ bkt_key[1];		\
		xor[2] = (pkt_key[2] & key_mask[2]) ^ bkt_key[2];		\
		xor[3] = (pkt_key[3] & key_mask[3]) ^ bkt_key[3];		\
		or = xor[0] | xor[1] | xor[2] | xor[3];			\
		match_key = 0;						\
		if (or == 0)						\
			match_key = 1;					\
	}								\
	break;								\
									\
	case 64:							\
	{								\
		uint64_t xor[8], or;					\
									\
		xor[0] = (pkt_key[0] & key_mask[0]) ^ bkt_key[0];		\
		xor[1] = (pkt_key[1] & key_mask[1]) ^ bkt_key[1];		\
		xor[2] = (pkt_key[2] & key_mask[2]) ^ bkt_key[2];		\
		xor[3] = (pkt_key[3] & key_mask[3]) ^ bkt_key[3];		\
		xor[4] = (pkt_key[4] & key_mask[4]) ^ bkt_key[4];		\
		xor[5] = (pkt_key[5] & key_mask[5]) ^ bkt_key[5];		\
		xor[6] = (pkt_key[6] & key_mask[6]) ^ bkt_key[6];		\
		xor[7] = (pkt_key[7] & key_mask[7]) ^ bkt_key[7];		\
		or = xor[0] | xor[1] | xor[2] | xor[3] |		\
			xor[4] | xor[5] | xor[6] | xor[7];		\
		match_key = 0;						\
		if (or == 0)						\
			match_key = 1;					\
	}								\
	break;								\
									\
	default:							\
		match_key = 0;						\
		if (keycmp(bkt_key, pkt_key, key_mask, f->key_size) == 0)	\
			match_key = 1;					\
	}								\
}

#define lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index)	\
{									\
	uint64_t pkt00_mask, pkt01_mask;				\
	struct rte_mbuf *mbuf00, *mbuf01;				\
	uint32_t key_offset = t->key_offset;			\
									\
	pkt00_index = rte_ctz64(pkts_mask);			\
	pkt00_mask = 1LLU << pkt00_index;				\
	pkts_mask &= ~pkt00_mask;					\
	mbuf00 = pkts[pkt00_index];					\
									\
	pkt01_index = rte_ctz64(pkts_mask);			\
	pkt01_mask = 1LLU << pkt01_index;				\
	pkts_mask &= ~pkt01_mask;					\
	mbuf01 = pkts[pkt01_index];					\
									\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));\
}

#define lookup2_stage0_with_odd_support(t, g, pkts, pkts_mask, pkt00_index, \
	pkt01_index)							\
{									\
	uint64_t pkt00_mask, pkt01_mask;				\
	struct rte_mbuf *mbuf00, *mbuf01;				\
	uint32_t key_offset = t->key_offset;			\
									\
	pkt00_index = rte_ctz64(pkts_mask);			\
	pkt00_mask = 1LLU << pkt00_index;				\
	pkts_mask &= ~pkt00_mask;					\
	mbuf00 = pkts[pkt00_index];					\
									\
	pkt01_index = rte_ctz64(pkts_mask);			\
	if (pkts_mask == 0)						\
		pkt01_index = pkt00_index;				\
	pkt01_mask = 1LLU << pkt01_index;				\
	pkts_mask &= ~pkt01_mask;					\
	mbuf01 = pkts[pkt01_index];					\
									\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));\
}

#define lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index)	\
{									\
	struct grinder *g10, *g11;					\
	uint64_t sig10, sig11, bkt10_index, bkt11_index;		\
	struct rte_mbuf *mbuf10, *mbuf11;				\
	struct bucket *bkt10, *bkt11, *buckets = t->buckets;		\
	uint8_t *key10, *key11;						\
	uint64_t bucket_mask = t->bucket_mask;				\
	rte_table_hash_op_hash f_hash = t->f_hash;			\
	uint64_t seed = t->seed;					\
	uint32_t key_size = t->key_size;				\
	uint32_t key_offset = t->key_offset;				\
									\
	mbuf10 = pkts[pkt10_index];					\
	key10 = RTE_MBUF_METADATA_UINT8_PTR(mbuf10, key_offset);	\
	sig10 = (uint64_t) f_hash(key10, t->key_mask, key_size, seed);	\
	bkt10_index = sig10 & bucket_mask;				\
	bkt10 = &buckets[bkt10_index];					\
									\
	mbuf11 = pkts[pkt11_index];					\
	key11 = RTE_MBUF_METADATA_UINT8_PTR(mbuf11, key_offset);	\
	sig11 = (uint64_t) f_hash(key11, t->key_mask, key_size, seed);	\
	bkt11_index = sig11 & bucket_mask;				\
	bkt11 = &buckets[bkt11_index];					\
									\
	rte_prefetch0(bkt10);						\
	rte_prefetch0(bkt11);						\
									\
	g10 = &g[pkt10_index];						\
	g10->sig = sig10;						\
	g10->bkt = bkt10;						\
									\
	g11 = &g[pkt11_index];						\
	g11->sig = sig11;						\
	g11->bkt = bkt11;						\
}

#define lookup2_stage2(t, g, pkt20_index, pkt21_index, pkts_mask_match_many)\
{									\
	struct grinder *g20, *g21;					\
	uint64_t sig20, sig21;						\
	struct bucket *bkt20, *bkt21;					\
	uint8_t *key20, *key21, *key_mem = t->key_mem;			\
	uint64_t match20, match21, match_many20, match_many21;		\
	uint64_t match_pos20, match_pos21;				\
	uint32_t key20_index, key21_index, key_size_shl = t->key_size_shl;\
									\
	g20 = &g[pkt20_index];						\
	sig20 = g20->sig;						\
	bkt20 = g20->bkt;						\
	sig20 = (sig20 >> 16) | 1LLU;					\
	lookup_cmp_sig(sig20, bkt20, match20, match_many20, match_pos20);\
	match20 <<= pkt20_index;					\
	match_many20 |= BUCKET_NEXT_VALID(bkt20);			\
	match_many20 <<= pkt20_index;					\
	key20_index = bkt20->key_pos[match_pos20];			\
	key20 = &key_mem[key20_index << key_size_shl];			\
									\
	g21 = &g[pkt21_index];						\
	sig21 = g21->sig;						\
	bkt21 = g21->bkt;						\
	sig21 = (sig21 >> 16) | 1LLU;					\
	lookup_cmp_sig(sig21, bkt21, match21, match_many21, match_pos21);\
	match21 <<= pkt21_index;					\
	match_many21 |= BUCKET_NEXT_VALID(bkt21);			\
	match_many21 <<= pkt21_index;					\
	key21_index = bkt21->key_pos[match_pos21];			\
	key21 = &key_mem[key21_index << key_size_shl];			\
									\
	rte_prefetch0(key20);						\
	rte_prefetch0(key21);						\
									\
	pkts_mask_match_many |= match_many20 | match_many21;		\
									\
	g20->match = match20;						\
	g20->key_index = key20_index;					\
									\
	g21->match = match21;						\
	g21->key_index = key21_index;					\
}

#define lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index, pkts_mask_out, \
	entries)							\
{									\
	struct grinder *g30, *g31;					\
	struct rte_mbuf *mbuf30, *mbuf31;				\
	uint8_t *key30, *key31, *key_mem = t->key_mem;			\
	uint8_t *data30, *data31, *data_mem = t->data_mem;		\
	uint64_t match30, match31, match_key30, match_key31, match_keys;\
	uint32_t key30_index, key31_index;				\
	uint32_t key_size_shl = t->key_size_shl;			\
	uint32_t data_size_shl = t->data_size_shl;			\
									\
	mbuf30 = pkts[pkt30_index];					\
	g30 = &g[pkt30_index];						\
	match30 = g30->match;						\
	key30_index = g30->key_index;					\
	key30 = &key_mem[key30_index << key_size_shl];			\
	lookup_cmp_key(mbuf30, key30, match_key30, t);			\
	match_key30 <<= pkt30_index;					\
	match_key30 &= match30;						\
	data30 = &data_mem[key30_index << data_size_shl];		\
	entries[pkt30_index] = data30;					\
									\
	mbuf31 = pkts[pkt31_index];					\
	g31 = &g[pkt31_index];						\
	match31 = g31->match;						\
	key31_index = g31->key_index;					\
	key31 = &key_mem[key31_index << key_size_shl];			\
	lookup_cmp_key(mbuf31, key31, match_key31, t);			\
	match_key31 <<= pkt31_index;					\
	match_key31 &= match31;						\
	data31 = &data_mem[key31_index << data_size_shl];		\
	entries[pkt31_index] = data31;					\
									\
	rte_prefetch0(data30);						\
	rte_prefetch0(data31);						\
									\
	match_keys = match_key30 | match_key31;				\
	pkts_mask_out |= match_keys;					\
}

/*
 * The lookup function implements a 4-stage pipeline, with each stage processing
 * two different packets. The purpose of pipelined implementation is to hide the
 * latency of prefetching the data structures and loosen the data dependency
 * between instructions.
 *
 *  p00  _______   p10  _______   p20  _______   p30  _______
 *----->|       |----->|       |----->|       |----->|       |----->
 *      |   0   |      |   1   |      |   2   |      |   3   |
 *----->|_______|----->|_______|----->|_______|----->|_______|----->
 *  p01            p11            p21            p31
 *
 * The naming convention is:
 *    pXY = packet Y of stage X, X = 0 .. 3, Y = 0 .. 1
 */
static int rte_table_hash_ext_lookup(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;
	struct grinder *g = t->grinders;
	uint64_t pkt00_index, pkt01_index, pkt10_index, pkt11_index;
	uint64_t pkt20_index, pkt21_index, pkt30_index, pkt31_index;
	uint64_t pkts_mask_out = 0, pkts_mask_match_many = 0;
	int status = 0;

	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);
	RTE_TABLE_HASH_EXT_STATS_PKTS_IN_ADD(t, n_pkts_in);

	/* Cannot run the pipeline with less than 7 packets */
	if (rte_popcount64(pkts_mask) < 7) {
		status = rte_table_hash_ext_lookup_unoptimized(table, pkts,
			pkts_mask, lookup_hit_mask, entries);
		RTE_TABLE_HASH_EXT_STATS_PKTS_LOOKUP_MISS(t, n_pkts_in -
				rte_popcount64(*lookup_hit_mask));
		return status;
	}

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline feed */
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline stage 1 */
	lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index);

	/* Pipeline feed */
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline stage 1 */
	lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index);

	/* Pipeline stage 2 */
	lookup2_stage2(t, g, pkt20_index, pkt21_index, pkts_mask_match_many);

	/*
	* Pipeline run
	*
	*/
	for ( ; pkts_mask; ) {
		/* Pipeline feed */
		pkt30_index = pkt20_index;
		pkt31_index = pkt21_index;
		pkt20_index = pkt10_index;
		pkt21_index = pkt11_index;
		pkt10_index = pkt00_index;
		pkt11_index = pkt01_index;

		/* Pipeline stage 0 */
		lookup2_stage0_with_odd_support(t, g, pkts, pkts_mask,
			pkt00_index, pkt01_index);

		/* Pipeline stage 1 */
		lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index);

		/* Pipeline stage 2 */
		lookup2_stage2(t, g, pkt20_index, pkt21_index,
			pkts_mask_match_many);

		/* Pipeline stage 3 */
		lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index,
			pkts_mask_out, entries);
	}

	/* Pipeline feed */
	pkt30_index = pkt20_index;
	pkt31_index = pkt21_index;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 1 */
	lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index);

	/* Pipeline stage 2 */
	lookup2_stage2(t, g, pkt20_index, pkt21_index, pkts_mask_match_many);

	/* Pipeline stage 3 */
	lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index, pkts_mask_out,
		entries);

	/* Pipeline feed */
	pkt30_index = pkt20_index;
	pkt31_index = pkt21_index;
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;

	/* Pipeline stage 2 */
	lookup2_stage2(t, g, pkt20_index, pkt21_index, pkts_mask_match_many);

	/* Pipeline stage 3 */
	lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index, pkts_mask_out,
		entries);

	/* Pipeline feed */
	pkt30_index = pkt20_index;
	pkt31_index = pkt21_index;

	/* Pipeline stage 3 */
	lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index, pkts_mask_out,
		entries);

	/* Slow path */
	pkts_mask_match_many &= ~pkts_mask_out;
	if (pkts_mask_match_many) {
		uint64_t pkts_mask_out_slow = 0;

		status = rte_table_hash_ext_lookup_unoptimized(table, pkts,
			pkts_mask_match_many, &pkts_mask_out_slow, entries);
		pkts_mask_out |= pkts_mask_out_slow;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_EXT_STATS_PKTS_LOOKUP_MISS(t, n_pkts_in - rte_popcount64(pkts_mask_out));
	return status;
}

static int
rte_table_hash_ext_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_hash *t = table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_hash_ext_ops	 = {
	.f_create = rte_table_hash_ext_create,
	.f_free = rte_table_hash_ext_free,
	.f_add = rte_table_hash_ext_entry_add,
	.f_delete = rte_table_hash_ext_entry_delete,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_ext_lookup,
	.f_stats = rte_table_hash_ext_stats_read,
};
