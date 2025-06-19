/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_MBUF_DYNFIELD_H_
#define _RTE_GRAPH_MBUF_DYNFIELD_H_

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

/**
 * @file: rte_node_mbuf_dynfield.h
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Defines rte_node specific mbuf dynamic field region [rte_node_mbuf_dynfield_t]
 * which can be used by both DPDK built-in and out-of-tree nodes
 * for storing per-mbuf fields for graph walk.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RTE_NODE_MBUF_PERSISTENT_FIELDS_SIZE
/** Size of persistent mbuf fields */
#define RTE_NODE_MBUF_PERSISTENT_FIELDS_SIZE          (0)
#endif /* !RTE_NODE_MBUF_PERSISTENT_FIELDS_SIZE */

#ifndef RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE
/** Size of overloadable mbuf fields */
#define RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE        (8)
#endif /* !RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE */

/** Size of node mbuf dynamic field */
#define RTE_NODE_MBUF_DYNFIELD_SIZE     \
	(RTE_NODE_MBUF_PERSISTENT_FIELDS_SIZE + RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE)

/**
 * Node mbuf overloadable data.
 *
 * Out-of-tree nodes can repurpose overloadable fields via
 * rte_node_mbuf_overload_fields_get(mbuf). Overloadable fields are not
 * preserved and typically can be used with-in two adjacent nodes in the graph.
 */
typedef struct rte_node_mbuf_overload_fields {
	union {
		uint8_t data[RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE];
	};
} rte_node_mbuf_overload_fields_t;

/**
 * rte_node specific mbuf dynamic field structure [rte_node_mbuf_dynfield_t]
 *
 * It holds two types of fields:
 * 1. Persistent fields: Fields which are preserved across nodes during graph walk.
 *    - Eg: rx/tx interface etc
 * 2. Overloadable fields: Fields which can be repurposed by two adjacent nodes.
 */
typedef struct rte_node_mbuf_dynfield {
	/**
	 * Persistent mbuf region across nodes in graph walk
	 *
	 * These fields are preserved across graph walk and can be accessed by
	 * rte_node_mbuf_dynfield_get() in fast path.
	 */
	union {
		uint8_t persistent_data[RTE_NODE_MBUF_PERSISTENT_FIELDS_SIZE];
	};
	/**
	 * Overloadable mbuf fields across graph walk. Fields which can change.
	 *
	 * Two adjacent nodes (producer, consumer) can use this memory region for
	 * sharing per-mbuf specific information. Once mbuf leaves a consumer node,
	 * this region can be repurposed by another sets of [producer, consumer] node.
	 *
	 * In fast path, this region can be accessed by rte_node_mbuf_overload_fields_get().
	 */
	rte_node_mbuf_overload_fields_t overloadable_data;
} rte_node_mbuf_dynfield_t;

/**
 * For a given mbuf and dynamic offset, return pointer to rte_node_mbuf_dynfield_t *
 *
 * @param m
 *   Mbuf
 * @param offset
 *   Dynamic offset returned by @ref rte_node_mbuf_dynfield_register()
 * @return
 *  Pointer to node specific mbuf dynamic field structure
 */
__rte_experimental
static __rte_always_inline rte_node_mbuf_dynfield_t *
rte_node_mbuf_dynfield_get(struct rte_mbuf *m, const int offset)
{
	return RTE_MBUF_DYNFIELD(m, offset, struct rte_node_mbuf_dynfield *);
}

/**
 * For a given mbuf and dynamic offset, return pointer to overloadable fields.
 * Nodes can typecast returned pointer to reuse for their own purpose.
 *
 * @param m
 *   Mbuf
 * @param offset
 *   Dynamic offset returned by @ref rte_node_mbuf_dynfield_register()
 * @return
 * Pointer to node mbuf overloadable fields
 */
__rte_experimental
static __rte_always_inline rte_node_mbuf_overload_fields_t *
rte_node_mbuf_overload_fields_get(struct rte_mbuf *m, const int offset)
{
	rte_node_mbuf_dynfield_t *f = NULL;

	f = RTE_MBUF_DYNFIELD(m, offset, rte_node_mbuf_dynfield_t *);

	return &(f->overloadable_data);
}

/**
 *  Register rte_node specific common mbuf dynamic field region. Can be called
 *  in rte_node_register()->init() function to save offset in node->ctx
 *
 *  In process() function, node->ctx can be passed to
 *  - rte_node_mbuf_dynfield_get(mbuf, offset)
 *  - rte_node_mbuf_overload_fields_get(mbuf, offset)
 *
 *  Can be called multiple times by any number of nodes in init() function.
 *  - Very first call registers dynamic field and returns offset.
 *  - Subsequent calls return same offset.
 *
 *  @return
 *   <0 on error: rte_errno set to one of:
 *    - ENOMEM - no memory
 *   >=0 on success: dynamic field offset
 */
__rte_experimental
int rte_node_mbuf_dynfield_register(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_MBUF_DYNFIELD_H_ */
