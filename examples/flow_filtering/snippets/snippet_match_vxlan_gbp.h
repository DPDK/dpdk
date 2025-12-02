/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_VXLAN_GBP_H
#define SNIPPET_MATCH_VXLAN_GBP_H

/* VXLAN-GBP Header Fields Matching
 * Added support of matching VXLAN-GBP header fields in the PMD,
 * such as VNI and flags (first 8 bits), group policy ID and reserved field 0.
 */

#define MAX_PATTERN_NUM		5 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

void
snippet_init_vxlan_gbp(void);
#define snippet_init snippet_init_vxlan_gbp

void
snippet_match_vxlan_gbp_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_vxlan_gbp_create_actions

void
snippet_match_vxlan_gbp_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_vxlan_gbp_create_patterns

struct rte_flow_template_table *
snippet_match_vxlan_gbp_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_vxlan_gbp_create_table

#endif /* SNIPPET_MATCH_VXLAN_GBP_H */
