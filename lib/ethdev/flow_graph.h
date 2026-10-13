/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _FLOW_GRAPH_H_
#define _FLOW_GRAPH_H_

/**
 * @file
 * Flow Graph
 *
 * Graph-based rte_flow pattern parser for drivers.
 *
 * @warning
 * Internal API for drivers only. Applications must not use it.
 */

#include <rte_compat.h>
#include <rte_flow.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLOW_GRAPH_NODE_FIRST (0)
/* Edge array termination sentinel (not a valid node index). */
#define FLOW_GRAPH_NODE_EDGE_END SIZE_MAX

/**
 * Common item constraints a node can declare instead of checking them in callbacks.
 * Flags may be ORed; the item is accepted if any one of them is satisfied.
 *
 * Not having spec is invalid. A NULL mask can be accepted but the driver must
 * interpret it as the item's default mask (see RTE_FLOW_CONV_OP_ITEM_DEFAULT_MASK_PTR).
 */
enum flow_graph_node_expect {
	FLOW_GRAPH_NODE_EXPECT_NONE =  0,            /**< Any valid spec/mask/last combination. */
	FLOW_GRAPH_NODE_EXPECT_EMPTY = RTE_BIT32(0), /**< spec, mask, last must be NULL. */
	FLOW_GRAPH_NODE_EXPECT_SPEC =  RTE_BIT32(1),
			/**< spec required, mask optional, last must be NULL. */
	FLOW_GRAPH_NODE_EXPECT_RANGE = RTE_BIT32(2), /**< spec and last required, mask optional. */
};

/* ORed combination of flow_graph_node_expect flags. */
typedef uint32_t flow_graph_node_expect_t;

/**
 * Node validation callback.
 *
 * Called after the node's constraints are met. Should perform all
 * driver-specific checks on the item.
 *
 * @param ctx
 *   Driver context passed to flow_graph_parse().
 * @param item
 *   Flow item being validated.
 * @param error
 *   Flow error to fill on failure.
 * @return
 *   0 on success, or the return value of rte_flow_error_set() on failure.
 */
typedef int (*flow_graph_node_validate_fn)(
	const void *ctx,
	const struct rte_flow_item *item,
	struct rte_flow_error *error);

/**
 * Node processing callback.
 *
 * Called after validation succeeds. Extracts item fields into driver state.
 *
 * @param ctx
 *   Driver context passed to flow_graph_parse().
 * @param item
 *   Flow item to process.
 * @param error
 *   Flow error to fill on failure.
 * @return
 *   0 on success, or the return value of rte_flow_error_set() on failure.
 */
typedef int (*flow_graph_node_process_fn)(
	void *ctx,
	const struct rte_flow_item *item,
	struct rte_flow_error *error);

/**
 * Graph node definition.
 *
 * Node validity rules:
 * - all nodes must define a name,
 * - all non-END nodes must define an edge list,
 * - start node must not define validation/processing callbacks.
 */
struct flow_graph_node {
	const char *name;                      /**< Node name, used for logging. */
	enum rte_flow_item_type type;          /**< Flow item type to match. */
	flow_graph_node_expect_t constraints;  /**< Common item constraints (ORed). */
	flow_graph_node_validate_fn validate;  /**< Validation callback (optional). */
	flow_graph_node_process_fn process;    /**< Processing callback (optional). */
};

/**
 * Graph edge definition.
 *
 * Allowed transitions from a node. Successor item types must be unique,
 * as the parser does not backtrack.
 */
struct flow_graph_edge {
	/** Array of valid successor nodes, terminated by FLOW_GRAPH_NODE_EDGE_END. */
	size_t *next;
};

/**
 * Driver-defined flow graph.
 *
 * Node and edge tables are trusted to be well-formed and are not hardened
 * against malformed definitions.
 */
struct flow_graph {
	struct flow_graph_node *nodes;
	struct flow_graph_edge *edges;
	/** Additional node types to ignore, terminated by RTE_FLOW_ITEM_TYPE_END. */
	enum rte_flow_item_type *ignore_nodes;
};

/**
 * Parse a flow pattern using the flow graph.
 *
 * Walks the pattern through the graph, invoking node callbacks for each item.
 * VOID items and items in the graph's ignore list are skipped.
 *
 * @param graph
 *   Driver's flow graph.
 * @param pattern
 *   Pattern terminated by RTE_FLOW_ITEM_TYPE_END.
 * @param error
 *   Flow error to fill on failure (may be NULL).
 * @param ctx
 *   Driver context passed to node callbacks.
 * @return
 *   0 on success, negative errno on failure.
 */
__rte_internal
int
flow_graph_parse(const struct flow_graph *graph, const struct rte_flow_item *pattern,
		struct rte_flow_error *error, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _FLOW_GRAPH_H_ */
