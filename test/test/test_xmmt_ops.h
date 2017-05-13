/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Cavium Networks. All rights reserved.
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

#ifndef _TEST_XMMT_OPS_H_
#define _TEST_XMMT_OPS_H_

#include <rte_vect.h>

#if defined(RTE_ARCH_ARM) || defined(RTE_ARCH_ARM64)

/* vect_* abstraction implementation using NEON */

/* loads the xmm_t value from address p(does not need to be 16-byte aligned)*/
#define vect_loadu_sil128(p) vld1q_s32((const int32_t *)p)

/* sets the 4 signed 32-bit integer values and returns the xmm_t variable */
static __rte_always_inline xmm_t
vect_set_epi32(int i3, int i2, int i1, int i0)
{
	int32_t data[4] = {i0, i1, i2, i3};

	return vld1q_s32(data);
}

#elif defined(RTE_ARCH_X86)

/* vect_* abstraction implementation using SSE */

/* loads the xmm_t value from address p(does not need to be 16-byte aligned)*/
#define vect_loadu_sil128(p) _mm_loadu_si128(p)

/* sets the 4 signed 32-bit integer values and returns the xmm_t variable */
#define vect_set_epi32(i3, i2, i1, i0) _mm_set_epi32(i3, i2, i1, i0)

#elif defined(RTE_ARCH_PPC_64)

/* vect_* abstraction implementation using ALTIVEC */

/* loads the xmm_t value from address p(does not need to be 16-byte aligned)*/
#define vect_loadu_sil128(p) vec_ld(0, p)

/* sets the 4 signed 32-bit integer values and returns the xmm_t variable */
static __rte_always_inline xmm_t
vect_set_epi32(int i3, int i2, int i1, int i0)
{
	xmm_t data = (xmm_t){i0, i1, i2, i3};

	return data;
}

#endif

#endif /* _TEST_XMMT_OPS_H_ */
