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

#define KEYS_PER_BUCKET	4

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_HASH_LRU_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_HASH_LRU_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_HASH_LRU_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_HASH_LRU_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct bucket {
	union {
		struct bucket *next;
		uint64_t lru_list;
	};
	uint16_t sig[KEYS_PER_BUCKET];
	uint32_t key_pos[KEYS_PER_BUCKET];
};

struct grinder {
	struct bucket *bkt;
	uint64_t sig;
	uint64_t match;
	uint64_t match_pos;
	uint32_t key_index;
};

struct rte_table_hash {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t n_keys;
	uint32_t n_buckets;
	rte_table_hash_op_hash f_hash;
	uint64_t seed;
	uint32_t signature_offset;
	uint32_t key_offset;

	/* Internal */
	uint64_t bucket_mask;
	uint32_t key_size_shl;
	uint32_t data_size_shl;
	uint32_t key_stack_tos;

	/* Grinder */
	struct grinder grinders[RTE_PORT_IN_BURST_SIZE_MAX];

	/* Tables */
	struct bucket *buckets;
	uint8_t *key_mem;
	uint8_t *data_mem;
	uint32_t *key_stack;

	/* Table memory */
	uint8_t memory[0] __rte_cache_aligned;
};

static int
check_params_create(struct rte_table_hash_lru_params *params)
{
	uint32_t n_buckets_min;

	/* key_size */
	if ((params->key_size == 0) ||
		(!rte_is_power_of_2(params->key_size))) {
		RTE_LOG(ERR, TABLE, "%s: key_size invalid value\n", __func__);
		return -EINVAL;
	}

	/* n_keys */
	if ((params->n_keys == 0) ||
		(!rte_is_power_of_2(params->n_keys))) {
		RTE_LOG(ERR, TABLE, "%s: n_keys invalid value\n", __func__);
		return -EINVAL;
	}

	/* n_buckets */
	n_buckets_min = (params->n_keys + KEYS_PER_BUCKET - 1) / params->n_keys;
	if ((params->n_buckets == 0) ||
		(!rte_is_power_of_2(params->n_keys)) ||
		(params->n_buckets < n_buckets_min)) {
		RTE_LOG(ERR, TABLE, "%s: n_buckets invalid value\n", __func__);
		return -EINVAL;
	}

	/* f_hash */
	if (params->f_hash == NULL) {
		RTE_LOG(ERR, TABLE, "%s: f_hash invalid value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_lru_create(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_hash_lru_params *p =
		(struct rte_table_hash_lru_params *) params;
	struct rte_table_hash *t;
	uint32_t total_size, table_meta_sz;
	uint32_t bucket_sz, key_sz, key_stack_sz, data_sz;
	uint32_t bucket_offset, key_offset, key_stack_offset, data_offset;
	uint32_t i;

	/* Check input parameters */
	if ((check_params_create(p) != 0) ||
		(!rte_is_power_of_2(entry_size)) ||
		((sizeof(struct rte_table_hash) % RTE_CACHE_LINE_SIZE) != 0) ||
		(sizeof(struct bucket) != (RTE_CACHE_LINE_SIZE / 2))) {
		return NULL;
	}

	/* Memory allocation */
	table_meta_sz = RTE_CACHE_LINE_ROUNDUP(sizeof(struct rte_table_hash));
	bucket_sz = RTE_CACHE_LINE_ROUNDUP(p->n_buckets * sizeof(struct bucket));
	key_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * p->key_size);
	key_stack_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * sizeof(uint32_t));
	data_sz = RTE_CACHE_LINE_ROUNDUP(p->n_keys * entry_size);
	total_size = table_meta_sz + bucket_sz + key_sz + key_stack_sz +
		data_sz;

	t = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE, socket_id);
	if (t == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for hash table\n",
			__func__, total_size);
		return NULL;
	}
	RTE_LOG(INFO, TABLE, "%s (%u-byte key): Hash table memory footprint is "
		"%u bytes\n", __func__, p->key_size, total_size);

	/* Memory initialization */
	t->key_size = p->key_size;
	t->entry_size = entry_size;
	t->n_keys = p->n_keys;
	t->n_buckets = p->n_buckets;
	t->f_hash = p->f_hash;
	t->seed = p->seed;
	t->signature_offset = p->signature_offset;
	t->key_offset = p->key_offset;

	/* Internal */
	t->bucket_mask = t->n_buckets - 1;
	t->key_size_shl = __builtin_ctzl(p->key_size);
	t->data_size_shl = __builtin_ctzl(entry_size);

	/* Tables */
	bucket_offset = 0;
	key_offset = bucket_offset + bucket_sz;
	key_stack_offset = key_offset + key_sz;
	data_offset = key_stack_offset + key_stack_sz;

	t->buckets = (struct bucket *) &t->memory[bucket_offset];
	t->key_mem = &t->memory[key_offset];
	t->key_stack = (uint32_t *) &t->memory[key_stack_offset];
	t->data_mem = &t->memory[data_offset];

	/* Key stack */
	for (i = 0; i < t->n_keys; i++)
		t->key_stack[i] = t->n_keys - 1 - i;
	t->key_stack_tos = t->n_keys;

	/* LRU */
	for (i = 0; i < t->n_buckets; i++) {
		struct bucket *bkt = &t->buckets[i];

		lru_init(bkt);
	}

	return t;
}

static int
rte_table_hash_lru_free(void *table)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;

	/* Check input parameters */
	if (t == NULL)
		return -EINVAL;

	rte_free(t);
	return 0;
}

static int
rte_table_hash_lru_entry_add(void *table, void *key, void *entry,
	int *key_found, void **entry_ptr)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;
	struct bucket *bkt;
	uint64_t sig;
	uint32_t bkt_index, i;

	sig = t->f_hash(key, t->key_size, t->seed);
	bkt_index = sig & t->bucket_mask;
	bkt = &t->buckets[bkt_index];
	sig = (sig >> 16) | 1LLU;

	/* Key is present in the bucket */
	for (i = 0; i < KEYS_PER_BUCKET; i++) {
		uint64_t bkt_sig = (uint64_t) bkt->sig[i];
		uint32_t bkt_key_index = bkt->key_pos[i];
		uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
			t->key_size_shl];

		if ((sig == bkt_sig) && (memcmp(key, bkt_key, t->key_size)
			== 0)) {
			uint8_t *data = &t->data_mem[bkt_key_index <<
				t->data_size_shl];

			memcpy(data, entry, t->entry_size);
			lru_update(bkt, i);
			*key_found = 1;
			*entry_ptr = (void *) data;
			return 0;
		}
	}

	/* Key is not present in the bucket */
	for (i = 0; i < KEYS_PER_BUCKET; i++) {
		uint64_t bkt_sig = (uint64_t) bkt->sig[i];

		if (bkt_sig == 0) {
			uint32_t bkt_key_index;
			uint8_t *bkt_key, *data;

			/* Allocate new key */
			if (t->key_stack_tos == 0) {
				/* No keys available */
				return -ENOSPC;
			}
			bkt_key_index = t->key_stack[--t->key_stack_tos];

			/* Install new key */
			bkt_key = &t->key_mem[bkt_key_index << t->key_size_shl];
			data = &t->data_mem[bkt_key_index << t->data_size_shl];

			bkt->sig[i] = (uint16_t) sig;
			bkt->key_pos[i] = bkt_key_index;
			memcpy(bkt_key, key, t->key_size);
			memcpy(data, entry, t->entry_size);
			lru_update(bkt, i);

			*key_found = 0;
			*entry_ptr = (void *) data;
			return 0;
		}
	}

	/* Bucket full */
	{
		uint64_t pos = lru_pos(bkt);
		uint32_t bkt_key_index = bkt->key_pos[pos];
		uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
			t->key_size_shl];
		uint8_t *data = &t->data_mem[bkt_key_index << t->data_size_shl];

		bkt->sig[pos] = (uint16_t) sig;
		memcpy(bkt_key, key, t->key_size);
		memcpy(data, entry, t->entry_size);
		lru_update(bkt, pos);

		*key_found = 0;
		*entry_ptr = (void *) data;
		return 0;
	}
}

static int
rte_table_hash_lru_entry_delete(void *table, void *key, int *key_found,
	void *entry)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;
	struct bucket *bkt;
	uint64_t sig;
	uint32_t bkt_index, i;

	sig = t->f_hash(key, t->key_size, t->seed);
	bkt_index = sig & t->bucket_mask;
	bkt = &t->buckets[bkt_index];
	sig = (sig >> 16) | 1LLU;

	/* Key is present in the bucket */
	for (i = 0; i < KEYS_PER_BUCKET; i++) {
		uint64_t bkt_sig = (uint64_t) bkt->sig[i];
		uint32_t bkt_key_index = bkt->key_pos[i];
		uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
			t->key_size_shl];

		if ((sig == bkt_sig) &&
			(memcmp(key, bkt_key, t->key_size) == 0)) {
			uint8_t *data = &t->data_mem[bkt_key_index <<
				t->data_size_shl];

			bkt->sig[i] = 0;
			t->key_stack[t->key_stack_tos++] = bkt_key_index;
			*key_found = 1;
			memcpy(entry, data, t->entry_size);
			return 0;
		}
	}

	/* Key is not present in the bucket */
	*key_found = 0;
	return 0;
}

static int rte_table_hash_lru_lookup_unoptimized(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries,
	int dosig)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;
	uint64_t pkts_mask_out = 0;

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_LRU_STATS_PKTS_IN_ADD(t, n_pkts_in);

	for ( ; pkts_mask; ) {
		struct bucket *bkt;
		struct rte_mbuf *pkt;
		uint8_t *key;
		uint64_t pkt_mask, sig;
		uint32_t pkt_index, bkt_index, i;

		pkt_index = __builtin_ctzll(pkts_mask);
		pkt_mask = 1LLU << pkt_index;
		pkts_mask &= ~pkt_mask;

		pkt = pkts[pkt_index];
		key = RTE_MBUF_METADATA_UINT8_PTR(pkt, t->key_offset);
		if (dosig)
			sig = (uint64_t) t->f_hash(key, t->key_size, t->seed);
		else
			sig = RTE_MBUF_METADATA_UINT32(pkt,
				t->signature_offset);

		bkt_index = sig & t->bucket_mask;
		bkt = &t->buckets[bkt_index];
		sig = (sig >> 16) | 1LLU;

		/* Key is present in the bucket */
		for (i = 0; i < KEYS_PER_BUCKET; i++) {
			uint64_t bkt_sig = (uint64_t) bkt->sig[i];
			uint32_t bkt_key_index = bkt->key_pos[i];
			uint8_t *bkt_key = &t->key_mem[bkt_key_index <<
				t->key_size_shl];

			if ((sig == bkt_sig) && (memcmp(key, bkt_key,
				t->key_size) == 0)) {
				uint8_t *data = &t->data_mem[bkt_key_index <<
					t->data_size_shl];

				lru_update(bkt, i);
				pkts_mask_out |= pkt_mask;
				entries[pkt_index] = (void *) data;
				break;
			}
		}
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_LRU_STATS_PKTS_LOOKUP_MISS(t, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return 0;
}

/***
*
* mask = match bitmask
* match = at least one match
* match_many = more than one match
* match_pos = position of first match
*
* ----------------------------------------
* mask		 match	 match_many	  match_pos
* ----------------------------------------
* 0000		 0		 0			  00
* 0001		 1		 0			  00
* 0010		 1		 0			  01
* 0011		 1		 1			  00
* ----------------------------------------
* 0100		 1		 0			  10
* 0101		 1		 1			  00
* 0110		 1		 1			  01
* 0111		 1		 1			  00
* ----------------------------------------
* 1000		 1		 0			  11
* 1001		 1		 1			  00
* 1010		 1		 1			  01
* 1011		 1		 1			  00
* ----------------------------------------
* 1100		 1		 1			  10
* 1101		 1		 1			  00
* 1110		 1		 1			  01
* 1111		 1		 1			  00
* ----------------------------------------
*
* match = 1111_1111_1111_1110
* match_many = 1111_1110_1110_1000
* match_pos = 0001_0010_0001_0011__0001_0010_0001_0000
*
* match = 0xFFFELLU
* match_many = 0xFEE8LLU
* match_pos = 0x12131210LLU
*
***/

#define LUT_MATCH						0xFFFELLU
#define LUT_MATCH_MANY						0xFEE8LLU
#define LUT_MATCH_POS						0x12131210LLU

#define lookup_cmp_sig(mbuf_sig, bucket, match, match_many, match_pos)\
{								\
	uint64_t bucket_sig[4], mask[4], mask_all;		\
								\
	bucket_sig[0] = bucket->sig[0];				\
	bucket_sig[1] = bucket->sig[1];				\
	bucket_sig[2] = bucket->sig[2];				\
	bucket_sig[3] = bucket->sig[3];				\
								\
	bucket_sig[0] ^= mbuf_sig;				\
	bucket_sig[1] ^= mbuf_sig;				\
	bucket_sig[2] ^= mbuf_sig;				\
	bucket_sig[3] ^= mbuf_sig;				\
								\
	mask[0] = 0;						\
	mask[1] = 0;						\
	mask[2] = 0;						\
	mask[3] = 0;						\
								\
	if (bucket_sig[0] == 0)					\
		mask[0] = 1;					\
	if (bucket_sig[1] == 0)					\
		mask[1] = 2;					\
	if (bucket_sig[2] == 0)					\
		mask[2] = 4;					\
	if (bucket_sig[3] == 0)					\
		mask[3] = 8;					\
								\
	mask_all = (mask[0] | mask[1]) | (mask[2] | mask[3]);	\
								\
	match = (LUT_MATCH >> mask_all) & 1;			\
	match_many = (LUT_MATCH_MANY >> mask_all) & 1;		\
	match_pos = (LUT_MATCH_POS >> (mask_all << 1)) & 3;	\
}

#define lookup_cmp_key(mbuf, key, match_key, f)			\
{								\
	uint64_t *pkt_key = RTE_MBUF_METADATA_UINT64_PTR(mbuf, f->key_offset);\
	uint64_t *bkt_key = (uint64_t *) key;			\
								\
	switch (f->key_size) {					\
	case 8:							\
	{							\
		uint64_t xor = pkt_key[0] ^ bkt_key[0];		\
		match_key = 0;					\
		if (xor == 0)					\
			match_key = 1;				\
	}							\
	break;							\
								\
	case 16:						\
	{							\
		uint64_t xor[2], or;				\
								\
		xor[0] = pkt_key[0] ^ bkt_key[0];		\
		xor[1] = pkt_key[1] ^ bkt_key[1];		\
		or = xor[0] | xor[1];				\
		match_key = 0;					\
		if (or == 0)					\
			match_key = 1;				\
	}							\
	break;							\
								\
	case 32:						\
	{							\
		uint64_t xor[4], or;				\
								\
		xor[0] = pkt_key[0] ^ bkt_key[0];		\
		xor[1] = pkt_key[1] ^ bkt_key[1];		\
		xor[2] = pkt_key[2] ^ bkt_key[2];		\
		xor[3] = pkt_key[3] ^ bkt_key[3];		\
		or = xor[0] | xor[1] | xor[2] | xor[3];		\
		match_key = 0;					\
		if (or == 0)					\
			match_key = 1;				\
	}							\
	break;							\
								\
	case 64:						\
	{							\
		uint64_t xor[8], or;				\
								\
		xor[0] = pkt_key[0] ^ bkt_key[0];		\
		xor[1] = pkt_key[1] ^ bkt_key[1];		\
		xor[2] = pkt_key[2] ^ bkt_key[2];		\
		xor[3] = pkt_key[3] ^ bkt_key[3];		\
		xor[4] = pkt_key[4] ^ bkt_key[4];		\
		xor[5] = pkt_key[5] ^ bkt_key[5];		\
		xor[6] = pkt_key[6] ^ bkt_key[6];		\
		xor[7] = pkt_key[7] ^ bkt_key[7];		\
		or = xor[0] | xor[1] | xor[2] | xor[3] |	\
			xor[4] | xor[5] | xor[6] | xor[7];	\
		match_key = 0;					\
		if (or == 0)					\
			match_key = 1;				\
	}							\
	break;							\
								\
	default:						\
		match_key = 0;					\
		if (memcmp(pkt_key, bkt_key, f->key_size) == 0)	\
			match_key = 1;				\
	}							\
}

#define lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index)\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	struct rte_mbuf *mbuf00, *mbuf01;			\
								\
	pkt00_index = __builtin_ctzll(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
	mbuf00 = pkts[pkt00_index];				\
								\
	pkt01_index = __builtin_ctzll(pkts_mask);		\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
	mbuf01 = pkts[pkt01_index];				\
								\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, 0));	\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, 0));	\
}

#define lookup2_stage0_with_odd_support(t, g, pkts, pkts_mask, pkt00_index, \
	pkt01_index)						\
{								\
	uint64_t pkt00_mask, pkt01_mask;			\
	struct rte_mbuf *mbuf00, *mbuf01;			\
								\
	pkt00_index = __builtin_ctzll(pkts_mask);		\
	pkt00_mask = 1LLU << pkt00_index;			\
	pkts_mask &= ~pkt00_mask;				\
	mbuf00 = pkts[pkt00_index];				\
								\
	pkt01_index = __builtin_ctzll(pkts_mask);		\
	if (pkts_mask == 0)					\
		pkt01_index = pkt00_index;			\
								\
	pkt01_mask = 1LLU << pkt01_index;			\
	pkts_mask &= ~pkt01_mask;				\
	mbuf01 = pkts[pkt01_index];				\
								\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf00, 0));	\
	rte_prefetch0(RTE_MBUF_METADATA_UINT8_PTR(mbuf01, 0));	\
}

#define lookup2_stage1(t, g, pkts, pkt10_index, pkt11_index)	\
{								\
	struct grinder *g10, *g11;				\
	uint64_t sig10, sig11, bkt10_index, bkt11_index;	\
	struct rte_mbuf *mbuf10, *mbuf11;			\
	struct bucket *bkt10, *bkt11, *buckets = t->buckets;	\
	uint64_t bucket_mask = t->bucket_mask;			\
	uint32_t signature_offset = t->signature_offset;	\
								\
	mbuf10 = pkts[pkt10_index];				\
	sig10 = (uint64_t) RTE_MBUF_METADATA_UINT32(mbuf10, signature_offset);\
	bkt10_index = sig10 & bucket_mask;			\
	bkt10 = &buckets[bkt10_index];				\
								\
	mbuf11 = pkts[pkt11_index];				\
	sig11 = (uint64_t) RTE_MBUF_METADATA_UINT32(mbuf11, signature_offset);\
	bkt11_index = sig11 & bucket_mask;			\
	bkt11 = &buckets[bkt11_index];				\
								\
	rte_prefetch0(bkt10);					\
	rte_prefetch0(bkt11);					\
								\
	g10 = &g[pkt10_index];					\
	g10->sig = sig10;					\
	g10->bkt = bkt10;					\
								\
	g11 = &g[pkt11_index];					\
	g11->sig = sig11;					\
	g11->bkt = bkt11;					\
}

#define lookup2_stage1_dosig(t, g, pkts, pkt10_index, pkt11_index)\
{								\
	struct grinder *g10, *g11;				\
	uint64_t sig10, sig11, bkt10_index, bkt11_index;	\
	struct rte_mbuf *mbuf10, *mbuf11;			\
	struct bucket *bkt10, *bkt11, *buckets = t->buckets;	\
	uint8_t *key10, *key11;					\
	uint64_t bucket_mask = t->bucket_mask;			\
	rte_table_hash_op_hash f_hash = t->f_hash;		\
	uint64_t seed = t->seed;				\
	uint32_t key_size = t->key_size;			\
	uint32_t key_offset = t->key_offset;			\
								\
	mbuf10 = pkts[pkt10_index];				\
	key10 = RTE_MBUF_METADATA_UINT8_PTR(mbuf10, key_offset);\
	sig10 = (uint64_t) f_hash(key10, key_size, seed);	\
	bkt10_index = sig10 & bucket_mask;			\
	bkt10 = &buckets[bkt10_index];				\
								\
	mbuf11 = pkts[pkt11_index];				\
	key11 = RTE_MBUF_METADATA_UINT8_PTR(mbuf11, key_offset);\
	sig11 = (uint64_t) f_hash(key11, key_size, seed);	\
	bkt11_index = sig11 & bucket_mask;			\
	bkt11 = &buckets[bkt11_index];				\
								\
	rte_prefetch0(bkt10);					\
	rte_prefetch0(bkt11);					\
								\
	g10 = &g[pkt10_index];					\
	g10->sig = sig10;					\
	g10->bkt = bkt10;					\
								\
	g11 = &g[pkt11_index];					\
	g11->sig = sig11;					\
	g11->bkt = bkt11;					\
}

#define lookup2_stage2(t, g, pkt20_index, pkt21_index, pkts_mask_match_many)\
{								\
	struct grinder *g20, *g21;				\
	uint64_t sig20, sig21;					\
	struct bucket *bkt20, *bkt21;				\
	uint8_t *key20, *key21, *key_mem = t->key_mem;		\
	uint64_t match20, match21, match_many20, match_many21;	\
	uint64_t match_pos20, match_pos21;			\
	uint32_t key20_index, key21_index, key_size_shl = t->key_size_shl;\
								\
	g20 = &g[pkt20_index];					\
	sig20 = g20->sig;					\
	bkt20 = g20->bkt;					\
	sig20 = (sig20 >> 16) | 1LLU;				\
	lookup_cmp_sig(sig20, bkt20, match20, match_many20, match_pos20);\
	match20 <<= pkt20_index;				\
	match_many20 <<= pkt20_index;				\
	key20_index = bkt20->key_pos[match_pos20];		\
	key20 = &key_mem[key20_index << key_size_shl];		\
								\
	g21 = &g[pkt21_index];					\
	sig21 = g21->sig;					\
	bkt21 = g21->bkt;					\
	sig21 = (sig21 >> 16) | 1LLU;				\
	lookup_cmp_sig(sig21, bkt21, match21, match_many21, match_pos21);\
	match21 <<= pkt21_index;				\
	match_many21 <<= pkt21_index;				\
	key21_index = bkt21->key_pos[match_pos21];		\
	key21 = &key_mem[key21_index << key_size_shl];		\
								\
	rte_prefetch0(key20);					\
	rte_prefetch0(key21);					\
								\
	pkts_mask_match_many |= match_many20 | match_many21;	\
								\
	g20->match = match20;					\
	g20->match_pos = match_pos20;				\
	g20->key_index = key20_index;				\
								\
	g21->match = match21;					\
	g21->match_pos = match_pos21;				\
	g21->key_index = key21_index;				\
}

#define lookup2_stage3(t, g, pkts, pkt30_index, pkt31_index, pkts_mask_out, \
	entries)						\
{								\
	struct grinder *g30, *g31;				\
	struct rte_mbuf *mbuf30, *mbuf31;			\
	struct bucket *bkt30, *bkt31;				\
	uint8_t *key30, *key31, *key_mem = t->key_mem;		\
	uint8_t *data30, *data31, *data_mem = t->data_mem;	\
	uint64_t match30, match31, match_pos30, match_pos31;	\
	uint64_t match_key30, match_key31, match_keys;		\
	uint32_t key30_index, key31_index;			\
	uint32_t key_size_shl = t->key_size_shl;		\
	uint32_t data_size_shl = t->data_size_shl;		\
								\
	mbuf30 = pkts[pkt30_index];				\
	g30 = &g[pkt30_index];					\
	bkt30 = g30->bkt;					\
	match30 = g30->match;					\
	match_pos30 = g30->match_pos;				\
	key30_index = g30->key_index;				\
	key30 = &key_mem[key30_index << key_size_shl];		\
	lookup_cmp_key(mbuf30, key30, match_key30, t);		\
	match_key30 <<= pkt30_index;				\
	match_key30 &= match30;					\
	data30 = &data_mem[key30_index << data_size_shl];	\
	entries[pkt30_index] = data30;				\
								\
	mbuf31 = pkts[pkt31_index];				\
	g31 = &g[pkt31_index];					\
	bkt31 = g31->bkt;					\
	match31 = g31->match;					\
	match_pos31 = g31->match_pos;				\
	key31_index = g31->key_index;				\
	key31 = &key_mem[key31_index << key_size_shl];		\
	lookup_cmp_key(mbuf31, key31, match_key31, t);		\
	match_key31 <<= pkt31_index;				\
	match_key31 &= match31;					\
	data31 = &data_mem[key31_index << data_size_shl];	\
	entries[pkt31_index] = data31;				\
								\
	rte_prefetch0(data30);					\
	rte_prefetch0(data31);					\
								\
	match_keys = match_key30 | match_key31;			\
	pkts_mask_out |= match_keys;				\
								\
	if (match_key30 == 0)					\
		match_pos30 = 4;				\
	lru_update(bkt30, match_pos30);				\
								\
	if (match_key31 == 0)					\
		match_pos31 = 4;				\
	lru_update(bkt31, match_pos31);				\
}

/***
* The lookup function implements a 4-stage pipeline, with each stage processing
* two different packets. The purpose of pipelined implementation is to hide the
* latency of prefetching the data structures and loosen the data dependency
* between instructions.
*
*   p00  _______   p10  _______   p20  _______   p30  _______
* ----->|       |----->|       |----->|       |----->|       |----->
*       |   0   |      |   1   |      |   2   |      |   3   |
* ----->|_______|----->|_______|----->|_______|----->|_______|----->
*   p01            p11            p21            p31
*
* The naming convention is:
*	  pXY = packet Y of stage X, X = 0 .. 3, Y = 0 .. 1
*
***/
static int rte_table_hash_lru_lookup(
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

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_LRU_STATS_PKTS_IN_ADD(t, n_pkts_in);

	/* Cannot run the pipeline with less than 7 packets */
	if (__builtin_popcountll(pkts_mask) < 7)
		return rte_table_hash_lru_lookup_unoptimized(table, pkts,
			pkts_mask, lookup_hit_mask, entries, 0);

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

		status = rte_table_hash_lru_lookup_unoptimized(table, pkts,
			pkts_mask_match_many, &pkts_mask_out_slow, entries, 0);
		pkts_mask_out |= pkts_mask_out_slow;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_LRU_STATS_PKTS_LOOKUP_MISS(t, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return status;
}

static int rte_table_hash_lru_lookup_dosig(
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

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_HASH_LRU_STATS_PKTS_IN_ADD(t, n_pkts_in);

	/* Cannot run the pipeline with less than 7 packets */
	if (__builtin_popcountll(pkts_mask) < 7)
		return rte_table_hash_lru_lookup_unoptimized(table, pkts,
			pkts_mask, lookup_hit_mask, entries, 1);

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline feed */
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline stage 1 */
	lookup2_stage1_dosig(t, g, pkts, pkt10_index, pkt11_index);

	/* Pipeline feed */
	pkt20_index = pkt10_index;
	pkt21_index = pkt11_index;
	pkt10_index = pkt00_index;
	pkt11_index = pkt01_index;

	/* Pipeline stage 0 */
	lookup2_stage0(t, g, pkts, pkts_mask, pkt00_index, pkt01_index);

	/* Pipeline stage 1 */
	lookup2_stage1_dosig(t, g, pkts, pkt10_index, pkt11_index);

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
		lookup2_stage1_dosig(t, g, pkts, pkt10_index, pkt11_index);

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
	lookup2_stage1_dosig(t, g, pkts, pkt10_index, pkt11_index);

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

		status = rte_table_hash_lru_lookup_unoptimized(table, pkts,
			pkts_mask_match_many, &pkts_mask_out_slow, entries, 1);
		pkts_mask_out |= pkts_mask_out_slow;
	}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_LRU_STATS_PKTS_LOOKUP_MISS(t, n_pkts_in - __builtin_popcountll(pkts_mask_out));
	return status;
}

static int
rte_table_hash_lru_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_hash *t = (struct rte_table_hash *) table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_hash_lru_ops = {
	.f_create = rte_table_hash_lru_create,
	.f_free = rte_table_hash_lru_free,
	.f_add = rte_table_hash_lru_entry_add,
	.f_delete = rte_table_hash_lru_entry_delete,
	.f_lookup = rte_table_hash_lru_lookup,
	.f_stats = rte_table_hash_lru_stats_read,
};

struct rte_table_ops rte_table_hash_lru_dosig_ops = {
	.f_create = rte_table_hash_lru_create,
	.f_free = rte_table_hash_lru_free,
	.f_add = rte_table_hash_lru_entry_add,
	.f_delete = rte_table_hash_lru_entry_delete,
	.f_lookup = rte_table_hash_lru_lookup_dosig,
	.f_stats = rte_table_hash_lru_stats_read,
};
