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

#ifdef RTE_ARCH_X86_64
#include "rte_lru_x86.h"
#else
#undef RTE_TABLE_HASH_LRU_STRATEGY
#define RTE_TABLE_HASH_LRU_STRATEGY                        1
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

#elif (RTE_TABLE_HASH_LRU_STRATEGY == 2) || (RTE_TABLE_HASH_LRU_STRATEGY == 3)

/**
 * These strategies are implemented in architecture specific header files.
 */

#else

#error "Incorrect value for RTE_TABLE_HASH_LRU_STRATEGY"

#endif

#ifdef __cplusplus
}
#endif

#endif
