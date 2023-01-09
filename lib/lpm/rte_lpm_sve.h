/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Limited
 */

#ifndef _RTE_LPM_SVE_H_
#define _RTE_LPM_SVE_H_

#include <rte_compat.h>
#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int
__rte_lpm_lookup_vec(const struct rte_lpm *lpm, const uint32_t *ips,
		uint32_t *__rte_restrict next_hops, const uint32_t n)
{
	uint32_t i;
	uint64_t vl = svcntw();
	svuint32_t v_ip, v_idx, v_tbl24, v_tbl8;
	svuint32_t v_mask_xv, v_mask_v;
	svbool_t pg = svptrue_b32();
	svbool_t pv;

	for (i = 0; i < n; i++)
		next_hops[i] = 0;

	for (i = 0; i < n - vl; i += vl) {
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
		svst1(pv, &next_hops[i], v_tbl24);

		/* Update predicate for tbl24 entries: (valid && valid_group) */
		pv = svcmpeq(pg, svand_z(pg, v_tbl24, v_mask_xv), v_mask_xv);
		if (svptest_any(pg, pv)) {
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
			svst1(pv, &next_hops[i], v_tbl8);
		}
	}

	pg = svwhilelt_b32(i, n);
	if (svptest_any(svptrue_b32(), pg)) {
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
		svst1(pv, &next_hops[i], v_tbl24);

		/* Update predicate for tbl24 entries: (valid && valid_group) */
		pv = svcmpeq(pg, svand_z(pg, v_tbl24, v_mask_xv), v_mask_xv);
		if (svptest_any(pg, pv)) {
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
			svst1(pv, &next_hops[i], v_tbl8);
		}
	}

	return 0;
}
#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_SVE_H_ */
