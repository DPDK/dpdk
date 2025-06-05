/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_FEATURE_ARC_H_
#define _RTE_GRAPH_FEATURE_ARC_H_

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_debug.h>
#include <rte_graph.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rte_graph_feature_arc.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * Graph Feature Arc API
 *
 * Define APIs and structures/variables with respect to feature arc
 *
 * - Feature arc(s)
 * - Feature(s)
 *
 * In a typical network stack, often a protocol must be first enabled in
 * control plane before any packet is steered for its processing in the
 * dataplane. For eg: incoming IPv4 packets are routed only after a valid IPv4
 * address is assigned to the received interface. In other words, often packets
 * received on an interface need to be steered to protocol not based on the
 * packet content but based on whether the protocol is configured on the
 * interface or not.
 *
 * Protocols can be enabled/disabled multiple times at runtime in the control
 * plane. Protocols enabled on one interface may not be enabled on another
 * interface.
 *
 * When more than one protocols are present at a networking layer (say IPv4,
 * IP tables, IPsec etc), it becomes imperative to steer packets (in dataplane)
 * across each protocol processing in a defined sequential order. In ingress
 * direction, stack decides to perform IPsec decryption first before IP
 * validation while in egress direction IPsec encryption is performed after IP
 * forwarding. In the case of IP tables, users can enable rules in any
 * protocol order i.e. pre-routing or post-routing etc. This implies that
 * protocols are configured differently at each networking layer and in each
 * traffic direction.
 *
 * A feature arc represents an ordered list of features/protocols nodes at the
 * given networking layer and in a given direction. It provides a high level
 * abstraction to enable/disable features on an index at runtime and provide a
 * mechanism to steer packets across these feature nodes in a generic manner.
 * Here index corresponds to either interface index, route index, flow index or
 * classification index etc. as it is deemed suitable to configure protocols at
 * the networking layer. Some typical examples of protocols which are
 * configured based on
 *
 * - Interface Index (like IPv4 VRF, Port mirroring, Port based IPsec etc)
 * - Routes Index (like Route based IPsec etc)
 * - Flow index (like SDN)
 * - Classification Index (like ACL based protocol steering)
 *
 * Feature arc also provides a way to steer packets from in-built DPDK *feature
 * nodes* to out-of-tree *feature nodes* and vice-versa without any code
 * changes required in DPDK in-built node's fast path functions. This way it
 * allows application to override default packet path defined by in-built DPDK
 * nodes.
 *
 * Features enabled on one index may not be enabled on another index hence
 * packets received on an interface "X" should be treated independently from
 * packets received on interface "Y".
 *
 * A given feature might consume packet (if it's configured to consume) or may
 * forward it to next enabled feature. For instance, "IPsec input" feature may
 * consume/drop all packets with "Protect" policy action while all packets with
 * policy action as "Bypass" may be forwarded to next enabled feature (with in
 * same feature arc)
 *
 * A feature arc in a graph is represented via *start_node* and *end_node*.
 * Feature nodes are added between start_node and end_node. Packets enter
 * feature arc path via start_node while they exits from end_node.
 *
 * This library facilitates rte graph based applications to implement stack
 * functionalities described above by providing "edge" to the next enabled
 * feature node in fast path
 *
 */

/** feature notifier callback called when feature is enabled/disabled */
typedef void (*rte_graph_feature_change_notifier_cb_t)(const char *arc_name,
						       const char *feature_name,
						       rte_node_t feature_node_id,
						       uint32_t index,
						       bool enable_disable,
						       uint16_t app_cookie);

/** cb for overriding arc->max_indexes via RTE_GRAPH_FEATURE_REGISTER() */
typedef uint16_t (*rte_graph_feature_override_index_cb_t)(void);

/**
 *  Feature registration structure provided to
 *  RTE_GRAPH_FEATURE_REGISTER()
 */
struct rte_graph_feature_register {
	/**
	 * Pointer to next registered feature in the same arc.
	 */
	STAILQ_ENTRY(rte_graph_feature_register) next_feature;

	/** Name of the arc which is registered either via
	 * RTE_GRAPH_FEATURE_ARC_REGISTER() or via
	 * rte_graph_feature_arc_create()
	 */
	const char *arc_name;

	/** Name of the feature */
	const char *feature_name;

	/**
	 * Node id of feature_node.
	 *
	 * Setting this field can be skipped if registering feature via
	 * RTE_GRAPH_FEATURE_REGISTER()
	 */
	rte_node_t feature_node_id;

	/**
	 * Feature node process() function calling feature fast path APIs.
	 *
	 * If application calls rte_graph_feature_arc_init(), node->process()
	 * provided in RTE_NODE_REGISTER() is overwritten by this
	 * function.
	 */
	rte_node_process_t feature_process_fn;

	/**
	 * Pointer to Feature node registration
	 *
	 * Used when features are registered via
	 * RTE_GRAPH_FEATURE_REGISTER().
	 */
	struct rte_node_register *feature_node;

	/** Feature ordering constraints
	 * runs_after: Name of the feature which must run before "this feature"
	 * runs_before: Name of the feature which must run after "this feature"
	 */
	const char *runs_after;
	const char *runs_before;

	/**
	 * Allow each feature registration to override arc->max_indexes
	 *
	 * If set, struct rte_graph_feature_arc_register::max_indexes is
	 * calculated as follows (before calling rte_graph_feature_arc_create())
	 *
	 * max_indexes = rte_graph_feature_arc_register:max_indexes
	 * FOR_EACH_FEATURE_REGISTER(arc, feat) {
	 *   rte_graph_feature_arc_register:max_indexes = max(feat->override_index_cb(),
	 *                                                    max_indexes)
	 */
	rte_graph_feature_override_index_cb_t override_index_cb;

	/**
	 * Callback for notifying any change in feature enable/disable state
	 */
	rte_graph_feature_change_notifier_cb_t notifier_cb;
};

/** Feature arc registration structure */
struct rte_graph_feature_arc_register {
	STAILQ_ENTRY(rte_graph_feature_arc_register) next_arc;

	/** Name of the feature arc */
	const char *arc_name;

	/**
	 * Maximum number of features supported in this feature arc.
	 *
	 * This field can be skipped for feature arc registration via
	 * RTE_GRAPH_FEATURE_ARC_REGISTER().
	 *
	 * API internally sets this field by calculating number of
	 * RTE_GRAPH_FEATURE_REGISTER() for every arc registration via
	 * RTE_GRAPH_FEATURE_ARC_REGISTER()
	 */
	uint16_t max_features;

	/**
	 * Maximum number of indexes supported in this feature arc
	 * Memory is allocated based on this field
	 */
	uint16_t max_indexes;

	/** Start node of this arc */
	struct rte_node_register *start_node;

	/**
	 * Feature arc specific process() function for Start node.
	 * If application calls rte_graph_feature_arc_init(),
	 * start_node->process() is replaced by this function
	 */
	rte_node_process_t start_node_feature_process_fn;

	/** End feature node registration */
	struct rte_graph_feature_register *end_feature;
};

/** constructor to register feature to an arc */
#define RTE_GRAPH_FEATURE_REGISTER(reg)                                                 \
	RTE_INIT(__rte_graph_feature_register_##reg)                                    \
	{                                                                               \
		__rte_graph_feature_register(&reg, __func__, __LINE__);                 \
	}

/** constructor to register a feature arc */
#define RTE_GRAPH_FEATURE_ARC_REGISTER(reg)                                             \
	RTE_INIT(__rte_graph_feature_arc_register_##reg)                                \
	{                                                                               \
		__rte_graph_feature_arc_register(&reg, __func__, __LINE__);             \
	}

/**
 * @internal
 *
 * function declaration for registering arc
 *
 * @param reg
 *      Pointer to struct rte_graph_feature_arc_register
 *  @param caller_name
 *      Name of the function which is calling this API
 *  @param lineno
 *      Line number of the function which is calling this API
 */
__rte_experimental
void __rte_graph_feature_arc_register(struct rte_graph_feature_arc_register *reg,
				      const char *caller_name, int lineno);

/**
 * @internal
 *
 * function declaration for registering feature
 *
 * @param reg
 *      Pointer to struct rte_graph_feature_register
 * @param caller_name
 *      Name of the function which is calling this API
 * @param lineno
 *      Line number of the function which is calling this API
 */
__rte_experimental
void __rte_graph_feature_register(struct rte_graph_feature_register *reg,
				  const char *caller_name, int lineno);

#ifdef __cplusplus
}
#endif

#endif
