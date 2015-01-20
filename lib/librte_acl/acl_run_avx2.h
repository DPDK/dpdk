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

#include "acl_run_sse.h"

static const rte_ymm_t ymm_match_mask = {
	.u32 = {
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
		RTE_ACL_NODE_MATCH,
	},
};

static const rte_ymm_t ymm_index_mask = {
	.u32 = {
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
		RTE_ACL_NODE_INDEX,
	},
};

static const rte_ymm_t ymm_shuffle_input = {
	.u32 = {
		0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c,
		0x00000000, 0x04040404, 0x08080808, 0x0c0c0c0c,
	},
};

static const rte_ymm_t ymm_ones_16 = {
	.u16 = {
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
	},
};

static inline __attribute__((always_inline)) ymm_t
calc_addr_avx2(ymm_t index_mask, ymm_t next_input, ymm_t shuffle_input,
	ymm_t ones_16, ymm_t tr_lo, ymm_t tr_hi)
{
	ymm_t in, node_type, r, t;
	ymm_t dfa_msk, dfa_ofs, quad_ofs;
	ymm_t addr;

	const ymm_t range_base = _mm256_set_epi32(
		0xffffff0c, 0xffffff08, 0xffffff04, 0xffffff00,
		0xffffff0c, 0xffffff08, 0xffffff04, 0xffffff00);

	t = _mm256_xor_si256(index_mask, index_mask);
	in = _mm256_shuffle_epi8(next_input, shuffle_input);

	/* Calc node type and node addr */
	node_type = _mm256_andnot_si256(index_mask, tr_lo);
	addr = _mm256_and_si256(index_mask, tr_lo);

	/* DFA calculations. */

	dfa_msk = _mm256_cmpeq_epi32(node_type, t);

	r = _mm256_srli_epi32(in, 30);
	r = _mm256_add_epi8(r, range_base);

	t = _mm256_srli_epi32(in, 24);
	r = _mm256_shuffle_epi8(tr_hi, r);

	dfa_ofs = _mm256_sub_epi32(t, r);

	/* QUAD/SINGLE caluclations. */

	t = _mm256_cmpgt_epi8(in, tr_hi);
	t = _mm256_sign_epi8(t, t);
	t = _mm256_maddubs_epi16(t, t);
	quad_ofs = _mm256_madd_epi16(t, ones_16);

	/* blend DFA and QUAD/SINGLE. */
	t = _mm256_blendv_epi8(quad_ofs, dfa_ofs, dfa_msk);

	addr = _mm256_add_epi32(addr, t);
	return addr;
}

static inline __attribute__((always_inline)) ymm_t
transition8(ymm_t next_input, const uint64_t *trans, ymm_t *tr_lo, ymm_t *tr_hi)
{
	const int32_t *tr;
	ymm_t addr;

	tr = (const int32_t *)(uintptr_t)trans;

	addr = calc_addr_avx2(ymm_index_mask.y, next_input, ymm_shuffle_input.y,
		ymm_ones_16.y, *tr_lo, *tr_hi);

	/* load lower 32 bits of 8 transactions at once. */
	*tr_lo = _mm256_i32gather_epi32(tr, addr, sizeof(trans[0]));

	next_input = _mm256_srli_epi32(next_input, CHAR_BIT);

	/* load high 32 bits of 8 transactions at once. */
	*tr_hi = _mm256_i32gather_epi32(tr + 1, addr, sizeof(trans[0]));

	return next_input;
}

static inline void
acl_process_matches_avx2x8(const struct rte_acl_ctx *ctx,
	struct parms *parms, struct acl_flow_data *flows, uint32_t slot,
	ymm_t matches, ymm_t *tr_lo, ymm_t *tr_hi)
{
	ymm_t t0, t1;
	ymm_t lo, hi;
	xmm_t l0, l1;
	uint32_t i;
	uint64_t tr[MAX_SEARCHES_SSE8];

	l1 = _mm256_extracti128_si256(*tr_lo, 1);
	l0 = _mm256_castsi256_si128(*tr_lo);

	for (i = 0; i != RTE_DIM(tr) / 2; i++) {
		tr[i] = (uint32_t)_mm_cvtsi128_si32(l0);
		tr[i + 4] = (uint32_t)_mm_cvtsi128_si32(l1);

		l0 = _mm_srli_si128(l0, sizeof(uint32_t));
		l1 = _mm_srli_si128(l1, sizeof(uint32_t));

		tr[i] = acl_match_check(tr[i], slot + i,
			ctx, parms, flows, resolve_priority_sse);
		tr[i + 4] = acl_match_check(tr[i + 4], slot + i + 4,
			ctx, parms, flows, resolve_priority_sse);
	}

	t0 = _mm256_set_epi64x(tr[5], tr[4], tr[1], tr[0]);
	t1 = _mm256_set_epi64x(tr[7], tr[6], tr[3], tr[2]);

	lo = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0x88);
	hi = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0xdd);

	*tr_lo = _mm256_blendv_epi8(*tr_lo, lo, matches);
	*tr_hi = _mm256_blendv_epi8(*tr_hi, hi, matches);
}

static inline void
acl_match_check_avx2x8(const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, uint32_t slot,
	ymm_t *tr_lo, ymm_t *tr_hi, ymm_t match_mask)
{
	uint32_t msk;
	ymm_t matches, temp;

	/* test for match node */
	temp = _mm256_and_si256(match_mask, *tr_lo);
	matches = _mm256_cmpeq_epi32(temp, match_mask);
	msk = _mm256_movemask_epi8(matches);

	while (msk != 0) {

		acl_process_matches_avx2x8(ctx, parms, flows, slot,
			matches, tr_lo, tr_hi);
		temp = _mm256_and_si256(match_mask, *tr_lo);
		matches = _mm256_cmpeq_epi32(temp, match_mask);
		msk = _mm256_movemask_epi8(matches);
	}
}

static inline int
search_avx2x16(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t total_packets, uint32_t categories)
{
	uint32_t n;
	struct acl_flow_data flows;
	uint64_t index_array[MAX_SEARCHES_AVX16];
	struct completion cmplt[MAX_SEARCHES_AVX16];
	struct parms parms[MAX_SEARCHES_AVX16];
	ymm_t input[2], tr_lo[2], tr_hi[2];
	ymm_t t0, t1;

	acl_set_flow(&flows, cmplt, RTE_DIM(cmplt), data, results,
		total_packets, categories, ctx->trans_table);

	for (n = 0; n < RTE_DIM(cmplt); n++) {
		cmplt[n].count = 0;
		index_array[n] = acl_start_next_trie(&flows, parms, n, ctx);
	}

	t0 = _mm256_set_epi64x(index_array[5], index_array[4],
		index_array[1], index_array[0]);
	t1 = _mm256_set_epi64x(index_array[7], index_array[6],
		index_array[3], index_array[2]);

	tr_lo[0] = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0x88);
	tr_hi[0] = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0xdd);

	t0 = _mm256_set_epi64x(index_array[13], index_array[12],
		index_array[9], index_array[8]);
	t1 = _mm256_set_epi64x(index_array[15], index_array[14],
		index_array[11], index_array[10]);

	tr_lo[1] = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0x88);
	tr_hi[1] = (ymm_t)_mm256_shuffle_ps((__m256)t0, (__m256)t1, 0xdd);

	 /* Check for any matches. */
	acl_match_check_avx2x8(ctx, parms, &flows, 0, &tr_lo[0], &tr_hi[0],
		ymm_match_mask.y);
	acl_match_check_avx2x8(ctx, parms, &flows, 8, &tr_lo[1], &tr_hi[1],
		ymm_match_mask.y);

	while (flows.started > 0) {

		uint32_t in[MAX_SEARCHES_SSE8];

		/* Gather 4 bytes of input data for first 8 flows. */
		in[0] = GET_NEXT_4BYTES(parms, 0);
		in[4] = GET_NEXT_4BYTES(parms, 4);
		in[1] = GET_NEXT_4BYTES(parms, 1);
		in[5] = GET_NEXT_4BYTES(parms, 5);
		in[2] = GET_NEXT_4BYTES(parms, 2);
		in[6] = GET_NEXT_4BYTES(parms, 6);
		in[3] = GET_NEXT_4BYTES(parms, 3);
		in[7] = GET_NEXT_4BYTES(parms, 7);
		input[0] = _mm256_set_epi32(in[7], in[6], in[5], in[4],
			in[3], in[2], in[1], in[0]);

		/* Gather 4 bytes of input data for last 8 flows. */
		in[0] = GET_NEXT_4BYTES(parms, 8);
		in[4] = GET_NEXT_4BYTES(parms, 12);
		in[1] = GET_NEXT_4BYTES(parms, 9);
		in[5] = GET_NEXT_4BYTES(parms, 13);
		in[2] = GET_NEXT_4BYTES(parms, 10);
		in[6] = GET_NEXT_4BYTES(parms, 14);
		in[3] = GET_NEXT_4BYTES(parms, 11);
		in[7] = GET_NEXT_4BYTES(parms, 15);
		input[1] = _mm256_set_epi32(in[7], in[6], in[5], in[4],
			in[3], in[2], in[1], in[0]);

		input[0] = transition8(input[0], flows.trans,
			&tr_lo[0], &tr_hi[0]);
		input[1] = transition8(input[1], flows.trans,
			&tr_lo[1], &tr_hi[1]);

		input[0] = transition8(input[0], flows.trans,
			&tr_lo[0], &tr_hi[0]);
		input[1] = transition8(input[1], flows.trans,
			&tr_lo[1], &tr_hi[1]);

		input[0] = transition8(input[0], flows.trans,
			&tr_lo[0], &tr_hi[0]);
		input[1] = transition8(input[1], flows.trans,
			&tr_lo[1], &tr_hi[1]);

		input[0] = transition8(input[0], flows.trans,
			&tr_lo[0], &tr_hi[0]);
		input[1] = transition8(input[1], flows.trans,
			&tr_lo[1], &tr_hi[1]);

		 /* Check for any matches. */
		acl_match_check_avx2x8(ctx, parms, &flows, 0,
			&tr_lo[0], &tr_hi[0], ymm_match_mask.y);
		acl_match_check_avx2x8(ctx, parms, &flows, 8,
			&tr_lo[1], &tr_hi[1], ymm_match_mask.y);
	}

	return 0;
}
