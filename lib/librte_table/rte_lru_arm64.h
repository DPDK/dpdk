/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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

#ifndef __RTE_LRU_ARM64_H__
#define __RTE_LRU_ARM64_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_vect.h>

#ifndef RTE_TABLE_HASH_LRU_STRATEGY
#ifdef RTE_MACHINE_CPUFLAG_NEON
#define RTE_TABLE_HASH_LRU_STRATEGY                        3
#else /* if no NEON, use simple scalar version */
#define RTE_TABLE_HASH_LRU_STRATEGY                        1
#endif
#endif

#if RTE_TABLE_HASH_LRU_STRATEGY == 3

#define lru_init(bucket)						\
	{ bucket->lru_list = ~0LLU; }

static inline int
f_lru_pos(uint64_t lru_list)
{
	/* Compare the vector to zero vector */
	uint16x4_t lru_vec = vld1_u16((uint16_t *)&lru_list);
	uint16x4_t min_vec = vmov_n_u16(vminv_u16(lru_vec));
	uint64_t mask = vget_lane_u64(vreinterpret_u64_u16(
			vceq_u16(min_vec, lru_vec)), 0);
	return __builtin_clzl(mask) >> 4;
}
#define lru_pos(bucket) f_lru_pos(bucket->lru_list)

#define lru_update(bucket, mru_val)					\
do {									\
	const uint64_t orvals[] = {0xFFFFLLU, 0xFFFFLLU << 16,		\
		0xFFFFLLU << 32, 0xFFFFLLU << 48, 0LLU};		\
	const uint64_t decs[] = {0x1000100010001LLU, 0};		\
	uint64x1_t lru = vdup_n_u64(bucket->lru_list);			\
	uint64x1_t vdec = vdup_n_u64(decs[mru_val>>2]);			\
	bucket->lru_list = vget_lane_u64(vreinterpret_u64_u16(		\
				vsub_u16(vreinterpret_u16_u64(lru),	\
					vreinterpret_u16_u64(vdec))),	\
				0);					\
	bucket->lru_list |= orvals[mru_val];				\
} while (0)

#endif

#ifdef __cplusplus
}
#endif

#endif
