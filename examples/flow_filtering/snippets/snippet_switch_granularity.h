/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

 #ifndef SNIPPET_SWITCH_GRANULARITY_H
 #define SNIPPET_SWITCH_GRANULARITY_H

/* Switch Granularity Rule Matching
 * supports the represented_port item in pattern.
 * If the spec and the mask are both set to NULL, the source vPort
 * will not be added to the matcher, it will match patterns for all
 * vPort to reduce rule count and memory consumption.
 * When testpmd starts with a PF, a VF-rep0 and a VF-rep1,
 * the snippets will redirect packets from VF0 and VF1 to the wire
 */

 #define MAX_PATTERN_NUM	3 /* Maximal number of patterns for this example. */
 #define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

static void
snippet_init_switch_granularity(void);
#define snippet_init snippet_init_switch_granularity

static void
snippet_match_switch_granularity_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_switch_granularity_create_actions

static void
snippet_match_switch_granularity_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_switch_granularity_create_patterns

static struct rte_flow_template_table *
create_table_switch_granularity(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table create_table_switch_granularity

 #endif /* SNIPPET_SWITCH_GRANULARITY_H */
