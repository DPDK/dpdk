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
#include "acl_vect.h"

enum {
	SHUFFLE32_SLOT1 = 0xe5,
	SHUFFLE32_SLOT2 = 0xe6,
	SHUFFLE32_SLOT3 = 0xe7,
	SHUFFLE32_SWAP64 = 0x4e,
};

static const rte_xmm_t xmm_shuffle_input = {
	.u32 = {0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c},
};

static const rte_xmm_t xmm_ones_16 = {
	.u16 = {1, 1, 1, 1, 1, 1, 1, 1},
};

static const rte_xmm_t xmm_match_mask = {
	.u32 = {
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
	},
};

static const rte_xmm_t xmm_index_mask = {
	.u32 = {
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
	},
};

static const rte_xmm_t xmm_range_base = {
	.u32 = {
		0xffffff00, 0xffffff04, 0xffffff08, 0xffffff0c,
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
		results = _mm_loadu_si128(
			(const xmm_t *)&p[transition].results[x]);
		priority = _mm_loadu_si128(
			(const xmm_t *)&p[transition].priority[x]);

		/* if this is not the first completed trie */
		if (parms[n].cmplt->count != ctx->num_tries) {

			/* get running best results and their priorities */
			results1 = _mm_loadu_si128(saved_results);
			priority1 = _mm_loadu_si128(saved_priority);

			/* select results that are highest priority */
			selector = _mm_cmpgt_epi32(priority1, priority);
			results = _mm_blendv_epi8(results, results1, selector);
			priority = _mm_blendv_epi8(priority, priority1,
				selector);
		}

		/* save running best results and their priorities */
		_mm_storeu_si128(saved_results, results);
		_mm_storeu_si128(saved_priority, priority);
	}
}

/*
 * Extract transitions from an XMM register and check for any matches
 */
static void
acl_process_matches(xmm_t *indices, int slot, const struct rte_acl_ctx *ctx,
	struct parms *parms, struct acl_flow_data *flows)
{
	uint64_t transition1, transition2;

	/* extract transition from low 64 bits. */
	transition1 = _mm_cvtsi128_si64(*indices);

	/* extract transition from high 64 bits. */
	*indices = _mm_shuffle_epi32(*indices, SHUFFLE32_SWAP64);
	transition2 = _mm_cvtsi128_si64(*indices);

	transition1 = acl_match_check(transition1, slot, ctx,
		parms, flows, resolve_priority_sse);
	transition2 = acl_match_check(transition2, slot + 1, ctx,
		parms, flows, resolve_priority_sse);

	/* update indices with new transitions. */
	*indices = _mm_set_epi64x(transition2, transition1);
}

/*
 * Check for any match in 4 transitions (contained in 2 SSE registers)
 */
static inline __attribute__((always_inline)) void
acl_match_check_x4(int slot, const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, xmm_t *indices1, xmm_t *indices2,
	xmm_t match_mask)
{
	xmm_t temp;

	/* put low 32 bits of each transition into one register */
	temp = (xmm_t)_mm_shuffle_ps((__m128)*indices1, (__m128)*indices2,
		0x88);
	/* test for match node */
	temp = _mm_and_si128(match_mask, temp);

	while (!_mm_testz_si128(temp, temp)) {
		acl_process_matches(indices1, slot, ctx, parms, flows);
		acl_process_matches(indices2, slot + 2, ctx, parms, flows);

		temp = (xmm_t)_mm_shuffle_ps((__m128)*indices1,
					(__m128)*indices2,
					0x88);
		temp = _mm_and_si128(match_mask, temp);
	}
}

/*
 * Process 4 transitions (in 2 XMM registers) in parallel
 */
static inline __attribute__((always_inline)) xmm_t
transition4(xmm_t next_input, const uint64_t *trans,
	xmm_t *indices1, xmm_t *indices2)
{
	xmm_t addr, tr_lo, tr_hi;
	uint64_t trans0, trans2;

	/* Shuffle low 32 into tr_lo and high 32 into tr_hi */
	ACL_TR_HILO(mm, __m128, *indices1, *indices2, tr_lo, tr_hi);

	 /* Calculate the address (array index) for all 4 transitions. */
	ACL_TR_CALC_ADDR(mm, 128, addr, xmm_index_mask.x, next_input,
		xmm_shuffle_input.x, xmm_ones_16.x, xmm_range_base.x,
		tr_lo, tr_hi);

	 /* Gather 64 bit transitions and pack back into 2 registers. */

	trans0 = trans[_mm_cvtsi128_si32(addr)];

	/* get slot 2 */

	/* {x0, x1, x2, x3} -> {x2, x1, x2, x3} */
	addr = _mm_shuffle_epi32(addr, SHUFFLE32_SLOT2);
	trans2 = trans[_mm_cvtsi128_si32(addr)];

	/* get slot 1 */

	/* {x2, x1, x2, x3} -> {x1, x1, x2, x3} */
	addr = _mm_shuffle_epi32(addr, SHUFFLE32_SLOT1);
	*indices1 = _mm_set_epi64x(trans[_mm_cvtsi128_si32(addr)], trans0);

	/* get slot 3 */

	/* {x1, x1, x2, x3} -> {x3, x1, x2, x3} */
	addr = _mm_shuffle_epi32(addr, SHUFFLE32_SLOT3);
	*indices2 = _mm_set_epi64x(trans[_mm_cvtsi128_si32(addr)], trans2);

	return _mm_srli_epi32(next_input, CHAR_BIT);
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
	xmm_t indices1, indices2, indices3, indices4;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_SSE8; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	/*
	 * indices1 contains index_array[0,1]
	 * indices2 contains index_array[2,3]
	 * indices3 contains index_array[4,5]
	 * indices4 contains index_array[6,7]
	 */

	indices1 = _mm_loadu_si128((xmm_t *) &index_array[0]);
	indices2 = _mm_loadu_si128((xmm_t *) &index_array[2]);

	indices3 = _mm_loadu_si128((xmm_t *) &index_array[4]);
	indices4 = _mm_loadu_si128((xmm_t *) &index_array[6]);

	 /* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&indices1, &indices2, xmm_match_mask.x);
	acl_match_check_x4(4, ctx, parms, &flows,
		&indices3, &indices4, xmm_match_mask.x);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input0 = _mm_cvtsi32_si128(GET_NEXT_4BYTES(parms, 0));
		input1 = _mm_cvtsi32_si128(GET_NEXT_4BYTES(parms, 4));

		input0 = _mm_insert_epi32(input0, GET_NEXT_4BYTES(parms, 1), 1);
		input1 = _mm_insert_epi32(input1, GET_NEXT_4BYTES(parms, 5), 1);

		input0 = _mm_insert_epi32(input0, GET_NEXT_4BYTES(parms, 2), 2);
		input1 = _mm_insert_epi32(input1, GET_NEXT_4BYTES(parms, 6), 2);

		input0 = _mm_insert_epi32(input0, GET_NEXT_4BYTES(parms, 3), 3);
		input1 = _mm_insert_epi32(input1, GET_NEXT_4BYTES(parms, 7), 3);

		 /* Process the 4 bytes of input on each stream. */

		input0 = transition4(input0, flows.trans,
			&indices1, &indices2);
		input1 = transition4(input1, flows.trans,
			&indices3, &indices4);

		input0 = transition4(input0, flows.trans,
			&indices1, &indices2);
		input1 = transition4(input1, flows.trans,
			&indices3, &indices4);

		input0 = transition4(input0, flows.trans,
			&indices1, &indices2);
		input1 = transition4(input1, flows.trans,
			&indices3, &indices4);

		input0 = transition4(input0, flows.trans,
			&indices1, &indices2);
		input1 = transition4(input1, flows.trans,
			&indices3, &indices4);

		 /* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&indices1, &indices2, xmm_match_mask.x);
		acl_match_check_x4(4, ctx, parms, &flows,
			&indices3, &indices4, xmm_match_mask.x);
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
	xmm_t input, indices1, indices2;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < MAX_SEARCHES_SSE4; n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	indices1 = _mm_loadu_si128((xmm_t *) &index_array[0]);
	indices2 = _mm_loadu_si128((xmm_t *) &index_array[2]);

	/* Check for any matches. */
	acl_match_check_x4(0, ctx, parms, &flows,
		&indices1, &indices2, xmm_match_mask.x);

	while (flows.started > 0) {

		/* Gather 4 bytes of input data for each stream. */
		input = _mm_cvtsi32_si128(GET_NEXT_4BYTES(parms, 0));
		input = _mm_insert_epi32(input, GET_NEXT_4BYTES(parms, 1), 1);
		input = _mm_insert_epi32(input, GET_NEXT_4BYTES(parms, 2), 2);
		input = _mm_insert_epi32(input, GET_NEXT_4BYTES(parms, 3), 3);

		/* Process the 4 bytes of input on each stream. */
		input = transition4(input, flows.trans, &indices1, &indices2);
		input = transition4(input, flows.trans, &indices1, &indices2);
		input = transition4(input, flows.trans, &indices1, &indices2);
		input = transition4(input, flows.trans, &indices1, &indices2);

		/* Check for any matches. */
		acl_match_check_x4(0, ctx, parms, &flows,
			&indices1, &indices2, xmm_match_mask.x);
	}

	return 0;
}
