/*
 *   BSD LICENSE
 *
 *   Copyright (C) IBM Corporation 2016.
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
 *     * Neither the name of IBM Corporation nor the names of its
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

struct _altivec_acl_const {
	rte_xmm_t xmm_shuffle_input;
	rte_xmm_t xmm_index_mask;
	rte_xmm_t xmm_ones_16;
	rte_xmm_t range_base;
} altivec_acl_const  __attribute__((aligned(RTE_CACHE_LINE_SIZE))) = {
	{
		.u32 = {0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c}
	},
	{
		.u32 = {RTE_ACL_NODE_INDEX, RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX, RTE_ACL_NODE_INDEX}
	},
	{
		.u16 = {1, 1, 1, 1, 1, 1, 1, 1}
	},
	{
		.u32 = {0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c}
	},
};

/*
 * Resolve priority for multiple results (altivec version).
 * This consists comparing the priority of the current traversal with the
 * running set of results for the packet.
 * For each result, keep a running array of the result (rule number) and
 * its priority for each category.
 */
static inline void
resolve_priority_altivec(uint64_t transition, int n,
	const struct rte_acl_ctx *ctx, struct parms *parms,
	const struct rte_acl_match_results *p, uint32_t categories)
{
	uint32_t x;
	xmm_t results, priority, results1, priority1;
	vector bool int selector;
	xmm_t *saved_results, *saved_priority;

	for (x = 0; x < categories; x += RTE_ACL_RESULTS_MULTIPLIER) {

		saved_results = (xmm_t *)(&parms[n].cmplt->results[x]);
		saved_priority =
			(xmm_t *)(&parms[n].cmplt->priority[x]);

		/* get results and priorities for completed trie */
		results = *(const xmm_t *)&p[transition].results[x];
		priority = *(const xmm_t *)&p[transition].priority[x];

		/* if this is not the first completed trie */
		if (parms[n].cmplt->count != ctx->num_tries) {

			/* get running best results and their priorities */
			results1 = *saved_results;
			priority1 = *saved_priority;

			/* select results that are highest priority */
			selector = vec_cmpgt(priority1, priority);
			results = vec_sel(results, results1, selector);
			priority = vec_sel(priority, priority1,
				selector);
		}

		/* save running best results and their priorities */
		*saved_results = results;
		*saved_priority = priority;
	}
}

/*
 * Check for any match in 4 transitions
 */
static inline __attribute__((always_inline)) uint32_t
check_any_match_x4(uint64_t val[])
{
	return (val[0] | val[1] | val[2] | val[3]) & RTE_ACL_NODE_MATCH;
}

static inline __attribute__((always_inline)) void
acl_match_check_x4(int slot, const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, uint64_t transitions[])
{
	while (check_any_match_x4(transitions)) {
		transitions[0] = acl_match_check(transitions[0], slot, ctx,
			parms, flows, resolve_priority_altivec);
		transitions[1] = acl_match_check(transitions[1], slot + 1, ctx,
			parms, flows, resolve_priority_altivec);
		transitions[2] = acl_match_check(transitions[2], slot + 2, ctx,
			parms, flows, resolve_priority_altivec);
		transitions[3] = acl_match_check(transitions[3], slot + 3, ctx,
			parms, flows, resolve_priority_altivec);
	}
}

/*
 * Process 4 transitions (in 2 XMM registers) in parallel
 */
static inline __attribute__((optimize("O2"))) xmm_t
transition4(xmm_t next_input, const uint64_t *trans,
	xmm_t *indices1, xmm_t *indices2)
{
	xmm_t addr, tr_lo, tr_hi;
	xmm_t in, node_type, r, t;
	xmm_t dfa_ofs, quad_ofs;
	xmm_t *index_mask, *tp;
	vector bool int dfa_msk;
	vector signed char zeroes = {};
	union {
		uint64_t d64[2];
		uint32_t d32[4];
	} v;

	/* Move low 32 into tr_lo and high 32 into tr_hi */
	tr_lo = (xmm_t){(*indices1)[0], (*indices1)[2],
			(*indices2)[0], (*indices2)[2]};
	tr_hi = (xmm_t){(*indices1)[1], (*indices1)[3],
			(*indices2)[1], (*indices2)[3]};

	 /* Calculate the address (array index) for all 4 transitions. */
	index_mask = (xmm_t *)&altivec_acl_const.xmm_index_mask.u32;
	t = vec_xor(*index_mask, *index_mask);
	in = vec_perm(next_input, (xmm_t){},
		*(vector unsigned char *)&altivec_acl_const.xmm_shuffle_input);

	/* Calc node type and node addr */
	node_type = vec_and(vec_nor(*index_mask, *index_mask), tr_lo);
	addr = vec_and(tr_lo, *index_mask);

	/* mask for DFA type(0) nodes */
	dfa_msk = vec_cmpeq(node_type, t);

	/* DFA calculations. */
	r = vec_sr(in, (vector unsigned int){30, 30, 30, 30});
	tp = (xmm_t *)&altivec_acl_const.range_base.u32;
	r = vec_add(r, *tp);
	t = vec_sr(in, (vector unsigned int){24, 24, 24, 24});
	r = vec_perm(tr_hi, (xmm_t){(uint16_t)0 << 16},
		(vector unsigned char)r);

	dfa_ofs = vec_sub(t, r);

	/* QUAD/SINGLE caluclations. */
	t = (xmm_t)vec_cmpgt((vector signed char)in, (vector signed char)tr_hi);
	t = (xmm_t)vec_sel(
		vec_sel(
			(vector signed char)vec_sub(
				zeroes, (vector signed char)t),
			(vector signed char)t,
			vec_cmpgt((vector signed char)t, zeroes)),
		zeroes,
		vec_cmpeq((vector signed char)t, zeroes));

	t = (xmm_t)vec_msum((vector signed char)t,
		(vector unsigned char)t, (xmm_t){});
	quad_ofs = (xmm_t)vec_msum((vector signed short)t,
		*(vector signed short *)&altivec_acl_const.xmm_ones_16.u16,
		(xmm_t){});

	/* blend DFA and QUAD/SINGLE. */
	t = vec_sel(quad_ofs, dfa_ofs, dfa_msk);

	/* calculate address for next transitions. */
	addr = vec_add(addr, t);

	v.d64[0] = (uint64_t)trans[addr[0]];
	v.d64[1] = (uint64_t)trans[addr[1]];
	*indices1 = (xmm_t){v.d32[0], v.d32[1], v.d32[2], v.d32[3]};
	v.d64[0] = (uint64_t)trans[addr[2]];
	v.d64[1] = (uint64_t)trans[addr[3]];
	*indices2 = (xmm_t){v.d32[0], v.d32[1], v.d32[2], v.d32[3]};

	return vec_sr(next_input,
		(vector unsigned int){CHAR_BIT, CHAR_BIT, CHAR_BIT, CHAR_BIT});
}

/*
 * Execute trie traversal with 8 traversals in parallel
 */
static inline int
search_altivec_8(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_ALTIVEC8];
	struct completion cmplt[MAX_SEARCHES_ALTIVEC8];
	struct parms parms[MAX_SEARCHES_ALTIVEC8];
	xmm_t input0, input1;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_ALTIVEC8; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	 /* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows, (uint64_t *)&index_array[0]);
	acl_match_check_x4(4, ctx, parms, &flows, (uint64_t *)&index_array[4]);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input0 = (xmm_t){GET_NEXT_4BYTES(parms, 0),
				GET_NEXT_4BYTES(parms, 1),
				GET_NEXT_4BYTES(parms, 2),
				GET_NEXT_4BYTES(parms, 3)};

		input1 = (xmm_t){GET_NEXT_4BYTES(parms, 4),
				GET_NEXT_4BYTES(parms, 5),
				GET_NEXT_4BYTES(parms, 6),
				GET_NEXT_4BYTES(parms, 7)};

		 /* Process the 4 bytes of input on each stream. */

		input0 = transition4(input0, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input1 = transition4(input1, flows.trans,
			(xmm_t *)&index_array[4], (xmm_t *)&index_array[6]);

		input0 = transition4(input0, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input1 = transition4(input1, flows.trans,
			(xmm_t *)&index_array[4], (xmm_t *)&index_array[6]);

		input0 = transition4(input0, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input1 = transition4(input1, flows.trans,
			(xmm_t *)&index_array[4], (xmm_t *)&index_array[6]);

		input0 = transition4(input0, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input1 = transition4(input1, flows.trans,
			(xmm_t *)&index_array[4], (xmm_t *)&index_array[6]);

		 /* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			(uint64_t *)&index_array[0]);
		acl_match_check_x4(4, ctx, parms, &flows,
			(uint64_t *)&index_array[4]);
	}

	return 0;
}

/*
 * Execute trie traversal with 4 traversals in parallel
 */
static inline int
search_altivec_4(const struct rte_acl_ctx *ctx, const uint8_t **data,
	 uint32_t *results, int total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_ALTIVEC4];
	struct completion cmplt[MAX_SEARCHES_ALTIVEC4];
	struct parms parms[MAX_SEARCHES_ALTIVEC4];
	xmm_t input;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_ALTIVEC4; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows, index_array);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input = (xmm_t){GET_NEXT_4BYTES(parms, 0),
				GET_NEXT_4BYTES(parms, 1),
				GET_NEXT_4BYTES(parms, 2),
				GET_NEXT_4BYTES(parms, 3)};

		/* Process the 4 bytes of input on each stream. */
		input = transition4(input, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input = transition4(input, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input = transition4(input, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);
		input = transition4(input, flows.trans,
			(xmm_t *)&index_array[0], (xmm_t *)&index_array[2]);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows, index_array);
	}

	return 0;
}
