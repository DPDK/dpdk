/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ETH_RING_H_
#define _RTE_ETH_RING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ring.h>

/**
 * Create a new ethdev port from a set of rings
 *
 * @param name
 *    name to be given to the new ethdev port
 * @param rx_queues
 *    pointer to array of rte_rings to be used as RX queues
 * @param nb_rx_queues
 *    number of elements in the rx_queues array
 * @param tx_queues
 *    pointer to array of rte_rings to be used as TX queues
 * @param nb_tx_queues
 *    number of elements in the tx_queues array
 * @param numa_node
 *    the numa node on which the memory for this port is to be allocated
 * @return
 *    the port number of the newly created the ethdev or -1 on error.
 */
int rte_eth_from_rings(const char *name,
		struct rte_ring * const rx_queues[],
		const unsigned nb_rx_queues,
		struct rte_ring *const tx_queues[],
		const unsigned nb_tx_queues,
		const unsigned numa_node);

/**
 * Create a new ethdev port from a ring
 *
 * This function is a shortcut call for rte_eth_from_rings for the
 * case where one wants to take a single rte_ring and use it as though
 * it were an ethdev
 *
 * @param ring
 *    the ring to be used as an ethdev
 * @return
 *    the port number of the newly created ethdev, or -1 on error
 */
int rte_eth_from_ring(struct rte_ring *r);

#ifdef __cplusplus
}
#endif

#endif
