/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */
#ifndef __INCLUDE_ETHDEV_TX_PRIV_H__
#define __INCLUDE_ETHDEV_TX_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ethdev_tx_node_ctx;
typedef struct ethdev_tx_node_ctx ethdev_tx_node_ctx_t;

enum ethdev_tx_next_nodes {
	ETHDEV_TX_NEXT_PKT_DROP,
	ETHDEV_TX_NEXT_MAX,
};

/**
 * @internal
 *
 * Ethernet Tx node context structure.
 */
struct ethdev_tx_node_ctx {
	uint16_t port;	/**< Port identifier of the Ethernet Tx node. */
	uint16_t queue; /**< Queue identifier of the Ethernet Tx node. */
};

/**
 * @internal
 *
 * Ethernet Tx node main structure.
 */
struct ethdev_tx_node_main {
	uint32_t nodes[RTE_MAX_ETHPORTS]; /**< Tx nodes for each ethdev port. */
};

/**
 * @internal
 *
 * Get the Ethernet Tx node data.
 *
 * @return
 *   Pointer to Ethernet Tx node data.
 */
struct ethdev_tx_node_main *ethdev_tx_node_data_get(void);

/**
 * @internal
 *
 * Get the Ethernet Tx node.
 *
 * @return
 *   Pointer to the Ethernet Tx node.
 */
struct rte_node_register *ethdev_tx_node_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_ETHDEV_TX_PRIV_H__ */
