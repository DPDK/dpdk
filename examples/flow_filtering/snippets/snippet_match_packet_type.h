/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_PACKET_TYPE_H
#define SNIPPET_MATCH_PACKET_TYPE_H

/*
 * Match packet type
 * The new RTE flow item RTE_FLOW_ITEM_TYPE_PTYPE provides a quick way of finding
 * out L2/L3/L4 protocols in each packet.
 * This helps with optimized flow rules matching, eliminating the need of stacking
 * all the packet headers in the matching criteria.
 *
 * The supported values are:
 *  L2: ``RTE_PTYPE_L2_ETHER``, ``RTE_PTYPE_L2_ETHER_VLAN``, ``RTE_PTYPE_L2_ETHER_QINQ``
 *  L3: ``RTE_PTYPE_L3_IPV4``, ``RTE_PTYPE_L3_IPV6``
 *  L4: ``RTE_PTYPE_L4_TCP``, ``RTE_PTYPE_L4_UDP``, ``RTE_PTYPE_L4_ICMP``
 *  and their ``RTE_PTYPE_INNER_XXX`` counterparts as well as ``RTE_PTYPE_TUNNEL_ESP``.
 *  Matching on both outer and inner IP fragmented is supported using
 *  ``RTE_PTYPE_L4_FRAG`` and ``RTE_PTYPE_INNER_L4_FRAG`` values.
 *  They are not part of L4 types, so they should be provided as a mask value
 *  during pattern template creation explicitly.
 */

#define MAX_PATTERN_NUM     2 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

void
snippet_init_packet_type(void);
#define snippet_init snippet_init_packet_type

void
snippet_match_packet_type_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_packet_type_create_actions

void
snippet_match_packet_type_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_packet_type_create_patterns

struct rte_flow_template_table *
snippet_match_packet_type_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_packet_type_create_table

#endif /* SNIPPET_MATCH_PACKET_TYPE_H */
