/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2015.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#include "acl_run.h"
#include "acl_vect.h"

struct _neon_acl_const {
	rte_xmm_t xmm_shuffle_input;
	rte_xmm_t xmm_index_mask;
	rte_xmm_t range_base;
} neon_acl_const  __attribute__((aligned(RTE_CACHE_LINE_SIZE))) = {
	{
		.u32 = {0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c}
	},
	{
		.u32 = {RTE_ACL_NODE_INDEX, RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX, RTE_ACL_NODE_INDEX}
	},
	{
		.u32 = {0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c}
	},
};

/*
 * Resolve priority for multiple results (neon version).
 * This consists comparing the priority of the current traversal with the
 * running set of results for the packet.
 * For each result, keep a running array of the result (rule number) and
 * its priority for each category.
 */
static inline void
resolve_priority_neon(uint64_t transition, int n, const struct rte_acl_ctx *ctx,
		      struct parms *parms,
		      const struct rte_acl_match_results *p,
		      uint32_t categories)
{
	uint32_t x;
	int32x4_t results, priority, results1, priority1;
	uint32x4_t selector;
	int32_t *saved_results, *saved_priority;

	for (x = 0; x < categories; x += RTE_ACL_RESULTS_MULTIPLIER) {
		saved_results = (int32_t *)(&parms[n].cmplt->results[x]);
		saved_priority = (int32_t *)(&parms[n].cmplt->priority[x]);

		/* get results and priorities for completed trie */
		results = vld1q_s32(
			(const int32_t *)&p[transition].results[x]);
		priority = vld1q_s32(
			(const int32_t *)&p[transition].priority[x]);

		/* if this is not the first completed trie */
		if (parms[n].cmplt->count != ctx->num_tries) {
			/* get running best results and their priorities */
			results1 = vld1q_s32(saved_results);
			priority1 = vld1q_s32(saved_priority);

			/* select results that are highest priority */
			selector = vcgtq_s32(priority1, priority);
			results = vbslq_s32(selector, results1, results);
			priority = vbslq_s32(selector, priority1, priority);
		}

		/* save running best results and their priorities */
		vst1q_s32(saved_results, results);
		vst1q_s32(saved_priority, priority);
	}
}

/*
 * Check for any match in 4 transitions
 */
static __rte_always_inline uint32_t
check_any_match_x4(uint64_t val[])
{
	return (val[0] | val[1] | val[2] | val[3]) & RTE_ACL_NODE_MATCH;
}

static __rte_always_inline void
acl_match_check_x4(int slot, const struct rte_acl_ctx *ctx, struct parms *parms,
		   struct acl_flow_data *flows, uint64_t transitions[])
{
	while (check_any_match_x4(transitions)) {
		transitions[0] = acl_match_check(transitions[0], slot, ctx,
			parms, flows, resolve_priority_neon);
		transitions[1] = acl_match_check(transitions[1], slot + 1, ctx,
			parms, flows, resolve_priority_neon);
		transitions[2] = acl_match_check(transitions[2], slot + 2, ctx,
			parms, flows, resolve_priority_neon);
		transitions[3] = acl_match_check(transitions[3], slot + 3, ctx,
			parms, flows, resolve_priority_neon);
	}
}

/*
 * Process 4 transitions (in 2 NEON Q registers) in parallel
 */
static __rte_always_inline int32x4_t
transition4(int32x4_t next_input, const uint64_t *trans, uint64_t transitions[])
{
	int32x4x2_t tr_hi_lo;
	int32x4_t t, in, r;
	uint32x4_t index_msk, node_type, addr;
	uint32x4_t dfa_msk, mask, quad_ofs, dfa_ofs;

	/* Move low 32 into tr_hi_lo.val[0] and high 32 into tr_hi_lo.val[1] */
	tr_hi_lo = vld2q_s32((const int32_t *)transitions);

	/* Calculate the address (array index) for all 4 transitions. */

	index_msk = vld1q_u32((const uint32_t *)&neon_acl_const.xmm_index_mask);

	/* Calc node type and node addr */
	node_type = vbicq_s32(tr_hi_lo.val[0], index_msk);
	addr = vandq_s32(tr_hi_lo.val[0], index_msk);

	/* t = 0 */
	t = veorq_s32(node_type, node_type);

	/* mask for DFA type(0) nodes */
	dfa_msk = vceqq_u32(node_type, t);

	mask = vld1q_s32((const int32_t *)&neon_acl_const.xmm_shuffle_input);
	in = vqtbl1q_u8((uint8x16_t)next_input, (uint8x16_t)mask);

	/* DFA calculations. */
	r = vshrq_n_u32(in, 30); /* div by 64 */
	mask = vld1q_s32((const int32_t *)&neon_acl_const.range_base);
	r = vaddq_u8(r, mask);
	t = vshrq_n_u32(in, 24);
	r = vqtbl1q_u8((uint8x16_t)tr_hi_lo.val[1], (uint8x16_t)r);
	dfa_ofs = vsubq_s32(t, r);

	/* QUAD/SINGLE calculations. */
	t = vcgtq_s8(in, tr_hi_lo.val[1]);
	t = vabsq_s8(t);
	t = vpaddlq_u8(t);
	quad_ofs = vpaddlq_u16(t);

	/* blend DFA and QUAD/SINGLE. */
	t = vbslq_u8(dfa_msk, dfa_ofs, quad_ofs);

	/* calculate address for next transitions */
	addr = vaddq_u32(addr, t);

	/* Fill next transitions */
	transitions[0] = trans[vgetq_lane_u32(addr, 0)];
	transitions[1] = trans[vgetq_lane_u32(addr, 1)];
	transitions[2] = trans[vgetq_lane_u32(addr, 2)];
	transitions[3] = trans[vgetq_lane_u32(addr, 3)];

	return vshrq_n_u32(next_input, CHAR_BIT);
}

/*
 * Execute trie traversal with 8 traversals in parallel
 */
static inline int
search_neon_8(const struct rte_acl_ctx *ctx, const uint8_t **data,
	      uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[8];
	struct completion cmplt[8];
	struct parms parms[8];
	int32x4_t input0, input1;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		     total_packets, categories, ctx->trans_table);

	for (n = 0; n < 8; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	 /* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows, &index_array[0]);
	acl_match_check_x4(4, ctx, parms, &flows, &index_array[4]);

	while (flows.started > 0) {
		/* Gather 4 bytes of input data for each stream. */
		input0 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 0), input0, 0);
		input1 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 4), input1, 0);

		input0 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 1), input0, 1);
		input1 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 5), input1, 1);

		input0 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 2), input0, 2);
		input1 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 6), input1, 2);

		input0 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 3), input0, 3);
		input1 = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 7), input1, 3);

		/* Process the 4 bytes of input on each stream. */

		input0 = transition4(input0, flows.trans, &index_array[0]);
		input1 = transition4(input1, flows.trans, &index_array[4]);

		input0 = transition4(input0, flows.trans, &index_array[0]);
		input1 = transition4(input1, flows.trans, &index_array[4]);

		input0 = transition4(input0, flows.trans, &index_array[0]);
		input1 = transition4(input1, flows.trans, &index_array[4]);

		input0 = transition4(input0, flows.trans, &index_array[0]);
		input1 = transition4(input1, flows.trans, &index_array[4]);

		 /* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows, &index_array[0]);
		acl_match_check_x4(4, ctx, parms, &flows, &index_array[4]);
	}

	return 0;
}

/*
 * Execute trie traversal with 4 traversals in parallel
 */
static inline int
search_neon_4(const struct rte_acl_ctx *ctx, const uint8_t **data,
	      uint32_t *results, int total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[4];
	struct completion cmplt[4];
	struct parms parms[4];
	int32x4_t input;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		     total_packets, categories, ctx->trans_table);

	for (n = 0; n < 4; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows, index_array);

	while (flows.started > 0) {
		/* Gather 4 bytes of input data for each stream. */
		input = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 0), input, 0);
		input = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 1), input, 1);
		input = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 2), input, 2);
		input = vsetq_lane_s32(GET_NEXT_4BYTES(parms, 3), input, 3);

		/* Process the 4 bytes of input on each stream. */
		input = transition4(input, flows.trans, index_array);
		input = transition4(input, flows.trans, index_array);
		input = transition4(input, flows.trans, index_array);
		input = transition4(input, flows.trans, index_array);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows, index_array);
	}

	return 0;
}
