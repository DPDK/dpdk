/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MODIFY_ECN_H
#define SNIPPET_MODIFY_ECN_H

/* ECN in IP Header Modification
 * ECN field in the IPv4/IPv6 header can now be modified using the modify field action
 * which now can be modified in the meter policy.
 */

#define MAX_PATTERN_NUM     3 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      3 /* Maximal number of actions for this example. */

void
snippet_init_modify_ecn(void);
#define snippet_init snippet_init_modify_ecn

void
snippet_match_modify_ecn_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_modify_ecn_create_actions

void
snippet_match_modify_ecn_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_modify_ecn_create_patterns

struct rte_flow_template_table *
snippet_match_modify_ecn_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_modify_ecn_create_table

#endif /* SNIPPET_MODIFY_ECN_H */
