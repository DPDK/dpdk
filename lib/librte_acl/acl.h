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

#ifndef	_ACL_H_
#define	_ACL_H_

#ifdef __cplusplus
extern"C" {
#endif /* __cplusplus */

#define RTE_ACL_QUAD_MAX	5
#define RTE_ACL_QUAD_SIZE	4
#define RTE_ACL_QUAD_SINGLE	UINT64_C(0x7f7f7f7f00000000)

#define RTE_ACL_SINGLE_TRIE_SIZE	2000

#define RTE_ACL_DFA_MAX		UINT8_MAX
#define RTE_ACL_DFA_SIZE	(UINT8_MAX + 1)

typedef int bits_t;

#define	RTE_ACL_BIT_SET_SIZE	((UINT8_MAX + 1) / (sizeof(bits_t) * CHAR_BIT))

struct rte_acl_bitset {
	bits_t             bits[RTE_ACL_BIT_SET_SIZE];
};

#define	RTE_ACL_NODE_DFA	(0 << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_SINGLE	(1U << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_QEXACT	(2U << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_QRANGE	(3U << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_MATCH	(4U << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_TYPE	(7U << RTE_ACL_TYPE_SHIFT)
#define	RTE_ACL_NODE_UNDEFINED	UINT32_MAX

/*
 * Structure of a node is a set of ptrs and each ptr has a bit map
 * of values associated with this transition.
 */
struct rte_acl_ptr_set {
	struct rte_acl_bitset values;	/* input values associated with ptr */
	struct rte_acl_node  *ptr;	/* transition to next node */
};

struct rte_acl_classifier_results {
	int results[RTE_ACL_MAX_CATEGORIES];
};

struct rte_acl_match_results {
	uint32_t results[RTE_ACL_MAX_CATEGORIES];
	int32_t priority[RTE_ACL_MAX_CATEGORIES];
};

struct rte_acl_node {
	uint64_t node_index;  /* index for this node */
	uint32_t level;       /* level 0-n in the trie */
	uint32_t ref_count;   /* ref count for this node */
	struct rte_acl_bitset  values;
	/* set of all values that map to another node
	 * (union of bits in each transition.
	 */
	uint32_t                num_ptrs; /* number of ptr_set in use */
	uint32_t                max_ptrs; /* number of allocated ptr_set */
	uint32_t                min_add;  /* number of ptr_set per allocation */
	struct rte_acl_ptr_set *ptrs;     /* transitions array for this node */
	int32_t                 match_flag;
	int32_t                 match_index; /* index to match data */
	uint32_t                node_type;
	int32_t                 fanout;
	/* number of ranges (transitions w/ consecutive bits) */
	int32_t                 id;
	struct rte_acl_match_results *mrt; /* only valid when match_flag != 0 */
	char                         transitions[RTE_ACL_QUAD_SIZE];
	/* boundaries for ranged node */
	struct rte_acl_node     *next;
	/* free list link or pointer to duplicate node during merge */
	struct rte_acl_node     *prev;
	/* points to node from which this node was duplicated */

	uint32_t                subtree_id;
	uint32_t                subtree_ref_count;

};
enum {
	RTE_ACL_SUBTREE_NODE = 0x80000000
};

/*
 * Types of tries used to generate runtime structure(s)
 */
enum {
	RTE_ACL_FULL_TRIE = 0,
	RTE_ACL_NOSRC_TRIE = 1,
	RTE_ACL_NODST_TRIE = 2,
	RTE_ACL_NOPORTS_TRIE = 4,
	RTE_ACL_NOVLAN_TRIE = 8,
	RTE_ACL_UNUSED_TRIE = 0x80000000
};


/** MAX number of tries per one ACL context.*/
#define RTE_ACL_MAX_TRIES	8

/** Max number of characters in PM name.*/
#define RTE_ACL_NAMESIZE	32


struct rte_acl_trie {
	uint32_t        type;
	uint32_t        count;
	int32_t         smallest;  /* smallest rule in this trie */
	uint32_t        root_index;
	const uint32_t *data_index;
	uint32_t        num_data_indexes;
};

struct rte_acl_bld_trie {
	struct rte_acl_node *trie;
};

struct rte_acl_ctx {
	TAILQ_ENTRY(rte_acl_ctx) next;    /**< Next in list. */
	char                name[RTE_ACL_NAMESIZE];
	/** Name of the ACL context. */
	int32_t             socket_id;
	/** Socket ID to allocate memory from. */
	void               *rules;
	uint32_t            max_rules;
	uint32_t            rule_sz;
	uint32_t            num_rules;
	uint32_t            num_categories;
	uint32_t            num_tries;
	uint32_t            match_index;
	uint64_t            no_match;
	uint64_t            idle;
	uint64_t           *trans_table;
	uint32_t           *data_indexes;
	struct rte_acl_trie trie[RTE_ACL_MAX_TRIES];
	void               *mem;
	size_t              mem_sz;
	struct rte_acl_config config; /* copy of build config. */
};

int rte_acl_gen(struct rte_acl_ctx *ctx, struct rte_acl_trie *trie,
	struct rte_acl_bld_trie *node_bld_trie, uint32_t num_tries,
	uint32_t num_categories, uint32_t data_index_sz, int match_num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ACL_H_ */
