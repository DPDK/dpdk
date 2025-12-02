/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_INTEGRITY_FLAGS_H
#define SNIPPET_MATCH_INTEGRITY_FLAGS_H

/*
 * Steering by Integrity Check Flags
 * Now steering can be performed by a new RTE flow item RTE_FLOW_ITEM_TYPE_INTEGRITY
 * to enable matching on l3_ok, ipv4_csum_ok, l4_ok and l4_csum_ok.
 *
 * l3_ok (bit 2): If set, packet is matched to IPv4/IPv6, and the length of the data
 * is not larger than the packet's length. For IPv4, protocol version is also verified.
 * l4_ok (bit 3):
 *      In case of UDP - the packet has a valid length. For bad UDP, the length field
 *      is greater than the packet.
 *      In case of TCP - a valid data offset value. For TCP the data offset value
 *      is greater than the packet size.
 * ipv4_csum_ok (bit 5): The IPv4 checksum is correct.
 * l4_csum_ok (bit 6): The UDP/TCP checksum is correct.
 *
 * The item parameter `level' select between inner and outer parts:
 *      0,1 – outer
 *      Level > 2 - inner
 */

#define MAX_PATTERN_NUM     4 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

void
snippet_init_integrity_flags(void);
#define snippet_init snippet_init_integrity_flags

void
snippet_match_integrity_flags_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_integrity_flags_create_actions

void
snippet_match_integrity_flags_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_integrity_flags_create_patterns

struct rte_flow_template_table *
snippet_match_integrity_flags_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_integrity_flags_create_table

#endif /* SNIPPET_MATCH_INTEGRITY_FLAGS_H */
