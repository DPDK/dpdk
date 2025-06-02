/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef __INCLUDE_RTE_NODE_PKT_CLS_API_H__
#define __INCLUDE_RTE_NODE_PKT_CLS_API_H__

/**
 * @file rte_node_pkt_cls_api.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * This API allows to do control path functions of pkt_cls node.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packet classification (pkt_cls) next nodes.
 */

enum rte_node_pkt_cls_next {
	RTE_NODE_PKT_CLS_NEXT_PKT_DROP,
	RTE_NODE_PKT_CLS_NEXT_IP4_LOOKUP,
	RTE_NODE_PKT_CLS_NEXT_IP6_LOOKUP,
	RTE_NODE_PKT_CLS_NEXT_IP4_LOOKUP_FIB,
	RTE_NODE_PKT_CLS_NEXT_MAX,
};

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_NODE_PKT_CLS_API_H__ */
