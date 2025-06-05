/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __NODE_PRIVATE_H__
#define __NODE_PRIVATE_H__

#include <stdalign.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#include <rte_graph_worker_common.h>
#include <rte_graph_feature_arc_worker.h>

extern int rte_node_logtype;
#define RTE_LOGTYPE_NODE rte_node_logtype

#define NODE_LOG(level, node_name, ...)                                        \
	RTE_LOG_LINE_PREFIX(level, NODE, "%s: %s():%u ",                       \
		node_name RTE_LOG_COMMA __func__ RTE_LOG_COMMA __LINE__,       \
		__VA_ARGS__)

#define node_err(node_name, ...) NODE_LOG(ERR, node_name, __VA_ARGS__)
#define node_info(node_name, ...) NODE_LOG(INFO, node_name, __VA_ARGS__)
#define node_dbg(node_name, ...) NODE_LOG(DEBUG, node_name, __VA_ARGS__)

/**
 * Node mbuf private data to store next hop, ttl and checksum.
 */
struct node_mbuf_priv1 {
	union {
		/* IP4/IP6 rewrite */
		struct {
			uint16_t nh;
			uint16_t ttl;
			uint32_t cksum;
		};

		uint64_t u;
	};
};

static const struct rte_mbuf_dynfield node_mbuf_priv1_dynfield_desc = {
	.name = "rte_node_dynfield_priv1",
	.size = sizeof(struct node_mbuf_priv1),
	.align = alignof(struct node_mbuf_priv1),
};
extern int node_mbuf_priv1_dynfield_offset;

/**
 * Node mbuf private area 2.
 */
struct __rte_cache_aligned node_mbuf_priv2 {
	uint64_t priv_data;
};

#define NODE_MBUF_PRIV2_SIZE sizeof(struct node_mbuf_priv2)

#define OBJS_PER_CLINE (RTE_CACHE_LINE_SIZE / sizeof(void *))

/**
 * Get mbuf_priv1 pointer from rte_mbuf.
 *
 * @param
 *   Pointer to the rte_mbuf.
 *
 * @return
 *   Pointer to the mbuf_priv1.
 */
static __rte_always_inline struct node_mbuf_priv1 *
node_mbuf_priv1(struct rte_mbuf *m, const int offset)
{
	return RTE_MBUF_DYNFIELD(m, offset, struct node_mbuf_priv1 *);
}

/**
 * Get mbuf_priv2 pointer from rte_mbuf.
 *
 * @param
 *   Pointer to the rte_mbuf.
 *
 * @return
 *   Pointer to the mbuf_priv2.
 */
static __rte_always_inline struct node_mbuf_priv2 *
node_mbuf_priv2(struct rte_mbuf *m)
{
	return (struct node_mbuf_priv2 *)rte_mbuf_to_priv(m);
}

#define NODE_INCREMENT_XSTAT_ID(node, id, cond, cnt) \
do { \
	if (unlikely(rte_graph_has_stats_feature() && (cond))) \
		((uint64_t *)RTE_PTR_ADD(node, node->xstat_off))[id] += (cnt); \
} while (0)

#endif /* __NODE_PRIVATE_H__ */
