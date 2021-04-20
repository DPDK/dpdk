/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 */

#ifndef _RTE_LPM_SVE_H_
#define _RTE_LPM_SVE_H_

#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

__rte_internal
static void
__rte_lpm_lookup_vec(const struct rte_lpm *lpm, const uint32_t *ips,
		uint32_t *__rte_restrict next_hops, const uint32_t n)
{
	uint32_t i = 0;
	svuint32_t v_ip, v_idx, v_tbl24, v_tbl8, v_hop;
	svuint32_t v_mask_xv, v_mask_v, v_mask_hop;
	svbool_t pg = svwhilelt_b32(i, n);
	svbool_t pv;

	do {
		v_ip = svld1(pg, &ips[i]);
		/* Get indices for tbl24[] */
		v_idx = svlsr_x(pg, v_ip, 8);
		/* Extract values from tbl24[] */
		v_tbl24 = svld1_gather_index(pg, (const uint32_t *)lpm->tbl24,
						v_idx);

		/* Create mask with valid set */
		v_mask_v = svdup_u32_z(pg, RTE_LPM_LOOKUP_SUCCESS);
		/* Create mask with valid and valid_group set */
		v_mask_xv = svdup_u32_z(pg, RTE_LPM_VALID_EXT_ENTRY_BITMASK);
		/* Create predicate for tbl24 entries: (valid && !valid_group) */
		pv = svcmpeq(pg, svand_z(pg, v_tbl24, v_mask_xv), v_mask_v);
		/* Create mask for next_hop in table entry */
		v_mask_hop = svdup_u32_z(pg, 0x00ffffff);
		/* Extract next_hop and write back */
		v_hop = svand_x(pv, v_tbl24, v_mask_hop);
		svst1(pv, &next_hops[i], v_hop);

		/* Update predicate for tbl24 entries: (valid && valid_group) */
		pv = svcmpeq(pg, svand_z(pg, v_tbl24, v_mask_xv), v_mask_xv);
		/* Compute tbl8 index */
		v_idx = svand_x(pv, v_tbl24, svdup_u32_z(pv, 0xffffff));
		v_idx = svmul_x(pv, v_idx, RTE_LPM_TBL8_GROUP_NUM_ENTRIES);
		v_idx = svadd_x(pv, svand_x(pv, v_ip, svdup_u32_z(pv, 0xff)),
				v_idx);
		/* Extract values from tbl8[] */
		v_tbl8 = svld1_gather_index(pv, (const uint32_t *)lpm->tbl8,
						v_idx);
		/* Update predicate for tbl8 entries: (valid) */
		pv = svcmpeq(pv, svand_z(pv, v_tbl8, v_mask_v), v_mask_v);
		/* Extract next_hop and write back */
		v_hop = svand_x(pv, v_tbl8, v_mask_hop);
		svst1(pv, &next_hops[i], v_hop);

		i += svlen(v_ip);
		pg = svwhilelt_b32(i, n);
	} while (svptest_any(svptrue_b32(), pg));
}

static inline void
rte_lpm_lookupx4(const struct rte_lpm *lpm, xmm_t ip, uint32_t hop[4],
		uint32_t defv)
{
	uint32_t i, ips[4];

	vst1q_s32((int32_t *)ips, ip);
	for (i = 0; i < 4; i++)
		hop[i] = defv;

	__rte_lpm_lookup_vec(lpm, ips, hop, 4);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_SVE_H_ */
