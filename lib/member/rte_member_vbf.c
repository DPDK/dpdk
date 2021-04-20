/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <math.h>
#include <string.h>

#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "rte_member.h"
#include "rte_member_vbf.h"

/*
 * vBF currently implemented as a big array.
 * The BFs have a vertical layout. Bits in same location of all bfs will stay
 * in the same cache line.
 * For example, if we have 32 bloom filters, we use a uint32_t array to
 * represent all of them. array[0] represent the first location of all the
 * bloom filters, array[1] represents the second location of all the
 * bloom filters, etc. The advantage of this layout is to minimize the average
 * number of memory accesses to test all bloom filters.
 *
 * Currently the implementation supports vBF containing 1,2,4,8,16,32 BFs.
 */
int
rte_member_create_vbf(struct rte_member_setsum *ss,
		const struct rte_member_parameters *params)
{

	if (params->num_set > RTE_MEMBER_MAX_BF ||
			!rte_is_power_of_2(params->num_set) ||
			params->num_keys == 0 ||
			params->false_positive_rate == 0 ||
			params->false_positive_rate > 1) {
		rte_errno = EINVAL;
		RTE_MEMBER_LOG(ERR, "Membership vBF create with invalid parameters\n");
		return -EINVAL;
	}

	/* We assume expected keys evenly distribute to all BFs */
	uint32_t num_keys_per_bf = 1 + (params->num_keys - 1) / ss->num_set;

	/*
	 * Note that the false positive rate is for all BFs in the vBF
	 * such that the single BF's false positive rate needs to be
	 * calculated.
	 * Assume each BF's False positive rate is fp_one_bf. The total false
	 * positive rate is fp = 1-(1-fp_one_bf)^n.
	 * => fp_one_bf = 1 - (1-fp)^(1/n)
	 */

	float fp_one_bf = 1 - pow((1 - params->false_positive_rate),
					1.0 / ss->num_set);

	if (fp_one_bf == 0) {
		rte_errno = EINVAL;
		RTE_MEMBER_LOG(ERR, "Membership BF false positive rate is too small\n");
		return -EINVAL;
	}

	uint32_t bits = ceil((num_keys_per_bf *
				log(fp_one_bf)) /
				log(1.0 / (pow(2.0, log(2.0)))));

	/* We round to power of 2 for performance during lookup */
	ss->bits = rte_align32pow2(bits);

	ss->num_hashes = (uint32_t)(log(2.0) * bits / num_keys_per_bf);
	ss->bit_mask = ss->bits - 1;

	/*
	 * Since we round the bits to power of 2, the final false positive
	 * rate will probably not be same as the user specified. We log the
	 * new value as debug message.
	 */
	float new_fp = pow((1 - pow((1 - 1.0 / ss->bits), num_keys_per_bf *
					ss->num_hashes)), ss->num_hashes);
	new_fp = 1 - pow((1 - new_fp), ss->num_set);

	/*
	 * Reduce hash function count, until we approach the user specified
	 * false-positive rate. Otherwise it is too conservative
	 */
	int tmp_num_hash = ss->num_hashes;

	while (tmp_num_hash > 1) {
		float tmp_fp = new_fp;

		tmp_num_hash--;
		new_fp = pow((1 - pow((1 - 1.0 / ss->bits), num_keys_per_bf *
					tmp_num_hash)), tmp_num_hash);
		new_fp = 1 - pow((1 - new_fp), ss->num_set);

		if (new_fp > params->false_positive_rate) {
			new_fp = tmp_fp;
			tmp_num_hash++;
			break;
		}
	}

	ss->num_hashes = tmp_num_hash;

	/*
	 * To avoid multiplication and division:
	 * mul_shift is used for multiplication shift during bit test
	 * div_shift is used for division shift, to be divided by number of bits
	 * represented by a uint32_t variable
	 */
	ss->mul_shift = __builtin_ctzl(ss->num_set);
	ss->div_shift = __builtin_ctzl(32 >> ss->mul_shift);

	RTE_MEMBER_LOG(DEBUG, "vector bloom filter created, "
		"each bloom filter expects %u keys, needs %u bits, %u hashes, "
		"with false positive rate set as %.5f, "
		"The new calculated vBF false positive rate is %.5f\n",
		num_keys_per_bf, ss->bits, ss->num_hashes, fp_one_bf, new_fp);

	ss->table = rte_zmalloc_socket(NULL, ss->num_set * (ss->bits >> 3),
					RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (ss->table == NULL)
		return -ENOMEM;

	return 0;
}

static inline uint32_t
test_bit(uint32_t bit_loc, const struct rte_member_setsum *ss)
{
	uint32_t *vbf = ss->table;
	uint32_t n = ss->num_set;
	uint32_t div_shift = ss->div_shift;
	uint32_t mul_shift = ss->mul_shift;
	/*
	 * a is how many bits in one BF are represented by one 32bit
	 * variable.
	 */
	uint32_t a = 32 >> mul_shift;
	/*
	 * x>>b is the divide, x & (a-1) is the mod, & (1<<n-1) to mask out bits
	 * we do not need
	 */
	return (vbf[bit_loc >> div_shift] >>
			((bit_loc & (a - 1)) << mul_shift)) & ((1ULL << n) - 1);
}

static inline void
set_bit(uint32_t bit_loc, const struct rte_member_setsum *ss, int32_t set)
{
	uint32_t *vbf = ss->table;
	uint32_t div_shift = ss->div_shift;
	uint32_t mul_shift = ss->mul_shift;
	uint32_t a = 32 >> mul_shift;

	vbf[bit_loc >> div_shift] |=
			1UL << (((bit_loc & (a - 1)) << mul_shift) + set - 1);
}

int
rte_member_lookup_vbf(const struct rte_member_setsum *ss, const void *key,
		member_set_t *set_id)
{
	uint32_t j;
	uint32_t h1 = MEMBER_HASH_FUNC(key, ss->key_len, ss->prim_hash_seed);
	uint32_t h2 = MEMBER_HASH_FUNC(&h1, sizeof(uint32_t),
						ss->sec_hash_seed);
	uint32_t mask = ~0;
	uint32_t bit_loc;

	for (j = 0; j < ss->num_hashes; j++) {
		bit_loc = (h1 + j * h2) & ss->bit_mask;
		mask &= test_bit(bit_loc, ss);
	}

	if (mask) {
		*set_id = __builtin_ctzl(mask) + 1;
		return 1;
	}

	*set_id = RTE_MEMBER_NO_MATCH;
	return 0;
}

uint32_t
rte_member_lookup_bulk_vbf(const struct rte_member_setsum *ss,
		const void **keys, uint32_t num_keys, member_set_t *set_ids)
{
	uint32_t i, k;
	uint32_t num_matches = 0;
	uint32_t mask[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t h1[RTE_MEMBER_LOOKUP_BULK_MAX], h2[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t bit_loc;

	for (i = 0; i < num_keys; i++)
		h1[i] = MEMBER_HASH_FUNC(keys[i], ss->key_len,
						ss->prim_hash_seed);
	for (i = 0; i < num_keys; i++)
		h2[i] = MEMBER_HASH_FUNC(&h1[i], sizeof(uint32_t),
						ss->sec_hash_seed);
	for (i = 0; i < num_keys; i++) {
		mask[i] = ~0;
		for (k = 0; k < ss->num_hashes; k++) {
			bit_loc = (h1[i] + k * h2[i]) & ss->bit_mask;
			mask[i] &= test_bit(bit_loc, ss);
		}
	}
	for (i = 0; i < num_keys; i++) {
		if (mask[i]) {
			set_ids[i] = __builtin_ctzl(mask[i]) + 1;
			num_matches++;
		} else
			set_ids[i] = RTE_MEMBER_NO_MATCH;
	}
	return num_matches;
}

uint32_t
rte_member_lookup_multi_vbf(const struct rte_member_setsum *ss,
		const void *key, uint32_t match_per_key,
		member_set_t *set_id)
{
	uint32_t num_matches = 0;
	uint32_t j;
	uint32_t h1 = MEMBER_HASH_FUNC(key, ss->key_len, ss->prim_hash_seed);
	uint32_t h2 = MEMBER_HASH_FUNC(&h1, sizeof(uint32_t),
						ss->sec_hash_seed);
	uint32_t mask = ~0;
	uint32_t bit_loc;

	for (j = 0; j < ss->num_hashes; j++) {
		bit_loc = (h1 + j * h2) & ss->bit_mask;
		mask &= test_bit(bit_loc, ss);
	}
	while (mask) {
		uint32_t loc = __builtin_ctzl(mask);
		set_id[num_matches] = loc + 1;
		num_matches++;
		if (num_matches >= match_per_key)
			return num_matches;
		mask &= ~(1UL << loc);
	}
	return num_matches;
}

uint32_t
rte_member_lookup_multi_bulk_vbf(const struct rte_member_setsum *ss,
		const void **keys, uint32_t num_keys, uint32_t match_per_key,
		uint32_t *match_count,
		member_set_t *set_ids)
{
	uint32_t i, k;
	uint32_t num_matches = 0;
	uint32_t match_cnt_t;
	uint32_t mask[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t h1[RTE_MEMBER_LOOKUP_BULK_MAX], h2[RTE_MEMBER_LOOKUP_BULK_MAX];
	uint32_t bit_loc;

	for (i = 0; i < num_keys; i++)
		h1[i] = MEMBER_HASH_FUNC(keys[i], ss->key_len,
						ss->prim_hash_seed);
	for (i = 0; i < num_keys; i++)
		h2[i] = MEMBER_HASH_FUNC(&h1[i], sizeof(uint32_t),
						ss->sec_hash_seed);
	for (i = 0; i < num_keys; i++) {
		mask[i] = ~0;
		for (k = 0; k < ss->num_hashes; k++) {
			bit_loc = (h1[i] + k * h2[i]) & ss->bit_mask;
			mask[i] &= test_bit(bit_loc, ss);
		}
	}
	for (i = 0; i < num_keys; i++) {
		match_cnt_t = 0;
		while (mask[i]) {
			uint32_t loc = __builtin_ctzl(mask[i]);
			set_ids[i * match_per_key + match_cnt_t] = loc + 1;
			match_cnt_t++;
			if (match_cnt_t >= match_per_key)
				break;
			mask[i] &= ~(1UL << loc);
		}
		match_count[i] = match_cnt_t;
		if (match_cnt_t != 0)
			num_matches++;
	}
	return num_matches;
}

int
rte_member_add_vbf(const struct rte_member_setsum *ss,
		const void *key, member_set_t set_id)
{
	uint32_t i, h1, h2;
	uint32_t bit_loc;

	if (set_id > ss->num_set || set_id == RTE_MEMBER_NO_MATCH)
		return -EINVAL;

	h1 = MEMBER_HASH_FUNC(key, ss->key_len, ss->prim_hash_seed);
	h2 = MEMBER_HASH_FUNC(&h1, sizeof(uint32_t), ss->sec_hash_seed);

	for (i = 0; i < ss->num_hashes; i++) {
		bit_loc = (h1 + i * h2) & ss->bit_mask;
		set_bit(bit_loc, ss, set_id);
	}
	return 0;
}

void
rte_member_free_vbf(struct rte_member_setsum *ss)
{
	rte_free(ss->table);
}

void
rte_member_reset_vbf(const struct rte_member_setsum *ss)
{
	uint32_t *vbf = ss->table;
	memset(vbf, 0, (ss->num_set * ss->bits) >> 3);
}
