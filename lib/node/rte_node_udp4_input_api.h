/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef __INCLUDE_RTE_NODE_UDP4_INPUT_API_H__
#define __INCLUDE_RTE_NODE_UDP4_INPUT_API_H__

/**
 * @file rte_node_udp4_input_api.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * This API allows to control path functions of udp4_* nodes
 * like udp4_input.
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_compat.h>

#include "rte_graph.h"
/**
 * UDP4 lookup next nodes.
 */
enum rte_node_udp4_input_next {
	RTE_NODE_UDP4_INPUT_NEXT_PKT_DROP,
	/**< Packet drop node. */
};

/**
 * Add usr node to receive udp4 frames.
 *
 * @param usr_node
 * Node registered by user to receive data.
 */
__rte_experimental
int rte_node_udp4_usr_node_add(const char *usr_node);

/**
 * Add udpv4 dst_port to lookup table.
 *
 * @param dst_port
 *   Dst Port of packet to be added for consumption.
 * @param next_node
 *   Next node packet to be added for consumption.
 * @return
 *   0 on success, negative otherwise.
 */
__rte_experimental
int rte_node_udp4_dst_port_add(uint32_t dst_port, rte_edge_t next_node);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_NODE_UDP4_API_H__ */
