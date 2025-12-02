/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_NAT64_H
#define SNIPPET_NAT64_H

/* NAT64 Action Usage
 * New steering entries will be created per flow rule, even if the action can be shared.
 * It is recommended to use shared rules for NAT64 actions in some dedicated flow tables
 * to reduce the duplication of entries.
 * The default address and other fields conversion will be handled within the NAT64 action.
 * To support another address, new rule(s) with modify fields on the IP address fields,
 * should be created after the NAT64.
 *
 * The following fields should be handled automatically:
 * Version
 * Traffic Class / TOS
 * Flow Label (0 in v4)
 * Payload Length / Total length
 * Next Header / Protocol
 * Hop Limit / TTL
 */

#define MAX_PATTERN_NUM		2 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		3 /* Maximal number of actions for this example. */

void
snippet_init_nat64(void);
#define snippet_init snippet_init_nat64

void
snippet_match_nat64_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_nat64_create_actions

void
snippet_match_nat64_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_nat64_create_patterns

struct rte_flow_template_table *
snippet_match_nat64_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_nat64_create_table

#endif /* SNIPPET_NAT64_H */
