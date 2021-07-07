/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_prefetch.h>

#include "rte_swx_table_em.h"

#define CHECK(condition, err_code)                                             \
do {                                                                           \
	if (!(condition))                                                      \
		return -(err_code);                                            \
} while (0)

#ifndef RTE_SWX_TABLE_EM_USE_HUGE_PAGES
#define RTE_SWX_TABLE_EM_USE_HUGE_PAGES 1
#endif

#if RTE_SWX_TABLE_EM_USE_HUGE_PAGES

#include <rte_malloc.h>

static void *
env_malloc(size_t size, size_t alignment, int numa_node)
{
	return rte_zmalloc_socket(NULL, size, alignment, numa_node);
}

static void
env_free(void *start, size_t size __rte_unused)
{
	rte_free(start);
}

#else

#include <numa.h>

static void *
env_malloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	return numa_alloc_onnode(size, numa_node);
}

static void
env_free(void *start, size_t size)
{
	numa_free(start, size);
}

#endif

#if defined(RTE_ARCH_X86_64)

#include <x86intrin.h>

#define crc32_u64(crc, v) _mm_crc32_u64(crc, v)

#else

static inline uint64_t
crc32_u64_generic(uint64_t crc, uint64_t value)
{
	int i;

	crc = (crc & 0xFFFFFFFFLLU) ^ value;
	for (i = 63; i >= 0; i--) {
		uint64_t mask;

		mask = -(crc & 1LLU);
		crc = (crc >> 1LLU) ^ (0x82F63B78LLU & mask);
	}

	return crc;
}

#define crc32_u64(crc, v) crc32_u64_generic(crc, v)

#endif

/* Key size needs to be one of: 8, 16, 32 or 64. */
static inline uint32_t
hash(void *key, void *key_mask, uint32_t key_size, uint32_t seed)
{
	uint64_t *k = key;
	uint64_t *m = key_mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	switch (key_size) {
	case 8:
		crc0 = crc32_u64(seed, k[0] & m[0]);
		return crc0;

	case 16:
		k0 = k[0] & m[0];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc0 ^= crc1;

		return crc0;

	case 32:
		k0 = k[0] & m[0];
		k2 = k[2] & m[2];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc2 = crc32_u64(k2, k[3] & m[3]);
		crc3 = k2 >> 32;

		crc0 = crc32_u64(crc0, crc1);
		crc1 = crc32_u64(crc2, crc3);

		crc0 ^= crc1;

		return crc0;

	case 64:
		k0 = k[0] & m[0];
		k2 = k[2] & m[2];
		k5 = k[5] & m[5];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc2 = crc32_u64(k2, k[3] & m[3]);
		crc3 = crc32_u64(k2 >> 32, k[4] & m[4]);

		crc4 = crc32_u64(k5, k[6] & m[6]);
		crc5 = crc32_u64(k5 >> 32, k[7] & m[7]);

		crc0 = crc32_u64(crc0, (crc1 << 32) ^ crc2);
		crc1 = crc32_u64(crc3, (crc4 << 32) ^ crc5);

		crc0 ^= crc1;

		return crc0;

	default:
		crc0 = 0;
		return crc0;
	}
}

/* n_bytes needs to be a multiple of 8 bytes. */
static void
keycpy(void *dst, void *src, void *src_mask, uint32_t n_bytes)
{
	uint64_t *dst64 = dst, *src64 = src, *src_mask64 = src_mask;
	uint32_t i;

	for (i = 0; i < n_bytes / sizeof(uint64_t); i++)
		dst64[i] = src64[i] & src_mask64[i];
}

/*
 * Return: 0 = Keys are NOT equal; 1 = Keys are equal.
 */
static inline uint32_t
keycmp(void *a, void *b, void *b_mask, uint32_t n_bytes)
{
	uint64_t *a64 = a, *b64 = b, *b_mask64 = b_mask;

	switch (n_bytes) {
	case 8: {
		uint64_t xor0 = a64[0] ^ (b64[0] & b_mask64[0]);
		uint32_t result = 1;

		if (xor0)
			result = 0;
		return result;
	}

	case 16: {
		uint64_t xor0 = a64[0] ^ (b64[0] & b_mask64[0]);
		uint64_t xor1 = a64[1] ^ (b64[1] & b_mask64[1]);
		uint64_t or = xor0 | xor1;
		uint32_t result = 1;

		if (or)
			result = 0;
		return result;
	}

	case 32: {
		uint64_t xor0 = a64[0] ^ (b64[0] & b_mask64[0]);
		uint64_t xor1 = a64[1] ^ (b64[1] & b_mask64[1]);
		uint64_t xor2 = a64[2] ^ (b64[2] & b_mask64[2]);
		uint64_t xor3 = a64[3] ^ (b64[3] & b_mask64[3]);
		uint64_t or = (xor0 | xor1) | (xor2 | xor3);
		uint32_t result = 1;

		if (or)
			result = 0;
		return result;
	}

	case 64: {
		uint64_t xor0 = a64[0] ^ (b64[0] & b_mask64[0]);
		uint64_t xor1 = a64[1] ^ (b64[1] & b_mask64[1]);
		uint64_t xor2 = a64[2] ^ (b64[2] & b_mask64[2]);
		uint64_t xor3 = a64[3] ^ (b64[3] & b_mask64[3]);
		uint64_t xor4 = a64[4] ^ (b64[4] & b_mask64[4]);
		uint64_t xor5 = a64[5] ^ (b64[5] & b_mask64[5]);
		uint64_t xor6 = a64[6] ^ (b64[6] & b_mask64[6]);
		uint64_t xor7 = a64[7] ^ (b64[7] & b_mask64[7]);
		uint64_t or = ((xor0 | xor1) | (xor2 | xor3)) |
			      ((xor4 | xor5) | (xor6 | xor7));
		uint32_t result = 1;

		if (or)
			result = 0;
		return result;
	}

	default: {
		uint32_t i;

		for (i = 0; i < n_bytes / sizeof(uint64_t); i++)
			if (a64[i] != (b64[i] & b_mask64[i]))
				return 0;
		return 1;
	}
	}
}

#define KEYS_PER_BUCKET 4

struct bucket_extension {
	struct bucket_extension *next;
	uint16_t sig[KEYS_PER_BUCKET];
	uint32_t key_id[KEYS_PER_BUCKET];
};

struct table {
	/* Input parameters */
	struct rte_swx_table_params params;

	/* Internal. */
	uint32_t key_size;
	uint32_t data_size;
	uint32_t key_size_shl;
	uint32_t data_size_shl;
	uint32_t n_buckets;
	uint32_t n_buckets_ext;
	uint32_t key_stack_tos;
	uint32_t bkt_ext_stack_tos;
	uint64_t total_size;

	/* Memory arrays. */
	uint8_t *key_mask;
	struct bucket_extension *buckets;
	struct bucket_extension *buckets_ext;
	uint8_t *keys;
	uint32_t *key_stack;
	uint32_t *bkt_ext_stack;
	uint8_t *data;
};

static inline uint8_t *
table_key(struct table *t, uint32_t key_id)
{
	return &t->keys[(uint64_t)key_id << t->key_size_shl];
}

static inline uint64_t *
table_key_data(struct table *t, uint32_t key_id)
{
	return (uint64_t *)&t->data[(uint64_t)key_id << t->data_size_shl];
}

static inline int
bkt_is_empty(struct bucket_extension *bkt)
{
	return (!bkt->sig[0] && !bkt->sig[1] && !bkt->sig[2] && !bkt->sig[3]) ?
		1 : 0;
}

/* Return:
 *    0 = Bucket key position is NOT empty;
 *    1 = Bucket key position is empty.
 */
static inline int
bkt_key_is_empty(struct bucket_extension *bkt, uint32_t bkt_pos)
{
	return bkt->sig[bkt_pos] ? 0 : 1;
}

/* Return: 0 = Keys are NOT equal; 1 = Keys are equal. */
static inline int
bkt_keycmp(struct table *t,
	   struct bucket_extension *bkt,
	   uint8_t *input_key,
	   uint32_t bkt_pos,
	   uint32_t input_sig)
{
	uint32_t bkt_key_id;
	uint8_t *bkt_key;

	/* Key signature comparison. */
	if (input_sig != bkt->sig[bkt_pos])
		return 0;

	/* Key comparison. */
	bkt_key_id = bkt->key_id[bkt_pos];
	bkt_key = table_key(t, bkt_key_id);
	return keycmp(bkt_key, input_key, t->key_mask, t->key_size);
}

static inline void
bkt_key_install(struct table *t,
		struct bucket_extension *bkt,
		struct rte_swx_table_entry *input,
		uint32_t bkt_pos,
		uint32_t bkt_key_id,
		uint32_t input_sig)
{
	uint8_t *bkt_key;
	uint64_t *bkt_data;

	/* Key signature. */
	bkt->sig[bkt_pos] = (uint16_t)input_sig;

	/* Key. */
	bkt->key_id[bkt_pos] = bkt_key_id;
	bkt_key = table_key(t, bkt_key_id);
	keycpy(bkt_key, input->key, t->key_mask, t->key_size);

	/* Key data. */
	bkt_data = table_key_data(t, bkt_key_id);
	bkt_data[0] = input->action_id;
	if (t->params.action_data_size && input->action_data)
		memcpy(&bkt_data[1],
		       input->action_data,
		       t->params.action_data_size);
}

static inline void
bkt_key_data_update(struct table *t,
		    struct bucket_extension *bkt,
		    struct rte_swx_table_entry *input,
		    uint32_t bkt_pos)
{
	uint32_t bkt_key_id;
	uint64_t *bkt_data;

	/* Key. */
	bkt_key_id = bkt->key_id[bkt_pos];

	/* Key data. */
	bkt_data = table_key_data(t, bkt_key_id);
	bkt_data[0] = input->action_id;
	if (t->params.action_data_size && input->action_data)
		memcpy(&bkt_data[1],
		       input->action_data,
		       t->params.action_data_size);
}

#define CL RTE_CACHE_LINE_ROUNDUP

static int
__table_create(struct table **table,
	       uint64_t *memory_footprint,
	       struct rte_swx_table_params *params,
	       const char *args __rte_unused,
	       int numa_node)
{
	struct table *t;
	uint8_t *memory;
	size_t table_meta_sz, key_mask_sz, bucket_sz, bucket_ext_sz, key_sz,
		key_stack_sz, bkt_ext_stack_sz, data_sz, total_size;
	size_t key_mask_offset, bucket_offset, bucket_ext_offset, key_offset,
		key_stack_offset, bkt_ext_stack_offset, data_offset;
	uint32_t key_size, key_data_size, n_buckets, n_buckets_ext, i;

	/* Check input arguments. */
	CHECK(params, EINVAL);
	CHECK(params->match_type == RTE_SWX_TABLE_MATCH_EXACT, EINVAL);
	CHECK(params->key_size, EINVAL);
	CHECK(params->key_size <= 64, EINVAL);
	CHECK(params->n_keys_max, EINVAL);

	/* Memory allocation. */
	key_size = rte_align64pow2(params->key_size);
	if (key_size < 8)
		key_size = 8;
	key_data_size = rte_align64pow2(params->action_data_size + 8);
	n_buckets = params->n_keys_max / KEYS_PER_BUCKET;
	n_buckets_ext = params->n_keys_max / KEYS_PER_BUCKET;

	table_meta_sz = CL(sizeof(struct table));
	key_mask_sz = CL(key_size);
	bucket_sz = CL(n_buckets * sizeof(struct bucket_extension));
	bucket_ext_sz = CL(n_buckets_ext * sizeof(struct bucket_extension));
	key_sz = CL(params->n_keys_max * key_size);
	key_stack_sz = CL(params->n_keys_max * sizeof(uint32_t));
	bkt_ext_stack_sz = CL(n_buckets_ext * sizeof(uint32_t));
	data_sz = CL(params->n_keys_max * key_data_size);
	total_size = table_meta_sz + key_mask_sz + bucket_sz + bucket_ext_sz +
		     key_sz + key_stack_sz + bkt_ext_stack_sz + data_sz;

	key_mask_offset = table_meta_sz;
	bucket_offset = key_mask_offset + key_mask_sz;
	bucket_ext_offset = bucket_offset + bucket_sz;
	key_offset = bucket_ext_offset + bucket_ext_sz;
	key_stack_offset = key_offset + key_sz;
	bkt_ext_stack_offset = key_stack_offset + key_stack_sz;
	data_offset = bkt_ext_stack_offset + bkt_ext_stack_sz;

	if (!table) {
		if (memory_footprint)
			*memory_footprint = total_size;
		return 0;
	}

	memory = env_malloc(total_size, RTE_CACHE_LINE_SIZE, numa_node);
	CHECK(memory,  ENOMEM);
	memset(memory, 0, total_size);

	/* Initialization. */
	t = (struct table *)memory;
	memcpy(&t->params, params, sizeof(*params));

	t->key_size = key_size;
	t->data_size = key_data_size;
	t->key_size_shl = __builtin_ctzl(key_size);
	t->data_size_shl = __builtin_ctzl(key_data_size);
	t->n_buckets = n_buckets;
	t->n_buckets_ext = n_buckets_ext;
	t->total_size = total_size;

	t->key_mask = &memory[key_mask_offset];
	t->buckets = (struct bucket_extension *)&memory[bucket_offset];
	t->buckets_ext = (struct bucket_extension *)&memory[bucket_ext_offset];
	t->keys = &memory[key_offset];
	t->key_stack = (uint32_t *)&memory[key_stack_offset];
	t->bkt_ext_stack = (uint32_t *)&memory[bkt_ext_stack_offset];
	t->data = &memory[data_offset];

	t->params.key_mask0 = t->key_mask;

	if (!params->key_mask0)
		memset(t->key_mask, 0xFF, params->key_size);
	else
		memcpy(t->key_mask, params->key_mask0, params->key_size);

	for (i = 0; i < t->params.n_keys_max; i++)
		t->key_stack[i] = t->params.n_keys_max - 1 - i;
	t->key_stack_tos = t->params.n_keys_max;

	for (i = 0; i < n_buckets_ext; i++)
		t->bkt_ext_stack[i] = n_buckets_ext - 1 - i;
	t->bkt_ext_stack_tos = n_buckets_ext;

	*table = t;
	return 0;
}

static void
table_free(void *table)
{
	struct table *t = table;

	if (!t)
		return;

	env_free(t, t->total_size);
}

static int
table_add(void *table, struct rte_swx_table_entry *entry)
{
	struct table *t = table;
	struct bucket_extension *bkt0, *bkt, *bkt_prev;
	uint32_t input_sig, bkt_id, i;

	CHECK(t, EINVAL);
	CHECK(entry, EINVAL);
	CHECK(entry->key, EINVAL);

	input_sig = hash(entry->key, t->key_mask, t->key_size, 0);
	bkt_id = input_sig & (t->n_buckets - 1);
	bkt0 = &t->buckets[bkt_id];
	input_sig = (input_sig >> 16) | 1;

	/* Key is present in the bucket. */
	for (bkt = bkt0; bkt; bkt = bkt->next)
		for (i = 0; i < KEYS_PER_BUCKET; i++)
			if (bkt_keycmp(t, bkt, entry->key, i, input_sig)) {
				bkt_key_data_update(t, bkt, entry, i);
				return 0;
			}

	/* Key is not present in the bucket. Bucket not full. */
	for (bkt = bkt0, bkt_prev = NULL; bkt; bkt_prev = bkt, bkt = bkt->next)
		for (i = 0; i < KEYS_PER_BUCKET; i++)
			if (bkt_key_is_empty(bkt, i)) {
				uint32_t new_bkt_key_id;

				/* Allocate new key & install. */
				CHECK(t->key_stack_tos, ENOSPC);
				new_bkt_key_id =
					t->key_stack[--t->key_stack_tos];
				bkt_key_install(t, bkt, entry, i,
						new_bkt_key_id, input_sig);
				return 0;
			}

	/* Bucket full: extend bucket. */
	if (t->bkt_ext_stack_tos && t->key_stack_tos) {
		struct bucket_extension *new_bkt;
		uint32_t new_bkt_id, new_bkt_key_id;

		/* Allocate new bucket extension & install. */
		new_bkt_id = t->bkt_ext_stack[--t->bkt_ext_stack_tos];
		new_bkt = &t->buckets_ext[new_bkt_id];
		memset(new_bkt, 0, sizeof(*new_bkt));
		bkt_prev->next = new_bkt;

		/* Allocate new key & install. */
		new_bkt_key_id = t->key_stack[--t->key_stack_tos];
		bkt_key_install(t, new_bkt, entry, 0,
				new_bkt_key_id, input_sig);
		return 0;
	}

	CHECK(0, ENOSPC);
}

static int
table_del(void *table, struct rte_swx_table_entry *entry)
{
	struct table *t = table;
	struct bucket_extension *bkt0, *bkt, *bkt_prev;
	uint32_t input_sig, bkt_id, i;

	CHECK(t, EINVAL);
	CHECK(entry, EINVAL);
	CHECK(entry->key, EINVAL);

	input_sig = hash(entry->key, t->key_mask, t->key_size, 0);
	bkt_id = input_sig & (t->n_buckets - 1);
	bkt0 = &t->buckets[bkt_id];
	input_sig = (input_sig >> 16) | 1;

	/* Key is present in the bucket. */
	for (bkt = bkt0, bkt_prev = NULL; bkt; bkt_prev = bkt, bkt = bkt->next)
		for (i = 0; i < KEYS_PER_BUCKET; i++)
			if (bkt_keycmp(t, bkt, entry->key, i, input_sig)) {
				/* Key free. */
				bkt->sig[i] = 0;
				t->key_stack[t->key_stack_tos++] =
					bkt->key_id[i];

				/* Bucket extension free if empty and not the
				 * 1st in bucket.
				 */
				if (bkt_prev && bkt_is_empty(bkt)) {
					bkt_prev->next = bkt->next;
					bkt_id = bkt - t->buckets_ext;
					t->bkt_ext_stack[t->bkt_ext_stack_tos++]
						= bkt_id;
				}

				return 0;
			}

	return 0;
}

static uint64_t
table_mailbox_size_get_unoptimized(void)
{
	return 0;
}

static int
table_lookup_unoptimized(void *table,
			 void *mailbox __rte_unused,
			 uint8_t **key,
			 uint64_t *action_id,
			 uint8_t **action_data,
			 int *hit)
{
	struct table *t = table;
	struct bucket_extension *bkt0, *bkt;
	uint8_t *input_key;
	uint32_t input_sig, bkt_id, i;

	input_key = &(*key)[t->params.key_offset];

	input_sig = hash(input_key, t->key_mask, t->key_size, 0);
	bkt_id = input_sig & (t->n_buckets - 1);
	bkt0 = &t->buckets[bkt_id];
	input_sig = (input_sig >> 16) | 1;

	/* Key is present in the bucket. */
	for (bkt = bkt0; bkt; bkt = bkt->next)
		for (i = 0; i < KEYS_PER_BUCKET; i++)
			if (bkt_keycmp(t, bkt, input_key, i, input_sig)) {
				uint32_t bkt_key_id;
				uint64_t *bkt_data;

				/* Key. */
				bkt_key_id = bkt->key_id[i];

				/* Key data. */
				bkt_data = table_key_data(t, bkt_key_id);
				*action_id = bkt_data[0];
				*action_data = (uint8_t *)&bkt_data[1];
				*hit = 1;
				return 1;
			}

	*hit = 0;
	return 1;
}

struct mailbox {
	struct bucket_extension *bkt;
	uint32_t input_sig;
	uint32_t bkt_key_id;
	uint32_t sig_match;
	uint32_t sig_match_many;
	int state;
};

static uint64_t
table_mailbox_size_get(void)
{
	return sizeof(struct mailbox);
}

/*
 * mask = match bitmask
 * match = at least one match
 * match_many = more than one match
 * match_pos = position of first match
 *
 *+------+-------+------------+-----------+
 *| mask | match | match_many | match_pos |
 *+------+-------+------------+-----------+
 *| 0000 | 0     | 0          | 00        |
 *| 0001 | 1     | 0          | 00        |
 *| 0010 | 1     | 0          | 01        |
 *| 0011 | 1     | 1          | 00        |
 *+------+-------+------------+-----------+
 *| 0100 | 1     | 0          | 10        |
 *| 0101 | 1     | 1          | 00        |
 *| 0110 | 1     | 1          | 01        |
 *| 0111 | 1     | 1          | 00        |
 *+------+-------+------------+-----------+
 *| 1000 | 1     | 0          | 11        |
 *| 1001 | 1     | 1          | 00        |
 *| 1010 | 1     | 1          | 01        |
 *| 1011 | 1     | 1          | 00        |
 *+------+-------+------------+-----------+
 *| 1100 | 1     | 1          | 10        |
 *| 1101 | 1     | 1          | 00        |
 *| 1110 | 1     | 1          | 01        |
 *| 1111 | 1     | 1          | 00        |
 *+------+-------+------------+-----------+
 *
 * match = 1111_1111_1111_1110 = 0xFFFE
 * match_many = 1111_1110_1110_1000 = 0xFEE8
 * match_pos = 0001_0010_0001_0011__0001_0010_0001_0000 = 0x12131210
 *
 */

#define LUT_MATCH      0xFFFE
#define LUT_MATCH_MANY 0xFEE8
#define LUT_MATCH_POS  0x12131210

static int
table_lookup(void *table,
	     void *mailbox,
	     uint8_t **key,
	     uint64_t *action_id,
	     uint8_t **action_data,
	     int *hit)
{
	struct table *t = table;
	struct mailbox *m = mailbox;

	switch (m->state) {
	case 0: {
		uint8_t *input_key = &(*key)[t->params.key_offset];
		struct bucket_extension *bkt;
		uint32_t input_sig, bkt_id;

		input_sig = hash(input_key, t->key_mask, t->key_size, 0);
		bkt_id = input_sig & (t->n_buckets - 1);
		bkt = &t->buckets[bkt_id];
		rte_prefetch0(bkt);

		m->bkt = bkt;
		m->input_sig = (input_sig >> 16) | 1;
		m->state++;
		return 0;
	}

	case 1: {
		struct bucket_extension *bkt = m->bkt;
		uint32_t input_sig = m->input_sig;
		uint32_t bkt_sig0, bkt_sig1, bkt_sig2, bkt_sig3;
		uint32_t mask0 = 0, mask1 = 0, mask2 = 0, mask3 = 0, mask_all;
		uint32_t sig_match = LUT_MATCH;
		uint32_t sig_match_many = LUT_MATCH_MANY;
		uint32_t sig_match_pos = LUT_MATCH_POS;
		uint32_t bkt_key_id;

		bkt_sig0 = input_sig ^ bkt->sig[0];
		if (!bkt_sig0)
			mask0 = 1 << 0;

		bkt_sig1 = input_sig ^ bkt->sig[1];
		if (!bkt_sig1)
			mask1 = 1 << 1;

		bkt_sig2 = input_sig ^ bkt->sig[2];
		if (!bkt_sig2)
			mask2 = 1 << 2;

		bkt_sig3 = input_sig ^ bkt->sig[3];
		if (!bkt_sig3)
			mask3 = 1 << 3;

		mask_all = (mask0 | mask1) | (mask2 | mask3);
		sig_match = (sig_match >> mask_all) & 1;
		sig_match_many = (sig_match_many >> mask_all) & 1;
		sig_match_pos = (sig_match_pos >> (mask_all << 1)) & 3;

		bkt_key_id = bkt->key_id[sig_match_pos];
		rte_prefetch0(table_key(t, bkt_key_id));
		rte_prefetch0(table_key_data(t, bkt_key_id));

		m->bkt_key_id = bkt_key_id;
		m->sig_match = sig_match;
		m->sig_match_many = sig_match_many;
		m->state++;
		return 0;
	}

	case 2: {
		uint8_t *input_key = &(*key)[t->params.key_offset];
		struct bucket_extension *bkt = m->bkt;
		uint32_t bkt_key_id = m->bkt_key_id;
		uint8_t *bkt_key = table_key(t, bkt_key_id);
		uint64_t *bkt_data = table_key_data(t, bkt_key_id);
		uint32_t lkp_hit;

		lkp_hit = keycmp(bkt_key, input_key, t->key_mask, t->key_size);
		lkp_hit &= m->sig_match;
		*action_id = bkt_data[0];
		*action_data = (uint8_t *)&bkt_data[1];
		*hit = lkp_hit;

		m->state = 0;

		if (!lkp_hit && (m->sig_match_many || bkt->next))
			return table_lookup_unoptimized(t,
							m,
							key,
							action_id,
							action_data,
							hit);

		return 1;
	}

	default:
		return 0;
	}
}

static void *
table_create(struct rte_swx_table_params *params,
	     struct rte_swx_table_entry_list *entries,
	     const char *args,
	     int numa_node)
{
	struct table *t;
	struct rte_swx_table_entry *entry;
	int status;

	/* Table create. */
	status = __table_create(&t, NULL, params, args, numa_node);
	if (status)
		return NULL;

	/* Table add entries. */
	if (!entries)
		return t;

	TAILQ_FOREACH(entry, entries, node) {
		int status;

		status = table_add(t, entry);
		if (status) {
			table_free(t);
			return NULL;
		}
	}

	return t;
}

static uint64_t
table_footprint(struct rte_swx_table_params *params,
		struct rte_swx_table_entry_list *entries __rte_unused,
		const char *args)
{
	uint64_t memory_footprint;
	int status;

	status = __table_create(NULL, &memory_footprint, params, args, 0);
	if (status)
		return 0;

	return memory_footprint;
}

struct rte_swx_table_ops rte_swx_table_exact_match_unoptimized_ops = {
	.footprint_get = table_footprint,
	.mailbox_size_get = table_mailbox_size_get_unoptimized,
	.create = table_create,
	.add = table_add,
	.del = table_del,
	.lkp = table_lookup_unoptimized,
	.free = table_free,
};

struct rte_swx_table_ops rte_swx_table_exact_match_ops = {
	.footprint_get = table_footprint,
	.mailbox_size_get = table_mailbox_size_get,
	.create = table_create,
	.add = table_add,
	.del = table_del,
	.lkp = table_lookup,
	.free = table_free,
};
