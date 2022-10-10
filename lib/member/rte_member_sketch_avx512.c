/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "rte_xxh64_avx512.h"
#include "rte_member_sketch_avx512.h"

__rte_always_inline void
sketch_update_avx512(const struct rte_member_setsum *ss,
		     const void *key,
		     uint32_t count)
{
	uint64_t *count_array = ss->table;
	uint32_t num_col = ss->num_col;
	uint32_t key_len = ss->key_len;
	__m256i v_row_base;
	__m256i v_hash_result;
	__m512i current_sketch;
	__m512i updated_sketch;
	__m512i v_count;

	const __m256i v_idx = _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0);
	const __m256i v_col = _mm256_set1_epi32(num_col);

	/* compute the hash result parallelly */
	v_hash_result = rte_xxh64_sketch_avx512
		(key, key_len, *(__m512i *)ss->hash_seeds, num_col);
	v_row_base = _mm256_mullo_epi32(v_idx, v_col);
	v_hash_result = _mm256_add_epi32(v_row_base, v_hash_result);

	current_sketch = _mm512_i32gather_epi64
				(v_hash_result, (void *)count_array, 8);
	v_count = _mm512_set1_epi64(count);
	updated_sketch = _mm512_add_epi64(current_sketch, v_count);
	_mm512_i32scatter_epi64
		((void *)count_array, v_hash_result, updated_sketch, 8);
}

uint64_t
sketch_lookup_avx512(const struct rte_member_setsum *ss, const void *key)
{
	uint32_t col[ss->num_row];

	/* currently only for sketch byte count mode */
	__m256i v_hash_result = rte_xxh64_sketch_avx512
		(key, ss->key_len, *(__m512i *)ss->hash_seeds, ss->num_col);
	_mm256_storeu_si256((__m256i *)col, v_hash_result);

	return count_min(ss, col);
}

void
sketch_delete_avx512(const struct rte_member_setsum *ss, const void *key)
{
	uint32_t col[ss->num_row];
	uint64_t *count_array = ss->table;
	uint64_t min = UINT64_MAX;
	uint32_t cur_row;

	__m256i v_hash_result = rte_xxh64_sketch_avx512
		(key, ss->key_len, *(__m512i *)ss->hash_seeds,
		 RTE_ALIGN_FLOOR(ss->num_col, 32));
	_mm256_storeu_si256((__m256i *)col, v_hash_result);

	min = count_min(ss, col);

	/* subtract the min value from all the counters */
	for (cur_row = 0; cur_row < ss->num_row; cur_row++)
		count_array[cur_row * ss->num_col + col[cur_row]] -= min;
}
