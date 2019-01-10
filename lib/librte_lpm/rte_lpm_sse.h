/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_LPM_SSE_H_
#define _RTE_LPM_SSE_H_

#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void
rte_lpm_lookupx4(const struct rte_lpm *lpm, xmm_t ip, uint32_t hop[4],
	uint32_t defv)
{
	__m128i i24;
	rte_xmm_t i8;
	uint32_t tbl[4];
	uint64_t idx, pt, pt2;
	const uint32_t *ptbl;

	const __m128i mask8 =
		_mm_set_epi32(UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX);

	/*
	 * RTE_LPM_VALID_EXT_ENTRY_BITMASK for 2 LPM entries
	 * as one 64-bit value (0x0300000003000000).
	 */
	const uint64_t mask_xv =
		((uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK |
		(uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK << 32);

	/*
	 * RTE_LPM_LOOKUP_SUCCESS for 2 LPM entries
	 * as one 64-bit value (0x0100000001000000).
	 */
	const uint64_t mask_v =
		((uint64_t)RTE_LPM_LOOKUP_SUCCESS |
		(uint64_t)RTE_LPM_LOOKUP_SUCCESS << 32);

	/* get 4 indexes for tbl24[]. */
	i24 = _mm_srli_epi32(ip, CHAR_BIT);

	/* extract values from tbl24[] */
	idx = _mm_cvtsi128_si64(i24);
	/* With -O0 option, gcc 4.8 - 5.4 fails to fold sizeof() into a constant */
	i24 = _mm_srli_si128(i24, /* sizeof(uint64_t) */ 8);

	ptbl = (const uint32_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[0] = *ptbl;
	ptbl = (const uint32_t *)&lpm->tbl24[idx >> 32];
	tbl[1] = *ptbl;

	idx = _mm_cvtsi128_si64(i24);

	ptbl = (const uint32_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[2] = *ptbl;
	ptbl = (const uint32_t *)&lpm->tbl24[idx >> 32];
	tbl[3] = *ptbl;

	/* get 4 indexes for tbl8[]. */
	i8.x = _mm_and_si128(ip, mask8);

	pt = (uint64_t)tbl[0] |
		(uint64_t)tbl[1] << 32;
	pt2 = (uint64_t)tbl[2] |
		(uint64_t)tbl[3] << 32;

	/* search successfully finished for all 4 IP addresses. */
	if (likely((pt & mask_xv) == mask_v) &&
			likely((pt2 & mask_xv) == mask_v)) {
		*(uint64_t *)hop = pt & RTE_LPM_MASKX4_RES;
		*(uint64_t *)(hop + 2) = pt2 & RTE_LPM_MASKX4_RES;
		return;
	}

	if (unlikely((pt & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[0] = i8.u32[0] +
			(uint8_t)tbl[0] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[0]];
		tbl[0] = *ptbl;
	}
	if (unlikely((pt >> 32 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[1] = i8.u32[1] +
			(uint8_t)tbl[1] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[1]];
		tbl[1] = *ptbl;
	}
	if (unlikely((pt2 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[2] = i8.u32[2] +
			(uint8_t)tbl[2] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[2]];
		tbl[2] = *ptbl;
	}
	if (unlikely((pt2 >> 32 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[3] = i8.u32[3] +
			(uint8_t)tbl[3] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[3]];
		tbl[3] = *ptbl;
	}

	hop[0] = (tbl[0] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[0] & 0x00FFFFFF : defv;
	hop[1] = (tbl[1] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[1] & 0x00FFFFFF : defv;
	hop[2] = (tbl[2] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[2] & 0x00FFFFFF : defv;
	hop[3] = (tbl[3] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[3] & 0x00FFFFFF : defv;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_SSE_H_ */
