/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_NSH_H
#define SNIPPET_MATCH_NSH_H

/* Network Service Header (NSH)
 * provides a mechanism for metadata exchange along the instantiated service paths.
 * The NSH is the Service Function Chaining (SFC) encapsulation required to support the
 * SFC architecture.
 * NSH, a data-plane protocol can be matched now using the existed item: RTE_FLOW_ITEM_TYPE_NSH.
 * Currently this is supported ONLY when NSH follows VXLAN-GPE,
 * and the "l3_vxlan_en=1" and "dv_flow_en=1" (Default) is set.
 */

#define MAX_PATTERN_NUM		7 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

static void
snippet_init_nsh(void);
#define snippet_init snippet_init_nsh

static void
snippet_match_nsh_create_actions(uint16_t port_id, struct rte_flow_action *actions);
#define snippet_skeleton_flow_create_actions snippet_match_nsh_create_actions

static void
snippet_match_nsh_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_nsh_create_patterns

static struct rte_flow_template_table *
snippet_nsh_flow_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_nsh_flow_create_table

#endif /* SNIPPET_MATCH_NSH_H */
