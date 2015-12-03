/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Cavium Networks. All rights reserved.
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
 *     * Neither the name of Cavium Networks nor the names of its
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

#ifndef _RTE_VECT_ARM_H_
#define _RTE_VECT_ARM_H_

#include "arm_neon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32x4_t xmm_t;

#define	XMM_SIZE	(sizeof(xmm_t))
#define	XMM_MASK	(XMM_SIZE - 1)

typedef union rte_xmm {
	xmm_t    x;
	uint8_t  u8[XMM_SIZE / sizeof(uint8_t)];
	uint16_t u16[XMM_SIZE / sizeof(uint16_t)];
	uint32_t u32[XMM_SIZE / sizeof(uint32_t)];
	uint64_t u64[XMM_SIZE / sizeof(uint64_t)];
	double   pd[XMM_SIZE / sizeof(double)];
} __attribute__((aligned(16))) rte_xmm_t;

#ifdef RTE_ARCH_ARM
/* NEON intrinsic vqtbl1q_u8() is not supported in ARMv7-A(AArch32) */
static __inline uint8x16_t
vqtbl1q_u8(uint8x16_t a, uint8x16_t b)
{
	uint8_t i, pos;
	rte_xmm_t rte_a, rte_b, rte_ret;

	vst1q_u8(rte_a.u8, a);
	vst1q_u8(rte_b.u8, b);

	for (i = 0; i < 16; i++) {
		pos = rte_b.u8[i];
		if (pos < 16)
			rte_ret.u8[i] = rte_a.u8[pos];
		else
			rte_ret.u8[i] = 0;
	}

	return vld1q_u8(rte_ret.u8);
}
#endif

#ifdef __cplusplus
}
#endif

#endif
