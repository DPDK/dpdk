/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_MPLS_H
#define SNIPPET_MATCH_MPLS_H

/* Multiprotocol Label Switching snippet
 * In this example we match MPLS tunnel over UDP when in HW steering mode.
 * It supports multiple MPLS headers for matching as well as encapsulation/decapsulation.
 * The maximum supported MPLS headers is 5.
 */

#define MAX_PATTERN_NUM     6 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

static void
snippet_init_mpls(void);
#define snippet_init snippet_init_mpls

static void
snippet_mpls_create_actions(uint16_t port_id, struct rte_flow_action *actions);
#define snippet_skeleton_flow_create_actions snippet_mpls_create_actions

static void
snippet_mpls_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_mpls_create_patterns

static struct rte_flow_template_table *
snippet_mpls_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_mpls_create_table

#endif /* SNIPPET_MATCH_MPLS_H */
