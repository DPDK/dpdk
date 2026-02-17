/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 Mellanox Technologies, Ltd
 *
 * This file contains the async flow API implementation
 * for the flow-perf application.
 */

#include <stdio.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>

#include "actions_gen.h"
#include "async_flow.h"
#include "flow_gen.h"
#include "items_gen.h"

/* Per-port async flow resources */
static struct async_flow_resources port_resources[MAX_PORTS];

int
async_flow_init_port(uint16_t port_id, uint32_t nb_queues, uint32_t queue_size,
		     uint64_t *flow_items, uint64_t *flow_actions, uint64_t *flow_attrs,
		     uint8_t flow_group, uint32_t rules_count)
{
	struct rte_flow_port_info port_info = {0};
	struct rte_flow_queue_info queue_info = {0};
	struct rte_flow_error error = {0};
	struct rte_flow_port_attr port_attr = {0};
	struct rte_flow_queue_attr *queue_attr = alloca(sizeof(struct rte_flow_queue_attr));
	const struct rte_flow_queue_attr **queue_attr_list =
		alloca(sizeof(struct rte_flow_queue_attr) * nb_queues);
	struct rte_flow_pattern_template_attr pt_attr = {0};
	struct rte_flow_actions_template_attr at_attr = {0};
	struct rte_flow_template_table_attr table_attr = {0};
	struct rte_flow_item pattern[MAX_ITEMS_NUM];
	struct rte_flow_action actions[MAX_ACTIONS_NUM];
	struct rte_flow_action action_masks[MAX_ACTIONS_NUM];
	struct async_flow_resources *res;
	bool need_wire_orig_table = false;
	uint32_t i;
	int ret;

	if (port_id >= MAX_PORTS)
		return -1;

	res = &port_resources[port_id];
	memset(res, 0, sizeof(*res));

	/* Query port flow info */
	ret = rte_flow_info_get(port_id, &port_info, &queue_info, &error);
	if (ret != 0) {
		fprintf(stderr, "Port %u: rte_flow_info_get failed: %s\n", port_id,
			error.message ? error.message : "(no message)");
		return ret;
	}

	/* Limit to device capabilities if reported */
	if (port_info.max_nb_queues != 0 && port_info.max_nb_queues != UINT32_MAX &&
	    nb_queues > port_info.max_nb_queues)
		nb_queues = port_info.max_nb_queues;
	if (queue_info.max_size != 0 && queue_info.max_size != UINT32_MAX &&
	    queue_size > queue_info.max_size)
		queue_size = queue_info.max_size;

	queue_attr->size = queue_size;
	for (i = 0; i < nb_queues; i++)
		queue_attr_list[i] = queue_attr;

	ret = rte_flow_configure(port_id, &port_attr, nb_queues, queue_attr_list, &error);
	if (ret != 0) {
		fprintf(stderr, "Port %u: rte_flow_configure failed (ret=%d, type=%d): %s\n",
			port_id, ret, error.type, error.message ? error.message : "(no message)");
		return ret;
	}

	/* Create pattern template */
	for (i = 0; i < MAX_ATTRS_NUM; i++) {
		if (flow_attrs[i] == 0)
			break;
		if (flow_attrs[i] & INGRESS)
			pt_attr.ingress = 1;
		else if (flow_attrs[i] & EGRESS)
			pt_attr.egress = 1;
		else if (flow_attrs[i] & TRANSFER)
			pt_attr.transfer = 1;
	}
	/* Enable relaxed matching for better performance */
	pt_attr.relaxed_matching = 1;

	memset(pattern, 0, sizeof(pattern));
	memset(actions, 0, sizeof(actions));
	memset(action_masks, 0, sizeof(action_masks));

	fill_items_template(pattern, flow_items, 0, 0);

	res->pattern_template =
		rte_flow_pattern_template_create(port_id, &pt_attr, pattern, &error);
	if (res->pattern_template == NULL) {
		fprintf(stderr, "Port %u: pattern template create failed: %s\n", port_id,
			error.message ? error.message : "(no message)");
		return -1;
	}

	/* Create actions template */
	at_attr.ingress = pt_attr.ingress;
	at_attr.egress = pt_attr.egress;
	at_attr.transfer = pt_attr.transfer;

	fill_actions_template(actions, action_masks, flow_actions, &need_wire_orig_table);

	res->actions_template =
		rte_flow_actions_template_create(port_id, &at_attr, actions, action_masks, &error);
	if (res->actions_template == NULL) {
		fprintf(stderr, "Port %u: actions template create failed: %s\n", port_id,
			error.message ? error.message : "(no message)");
		rte_flow_pattern_template_destroy(port_id, res->pattern_template, &error);
		res->pattern_template = NULL;
		return -1;
	}

	/* Create template table */
	table_attr.flow_attr.group = flow_group;
	table_attr.flow_attr.priority = 0;
	table_attr.flow_attr.ingress = pt_attr.ingress;
	table_attr.flow_attr.egress = pt_attr.egress;
	table_attr.flow_attr.transfer = pt_attr.transfer;
	table_attr.nb_flows = rules_count;

	if (pt_attr.transfer && need_wire_orig_table)
		table_attr.specialize = RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG;

	res->table = rte_flow_template_table_create(port_id, &table_attr, &res->pattern_template, 1,
						    &res->actions_template, 1, &error);
	if (res->table == NULL) {
		fprintf(stderr, "Port %u: template table create failed: %s\n", port_id,
			error.message ? error.message : "(no message)");
		rte_flow_actions_template_destroy(port_id, res->actions_template, &error);
		rte_flow_pattern_template_destroy(port_id, res->pattern_template, &error);
		res->pattern_template = NULL;
		res->actions_template = NULL;
		return -1;
	}

	res->table_capacity = rules_count;
	res->initialized = true;

	printf("Port %u: Async flow engine initialized (queues=%u, queue_size=%u)\n", port_id,
	       nb_queues, queue_size);

	return 0;
}

struct rte_flow *
async_generate_flow(uint16_t port_id, uint32_t queue_id, uint64_t *flow_items,
		    uint64_t *flow_actions, uint32_t counter, uint16_t hairpinq,
		    uint64_t encap_data, uint64_t decap_data, uint16_t dst_port, uint8_t core_idx,
		    uint8_t rx_queues_count, bool unique_data, bool postpone,
		    struct rte_flow_error *error)
{
	struct async_flow_resources *res;
	struct rte_flow_item items[MAX_ITEMS_NUM];
	struct rte_flow_action actions[MAX_ACTIONS_NUM];
	struct rte_flow_op_attr op_attr = {
		.postpone = postpone,
	};
	struct rte_flow *flow;

	if (port_id >= MAX_PORTS) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Invalid port ID");
		return NULL;
	}

	res = &port_resources[port_id];
	if (!res->initialized) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Async flow resources not initialized");
		return NULL;
	}

	/* Fill pattern items with actual values */
	memset(items, 0, sizeof(items));
	fill_items(items, flow_items, counter, core_idx);

	/* Fill actions with actual values */
	memset(actions, 0, sizeof(actions));
	fill_actions(actions, flow_actions, counter, JUMP_ACTION_TABLE, hairpinq, encap_data,
		     decap_data, core_idx, unique_data, rx_queues_count, dst_port);

	/* Create flow asynchronously */
	flow = rte_flow_async_create(port_id, queue_id, &op_attr, res->table, items, 0, actions, 0,
				     NULL, error);

	return flow;
}

void
async_flow_cleanup_port(uint16_t port_id)
{
	struct async_flow_resources *res;
	struct rte_flow_error error;
	struct rte_flow_op_result results[64];
	int ret, i;

	if (port_id >= MAX_PORTS)
		return;

	res = &port_resources[port_id];
	if (!res->initialized)
		return;

	/* Drain any pending async completions from flow flush */
	for (i = 0; i < 100; i++) { /* Max iterations to avoid infinite loop */
		rte_flow_push(port_id, 0, &error);
		ret = rte_flow_pull(port_id, 0, results, 64, &error);
		if (ret <= 0)
			break;
	}

	if (res->table != NULL) {
		rte_flow_template_table_destroy(port_id, res->table, &error);
		res->table = NULL;
	}

	if (res->actions_template != NULL) {
		rte_flow_actions_template_destroy(port_id, res->actions_template, &error);
		res->actions_template = NULL;
	}

	if (res->pattern_template != NULL) {
		rte_flow_pattern_template_destroy(port_id, res->pattern_template, &error);
		res->pattern_template = NULL;
	}

	res->initialized = false;
}
