/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_PORT_AFFINITY_H
#define SNIPPET_MATCH_PORT_AFFINITY_H

/* Port Affinity Match
 * indicates in the DPDK level the physical port a packet belongs to.
 * This capability is available by using a new pattern item type for aggregated port affinity,
 * its value reflects the physical port affinity of the received packets.
 * Additionally, the tx_affinity setting was added by calling rte_eth_dev_map_aggr_tx_affinity(),
 * its value reflects the physical port the packets will be sent to.
 * This new capability is enables the app to receive the ingress port of a packet,
 * and send the ACK out on the same port when dual ports devices are configured as a bond in Linux.
 * This feature is used in conjunction with link aggregation, also known as port bonding,
 * where multiple physical ports are combined into a single logical interface.
 */

#define MAX_PATTERN_NUM     3 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

void
snippet_init_match_port_affinity(void);
#define snippet_init snippet_init_match_port_affinity

void
snippet_match_port_affinity_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_port_affinity_create_actions

void
snippet_match_port_affinity_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_port_affinity_create_patterns

struct rte_flow_template_table *
snippet_match_port_affinity_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_port_affinity_create_table

#endif /* SNIPPET_MATCH_PORT_AFFINITY_H */
