/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef _RTE_LPM_SCALAR_H_
#define _RTE_LPM_SCALAR_H_

#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t
__rte_lpm_lookupx4_hop(const struct rte_lpm *lpm, uint32_t tbl_entry,
		uint32_t ip, uint32_t defv)
{
	if (unlikely((tbl_entry & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		const uint32_t *tbl8 = (const uint32_t *)lpm->tbl8;

		tbl_entry = tbl8[(uint8_t)ip + (tbl_entry & 0x00FFFFFF) *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES];
	}

	return (tbl_entry & RTE_LPM_LOOKUP_SUCCESS) ?
		(tbl_entry & 0x00FFFFFF) : defv;
}

static inline void
rte_lpm_lookupx4(const struct rte_lpm *lpm, xmm_t ip, uint32_t hop[4],
		uint32_t defv)
{
	rte_xmm_t xip = { .x = ip };
	const uint32_t *tbl24 = (const uint32_t *)lpm->tbl24;
	uint32_t tbl0, tbl1, tbl2, tbl3;

	/* Issue the four tbl24 loads before any entry is examined. */
	tbl0 = tbl24[xip.u32[0] >> 8];
	tbl1 = tbl24[xip.u32[1] >> 8];
	tbl2 = tbl24[xip.u32[2] >> 8];
	tbl3 = tbl24[xip.u32[3] >> 8];

	hop[0] = __rte_lpm_lookupx4_hop(lpm, tbl0, xip.u32[0], defv);
	hop[1] = __rte_lpm_lookupx4_hop(lpm, tbl1, xip.u32[1], defv);
	hop[2] = __rte_lpm_lookupx4_hop(lpm, tbl2, xip.u32[2], defv);
	hop[3] = __rte_lpm_lookupx4_hop(lpm, tbl3, xip.u32[3], defv);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_SCALAR_H_ */
