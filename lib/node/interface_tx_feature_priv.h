/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#ifndef __INCLUDE_IF_TX_FEATURE_PRIV_H__
#define __INCLUDE_IF_TX_FEATURE_PRIV_H__

#include <rte_common.h>

extern struct rte_graph_feature_register if_tx_feature;

/**
 * @internal
 *
 * Get the ipv4 rewrite node.
 *
 * @return
 *   Pointer to the ipv4 rewrite node.
 */
struct rte_node_register *if_tx_feature_node_get(void);

/**
 * @internal
 *
 * Set the Edge index of a given port_id.
 *
 * @param port_id
 *   Ethernet port identifier.
 * @param next_index
 *   Edge index of the Given Tx node.
 */
int if_tx_feature_node_set_next(uint16_t port_id, uint16_t next_index);

#endif /* __INCLUDE_IF_TX_FEATURE_PRIV_H__ */
