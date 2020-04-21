/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef RTE_FLOW_DRIVER_H_
#define RTE_FLOW_DRIVER_H_

/**
 * @file
 * RTE generic flow API (driver side)
 *
 * This file provides implementation helpers for internal use by PMDs, they
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#include <stdint.h>

#include "rte_ethdev.h"
#include "rte_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic flow operations structure implemented and returned by PMDs.
 *
 * To implement this API, PMDs must handle the RTE_ETH_FILTER_GENERIC filter
 * type in their .filter_ctrl callback function (struct eth_dev_ops) as well
 * as the RTE_ETH_FILTER_GET filter operation.
 *
 * If successful, this operation must result in a pointer to a PMD-specific
 * struct rte_flow_ops written to the argument address as described below:
 *
 * \code
 *
 * // PMD filter_ctrl callback
 *
 * static const struct rte_flow_ops pmd_flow_ops = { ... };
 *
 * switch (filter_type) {
 * case RTE_ETH_FILTER_GENERIC:
 *     if (filter_op != RTE_ETH_FILTER_GET)
 *         return -EINVAL;
 *     *(const void **)arg = &pmd_flow_ops;
 *     return 0;
 * }
 *
 * \endcode
 *
 * See also rte_flow_ops_get().
 *
 * These callback functions are not supposed to be used by applications
 * directly, which must rely on the API defined in rte_flow.h.
 *
 * Public-facing wrapper functions perform a few consistency checks so that
 * unimplemented (i.e. NULL) callbacks simply return -ENOTSUP. These
 * callbacks otherwise only differ by their first argument (with port ID
 * already resolved to a pointer to struct rte_eth_dev).
 */
struct rte_flow_ops {
	/** See rte_flow_validate(). */
	int (*validate)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_create(). */
	struct rte_flow *(*create)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_destroy(). */
	int (*destroy)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 struct rte_flow_error *);
	/** See rte_flow_flush(). */
	int (*flush)
		(struct rte_eth_dev *,
		 struct rte_flow_error *);
	/** See rte_flow_query(). */
	int (*query)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 const struct rte_flow_action *,
		 void *,
		 struct rte_flow_error *);
	/** See rte_flow_isolate(). */
	int (*isolate)
		(struct rte_eth_dev *,
		 int,
		 struct rte_flow_error *);
	/** See rte_flow_dev_dump(). */
	int (*dev_dump)
		(struct rte_eth_dev *dev,
		 FILE *file,
		 struct rte_flow_error *error);
	/** See rte_flow_get_aged_flows() */
	int (*get_aged_flows)
		(struct rte_eth_dev *dev,
		 void **context,
		 uint32_t nb_contexts,
		 struct rte_flow_error *err);
};

/**
 * Get generic flow operations structure from a port.
 *
 * @param port_id
 *   Port identifier to query.
 * @param[out] error
 *   Pointer to flow error structure.
 *
 * @return
 *   The flow operations structure associated with port_id, NULL in case of
 *   error, in which case rte_errno is set and the error structure contains
 *   additional details.
 */
const struct rte_flow_ops *
rte_flow_ops_get(uint16_t port_id, struct rte_flow_error *error);

/** Helper macro to build input graph for rte_flow_expand_rss(). */
#define RTE_FLOW_EXPAND_RSS_NEXT(...) \
	(const int []){ \
		__VA_ARGS__, 0, \
	}

/** Node object of input graph for rte_flow_expand_rss(). */
struct rte_flow_expand_node {
	const int *const next;
	/**<
	 * List of next node indexes. Index 0 is interpreted as a terminator.
	 */
	const enum rte_flow_item_type type;
	/**< Pattern item type of current node. */
	uint64_t rss_types;
	/**<
	 * RSS types bit-field associated with this node
	 * (see ETH_RSS_* definitions).
	 */
};

/** Object returned by rte_flow_expand_rss(). */
struct rte_flow_expand_rss {
	uint32_t entries;
	/**< Number of entries @p patterns and @p priorities. */
	struct {
		struct rte_flow_item *pattern; /**< Expanded pattern array. */
		uint32_t priority; /**< Priority offset for each expansion. */
	} entry[];
};

/**
 * Expand RSS flows into several possible flows according to the RSS hash
 * fields requested and the driver capabilities.
 *
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * @param[out] buf
 *   Buffer to store the result expansion.
 * @param[in] size
 *   Buffer size in bytes. If 0, @p buf can be NULL.
 * @param[in] pattern
 *   User flow pattern.
 * @param[in] types
 *   RSS types to expand (see ETH_RSS_* definitions).
 * @param[in] graph
 *   Input graph to expand @p pattern according to @p types.
 * @param[in] graph_root_index
 *   Index of root node in @p graph, typically 0.
 *
 * @return
 *   A positive value representing the size of @p buf in bytes regardless of
 *   @p size on success, a negative errno value otherwise and rte_errno is
 *   set, the following errors are defined:
 *
 *   -E2BIG: graph-depth @p graph is too deep.
 */
__rte_experimental
int
rte_flow_expand_rss(struct rte_flow_expand_rss *buf, size_t size,
		    const struct rte_flow_item *pattern, uint64_t types,
		    const struct rte_flow_expand_node graph[],
		    int graph_root_index);

#ifdef __cplusplus
}
#endif

#endif /* RTE_FLOW_DRIVER_H_ */
