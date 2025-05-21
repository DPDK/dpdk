/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_GRE_H
#define SNIPPET_MATCH_GRE_H

/* Matching GRE Checksum/Key/Sequence
 * GRE optional fields (checksum, key and sequence) can be matched using the gre_option item.
 * The item requires a GRE item such as gre_key, and its pattern must correspond with the
 * c_bit/k_bit/s_bit in the GRE pattern.
 */

#define MAX_PATTERN_NUM     5 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

static void
snippet_init_gre(void);
#define snippet_init snippet_init_gre

static void
snippet_match_gre_create_actions(struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_gre_create_actions

static void
snippet_match_gre_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_gre_create_patterns

static struct rte_flow_template_table *
snippet_gre_flow_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_gre_flow_create_table


#endif /* SNIPPET_MATCH_GRE_H */
