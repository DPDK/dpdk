/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

/*
 * Defines required by "acl_run_avx512_common.h".
 * Note that all of them has to be undefined by the end
 * of this file, as "acl_run_avx512_common.h" can be included several
 * times from different *.h files for the same *.c.
 */

/*
 * This implementation uses 256-bit registers(ymm) and intrinsics.
 * So our main SIMD type is 256-bit width and each such variable can
 * process sizeof(__m256i) / sizeof(uint32_t) == 8 entries in parallel.
 */
#define _T_simd		__m256i
#define _T_mask		__mmask8

/* Naming convention for static const variables. */
#define _SC_(x)		ymm_##x
#define _SV_(x)		(ymm_##x.y)

/* Naming convention for internal functions. */
#define _F_(x)		x##_avx512x8

/*
 * Same intrinsics have different syntaxes (depending on the bit-width),
 * so to overcome that few macros need to be defined.
 */

/* Naming convention for generic epi(packed integers) type intrinsics. */
#define _M_I_(x)	_mm256_##x

/* Naming convention for si(whole simd integer) type intrinsics. */
#define _M_SI_(x)	_mm256_##x##_si256

/* Naming convention for masked gather type intrinsics. */
#define _M_MGI_(x)	_mm256_m##x

/* Naming convention for gather type intrinsics. */
#define _M_GI_(name, idx, base, scale)	_mm256_##name(base, idx, scale)

/* num/mask of transitions per SIMD regs */
#define _SIMD_MASK_BIT_	(sizeof(_T_simd) / sizeof(uint32_t))
#define _SIMD_MASK_MAX_	RTE_LEN2MASK(_SIMD_MASK_BIT_, uint32_t)

#define _SIMD_FLOW_NUM_	(2 * _SIMD_MASK_BIT_)
#define _SIMD_FLOW_MSK_	(_SIMD_FLOW_NUM_ - 1)

/* num/mask of pointers per SIMD regs */
#define _SIMD_PTR_NUM_	(sizeof(_T_simd) / sizeof(uintptr_t))
#define _SIMD_PTR_MSK_	RTE_LEN2MASK(_SIMD_PTR_NUM_, uint32_t)

static const rte_ymm_t _SC_(match_mask) = {
	.u32 = {
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
	},
};

static const rte_ymm_t _SC_(index_mask) = {
	.u32 = {
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
	},
};

static const rte_ymm_t _SC_(trlo_idle) = {
	.u32 = {
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
		RTE_ACL_IDLE_NODE,
	},
};

static const rte_ymm_t _SC_(trhi_idle) = {
	.u32 = {
		0, 0, 0, 0,
		0, 0, 0, 0,
	},
};

static const rte_ymm_t _SC_(shuffle_input) = {
	.u32 = {
		0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c,
		0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c,
	},
};

static const rte_ymm_t _SC_(four_32) = {
	.u32 = {
		4, 4, 4, 4,
		4, 4, 4, 4,
	},
};

static const rte_ymm_t _SC_(idx_add) = {
	.u32 = {
		0, 1, 2, 3,
		4, 5, 6, 7,
	},
};

static const rte_ymm_t _SC_(range_base) = {
	.u32 = {
		0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c,
		0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c,
	},
};

static const rte_ymm_t _SC_(pminp) = {
	.u32 = {
		0x00, 0x01, 0x02, 0x03,
		0x08, 0x09, 0x0a, 0x0b,
	},
};

static const __mmask16 _SC_(pmidx_msk) = 0x55;

static const rte_ymm_t _SC_(pmidx[2]) = {
	[0] = {
		.u32 = {
			0, 0, 1, 0, 2, 0, 3, 0,
		},
	},
	[1] = {
		.u32 = {
			4, 0, 5, 0, 6, 0, 7, 0,
		},
	},
};

/*
 * unfortunately current AVX512 ISA doesn't provide ability for
 * gather load on a byte quantity. So we have to mimic it in SW,
 * by doing 4x1B scalar loads.
 */
static inline __m128i
_m256_mask_gather_epi8x4(__m256i pdata, __mmask8 mask)
{
	rte_xmm_t v;
	rte_ymm_t p;

	static const uint32_t zero;

	p.y = _mm256_mask_set1_epi64(pdata, mask ^ _SIMD_PTR_MSK_,
		(uintptr_t)&zero);

	v.u32[0] = *(uint8_t *)p.u64[0];
	v.u32[1] = *(uint8_t *)p.u64[1];
	v.u32[2] = *(uint8_t *)p.u64[2];
	v.u32[3] = *(uint8_t *)p.u64[3];

	return v.x;
}

/*
 * Gather 4/1 input bytes for up to 8 (2*8) locations in parallel.
 */
static __rte_always_inline __m256i
_F_(gather_bytes)(__m256i zero, const __m256i p[2], const uint32_t m[2],
	uint32_t bnum)
{
	__m128i inp[2];

	if (bnum == sizeof(uint8_t)) {
		inp[0] = _m256_mask_gather_epi8x4(p[0], m[0]);
		inp[1] = _m256_mask_gather_epi8x4(p[1], m[1]);
	} else {
		inp[0] = _mm256_mmask_i64gather_epi32(
				_mm256_castsi256_si128(zero),
				m[0], p[0], NULL, sizeof(uint8_t));
		inp[1] = _mm256_mmask_i64gather_epi32(
				_mm256_castsi256_si128(zero),
				m[1], p[1], NULL, sizeof(uint8_t));
	}

	/* squeeze input into one 256-bit register */
	return _mm256_permutex2var_epi32(_mm256_castsi128_si256(inp[0]),
			_SV_(pminp), _mm256_castsi128_si256(inp[1]));
}

#include "acl_run_avx512_common.h"

/*
 * Perform search for up to (2 * 8) flows in parallel.
 * Use two sets of metadata, each serves 8 flows max.
 */
static inline int
search_avx512x8x2(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	uint32_t i, *pm;
	const struct rte_acl_match_results *pr;
	struct acl_flow_avx512 flow;
	uint32_t match[ctx->num_tries * total_packets];

	for (i = 0, pm = match; i != ctx->num_tries; i++, pm += total_packets) {

		/* setup for next trie */
		acl_set_flow_avx512(&flow, ctx, i, data, pm, total_packets);

		/* process the trie */
		_F_(search_trie)(&flow);
	}

	/* resolve matches */
	pr = (const struct rte_acl_match_results *)
		(ctx->trans_table + ctx->match_index);

	if (categories == 1)
		_F_(resolve_single_cat)(results, pr, match, total_packets,
			ctx->num_tries);
	else
		resolve_mcle8_avx512x1(results, pr, match, total_packets,
			categories, ctx->num_tries);

	return 0;
}

#undef _SIMD_PTR_MSK_
#undef _SIMD_PTR_NUM_
#undef _SIMD_FLOW_MSK_
#undef _SIMD_FLOW_NUM_
#undef _SIMD_MASK_MAX_
#undef _SIMD_MASK_BIT_
#undef _M_GI_
#undef _M_MGI_
#undef _M_SI_
#undef _M_I_
#undef _F_
#undef _SV_
#undef _SC_
#undef _T_mask
#undef _T_simd
