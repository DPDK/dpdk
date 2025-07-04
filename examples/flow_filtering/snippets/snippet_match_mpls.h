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

#define MAX_PATTERN_NUM		6 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

/* Replace this function with the snippet_*_create_actions() function in flow_skeleton.c. */
static void
snippet_mpls_create_actions(struct rte_flow_action *actions);

/* Replace this function with the snippet_*_create_patterns() function in flow_skeleton.c. */
static void
snippet_mpls_create_patterns(struct rte_flow_item *pattern);

/* Replace this function with the snippet_*_create_table() function in flow_skeleton.c. */
static struct rte_flow_template_table *
snippet_mpls_create_table(uint16_t port_id, struct rte_flow_error *error);

#endif /* SNIPPET_MATCH_MPLS_H */
