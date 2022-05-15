/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_acl.h>
#include "tb_mem.h"
#include "acl.h"

#define	ACL_POOL_ALIGN		8
#define	ACL_POOL_ALLOC_MIN	0x800000

/* number of pointers per alloc */
#define ACL_PTR_ALLOC	32

/* account for situation when all fields are 8B long */
#define ACL_MAX_INDEXES	(2 * RTE_ACL_MAX_FIELDS)

/* macros for dividing rule sets heuristics */
#define NODE_MAX	0x4000
#define NODE_MIN	0x800

/* TALLY are statistics per field */
enum {
	TALLY_0 = 0,        /* number of rules that are 0% or more wild. */
	TALLY_25,	    /* number of rules that are 25% or more wild. */
	TALLY_50,
	TALLY_75,
	TALLY_100,
	TALLY_DEACTIVATED, /* deactivated fields (100% wild in all rules). */
	TALLY_DEPTH,
	/* number of rules that are 100% wild for this field and higher. */
	TALLY_NUM
};

static const uint32_t wild_limits[TALLY_DEACTIVATED] = {0, 25, 50, 75, 100};

enum {
	ACL_INTERSECT_NONE = 0,
	ACL_INTERSECT_A = 1,    /* set A is a superset of A and B intersect */
	ACL_INTERSECT_B = 2,    /* set B is a superset of A and B intersect */
	ACL_INTERSECT = 4,	/* sets A and B intersect */
};

enum {
	ACL_PRIORITY_EQUAL = 0,
	ACL_PRIORITY_NODE_A = 1,
	ACL_PRIORITY_NODE_B = 2,
	ACL_PRIORITY_MIXED = 3
};


struct acl_mem_block {
	uint32_t block_size;
	void     *mem_ptr;
};

#define	MEM_BLOCK_NUM	16

/* Single ACL rule, build representation.*/
struct rte_acl_build_rule {
	struct rte_acl_build_rule   *next;
	struct rte_acl_config       *config;
	/**< configuration for each field in the rule. */
	const struct rte_acl_rule   *f;
	uint32_t                    *wildness;
};

/* Context for build phase */
struct acl_build_context {
	const struct rte_acl_ctx *acx;
	struct rte_acl_build_rule *build_rules;
	struct rte_acl_config     cfg;
	int32_t                   node_max;
	int32_t                   cur_node_max;
	uint32_t                  node;
	uint32_t                  num_nodes;
	uint32_t                  category_mask;
	uint32_t                  num_rules;
	uint32_t                  node_id;
	uint32_t                  src_mask;
	uint32_t                  num_build_rules;
	uint32_t                  num_tries;
	struct tb_mem_pool        pool;
	struct rte_acl_trie       tries[RTE_ACL_MAX_TRIES];
	struct rte_acl_bld_trie   bld_tries[RTE_ACL_MAX_TRIES];
	uint32_t            data_indexes[RTE_ACL_MAX_TRIES][ACL_MAX_INDEXES];

	/* memory free lists for nodes and blocks used for node ptrs */
	struct acl_mem_block      blocks[MEM_BLOCK_NUM];
	struct rte_acl_node       *node_free_list;
};

static int acl_merge_trie(struct acl_build_context *context,
	struct rte_acl_node *node_a, struct rte_acl_node *node_b,
	uint32_t level, struct rte_acl_node **node_c);

static void
acl_deref_ptr(struct acl_build_context *context,
	struct rte_acl_node *node, int index);

static void *
acl_build_alloc(struct acl_build_context *context, size_t n, size_t s)
{
	uint32_t m;
	void *p;
	size_t alloc_size = n * s;

	/*
	 * look for memory in free lists
	 */
	for (m = 0; m < RTE_DIM(context->blocks); m++) {
		if (context->blocks[m].block_size ==
		   alloc_size && context->blocks[m].mem_ptr != NULL) {
			p = context->blocks[m].mem_ptr;
			context->blocks[m].mem_ptr = *((void **)p);
			memset(p, 0, alloc_size);
			return p;
		}
	}

	/*
	 * return allocation from memory pool
	 */
	p = tb_alloc(&context->pool, alloc_size);
	return p;
}

/*
 * Free memory blocks (kept in context for reuse).
 */
static void
acl_build_free(struct acl_build_context *context, size_t s, void *p)
{
	uint32_t n;

	for (n = 0; n < RTE_DIM(context->blocks); n++) {
		if (context->blocks[n].block_size == s) {
			*((void **)p) = context->blocks[n].mem_ptr;
			context->blocks[n].mem_ptr = p;
			return;
		}
	}
	for (n = 0; n < RTE_DIM(context->blocks); n++) {
		if (context->blocks[n].block_size == 0) {
			context->blocks[n].block_size = s;
			*((void **)p) = NULL;
			context->blocks[n].mem_ptr = p;
			return;
		}
	}
}

/*
 * Allocate and initialize a new node.
 */
static struct rte_acl_node *
acl_alloc_node(struct acl_build_context *context, int level)
{
	struct rte_acl_node *node;

	if (context->node_free_list != NULL) {
		node = context->node_free_list;
		context->node_free_list = node->next;
		memset(node, 0, sizeof(struct rte_acl_node));
	} else {
		node = acl_build_alloc(context, sizeof(struct rte_acl_node), 1);
	}

	if (node != NULL) {
		node->num_ptrs = 0;
		node->level = level;
		node->node_type = RTE_ACL_NODE_UNDEFINED;
		node->node_index = RTE_ACL_NODE_UNDEFINED;
		context->num_nodes++;
		node->id = context->node_id++;
	}
	return node;
}

/*
 * Dereference all nodes to which this node points
 */
static void
acl_free_node(struct acl_build_context *context,
	struct rte_acl_node *node)
{
	uint32_t n;

	if (node->prev != NULL)
		node->prev->next = NULL;
	for (n = 0; n < node->num_ptrs; n++)
		acl_deref_ptr(context, node, n);

	/* free mrt if this is a match node */
	if (node->mrt != NULL) {
		acl_build_free(context, sizeof(struct rte_acl_match_results),
			node->mrt);
		node->mrt = NULL;
	}

	/* free transitions to other nodes */
	if (node->ptrs != NULL) {
		acl_build_free(context,
			node->max_ptrs * sizeof(struct rte_acl_ptr_set),
			node->ptrs);
		node->ptrs = NULL;
	}

	/* put it on the free list */
	context->num_nodes--;
	node->next = context->node_free_list;
	context->node_free_list = node;
}


/*
 * Include src bitset in dst bitset
 */
static void
acl_include(struct rte_acl_bitset *dst, struct rte_acl_bitset *src, bits_t mask)
{
	uint32_t n;

	for (n = 0; n < RTE_ACL_BIT_SET_SIZE; n++)
		dst->bits[n] = (dst->bits[n] & mask) | src->bits[n];
}

/*
 * Set dst to bits of src1 that are not in src2
 */
static int
acl_exclude(struct rte_acl_bitset *dst,
	struct rte_acl_bitset *src1,
	struct rte_acl_bitset *src2)
{
	uint32_t n;
	bits_t all_bits = 0;

	for (n = 0; n < RTE_ACL_BIT_SET_SIZE; n++) {
		dst->bits[n] = src1->bits[n] & ~src2->bits[n];
		all_bits |= dst->bits[n];
	}
	return all_bits != 0;
}

/*
 * Add a pointer (ptr) to a node.
 */
static int
acl_add_ptr(struct acl_build_context *context,
	struct rte_acl_node *node,
	struct rte_acl_node *ptr,
	struct rte_acl_bitset *bits)
{
	uint32_t n, num_ptrs;
	struct rte_acl_ptr_set *ptrs = NULL;

	/*
	 * If there's already a pointer to the same node, just add to the bitset
	 */
	for (n = 0; n < node->num_ptrs; n++) {
		if (node->ptrs[n].ptr != NULL) {
			if (node->ptrs[n].ptr == ptr) {
				acl_include(&node->ptrs[n].values, bits, -1);
				acl_include(&node->values, bits, -1);
				return 0;
			}
		}
	}

	/* if there's no room for another pointer, make room */
	if (node->num_ptrs >= node->max_ptrs) {
		/* add room for more pointers */
		num_ptrs = node->max_ptrs + ACL_PTR_ALLOC;
		ptrs = acl_build_alloc(context, num_ptrs, sizeof(*ptrs));

		/* copy current points to new memory allocation */
		if (node->ptrs != NULL) {
			memcpy(ptrs, node->ptrs,
				node->num_ptrs * sizeof(*ptrs));
			acl_build_free(context, node->max_ptrs * sizeof(*ptrs),
				node->ptrs);
		}
		node->ptrs = ptrs;
		node->max_ptrs = num_ptrs;
	}

	/* Find available ptr and add a new pointer to this node */
	for (n = node->min_add; n < node->max_ptrs; n++) {
		if (node->ptrs[n].ptr == NULL) {
			node->ptrs[n].ptr = ptr;
			acl_include(&node->ptrs[n].values, bits, 0);
			acl_include(&node->values, bits, -1);
			if (ptr != NULL)
				ptr->ref_count++;
			if (node->num_ptrs <= n)
				node->num_ptrs = n + 1;
			return 0;
		}
	}

	return 0;
}

/*
 * Add a pointer for a range of values
 */
static int
acl_add_ptr_range(struct acl_build_context *context,
	struct rte_acl_node *root,
	struct rte_acl_node *node,
	uint8_t low,
	uint8_t high)
{
	uint32_t n;
	struct rte_acl_bitset bitset;

	/* clear the bitset values */
	for (n = 0; n < RTE_ACL_BIT_SET_SIZE; n++)
		bitset.bits[n] = 0;

	/* for each bit in range, add bit to set */
	for (n = 0; n < UINT8_MAX + 1; n++)
		if (n >= low && n <= high)
			bitset.bits[n / (sizeof(bits_t) * 8)] |=
				1U << (n % (sizeof(bits_t) * CHAR_BIT));

	return acl_add_ptr(context, root, node, &bitset);
}

/*
 * Generate a bitset from a byte value and mask.
 */
static int
acl_gen_mask(struct rte_acl_bitset *bitset, uint32_t value, uint32_t mask)
{
	int range = 0;
	uint32_t n;

	/* clear the bitset values */
	for (n = 0; n < RTE_ACL_BIT_SET_SIZE; n++)
		bitset->bits[n] = 0;

	/* for each bit in value/mask, add bit to set */
	for (n = 0; n < UINT8_MAX + 1; n++) {
		if ((n & mask) == value) {
			range++;
			bitset->bits[n / (sizeof(bits_t) * 8)] |=
				1U << (n % (sizeof(bits_t) * CHAR_BIT));
		}
	}
	return range;
}

/*
 * Determine how A and B intersect.
 * Determine if A and/or B are supersets of the intersection.
 */
static int
acl_intersect_type(const struct rte_acl_bitset *a_bits,
	const struct rte_acl_bitset *b_bits,
	struct rte_acl_bitset *intersect)
{
	uint32_t n;
	bits_t intersect_bits = 0;
	bits_t a_superset = 0;
	bits_t b_superset = 0;

	/*
	 * calculate and store intersection and check if A and/or B have
	 * bits outside the intersection (superset)
	 */
	for (n = 0; n < RTE_ACL_BIT_SET_SIZE; n++) {
		intersect->bits[n] = a_bits->bits[n] & b_bits->bits[n];
		a_superset |= a_bits->bits[n] ^ intersect->bits[n];
		b_superset |= b_bits->bits[n] ^ intersect->bits[n];
		intersect_bits |= intersect->bits[n];
	}

	n = (intersect_bits == 0 ? ACL_INTERSECT_NONE : ACL_INTERSECT) |
		(b_superset == 0 ? 0 : ACL_INTERSECT_B) |
		(a_superset == 0 ? 0 : ACL_INTERSECT_A);

	return n;
}

/*
 * Duplicate a node
 */
static struct rte_acl_node *
acl_dup_node(struct acl_build_context *context, struct rte_acl_node *node)
{
	uint32_t n;
	struct rte_acl_node *next;

	next = acl_alloc_node(context, node->level);

	/* allocate the pointers */
	if (node->num_ptrs > 0) {
		next->ptrs = acl_build_alloc(context,
			node->max_ptrs,
			sizeof(struct rte_acl_ptr_set));
		next->max_ptrs = node->max_ptrs;
	}

	/* copy over the pointers */
	for (n = 0; n < node->num_ptrs; n++) {
		if (node->ptrs[n].ptr != NULL) {
			next->ptrs[n].ptr = node->ptrs[n].ptr;
			next->ptrs[n].ptr->ref_count++;
			acl_include(&next->ptrs[n].values,
				&node->ptrs[n].values, -1);
		}
	}

	next->num_ptrs = node->num_ptrs;

	/* copy over node's match results */
	if (node->match_flag == 0)
		next->match_flag = 0;
	else {
		next->match_flag = -1;
		next->mrt = acl_build_alloc(context, 1, sizeof(*next->mrt));
		memcpy(next->mrt, node->mrt, sizeof(*next->mrt));
	}

	/* copy over node's bitset */
	acl_include(&next->values, &node->values, -1);

	node->next = next;
	next->prev = node;

	return next;
}

/*
 * Dereference a pointer from a node
 */
static void
acl_deref_ptr(struct acl_build_context *context,
	struct rte_acl_node *node, int index)
{
	struct rte_acl_node *ref_node;

	/* De-reference the node at the specified pointer */
	if (node != NULL && node->ptrs[index].ptr != NULL) {
		ref_node = node->ptrs[index].ptr;
		ref_node->ref_count--;
		if (ref_node->ref_count == 0)
			acl_free_node(context, ref_node);
	}
}

/*
 * acl_exclude rte_acl_bitset from src and copy remaining pointer to dst
 */
static int
acl_copy_ptr(struct acl_build_context *context,
	struct rte_acl_node *dst,
	struct rte_acl_node *src,
	int index,
	struct rte_acl_bitset *b_bits)
{
	int rc;
	struct rte_acl_bitset bits;

	if (b_bits != NULL)
		if (!acl_exclude(&bits, &src->ptrs[index].values, b_bits))
			return 0;

	rc = acl_add_ptr(context, dst, src->ptrs[index].ptr, &bits);
	if (rc < 0)
		return rc;
	return 1;
}

/*
 * Fill in gaps in ptrs list with the ptr at the end of the list
 */
static void
acl_compact_node_ptrs(struct rte_acl_node *node_a)
{
	uint32_t n;
	int min_add = node_a->min_add;

	while (node_a->num_ptrs > 0  &&
			node_a->ptrs[node_a->num_ptrs - 1].ptr == NULL)
		node_a->num_ptrs--;

	for (n = min_add; n + 1 < node_a->num_ptrs; n++) {

		/* if this entry is empty */
		if (node_a->ptrs[n].ptr == NULL) {

			/* move the last pointer to this entry */
			acl_include(&node_a->ptrs[n].values,
				&node_a->ptrs[node_a->num_ptrs - 1].values,
				0);
			node_a->ptrs[n].ptr =
				node_a->ptrs[node_a->num_ptrs - 1].ptr;

			/*
			 * mark the end as empty and adjust the number
			 * of used pointer enum_tries
			 */
			node_a->ptrs[node_a->num_ptrs - 1].ptr = NULL;
			while (node_a->num_ptrs > 0  &&
				node_a->ptrs[node_a->num_ptrs - 1].ptr == NULL)
				node_a->num_ptrs--;
		}
	}
}

static int
acl_resolve_leaf(struct acl_build_context *context,
	struct rte_acl_node *node_a,
	struct rte_acl_node *node_b,
	struct rte_acl_node **node_c)
{
	uint32_t n;
	int combined_priority = ACL_PRIORITY_EQUAL;

	for (n = 0; n < context->cfg.num_categories; n++) {
		if (node_a->mrt->priority[n] != node_b->mrt->priority[n]) {
			combined_priority |= (node_a->mrt->priority[n] >
				node_b->mrt->priority[n]) ?
				ACL_PRIORITY_NODE_A : ACL_PRIORITY_NODE_B;
		}
	}

	/*
	 * if node a is higher or equal priority for all categories,
	 * then return node_a.
	 */
	if (combined_priority == ACL_PRIORITY_NODE_A ||
			combined_priority == ACL_PRIORITY_EQUAL) {
		*node_c = node_a;
		return 0;
	}

	/*
	 * if node b is higher or equal priority for all categories,
	 * then return node_b.
	 */
	if (combined_priority == ACL_PRIORITY_NODE_B) {
		*node_c = node_b;
		return 0;
	}

	/*
	 * mixed priorities - create a new node with the highest priority
	 * for each category.
	 */

	/* force new duplication. */
	node_a->next = NULL;

	*node_c = acl_dup_node(context, node_a);
	for (n = 0; n < context->cfg.num_categories; n++) {
		if ((*node_c)->mrt->priority[n] < node_b->mrt->priority[n]) {
			(*node_c)->mrt->priority[n] = node_b->mrt->priority[n];
			(*node_c)->mrt->results[n] = node_b->mrt->results[n];
		}
	}
	return 0;
}

/*
 * Merge nodes A and B together,
 *   returns a node that is the path for the intersection
 *
 * If match node (leaf on trie)
 *	For each category
 *		return node = highest priority result
 *
 * Create C as a duplicate of A to point to child intersections
 * If any pointers in C intersect with any in B
 *	For each intersection
 *		merge children
 *		remove intersection from C pointer
 *		add a pointer from C to child intersection node
 * Compact the pointers in A and B
 * Copy any B pointers that are outside of the intersection to C
 * If C has no references to the B trie
 *   free C and return A
 * Else If C has no references to the A trie
 *   free C and return B
 * Else
 *   return C
 */
static int
acl_merge_trie(struct acl_build_context *context,
	struct rte_acl_node *node_a, struct rte_acl_node *node_b,
	uint32_t level, struct rte_acl_node **return_c)
{
	uint32_t n, m, ptrs_c, ptrs_b;
	uint32_t min_add_c, min_add_b;
	int node_intersect_type;
	struct rte_acl_bitset node_intersect;
	struct rte_acl_node *node_c;
	struct rte_acl_node *node_a_next;
	int node_b_refs;
	int node_a_refs;

	node_c = node_a;
	node_a_next = node_a->next;
	min_add_c = 0;
	min_add_b = 0;
	node_a_refs = node_a->num_ptrs;
	node_b_refs = 0;
	node_intersect_type = 0;

	/* Resolve leaf nodes (matches) */
	if (node_a->match_flag != 0) {
		acl_resolve_leaf(context, node_a, node_b, return_c);
		return 0;
	}

	/*
	 * Create node C as a copy of node A, and do: C = merge(A,B);
	 * If node A can be used instead (A==C), then later we'll
	 * destroy C and return A.
	 */
	if (level > 0)
		node_c = acl_dup_node(context, node_a);

	/*
	 * If the two node transitions intersect then merge the transitions.
	 * Check intersection for entire node (all pointers)
	 */
	node_intersect_type = acl_intersect_type(&node_c->values,
		&node_b->values,
		&node_intersect);

	if (node_intersect_type & ACL_INTERSECT) {

		min_add_b = node_b->min_add;
		node_b->min_add = node_b->num_ptrs;
		ptrs_b = node_b->num_ptrs;

		min_add_c = node_c->min_add;
		node_c->min_add = node_c->num_ptrs;
		ptrs_c = node_c->num_ptrs;

		for (n = 0; n < ptrs_c; n++) {
			if (node_c->ptrs[n].ptr == NULL) {
				node_a_refs--;
				continue;
			}
			node_c->ptrs[n].ptr->next = NULL;
			for (m = 0; m < ptrs_b; m++) {

				struct rte_acl_bitset child_intersect;
				int child_intersect_type;
				struct rte_acl_node *child_node_c = NULL;

				if (node_b->ptrs[m].ptr == NULL ||
						node_c->ptrs[n].ptr ==
						node_b->ptrs[m].ptr)
						continue;

				child_intersect_type = acl_intersect_type(
					&node_c->ptrs[n].values,
					&node_b->ptrs[m].values,
					&child_intersect);

				if ((child_intersect_type & ACL_INTERSECT) !=
						0) {
					if (acl_merge_trie(context,
							node_c->ptrs[n].ptr,
							node_b->ptrs[m].ptr,
							level + 1,
							&child_node_c))
						return 1;

					if (child_node_c != NULL &&
							child_node_c !=
							node_c->ptrs[n].ptr) {

						node_b_refs++;

						/*
						 * Added link from C to
						 * child_C for all transitions
						 * in the intersection.
						 */
						acl_add_ptr(context, node_c,
							child_node_c,
							&child_intersect);

						/*
						 * inc refs if pointer is not
						 * to node b.
						 */
						node_a_refs += (child_node_c !=
							node_b->ptrs[m].ptr);

						/*
						 * Remove intersection from C
						 * pointer.
						 */
						if (!acl_exclude(
							&node_c->ptrs[n].values,
							&node_c->ptrs[n].values,
							&child_intersect)) {
							acl_deref_ptr(context,
								node_c, n);
							node_c->ptrs[n].ptr =
								NULL;
							node_a_refs--;
						}
					}
				}
			}
		}

		/* Compact pointers */
		node_c->min_add = min_add_c;
		acl_compact_node_ptrs(node_c);
		node_b->min_add = min_add_b;
		acl_compact_node_ptrs(node_b);
	}

	/*
	 *  Copy pointers outside of the intersection from B to C
	 */
	if ((node_intersect_type & ACL_INTERSECT_B) != 0) {
		node_b_refs++;
		for (m = 0; m < node_b->num_ptrs; m++)
			if (node_b->ptrs[m].ptr != NULL)
				acl_copy_ptr(context, node_c,
					node_b, m, &node_intersect);
	}

	/*
	 * Free node C if top of trie is contained in A or B
	 *  if node C is a duplicate of node A &&
	 *     node C was not an existing duplicate
	 */
	if (node_c != node_a && node_c != node_a_next) {

		/*
		 * if the intersection has no references to the
		 * B side, then it is contained in A
		 */
		if (node_b_refs == 0) {
			acl_free_node(context, node_c);
			node_c = node_a;
		} else {
			/*
			 * if the intersection has no references to the
			 * A side, then it is contained in B.
			 */
			if (node_a_refs == 0) {
				acl_free_node(context, node_c);
				node_c = node_b;
			}
		}
	}

	if (return_c != NULL)
		*return_c = node_c;

	if (level == 0)
		acl_free_node(context, node_b);

	return 0;
}

/*
 * Reset current runtime fields before next build:
 *  - free allocated RT memory.
 *  - reset all RT related fields to zero.
 */
static void
acl_build_reset(struct rte_acl_ctx *ctx)
{
	rte_free(ctx->mem);
	memset(&ctx->num_categories, 0,
		sizeof(*ctx) - offsetof(struct rte_acl_ctx, num_categories));
}

static void
acl_gen_full_range(struct acl_build_context *context, struct rte_acl_node *root,
	struct rte_acl_node *end, int size, int level)
{
	struct rte_acl_node *node, *prev;
	uint32_t n;

	prev = root;
	for (n = size - 1; n > 0; n--) {
		node = acl_alloc_node(context, level++);
		acl_add_ptr_range(context, prev, node, 0, UINT8_MAX);
		prev = node;
	}
	acl_add_ptr_range(context, prev, end, 0, UINT8_MAX);
}

static void
acl_gen_range_mdl(struct acl_build_context *context, struct rte_acl_node *root,
	struct rte_acl_node *end, uint8_t lo, uint8_t hi, int size, int level)
{
	struct rte_acl_node *node;

	node = acl_alloc_node(context, level++);
	acl_add_ptr_range(context, root, node, lo, hi);
	acl_gen_full_range(context, node, end, size - 1, level);
}

static void
acl_gen_range_low(struct acl_build_context *context, struct rte_acl_node *root,
	struct rte_acl_node *end, const uint8_t *lo, int size, int level)
{
	struct rte_acl_node *node;
	uint32_t n;

	n = size - 1;
	if (n == 0) {
		acl_add_ptr_range(context, root, end, lo[0], UINT8_MAX);
		return;
	}

	node = acl_alloc_node(context, level++);
	acl_add_ptr_range(context, root, node, lo[n], lo[n]);

	/* generate lower-bound sub-trie */
	acl_gen_range_low(context, node, end, lo, n, level);

	/* generate middle sub-trie */
	if (n > 1 && lo[n - 1] != UINT8_MAX)
		acl_gen_range_mdl(context, node, end, lo[n - 1] + 1, UINT8_MAX,
			n, level);
}

static void
acl_gen_range_high(struct acl_build_context *context, struct rte_acl_node *root,
	struct rte_acl_node *end, const uint8_t *hi, int size, int level)
{
	struct rte_acl_node *node;
	uint32_t n;

	n = size - 1;
	if (n == 0) {
		acl_add_ptr_range(context, root, end, 0, hi[0]);
		return;
	}

	node = acl_alloc_node(context, level++);
	acl_add_ptr_range(context, root, node, hi[n], hi[n]);

	/* generate upper-bound sub-trie */
	acl_gen_range_high(context, node, end, hi, n, level);

	/* generate middle sub-trie */
	if (n > 1 && hi[n - 1] != 0)
		acl_gen_range_mdl(context, node, end, 0, hi[n - 1] - 1,
			n, level);
}

static struct rte_acl_node *
acl_gen_range_trie(struct acl_build_context *context,
	const void *min, const void *max,
	int size, int level, struct rte_acl_node **pend)
{
	int32_t k, n;
	uint8_t hi_ff, lo_00;
	struct rte_acl_node *node, *prev, *root;
	const uint8_t *lo;
	const uint8_t *hi;

	lo = min;
	hi = max;

	*pend = acl_alloc_node(context, level + size);
	root = acl_alloc_node(context, level++);
	prev = root;

	/* build common sub-trie till possible */
	for (n = size - 1; n > 0 && lo[n] == hi[n]; n--) {
		node = acl_alloc_node(context, level++);
		acl_add_ptr_range(context, prev, node, lo[n], hi[n]);
		prev = node;
	}

	/* no branch needed, just one sub-trie */
	if (n == 0) {
		acl_add_ptr_range(context, prev, *pend, lo[0], hi[0]);
		return root;
	}

	/* gather information about divergent paths */
	lo_00 = 0;
	hi_ff = UINT8_MAX;
	for (k = n - 1; k >= 0; k--) {
		hi_ff &= hi[k];
		lo_00 |= lo[k];
	}

	/* generate left (lower-bound) sub-trie */
	if (lo_00 != 0)
		acl_gen_range_low(context, prev, *pend, lo, n + 1, level);

	/* generate right (upper-bound) sub-trie */
	if (hi_ff != UINT8_MAX)
		acl_gen_range_high(context, prev, *pend, hi, n + 1, level);

	/* generate sub-trie in the middle */
	if (lo[n] + 1 != hi[n] || lo_00 == 0 || hi_ff == UINT8_MAX) {
		lo_00 = lo[n] + (lo_00 != 0);
		hi_ff = hi[n] - (hi_ff != UINT8_MAX);
		acl_gen_range_mdl(context, prev, *pend, lo_00, hi_ff,
			n + 1, level);
	}

	return root;
}

static struct rte_acl_node *
acl_gen_mask_trie(struct acl_build_context *context,
	const void *value, const void *mask,
	int size, int level, struct rte_acl_node **pend)
{
	int32_t n;
	struct rte_acl_node *root;
	struct rte_acl_node *node, *prev;
	struct rte_acl_bitset bits;
	const uint8_t *val = value;
	const uint8_t *msk = mask;

	root = acl_alloc_node(context, level++);
	prev = root;

	for (n = size - 1; n >= 0; n--) {
		node = acl_alloc_node(context, level++);
		acl_gen_mask(&bits, val[n] & msk[n], msk[n]);
		acl_add_ptr(context, prev, node, &bits);
		prev = node;
	}

	*pend = prev;
	return root;
}

static struct rte_acl_node *
build_trie(struct acl_build_context *context, struct rte_acl_build_rule *head,
	struct rte_acl_build_rule **last, uint32_t *count)
{
	uint32_t n, m;
	int field_index, node_count;
	struct rte_acl_node *trie;
	struct rte_acl_build_rule *prev, *rule;
	struct rte_acl_node *end, *merge, *root, *end_prev;
	const struct rte_acl_field *fld;

	prev = head;
	rule = head;
	*last = prev;

	trie = acl_alloc_node(context, 0);

	while (rule != NULL) {

		root = acl_alloc_node(context, 0);

		root->ref_count = 1;
		end = root;

		for (n = 0; n < rule->config->num_fields; n++) {

			field_index = rule->config->defs[n].field_index;
			fld = rule->f->field + field_index;
			end_prev = end;

			/* build a mini-trie for this field */
			switch (rule->config->defs[n].type) {

			case RTE_ACL_FIELD_TYPE_BITMASK:
				merge = acl_gen_mask_trie(context,
					&fld->value,
					&fld->mask_range,
					rule->config->defs[n].size,
					end->level + 1,
					&end);
				break;

			case RTE_ACL_FIELD_TYPE_MASK:
			{
				/*
				 * set msb for the size of the field and
				 * all higher bits.
				 */
				uint64_t mask;
				mask = RTE_ACL_MASKLEN_TO_BITMASK(
					fld->mask_range.u64,
					rule->config->defs[n].size);

				/* gen a mini-trie for this field */
				merge = acl_gen_mask_trie(context,
					&fld->value,
					(char *)&mask,
					rule->config->defs[n].size,
					end->level + 1,
					&end);
			}
			break;

			case RTE_ACL_FIELD_TYPE_RANGE:
				merge = acl_gen_range_trie(context,
					&rule->f->field[field_index].value,
					&rule->f->field[field_index].mask_range,
					rule->config->defs[n].size,
					end->level + 1,
					&end);
				break;

			default:
				RTE_LOG(ERR, ACL,
					"Error in rule[%u] type - %hhu\n",
					rule->f->data.userdata,
					rule->config->defs[n].type);
				return NULL;
			}

			/* merge this field on to the end of the rule */
			if (acl_merge_trie(context, end_prev, merge, 0,
					NULL) != 0) {
				return NULL;
			}
		}

		end->match_flag = ++context->num_build_rules;

		/*
		 * Setup the results for this rule.
		 * The result and priority of each category.
		 */
		if (end->mrt == NULL)
			end->mrt = acl_build_alloc(context, 1,
				sizeof(*end->mrt));

		for (m = context->cfg.num_categories; 0 != m--; ) {
			if (rule->f->data.category_mask & (1U << m)) {
				end->mrt->results[m] = rule->f->data.userdata;
				end->mrt->priority[m] = rule->f->data.priority;
			} else {
				end->mrt->results[m] = 0;
				end->mrt->priority[m] = 0;
			}
		}

		node_count = context->num_nodes;
		(*count)++;

		/* merge this rule into the trie */
		if (acl_merge_trie(context, trie, root, 0, NULL))
			return NULL;

		node_count = context->num_nodes - node_count;
		if (node_count > context->cur_node_max) {
			*last = prev;
			return trie;
		}

		prev = rule;
		rule = rule->next;
	}

	*last = NULL;
	return trie;
}

static void
acl_calc_wildness(struct rte_acl_build_rule *head,
	const struct rte_acl_config *config)
{
	uint32_t n;
	struct rte_acl_build_rule *rule;

	for (rule = head; rule != NULL; rule = rule->next) {

		for (n = 0; n < config->num_fields; n++) {

			double wild = 0;
			uint32_t bit_len = CHAR_BIT * config->defs[n].size;
			uint64_t msk_val = RTE_LEN2MASK(bit_len,
				typeof(msk_val));
			double size = bit_len;
			int field_index = config->defs[n].field_index;
			const struct rte_acl_field *fld = rule->f->field +
				field_index;

			switch (rule->config->defs[n].type) {
			case RTE_ACL_FIELD_TYPE_BITMASK:
				wild = (size - __builtin_popcountll(
					fld->mask_range.u64 & msk_val)) /
					size;
				break;

			case RTE_ACL_FIELD_TYPE_MASK:
				wild = (size - fld->mask_range.u32) / size;
				break;

			case RTE_ACL_FIELD_TYPE_RANGE:
				wild = (fld->mask_range.u64 & msk_val) -
					(fld->value.u64 & msk_val);
				wild = wild / msk_val;
				break;
			}

			rule->wildness[field_index] = (uint32_t)(wild * 100);
		}
	}
}

static void
acl_rule_stats(struct rte_acl_build_rule *head, struct rte_acl_config *config)
{
	struct rte_acl_build_rule *rule;
	uint32_t n, m, fields_deactivated = 0;
	uint32_t start = 0, deactivate = 0;
	int tally[RTE_ACL_MAX_LEVELS][TALLY_NUM];

	memset(tally, 0, sizeof(tally));

	for (rule = head; rule != NULL; rule = rule->next) {

		for (n = 0; n < config->num_fields; n++) {
			uint32_t field_index = config->defs[n].field_index;

			tally[n][TALLY_0]++;
			for (m = 1; m < RTE_DIM(wild_limits); m++) {
				if (rule->wildness[field_index] >=
						wild_limits[m])
					tally[n][m]++;
			}
		}

		for (n = config->num_fields - 1; n > 0; n--) {
			uint32_t field_index = config->defs[n].field_index;

			if (rule->wildness[field_index] == 100)
				tally[n][TALLY_DEPTH]++;
			else
				break;
		}
	}

	/*
	 * Look for any field that is always wild and drop it from the config
	 * Only deactivate if all fields for a given input loop are deactivated.
	 */
	for (n = 1; n < config->num_fields; n++) {
		if (config->defs[n].input_index !=
				config->defs[n - 1].input_index) {
			for (m = start; m < n; m++)
				tally[m][TALLY_DEACTIVATED] = deactivate;
			fields_deactivated += deactivate;
			start = n;
			deactivate = 1;
		}

		/* if the field is not always completely wild */
		if (tally[n][TALLY_100] != tally[n][TALLY_0])
			deactivate = 0;
	}

	for (m = start; m < n; m++)
		tally[m][TALLY_DEACTIVATED] = deactivate;

	fields_deactivated += deactivate;

	/* remove deactivated fields */
	if (fields_deactivated) {
		uint32_t k, l = 0;

		for (k = 0; k < config->num_fields; k++) {
			if (tally[k][TALLY_DEACTIVATED] == 0) {
				memmove(&tally[l][0], &tally[k][0],
					TALLY_NUM * sizeof(tally[0][0]));
				memmove(&config->defs[l++],
					&config->defs[k],
					sizeof(struct rte_acl_field_def));
			}
		}
		config->num_fields = l;
	}
}

static int
rule_cmp_wildness(struct rte_acl_build_rule *r1, struct rte_acl_build_rule *r2)
{
	uint32_t n;

	for (n = 1; n < r1->config->num_fields; n++) {
		int field_index = r1->config->defs[n].field_index;

		if (r1->wildness[field_index] != r2->wildness[field_index])
			return r1->wildness[field_index] -
				r2->wildness[field_index];
	}
	return 0;
}

/*
 * Split the rte_acl_build_rule list into two lists.
 */
static void
rule_list_split(struct rte_acl_build_rule *source,
	struct rte_acl_build_rule **list_a,
	struct rte_acl_build_rule **list_b)
{
	struct rte_acl_build_rule *fast;
	struct rte_acl_build_rule *slow;

	if (source == NULL || source->next == NULL) {
		/* length < 2 cases */
		*list_a = source;
		*list_b = NULL;
	} else {
		slow = source;
		fast = source->next;
		/* Advance 'fast' two nodes, and advance 'slow' one node */
		while (fast != NULL) {
			fast = fast->next;
			if (fast != NULL) {
				slow = slow->next;
				fast = fast->next;
			}
		}
		/* 'slow' is before the midpoint in the list, so split it in two
		   at that point. */
		*list_a = source;
		*list_b = slow->next;
		slow->next = NULL;
	}
}

/*
 * Merge two sorted lists.
 */
static struct rte_acl_build_rule *
rule_list_sorted_merge(struct rte_acl_build_rule *a,
	struct rte_acl_build_rule *b)
{
	struct rte_acl_build_rule *result = NULL;
	struct rte_acl_build_rule **last_next = &result;

	while (1) {
		if (a == NULL) {
			*last_next = b;
			break;
		} else if (b == NULL) {
			*last_next = a;
			break;
		}
		if (rule_cmp_wildness(a, b) >= 0) {
			*last_next = a;
			last_next = &a->next;
			a = a->next;
		} else {
			*last_next = b;
			last_next = &b->next;
			b = b->next;
		}
	}
	return result;
}

/*
 * Sort list of rules based on the rules wildness.
 * Use recursive mergesort algorithm.
 */
static struct rte_acl_build_rule *
sort_rules(struct rte_acl_build_rule *head)
{
	struct rte_acl_build_rule *a;
	struct rte_acl_build_rule *b;

	/* Base case -- length 0 or 1 */
	if (head == NULL || head->next == NULL)
		return head;

	/* Split head into 'a' and 'b' sublists */
	rule_list_split(head, &a, &b);

	/* Recursively sort the sublists */
	a = sort_rules(a);
	b = sort_rules(b);

	/* answer = merge the two sorted lists together */
	return rule_list_sorted_merge(a, b);
}

static uint32_t
acl_build_index(const struct rte_acl_config *config, uint32_t *data_index)
{
	uint32_t n, m;
	int32_t last_header;

	m = 0;
	last_header = -1;

	for (n = 0; n < config->num_fields; n++) {
		if (last_header != config->defs[n].input_index) {
			last_header = config->defs[n].input_index;
			data_index[m++] = config->defs[n].offset;
			if (config->defs[n].size > sizeof(uint32_t))
				data_index[m++] = config->defs[n].offset +
					sizeof(uint32_t);
		}
	}

	return m;
}

static struct rte_acl_build_rule *
build_one_trie(struct acl_build_context *context,
	struct rte_acl_build_rule *rule_sets[RTE_ACL_MAX_TRIES],
	uint32_t n, int32_t node_max)
{
	struct rte_acl_build_rule *last;
	struct rte_acl_config *config;

	config = rule_sets[n]->config;

	acl_rule_stats(rule_sets[n], config);
	rule_sets[n] = sort_rules(rule_sets[n]);

	context->tries[n].type = RTE_ACL_FULL_TRIE;
	context->tries[n].count = 0;

	context->tries[n].num_data_indexes = acl_build_index(config,
		context->data_indexes[n]);
	context->tries[n].data_index = context->data_indexes[n];

	context->cur_node_max = node_max;

	context->bld_tries[n].trie = build_trie(context, rule_sets[n],
		&last, &context->tries[n].count);

	return last;
}

static int
acl_build_tries(struct acl_build_context *context,
	struct rte_acl_build_rule *head)
{
	uint32_t n, num_tries;
	struct rte_acl_config *config;
	struct rte_acl_build_rule *last;
	struct rte_acl_build_rule *rule_sets[RTE_ACL_MAX_TRIES];

	config = head->config;
	rule_sets[0] = head;

	/* initialize tries */
	for (n = 0; n < RTE_DIM(context->tries); n++) {
		context->tries[n].type = RTE_ACL_UNUSED_TRIE;
		context->bld_tries[n].trie = NULL;
		context->tries[n].count = 0;
	}

	context->tries[0].type = RTE_ACL_FULL_TRIE;

	/* calc wildness of each field of each rule */
	acl_calc_wildness(head, config);

	for (n = 0;; n = num_tries) {

		num_tries = n + 1;

		last = build_one_trie(context, rule_sets, n, context->node_max);
		if (context->bld_tries[n].trie == NULL) {
			RTE_LOG(ERR, ACL, "Build of %u-th trie failed\n", n);
			return -ENOMEM;
		}

		/* Build of the last trie completed. */
		if (last == NULL)
			break;

		if (num_tries == RTE_DIM(context->tries)) {
			RTE_LOG(ERR, ACL,
				"Exceeded max number of tries: %u\n",
				num_tries);
			return -ENOMEM;
		}

		/* Trie is getting too big, split remaining rule set. */
		rule_sets[num_tries] = last->next;
		last->next = NULL;
		acl_free_node(context, context->bld_tries[n].trie);

		/* Create a new copy of config for remaining rules. */
		config = acl_build_alloc(context, 1, sizeof(*config));
		memcpy(config, rule_sets[n]->config, sizeof(*config));

		/* Make remaining rules use new config. */
		for (head = rule_sets[num_tries]; head != NULL;
				head = head->next)
			head->config = config;

		/*
		 * Rebuild the trie for the reduced rule-set.
		 * Don't try to split it any further.
		 */
		last = build_one_trie(context, rule_sets, n, INT32_MAX);
		if (context->bld_tries[n].trie == NULL || last != NULL) {
			RTE_LOG(ERR, ACL, "Build of %u-th trie failed\n", n);
			return -ENOMEM;
		}

	}

	context->num_tries = num_tries;
	return 0;
}

static void
acl_build_log(const struct acl_build_context *ctx)
{
	uint32_t n;

	RTE_LOG(DEBUG, ACL, "Build phase for ACL \"%s\":\n"
		"node limit for tree split: %u\n"
		"nodes created: %u\n"
		"memory consumed: %zu\n",
		ctx->acx->name,
		ctx->node_max,
		ctx->num_nodes,
		ctx->pool.alloc);

	for (n = 0; n < RTE_DIM(ctx->tries); n++) {
		if (ctx->tries[n].count != 0)
			RTE_LOG(DEBUG, ACL,
				"trie %u: number of rules: %u, indexes: %u\n",
				n, ctx->tries[n].count,
				ctx->tries[n].num_data_indexes);
	}
}

static int
acl_build_rules(struct acl_build_context *bcx)
{
	struct rte_acl_build_rule *br, *head;
	const struct rte_acl_rule *rule;
	uint32_t *wp;
	uint32_t fn, i, n, num;
	size_t ofs, sz;

	fn = bcx->cfg.num_fields;
	n = bcx->acx->num_rules;
	ofs = n * sizeof(*br);
	sz = ofs + n * fn * sizeof(*wp);

	br = tb_alloc(&bcx->pool, sz);

	wp = (uint32_t *)((uintptr_t)br + ofs);
	num = 0;
	head = NULL;

	for (i = 0; i != n; i++) {
		rule = (const struct rte_acl_rule *)
			((uintptr_t)bcx->acx->rules + bcx->acx->rule_sz * i);
		if ((rule->data.category_mask & bcx->category_mask) != 0) {
			br[num].next = head;
			br[num].config = &bcx->cfg;
			br[num].f = rule;
			br[num].wildness = wp;
			wp += fn;
			head = br + num;
			num++;
		}
	}

	bcx->num_rules = num;
	bcx->build_rules = head;

	return 0;
}

/*
 * Copy data_indexes for each trie into RT location.
 */
static void
acl_set_data_indexes(struct rte_acl_ctx *ctx)
{
	uint32_t i, n, ofs;

	ofs = 0;
	for (i = 0; i != ctx->num_tries; i++) {
		n = ctx->trie[i].num_data_indexes;
		memcpy(ctx->data_indexes + ofs, ctx->trie[i].data_index,
			n * sizeof(ctx->data_indexes[0]));
		ctx->trie[i].data_index = ctx->data_indexes + ofs;
		ofs += ACL_MAX_INDEXES;
	}
}

/*
 * Internal routine, performs 'build' phase of trie generation:
 * - setups build context.
 * - analyzes given set of rules.
 * - builds internal tree(s).
 */
static int
acl_bld(struct acl_build_context *bcx, struct rte_acl_ctx *ctx,
	const struct rte_acl_config *cfg, uint32_t node_max)
{
	int32_t rc;

	/* setup build context. */
	memset(bcx, 0, sizeof(*bcx));
	bcx->acx = ctx;
	bcx->pool.alignment = ACL_POOL_ALIGN;
	bcx->pool.min_alloc = ACL_POOL_ALLOC_MIN;
	bcx->cfg = *cfg;
	bcx->category_mask = RTE_LEN2MASK(bcx->cfg.num_categories,
		typeof(bcx->category_mask));
	bcx->node_max = node_max;

	rc = sigsetjmp(bcx->pool.fail, 0);

	/* build phase runs out of memory. */
	if (rc != 0) {
		RTE_LOG(ERR, ACL,
			"ACL context: %s, %s() failed with error code: %d\n",
			bcx->acx->name, __func__, rc);
		return rc;
	}

	/* Create a build rules copy. */
	rc = acl_build_rules(bcx);
	if (rc != 0)
		return rc;

	/* No rules to build for that context+config */
	if (bcx->build_rules == NULL) {
		rc = -EINVAL;
	} else {
		/* build internal trie representation. */
		rc = acl_build_tries(bcx, bcx->build_rules);
	}
	return rc;
}

/*
 * Check that parameters for acl_build() are valid.
 */
static int
acl_check_bld_param(struct rte_acl_ctx *ctx, const struct rte_acl_config *cfg)
{
	static const size_t field_sizes[] = {
		sizeof(uint8_t), sizeof(uint16_t),
		sizeof(uint32_t), sizeof(uint64_t),
	};

	uint32_t i, j;

	if (ctx == NULL || cfg == NULL || cfg->num_categories == 0 ||
			cfg->num_categories > RTE_ACL_MAX_CATEGORIES ||
			cfg->num_fields == 0 ||
			cfg->num_fields > RTE_ACL_MAX_FIELDS)
		return -EINVAL;

	for (i = 0; i != cfg->num_fields; i++) {
		if (cfg->defs[i].type > RTE_ACL_FIELD_TYPE_BITMASK) {
			RTE_LOG(ERR, ACL,
			"ACL context: %s, invalid type: %hhu for %u-th field\n",
			ctx->name, cfg->defs[i].type, i);
			return -EINVAL;
		}
		for (j = 0;
				j != RTE_DIM(field_sizes) &&
				cfg->defs[i].size != field_sizes[j];
				j++)
			;

		if (j == RTE_DIM(field_sizes)) {
			RTE_LOG(ERR, ACL,
			"ACL context: %s, invalid size: %hhu for %u-th field\n",
			ctx->name, cfg->defs[i].size, i);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * With current ACL implementation first field in the rule definition
 * has always to be one byte long. Though for optimising *classify*
 * implementation it might be useful to be able to use 4B reads
 * (as we do for rest of the fields).
 * This function checks input config to determine is it safe to do 4B
 * loads for first ACL field. For that we need to make sure that
 * first field in our rule definition doesn't have the biggest offset,
 * i.e. we still do have other fields located after the first one.
 * Contrary if first field has the largest offset, then it means
 * first field can occupy the very last byte in the input data buffer,
 * and we have to do single byte load for it.
 */
static uint32_t
get_first_load_size(const struct rte_acl_config *cfg)
{
	uint32_t i, max_ofs, ofs;

	ofs = 0;
	max_ofs = 0;

	for (i = 0; i != cfg->num_fields; i++) {
		if (cfg->defs[i].field_index == 0)
			ofs = cfg->defs[i].offset;
		else if (max_ofs < cfg->defs[i].offset)
			max_ofs = cfg->defs[i].offset;
	}

	return (ofs < max_ofs) ? sizeof(uint32_t) : sizeof(uint8_t);
}

int
rte_acl_build(struct rte_acl_ctx *ctx, const struct rte_acl_config *cfg)
{
	int32_t rc;
	uint32_t n;
	size_t max_size;
	struct acl_build_context bcx;

	rc = acl_check_bld_param(ctx, cfg);
	if (rc != 0)
		return rc;

	acl_build_reset(ctx);

	if (cfg->max_size == 0) {
		n = NODE_MIN;
		max_size = SIZE_MAX;
	} else {
		n = NODE_MAX;
		max_size = cfg->max_size;
	}

	for (rc = -ERANGE; n >= NODE_MIN && rc == -ERANGE; n /= 2) {

		/* perform build phase. */
		rc = acl_bld(&bcx, ctx, cfg, n);

		if (rc == 0) {
			/* allocate and fill run-time  structures. */
			rc = rte_acl_gen(ctx, bcx.tries, bcx.bld_tries,
				bcx.num_tries, bcx.cfg.num_categories,
				ACL_MAX_INDEXES * RTE_DIM(bcx.tries) *
				sizeof(ctx->data_indexes[0]), max_size);
			if (rc == 0) {
				/* set data indexes. */
				acl_set_data_indexes(ctx);

				/* determine can we always do 4B load */
				ctx->first_load_sz = get_first_load_size(cfg);

				/* copy in build config. */
				ctx->config = *cfg;
			}
		}

		acl_build_log(&bcx);

		/* cleanup after build. */
		tb_free_pool(&bcx.pool);
	}

	return rc;
}
