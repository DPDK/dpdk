/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef JUMP_FLOW_H
#define JUMP_FLOW_H

struct rte_flow *
create_jump_flow(uint16_t port_id, uint16_t group_id, struct rte_flow_error *error);

#endif /* JUMP_FLOW_H */
