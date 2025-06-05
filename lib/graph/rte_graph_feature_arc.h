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
 * A feature arc in a graph is represented via *start_node* and
 * *end_feature_node*.  Feature nodes are added between start_node and
 * end_feature_node. Packets enter feature arc path via start_node while they
 * exit from end_feature_node.
 *
 * This library facilitates rte graph based applications to implement stack
 * functionalities described above by providing "edge" to the next enabled
 * feature node in fast path
 *
 * In order to use feature-arc APIs, applications needs to do following in
 * control plane:
 * - Create feature arc object using RTE_GRAPH_FEATURE_ARC_REGISTER()
 * - New feature nodes (In-built/Out-of-tree) can be added to an arc via
 *   RTE_GRAPH_FEATURE_REGISTER(). RTE_GRAPH_FEATURE_REGISTER() has
 *   rte_graph_feature_register::runs_after and
 *   rte_graph_feature_register::runs_before to specify protocol
 *   ordering constraints.
 * - Before calling rte_graph_create(), rte_graph_feature_arc_init() API must
 *   be called. If rte_graph_feature_arc_init() is not called by application,
 *   feature arc library has no affect.
 * - Feature arc can be destroyed via rte_graph_feature_arc_destroy()
 *
 * If a given feature likes to control number of indexes (which is higher than
 * rte_graph_feature_arc_register::max_indexes) it can do so by setting
 * rte_graph_feature_register::override_index_cb(). As part of
 * rte_graph_feature_arc_init(), all
 * rte_graph_feature_register::override_index_cb(), if set, are called and with
 * maximum value returned by any of the feature is used for
 * rte_graph_feature_arc_create()
 *
 * Constraints
 * -----------
 *  - rte_graph_feature_arc_init(), rte_graph_feature_arc_create() and
 *  rte_graph_feature_add() must be called before rte_graph_create().
 *  - Not more than 63 features can be added to a feature arc. There is no
 *  limit to number of feature arcs i.e. number of
 *  RTE_GRAPH_FEATURE_ARC_REGISTER()
 *  - There is also no limit for number of indexes
 *  (rte_graph_feature_arc_register::max_indexes). There is also a provision
 *  for each RTE_GRAPH_FEATURE_REGISTER() to override number of indexes via
 *  rte_graph_feature_register::override_index_cb()
 *  - A feature node cannot be part of more than one arc due to
 *  performance reason.
 *
 * Optional Usage
 * --------------
 * Feature arc is added as an optional functionality to the graph library hence
 * an application may choose not to use it by skipping explicit call to
 * rte_graph_feature_arc_init(). In that case feature arc would be a no-op for
 * application.
 */

/** Length of feature arc name */
#define RTE_GRAPH_FEATURE_ARC_NAMELEN RTE_NODE_NAMESIZE

/** Initializer values for ARC, Feature, Feature data */
#define RTE_GRAPH_FEATURE_ARC_INITIALIZER ((rte_graph_feature_arc_t)UINT16_MAX)
#define RTE_GRAPH_FEATURE_DATA_INVALID ((rte_graph_feature_data_t)UINT32_MAX)
#define RTE_GRAPH_FEATURE_INVALID  ((rte_graph_feature_t)UINT8_MAX)

/** rte_graph feature arc object */
typedef uint16_t rte_graph_feature_arc_t;

/** rte_graph feature object */
typedef uint8_t rte_graph_feature_t;

/** rte_graph feature data object */
typedef uint32_t rte_graph_feature_data_t;

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
	 * runs_after: Name of the feature after which "this feature" should run
	 */
	const char *runs_after;

	/** Feature ordering constraints
	 * runs_before: Name of the feature which must run after "this feature"
	 */
	const char *runs_before;

	/**
	 * Allow each feature registration to override arc->max_indexes
	 *
	 * If set, struct rte_graph_feature_arc_register::max_indexes is
	 * calculated as follows (before calling rte_graph_feature_arc_create())
	 *
	 * FOR_EACH_FEATURE_REGISTER(arc, feat) {
	 *   rte_graph_feature_arc_register::max_indexes = max(feat->override_index_cb(),
	 *                                  rte_graph_feature_arc_register::max_indexes)
	 * }
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
 * Initialize feature arc subsystem
 *
 * This API
 * - Initializes feature arc module and alloc associated memory
 * - creates feature arc for every RTE_GRAPH_FEATURE_ARC_REGISTER()
 * - Add feature node to a feature arc for every RTE_GRAPH_FEATURE_REGISTER()
 * - Replaces all RTE_NODE_REGISTER()->process() functions for
 *   - Every start_node/end_feature_node provided in arc registration
 *   - Every feature node provided in feature registration
 *
 * @param num_feature_arcs
 *  Number of feature arcs that application wants to create by explicitly using
 *  "rte_graph_feature_arc_create()" API.
 *
 *  Number of RTE_GRAPH_FEATURE_ARC_REGISTER() should be excluded from this
 *  count as API internally calculates number of
 *  RTE_GRAPH_FEATURE_ARC_REGISTER().
 *
 *  So,
 *  total number of supported arcs = num_feature_arcs +
 *                                   NUMBER_OF(RTE_GRAPH_FEATURE_ARC_REGISTER())
 *
 *  @return
 *   0: Success
 *   <0: Failure
 *
 *  rte_graph_feature_arc_init(0) is valid call which will accommodates
 *  constructor based arc registration
 */
__rte_experimental
int rte_graph_feature_arc_init(uint16_t num_feature_arcs);

/**
 * Create a feature arc.
 *
 * This API can be skipped if RTE_GRAPH_FEATURE_ARC_REGISTER() is used
 *
 * @param reg
 *   Pointer to struct rte_graph_feature_arc_register
 * @param[out] _arc
 *  Feature arc object
 *
 * @return
 *  0: Success
 * <0: Failure
 */
__rte_experimental
int rte_graph_feature_arc_create(struct rte_graph_feature_arc_register *reg,
				 rte_graph_feature_arc_t *_arc);

/**
 * Get feature arc object with name
 *
 * @param arc_name
 *   Feature arc name provided to successful @ref rte_graph_feature_arc_create
 * @param[out] _arc
 *   Feature arc object returned. Valid only when API returns SUCCESS
 *
 * @return
 *  0: Success
 * <0: Failure.
 */
__rte_experimental
int rte_graph_feature_arc_lookup_by_name(const char *arc_name, rte_graph_feature_arc_t *_arc);

/**
 * Add a feature to already created feature arc.
 *
 * This API is not required in case RTE_GRAPH_FEATURE_REGISTER() is used
 *
 * @param feat_reg
 * Pointer to struct rte_graph_feature_register
 *
 * <I> Must be called before rte_graph_create() </I>
 * <I> When called by application, then feature_node_id should be appropriately set as
 *     freg->feature_node_id = freg->feature_node->id;
 * </I>
 *
 * @return
 *  0: Success
 * <0: Failure
 */
__rte_experimental
int rte_graph_feature_add(struct rte_graph_feature_register *feat_reg);

/**
 * Get rte_graph_feature_t object from feature name
 *
 * @param arc
 *   Feature arc object returned by @ref rte_graph_feature_arc_create or @ref
 *   rte_graph_feature_arc_lookup_by_name
 * @param feature_name
 *   Feature name provided to @ref rte_graph_feature_add
 * @param[out] feature
 *   Feature object
 *
 * @return
 *  0: Success
 * <0: Failure
 */
__rte_experimental
int rte_graph_feature_lookup(rte_graph_feature_arc_t arc, const char *feature_name,
			     rte_graph_feature_t *feature);

/**
 * Delete feature_arc object
 *
 * @param _arc
 *   Feature arc object returned by @ref rte_graph_feature_arc_create or @ref
 *   rte_graph_feature_arc_lookup_by_name
 *
 * @return
 *  0: Success
 * <0: Failure
 */
__rte_experimental
int rte_graph_feature_arc_destroy(rte_graph_feature_arc_t _arc);

/**
 * Cleanup all feature arcs
 *
 * @return
 *  0: Success
 * <0: Failure
 */
__rte_experimental
int rte_graph_feature_arc_cleanup(void);

/**
 * Slow path API to know how many features are added (NOT enabled) within a
 * feature arc
 *
 * @param _arc
 *  Feature arc object
 *
 * @return: Number of added features to arc
 */
__rte_experimental
uint32_t rte_graph_feature_arc_num_features(rte_graph_feature_arc_t _arc);

/**
 * Slow path API to get feature node name from rte_graph_feature_t object
 *
 * @param _arc
 *   Feature arc object
 * @param feature
 *   Feature object
 *
 * @return: Name of the feature node
 */
__rte_experimental
char *rte_graph_feature_arc_feature_to_name(rte_graph_feature_arc_t _arc,
					    rte_graph_feature_t feature);

/**
 * Slow path API to get corresponding rte_node_t from
 * rte_graph_feature_t
 *
 * @param _arc
 *   Feature arc object
 * @param feature
 *   Feature object
 * @param[out] node
 *   rte_node_t of feature node, Valid only when API returns SUCCESS
 *
 * @return: 0 on success, < 0 on failure
 */
__rte_experimental
int
rte_graph_feature_arc_feature_to_node(rte_graph_feature_arc_t _arc,
				      rte_graph_feature_t feature,
				      rte_node_t *node);

/**
 * Slow path API to dump valid feature arc names
 *
 *  @param[out] arc_names
 *   Buffer to copy the arc names. The NULL value is allowed in that case,
 * the function returns the size of the array that needs to be allocated.
 *
 * @return
 *   When next_nodes == NULL, it returns the size of the array else
 *  number of item copied.
 */
__rte_experimental
uint32_t
rte_graph_feature_arc_names_get(char *arc_names[]);

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
