/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */
#ifndef __INCLUDE_IP6_REWRITE_PRIV_H__
#define __INCLUDE_IP6_REWRITE_PRIV_H__

#include <rte_common.h>

#define RTE_GRAPH_IP6_REWRITE_MAX_NH 64
#define RTE_GRAPH_IP6_REWRITE_MAX_LEN 56

/**
 * @internal
 *
 * IPv6 rewrite next hop header data structure.
 * Used to store port specific rewrite data.
 */
struct ip6_rewrite_nh_header {
	uint16_t rewrite_len; /**< Header rewrite length. */
	uint16_t tx_node;     /**< Tx node next index identifier. */
	uint16_t enabled;     /**< NH enable flag */
	uint16_t rsvd;
	union {
		struct {
			struct rte_ether_addr dst;
			/**< Destination mac address. */
			struct rte_ether_addr src;
			/**< Source mac address. */
		};
		uint8_t rewrite_data[RTE_GRAPH_IP6_REWRITE_MAX_LEN];
		/**< Generic rewrite data */
	};
};

/**
 * @internal
 *
 * IPv6 node main data structure.
 */
struct ip6_rewrite_node_main {
	struct ip6_rewrite_nh_header nh[RTE_GRAPH_IP6_REWRITE_MAX_NH];
	/**< Array of next hop header data */
	uint16_t next_index[RTE_MAX_ETHPORTS];
	/**< Next index of each configured port. */
};

/**
 * @internal
 *
 * Get the IPv6 rewrite node.
 *
 * @return
 *   Pointer to the IPv6 rewrite node.
 */
struct rte_node_register *ip6_rewrite_node_get(void);

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
int ip6_rewrite_set_next(uint16_t port_id, uint16_t next_index);

#endif /* __INCLUDE_IP6_REWRITE_PRIV_H__ */
