/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>

#include "rte_swx_table_learner.h"

#ifndef RTE_SWX_TABLE_LEARNER_USE_HUGE_PAGES
#define RTE_SWX_TABLE_LEARNER_USE_HUGE_PAGES 1
#endif

#ifndef RTE_SWX_TABLE_SELECTOR_HUGE_PAGES_DISABLE

#include <rte_malloc.h>

static void *
env_calloc(size_t size, size_t alignment, int numa_node)
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
env_calloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	void *start;

	if (numa_available() == -1)
		return NULL;

	start = numa_alloc_onnode(size, numa_node);
	if (!start)
		return NULL;

	memset(start, 0, size);
	return start;
}

static void
env_free(void *start, size_t size)
{
	if ((numa_available() == -1) || !start)
		return;

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
table_keycpy(void *dst, void *src, void *src_mask, uint32_t n_bytes)
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
table_keycmp(void *a, void *b, void *b_mask, uint32_t n_bytes)
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

#define TABLE_KEYS_PER_BUCKET 4

#define TABLE_BUCKET_USEFUL_SIZE \
	(TABLE_KEYS_PER_BUCKET * (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t)))

#define TABLE_BUCKET_PAD_SIZE \
	(RTE_CACHE_LINE_SIZE - TABLE_BUCKET_USEFUL_SIZE)

struct table_bucket {
	uint32_t time[TABLE_KEYS_PER_BUCKET];
	uint32_t sig[TABLE_KEYS_PER_BUCKET];
	uint8_t key_timeout_id[TABLE_KEYS_PER_BUCKET];
	uint8_t pad[TABLE_BUCKET_PAD_SIZE];
	uint8_t key[];
};

struct table_params {
	/* The real key size. Must be non-zero. */
	size_t key_size;

	/* They key size upgrated to the next power of 2. This used for hash generation (in
	 * increments of 8 bytes, from 8 to 64 bytes) and for run-time key comparison. This is why
	 * key sizes bigger than 64 bytes are not allowed.
	 */
	size_t key_size_pow2;

	/* log2(key_size_pow2). Purpose: avoid multiplication with non-power-of-2 numbers. */
	size_t key_size_log2;

	/* The key offset within the key buffer. */
	size_t key_offset;

	/* The real action data size. */
	size_t action_data_size;

	/* The data size, i.e. the 8-byte action_id field plus the action data size, upgraded to the
	 * next power of 2.
	 */
	size_t data_size_pow2;

	/* log2(data_size_pow2). Purpose: avoid multiplication with non-power of 2 numbers. */
	size_t data_size_log2;

	/* Number of buckets. Must be a power of 2 to avoid modulo with non-power-of-2 numbers. */
	size_t n_buckets;

	/* Bucket mask. Purpose: replace modulo with bitmask and operation. */
	size_t bucket_mask;

	/* Total number of key bytes in the bucket, including the key padding bytes. There are
	 * (key_size_pow2 - key_size) padding bytes for each key in the bucket.
	 */
	size_t bucket_key_all_size;

	/* Bucket size. Must be a power of 2 to avoid multiplication with non-power-of-2 number. */
	size_t bucket_size;

	/* log2(bucket_size). Purpose: avoid multiplication with non-power of 2 numbers. */
	size_t bucket_size_log2;

	/* Set of all possible key timeout values measured in CPU clock cycles. */
	uint64_t key_timeout[RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX];

	/* Number of key timeout values. */
	uint32_t n_key_timeouts;

	/* Total memory size. */
	size_t total_size;
};

struct table {
	/* Table parameters. */
	struct table_params params;

	/* Key mask. Array of *key_size* bytes. */
	uint8_t key_mask0[RTE_CACHE_LINE_SIZE];

	/* Table buckets. */
	uint8_t buckets[];
} __rte_cache_aligned;

/* The timeout (in cycles) is stored in the table as a 32-bit value by truncating its least
 * significant 32 bits. Therefore, to make sure the time is always advancing when adding the timeout
 * value on top of the current time, the minimum timeout value is 1^32 cycles, which is 2 seconds on
 * a 2 GHz CPU.
 */
static uint64_t
timeout_convert(uint32_t timeout_in_seconds)
{
	uint64_t timeout_in_cycles = timeout_in_seconds * rte_get_tsc_hz();

	if (!(timeout_in_cycles >> 32))
		timeout_in_cycles = 1LLU << 32;

	return timeout_in_cycles;
}

static int
table_params_get(struct table_params *p, struct rte_swx_table_learner_params *params)
{
	uint32_t i;

	/* Check input parameters. */
	if (!params ||
	    !params->key_size ||
	    (params->key_size > 64) ||
	    !params->n_keys_max ||
	    (params->n_keys_max > 1U << 31) ||
	    !params->key_timeout ||
	    !params->n_key_timeouts ||
	    (params->n_key_timeouts > RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX))
		return -EINVAL;

	for (i = 0; i < params->n_key_timeouts; i++)
		if (!params->key_timeout[i])
			return -EINVAL;

	/* Key. */
	p->key_size = params->key_size;

	p->key_size_pow2 = rte_align64pow2(p->key_size);
	if (p->key_size_pow2 < 8)
		p->key_size_pow2 = 8;

	p->key_size_log2 = __builtin_ctzll(p->key_size_pow2);

	p->key_offset = params->key_offset;

	/* Data. */
	p->action_data_size = params->action_data_size;

	p->data_size_pow2 = rte_align64pow2(sizeof(uint64_t) + p->action_data_size);

	p->data_size_log2 = __builtin_ctzll(p->data_size_pow2);

	/* Buckets. */
	p->n_buckets = rte_align32pow2(params->n_keys_max);

	p->bucket_mask = p->n_buckets - 1;

	p->bucket_key_all_size = TABLE_KEYS_PER_BUCKET * p->key_size_pow2;

	p->bucket_size = rte_align64pow2(sizeof(struct table_bucket) +
					 p->bucket_key_all_size +
					 TABLE_KEYS_PER_BUCKET * p->data_size_pow2);

	p->bucket_size_log2 = __builtin_ctzll(p->bucket_size);

	/* Timeout. */
	for (i = 0; i < params->n_key_timeouts; i++)
		p->key_timeout[i] = timeout_convert(params->key_timeout[i]);

	p->n_key_timeouts = rte_align32pow2(params->n_key_timeouts);

	for ( ; i < p->n_key_timeouts; i++)
		p->key_timeout[i] = p->key_timeout[0];

	/* Total size. */
	p->total_size = sizeof(struct table) + p->n_buckets * p->bucket_size;

	return 0;
}

static inline struct table_bucket *
table_bucket_get(struct table *t, size_t bucket_id)
{
	return (struct table_bucket *)&t->buckets[bucket_id << t->params.bucket_size_log2];
}

static inline uint8_t *
table_bucket_key_get(struct table *t, struct table_bucket *b, size_t bucket_key_pos)
{
	return &b->key[bucket_key_pos << t->params.key_size_log2];
}

static inline uint64_t *
table_bucket_data_get(struct table *t, struct table_bucket *b, size_t bucket_key_pos)
{
	return (uint64_t *)&b->key[t->params.bucket_key_all_size +
				   (bucket_key_pos << t->params.data_size_log2)];
}

uint64_t
rte_swx_table_learner_footprint_get(struct rte_swx_table_learner_params *params)
{
	struct table_params p;
	int status;

	status = table_params_get(&p, params);

	return status ? 0 : p.total_size;
}

void *
rte_swx_table_learner_create(struct rte_swx_table_learner_params *params, int numa_node)
{
	struct table_params p;
	struct table *t;
	int status;

	/* Check and process the input parameters. */
	status = table_params_get(&p, params);
	if (status)
		return NULL;

	/* Memory allocation. */
	t = env_calloc(p.total_size, RTE_CACHE_LINE_SIZE, numa_node);
	if (!t)
		return NULL;

	/* Memory initialization. */
	memcpy(&t->params, &p, sizeof(struct table_params));

	if (params->key_mask0)
		memcpy(t->key_mask0, params->key_mask0, params->key_size);
	else
		memset(t->key_mask0, 0xFF, params->key_size);

	return t;
}

void
rte_swx_table_learner_free(void *table)
{
	struct table *t = table;

	if (!t)
		return;

	env_free(t, t->params.total_size);
}

int
rte_swx_table_learner_timeout_update(void *table,
				     uint32_t key_timeout_id,
				     uint32_t key_timeout)
{
	struct table *t = table;

	if (!t ||
	    (key_timeout_id >= t->params.n_key_timeouts) ||
	    !key_timeout)
		return -EINVAL;

	t->params.key_timeout[key_timeout_id] = timeout_convert(key_timeout);

	return 0;
}

struct mailbox {
	/* Writer: lookup state 0. Reader(s): lookup state 1, add(). */
	struct table_bucket *bucket;

	/* Writer: lookup state 0. Reader(s): lookup state 1, add(). */
	uint32_t input_sig;

	/* Writer: lookup state 1. Reader(s): add(). */
	uint8_t *input_key;

	/* Writer: lookup state 1. Reader(s): add(). Values: 0 = miss; 1 = hit. */
	uint32_t hit;

	/* Writer: lookup state 1. Reader(s): add(). Valid only when hit is non-zero. */
	size_t bucket_key_pos;

	/* State. */
	int state;
};

uint64_t
rte_swx_table_learner_mailbox_size_get(void)
{
	return sizeof(struct mailbox);
}

int
rte_swx_table_learner_lookup(void *table,
			     void *mailbox,
			     uint64_t input_time,
			     uint8_t **key,
			     uint64_t *action_id,
			     uint8_t **action_data,
			     int *hit)
{
	struct table *t = table;
	struct mailbox *m = mailbox;

	switch (m->state) {
	case 0: {
		uint8_t *input_key;
		struct table_bucket *b;
		size_t bucket_id;
		uint32_t input_sig;

		input_key = &(*key)[t->params.key_offset];
		input_sig = hash(input_key, t->key_mask0, t->params.key_size_pow2, 0);
		bucket_id = input_sig & t->params.bucket_mask;
		b = table_bucket_get(t, bucket_id);

		rte_prefetch0(b);
		rte_prefetch0(&b->key[0]);
		rte_prefetch0(&b->key[RTE_CACHE_LINE_SIZE]);

		m->bucket = b;
		m->input_key = input_key;
		m->input_sig = input_sig | 1;
		m->state = 1;
		return 0;
	}

	case 1: {
		struct table_bucket *b = m->bucket;
		uint32_t i;

		/* Search the input key through the bucket keys. */
		for (i = 0; i < TABLE_KEYS_PER_BUCKET; i++) {
			uint64_t time = b->time[i];
			uint32_t sig = b->sig[i];
			uint8_t *key = table_bucket_key_get(t, b, i);
			uint32_t key_size_pow2 = t->params.key_size_pow2;

			time <<= 32;

			if ((time > input_time) &&
			    (sig == m->input_sig) &&
			    table_keycmp(key, m->input_key, t->key_mask0, key_size_pow2)) {
				uint64_t *data = table_bucket_data_get(t, b, i);

				/* Hit. */
				rte_prefetch0(data);

				m->hit = 1;
				m->bucket_key_pos = i;
				m->state = 0;

				*action_id = data[0];
				*action_data = (uint8_t *)&data[1];
				*hit = 1;
				return 1;
			}
		}

		/* Miss. */
		m->hit = 0;
		m->state = 0;

		*hit = 0;
		return 1;
	}

	default:
		/* This state should never be reached. Miss. */
		m->hit = 0;
		m->state = 0;

		*hit = 0;
		return 1;
	}
}

void
rte_swx_table_learner_rearm(void *table,
			    void *mailbox,
			    uint64_t input_time)
{
	struct table *t = table;
	struct mailbox *m = mailbox;
	struct table_bucket *b;
	size_t bucket_key_pos;
	uint64_t key_timeout;
	uint32_t key_timeout_id;

	if (!m->hit)
		return;

	b = m->bucket;
	bucket_key_pos = m->bucket_key_pos;

	key_timeout_id = b->key_timeout_id[bucket_key_pos];
	key_timeout = t->params.key_timeout[key_timeout_id];
	b->time[bucket_key_pos] = (input_time + key_timeout) >> 32;
}

void
rte_swx_table_learner_rearm_new(void *table,
				void *mailbox,
				uint64_t input_time,
				uint32_t key_timeout_id)
{
	struct table *t = table;
	struct mailbox *m = mailbox;
	struct table_bucket *b;
	size_t bucket_key_pos;
	uint64_t key_timeout;

	if (!m->hit)
		return;

	b = m->bucket;
	bucket_key_pos = m->bucket_key_pos;

	key_timeout_id &= t->params.n_key_timeouts - 1;
	key_timeout = t->params.key_timeout[key_timeout_id];
	b->time[bucket_key_pos] = (input_time + key_timeout) >> 32;
	b->key_timeout_id[bucket_key_pos] = (uint8_t)key_timeout_id;
}

uint32_t
rte_swx_table_learner_add(void *table,
			  void *mailbox,
			  uint64_t input_time,
			  uint64_t action_id,
			  uint8_t *action_data,
			  uint32_t key_timeout_id)
{
	struct table *t = table;
	struct mailbox *m = mailbox;
	struct table_bucket *b = m->bucket;
	uint64_t key_timeout;
	uint32_t i;

	/* Adjust the key timeout ID to fit the valid range. */
	key_timeout_id &= t->params.n_key_timeouts - 1;
	key_timeout = t->params.key_timeout[key_timeout_id];

	/* Lookup hit: The following bucket fields need to be updated:
	 * - key (key, sig): NO (already correctly set).
	 * - key timeout (key_timeout_id, time): YES.
	 * - key data (data): YES.
	 */
	if (m->hit) {
		size_t bucket_key_pos = m->bucket_key_pos;
		uint64_t *data = table_bucket_data_get(t, b, bucket_key_pos);

		/* Install the key timeout. */
		b->time[bucket_key_pos] = (input_time + key_timeout) >> 32;
		b->key_timeout_id[bucket_key_pos] = (uint8_t)key_timeout_id;

		/* Install the key data. */
		data[0] = action_id;
		if (t->params.action_data_size && action_data)
			memcpy(&data[1], action_data, t->params.action_data_size);

		return 0;
	}

	/* Lookup miss: Search for a free position in the current bucket and install the key. */
	for (i = 0; i < TABLE_KEYS_PER_BUCKET; i++) {
		uint64_t time = b->time[i];

		time <<= 32;

		/* Free position: Either there was never a key installed here, so the key time is
		 * set to zero (the init value), which is always less than the current time, or this
		 * position was used before, but the key expired (the key time is in the past).
		 */
		if (time < input_time) {
			uint8_t *key = table_bucket_key_get(t, b, i);
			uint64_t *data = table_bucket_data_get(t, b, i);

			/* Install the key and the key timeout. */
			b->time[i] = (input_time + key_timeout) >> 32;
			b->sig[i] = m->input_sig;
			b->key_timeout_id[i] = (uint8_t)key_timeout_id;
			table_keycpy(key, m->input_key, t->key_mask0, t->params.key_size_pow2);

			/* Install the key data. */
			data[0] = action_id;
			if (t->params.action_data_size && action_data)
				memcpy(&data[1], action_data, t->params.action_data_size);

			/* Mailbox. */
			m->hit = 1;
			m->bucket_key_pos = i;

			return 0;
		}
	}

	/* Bucket full. */
	return 1;
}

void
rte_swx_table_learner_delete(void *table __rte_unused,
			     void *mailbox)
{
	struct mailbox *m = mailbox;

	if (m->hit) {
		struct table_bucket *b = m->bucket;

		/* Expire the key. */
		b->time[m->bucket_key_pos] = 0;

		/* Mailbox. */
		m->hit = 0;
	}
}
