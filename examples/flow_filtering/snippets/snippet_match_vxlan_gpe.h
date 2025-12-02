/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_VXLAN_GPE_H
#define SNIPPET_MATCH_VXLAN_GPE_H

#include <rte_flow.h>

/* VXLAN-GPE Header Fields Matching */

#define MAX_PATTERN_NUM		5 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

void
snippet_init_vxlan_gpe(void);
#define snippet_init snippet_init_vxlan_gpe

void
snippet_match_vxlan_gpe_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_vxlan_gpe_create_actions

void
snippet_match_vxlan_gpe_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_vxlan_gpe_create_patterns

struct rte_flow_template_table *
snippet_match_vxlan_gpe_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_vxlan_gpe_create_table

#endif /* SNIPPET_MATCH_VXLAN_GPE_H */
