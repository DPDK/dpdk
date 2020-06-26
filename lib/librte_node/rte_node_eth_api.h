/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __INCLUDE_RTE_NODE_ETH_API_H__
#define __INCLUDE_RTE_NODE_ETH_API_H__

/**
 * @file rte_node_eth_api.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * This API allows to setup ethdev_rx and ethdev_tx nodes
 * and its queue associations.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_mempool.h>

/**
 * Port config for ethdev_rx and ethdev_tx node.
 */
struct rte_node_ethdev_config {
	uint16_t port_id;
	/**< Port identifier */
	uint16_t num_rx_queues;
	/**< Number of Rx queues. */
	uint16_t num_tx_queues;
	/**< Number of Tx queues. */
	struct rte_mempool **mp;
	/**< Array of mempools associated to Rx queue. */
	uint16_t mp_count;
	/**< Size of mp array. */
};

/**
 * Initializes ethdev nodes.
 *
 * @param cfg
 *   Array of ethdev config that identifies which port's
 *   ethdev_rx and ethdev_tx nodes need to be created
 *   and queue association.
 * @param cnt
 *   Size of cfg array.
 * @param nb_graphs
 *   Number of graphs that will be used.
 *
 * @return
 *   0 on successful initialization, negative otherwise.
 */
__rte_experimental
int rte_node_eth_config(struct rte_node_ethdev_config *cfg,
			uint16_t cnt, uint16_t nb_graphs);
#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_NODE_ETH_API_H__ */
