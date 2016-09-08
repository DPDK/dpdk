/*-
 *	 BSD LICENSE
 *
 *	 Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *	 All rights reserved.
 *
 *	 Redistribution and use in source and binary forms, with or without
 *	 modification, are permitted provided that the following conditions
 *	 are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *		 notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above copyright
 *		 notice, this list of conditions and the following disclaimer in
 *		 the documentation and/or other materials provided with the
 *		 distribution.
 *	* Neither the name of Intel Corporation nor the names of its
 *		 contributors may be used to endorse or promote products derived
 *		 from this software without specific prior written permission.
 *
 *	 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *	 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *	 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *	 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *	 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *	 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *	 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *	 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *	 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *	 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *	 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_hash.h"
#include "rte_lru.h"

#define RTE_TABLE_HASH_KEY_SIZE						8

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct rte_bucket_4_8 {
	/* Cache line 0 */
	uint64_t signature;
	uint64_t lru_list;
	struct rte_bucket_4_8 *next;
	uint64_t next_valid;

	uint64_t key[4];

	/* Cache line 1 */
	uint8_t data[0];
};

struct rte_table_hash {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t n_buckets;
	uint32_t n_entries_per_bucket;
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t bucket_size;
	uint32_t signature_offset;
	uint32_t key_offset;
	uint64_t key_mask;
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
check_params_create_lru(struct rte_table_hash_key8_lru_params *params) {
	/* n_entries */
	if (params->n_entries == 0) {
		RTE_LOG(ERR, TABLE, "%s: n_entries is zero\n", __func__);
		return -EINVAL;
	}

	/* f_hash */
	if (params->f_hash == NULL) {
		RTE_LOG(ERR, TABLE, "%s: f_hash function pointer is NULL\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_create_key8_lru(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_hash_key8_lru_params *p =
		(struct rte_table_hash_key8_lru_params *) params;
	struct rte_table_hash *f;
	uint32_t n_buckets, n_entries_per_bucket, key_size, bucket_size_cl;
	uint32_t total_size, i;

	/* Check input parameters */
	if ((check_params_create_lru(p) != 0) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		((sizeof(struct rte_bucket_4_8) % 64) != 0)) {
		return NULL;
	}
	n_entries_per_bucket = 4;
	key_size = 8;

	/* Memory allocation */
	n_buckets = rte_align32pow2((p->n_entries + n_entries_per_bucket - 1) /
		n_entries_per_bucket);
	bucket_size_cl = (sizeof(struct rte_bucket_4_8) + n_entries_per_bucket *
		entry_size + RTE_CACHE_LINE_SIZE - 1) / RTE_CACHE_LINE_SIZE;
	total_size = sizeof(struct rte_table_hash) + n_buckets *
		bucket_size_cl * RTE_CACHE_LINE_SIZE;

	f = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE, socket_id);
	if (f == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for hash table\n",
			__func__, total_size);
		return NULL;
	}
	RTE_LOG(INFO, TABLE,
		"%s: Hash table memory footprint is %u bytes\n",
		__func__, total_size);

	/* Memory initialization */
	f->n_buckets = n_buckets;
	f->n_entries_per_bucket = n_entries_per_bucket;
	f->key_size = key_size;
	f->entry_size = entry_size;
	f->bucket_size = bucket_size_cl * RTE_CACHE_LINE_SIZE;
	f->signature_offset = p->signature_offset;
	f->key_offset = p->key_offset;
	f->f_hash = p->f_hash;
	f->seed = p->seed;

	if (p->key_mask != NULL)
		f->key_mask = ((uint64_t *)p->key_mask)[0];
	else
		f->key_mask = 0xFFFFFFFFFFFFFFFFLLU;

	for (i = 0; i < n_buckets; i++) {
		struct rte_bucket_4_8 *bucket;

		bucket = (struct rte_bucket_4_8 *) &f->memory[i *
			f->bucket_size];
		bucket->lru_list = 0x0000000100020003LLU;
	}

	return f;
}

static int
rte_table_hash_free_key8_lru(void *table)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;

	/* Check input parameters */
	if (f == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(f);
	return 0;
}

static int
rte_table_hash_entry_add_key8_lru(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket;
	uint64_t signature, mask, pos;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket = (struct rte_bucket_4_8 *)
		&f->memory[bucket_index * f->bucket_size];

	/* Key is present in the bucket */
	for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
		uint64_t bucket_signature = bucket->signature;
		uint64_t bucket_key = bucket->key[i];

		if ((bucket_signature & mask) &&
		    (*((uint64_t *) key) == bucket_key)) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			memcpy(bucket_data, entry, f->entry_size);
			lru_update(bucket, i);
			*key_found = 1;
			*entry_ptr = (void *) bucket_data;
			return 0;
		}
	}

	/* Key is not present in the bucket */
	for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
		uint64_t bucket_signature = bucket->signature;

		if ((bucket_signature & mask) == 0) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			bucket->signature |= mask;
			bucket->key[i] = *((uint64_t *) key);
			memcpy(bucket_data, entry, f->entry_size);
			lru_update(bucket, i);
			*key_found = 0;
			*entry_ptr = (void *) bucket_data;

			return 0;
		}
	}

	/* Bucket full: replace LRU entry */
	pos = lru_pos(bucket);
	bucket->key[pos] = *((uint64_t *) key);
	memcpy(&bucket->data[pos * f->entry_size], entry, f->entry_size);
	lru_update(bucket, pos);
	*key_found	= 0;
	*entry_ptr = (void *) &bucket->data[pos * f->entry_size];

	return 0;
}

static int
rte_table_hash_entry_delete_key8_lru(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket;
	uint64_t signature, mask;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket = (struct rte_bucket_4_8 *)
		&f->memory[bucket_index * f->bucket_size];

	/* Key is present in the bucket */
	for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
		uint64_t bucket_signature = bucket->signature;
		uint64_t bucket_key = bucket->key[i];

		if ((bucket_signature & mask) &&
		    (*((uint64_t *) key) == bucket_key)) {
			uint8_t *bucket_data = &bucket->data[i * f->entry_size];

			bucket->signature &= ~mask;
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

static int
check_params_create_ext(struct rte_table_hash_key8_ext_params *params) {
	/* n_entries */
	if (params->n_entries == 0) {
		RTE_LOG(ERR, TABLE, "%s: n_entries is zero\n", __func__);
		return -EINVAL;
	}

	/* n_entries_ext */
	if (params->n_entries_ext == 0) {
		RTE_LOG(ERR, TABLE, "%s: n_entries_ext is zero\n", __func__);
		return -EINVAL;
	}

	/* f_hash */
	if (params->f_hash == NULL) {
		RTE_LOG(ERR, TABLE, "%s: f_hash function pointer is NULL\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_create_key8_ext(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_hash_key8_ext_params *p =
		(struct rte_table_hash_key8_ext_params *) params;
	struct rte_table_hash *f;
	uint32_t n_buckets, n_buckets_ext, n_entries_per_bucket, key_size;
	uint32_t bucket_size_cl, stack_size_cl, total_size, i;

	/* Check input parameters */
	if ((check_params_create_ext(p) != 0) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		((sizeof(struct rte_bucket_4_8) % 64) != 0))
		return NULL;

	n_entries_per_bucket = 4;
	key_size = 8;

	/* Memory allocation */
	n_buckets = rte_align32pow2((p->n_entries + n_entries_per_bucket - 1) /
		n_entries_per_bucket);
	n_buckets_ext = (p->n_entries_ext + n_entries_per_bucket - 1) /
		n_entries_per_bucket;
	bucket_size_cl = (sizeof(struct rte_bucket_4_8) + n_entries_per_bucket *
		entry_size + RTE_CACHE_LINE_SIZE - 1) / RTE_CACHE_LINE_SIZE;
	stack_size_cl = (n_buckets_ext * sizeof(uint32_t) + RTE_CACHE_LINE_SIZE - 1)
		/ RTE_CACHE_LINE_SIZE;
	total_size = sizeof(struct rte_table_hash) + ((n_buckets +
		n_buckets_ext) * bucket_size_cl + stack_size_cl) *
		RTE_CACHE_LINE_SIZE;

	f = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE, socket_id);
	if (f == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for hash table\n",
			__func__, total_size);
		return NULL;
	}
	RTE_LOG(INFO, TABLE,
		"%s: Hash table memory footprint is %u bytes\n",
		__func__, total_size);

	/* Memory initialization */
	f->n_buckets = n_buckets;
	f->n_entries_per_bucket = n_entries_per_bucket;
	f->key_size = key_size;
	f->entry_size = entry_size;
	f->bucket_size = bucket_size_cl * RTE_CACHE_LINE_SIZE;
	f->signature_offset = p->signature_offset;
	f->key_offset = p->key_offset;
	f->f_hash = p->f_hash;
	f->seed = p->seed;

	f->n_buckets_ext = n_buckets_ext;
	f->stack_pos = n_buckets_ext;
	f->stack = (uint32_t *)
		&f->memory[(n_buckets + n_buckets_ext) * f->bucket_size];

	if (p->key_mask != NULL)
		f->key_mask = ((uint64_t *)p->key_mask)[0];
	else
		f->key_mask = 0xFFFFFFFFFFFFFFFFLLU;

	for (i = 0; i < n_buckets_ext; i++)
		f->stack[i] = i;

	return f;
}

static int
rte_table_hash_free_key8_ext(void *table)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;

	/* Check input parameters */
	if (f == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}

	rte_free(f);
	return 0;
}

static int
rte_table_hash_entry_add_key8_ext(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket0, *bucket, *bucket_prev;
	uint64_t signature;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket0 = (struct rte_bucket_4_8 *)
		&f->memory[bucket_index * f->bucket_size];

	/* Key is present in the bucket */
	for (bucket = bucket0; bucket != NULL; bucket = bucket->next) {
		uint64_t mask;

		for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
			uint64_t bucket_signature = bucket->signature;
			uint64_t bucket_key = bucket->key[i];

			if ((bucket_signature & mask) &&
					(*((uint64_t *) key) == bucket_key)) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				memcpy(bucket_data, entry, f->entry_size);
				*key_found = 1;
				*entry_ptr = (void *) bucket_data;
				return 0;
			}
		}
	}

	/* Key is not present in the bucket */
	for (bucket_prev = NULL, bucket = bucket0;
		bucket != NULL; bucket_prev = bucket, bucket = bucket->next) {
		uint64_t mask;

		for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
			uint64_t bucket_signature = bucket->signature;

			if ((bucket_signature & mask) == 0) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				bucket->signature |= mask;
				bucket->key[i] = *((uint64_t *) key);
				memcpy(bucket_data, entry, f->entry_size);
				*key_found = 0;
				*entry_ptr = (void *) bucket_data;

				return 0;
			}
		}
	}

	/* Bucket full: extend bucket */
	if (f->stack_pos > 0) {
		bucket_index = f->stack[--f->stack_pos];

		bucket = (struct rte_bucket_4_8 *) &f->memory[(f->n_buckets +
			bucket_index) * f->bucket_size];
		bucket_prev->next = bucket;
		bucket_prev->next_valid = 1;

		bucket->signature = 1;
		bucket->key[0] = *((uint64_t *) key);
		memcpy(&bucket->data[0], entry, f->entry_size);
		*key_found = 0;
		*entry_ptr = (void *) &bucket->data[0];
		return 0;
	}

	return -ENOSPC;
}

static int
rte_table_hash_entry_delete_key8_ext(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket0, *bucket, *bucket_prev;
	uint64_t signature;
	uint32_t bucket_index, i;

	signature = f->f_hash(key, f->key_size, f->seed);
	bucket_index = signature & (f->n_buckets - 1);
	bucket0 = (struct rte_bucket_4_8 *)
		&f->memory[bucket_index * f->bucket_size];

	/* Key is present in the bucket */
	for (bucket_prev = NULL, bucket = bucket0; bucket != NULL;
		bucket_prev = bucket, bucket = bucket->next) {
		uint64_t mask;

		for (i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
			uint64_t bucket_signature = bucket->signature;
			uint64_t bucket_key = bucket->key[i];

			if ((bucket_signature & mask) &&
				(*((uint64_t *) key) == bucket_key)) {
				uint8_t *bucket_data = &bucket->data[i *
					f->entry_size];

				bucket->signature &= ~mask;
				*key_found = 1;
				if (entry)
					memcpy(entry, bucket_data,
						f->entry_size);

				if ((bucket->signature == 0) &&
				    (bucket_prev != NULL)) {
					bucket_prev->next = bucket->next;
					bucket_prev->next_valid =
						bucket->next_valid;

					memset(bucket, 0,
						sizeof(struct rte_bucket_4_8));
					bucket_index = (((uint8_t *)bucket -
						(uint8_t *)f->memory)/f->bucket_size) - f->n_buckets;
					f->stack[f->stack_pos++] = bucket_index;
				}

				return 0;
			}
		}
	}

	/* Key is not present in the bucket */
	*key_found = 0;
	return 0;
}

#define lookup_key8_cmp(key_in, bucket, pos)			\
{								\
	uint64_t xor[4], signature;				\
								\
	signature = ~bucket->signature;				\
								\
	xor[0] = (key_in[0] ^	 bucket->key[0]) | (signature & 1);\
	xor[1] = (key_in[0] ^	 bucket->key[1]) | (signature & 2);\
	xor[2] = (key_in[0] ^	 bucket->key[2]) | (signature & 4);\
	xor[3] = (key_in[0] ^	 bucket->key[3]) | (signature & 8);\
								\
	pos = 4;						\
	if (xor[0] == 0)					\
		pos = 0;					\
	if (xor[1] == 0)					\
		pos = 1;					\
	if (xor[2] == 0)					\
		pos = 2;					\
	if (xor[3] == 0)					\
		pos = 3;					\
}

#define lookup1_stage0(pkt0_index, mbuf0, pkts, pkts_mask, f)	\
{								\
	uint64_t pkt_mask;					\
	uint32_t key_offset = f->key_offset;\
								\
	pkt0_index = __builtin_ctzll(pkts_mask);		\
	pkt_mask = 1LLU << pkt0_index;				\
	pkts_mask &= ~pkt_mask;					\
								\
	mbuf0 = pkts[pkt0_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf0, key_offset));	\
}

#define lookup1_stage1(mbuf1, bucket1, f)			\
{								\
	uint64_t signature;					\
	uint32_t bucket_index;					\
								\
	signature = RTE_MBUF_METADATA_UINT32(mbuf1, f->signature_offset);\
	bucket_index = signature & (f->n_buckets - 1);		\
	bucket1 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket_index * f->bucket_size];	\
	rte_prefetch0(bucket1);					\
}

#define lookup1_stage1_dosig(mbuf1, bucket1, f)			\
{								\
	uint64_t *key;						\
	uint64_t signature;					\
	uint32_t bucket_index;					\
	uint64_t hash_key_buffer;				\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf1, f->key_offset);\
	hash_key_buffer = *key & f->key_mask;			\
	signature = f->f_hash(&hash_key_buffer,			\
		RTE_TABLE_HASH_KEY_SIZE, f->seed);		\
	bucket_index = signature & (f->n_buckets - 1);		\
	bucket1 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket_index * f->bucket_size];	\
	rte_prefetch0(bucket1);					\
}

#define lookup1_stage2_lru(pkt2_index, mbuf2, bucket2,		\
	pkts_mask_out, entries, f)				\
{								\
	void *a;						\
	uint64_t pkt_mask;					\
	uint64_t *key;						\
	uint32_t pos;						\
	uint64_t hash_key_buffer;				\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf2, f->key_offset);\
	hash_key_buffer = key[0] & f->key_mask;			\
								\
	lookup_key8_cmp((&hash_key_buffer), bucket2, pos);	\
								\
	pkt_mask = ((bucket2->signature >> pos) & 1LLU) << pkt2_index;\
	pkts_mask_out |= pkt_mask;				\
								\
	a = (void *) &bucket2->data[pos * f->entry_size];	\
	rte_prefetch0(a);					\
	entries[pkt2_index] = a;				\
	lru_update(bucket2, pos);				\
}

#define lookup1_stage2_ext(pkt2_index, mbuf2, bucket2, pkts_mask_out,\
	entries, buckets_mask, buckets, keys, f)		\
{								\
	struct rte_bucket_4_8 *bucket_next;			\
	void *a;						\
	uint64_t pkt_mask, bucket_mask;				\
	uint64_t *key;						\
	uint32_t pos;						\
	uint64_t hash_key_buffer;				\
								\
	key = RTE_MBUF_METADATA_UINT64_PTR(mbuf2, f->key_offset);\
	hash_key_buffer = *key & f->key_mask;			\
								\
	lookup_key8_cmp((&hash_key_buffer), bucket2, pos);	\
								\
	pkt_mask = ((bucket2->signature >> pos) & 1LLU) << pkt2_index;\
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
	struct rte_bucket_4_8 *bucket, *bucket_next;		\
	void *a;						\
	uint64_t pkt_mask, bucket_mask;				\
	uint64_t *key;						\
	uint32_t pos;						\
	uint64_t hash_key_buffer;				\
								\
	bucket = buckets[pkt_index];				\
	key = keys[pkt_index];					\
	hash_key_buffer = (*key) & f->key_mask;			\
								\
	lookup_key8_cmp((&hash_key_buffer), bucket, pos);	\
								\
	pkt_mask = ((bucket->signature >> pos) & 1LLU) << pkt_index;\
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
	buckets[pkt_index] = bucket_next;			\
	keys[pkt_index] = key;					\
}

#define lookup2_stage0(pkt00_index, pkt01_index, mbuf00, mbuf01,\
	pkts, pkts_mask, f)					\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	uint32_t key_offset = f->key_offset;		\
								\
	pkt00_index = __builtin_ctzll(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
								\
	mbuf00 = pkts[pkt00_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));\
								\
	pkt01_index = __builtin_ctzll(pkts_mask);		\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
								\
	mbuf01 = pkts[pkt01_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));\
}

#define lookup2_stage0_with_odd_support(pkt00_index, pkt01_index,\
	mbuf00, mbuf01, pkts, pkts_mask, f)			\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	uint32_t key_offset = f->key_offset;		\
								\
	pkt00_index = __builtin_ctzll(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
								\
	mbuf00 = pkts[pkt00_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, key_offset));\
								\
	pkt01_index = __builtin_ctzll(pkts_mask);		\
	if (pkts_mask == 0)					\
		pkt01_index = pkt00_index;			\
								\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
								\
	mbuf01 = pkts[pkt01_index];				\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, key_offset));\
}

#define lookup2_stage1(mbuf10, mbuf11, bucket10, bucket11, f)	\
{								\
	uint64_t signature10, signature11;			\
	uint32_t bucket10_index, bucket11_index;		\
								\
	signature10 = RTE_MBUF_METADATA_UINT32(mbuf10, f->signature_offset);\
	bucket10_index = signature10 & (f->n_buckets - 1);	\
	bucket10 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket10_index * f->bucket_size];	\
	rte_prefetch0(bucket10);				\
								\
	signature11 = RTE_MBUF_METADATA_UINT32(mbuf11, f->signature_offset);\
	bucket11_index = signature11 & (f->n_buckets - 1);	\
	bucket11 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket11_index * f->bucket_size];	\
	rte_prefetch0(bucket11);				\
}

#define lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f)\
{								\
	uint64_t *key10, *key11;				\
	uint64_t hash_offset_buffer10;				\
	uint64_t hash_offset_buffer11;				\
	uint64_t signature10, signature11;			\
	uint32_t bucket10_index, bucket11_index;		\
	rte_table_hash_op_hash f_hash = f->f_hash;		\
	uint64_t seed = f->seed;				\
	uint32_t key_offset = f->key_offset;			\
								\
	key10 = RTE_MBUF_METADATA_UINT64_PTR(mbuf10, key_offset);\
	key11 = RTE_MBUF_METADATA_UINT64_PTR(mbuf11, key_offset);\
	hash_offset_buffer10 = *key10 & f->key_mask;		\
	hash_offset_buffer11 = *key11 & f->key_mask;		\
								\
	signature10 = f_hash(&hash_offset_buffer10,		\
		RTE_TABLE_HASH_KEY_SIZE, seed);			\
	bucket10_index = signature10 & (f->n_buckets - 1);	\
	bucket10 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket10_index * f->bucket_size];	\
	rte_prefetch0(bucket10);				\
								\
	signature11 = f_hash(&hash_offset_buffer11,		\
		RTE_TABLE_HASH_KEY_SIZE, seed);			\
	bucket11_index = signature11 & (f->n_buckets - 1);	\
	bucket11 = (struct rte_bucket_4_8 *)			\
		&f->memory[bucket11_index * f->bucket_size];	\
	rte_prefetch0(bucket11);				\
}

#define lookup2_stage2_lru(pkt20_index, pkt21_index, mbuf20, mbuf21,\
	bucket20, bucket21, pkts_mask_out, entries, f)		\
{								\
	void *a20, *a21;					\
	uint64_t pkt20_mask, pkt21_mask;			\
	uint64_t *key20, *key21;				\
	uint64_t hash_offset_buffer20;				\
	uint64_t hash_offset_buffer21;				\
	uint32_t pos20, pos21;					\
								\
	key20 = RTE_MBUF_METADATA_UINT64_PTR(mbuf20, f->key_offset);\
	key21 = RTE_MBUF_METADATA_UINT64_PTR(mbuf21, f->key_offset);\
	hash_offset_buffer20 = *key20 & f->key_mask;		\
	hash_offset_buffer21 = *key21 & f->key_mask;		\
								\
	lookup_key8_cmp((&hash_offset_buffer20), bucket20, pos20);\
	lookup_key8_cmp((&hash_offset_buffer21), bucket21, pos21);\
								\
	pkt20_mask = ((bucket20->signature >> pos20) & 1LLU) << pkt20_index;\
	pkt21_mask = ((bucket21->signature >> pos21) & 1LLU) << pkt21_index;\
	pkts_mask_out |= pkt20_mask | pkt21_mask;		\
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
	bucket21, pkts_mask_out, entries, buckets_mask, buckets, keys, f)\
{								\
	struct rte_bucket_4_8 *bucket20_next, *bucket21_next;	\
	void *a20, *a21;					\
	uint64_t pkt20_mask, pkt21_mask, bucket20_mask, bucket21_mask;\
	uint64_t *key20, *key21;				\
	uint64_t hash_offset_buffer20;				\
	uint64_t hash_offset_buffer21;				\
	uint32_t pos20, pos21;					\
								\
	key20 = RTE_MBUF_METADATA_UINT64_PTR(mbuf20, f->key_offset);\
	key21 = RTE_MBUF_METADATA_UINT64_PTR(mbuf21, f->key_offset);\
	hash_offset_buffer20 = *key20 & f->key_mask;		\
	hash_offset_buffer21 = *key21 & f->key_mask;		\
								\
	lookup_key8_cmp((&hash_offset_buffer20), bucket20, pos20);\
	lookup_key8_cmp((&hash_offset_buffer21), bucket21, pos21);\
								\
	pkt20_mask = ((bucket20->signature >> pos20) & 1LLU) << pkt20_index;\
	pkt21_mask = ((bucket21->signature >> pos21) & 1LLU) << pkt21_index;\
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
rte_table_hash_lookup_key8_lru(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index,
			pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0;

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (__builtin_popcountll(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_8 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1(mbuf, bucket, f);
			lookup1_stage2_lru(pkt_index, mbuf, bucket,
					pkts_mask_out, entries, f);
		}

		*lookup_hit_mask = pkts_mask_out;
		RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
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
	RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return 0;
} /* rte_table_hash_lookup_key8_lru() */

static int
rte_table_hash_lookup_key8_lru_dosig(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index;
	uint32_t pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0;

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (__builtin_popcountll(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_8 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1_dosig(mbuf, bucket, f);
			lookup1_stage2_lru(pkt_index, mbuf, bucket,
				pkts_mask_out, entries, f);
		}

		*lookup_hit_mask = pkts_mask_out;
		RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
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
	lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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
		lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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
	lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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
	RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return 0;
} /* rte_table_hash_lookup_key8_lru_dosig() */

static int
rte_table_hash_lookup_key8_ext(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index;
	uint32_t pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0, buckets_mask = 0;
	struct rte_bucket_4_8 *buckets[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t *keys[RTE_PORT_IN_BURST_SIZE_MAX];

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (__builtin_popcountll(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_8 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1(mbuf, bucket, f);
			lookup1_stage2_ext(pkt_index, mbuf, bucket,
				pkts_mask_out, entries, buckets_mask, buckets,
				keys, f);
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

			pkt_index = __builtin_ctzll(buckets_mask);
			pkt_mask = 1LLU << pkt_index;
			buckets_mask &= ~pkt_mask;

			lookup_grinder(pkt_index, buckets, keys, pkts_mask_out,
				entries, buckets_mask_next, f);
		}

		buckets_mask = buckets_mask_next;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return 0;
} /* rte_table_hash_lookup_key8_ext() */

static int
rte_table_hash_lookup_key8_ext_dosig(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *f = (struct rte_table_hash *) table;
	struct rte_bucket_4_8 *bucket10, *bucket11, *bucket20, *bucket21;
	struct rte_mbuf *mbuf00, *mbuf01, *mbuf10, *mbuf11, *mbuf20, *mbuf21;
	uint32_t pkt00_index, pkt01_index, pkt10_index;
	uint32_t pkt11_index, pkt20_index, pkt21_index;
	uint64_t pkts_mask_out = 0, buckets_mask = 0;
	struct rte_bucket_4_8 *buckets[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t *keys[RTE_PORT_IN_BURST_SIZE_MAX];

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_KEY8_STATS_PKTS_IN_ADD(f, n_pkts_in);

	/* Cannot run the pipeline with less than 5 packets */
	if (__builtin_popcountll(pkts_mask) < 5) {
		for ( ; pkts_mask; ) {
			struct rte_bucket_4_8 *bucket;
			struct rte_mbuf *mbuf;
			uint32_t pkt_index;

			lookup1_stage0(pkt_index, mbuf, pkts, pkts_mask, f);
			lookup1_stage1_dosig(mbuf, bucket, f);
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
	lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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
		lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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
	lookup2_stage1_dosig(mbuf10, mbuf11, bucket10, bucket11, f);

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

			pkt_index = __builtin_ctzll(buckets_mask);
			pkt_mask = 1LLU << pkt_index;
			buckets_mask &= ~pkt_mask;

			lookup_grinder(pkt_index, buckets, keys, pkts_mask_out,
				entries, buckets_mask_next, f);
		}

		buckets_mask = buckets_mask_next;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_KEY8_STATS_PKTS_LOOKUP_MISS(f, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return 0;
} /* rte_table_hash_lookup_key8_dosig_ext() */

static int
rte_table_hash_key8_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_hash_key8_lru_ops = {
	.f_create = rte_table_hash_create_key8_lru,
	.f_free = rte_table_hash_free_key8_lru,
	.f_add = rte_table_hash_entry_add_key8_lru,
	.f_delete = rte_table_hash_entry_delete_key8_lru,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key8_lru,
	.f_stats = rte_table_hash_key8_stats_read,
};

struct rte_table_ops rte_table_hash_key8_lru_dosig_ops = {
	.f_create = rte_table_hash_create_key8_lru,
	.f_free = rte_table_hash_free_key8_lru,
	.f_add = rte_table_hash_entry_add_key8_lru,
	.f_delete = rte_table_hash_entry_delete_key8_lru,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key8_lru_dosig,
	.f_stats = rte_table_hash_key8_stats_read,
};

struct rte_table_ops rte_table_hash_key8_ext_ops = {
	.f_create = rte_table_hash_create_key8_ext,
	.f_free = rte_table_hash_free_key8_ext,
	.f_add = rte_table_hash_entry_add_key8_ext,
	.f_delete = rte_table_hash_entry_delete_key8_ext,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key8_ext,
	.f_stats = rte_table_hash_key8_stats_read,
};

struct rte_table_ops rte_table_hash_key8_ext_dosig_ops = {
	.f_create = rte_table_hash_create_key8_ext,
	.f_free = rte_table_hash_free_key8_ext,
	.f_add = rte_table_hash_entry_add_key8_ext,
	.f_delete = rte_table_hash_entry_delete_key8_ext,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_lookup_key8_ext_dosig,
	.f_stats = rte_table_hash_key8_stats_read,
};
