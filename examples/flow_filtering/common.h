/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef COMMON_H
#define COMMON_H

#define QUEUE_ID 1

extern struct rte_flow_attr attr;
extern struct rte_flow_op_attr ops_attr;

/**
 * Skeleton for creation of a flow rule using template and non template API.
 *
 * @param port_id
 *   The selected port.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[out] flow_template
 *   The selected API to use.
 * @return
 *   A flow if the rule could be created else return NULL.
 */
struct rte_flow *
generate_flow_skeleton(uint16_t port_id, struct rte_flow_error *error, int use_template_api);

#endif /* COMMON_H */
