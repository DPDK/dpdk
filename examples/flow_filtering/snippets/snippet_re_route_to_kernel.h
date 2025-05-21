/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_REROUTE_TO_KERNEL_H
#define SNIPPET_REROUTE_TO_KERNEL_H

/* Packet Re-Routing to the Kernel
 * This feature introduces a new rte_flow action (RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL)
 * which allows application to re-route packets directly to the kernel without software involvement.
 * The packets will be received by the kernel sharing the same device as the DPDK port
 * on which this action is configured.
 */

#define MAX_PATTERN_NUM    2 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM     2 /* Maximal number of actions for this example. */

static void
snippet_init_re_route_to_kernel(void);
#define snippet_init snippet_init_re_route_to_kernel

static void
snippet_re_route_to_kernel_create_actions(struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_re_route_to_kernel_create_actions

static void
snippet_re_route_to_kernel_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_re_route_to_kernel_create_patterns

static struct rte_flow_template_table *
snippet_re_route_to_kernel_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_re_route_to_kernel_create_table

#endif /* SNIPPET_REROUTE_TO_KERNEL_H */
