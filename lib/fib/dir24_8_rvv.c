/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#if defined(RTE_RISCV_FEATURE_V)

#include <rte_vect.h>
#include <rte_fib.h>

#include "dir24_8.h"
#include "dir24_8_rvv.h"

#define DECLARE_VECTOR_FN(SFX, NH_SZ) \
void \
rte_dir24_8_vec_lookup_bulk_##SFX(void *p, \
		const uint32_t *ips, uint64_t *next_hops, unsigned int n) \
{ \
	const uint8_t  idx_bits = 3 - NH_SZ; \
	const uint32_t idx_mask = (1u << (3 - NH_SZ)) - 1u; \
	const uint64_t e_mask   = ~0ULL >> (64 - (8u << NH_SZ)); \
	struct dir24_8_tbl *tbl = (struct dir24_8_tbl *)p; \
	const uint64_t *tbl24   = tbl->tbl24; \
	size_t vl; \
	for (unsigned int i = 0; i < n; i += vl) { \
		vl = __riscv_vsetvl_e32m4(n - i); \
		vuint32m4_t v_ips = __riscv_vle32_v_u32m4(&ips[i], vl); \
		vuint64m8_t vtbl_word = __riscv_vluxei32_v_u64m8(tbl24, \
				__riscv_vsll_vx_u32m4( \
				__riscv_vsrl_vx_u32m4(v_ips, idx_bits + 8, vl), 3, vl), vl); \
		vuint32m4_t v_tbl_index = __riscv_vsrl_vx_u32m4(v_ips, 8, vl); \
		vuint32m4_t v_entry_idx = __riscv_vand_vx_u32m4(v_tbl_index, idx_mask, vl); \
		vuint32m4_t v_shift     = __riscv_vsll_vx_u32m4(v_entry_idx, 3 + NH_SZ, vl); \
		vuint64m8_t vtbl_entry  = __riscv_vand_vx_u64m8( \
				__riscv_vsrl_vv_u64m8(vtbl_word, \
					__riscv_vwcvtu_x_x_v_u64m8(v_shift, vl), vl), e_mask, vl); \
		vbool8_t mask = __riscv_vmseq_vx_u64m8_b8( \
				__riscv_vand_vx_u64m8(vtbl_entry, 1, vl), 1, vl); \
		if (__riscv_vcpop_m_b8(mask, vl)) { \
			const uint64_t *tbl8 = tbl->tbl8; \
			v_tbl_index = __riscv_vadd_vv_u32m4_mu(mask, v_tbl_index, \
					__riscv_vsll_vx_u32m4( \
						__riscv_vnsrl_wx_u32m4(vtbl_entry, 1, vl), 8, vl), \
						__riscv_vand_vx_u32m4(v_ips, 0xFF, vl), vl); \
			vtbl_word = __riscv_vluxei32_v_u64m8_mu(mask, vtbl_word, tbl8, \
					__riscv_vsll_vx_u32m4( \
					__riscv_vsrl_vx_u32m4(v_tbl_index, idx_bits, vl), 3, vl), \
						vl); \
			v_entry_idx = __riscv_vand_vx_u32m4(v_tbl_index, idx_mask, vl); \
			v_shift     = __riscv_vsll_vx_u32m4(v_entry_idx, 3 + NH_SZ, vl); \
			vtbl_entry  = __riscv_vand_vx_u64m8( \
					__riscv_vsrl_vv_u64m8(vtbl_word, \
					__riscv_vwcvtu_x_x_v_u64m8(v_shift, vl), vl), e_mask, vl); \
		} \
		__riscv_vse64_v_u64m8(&next_hops[i], \
				__riscv_vsrl_vx_u64m8(vtbl_entry, 1, vl), vl); \
	} \
}

DECLARE_VECTOR_FN(1b, 0)
DECLARE_VECTOR_FN(2b, 1)
DECLARE_VECTOR_FN(4b, 2)
DECLARE_VECTOR_FN(8b, 3)

#endif
