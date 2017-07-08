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

/*
 * rte_efd_arm64.h
 * This file holds all arm64 specific EFD functions
 */

#ifndef __RTE_EFD_ARM64_H__
#define __RTE_EFD_ARM64_H__

#include <rte_vect.h>

static inline efd_value_t
efd_lookup_internal_neon(const efd_hashfunc_t *group_hash_idx,
		const efd_lookuptbl_t *group_lookup_table,
		const uint32_t hash_val_a, const uint32_t hash_val_b)
{
	efd_value_t value = 0;
	uint32_t i = 0;
	uint32x4_t vhash_val_a = vmovq_n_u32(hash_val_a);
	uint32x4_t vhash_val_b = vmovq_n_u32(hash_val_b);
	int32x4_t vshift = {0, 1, 2, 3};
	uint32x4_t vmask = vdupq_n_u32(0x1);
	int32x4_t vincr = vdupq_n_s32(4);

	for (; i < RTE_EFD_VALUE_NUM_BITS; i += 4) {
		uint32x4_t vhash_idx = vshll_n_u16(
			vld1_u16((uint16_t const *)&group_hash_idx[i]), 0);
		uint32x4_t vlookup_table = vshll_n_u16(
			vld1_u16((uint16_t const *)&group_lookup_table[i]), 0);
		uint32x4_t vhash = vaddq_u32(vhash_val_a,
					vmulq_u32(vhash_idx, vhash_val_b));
		int32x4_t vbucket_idx = vnegq_s32(vreinterpretq_s32_u32(
				vshrq_n_u32(vhash, EFD_LOOKUPTBL_SHIFT)));
		uint32x4_t vresult = vshlq_u32(vlookup_table, vbucket_idx);

		vresult = vandq_u32(vresult, vmask);
		vresult = vshlq_u32(vresult, vshift);
		value |= vaddvq_u32(vresult);
		vshift = vaddq_s32(vshift, vincr);
	}

	return value;
}

#endif /* __RTE_EFD_ARM64_H__ */
