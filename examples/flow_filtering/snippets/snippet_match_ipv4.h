/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_IPV4_H
#define SNIPPET_MATCH_IPV4_H

/* Match IPV4 flow
 * sends packets with matching src and dest ip to selected queue.
 */

#define MAX_PATTERN_NUM		3 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

void
snippet_init_ipv4(void);
#define snippet_init snippet_init_ipv4

void
snippet_ipv4_flow_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_ipv4_flow_create_actions

void
snippet_ipv4_flow_create_patterns(struct rte_flow_item *patterns);
#define snippet_skeleton_flow_create_patterns snippet_ipv4_flow_create_patterns

struct rte_flow_template_table *
snippet_ipv4_flow_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_ipv4_flow_create_table


#endif /* SNIPPET_MATCH_IPV4_H */
