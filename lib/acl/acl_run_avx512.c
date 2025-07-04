/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "acl_run_sse.h"

/*sizeof(uint32_t) << ACL_MATCH_LOG == sizeof(struct rte_acl_match_results)*/
#define ACL_MATCH_LOG	5

struct acl_flow_avx512 {
	uint32_t num_packets;       /* number of packets processed */
	uint32_t total_packets;     /* max number of packets to process */
	uint32_t root_index;        /* current root index */
	uint32_t first_load_sz;     /* first load size for new packet */
	const uint64_t *trans;      /* transition table */
	const uint32_t *data_index; /* input data indexes */
	const uint8_t **idata;      /* input data */
	uint32_t *matches;          /* match indexes */
};

static inline void
acl_set_flow_avx512(struct acl_flow_avx512 *flow, const struct rte_acl_ctx *ctx,
	uint32_t trie, const uint8_t *data[], uint32_t *matches,
	uint32_t total_packets)
{
	flow->num_packets = 0;
	flow->total_packets = total_packets;
	flow->first_load_sz = ctx->first_load_sz;
	flow->root_index = ctx->trie[trie].root_index;
	flow->trans = ctx->trans_table;
	flow->data_index = ctx->trie[trie].data_index;
	flow->idata = data;
	flow->matches = matches;
}

/*
 * Update flow and result masks based on the number of unprocessed flows.
 */
static inline uint32_t
update_flow_mask(const struct acl_flow_avx512 *flow, uint32_t *fmsk,
	uint32_t *rmsk)
{
	uint32_t i, j, k, m, n;

	fmsk[0] ^= rmsk[0];
	m = rmsk[0];

	k = rte_popcount32(m);
	n = flow->total_packets - flow->num_packets;

	if (n < k) {
		/* reduce mask */
		for (i = k - n; i != 0; i--) {
			j = sizeof(m) * CHAR_BIT - 1 - rte_clz32(m);
			m ^= 1 << j;
		}
	} else
		n = k;

	rmsk[0] = m;
	fmsk[0] |= rmsk[0];

	return n;
}

/*
 * Resolve matches for multiple categories (LE 8, use 128b instructions/regs)
 */
static inline void
resolve_mcle8_avx512x1(uint32_t result[],
	const struct rte_acl_match_results pr[], const uint32_t match[],
	uint32_t nb_pkt, uint32_t nb_cat, uint32_t nb_trie)
{
	const int32_t *pri;
	const uint32_t *pm, *res;
	uint32_t i, j, k, mi, mn;
	__mmask8 msk;
	xmm_t cp, cr, np, nr;

	res = pr->results;
	pri = pr->priority;

	for (k = 0; k != nb_pkt; k++, result += nb_cat) {

		mi = match[k] << ACL_MATCH_LOG;

		for (j = 0; j != nb_cat; j += RTE_ACL_RESULTS_MULTIPLIER) {

			cr = _mm_loadu_si128((const xmm_t *)(res + mi + j));
			cp = _mm_loadu_si128((const xmm_t *)(pri + mi + j));

			for (i = 1, pm = match + nb_pkt; i != nb_trie;
				i++, pm += nb_pkt) {

				mn = j + (pm[k] << ACL_MATCH_LOG);

				nr = _mm_loadu_si128((const xmm_t *)(res + mn));
				np = _mm_loadu_si128((const xmm_t *)(pri + mn));

				msk = _mm_cmpgt_epi32_mask(cp, np);
				cr = _mm_mask_mov_epi32(nr, msk, cr);
				cp = _mm_mask_mov_epi32(np, msk, cp);
			}

			_mm_storeu_si128((xmm_t *)(result + j), cr);
		}
	}
}

#include "acl_run_avx512x8.h"

int
rte_acl_classify_avx512x16(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	const uint32_t max_iter = MAX_SEARCHES_AVX16 * MAX_SEARCHES_AVX16;

	/* split huge lookup (gt 256) into series of fixed size ones */
	while (num > max_iter) {
		search_avx512x8x2(ctx, data, results, max_iter, categories);
		data += max_iter;
		results += max_iter * categories;
		num -= max_iter;
	}

	/* select classify method based on number of remaining requests */
	if (num >= MAX_SEARCHES_AVX16)
		return search_avx512x8x2(ctx, data, results, num, categories);
	if (num >= MAX_SEARCHES_SSE8)
		return search_sse_8(ctx, data, results, num, categories);
	if (num >= MAX_SEARCHES_SSE4)
		return search_sse_4(ctx, data, results, num, categories);

	return rte_acl_classify_scalar(ctx, data, results, num, categories);
}

#include "acl_run_avx512x16.h"

int
rte_acl_classify_avx512x32(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	const uint32_t max_iter = MAX_SEARCHES_AVX16 * MAX_SEARCHES_AVX16;

	/* split huge lookup (gt 256) into series of fixed size ones */
	while (num > max_iter) {
		search_avx512x16x2(ctx, data, results, max_iter, categories);
		data += max_iter;
		results += max_iter * categories;
		num -= max_iter;
	}

	/* select classify method based on number of remaining requests */
	if (num >= 2 * MAX_SEARCHES_AVX16)
		return search_avx512x16x2(ctx, data, results, num, categories);
	if (num >= MAX_SEARCHES_AVX16)
		return search_avx512x8x2(ctx, data, results, num, categories);
	if (num >= MAX_SEARCHES_SSE8)
		return search_sse_8(ctx, data, results, num, categories);
	if (num >= MAX_SEARCHES_SSE4)
		return search_sse_4(ctx, data, results, num, categories);

	return rte_acl_classify_scalar(ctx, data, results, num, categories);
}
