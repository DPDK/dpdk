/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __INCLUDE_RTE_NODE_IP4_API_H__
#define __INCLUDE_RTE_NODE_IP4_API_H__

/**
 * @file rte_node_ip4_api.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * This API allows to do control path functions of ip4_* nodes
 * like ip4_lookup, ip4_rewrite.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_compat.h>

#include <rte_graph.h>

/**
 * IP4 lookup next nodes.
 */
enum rte_node_ip4_lookup_next {
	RTE_NODE_IP4_LOOKUP_NEXT_REWRITE,
	/**< Rewrite node. */
	RTE_NODE_IP4_LOOKUP_NEXT_IP4_LOCAL,
	/** IP Local node. */
	RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP,
	/**< Number of next nodes of lookup node. */
};

/**
 * IP4 Local next nodes.
 */
enum rte_node_ip4_local_next {
	RTE_NODE_IP4_LOCAL_NEXT_UDP4_INPUT,
	/**< ip4 Local node. */
	RTE_NODE_IP4_LOCAL_NEXT_PKT_DROP,
	/**< Packet drop node. */
};

/**
 * IP4 reassembly next nodes.
 */
enum rte_node_ip4_reassembly_next {
	RTE_NODE_IP4_REASSEMBLY_NEXT_PKT_DROP,
       /**< Packet drop node. */
};

/**
 * Reassembly configure structure.
 * @see rte_node_ip4_reassembly_configure
 */
struct rte_node_ip4_reassembly_cfg {
	struct rte_ip_frag_tbl *tbl;
	/**< Reassembly fragmentation table. */
	struct rte_ip_frag_death_row *dr;
	/**< Reassembly deathrow table. */
	rte_node_t node_id;
	/**< Node identifier to configure. */
};

/**
 * Add ipv4 route to lookup table.
 *
 * @param ip
 *   IP address of route to be added.
 * @param depth
 *   Depth of the rule to be added.
 * @param next_hop
 *   Next hop id of the rule result to be added.
 * @param next_node
 *   Next node to redirect traffic to.
 *
 * @return
 *   0 on success, negative otherwise.
 */
__rte_experimental
int rte_node_ip4_route_add(uint32_t ip, uint8_t depth, uint16_t next_hop,
			   enum rte_node_ip4_lookup_next next_node);

/**
 * Add a next hop's rewrite data.
 *
 * @param next_hop
 *   Next hop id to add rewrite data to.
 * @param rewrite_data
 *   Rewrite data.
 * @param rewrite_len
 *   Length of rewrite data.
 * @param dst_port
 *   Destination port to redirect traffic to.
 *
 * @return
 *   0 on success, negative otherwise.
 */
__rte_experimental
int rte_node_ip4_rewrite_add(uint16_t next_hop, uint8_t *rewrite_data,
			     uint8_t rewrite_len, uint16_t dst_port);

/**
 * Add reassembly node configuration data.
 *
 * @param cfg
 *   Pointer to the configuration structure.
 * @param cnt
 *   Number of configuration structures passed.
 *
 * @return
 *   0 on success, negative otherwise.
 */
__rte_experimental
int rte_node_ip4_reassembly_configure(struct rte_node_ip4_reassembly_cfg *cfg, uint16_t cnt);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_NODE_IP4_API_H__ */
