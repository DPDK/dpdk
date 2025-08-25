/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef SNIPPET_MATCH_ROCE_IB_BTH_H
#define SNIPPET_MATCH_ROCE_IB_BTH_H

/* Matching RoCE IB BTH opcode/dest_qp
 * IB BTH fields (opcode, and dst_qp) can be matched now using the new IB BTH item:
 * RTE_FLOW_ITEM_TYPE_IB_BTH.
 * Currently, this item is supported on group > 1, and supports only the RoCEv2 packet.
 * The input BTH match item is defaulted to match one RoCEv2 packet.
 */

#define MAX_PATTERN_NUM     5 /* Maximal number of patterns for this example. */
#define MAX_ACTION_NUM      2 /* Maximal number of actions for this example. */

static void
snippet_init_roce_ib_bth(void);
#define snippet_init snippet_init_roce_ib_bth

static void
snippet_match_roce_ib_bth_create_actions(uint16_t port_id, struct rte_flow_action *action);
#define snippet_skeleton_flow_create_actions snippet_match_roce_ib_bth_create_actions

static void
snippet_match_roce_ib_bth_create_patterns(struct rte_flow_item *pattern);
#define snippet_skeleton_flow_create_patterns snippet_match_roce_ib_bth_create_patterns

static struct rte_flow_template_table *
snippet_match_roce_ib_bth_create_table(__rte_unused uint16_t port_id,
__rte_unused struct rte_flow_error *error);
#define snippet_skeleton_flow_create_table snippet_match_roce_ib_bth_create_table

#endif /* SNIPPET_MATCH_ROCE_IB_BTH_H */
