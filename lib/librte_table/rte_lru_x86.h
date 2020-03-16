/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_LRU_X86_H__
#define __INCLUDE_RTE_LRU_X86_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_config.h>

#ifndef RTE_TABLE_HASH_LRU_STRATEGY
#define RTE_TABLE_HASH_LRU_STRATEGY                        2
#endif

#if RTE_TABLE_HASH_LRU_STRATEGY == 2

#if RTE_CC_IS_GNU && (GCC_VERSION > 40306)
#include <x86intrin.h>
#else
#include <emmintrin.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#endif

#define lru_init(bucket)						\
	{ bucket->lru_list = 0x0000000100020003LLU; }

#define lru_pos(bucket) (bucket->lru_list & 0xFFFFLLU)

#define lru_update(bucket, mru_val)					\
do {									\
	/* set up the masks for all possible shuffles, depends on pos */\
	static uint64_t masks[10] = {					\
		/* Shuffle order; Make Zero (see _mm_shuffle_epi8 manual) */\
		0x0100070605040302, 0x8080808080808080,			\
		0x0302070605040100, 0x8080808080808080,			\
		0x0504070603020100, 0x8080808080808080,			\
		0x0706050403020100, 0x8080808080808080,			\
		0x0706050403020100, 0x8080808080808080};		\
	/* load up one register with repeats of mru-val  */		\
	uint64_t mru2 = mru_val;					\
	uint64_t mru3 = mru2 | (mru2 << 16);				\
	uint64_t lru = bucket->lru_list;				\
	/* XOR to cause the word we're looking for to go to zero */	\
	uint64_t mru = lru ^ ((mru3 << 32) | mru3);			\
	__m128i c = _mm_cvtsi64_si128(mru);				\
	__m128i b = _mm_cvtsi64_si128(lru);				\
	/* Find the minimum value (first zero word, if it's in there) */\
	__m128i d = _mm_minpos_epu16(c);				\
	/* Second word is the index to found word (first word is the value) */\
	unsigned int pos = _mm_extract_epi16(d, 1);			\
	/* move the recently used location to top of list */		\
	__m128i k = _mm_shuffle_epi8(b, *((__m128i *) &masks[2 * pos]));\
	/* Finally, update the original list with the reordered data */	\
	bucket->lru_list = _mm_extract_epi64(k, 0);			\
	/* Phwew! */							\
} while (0)

#elif RTE_TABLE_HASH_LRU_STRATEGY == 3

#if RTE_CC_IS_GNU && (GCC_VERSION > 40306)
#include <x86intrin.h>
#else
#include <emmintrin.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#endif

#define lru_init(bucket)						\
	{ bucket->lru_list = ~0LLU; }

static inline int
f_lru_pos(uint64_t lru_list)
{
	__m128i lst = _mm_set_epi64x((uint64_t)-1, lru_list);
	__m128i min = _mm_minpos_epu16(lst);
	return _mm_extract_epi16(min, 1);
}
#define lru_pos(bucket) f_lru_pos(bucket->lru_list)

#define lru_update(bucket, mru_val)					\
do {									\
	const uint64_t orvals[] = {0xFFFFLLU, 0xFFFFLLU << 16,		\
		0xFFFFLLU << 32, 0xFFFFLLU << 48, 0LLU};		\
	const uint64_t decs[] = {0x1000100010001LLU, 0};		\
	__m128i lru = _mm_cvtsi64_si128(bucket->lru_list);		\
	__m128i vdec = _mm_cvtsi64_si128(decs[mru_val>>2]);		\
	lru = _mm_subs_epu16(lru, vdec);				\
	bucket->lru_list = _mm_extract_epi64(lru, 0) | orvals[mru_val];	\
} while (0)

#endif

#ifdef __cplusplus
}
#endif

#endif
