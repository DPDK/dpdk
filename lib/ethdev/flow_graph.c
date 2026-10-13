/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_errno.h>

#include "rte_ethdev.h"
#include "rte_flow.h"
#include "rte_flow_driver.h"
#include "flow_graph.h"

#define FLOW_GRAPH_LOG(level, fmt, ...) \
	RTE_ETHDEV_LOG_LINE(level, "FLOW GRAPH: %s(): " fmt, __func__, ##__VA_ARGS__)

static const char *
flow_item_type_to_str(enum rte_flow_item_type type)
{
	const char *name;
	int ret;

	ret = rte_flow_conv(RTE_FLOW_CONV_OP_ITEM_NAME_PTR,
			&name, sizeof(name), (const void *)(uintptr_t)type, NULL);
	if (ret < 0)
		return "UNKNOWN";

	return name;
}

static bool
flow_graph_node_is_expected(const struct flow_graph_node *node,
		const struct rte_flow_item *item, struct rte_flow_error *error)
{
	flow_graph_node_expect_t c = node->constraints;
	bool has_spec = (item->spec != NULL);
	bool has_mask = (item->mask != NULL);
	bool has_last = (item->last != NULL);
	const char *msg;

	/* mask or last without spec is invalid per rte_flow API */
	if (!has_spec && (has_mask || has_last)) {
		msg = has_mask ? "Unexpected mask without spec in flow item" :
				"Unexpected last without spec in flow item";
		goto fail;
	}

	if (c == FLOW_GRAPH_NODE_EXPECT_NONE)
		return true;
	if ((c & FLOW_GRAPH_NODE_EXPECT_EMPTY) && !has_spec)
		return true;
	if ((c & FLOW_GRAPH_NODE_EXPECT_SPEC) && has_spec && !has_last)
		return true;
	if ((c & FLOW_GRAPH_NODE_EXPECT_RANGE) && has_spec && has_last)
		return true;

	if (!has_spec)
		msg = "Missing spec in flow item";
	else if (has_last)
		msg = "Unexpected last in flow item";
	else if (c & FLOW_GRAPH_NODE_EXPECT_RANGE)
		msg = "Missing last in flow item";
	else
		msg = "Unexpected spec in flow item";
fail:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item, msg);

	return false;
}

static bool
flow_graph_item_is_ignored(const struct flow_graph *graph,
		enum rte_flow_item_type fi_type)
{
	const enum rte_flow_item_type *ignored;

	if (fi_type == RTE_FLOW_ITEM_TYPE_VOID)
		return true;

	if (graph->ignore_nodes == NULL)
		return false;

	for (ignored = graph->ignore_nodes; *ignored != RTE_FLOW_ITEM_TYPE_END; ignored++) {
		if (*ignored == fi_type)
			return true;
	}

	return false;
}

static size_t
flow_graph_get_node_idx(const struct flow_graph *graph,
		const struct flow_graph_node *node)
{
	return (size_t)(node - graph->nodes);
}

static bool
flow_graph_node_is_valid(const struct flow_graph *graph,
		const struct flow_graph_node *node,
		struct rte_flow_error *error)
{
	size_t node_idx;

	if (node == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Flow graph node pointer is NULL");
		return false;
	}

	if (node->name == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, node,
				"Flow graph node name is not defined");
		return false;
	}

	node_idx = flow_graph_get_node_idx(graph, node);

	/* first node can't have callbacks because there's no flow item */
	if (node_idx == FLOW_GRAPH_NODE_FIRST &&
			(node->validate != NULL || node->process != NULL)) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, node,
				"Flow graph start node callbacks are not allowed");
		return false;
	}

	/* all non-END nodes must have edges */
	if ((node->type != RTE_FLOW_ITEM_TYPE_END || node_idx == FLOW_GRAPH_NODE_FIRST) &&
			graph->edges[node_idx].next == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, node,
				"Flow graph edge list is not defined for non-END node");
		return false;
	}

	return true;
}

static const struct flow_graph_node *
flow_graph_find_next_node(const struct flow_graph *graph,
		const struct flow_graph_node *cur_node,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	enum rte_flow_item_type next_type = item->type;
	const size_t *next_nodes;
	size_t cur_idx, edge_idx;

	cur_idx = flow_graph_get_node_idx(graph, cur_node);
	next_nodes = graph->edges[cur_idx].next;

	for (edge_idx = 0; next_nodes[edge_idx] != FLOW_GRAPH_NODE_EDGE_END; edge_idx++) {
		const struct flow_graph_node *tmp =
				&graph->nodes[next_nodes[edge_idx]];
		if (!flow_graph_node_is_valid(graph, tmp, error))
			return NULL;
		if (tmp->type == next_type)
			return tmp;
	}

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
		item, "Pattern item not supported");

	return NULL;
}

static int
flow_graph_visit_node(const struct flow_graph_node *node, void *ctx,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	int ret;

	if (!flow_graph_node_is_expected(node, item, error))
		return -rte_errno;

	if (node->validate != NULL) {
		FLOW_GRAPH_LOG(DEBUG, "validating node: %s", node->name);
		ret = node->validate(ctx, item, error);
		if (ret != 0)
			return ret;
	}

	if (node->process != NULL) {
		FLOW_GRAPH_LOG(DEBUG, "processing node: %s", node->name);
		ret = node->process(ctx, item, error);
		if (ret != 0)
			return ret;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(flow_graph_parse)
int
flow_graph_parse(const struct flow_graph *graph,
		const struct rte_flow_item *pattern,
		struct rte_flow_error *error,
		void *ctx)
{
	const struct flow_graph_node *cur_node;
	const struct rte_flow_item *item;
	struct rte_flow_error local_error = {0};
	int ret;

	/* error may be NULL, but we want the message for logging */
	if (error == NULL)
		error = &local_error;

	if (graph == NULL || graph->nodes == NULL || graph->edges == NULL) {
		FLOW_GRAPH_LOG(DEBUG, "flow graph is not defined");
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Flow graph is not defined");
	}
	if (pattern == NULL) {
		FLOW_GRAPH_LOG(DEBUG, "flow pattern is NULL");
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"Flow pattern is NULL");
	}

	/* use start node as traversal anchor */
	cur_node = &graph->nodes[FLOW_GRAPH_NODE_FIRST];

	if (!flow_graph_node_is_valid(graph, cur_node, error)) {
		FLOW_GRAPH_LOG(DEBUG, "invalid start node: %s", error->message);
		return -rte_errno;
	}

	item = pattern;
	do {
		const struct flow_graph_node *next_node;

		FLOW_GRAPH_LOG(DEBUG, "current node: %s (%s)",
				cur_node->name, flow_item_type_to_str(cur_node->type));

		if (flow_graph_item_is_ignored(graph, item->type)) {
			FLOW_GRAPH_LOG(DEBUG, "ignored pattern item: %s",
					flow_item_type_to_str(item->type));
			continue;
		}
		FLOW_GRAPH_LOG(DEBUG, "next pattern item: %s",
				flow_item_type_to_str(item->type));

		next_node = flow_graph_find_next_node(graph, cur_node, item, error);
		if (next_node == NULL) {
			ret = -rte_errno;
			goto fail;
		}
		FLOW_GRAPH_LOG(DEBUG, "found next node: %s (%s)", next_node->name,
				flow_item_type_to_str(next_node->type));

		ret = flow_graph_visit_node(next_node, ctx, item, error);
		if (ret != 0)
			goto fail;
		FLOW_GRAPH_LOG(DEBUG, "visited node: %s", next_node->name);

		cur_node = next_node;
	} while ((item++)->type != RTE_FLOW_ITEM_TYPE_END);

	return 0;
fail:
	FLOW_GRAPH_LOG(DEBUG, "failed to transition from node '%s' to flow item '%s': %s",
			cur_node->name, flow_item_type_to_str(item->type), error->message);
	return ret;
}
