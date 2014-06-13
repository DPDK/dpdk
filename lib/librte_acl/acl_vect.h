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

#ifndef _RTE_ACL_VECT_H_
#define _RTE_ACL_VECT_H_

/**
 * @file
 *
 * RTE ACL SSE/AVX related header.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	MM_ADD16(a, b)		_mm_add_epi16(a, b)
#define	MM_ADD32(a, b)		_mm_add_epi32(a, b)
#define	MM_ALIGNR8(a, b, c)	_mm_alignr_epi8(a, b, c)
#define	MM_AND(a, b)		_mm_and_si128(a, b)
#define MM_ANDNOT(a, b)		_mm_andnot_si128(a, b)
#define MM_BLENDV8(a, b, c)	_mm_blendv_epi8(a, b, c)
#define MM_CMPEQ16(a, b)	_mm_cmpeq_epi16(a, b)
#define MM_CMPEQ32(a, b)	_mm_cmpeq_epi32(a, b)
#define	MM_CMPEQ8(a, b)		_mm_cmpeq_epi8(a, b)
#define MM_CMPGT32(a, b)	_mm_cmpgt_epi32(a, b)
#define MM_CMPGT8(a, b)		_mm_cmpgt_epi8(a, b)
#define MM_CVT(a)		_mm_cvtsi32_si128(a)
#define	MM_CVT32(a)		_mm_cvtsi128_si32(a)
#define MM_CVTU32(a)		_mm_cvtsi32_si128(a)
#define	MM_INSERT16(a, c, b)	_mm_insert_epi16(a, c, b)
#define	MM_INSERT32(a, c, b)	_mm_insert_epi32(a, c, b)
#define	MM_LOAD(a)		_mm_load_si128(a)
#define	MM_LOADH_PI(a, b)	_mm_loadh_pi(a, b)
#define	MM_LOADU(a)		_mm_loadu_si128(a)
#define	MM_MADD16(a, b)		_mm_madd_epi16(a, b)
#define	MM_MADD8(a, b)		_mm_maddubs_epi16(a, b)
#define	MM_MOVEMASK8(a)		_mm_movemask_epi8(a)
#define MM_OR(a, b)		_mm_or_si128(a, b)
#define	MM_SET1_16(a)		_mm_set1_epi16(a)
#define	MM_SET1_32(a)		_mm_set1_epi32(a)
#define	MM_SET1_64(a)		_mm_set1_epi64(a)
#define	MM_SET1_8(a)		_mm_set1_epi8(a)
#define	MM_SET32(a, b, c, d)	_mm_set_epi32(a, b, c, d)
#define	MM_SHUFFLE32(a, b)	_mm_shuffle_epi32(a, b)
#define	MM_SHUFFLE8(a, b)	_mm_shuffle_epi8(a, b)
#define	MM_SHUFFLEPS(a, b, c)	_mm_shuffle_ps(a, b, c)
#define	MM_SIGN8(a, b)		_mm_sign_epi8(a, b)
#define	MM_SLL64(a, b)		_mm_sll_epi64(a, b)
#define	MM_SRL128(a, b)		_mm_srli_si128(a, b)
#define MM_SRL16(a, b)		_mm_srli_epi16(a, b)
#define	MM_SRL32(a, b)		_mm_srli_epi32(a, b)
#define	MM_STORE(a, b)		_mm_store_si128(a, b)
#define	MM_STOREU(a, b)		_mm_storeu_si128(a, b)
#define	MM_TESTZ(a, b)		_mm_testz_si128(a, b)
#define	MM_XOR(a, b)		_mm_xor_si128(a, b)

#define	MM_SET16(a, b, c, d, e, f, g, h)	\
	_mm_set_epi16(a, b, c, d, e, f, g, h)

#define	MM_SET8(c0, c1, c2, c3, c4, c5, c6, c7,	\
		c8, c9, cA, cB, cC, cD, cE, cF)	\
	_mm_set_epi8(c0, c1, c2, c3, c4, c5, c6, c7,	\
		c8, c9, cA, cB, cC, cD, cE, cF)

#ifdef RTE_ARCH_X86_64

#define	MM_CVT64(a)		_mm_cvtsi128_si64(a)

#else

#define	MM_CVT64(a)	({ \
	rte_xmm_t m;       \
	m.m = (a);         \
	(m.u64[0]);        \
})

#endif /*RTE_ARCH_X86_64 */

/*
 * Prior to version 12.1 icc doesn't support _mm_set_epi64x.
 */
#if (defined(__ICC) && __ICC < 1210)

#define	MM_SET64(a, b)	({ \
	rte_xmm_t m;       \
	m.u64[0] = b;      \
	m.u64[1] = a;      \
	(m.m);             \
})

#else

#define	MM_SET64(a, b)		_mm_set_epi64x(a, b)

#endif /* (defined(__ICC) && __ICC < 1210) */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ACL_VECT_H_ */
