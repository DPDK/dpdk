/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_NVGRE_H
#define SNIPPET_MATCH_NVGRE_H

/* NVGRE Matching
 * PMD supports matching all NVGRE fields in HWS, including:
 * _k_s_rsvd0_ver
 * protocol
 * tni
 * flow_id
 *
 *  The following example shows how to build the arrays for pattern and rule
 *  to match the NVGRE packets with tni + flow_id + inner udp_src:
 *  ETH / IPV4 / NVGRE(TNI+FLOWID) / ETH / IPV4 / UDP(SRC).
 */

#define MAX_PATTERN_NUM		8 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM		2 /* Maximal number of actions for this example. */

void
snippet_init_nvgre(void);
#define snippet_init snippet_init_nvgre

void
snippet_match_nvgre_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_nvgre_create_actions

void
snippet_match_nvgre_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_nvgre_create_patterns

struct rte_flow_template_table *
snippet_match_nvgre_create_table(uint16_t port_id, struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_nvgre_create_table

#endif /* SNIPPET_MATCH_NVGRE_H */
