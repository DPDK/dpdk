/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef COMMON_H
#define COMMON_H

extern bool enable_promiscuous_mode;
extern bool enable_flow_isolation;
extern struct rte_flow_attr flow_attr;

static inline void init_default_snippet(void)
{
	enable_promiscuous_mode = true;
	enable_flow_isolation = false;
	flow_attr.ingress = 1;
}

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
