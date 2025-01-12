/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_IPV4_H
#define SNIPPET_MATCH_IPV4_H

/* Match IPV4 flow
 * sends packets with matching src and dest ip to selected queue.
 */

#define SRC_IP ((0<<24) + (0<<16) + (0<<8) + 0) /* src ip = 0.0.0.0 */
#define DEST_IP ((192<<24) + (168<<16) + (1<<8) + 1) /* dest ip = 192.168.1.1 */
#define FULL_MASK 0xffffffff /* full mask */
#define EMPTY_MASK 0x0 /* empty mask */

#define MAX_PATTERN_NUM		3 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

void
snippet_ipv4_flow_create_actions(struct rte_flow_action *action);

void
snippet_ipv4_flow_create_patterns(struct rte_flow_item *patterns);

struct rte_flow_template_table *
snippet_ipv4_flow_create_table(uint16_t port_id, struct rte_flow_error *error);


#endif /* SNIPPET_MATCH_IPV4_H */
