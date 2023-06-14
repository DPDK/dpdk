/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_acl.h>
#include "acl.h"

#define	QRANGE_MIN	((uint8_t)INT8_MIN)

#define	RTE_ACL_VERIFY(exp)	do {                                          \
	if (!(exp))                                                           \
		rte_panic("line %d\tassert \"" #exp "\" failed\n", __LINE__); \
} while (0)

struct acl_node_counters {
	int32_t match;
	int32_t match_used;
	int32_t single;
	int32_t quad;
	int32_t quad_vectors;
	int32_t dfa;
	int32_t dfa_gr64;
};

struct rte_acl_indices {
	int32_t dfa_index;
	int32_t quad_index;
	int32_t single_index;
	int32_t match_index;
	int32_t match_start;
};

static void
acl_gen_log_stats(const struct rte_acl_ctx *ctx,
	const struct acl_node_counters *counts,
	const struct rte_acl_indices *indices,
	size_t max_size)
{
	RTE_LOG(DEBUG, ACL, "Gen phase for ACL \"%s\":\n"
		"runtime memory footprint on socket %d:\n"
		"single nodes/bytes used: %d/%zu\n"
		"quad nodes/vectors/bytes used: %d/%d/%zu\n"
		"DFA nodes/group64/bytes used: %d/%d/%zu\n"
		"match nodes/bytes used: %d/%zu\n"
		"total: %zu bytes\n"
		"max limit: %zu bytes\n",
		ctx->name, ctx->socket_id,
		counts->single, counts->single * sizeof(uint64_t),
		counts->quad, counts->quad_vectors,
		(indices->quad_index - indices->dfa_index) * sizeof(uint64_t),
		counts->dfa, counts->dfa_gr64,
		indices->dfa_index * sizeof(uint64_t),
		counts->match,
		counts->match * sizeof(struct rte_acl_match_results),
		ctx->mem_sz,
		max_size);
}

static uint64_t
acl_dfa_gen_idx(const struct rte_acl_node *node, uint32_t index)
{
	uint64_t idx;
	uint32_t i;

	idx = 0;
	for (i = 0; i != RTE_DIM(node->dfa_gr64); i++) {
		RTE_ACL_VERIFY(node->dfa_gr64[i] < RTE_ACL_DFA_GR64_NUM);
		RTE_ACL_VERIFY(node->dfa_gr64[i] < node->fanout);
		idx |= (i - node->dfa_gr64[i]) <<
			(6 + RTE_ACL_DFA_GR64_BIT * i);
	}

	return idx << (CHAR_BIT * sizeof(index)) | index | node->node_type;
}

static void
acl_dfa_fill_gr64(const struct rte_acl_node *node,
	const uint64_t src[RTE_ACL_DFA_SIZE], uint64_t dst[RTE_ACL_DFA_SIZE])
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(node->dfa_gr64); i++) {
		memcpy(dst + node->dfa_gr64[i] * RTE_ACL_DFA_GR64_SIZE,
			src + i * RTE_ACL_DFA_GR64_SIZE,
			RTE_ACL_DFA_GR64_SIZE * sizeof(dst[0]));
	}
}

static uint32_t
acl_dfa_count_gr64(const uint64_t array_ptr[RTE_ACL_DFA_SIZE],
	uint8_t gr64[RTE_ACL_DFA_GR64_NUM])
{
	uint32_t i, j, k;

	k = 0;
	for (i = 0; i != RTE_ACL_DFA_GR64_NUM; i++) {
		gr64[i] = i;
		for (j = 0; j != i; j++) {
			if (memcmp(array_ptr + i * RTE_ACL_DFA_GR64_SIZE,
					array_ptr + j * RTE_ACL_DFA_GR64_SIZE,
					RTE_ACL_DFA_GR64_SIZE *
					sizeof(array_ptr[0])) == 0)
				break;
		}
		gr64[i] = (j != i) ? gr64[j] : k++;
	}

	return k;
}

static uint32_t
acl_node_fill_dfa(const struct rte_acl_node *node,
	uint64_t dfa[RTE_ACL_DFA_SIZE], uint64_t no_match, int32_t resolved)
{
	uint32_t n, x;
	uint32_t ranges, last_bit;
	struct rte_acl_node *child;
	struct rte_acl_bitset *bits;

	ranges = 0;
	last_bit = 0;

	for (n = 0; n < RTE_ACL_DFA_SIZE; n++)
		dfa[n] = no_match;

	for (x = 0; x < node->num_ptrs; x++) {

		child = node->ptrs[x].ptr;
		if (child == NULL)
			continue;

		bits = &node->ptrs[x].values;
		for (n = 0; n < RTE_ACL_DFA_SIZE; n++) {

			if (bits->bits[n / (sizeof(bits_t) * CHAR_BIT)] &
				(1U << (n % (sizeof(bits_t) * CHAR_BIT)))) {

				dfa[n] = resolved ? child->node_index : x;
				ranges += (last_bit == 0);
				last_bit = 1;
			} else {
				last_bit = 0;
			}
		}
	}

	return ranges;
}

/*
 * Count the number of groups of sequential bits that are either 0 or 1,
 * as specified by the zero_one parameter.
 * This is used to calculate the number of ranges in a node
 * to see if it fits in a quad range node.
 */
static int
acl_count_sequential_groups(struct rte_acl_bitset *bits, int zero_one)
{
	int n, ranges, last_bit;

	ranges = 0;
	last_bit = zero_one ^ 1;

	for (n = QRANGE_MIN; n < UINT8_MAX + 1; n++) {
		if (bits->bits[n / (sizeof(bits_t) * 8)] &
				(1U << (n % (sizeof(bits_t) * 8)))) {
			if (zero_one == 1 && last_bit != 1)
				ranges++;
			last_bit = 1;
		} else {
			if (zero_one == 0 && last_bit != 0)
				ranges++;
			last_bit = 0;
		}
	}
	for (n = 0; n < QRANGE_MIN; n++) {
		if (bits->bits[n / (sizeof(bits_t) * 8)] &
				(1U << (n % (sizeof(bits_t) * CHAR_BIT)))) {
			if (zero_one == 1 && last_bit != 1)
				ranges++;
			last_bit = 1;
		} else {
			if (zero_one == 0 && last_bit != 0)
				ranges++;
			last_bit = 0;
		}
	}

	return ranges;
}

/*
 * Count number of ranges spanned by the node's pointers
 */
static int
acl_count_fanout(struct rte_acl_node *node)
{
	uint32_t n;
	int ranges;

	if (node->fanout != 0)
		return node->fanout;

	ranges = acl_count_sequential_groups(&node->values, 0);

	for (n = 0; n < node->num_ptrs; n++) {
		if (node->ptrs[n].ptr != NULL)
			ranges += acl_count_sequential_groups(
				&node->ptrs[n].values, 1);
	}

	node->fanout = ranges;
	return node->fanout;
}

/*
 * Determine the type of nodes and count each type
 */
static void
acl_count_trie_types(struct acl_node_counters *counts,
	struct rte_acl_node *node, uint64_t no_match, int force_dfa)
{
	uint32_t n;
	int num_ptrs;
	uint64_t dfa[RTE_ACL_DFA_SIZE];

	/* skip if this node has been counted */
	if (node->node_type != (uint32_t)RTE_ACL_NODE_UNDEFINED)
		return;

	if (node->match_flag != 0 || node->num_ptrs == 0) {
		counts->match++;
		node->node_type = RTE_ACL_NODE_MATCH;
		return;
	}

	num_ptrs = acl_count_fanout(node);

	/* Force type to dfa */
	if (force_dfa)
		num_ptrs = RTE_ACL_DFA_SIZE;

	/* determine node type based on number of ranges */
	if (num_ptrs == 1) {
		counts->single++;
		node->node_type = RTE_ACL_NODE_SINGLE;
	} else if (num_ptrs <= RTE_ACL_QUAD_MAX) {
		counts->quad++;
		counts->quad_vectors += node->fanout;
		node->node_type = RTE_ACL_NODE_QRANGE;
	} else {
		counts->dfa++;
		node->node_type = RTE_ACL_NODE_DFA;
		if (force_dfa != 0) {
			/* always expand to a max number of nodes. */
			for (n = 0; n != RTE_DIM(node->dfa_gr64); n++)
				node->dfa_gr64[n] = n;
			node->fanout = n;
		} else {
			acl_node_fill_dfa(node, dfa, no_match, 0);
			node->fanout = acl_dfa_count_gr64(dfa, node->dfa_gr64);
		}
		counts->dfa_gr64 += node->fanout;
	}

	/*
	 * recursively count the types of all children
	 */
	for (n = 0; n < node->num_ptrs; n++) {
		if (node->ptrs[n].ptr != NULL)
			acl_count_trie_types(counts, node->ptrs[n].ptr,
				no_match, 0);
	}
}

static void
acl_add_ptrs(struct rte_acl_node *node, uint64_t *node_array, uint64_t no_match,
	int resolved)
{
	uint32_t x;
	int32_t m;
	uint64_t *node_a, index, dfa[RTE_ACL_DFA_SIZE];

	acl_node_fill_dfa(node, dfa, no_match, resolved);

	/*
	 * Rather than going from 0 to 256, the range count and
	 * the layout are from 80-ff then 0-7f due to signed compare
	 * for SSE (cmpgt).
	 */
	if (node->node_type == RTE_ACL_NODE_QRANGE) {

		m = 0;
		node_a = node_array;
		index = dfa[QRANGE_MIN];
		*node_a++ = index;

		for (x = QRANGE_MIN + 1; x < UINT8_MAX + 1; x++) {
			if (dfa[x] != index) {
				index = dfa[x];
				*node_a++ = index;
				node->transitions[m++] = (uint8_t)(x - 1);
			}
		}

		for (x = 0; x < INT8_MAX + 1; x++) {
			if (dfa[x] != index) {
				index = dfa[x];
				*node_a++ = index;
				node->transitions[m++] = (uint8_t)(x - 1);
			}
		}

		/* fill unused locations with max value - nothing is greater */
		for (; m < RTE_ACL_QUAD_SIZE; m++)
			node->transitions[m] = INT8_MAX;

		RTE_ACL_VERIFY(m <= RTE_ACL_QUAD_SIZE);

	} else if (node->node_type == RTE_ACL_NODE_DFA && resolved) {
		acl_dfa_fill_gr64(node, dfa, node_array);
	}
}

/*
 * Routine that allocates space for this node and recursively calls
 * to allocate space for each child. Once all the children are allocated,
 * then resolve all transitions for this node.
 */
static void
acl_gen_node(struct rte_acl_node *node, uint64_t *node_array,
	uint64_t no_match, struct rte_acl_indices *index, int num_categories)
{
	uint32_t n, sz, *qtrp;
	uint64_t *array_ptr;
	struct rte_acl_match_results *match;

	if (node->node_index != RTE_ACL_NODE_UNDEFINED)
		return;

	array_ptr = NULL;

	switch (node->node_type) {
	case RTE_ACL_NODE_DFA:
		array_ptr = &node_array[index->dfa_index];
		node->node_index = acl_dfa_gen_idx(node, index->dfa_index);
		sz = node->fanout * RTE_ACL_DFA_GR64_SIZE;
		index->dfa_index += sz;
		for (n = 0; n < sz; n++)
			array_ptr[n] = no_match;
		break;
	case RTE_ACL_NODE_SINGLE:
		node->node_index = RTE_ACL_QUAD_SINGLE | index->single_index |
			node->node_type;
		array_ptr = &node_array[index->single_index];
		index->single_index += 1;
		array_ptr[0] = no_match;
		break;
	case RTE_ACL_NODE_QRANGE:
		array_ptr = &node_array[index->quad_index];
		acl_add_ptrs(node, array_ptr, no_match, 0);
		qtrp = (uint32_t *)node->transitions;
		node->node_index = qtrp[0];
		node->node_index <<= sizeof(index->quad_index) * CHAR_BIT;
		node->node_index |= index->quad_index | node->node_type;
		index->quad_index += node->fanout;
		break;
	case RTE_ACL_NODE_MATCH:
		match = ((struct rte_acl_match_results *)
			(node_array + index->match_start));
		for (n = 0; n != RTE_DIM(match->results); n++)
			RTE_ACL_VERIFY(match->results[0] == 0);
		memcpy(match + index->match_index, node->mrt,
			sizeof(*node->mrt));
		node->node_index = index->match_index | node->node_type;
		index->match_index += 1;
		break;
	case RTE_ACL_NODE_UNDEFINED:
		RTE_ACL_VERIFY(node->node_type !=
			(uint32_t)RTE_ACL_NODE_UNDEFINED);
		break;
	}

	/* recursively allocate space for all children */
	for (n = 0; n < node->num_ptrs; n++) {
		if (node->ptrs[n].ptr != NULL)
			acl_gen_node(node->ptrs[n].ptr,
				node_array,
				no_match,
				index,
				num_categories);
	}

	/* All children are resolved, resolve this node's pointers */
	switch (node->node_type) {
	case RTE_ACL_NODE_DFA:
		acl_add_ptrs(node, array_ptr, no_match, 1);
		break;
	case RTE_ACL_NODE_SINGLE:
		for (n = 0; n < node->num_ptrs; n++) {
			if (node->ptrs[n].ptr != NULL)
				array_ptr[0] = node->ptrs[n].ptr->node_index;
		}
		break;
	case RTE_ACL_NODE_QRANGE:
		acl_add_ptrs(node, array_ptr, no_match, 1);
		break;
	case RTE_ACL_NODE_MATCH:
		break;
	case RTE_ACL_NODE_UNDEFINED:
		RTE_ACL_VERIFY(node->node_type !=
			(uint32_t)RTE_ACL_NODE_UNDEFINED);
		break;
	}
}

static void
acl_calc_counts_indices(struct acl_node_counters *counts,
	struct rte_acl_indices *indices,
	struct rte_acl_bld_trie *node_bld_trie, uint32_t num_tries,
	uint64_t no_match)
{
	uint32_t n;

	memset(indices, 0, sizeof(*indices));
	memset(counts, 0, sizeof(*counts));

	/* Get stats on nodes */
	for (n = 0; n < num_tries; n++) {
		acl_count_trie_types(counts, node_bld_trie[n].trie,
			no_match, 1);
	}

	indices->dfa_index = RTE_ACL_DFA_SIZE + 1;
	indices->quad_index = indices->dfa_index +
		counts->dfa_gr64 * RTE_ACL_DFA_GR64_SIZE;
	indices->single_index = indices->quad_index + counts->quad_vectors;
	indices->match_start = indices->single_index + counts->single + 1;
	indices->match_start = RTE_ALIGN(indices->match_start,
		(XMM_SIZE / sizeof(uint64_t)));
	indices->match_index = 1;
}

/*
 * Generate the runtime structure using build structure
 */
int
rte_acl_gen(struct rte_acl_ctx *ctx, struct rte_acl_trie *trie,
	struct rte_acl_bld_trie *node_bld_trie, uint32_t num_tries,
	uint32_t num_categories, uint32_t data_index_sz, size_t max_size)
{
	void *mem;
	size_t total_size;
	uint64_t *node_array, no_match;
	uint32_t n, match_index;
	struct rte_acl_match_results *match;
	struct acl_node_counters counts;
	struct rte_acl_indices indices;

	no_match = RTE_ACL_NODE_MATCH;

	/* Fill counts and indices arrays from the nodes. */
	acl_calc_counts_indices(&counts, &indices,
		node_bld_trie, num_tries, no_match);

	/* Allocate runtime memory (align to cache boundary) */
	total_size = RTE_ALIGN(data_index_sz, RTE_CACHE_LINE_SIZE) +
		indices.match_start * sizeof(uint64_t) +
		(counts.match + 1) * sizeof(struct rte_acl_match_results) +
		XMM_SIZE;

	if (total_size > max_size) {
		RTE_LOG(DEBUG, ACL,
			"Gen phase for ACL ctx \"%s\" exceeds max_size limit, "
			"bytes required: %zu, allowed: %zu\n",
			ctx->name, total_size, max_size);
		return -ERANGE;
	}

	mem = rte_zmalloc_socket(ctx->name, total_size, RTE_CACHE_LINE_SIZE,
			ctx->socket_id);
	if (mem == NULL) {
		RTE_LOG(ERR, ACL,
			"allocation of %zu bytes on socket %d for %s failed\n",
			total_size, ctx->socket_id, ctx->name);
		return -ENOMEM;
	}

	/* Fill the runtime structure */
	match_index = indices.match_start;
	node_array = (uint64_t *)((uintptr_t)mem +
		RTE_ALIGN(data_index_sz, RTE_CACHE_LINE_SIZE));

	/*
	 * Setup the NOMATCH node (a SINGLE at the
	 * highest index, that points to itself)
	 */

	node_array[RTE_ACL_DFA_SIZE] = RTE_ACL_IDLE_NODE;

	for (n = 0; n < RTE_ACL_DFA_SIZE; n++)
		node_array[n] = no_match;

	/* NOMATCH result at index 0 */
	match = ((struct rte_acl_match_results *)(node_array + match_index));
	memset(match, 0, sizeof(*match));

	for (n = 0; n < num_tries; n++) {

		acl_gen_node(node_bld_trie[n].trie, node_array, no_match,
			&indices, num_categories);

		if (node_bld_trie[n].trie->node_index == no_match)
			trie[n].root_index = 0;
		else
			trie[n].root_index = node_bld_trie[n].trie->node_index;
	}

	ctx->mem = mem;
	ctx->mem_sz = total_size;
	ctx->data_indexes = mem;
	ctx->num_tries = num_tries;
	ctx->num_categories = num_categories;
	ctx->match_index = match_index;
	ctx->no_match = no_match;
	ctx->idle = node_array[RTE_ACL_DFA_SIZE];
	ctx->trans_table = node_array;
	memcpy(ctx->trie, trie, sizeof(ctx->trie));

	acl_gen_log_stats(ctx, &counts, &indices, max_size);
	return 0;
}
