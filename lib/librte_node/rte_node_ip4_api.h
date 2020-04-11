/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __INCLUDE_RTE_NODE_IP4_API_H__
#define __INCLUDE_RTE_NODE_IP4_API_H__

/**
 * @file rte_node_ip4_api.h
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * This API allows to do control path functions of ip4_* nodes
 * like ip4_lookup, ip4_rewrite.
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

/**
 * IP4 lookup next nodes.
 */
enum rte_node_ip4_lookup_next {
	RTE_NODE_IP4_LOOKUP_NEXT_REWRITE,
	/**< Rewrite node. */
	RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP,
	/**< Packet drop node. */
	RTE_NODE_IP4_LOOKUP_NEXT_MAX,
	/**< Number of next nodes of lookup node. */
};

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_NODE_IP4_API_H__ */
