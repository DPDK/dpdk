/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#if defined(RTE_RISCV_FEATURE_V)

#include <rte_vect.h>
#include <rte_fib.h>

#include "dir24_8.h"
#include "dir24_8_rvv.h"

/* Byte offset of entry number idx, for each next hop size. */
#define OFS_1b(idx, vl) ((void)(vl), (idx))
#define OFS_2b(idx, vl) __riscv_vsll_vx_u32m4(idx, 1, vl)
#define OFS_4b(idx, vl) __riscv_vsll_vx_u32m4(idx, 2, vl)
#define OFS_8b(idx, vl) __riscv_vsll_vx_u32m4(idx, 3, vl)

/* Entry without its low bit, as 32-bit tbl8 group number. */
#define GRP_1b(ent, vl) \
	__riscv_vzext_vf4_u32m4(__riscv_vsrl_vx_u8m1(ent, 1, vl), vl)
#define GRP_2b(ent, vl) \
	__riscv_vzext_vf2_u32m4(__riscv_vsrl_vx_u16m2(ent, 1, vl), vl)
#define GRP_4b(ent, vl) __riscv_vsrl_vx_u32m4(ent, 1, vl)
#define GRP_8b(ent, vl) __riscv_vnsrl_wx_u32m4(ent, 1, vl)

/* Entry without its low bit, as 64-bit next hop. */
#define NH_1b(ent, vl) \
	__riscv_vzext_vf8_u64m8(__riscv_vsrl_vx_u8m1(ent, 1, vl), vl)
#define NH_2b(ent, vl) \
	__riscv_vzext_vf4_u64m8(__riscv_vsrl_vx_u16m2(ent, 1, vl), vl)
#define NH_4b(ent, vl) \
	__riscv_vzext_vf2_u64m8(__riscv_vsrl_vx_u32m4(ent, 1, vl), vl)
#define NH_8b(ent, vl) __riscv_vsrl_vx_u64m8(ent, 1, vl)

/* Entries are gathered at their own width, with 32-bit byte offsets. */
#define DECLARE_VECTOR_FN(SFX, TYPE, BITS, LMUL) \
void \
rte_dir24_8_vec_lookup_bulk_##SFX(void *p, \
		const uint32_t *ips, uint64_t *next_hops, unsigned int n) \
{ \
	const struct dir24_8_tbl *tbl = (const struct dir24_8_tbl *)p; \
	const TYPE *tbl24 = (const TYPE *)tbl->tbl24; \
	const TYPE *tbl8 = (const TYPE *)tbl->tbl8; \
	size_t vl; \
	for (unsigned int i = 0; i < n; i += vl) { \
		vl = __riscv_vsetvl_e32m4(n - i); \
		vuint32m4_t v_ips = __riscv_vle32_v_u32m4(&ips[i], vl); \
		vuint##BITS##m##LMUL##_t v_ent = \
			__riscv_vluxei32_v_u##BITS##m##LMUL(tbl24, \
				OFS_##SFX(__riscv_vsrl_vx_u32m4(v_ips, 8, vl), \
					vl), vl); \
		vbool8_t mask = __riscv_vmsne_vx_u##BITS##m##LMUL##_b8( \
			__riscv_vand_vx_u##BITS##m##LMUL(v_ent, \
				DIR24_8_EXT_ENT, vl), 0, vl); \
		if (unlikely(__riscv_vfirst_m_b8(mask, vl) >= 0)) { \
			vuint32m4_t v_idx = __riscv_vadd_vv_u32m4( \
				__riscv_vsll_vx_u32m4(GRP_##SFX(v_ent, vl), \
					8, vl), \
				__riscv_vand_vx_u32m4(v_ips, 0xFF, vl), vl); \
			v_ent = __riscv_vluxei32_v_u##BITS##m##LMUL##_mu(mask, \
				v_ent, tbl8, OFS_##SFX(v_idx, vl), vl); \
		} \
		__riscv_vse64_v_u64m8(&next_hops[i], NH_##SFX(v_ent, vl), vl); \
	} \
}

DECLARE_VECTOR_FN(1b, uint8_t, 8, 1)
DECLARE_VECTOR_FN(2b, uint16_t, 16, 2)
DECLARE_VECTOR_FN(4b, uint32_t, 32, 4)
DECLARE_VECTOR_FN(8b, uint64_t, 64, 8)

#endif
