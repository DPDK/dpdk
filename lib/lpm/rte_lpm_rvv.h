/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#ifndef _RTE_LPM_RVV_H_
#define _RTE_LPM_RVV_H_

#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_LPM_LOOKUP_SUCCESS 0x01000000
#define RTE_LPM_VALID_EXT_ENTRY_BITMASK 0x03000000

static inline void rte_lpm_lookupx4(
	const struct rte_lpm *lpm, xmm_t ip, uint32_t hop[4], uint32_t defv)
{
	size_t vl = 4;

	const uint32_t *tbl24_p = (const uint32_t *)lpm->tbl24;
	uint32_t tbl_entries[4] = {
		tbl24_p[((uint32_t)ip[0]) >> 8],
		tbl24_p[((uint32_t)ip[1]) >> 8],
		tbl24_p[((uint32_t)ip[2]) >> 8],
		tbl24_p[((uint32_t)ip[3]) >> 8],
	};
	vuint32m1_t vtbl_entry = __riscv_vle32_v_u32m1(tbl_entries, vl);

	vbool32_t mask = __riscv_vmseq_vx_u32m1_b32(
	    __riscv_vand_vx_u32m1(vtbl_entry, RTE_LPM_VALID_EXT_ENTRY_BITMASK, vl),
	    RTE_LPM_VALID_EXT_ENTRY_BITMASK, vl);

	vuint32m1_t vtbl8_index = __riscv_vsll_vx_u32m1(
	    __riscv_vadd_vv_u32m1(
		__riscv_vsll_vx_u32m1(__riscv_vand_vx_u32m1(vtbl_entry, 0x00FFFFFF, vl), 8, vl),
		__riscv_vand_vx_u32m1(
		    __riscv_vle32_v_u32m1((const uint32_t *)&ip, vl), 0x000000FF, vl),
		vl),
	    2, vl);

	vtbl_entry = __riscv_vluxei32_v_u32m1_mu(
	    mask, vtbl_entry, (const uint32_t *)(lpm->tbl8), vtbl8_index, vl);

	vuint32m1_t vnext_hop = __riscv_vand_vx_u32m1(vtbl_entry, 0x00FFFFFF, vl);
	mask = __riscv_vmseq_vx_u32m1_b32(
	    __riscv_vand_vx_u32m1(vtbl_entry, RTE_LPM_LOOKUP_SUCCESS, vl), 0, vl);

	vnext_hop = __riscv_vmerge_vxm_u32m1(vnext_hop, defv, mask, vl);

	__riscv_vse32_v_u32m1(hop, vnext_hop, vl);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_RVV_H_ */
