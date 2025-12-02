/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_RANDOM_MATCH_H
#define SNIPPET_RANDOM_MATCH_H


/* Random Match
 * Add support for random matching 16 bits in template async API.
 * This value is not based on the packet data/headers.
 * Application shouldn't assume that this value is kept during the lifetime of the packet.
 */

#define MAX_PATTERN_NUM     3 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

static void
snippet_init_random_match(void);
#define snippet_init snippet_init_random_match

static void
snippet_match_random_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_random_create_actions

static void
snippet_match_random_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_random_create_patterns

static struct rte_flow_template_table *
snippet_match_random_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_random_create_table

#endif /* SNIPPET_RANDOM_MATCH_H */
