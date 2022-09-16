/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef RTE_XXH64_AVX512_H
#define RTE_XXH64_AVX512_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <immintrin.h>

/* 0b1001111000110111011110011011000110000101111010111100101010000111 */
static const uint64_t PRIME64_1 = 0x9E3779B185EBCA87ULL;
/* 0b1100001010110010101011100011110100100111110101001110101101001111 */
static const uint64_t PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
/* 0b0001011001010110011001111011000110011110001101110111100111111001 */
static const uint64_t PRIME64_3 = 0x165667B19E3779F9ULL;
/* 0b1000010111101011110010100111011111000010101100101010111001100011 */
static const uint64_t PRIME64_4 = 0x85EBCA77C2B2AE63ULL;
/* 0b0010011111010100111010110010111100010110010101100110011111000101 */
static const uint64_t PRIME64_5 = 0x27D4EB2F165667C5ULL;

static __rte_always_inline  __m512i
xxh64_round_avx512(__m512i hash, __m512i input)
{
	hash = _mm512_madd52lo_epu64(hash,
			input,
			_mm512_set1_epi64(PRIME64_2));

	hash = _mm512_rol_epi64(hash, 31);

	return hash;
}

static __rte_always_inline  __m512i
xxh64_fmix_avx512(__m512i hash)
{
	hash = _mm512_xor_si512(hash, _mm512_srli_epi64(hash, 33));

	return hash;
}

static __rte_always_inline __m256i
rte_xxh64_sketch_avx512(const void *key, uint32_t key_len,
			__m512i v_seed, uint32_t modulo)
{
	__m512i v_prime64_5, v_hash;
	size_t remaining = key_len;
	size_t offset = 0;
	__m512i input;

	v_prime64_5 = _mm512_set1_epi64(PRIME64_5);
	v_hash = _mm512_add_epi64
			(_mm512_add_epi64(v_seed, v_prime64_5),
			 _mm512_set1_epi64(key_len));

	while (remaining >= 8) {
		input = _mm512_set1_epi64(*(uint64_t *)RTE_PTR_ADD(key, offset));
		v_hash = _mm512_xor_epi64(v_hash,
				xxh64_round_avx512(_mm512_setzero_si512(), input));
		v_hash = _mm512_madd52lo_epu64(_mm512_set1_epi64(PRIME64_4),
				v_hash,
				_mm512_set1_epi64(PRIME64_1));

		remaining -= 8;
		offset += 8;
	}

	if (remaining >= 4) {
		input = _mm512_set1_epi64
			(*(uint32_t *)RTE_PTR_ADD(key, offset));
		v_hash = _mm512_xor_epi64(v_hash,
			_mm512_mullo_epi64(input,
				_mm512_set1_epi64(PRIME64_1)));
		v_hash = _mm512_madd52lo_epu64
				(_mm512_set1_epi64(PRIME64_3),
				_mm512_rol_epi64(v_hash, 23),
				_mm512_set1_epi64(PRIME64_2));

		offset += 4;
		remaining -= 4;
	}

	while (remaining != 0) {
		input = _mm512_set1_epi64
			(*(uint8_t *)RTE_PTR_ADD(key, offset));
		v_hash = _mm512_xor_epi64(v_hash,
			_mm512_mullo_epi64(input,
				_mm512_set1_epi64(PRIME64_5)));
		v_hash = _mm512_mullo_epi64
			(_mm512_rol_epi64(v_hash, 11),
			_mm512_set1_epi64(PRIME64_1));
		offset++;
		remaining--;
	}

	v_hash = xxh64_fmix_avx512(v_hash);

	/*
	 * theoritically, such modular operations can be replaced by
	 * _mm512_rem_epi64(), but seems it depends on the compiler's
	 * implementation. so here is the limitation that the modulo
	 * value should be power of 2.
	 */
	__m512i v_hash_remainder = _mm512_set1_epi64((modulo - 1));

	return _mm512_cvtepi64_epi32(_mm512_and_si512(v_hash, v_hash_remainder));
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_XXH64_AVX512_H */
