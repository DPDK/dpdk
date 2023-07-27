/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */
#ifndef __INCLUDE_ETHDEV_RX_PRIV_H__
#define __INCLUDE_ETHDEV_RX_PRIV_H__

#include <rte_common.h>

struct ethdev_rx_node_elem;
struct ethdev_rx_node_ctx;
typedef struct ethdev_rx_node_elem ethdev_rx_node_elem_t;
typedef struct ethdev_rx_node_ctx ethdev_rx_node_ctx_t;

/**
 * @internal
 *
 * Ethernet device Rx node context structure.
 */
struct ethdev_rx_node_ctx {
	uint16_t port_id;  /**< Port identifier of the Rx node. */
	uint16_t queue_id; /**< Queue identifier of the Rx node. */
	uint16_t cls_next;
};

/**
 * @internal
 *
 * Ethernet device Rx node list element structure.
 */
struct ethdev_rx_node_elem {
	struct ethdev_rx_node_elem *next;
	/**< Pointer to the next Rx node element. */
	struct ethdev_rx_node_ctx ctx;
	/**< Rx node context. */
	rte_node_t nid;
	/**< Node identifier of the Rx node. */
};

enum ethdev_rx_next_nodes {
	ETHDEV_RX_NEXT_IP4_LOOKUP,
	ETHDEV_RX_NEXT_PKT_CLS,
	ETHDEV_RX_NEXT_IP4_REASSEMBLY,
	ETHDEV_RX_NEXT_MAX,
};

/**
 * @internal
 *
 * Ethernet Rx node main structure.
 */
struct ethdev_rx_node_main {
	ethdev_rx_node_elem_t *head;
	/**< Pointer to the head Rx node element. */
};

/**
 * @internal
 *
 * Get the Ethernet Rx node data.
 *
 * @return
 *   Pointer to Ethernet Rx node data.
 */
struct ethdev_rx_node_main *ethdev_rx_get_node_data_get(void);

/**
 * @internal
 *
 * Get the Ethernet Rx node.
 *
 * @return
 *   Pointer to the Ethernet Rx node.
 */
struct rte_node_register *ethdev_rx_node_get(void);

#endif /* __INCLUDE_ETHDEV_RX_PRIV_H__ */
