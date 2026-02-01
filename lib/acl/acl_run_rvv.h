/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#include "acl_run.h"
#include <rte_vect.h>

static const uint32_t rvv_range_base[4] = {
	0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c
};

/*
 * Resolve priority for multiple results (RVV version).
 * This consists of comparing the priority of the current traversal with the
 * running set of results for the packet.
 * For each result, keep a running array of the result (rule number) and
 * its priority for each category.
 */
static inline void
resolve_priority_rvv(uint64_t transition, int n, const struct rte_acl_ctx *ctx,
	struct parms *parms, const struct rte_acl_match_results *p,
	uint32_t categories)
{
	const size_t vl = 4;

	for (size_t i = 0; i < categories; i += vl) {

		/* get results and priorities for completed trie */
		vuint32m1_t v_current_results =
			__riscv_vle32_v_u32m1(&p[transition].results[i], vl);
		vint32m1_t v_current_priority =
			__riscv_vle32_v_i32m1(&p[transition].priority[i], vl);

		/* if this is not the first completed trie */
		if (parms[n].cmplt->count != ctx->num_tries) {

			/* get running best results and their priorities */
			vuint32m1_t v_saved_results =
				__riscv_vle32_v_u32m1(&parms[n].cmplt->results[i], vl);
			vint32m1_t v_saved_priority =
				__riscv_vle32_v_i32m1(&parms[n].cmplt->priority[i], vl);

			/* select results that are highest priority */
			vbool32_t v_mask = __riscv_vmsle_vv_i32m1_b32(
				v_saved_priority, v_current_priority, vl);

			v_current_results = __riscv_vmerge_vvm_u32m1(
				v_saved_results, v_current_results, v_mask, vl);
			v_current_priority = __riscv_vmerge_vvm_i32m1(
				v_saved_priority, v_current_priority, v_mask, vl);
		}

		/* save running best results and their priorities */
		__riscv_vse32_v_u32m1(&parms[n].cmplt->results[i],
			v_current_results, vl);
		__riscv_vse32_v_i32m1(&parms[n].cmplt->priority[i],
			v_current_priority, vl);
	}
}

/*
 * Extract transitions from a vector register and check for any matches
 */
static void
acl_process_matches(uint64_t *indices, int slot,
	const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows)
{
	/* extract transition from low 64 bits. */
	indices[0] = acl_match_check(indices[0], slot, ctx,
		parms, flows, resolve_priority_rvv);

	/* extract transition from high 64 bits. */
	indices[1] = acl_match_check(indices[1], slot + 1, ctx,
		parms, flows, resolve_priority_rvv);
}

/*
 * Check for any match in 4 transitions (contained in 2 pairs of indices)
 */
static __rte_always_inline void
acl_match_check_x4(int slot, const struct rte_acl_ctx *ctx,
	struct parms *parms, struct acl_flow_data *flows,
	uint64_t *indices1, uint64_t *indices2, uint32_t match_mask)
{
	uint64_t check;

	while (1) {
		/* test for match node */
		check = ((indices1[0] | indices1[1]) |
			(indices2[0] | indices2[1])) & match_mask;
		if (check == 0)
			break;

		acl_process_matches(indices1, slot, ctx, parms, flows);
		acl_process_matches(indices2, slot + 2, ctx, parms, flows);
	}
}

/*
 * Process 4 transitions (in 1 RVV vector register) in parallel
 */
static __rte_always_inline vuint32m1_t
transition_vec(vuint32m1_t v_next_input, const uint64_t *trans,
	uint64_t *indices1, uint64_t *indices2, size_t vl,
	vuint32m1_t v_range_base)
{
	vuint32m1_t v_tr_lo, v_tr_hi;
	vuint64m2_t v_indices;

	v_indices = __riscv_vle64_v_u64m2(indices1, vl);
	v_tr_lo = __riscv_vnsrl_wx_u32m1(v_indices, 0, vl);
	v_tr_hi = __riscv_vnsrl_wx_u32m1(v_indices, 32, vl);

	/* expand input byte to 4 identical bytes per 32-bit element */
	vuint32m1_t v_input_expanded = __riscv_vmul_vx_u32m1(
		__riscv_vand_vx_u32m1(v_next_input, 0xFF, vl),
		0x01010101, vl);

	/* Calculate the address (array index) for all 4 transitions. */

	vint8m1_t v_input_bytes = __riscv_vreinterpret_v_i32m1_i8m1(
		__riscv_vreinterpret_v_u32m1_i32m1(v_input_expanded));
	vint8m1_t v_tr_hi_bytes = __riscv_vreinterpret_v_i32m1_i8m1(
		__riscv_vreinterpret_v_u32m1_i32m1(v_tr_hi));
	vbool8_t v_compare = __riscv_vmsgt_vv_i8m1_b8(v_input_bytes,
		v_tr_hi_bytes, vl * 4);

	vuint32m1_t v_bitmap = __riscv_vreinterpret_v_u8m1_u32m1(
		__riscv_vmerge_vxm_u8m1(__riscv_vmv_v_x_u8m1(0, vl * 4),
			1, v_compare, vl * 4));

	/* count set bits in bitmap to get quad offset */
	vuint32m1_t v_low16 = __riscv_vand_vx_u32m1(v_bitmap, 0xFFFF, vl);
	vuint32m1_t v_high16 = __riscv_vsrl_vx_u32m1(v_bitmap, 16, vl);
	vuint32m1_t v_sum_low = __riscv_vadd_vv_u32m1(
		__riscv_vand_vx_u32m1(v_low16, 0xFF, vl),
		__riscv_vsrl_vx_u32m1(v_low16, 8, vl),
		vl);
	vuint32m1_t v_sum_high = __riscv_vadd_vv_u32m1(
		__riscv_vand_vx_u32m1(v_high16, 0xFF, vl),
		__riscv_vsrl_vx_u32m1(v_high16, 8, vl),
		vl);
	vuint32m1_t v_quad_ofs = __riscv_vadd_vv_u32m1(v_sum_low,
		v_sum_high, vl);

	/* calculate DFA range offset */
	vuint32m1_t v_input_byte3 = __riscv_vsrl_vx_u32m1(v_input_expanded,
		24, vl);
	vuint8m1_t v_range_index = __riscv_vreinterpret_v_u32m1_u8m1(
		__riscv_vadd_vv_u32m1(__riscv_vsrl_vx_u32m1(v_input_expanded,
			30, vl), v_range_base, vl));
	vuint32m1_t v_range_value = __riscv_vreinterpret_v_u8m1_u32m1(
		__riscv_vrgather_vv_u8m1(__riscv_vreinterpret_v_u32m1_u8m1(
			v_tr_hi), v_range_index, vl * 4));

	/* select between quad offset (QRANGE/SINGLE) and DFA offset */
	vuint32m1_t v_offset = __riscv_vmerge_vvm_u32m1(v_quad_ofs,
		__riscv_vsub_vv_u32m1(v_input_byte3, v_range_value, vl),
		__riscv_vmseq_vx_u32m1_b32(__riscv_vand_vx_u32m1(v_tr_lo,
			~RTE_ACL_NODE_INDEX, vl), 0, vl), vl);

	/* calculate final transition address */
	vuint32m1_t v_addr = __riscv_vadd_vv_u32m1(
		__riscv_vand_vx_u32m1(v_tr_lo, RTE_ACL_NODE_INDEX, vl),
		v_offset, vl);

	/* Gather 64 bit transitions and pack back into 2 pairs. */

	indices1[0] = trans[__riscv_vmv_x_s_u32m1_u32(v_addr)];

	indices1[1] = trans[__riscv_vmv_x_s_u32m1_u32(
		__riscv_vslidedown_vx_u32m1(v_addr, 1, vl))];

	indices2[0] = trans[__riscv_vmv_x_s_u32m1_u32(
		__riscv_vslidedown_vx_u32m1(v_addr, 2, vl))];

	indices2[1] = trans[__riscv_vmv_x_s_u32m1_u32(
		__riscv_vslidedown_vx_u32m1(v_addr, 3, vl))];

	return __riscv_vsrl_vx_u32m1(v_next_input, CHAR_BIT, vl);
}

/*
 * Execute trie traversal with 8 traversals in parallel
 */
static inline int
search_rvv_8(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	const size_t vl = 4;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_RVV8];
	struct completion cmplt[MAX_SEARCHES_RVV8];
	struct parms parms[MAX_SEARCHES_RVV8];
	vuint32m1_t v_input0, v_input1;
	vuint32m1_t v_range_base;

	v_range_base = __riscv_vle32_v_u32m1(rvv_range_base, vl);

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_RVV8; n++)
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);

	/*
	 * index_array[0,1] and index_array[2,3] are processed by v_input0
	 * index_array[4,5] and index_array[6,7] are processed by v_input1
	 */

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&index_array[0], &index_array[2], RTE_ACL_NODE_MATCH);
	acl_match_check_x4(4, ctx, parms, &flows,
		&index_array[4], &index_array[6], RTE_ACL_NODE_MATCH);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		uint32_t input_data0[4] = {
			GET_NEXT_4BYTES(parms, 0),
			GET_NEXT_4BYTES(parms, 1),
			GET_NEXT_4BYTES(parms, 2),
			GET_NEXT_4BYTES(parms, 3)
		};
		uint32_t input_data1[4] = {
			GET_NEXT_4BYTES(parms, 4),
			GET_NEXT_4BYTES(parms, 5),
			GET_NEXT_4BYTES(parms, 6),
			GET_NEXT_4BYTES(parms, 7)
		};

		v_input0 = __riscv_vle32_v_u32m1(&input_data0[0], vl);
		v_input1 = __riscv_vle32_v_u32m1(&input_data1[0], vl);

		/* Process the 4 bytes of input on each stream. */

		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input1 = transition_vec(v_input1, flows.trans,
			&index_array[4], &index_array[6], vl, v_range_base);

		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input1 = transition_vec(v_input1, flows.trans,
			&index_array[4], &index_array[6], vl, v_range_base);

		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input1 = transition_vec(v_input1, flows.trans,
			&index_array[4], &index_array[6], vl, v_range_base);

		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input1 = transition_vec(v_input1, flows.trans,
			&index_array[4], &index_array[6], vl, v_range_base);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&index_array[0], &index_array[2], RTE_ACL_NODE_MATCH);
		acl_match_check_x4(4, ctx, parms, &flows,
			&index_array[4], &index_array[6], RTE_ACL_NODE_MATCH);
	}

	return 0;
}

/*
 * Execute trie traversal with 4 traversals in parallel
 */
static inline int
search_rvv_4(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	const size_t vl = 4;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_RVV4];
	struct completion cmplt[MAX_SEARCHES_RVV4];
	struct parms parms[MAX_SEARCHES_RVV4];
	vuint32m1_t v_input0;
	vuint32m1_t v_range_base;

	v_range_base = __riscv_vle32_v_u32m1(rvv_range_base, vl);

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_RVV4; n++)
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&index_array[0], &index_array[2], RTE_ACL_NODE_MATCH);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		uint32_t input_data[4] = {
			GET_NEXT_4BYTES(parms, 0),
			GET_NEXT_4BYTES(parms, 1),
			GET_NEXT_4BYTES(parms, 2),
			GET_NEXT_4BYTES(parms, 3)
		};

		v_input0 = __riscv_vle32_v_u32m1(&input_data[0], vl);

		/* Process the 4 bytes of input on each stream. */
		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);
		v_input0 = transition_vec(v_input0, flows.trans,
			&index_array[0], &index_array[2], vl, v_range_base);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&index_array[0], &index_array[2], RTE_ACL_NODE_MATCH);
	}

	return 0;
}
