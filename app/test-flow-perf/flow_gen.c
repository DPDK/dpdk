/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * The file contains the implementations of the method to
 * fill items, actions & attributes in their corresponding
 * arrays, and then generate rte_flow rule.
 *
 * After the generation. The rule goes to validation then
 * creation state and then return the results.
 */

#include <stdint.h>

#include "flow_gen.h"
#include "items_gen.h"
#include "actions_gen.h"
#include "config.h"

static void
fill_attributes(struct rte_flow_attr *attr,
	uint64_t *flow_attrs, uint16_t group)
{
	uint8_t i;
	for (i = 0; i < MAX_ATTRS_NUM; i++) {
		if (flow_attrs[i] == 0)
			break;
		if (flow_attrs[i] & INGRESS)
			attr->ingress = 1;
		else if (flow_attrs[i] & EGRESS)
			attr->egress = 1;
		else if (flow_attrs[i] & TRANSFER)
			attr->transfer = 1;
	}
	attr->group = group;
}

struct rte_flow *
generate_flow(uint16_t port_id,
	uint16_t group,
	uint64_t *flow_attrs,
	uint64_t *flow_items,
	uint64_t *flow_actions,
	uint16_t next_table,
	uint32_t outer_ip_src,
	uint16_t hairpinq,
	uint64_t encap_data,
	uint64_t decap_data,
	struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item items[MAX_ITEMS_NUM];
	struct rte_flow_action actions[MAX_ACTIONS_NUM];
	struct rte_flow *flow = NULL;

	memset(items, 0, sizeof(items));
	memset(actions, 0, sizeof(actions));
	memset(&attr, 0, sizeof(struct rte_flow_attr));

	fill_attributes(&attr, flow_attrs, group);

	fill_actions(actions, flow_actions,
		outer_ip_src, next_table, hairpinq,
		encap_data, decap_data);

	fill_items(items, flow_items, outer_ip_src);

	flow = rte_flow_create(port_id, &attr, items, actions, error);
	return flow;
}
