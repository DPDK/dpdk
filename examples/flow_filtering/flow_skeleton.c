/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdint.h>

#include <rte_errno.h>
#include <rte_flow.h>

#include "common.h"
#include "snippets/snippet_match_ipv4.h"

struct rte_flow_attr flow_attr;
struct rte_flow_op_attr ops_attr = { .postpone = 0 };

static struct rte_flow *
create_flow_non_template(uint16_t port_id, struct rte_flow_attr *flow_attr,
						struct rte_flow_item *patterns,
						struct rte_flow_action *actions,
						struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;

	/* Validate the rule and create it. */
	if (rte_flow_validate(port_id, flow_attr, patterns, actions, error) == 0)
		flow = rte_flow_create(port_id, flow_attr, patterns, actions, error);
	return flow;
}

static struct rte_flow *
create_flow_template(uint16_t port_id, struct rte_flow_op_attr *ops_attr,
					struct rte_flow_item *patterns,
					struct rte_flow_action *actions,
					struct rte_flow_error *error)
{
	struct rte_flow_template_table *table = snippet_skeleton_flow_create_table(port_id, error);
	if (table == NULL) {
		printf("Failed to create table: %s (%s)\n",
		error->message, rte_strerror(rte_errno));
		return NULL;
	}

	return rte_flow_async_create(port_id,
		1, /* Flow queue used to insert the rule. */
		ops_attr,
		table,
		patterns,
		0, /* Pattern template index in the table. */
		actions,
		0, /* Actions template index in the table. */
		0, /* user data */
		error);
}

struct rte_flow *
generate_flow_skeleton(uint16_t port_id, struct rte_flow_error *error, int use_template_api)
{
	/* Set the common action and pattern structures 8< */
	struct rte_flow_action actions[MAX_ACTION_NUM] = {0};
	struct rte_flow_item patterns[MAX_PATTERN_NUM] = {0};

	snippet_skeleton_flow_create_actions(port_id, actions);

	snippet_skeleton_flow_create_patterns(patterns);
	/* >8 End of setting the common action and pattern structures. */

	/* Create a flow rule using template API 8< */
	if (use_template_api)
		return create_flow_template(port_id, &ops_attr, patterns, actions, error);
	/* >8 End of creating a flow rule using template API. */

	/* Validate and create the rule 8< */
	return create_flow_non_template(port_id, &flow_attr, patterns, actions, error);
	/* >8 End of validating and creating the rule. */
}
