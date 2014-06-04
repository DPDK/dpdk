/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INCLUDE_RTE_LRU_H__
#define __INCLUDE_RTE_LRU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef __INTEL_COMPILER
#define GCC_VERSION (0)
#else
#define GCC_VERSION (__GNUC__ * 10000+__GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)
#endif

#ifndef RTE_TABLE_HASH_LRU_STRATEGY
#ifdef __SSE4_2__
#define RTE_TABLE_HASH_LRU_STRATEGY                        2
#else /* if no SSE, use simple scalar version */
#define RTE_TABLE_HASH_LRU_STRATEGY                        1
#endif
#endif

#ifndef RTE_ARCH_X86_64
#undef RTE_TABLE_HASH_LRU_STRATEGY
#define RTE_TABLE_HASH_LRU_STRATEGY                        1
#endif

#if (RTE_TABLE_HASH_LRU_STRATEGY < 0) || (RTE_TABLE_HASH_LRU_STRATEGY > 3)
#error Invalid value for RTE_TABLE_HASH_LRU_STRATEGY
#endif

#if RTE_TABLE_HASH_LRU_STRATEGY == 0

#define lru_init(bucket)						\
do									\
	bucket = bucket;						\
while (0)

#define lru_pos(bucket) (bucket->lru_list & 0xFFFFLLU)

#define lru_update(bucket, mru_val)					\
do {									\
	bucket = bucket;						\
	mru_val = mru_val;						\
} while (0)

#elif RTE_TABLE_HASH_LRU_STRATEGY == 1

#define lru_init(bucket)						\
do									\
	bucket->lru_list = 0x0000000100020003LLU;			\
while (0)

#define lru_pos(bucket) (bucket->lru_list & 0xFFFFLLU)

#define lru_update(bucket, mru_val)					\
do {									\
	uint64_t x, pos, x0, x1, x2, mask;				\
									\
	x = bucket->lru_list;						\
									\
	pos = 4;							\
	if ((x >> 48) == ((uint64_t) mru_val))				\
		pos = 3;						\
									\
	if (((x >> 32) & 0xFFFFLLU) == ((uint64_t) mru_val))		\
		pos = 2;						\
									\
	if (((x >> 16) & 0xFFFFLLU) == ((uint64_t) mru_val))		\
		pos = 1;						\
									\
	if ((x & 0xFFFFLLU) == ((uint64_t) mru_val))			\
		pos = 0;						\
									\
									\
	pos <<= 4;							\
	mask = (~0LLU) << pos;						\
	x0 = x & (~mask);						\
	x1 = (x >> 16) & mask;						\
	x2 = (x << (48 - pos)) & (0xFFFFLLU << 48);			\
	x = x0 | x1 | x2;						\
									\
	if (pos != 64)							\
		bucket->lru_list = x;					\
} while (0)

#elif RTE_TABLE_HASH_LRU_STRATEGY == 2

#if GCC_VERSION > 40306
#include <x86intrin.h>
#else
#include <emmintrin.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#endif

#define lru_init(bucket)						\
do									\
	bucket->lru_list = 0x0000000100020003LLU;			\
while (0)

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
	unsigned pos = _mm_extract_epi16(d, 1);				\
	/* move the recently used location to top of list */		\
	__m128i k = _mm_shuffle_epi8(b, *((__m128i *) &masks[2 * pos]));\
	/* Finally, update the original list with the reordered data */	\
	bucket->lru_list = _mm_extract_epi64(k, 0);			\
	/* Phwew! */							\
} while (0)

#elif RTE_TABLE_HASH_LRU_STRATEGY == 3

#if GCC_VERSION > 40306
#include <x86intrin.h>
#else
#include <emmintrin.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#endif

#define lru_init(bucket)						\
do									\
	bucket->lru_list = ~0LLU;					\
while (0)


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

#else

#error "Incorrect value for RTE_TABLE_HASH_LRU_STRATEGY"

#endif

#ifdef __cplusplus
}
#endif

#endif
