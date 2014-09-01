/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

enum {
	SHUFFLE32_SLOT1 = 0xe5,
	SHUFFLE32_SLOT2 = 0xe6,
	SHUFFLE32_SLOT3 = 0xe7,
	SHUFFLE32_SWAP64 = 0x4e,
};

static const rte_xmm_t mm_type_quad_range = {
	.u32 = {
		RTE_ACL_NODE_QRANGE,
		RTE_ACL_NODE_QRANGE,
		RTE_ACL_NODE_QRANGE,
		RTE_ACL_NODE_QRANGE,
	},
};

static const rte_xmm_t mm_type_quad_range64 = {
	.u32 = {
		RTE_ACL_NODE_QRANGE,
		RTE_ACL_NODE_QRANGE,
		0,
		0,
	},
};

static const rte_xmm_t mm_shuffle_input = {
	.u32 = {0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c},
};

static const rte_xmm_t mm_shuffle_input64 = {
	.u32 = {0x00000000, 0x04040404, 0x80808080, 0x80808080},
};

static const rte_xmm_t mm_ones_16 = {
	.u16 = {1, 1, 1, 1, 1, 1, 1, 1},
};

static const rte_xmm_t mm_bytes = {
	.u32 = {UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX},
};

static const rte_xmm_t mm_bytes64 = {
	.u32 = {UINT8_MAX, UINT8_MAX, 0, 0},
};

static const rte_xmm_t mm_match_mask = {
	.u32 = {
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
	},
};

static const rte_xmm_t mm_match_mask64 = {
	.u32 = {
		RTE_ACL_NODE_MATCH,
		0,
		RTE_ACL_NODE_MATCH,
		0,
	},
};

static const rte_xmm_t mm_index_mask = {
	.u32 = {
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
	},
};

static const rte_xmm_t mm_index_mask64 = {
	.u32 = {
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		0,
		0,
	},
};


/*
 * Resolve priority for multiple results (sse version).
 * This consists comparing the priority of the current traversal with the
 * running set of results for the packet.
 * For each result, keep a running array of the result (rule number) and
 * its priority for each category.
 */
static inline void
resolve_priority_sse(uint64_t transition, int n, const struct rte_acl_ctx *ctx,
	struct parms *parms, const struct rte_acl_match_results *p,
	uint32_t categories)
{
	uint32_t x;
	xmm_t results, priority, results1, priority1, selector;
	xmm_t *saved_results, *saved_priority;

	for (x = 0; x < categories; x += RTE_ACL_RESULTS_MULTIPLIER) {

		saved_results = (xmm_t *)(&parms[n].cmplt->results[x]);
		saved_priority =
			(xmm_t *)(&parms[n].cmplt->priority[x]);

		/* get results and priorities for completed trie */
		results = MM_LOADU((const xmm_t *)&p[transition].results[x]);
		priority = MM_LOADU((const xmm_t *)&p[transition].priority[x]);

		/* if this is not the first completed trie */
		if (parms[n].cmplt->count != ctx->num_tries) {

			/* get running best results and their priorities */
			results1 = MM_LOADU(saved_results);
			priority1 = MM_LOADU(saved_priority);

			/* select results that are highest priority */
			selector = MM_CMPGT32(priority1, priority);
			results = MM_BLENDV8(results, results1, selector);
			priority = MM_BLENDV8(priority, priority1, selector);
		}

		/* save running best results and their priorities */
		MM_STOREU(saved_results, results);
		MM_STOREU(saved_priority, priority);
	}
}

/*
 * Extract transitions from an XMM register and check for any matches
 */
static void
acl_process_matches(xmm_t *indicies, int slot, const struct rte_acl_ctx *ctx,
	struct parms *parms, struct acl_flow_data *flows)
{
	uint64_t transition1, transition2;

	/* extract transition from low 64 bits. */
	transition1 = MM_CVT64(*indicies);

	/* extract transition from high 64 bits. */
	*indicies = MM_SHUFFLE32(*indicies, SHUFFLE32_SWAP64);
	transition2 = MM_CVT64(*indicies);

	transition1 = acl_match_check(transition1, slot, ctx,
		parms, flows, resolve_priority_sse);
	transition2 = acl_match_check(transition2, slot + 1, ctx,
		parms, flows, resolve_priority_sse);

	/* update indicies with new transitions. */
	*indicies = MM_SET64(transition2, transition1);
}

/*
 * Check for a match in 2 transitions (contained in SSE register)
 */
static inline void
acl_match_check_x2(int slot, const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, xmm_t *indicies, xmm_t match_mask)
{
	xmm_t temp;

	temp = MM_AND(match_mask, *indicies);
	while (!MM_TESTZ(temp, temp)) {
		acl_process_matches(indicies, slot, ctx, parms, flows);
		temp = MM_AND(match_mask, *indicies);
	}
}

/*
 * Check for any match in 4 transitions (contained in 2 SSE registers)
 */
static inline void
acl_match_check_x4(int slot, const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, xmm_t *indicies1, xmm_t *indicies2,
	xmm_t match_mask)
{
	xmm_t temp;

	/* put low 32 bits of each transition into one register */
	temp = (xmm_t)MM_SHUFFLEPS((__m128)*indicies1, (__m128)*indicies2,
		0x88);
	/* test for match node */
	temp = MM_AND(match_mask, temp);

	while (!MM_TESTZ(temp, temp)) {
		acl_process_matches(indicies1, slot, ctx, parms, flows);
		acl_process_matches(indicies2, slot + 2, ctx, parms, flows);

		temp = (xmm_t)MM_SHUFFLEPS((__m128)*indicies1,
					(__m128)*indicies2,
					0x88);
		temp = MM_AND(match_mask, temp);
	}
}

/*
 * Calculate the address of the next transition for
 * all types of nodes. Note that only DFA nodes and range
 * nodes actually transition to another node. Match
 * nodes don't move.
 */
static inline xmm_t
acl_calc_addr(xmm_t index_mask, xmm_t next_input, xmm_t shuffle_input,
	xmm_t ones_16, xmm_t bytes, xmm_t type_quad_range,
	xmm_t *indicies1, xmm_t *indicies2)
{
	xmm_t addr, node_types, temp;

	/*
	 * Note that no transition is done for a match
	 * node and therefore a stream freezes when
	 * it reaches a match.
	 */

	/* Shuffle low 32 into temp and high 32 into indicies2 */
	temp = (xmm_t)MM_SHUFFLEPS((__m128)*indicies1, (__m128)*indicies2,
		0x88);
	*indicies2 = (xmm_t)MM_SHUFFLEPS((__m128)*indicies1,
		(__m128)*indicies2, 0xdd);

	/* Calc node type and node addr */
	node_types = MM_ANDNOT(index_mask, temp);
	addr = MM_AND(index_mask, temp);

	/*
	 * Calc addr for DFAs - addr = dfa_index + input_byte
	 */

	/* mask for DFA type (0) nodes */
	temp = MM_CMPEQ32(node_types, MM_XOR(node_types, node_types));

	/* add input byte to DFA position */
	temp = MM_AND(temp, bytes);
	temp = MM_AND(temp, next_input);
	addr = MM_ADD32(addr, temp);

	/*
	 * Calc addr for Range nodes -> range_index + range(input)
	 */
	node_types = MM_CMPEQ32(node_types, type_quad_range);

	/*
	 * Calculate number of range boundaries that are less than the
	 * input value. Range boundaries for each node are in signed 8 bit,
	 * ordered from -128 to 127 in the indicies2 register.
	 * This is effectively a popcnt of bytes that are greater than the
	 * input byte.
	 */

	/* shuffle input byte to all 4 positions of 32 bit value */
	temp = MM_SHUFFLE8(next_input, shuffle_input);

	/* check ranges */
	temp = MM_CMPGT8(temp, *indicies2);

	/* convert -1 to 1 (bytes greater than input byte */
	temp = MM_SIGN8(temp, temp);

	/* horizontal add pairs of bytes into words */
	temp = MM_MADD8(temp, temp);

	/* horizontal add pairs of words into dwords */
	temp = MM_MADD16(temp, ones_16);

	/* mask to range type nodes */
	temp = MM_AND(temp, node_types);

	/* add index into node position */
	return MM_ADD32(addr, temp);
}

/*
 * Process 4 transitions (in 2 SIMD registers) in parallel
 */
static inline xmm_t
transition4(xmm_t index_mask, xmm_t next_input, xmm_t shuffle_input,
	xmm_t ones_16, xmm_t bytes, xmm_t type_quad_range,
	const uint64_t *trans, xmm_t *indicies1, xmm_t *indicies2)
{
	xmm_t addr;
	uint64_t trans0, trans2;

	 /* Calculate the address (array index) for all 4 transitions. */

	addr = acl_calc_addr(index_mask, next_input, shuffle_input, ones_16,
		bytes, type_quad_range, indicies1, indicies2);

	 /* Gather 64 bit transitions and pack back into 2 registers. */

	trans0 = trans[MM_CVT32(addr)];

	/* get slot 2 */

	/* {x0, x1, x2, x3} -> {x2, x1, x2, x3} */
	addr = MM_SHUFFLE32(addr, SHUFFLE32_SLOT2);
	trans2 = trans[MM_CVT32(addr)];

	/* get slot 1 */

	/* {x2, x1, x2, x3} -> {x1, x1, x2, x3} */
	addr = MM_SHUFFLE32(addr, SHUFFLE32_SLOT1);
	*indicies1 = MM_SET64(trans[MM_CVT32(addr)], trans0);

	/* get slot 3 */

	/* {x1, x1, x2, x3} -> {x3, x1, x2, x3} */
	addr = MM_SHUFFLE32(addr, SHUFFLE32_SLOT3);
	*indicies2 = MM_SET64(trans[MM_CVT32(addr)], trans2);

	return MM_SRL32(next_input, 8);
}

/*
 * Execute trie traversal with 8 traversals in parallel
 */
static inline int
search_sse_8(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_SSE8];
	struct completion cmplt[MAX_SEARCHES_SSE8];
	struct parms parms[MAX_SEARCHES_SSE8];
	xmm_t input0, input1;
	xmm_t indicies1, indicies2, indicies3, indicies4;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_SSE8; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	/*
	 * indicies1 contains index_array[0,1]
	 * indicies2 contains index_array[2,3]
	 * indicies3 contains index_array[4,5]
	 * indicies4 contains index_array[6,7]
	 */

	indicies1 = MM_LOADU((xmm_t *) &index_array[0]);
	indicies2 = MM_LOADU((xmm_t *) &index_array[2]);

	indicies3 = MM_LOADU((xmm_t *) &index_array[4]);
	indicies4 = MM_LOADU((xmm_t *) &index_array[6]);

	 /* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&indicies1, &indicies2, mm_match_mask.m);
	acl_match_check_x4(4, ctx, parms, &flows,
		&indicies3, &indicies4, mm_match_mask.m);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input0 = MM_INSERT32(mm_ones_16.m, GET_NEXT_4BYTES(parms, 0),
			0);
		input1 = MM_INSERT32(mm_ones_16.m, GET_NEXT_4BYTES(parms, 4),
			0);

		input0 = MM_INSERT32(input0, GET_NEXT_4BYTES(parms, 1), 1);
		input1 = MM_INSERT32(input1, GET_NEXT_4BYTES(parms, 5), 1);

		input0 = MM_INSERT32(input0, GET_NEXT_4BYTES(parms, 2), 2);
		input1 = MM_INSERT32(input1, GET_NEXT_4BYTES(parms, 6), 2);

		input0 = MM_INSERT32(input0, GET_NEXT_4BYTES(parms, 3), 3);
		input1 = MM_INSERT32(input1, GET_NEXT_4BYTES(parms, 7), 3);

		 /* Process the 4 bytes of input on each stream. */

		input0 = transition4(mm_index_mask.m, input0,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		input1 = transition4(mm_index_mask.m, input1,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies3, &indicies4);

		input0 = transition4(mm_index_mask.m, input0,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		input1 = transition4(mm_index_mask.m, input1,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies3, &indicies4);

		input0 = transition4(mm_index_mask.m, input0,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		input1 = transition4(mm_index_mask.m, input1,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies3, &indicies4);

		input0 = transition4(mm_index_mask.m, input0,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		input1 = transition4(mm_index_mask.m, input1,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies3, &indicies4);

		 /* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&indicies1, &indicies2, mm_match_mask.m);
		acl_match_check_x4(4, ctx, parms, &flows,
			&indicies3, &indicies4, mm_match_mask.m);
	}

	return 0;
}

/*
 * Execute trie traversal with 4 traversals in parallel
 */
static inline int
search_sse_4(const struct rte_acl_ctx *ctx, const uint8_t **data,
	 uint32_t *results, int total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_SSE4];
	struct completion cmplt[MAX_SEARCHES_SSE4];
	struct parms parms[MAX_SEARCHES_SSE4];
	xmm_t input, indicies1, indicies2;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_SSE4; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	indicies1 = MM_LOADU((xmm_t *) &index_array[0]);
	indicies2 = MM_LOADU((xmm_t *) &index_array[2]);

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&indicies1, &indicies2, mm_match_mask.m);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input = MM_INSERT32(mm_ones_16.m, GET_NEXT_4BYTES(parms, 0), 0);
		input = MM_INSERT32(input, GET_NEXT_4BYTES(parms, 1), 1);
		input = MM_INSERT32(input, GET_NEXT_4BYTES(parms, 2), 2);
		input = MM_INSERT32(input, GET_NEXT_4BYTES(parms, 3), 3);

		/* Process the 4 bytes of input on each stream. */
		input = transition4(mm_index_mask.m, input,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		 input = transition4(mm_index_mask.m, input,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		 input = transition4(mm_index_mask.m, input,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		 input = transition4(mm_index_mask.m, input,
			mm_shuffle_input.m, mm_ones_16.m,
			mm_bytes.m, mm_type_quad_range.m,
			flows.trans, &indicies1, &indicies2);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&indicies1, &indicies2, mm_match_mask.m);
	}

	return 0;
}

static inline xmm_t
transition2(xmm_t index_mask, xmm_t next_input, xmm_t shuffle_input,
	xmm_t ones_16, xmm_t bytes, xmm_t type_quad_range,
	const uint64_t *trans, xmm_t *indicies1)
{
	uint64_t t;
	xmm_t addr, indicies2;

	indicies2 = MM_XOR(ones_16, ones_16);

	addr = acl_calc_addr(index_mask, next_input, shuffle_input, ones_16,
		bytes, type_quad_range, indicies1, &indicies2);

	/* Gather 64 bit transitions and pack 2 per register. */

	t = trans[MM_CVT32(addr)];

	/* get slot 1 */
	addr = MM_SHUFFLE32(addr, SHUFFLE32_SLOT1);
	*indicies1 = MM_SET64(trans[MM_CVT32(addr)], t);

	return MM_SRL32(next_input, 8);
}

/*
 * Execute trie traversal with 2 traversals in parallel.
 */
static inline int
search_sse_2(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	int n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_SSE2];
	struct completion cmplt[MAX_SEARCHES_SSE2];
	struct parms parms[MAX_SEARCHES_SSE2];
	xmm_t input, indicies;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_SSE2; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	indicies = MM_LOADU((xmm_t *) &index_array[0]);

	/* Check for any matches. */
	acl_match_check_x2(0, ctx, parms, &flows, &indicies, mm_match_mask64.m);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input = MM_INSERT32(mm_ones_16.m, GET_NEXT_4BYTES(parms, 0), 0);
		input = MM_INSERT32(input, GET_NEXT_4BYTES(parms, 1), 1);

		/* Process the 4 bytes of input on each stream. */

		input = transition2(mm_index_mask64.m, input,
			mm_shuffle_input64.m, mm_ones_16.m,
			mm_bytes64.m, mm_type_quad_range64.m,
			flows.trans, &indicies);

		input = transition2(mm_index_mask64.m, input,
			mm_shuffle_input64.m, mm_ones_16.m,
			mm_bytes64.m, mm_type_quad_range64.m,
			flows.trans, &indicies);

		input = transition2(mm_index_mask64.m, input,
			mm_shuffle_input64.m, mm_ones_16.m,
			mm_bytes64.m, mm_type_quad_range64.m,
			flows.trans, &indicies);

		input = transition2(mm_index_mask64.m, input,
			mm_shuffle_input64.m, mm_ones_16.m,
			mm_bytes64.m, mm_type_quad_range64.m,
			flows.trans, &indicies);

		/* Check for any matches. */
		acl_match_check_x2(0, ctx, parms, &flows, &indicies,
			mm_match_mask64.m);
	}

	return 0;
}

int
rte_acl_classify_sse(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	if (categories != 1 &&
		((RTE_ACL_RESULTS_MULTIPLIER - 1) & categories) != 0)
		return -EINVAL;

	if (likely(num >= MAX_SEARCHES_SSE8))
		return search_sse_8(ctx, data, results, num, categories);
	else if (num >= MAX_SEARCHES_SSE4)
		return search_sse_4(ctx, data, results, num, categories);
	else
		return search_sse_2(ctx, data, results, num, categories);
}
