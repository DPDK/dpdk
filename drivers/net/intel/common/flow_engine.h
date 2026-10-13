/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _COMMON_INTEL_FLOW_ENGINE_H_
#define _COMMON_INTEL_FLOW_ENGINE_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <rte_bitops.h>
#include <rte_errno.h>
#include <rte_malloc.h>

#include <ethdev_driver.h>
#include <rte_flow.h>
#include <flow_graph.h>
#include <rte_tailq.h>
#include <rte_rwlock.h>
#include <rte_hexdump.h>

#include "log.h"

/*
 * This is a common header for Intel Ethernet drivers' flow engine
 * implementations. It defines the interfaces and data structures required to
 * implement flow rule engines that plug into the drivers' flow handling logic.
 *
 * Design considerations:
 *
 * 1. Ease of implementation
 *
 * The flow engine interface is kept simple, with obvious defaults (not
 * specifying something leads to the most expected behavior in context). The
 * point is not to produce a monstrous driver-within-a-driver framework, but to
 * make engine definitions follow semantic expectations of what the engine does.
 *
 * All boilerplate (flow management, engine enablement tracking, etc.) is
 * handled by the common flow infrastructure, so an engine only needs to parse
 * and install/uninstall flow rules. Drivers are expected to use other common
 * flow utilities where applicable (e.g. flow_util.h, flow_check.h).
 *
 * 2. Full secondary process compatibility
 *
 * To support rte_flow operations in secondary processes, the engines enabled
 * for a driver instance are tracked and resolved at runtime. The engine index
 * (its position in the engine list) is used as a bit position in a
 * driver-specific 64-bit field of enabled engines, so engine definitions can
 * live in read-only memory shared by primary and secondary processes. For this
 * to be safe, engine lists and definitions must be immutable for process
 * lifetime (declare them as const).
 *
 * This does not mean every driver supports rte_flow in secondary processes -
 * that is still up to each driver. It only ensures the framework itself does not
 * prevent it.
 *
 * Engine callbacks must not access or retain a `struct rte_eth_dev *`, as it is
 * process-local; use the process-independent `struct rte_eth_dev_data *`
 * provided by the framework instead.
 *
 * The per-instance engine configuration is set up and torn down exclusively by
 * `ci_flow_engine_conf_init()` and `ci_flow_engine_conf_reset()`, which should
 * only be called by the primary process at device setup/teardown.
 *
 * 3. Flow object lifecycle is framework-owned
 *
 * Engines should treat framework-provided context and flow objects as storage
 * they fill in, not storage they own: engine logic focuses on the contents of
 * flow data, while object lifetime is managed by the framework. Engines may
 * still allocate auxiliary data, but only where the framework guarantees a
 * matching teardown call that lets the engine release it.
 *
 * 4. Pattern parsing: flow_graph and pattern_parse callback
 *
 * The flow engine framework works hand-in-hand with the `flow_graph` parsing
 * infrastructure. Each engine may provide a pattern graph that is used to match
 * the flow pattern and extract relevant data into the engine context provided
 * by the framework.
 *
 * Engines may also provide a `pattern_parse` callback that is invoked before
 * the graph parser runs. This allows engines to handle pattern items that don't
 * fit neatly into the graph model (e.g. FUZZY items that can appear at any
 * position), or to bypass the graph parser entirely with custom parsing.
 *
 * Pattern contents cannot be ignored. This is by design, as ignoring them is
 * considered rte_flow API misuse. By default, even in the empty fallback case,
 * a meaningful pattern (one that is not empty or ANY) is treated as an error.
 *
 * 5. Setup, teardown, and flow list lifecycle ordering
 *
 * The expected sequence of calls for a driver instance is:
 *
 * - `ci_flow_engine_conf_init()` should run from the driver's `dev_init` path.
 *
 * - At `dev_close`, `ci_flow_engine_conf_cleanup()` should run first to drop
 *   all flows and their internal tracking, followed by
 *   `ci_flow_engine_conf_reset()`.
 *
 * - Devices may or may not advertise `RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP`, i.e.
 *   support for keeping flow rules across a `dev_stop`/`dev_start` cycle.
 *
 * - If the device does not advertise this capability, flows must be flushed
 *   via `ci_flow_flush()` as the first step of `dev_stop()` (flushing later
 *   may interfere with flow uninstall).
 *
 * - If the flows are kept but  device loses its hardware state across a
 *   `dev_stop`/`dev_start` cycle, the driver should call
 *   `ci_flow_engine_conf_replay()` as the last step of `dev_start`.
 */

struct ci_flow_engine_ops;
struct ci_flow_engine_ctx;
struct ci_flow_engine;
struct ci_flow_engine_ref;
struct ci_flow_engine_list;
struct ci_flow_engine_conf;
struct ci_flow;

/*
 * Flow engine ops.
 *
 * Each flow engine provides a set of operations that make up the flow engine
 * pipeline. Each stage has a setup step (run when a flow is created) and,
 * where applicable, a teardown step (run when a flow is removed). Operations
 * are compositions of stages: setup steps run in forward order, teardown steps
 * in reverse. Optional callbacks are shown in [brackets].
 *
 * - Allocation stage
 *     setup:    [flow_alloc]      teardown: [flow_free]
 *   Engine-specific flow object allocation. Skipped on the validate path.
 *
 * - Parse stage
 *     setup:    ctx_init -> [pattern_parse] -> [graph parser] ->
 *               [ctx_finalize] -> [ctx_to_flow]
 *     teardown: (none)
 *   Interprets the request into the flow structure. Must NOT allocate.
 *
 * - Track stage
 *     setup:    [flow_register]   teardown: [flow_unregister]
 *   Updates driver-internal state. Permitted to allocate.
 *
 * - Apply stage
 *     setup:    [flow_install]    teardown: [flow_uninstall]
 *   Programs the hardware. Must NOT allocate.
 *
 * Typical operation sequences:
 *
 * - rte_flow_create:   Allocation -> Parse -> Track -> Apply   (setup)
 * - rte_flow_validate: Parse
 * - rte_flow_destroy:  Apply -> Track -> Allocation            (teardown)
 * - rte_flow_flush:    Apply -> Track -> Allocation            (teardown, per flow)
 * - ci_flow_engine_conf_replay:   Apply                        (setup, per flow)
 * - ci_flow_engine_conf_cleanup:  Track -> Allocation          (teardown, per flow)
 *
 * 1) Engine availability and lifecycle
 *
 * Availability is checked by the driver at init time. The criteria are up to
 * the driver (hardware capability bits, PHY type, devargs, etc.). If
 * `engine_init` is not implemented, the engine is always available.
 *
 * If `priv_size` is non-zero, the framework allocates a zeroed private block
 * and passes it to engine_init/engine_uninit. Each allocated flow also carries
 * a pointer to this per-device data in `flow->engine_priv`.
 *
 * 2) Input parsing
 *
 * `ctx_init` is mandatory and is the gateway for processing actions and
 * attributes into engine context.
 *
 * Patterns are matched against the pattern graph in `ci_flow_engine`, the
 * `pattern_parse` callback, or both:
 *
 * - graph only: graph parser runs
 * - callback only: pattern_parse runs
 * - callback + graph: pattern_parse runs, then graph parser runs
 * - neither: empty fallback matcher is used
 *
 * The empty fallback matcher accepts only a NULL pattern, an empty pattern
 * (start -> end), or an ANY-only pattern (start -> any -> end).
 *
 * In callback + graph mode, the callback does not consume items: the graph
 * parser sees the same pattern, so the graph should list the relevant
 * `ignore_nodes`.
 *
 * 3) Rule finalization
 *
 * `ctx_finalize` is an optional final consistency check between parsed
 * action/attribute data and pattern-derived data, and a place for any other
 * post-parse work.
 *
 * 4) Rule materialization
 *
 * `ctx_to_flow` translates the parsed context into the engine-specific flow
 * rule. The rule should contain only what is needed for installation or
 * querying; temporary data belongs in the build context.
 *
 * 5) Rule lifecycle hooks
 *
 * `flow_alloc`/`flow_free` are optional. If `flow_alloc` is absent or returns
 * NULL, the framework falls back to rte_zmalloc. `flow_free` is required if
 * `flow_alloc` is provided.
 *
 * `flow_register`/`flow_unregister` (Track) and `flow_install`/`flow_uninstall`
 * (Apply) are optional. Register/unregister must be both defined or both NULL.
 *
 * Engines own only engine-specific fields. Common `ci_flow` fields (engine_idx,
 * engine_priv, fallback_alloc, etc.) belong to the framework and must not be
 * modified.
 *
 * IMPORTANT: by the time Track and Apply run, the engine has already accepted
 * the flow. A failure in `flow_register` or `flow_install` (e.g. resource
 * exhaustion) is a hard failure: no other engines are tried. Any check
 * intrinsic to the pattern that would prevent tracking or installation must
 * therefore be done at the parse stage.
 *
 * 6) Memory ownership and allocation policy
 *
 * An engine may allocate in exactly three places:
 *
 * - engine init: engine-wide resources used by other callbacks (e.g. a mempool
 *   from which `flow_alloc` takes flow objects)
 * - flow alloc: memory for the flow itself (e.g. a mempool object), plus
 *   allocator-associated bookkeeping. It must not produce anything the parse
 *   stage depends on, because `rte_flow_validate()` bypasses `flow_alloc`.
 * - flow register: the only per-flow callback that may allocate memory and
 *   store the pointer in the flow object or driver-internal structures. This is
 *   where "parsing the flow" ends and "updating driver state" begins.
 *
 * Parse callbacks (`ctx_init`, `pattern_parse`, `ctx_finalize`, `ctx_to_flow`)
 * and `flow_install` must not allocate. These steps may fail with no
 * teardown.
 *
 * 7) Flow query
 *
 * `flow_query` is optional. If not provided, querying the rule fails.
 *
 * 8) Concurrency model
 *
 * The framework serializes access to each driver instance's flow state with a
 * single rwlock. Create, destroy and flush take it exclusively; validate and
 * query take it shared. Parse callbacks (ctx_init, pattern_parse,
 * ctx_finalize, ctx_to_flow) can therefore run concurrently on the validate
 * path, as can flow_query, and must treat shared engine state (including
 * engine_priv) as read-only. State mutation belongs in the write-locked
 * callbacks (engine_init/engine_uninit, flow_alloc/flow_free,
 * flow_register/flow_unregister, flow_install/flow_uninstall).
 */
struct ci_flow_engine_ops {
	/* engine init callback - can be NULL */
	int (*engine_init)(const struct ci_flow_engine *engine,
			struct rte_eth_dev_data *dev_data,
			void *priv);
	/* engine uninit callback - can be NULL */
	void (*engine_uninit)(const struct ci_flow_engine *engine,
			void *priv);
	/* allocation callback for flow rules - can be NULL */
	struct ci_flow *(*flow_alloc)(const struct ci_flow_engine *engine,
			struct rte_eth_dev_data *dev_data,
			void *priv);
	/* deallocation callback for flow rules - can be NULL */
	void (*flow_free)(struct ci_flow *flow,
			struct rte_eth_dev_data *dev_data,
			void *priv);
	/* initialize engine context from flow attr/actions - mandatory */
	int (*ctx_init)(const struct rte_flow_action actions[],
			const struct rte_flow_attr *attr,
			struct ci_flow_engine_ctx *ctx,
			struct rte_flow_error *error);
	/* pattern parsing callback - can be NULL */
	int (*pattern_parse)(struct ci_flow_engine_ctx *ctx,
			const struct rte_flow_item pattern[],
			struct rte_flow_error *error);
	/* final pass before converting context to flow - can be NULL */
	int (*ctx_finalize)(struct ci_flow_engine_ctx *ctx,
			struct rte_flow_error *error);
	/* initialize flow rule from parsed context - can be NULL */
	int (*ctx_to_flow)(const struct ci_flow_engine_ctx *ctx,
			struct ci_flow *flow,
			struct rte_flow_error *error);
	/* track a flow rule in driver-internal state - can be NULL */
	int (*flow_register)(struct ci_flow *flow,
			struct rte_flow_error *error);
	/* untrack a flow rule from driver-internal state - can be NULL */
	int (*flow_unregister)(struct ci_flow *flow,
			struct rte_flow_error *error);
	/* install a flow rule to hardware - can be NULL */
	int (*flow_install)(struct ci_flow *flow,
			struct rte_flow_error *error);
	/* uninstall a flow rule from hardware - can be NULL */
	int (*flow_uninstall)(struct ci_flow *flow,
			struct rte_flow_error *error);
	/* query flow - can be NULL */
	int (*flow_query)(struct ci_flow *flow,
			const struct rte_flow_action *action,
			void *data,
			struct rte_flow_error *error);
};

/* base parse context; engine contexts must start with this struct */
struct ci_flow_engine_ctx {
	/* ethernet device this context belongs to */
	struct rte_eth_dev_data *dev_data;
	/* per-engine private data, same pointer as flow->engine_priv */
	void *engine_priv;
	/* original flow attributes, as passed by the caller */
	const struct rte_flow_attr *attr;
	/* original flow pattern, as passed by the caller */
	const struct rte_flow_item *pattern;
	/* original flow actions, as passed by the caller */
	const struct rte_flow_action *actions;
};

/*
 * Common definition for flow rules.
 *
 * A flow rule has three layers of data:
 *
 * 1) Common data (`ci_flow`, defined here)
 * 2) Driver-specific data
 * 3) Engine-specific data
 *
 * `ci_flow` holds the fields common to all drivers and engines. Some are used
 * internally by the framework (e.g. list linkage), others are provided as a
 * basic service for engines' convenience (e.g. a reference to the owning
 * device).
 *
 * Each driver defines its own `rte_flow` structure, which must start with
 * `ci_flow`, followed by driver-specific fields. Each engine may in turn define
 * its own flow structure, which must start with the driver's `rte_flow`,
 * followed by engine-specific fields.
 *
 * IMPORTANT:
 *
 * All of these structures are referred to by the same pointer and can be safely
 * cast between each other *as long as* each one has its parent structure as its
 * first member:
 *
 *     struct rte_flow {
 *         struct ci_flow base;
 *         ...driver-specific fields...
 *     };
 *
 *     struct ixgbe_fdir_flow {
 *         struct rte_flow base;
 *         ...engine-specific fields...
 *     };
 *
 *     struct ci_flow *flow = ...;
 *     struct rte_flow *rte_flow = (struct rte_flow *)flow;
 *     struct ixgbe_fdir_flow *es_flow = (struct ixgbe_fdir_flow *)flow;
 *
 * Each engine provides a `flow_size` field giving the memory required for its
 * flow structure. The driver must set it for every engine, as it is used to size
 * flow allocations. An engine that needs nothing beyond the driver's `rte_flow`
 * should use `sizeof(struct rte_flow)`.
 *
 * Engine references and engine_idx:
 *
 * Every flow stores its engine index as the engine type discriminator. Runtime
 * code rebuilds a `ci_flow_engine_ref` (engine pointer + index) from the
 * immutable engine list and this index. This means:
 *
 * - The framework only ever works with immutable engine definitions/lists.
 * - No pointer-to-index lookups are needed after flow creation.
 * - Secondary processes work correctly: the index is the stable identity, while
 *   pointer values are only meaningful within the shared immutable engine list.
 *
 * Treat engine references as the basic building block when handling engine
 * pointers and indices.
 *
 * Per-engine private data (engine_priv):
 *
 * Each flow carries a pointer to engine-private data, such as custom allocation
 * structures used with flow_alloc/flow_free.
 *
 * Fallback allocation:
 *
 * If a driver-provided `flow_alloc` returns NULL, the framework falls back to
 * rte_zmalloc(). The `fallback_alloc` flag then routes deallocation to
 * rte_free(). Consequently, `flow_alloc` returning NULL is *not* treated as an
 * error.
 */
struct ci_flow {
	/* linkage in the per-device flow list */
	TAILQ_ENTRY(ci_flow) node;
	/* device this flow belongs to */
	struct rte_eth_dev_data *dev_data;
	/* index of engine this flow was created by */
	size_t engine_idx;
	/* per-engine private data pointer, set by the framework at alloc time */
	void *engine_priv;
	/* set if flow_alloc returned NULL and rte_zmalloc() was used instead */
	bool fallback_alloc;
};

/* flow engine definition */
struct ci_flow_engine {
	/* engine name */
	const char *name;
	/* size of engine parse context, can be omitted */
	size_t ctx_size;
	/* size of engine flow struct, at least sizeof(struct ci_flow) */
	size_t flow_size;
	/* size of per-device engine private data, can be omitted */
	size_t priv_size;
	/* ops for this flow engine */
	const struct ci_flow_engine_ops *ops;
	/* pattern graph this engine supports, can be omitted */
	const struct flow_graph *graph;
};

/* engine reference: immutable engine pointer paired with its list index */
struct ci_flow_engine_ref {
	/* engine definition */
	const struct ci_flow_engine *engine;
	/* index of the engine in the driver's engine list */
	size_t engine_idx;
};

#define CI_FLOW_ENGINE_MAX	64

/* flow engine list definition */
struct ci_flow_engine_list {
	/* NULL-terminated immutable array of flow engine pointers */
	const struct ci_flow_engine * const engines[CI_FLOW_ENGINE_MAX];
};

/* per-device engine configuration */
struct ci_flow_engine_conf {
	/* lock protecting the config and the flow list */
	rte_rwlock_t config_lock;
	/* list of flows created on this device */
	TAILQ_HEAD(ci_flow_list, ci_flow) flows;
	/* bitmask of enabled engines, bit N set if engine at index N is enabled */
	uint64_t enabled_engines;
	/* back-reference to device data */
	struct rte_eth_dev_data *dev_data;
	/* reference to the driver's engine list */
	const struct ci_flow_engine_list *engines;
	/* per-engine private data pointers, indexed by engine index */
	void *engine_priv[CI_FLOW_ENGINE_MAX];
};

/* Engine primitives. */

/* caller must hold config lock */
static inline struct ci_flow_engine_ref
ci_flow_engine_ref_from_idx(const struct ci_flow_engine_conf *engine_conf,
		const size_t engine_idx)
{
	/* check if valid reference can be constructed at all */
	if (engine_conf == NULL || engine_conf->engines == NULL ||
			engine_idx >= CI_FLOW_ENGINE_MAX) {
		return (struct ci_flow_engine_ref) {
			.engine = NULL,
			.engine_idx = CI_FLOW_ENGINE_MAX
		};
	}
	/* the inputs are valid, but there may be no engine */
	if (engine_conf->engines->engines[engine_idx] == NULL) {
		return (struct ci_flow_engine_ref) {
			.engine = NULL,
			.engine_idx = CI_FLOW_ENGINE_MAX
		};
	}

	return (struct ci_flow_engine_ref) {
		.engine = engine_conf->engines->engines[engine_idx],
		.engine_idx = engine_idx,
	};
}

#define CI_FLOW_ENGINE_REF_FOREACH(engine_ref, engine_conf)                         \
	for (size_t __ci_flow_engine_idx = 0;                                        \
		__ci_flow_engine_idx < CI_FLOW_ENGINE_MAX &&                         \
			(((engine_ref) = ci_flow_engine_ref_from_idx((engine_conf),  \
		__ci_flow_engine_idx)).engine != NULL);                              \
		__ci_flow_engine_idx++)

static inline bool
ci_flow_engine_is_valid(const struct ci_flow_engine *engine)
{
	if (engine == NULL)
		return false;
	if (engine->name == NULL)
		return false;
	if (engine->ops == NULL)
		return false;
	if (engine->ops->ctx_init == NULL)
		return false;
	/* flow size cannot be less than ci_flow */
	if (engine->flow_size < sizeof(struct ci_flow))
		return false;
	/* alloc and free must both be defined or NULL */
	if ((engine->ops->flow_alloc == NULL) != (engine->ops->flow_free == NULL))
		return false;
	/* register and unregister must both be defined or NULL */
	if ((engine->ops->flow_register == NULL) != (engine->ops->flow_unregister == NULL))
		return false;
	return true;
}

/* caller must hold config lock */
static inline bool
ci_flow_engine_is_enabled(const struct ci_flow_engine_conf *conf,
		const size_t engine_idx)
{
	return (conf->enabled_engines & RTE_BIT64(engine_idx)) != 0;
}

/* caller must hold config lock */
static inline void
ci_flow_engine_set_enabled(struct ci_flow_engine_conf *conf, const size_t engine_idx,
		bool enabled)
{
	if (enabled)
		conf->enabled_engines |= RTE_BIT64(engine_idx);
	else
		conf->enabled_engines &= ~RTE_BIT64(engine_idx);
}

/* Engine lifecycle primitives. */

/* caller must hold config lock */
static inline int
ci_flow_engine_init(struct ci_flow_engine_conf *engine_conf,
		struct ci_flow_engine_ref engine_ref)
{
	const struct ci_flow_engine *engine = engine_ref.engine;
	void *priv = NULL;
	int ret;

	if (engine->priv_size > 0) {
		priv = rte_zmalloc(engine->name, engine->priv_size, 0);
		if (priv == NULL) {
			ret = -ENOMEM;
			goto err;
		}
	}

	if (engine->ops->engine_init != NULL) {
		ret = engine->ops->engine_init(engine, engine_conf->dev_data, priv);
		if (ret != 0)
			goto err;
	}
	engine_conf->engine_priv[engine_ref.engine_idx] = priv;
	return 0;
err:
	rte_free(priv);
	return ret;
}

/* caller must hold config lock */
static inline void
ci_flow_engine_uninit(struct ci_flow_engine_conf *engine_conf,
		struct ci_flow_engine_ref engine_ref)
{
	const struct ci_flow_engine *engine = engine_ref.engine;
	void *priv;

	priv = engine_conf->engine_priv[engine_ref.engine_idx];

	CI_DRV_LOG(DEBUG, "engine '%s': uninit", engine->name);
	/* ignore uninit errors */
	if (engine->ops->engine_uninit != NULL)
		engine->ops->engine_uninit(engine, priv);

	rte_free(priv);
	engine_conf->engine_priv[engine_ref.engine_idx] = NULL;
}

/* Flow primitives. */

/* caller must hold config lock */
static inline bool
ci_flow_is_valid(const struct ci_flow *flow,
		const struct ci_flow_engine_conf *engine_conf)
{
	if (flow == NULL)
		return false;
	if (engine_conf == NULL || engine_conf->dev_data == NULL ||
			engine_conf->engines == NULL)
		return false;
	if (flow->dev_data != engine_conf->dev_data)
		return false;
	if (flow->engine_idx >= CI_FLOW_ENGINE_MAX)
		return false;
	if (!ci_flow_engine_is_enabled(engine_conf, flow->engine_idx))
		return false;
	return true;
}

static inline struct ci_flow *
ci_flow_alloc(const struct ci_flow_engine_conf *engine_conf,
		struct ci_flow_engine_ref engine_ref)
{
	const struct ci_flow_engine *engine = engine_ref.engine;
	void *priv = engine_conf->engine_priv[engine_ref.engine_idx];
	struct ci_flow *flow = NULL;
	bool fallback = false;

	/* if engine has an allocator callback, try it first */
	if (engine->ops->flow_alloc != NULL)
		flow = engine->ops->flow_alloc(engine, engine_conf->dev_data, priv);
	/* if allocator callback is not defined or failed, use default allocator */
	if (flow == NULL) {
		flow = (struct ci_flow *)rte_zmalloc(NULL, engine->flow_size, 0);

		/* if callback exists and we're here, allocation has fallen back */
		if (flow != NULL && engine->ops->flow_alloc != NULL)
			fallback = true;
	}
	if (flow != NULL) {
		/* initialize the common part */
		memset(flow, 0, sizeof(struct ci_flow));
		flow->fallback_alloc = fallback;
		flow->engine_idx = engine_ref.engine_idx;
		flow->dev_data = engine_conf->dev_data;
		flow->engine_priv = priv;
	}
	return flow;
}

static inline void
ci_flow_free(struct ci_flow_engine_ref engine_ref, struct ci_flow *flow)
{
	const struct ci_flow_engine *engine = engine_ref.engine;

	if (engine->ops->flow_free != NULL && !flow->fallback_alloc)
		engine->ops->flow_free(flow, flow->dev_data, flow->engine_priv);
	else
		rte_free(flow);
}

/* Track stage - caller must hold config lock */
static inline int
ci_flow_register(struct ci_flow_engine_ref engine_ref,
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	const struct ci_flow_engine *engine = engine_ref.engine;

	if (engine->ops->flow_register != NULL)
		return engine->ops->flow_register(flow, error);

	return 0;
}

/* Track stage - caller must hold config lock */
static inline int
ci_flow_unregister(struct ci_flow_engine_ref engine_ref,
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	const struct ci_flow_engine *engine = engine_ref.engine;

	if (engine->ops->flow_unregister != NULL)
		return engine->ops->flow_unregister(flow, error);

	return 0;
}

/* Apply stage - caller must hold config lock */
static inline int
ci_flow_install(struct ci_flow_engine_ref engine_ref,
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	const struct ci_flow_engine *engine = engine_ref.engine;

	if (engine->ops->flow_install != NULL)
		return engine->ops->flow_install(flow, error);

	return 0;
}

/* Apply stage - caller must hold config lock */
static inline int
ci_flow_uninstall(struct ci_flow_engine_ref engine_ref,
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	const struct ci_flow_engine *engine = engine_ref.engine;

	if (engine->ops->flow_uninstall != NULL)
		return engine->ops->flow_uninstall(flow, error);

	return 0;
}

/* Configuration primitives. */

/* caller must serialize initialization */
static inline int
ci_flow_engine_conf_init(struct ci_flow_engine_conf *engine_conf,
		const struct ci_flow_engine_list *engine_list,
		struct rte_eth_dev_data *dev_data)
{
	struct ci_flow_engine_ref engine_ref;

	if (engine_conf == NULL || engine_list == NULL || dev_data == NULL)
		return -1;

	rte_rwlock_init(&engine_conf->config_lock);

	engine_conf->dev_data = dev_data;
	engine_conf->engines = engine_list;

	TAILQ_INIT(&engine_conf->flows);

	CI_FLOW_ENGINE_REF_FOREACH(engine_ref, engine_conf) {
		if (!ci_flow_engine_is_valid(engine_ref.engine)) {
			CI_DRV_LOG(DEBUG, "engine[%zu]: invalid, skipping",
					engine_ref.engine_idx);
			continue;
		}
		int ret = ci_flow_engine_init(engine_conf, engine_ref);

		if (ret != 0) {
			if (ret == -ENOTSUP)
				CI_DRV_LOG(DEBUG, "engine '%s': not supported, skipping",
						engine_ref.engine->name);
			else
				CI_DRV_LOG(ERR, "engine '%s': init failed (%d: %s), skipping",
						engine_ref.engine->name, ret, rte_strerror(-ret));
			continue;
		}

		ci_flow_engine_set_enabled(engine_conf,
				engine_ref.engine_idx, true);
		CI_DRV_LOG(DEBUG, "engine '%s': enabled", engine_ref.engine->name);
	}
	return 0;
}

/* caller must hold config lock */
static inline void
ci_flow_engine_conf_clear(struct ci_flow_engine_conf *engine_conf)
{
	struct ci_flow *flow, *tmp;

	RTE_TAILQ_FOREACH_SAFE(flow, &engine_conf->flows, node, tmp) {
		struct ci_flow_engine_ref engine_ref;
		struct rte_flow_error error = {0};
		int unregister_ret;

		engine_ref = ci_flow_engine_ref_from_idx(engine_conf, flow->engine_idx);
		unregister_ret = ci_flow_unregister(engine_ref, flow, &error);
		if (unregister_ret != 0)
			CI_DRV_LOG(DEBUG, "engine '%s': failed to unregister flow: %s",
					engine_ref.engine->name, error.message);

		TAILQ_REMOVE(&engine_conf->flows, flow, node);
		ci_flow_free(engine_ref, flow);
	}
}
/* caller must serialize teardown */
static inline void
ci_flow_engine_conf_reset(struct ci_flow_engine_conf *engine_conf)
{
	struct ci_flow_engine_ref engine_ref;

	ci_flow_engine_conf_clear(engine_conf);

	CI_FLOW_ENGINE_REF_FOREACH(engine_ref, engine_conf) {
		if (!ci_flow_engine_is_enabled(engine_conf, engine_ref.engine_idx))
			continue;
		/* ignore errors */
		ci_flow_engine_uninit(engine_conf, engine_ref);
		ci_flow_engine_set_enabled(engine_conf,
				engine_ref.engine_idx, false);
	}

	engine_conf->dev_data = NULL;
	engine_conf->engines = NULL;
}

/* Driver-facing configuration operations. These take the config lock internally. */

/* reinstall kept flows to hardware */
static inline int
ci_flow_engine_conf_replay(struct ci_flow_engine_conf *engine_conf)
{
	struct rte_flow_error error = {0};
	struct ci_flow *flow;

	CI_DRV_LOG(DEBUG, "replaying all flows");

	rte_rwlock_write_lock(&engine_conf->config_lock);

	TAILQ_FOREACH(flow, &engine_conf->flows, node) {
		struct ci_flow_engine_ref engine_ref;
		int install_ret;

		/* this shouldn't happen */
		if (!ci_flow_is_valid(flow, engine_conf))
			continue;

		engine_ref = ci_flow_engine_ref_from_idx(engine_conf,
				flow->engine_idx);

		install_ret = ci_flow_install(engine_ref, flow, &error);
		if (install_ret != 0) {
			CI_DRV_LOG(DEBUG, "engine '%s': failed to install flow: %s",
					engine_ref.engine->name, error.message);
		}
	}

	rte_rwlock_write_unlock(&engine_conf->config_lock);

	return 0;
}

/* drop all flows from config */
static inline int
ci_flow_engine_conf_cleanup(struct ci_flow_engine_conf *engine_conf)
{
	CI_DRV_LOG(DEBUG, "cleaning up all flows");

	rte_rwlock_write_lock(&engine_conf->config_lock);
	ci_flow_engine_conf_clear(engine_conf);
	rte_rwlock_write_unlock(&engine_conf->config_lock);

	return 0;
}

/* Pattern parsing primitives. */

/* accepts only empty and ANY-only patterns */
enum ci_flow_empty_graph_node_id {
	CI_FLOW_EMPTY_GRAPH_NODE_START = FLOW_GRAPH_NODE_FIRST,
	CI_FLOW_EMPTY_GRAPH_NODE_ANY,
	CI_FLOW_EMPTY_GRAPH_NODE_END,
};

static const struct flow_graph ci_flow_empty_graph = {
	.nodes = (struct flow_graph_node []) {
		[CI_FLOW_EMPTY_GRAPH_NODE_START] = {
			.name = "START",
		},
		[CI_FLOW_EMPTY_GRAPH_NODE_ANY] = {
			.name = "ANY",
			.type = RTE_FLOW_ITEM_TYPE_ANY,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[CI_FLOW_EMPTY_GRAPH_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge []) {
		[CI_FLOW_EMPTY_GRAPH_NODE_START] = {
			.next = (size_t []) {
				CI_FLOW_EMPTY_GRAPH_NODE_ANY,
				CI_FLOW_EMPTY_GRAPH_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END,
			},
		},
		[CI_FLOW_EMPTY_GRAPH_NODE_ANY] = {
			.next = (size_t []) {
				CI_FLOW_EMPTY_GRAPH_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END,
			},
		},
	}
};

/* concrete pattern-matching mode selected from graph/callback presence */
enum ci_match_type {
	CI_MATCH_EMPTY,    /* no graph, no callback */
	CI_MATCH_CALLBACK, /* callback only */
	CI_MATCH_GRAPH,    /* graph only */
	CI_MATCH_ALL,      /* callback + graph */
};

/* caller must hold config lock */
static inline int
ci_flow_match(const struct ci_flow_engine *engine,
		const struct rte_flow_item pattern[],
		struct ci_flow_engine_ctx *ctx,
		enum ci_match_type match_type,
		struct rte_flow_error *error)
{
	switch (match_type) {
	case CI_MATCH_EMPTY:
		/* for empty matching, NULL pattern is not an error */
		if (pattern != NULL) {
			return flow_graph_parse(&ci_flow_empty_graph,
					pattern, error, ctx);
		}
		return 0;

	case CI_MATCH_CALLBACK:
		if (pattern == NULL) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					"Pattern cannot be NULL");
		}
		return engine->ops->pattern_parse(ctx, pattern, error);

	case CI_MATCH_GRAPH:
		if (pattern == NULL) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					"Pattern cannot be NULL");
		}
		return flow_graph_parse(engine->graph, pattern, error, ctx);

	case CI_MATCH_ALL:
	{
		int ret;

		if (pattern == NULL) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					"Pattern cannot be NULL");
		}
		ret = engine->ops->pattern_parse(ctx, pattern, error);
		if (ret != 0)
			return ret;
		return flow_graph_parse(engine->graph, pattern, error, ctx);
	}

	default:
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"Invalid match type");
	}
}

/* caller must hold config lock */
static inline int
ci_flow_parse(const struct ci_flow_engine_conf *engine_conf,
		const struct ci_flow_engine *engine,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	enum ci_match_type match_type;
	struct ci_flow_engine_ctx *ctx;
	int ret = 0;

	/*
	 * Matching mode depends on whether a pattern graph and/or a
	 * pattern_parse callback are provided:
	 *
	 * - graph only: match against the graph. This is the expected default;
	 *   no special handling of pattern items.
	 *
	 * - graph + callback: the callback preprocesses the pattern, then the
	 *   graph engine does the matching. The graph is expected to have an
	 *   ignore list to skip nodes not meant for it.
	 *
	 * - callback only: fully custom pattern parsing.
	 *
	 * - neither: match against an empty graph, for engines that don't care
	 *   about the pattern. Default behavior accepts a NULL pattern and
	 *   END or ANY -> END patterns, and rejects anything else (so the
	 *   pattern is still checked, not ignored).
	 */
	match_type = engine->graph == NULL ?
		(engine->ops->pattern_parse == NULL ? CI_MATCH_EMPTY : CI_MATCH_CALLBACK) :
		(engine->ops->pattern_parse == NULL ? CI_MATCH_GRAPH : CI_MATCH_ALL);

	CI_DRV_LOG(DEBUG, "engine '%s': parsing flow", engine->name);

	ctx = (struct ci_flow_engine_ctx *)calloc(1,
			RTE_MAX(engine->ctx_size, sizeof(struct ci_flow_engine_ctx)));
	if (ctx == NULL) {
		return rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to allocate memory for rule engine context");
	}
	ctx->dev_data = engine_conf->dev_data;
	ctx->engine_priv = flow->engine_priv;
	ctx->attr = attr;
	ctx->pattern = pattern;
	ctx->actions = actions;
	flow->dev_data = engine_conf->dev_data;

	ret = engine->ops->ctx_init(actions, attr, ctx, error);
	if (ret != 0)
		goto free_ctx;

	ret = ci_flow_match(engine, pattern, ctx, match_type, error);
	if (ret != 0)
		goto free_ctx;

	if (engine->ops->ctx_finalize != NULL)
		ret = engine->ops->ctx_finalize(ctx, error);
	if (ret != 0)
		goto free_ctx;

	if (engine->ops->ctx_to_flow != NULL) {
		ret = engine->ops->ctx_to_flow(ctx, flow, error);
		if (ret != 0)
			goto free_ctx;
	}
	ret = 0;

free_ctx:
	free(ctx);
	return ret;
}

/* Default rte_flow op implementations. These take the config lock internally. */

static inline struct rte_flow *
ci_flow_create(struct ci_flow_engine_conf *engine_conf,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct ci_flow_engine_ref engine_ref;
	struct ci_flow *flow = NULL;
	struct rte_flow_error local_error = {0};
	int ret;

	if (error == NULL)
		error = &local_error;

	if (attr == NULL || actions == NULL) {
		CI_DRV_LOG(DEBUG, "attr or actions is NULL");
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				"Attributes and actions cannot be NULL");
		return NULL;
	}

	rte_rwlock_write_lock(&engine_conf->config_lock);

	CI_FLOW_ENGINE_REF_FOREACH(engine_ref, engine_conf) {
		if (!ci_flow_engine_is_enabled(engine_conf, engine_ref.engine_idx))
			continue;

		flow = ci_flow_alloc(engine_conf, engine_ref);
		if (flow == NULL) {
			CI_DRV_LOG(DEBUG, "engine '%s': failed to allocate flow",
					engine_ref.engine->name);
			rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to allocate memory for flow rule");
			/* this is a serious error so don't continue */
			goto unlock;
		}

		ret = ci_flow_parse(engine_conf, engine_ref.engine, attr, pattern,
				actions, flow, error);

		/* parsing failed - free the flow and try next engine */
		if (ret != 0) {
			ci_flow_free(engine_ref, flow);
			CI_DRV_LOG(DEBUG, "engine '%s' rejected flow: %s",
				engine_ref.engine->name, error->message);
			/* if parsing failed due to calloc failure */
			if (ret == -ENOMEM) {
				flow = NULL;
				goto unlock;
			}
			continue;
		}

		CI_DRV_LOG(DEBUG, "engine '%s' accepted flow, committing",
				engine_ref.engine->name);

		ret = ci_flow_register(engine_ref, flow, error);
		if (ret != 0) {
			/* track failed after parse accepted - do not try other engines */
			CI_DRV_LOG(DEBUG, "engine '%s' register failed: %s",
					engine_ref.engine->name, error->message);
			ci_flow_free(engine_ref, flow);
			flow = NULL;
			goto unlock;
		}

		ret = ci_flow_install(engine_ref, flow, error);
		if (ret != 0) {
			struct rte_flow_error unreg_error = { 0 };
			int unreg_ret;

			/* install failed after parse accepted - do not try other engines */
			CI_DRV_LOG(DEBUG, "engine '%s' install failed: %s",
					engine_ref.engine->name, error->message);
			/* separate error struct to preserve the install error */
			unreg_ret = ci_flow_unregister(engine_ref, flow, &unreg_error);
			if (unreg_ret != 0)
				CI_DRV_LOG(DEBUG, "engine '%s': failed to unregister: %s",
						engine_ref.engine->name, unreg_error.message);
			ci_flow_free(engine_ref, flow);
			flow = NULL;
			goto unlock;
		}

		CI_DRV_LOG(DEBUG, "flow installed by engine '%s'",
				engine_ref.engine->name);
		TAILQ_INSERT_TAIL(&engine_conf->flows, flow, node);
		goto unlock;
	}

	CI_DRV_LOG(DEBUG, "no engine accepted the flow");
	flow = NULL;
	rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"No flow engine could handle the requested flow");
unlock:
	rte_rwlock_write_unlock(&engine_conf->config_lock);

	return (struct rte_flow *)flow;
}

static inline int
ci_flow_validate(struct ci_flow_engine_conf *engine_conf,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct ci_flow_engine_ref engine_ref;
	struct rte_flow_error local_error = {0};
	int ret;

	if (error == NULL)
		error = &local_error;

	if (attr == NULL || actions == NULL) {
		CI_DRV_LOG(DEBUG, "attr or actions is NULL");
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				"Attributes and actions cannot be NULL");
	}

	rte_rwlock_read_lock(&engine_conf->config_lock);

	CI_FLOW_ENGINE_REF_FOREACH(engine_ref, engine_conf) {
		struct ci_flow *flow;

		if (!ci_flow_engine_is_enabled(engine_conf, engine_ref.engine_idx))
			continue;

		/* flow is not kept, so skip the engine allocator */
		flow = (struct ci_flow *)calloc(1, engine_ref.engine->flow_size);
		if (flow == NULL) {
			CI_DRV_LOG(DEBUG, "engine '%s': failed to allocate flow",
					engine_ref.engine->name);
			ret = rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to allocate memory for flow rule");
			goto unlock;
		}
		flow->fallback_alloc = false;
		flow->engine_idx = engine_ref.engine_idx;
		flow->dev_data = engine_conf->dev_data;
		flow->engine_priv = engine_conf->engine_priv[engine_ref.engine_idx];

		ret = ci_flow_parse(engine_conf, engine_ref.engine, attr, pattern,
				actions, flow, error);
		free(flow);

		if (ret == 0) {
			CI_DRV_LOG(DEBUG, "engine '%s' accepted flow",
					engine_ref.engine->name);
			goto unlock;
		} else {
			CI_DRV_LOG(DEBUG, "engine '%s' rejected flow: %s",
					engine_ref.engine->name, error->message);
		}
		/* if parsing failed to due to calloc failure */
		if (ret == -ENOMEM) {
			goto unlock;
		}
	}
	CI_DRV_LOG(DEBUG, "no engine accepted the flow");
	ret = rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"No flow engine could handle the requested flow");
unlock:
	rte_rwlock_read_unlock(&engine_conf->config_lock);
	return ret;
}

static inline int
ci_flow_destroy(struct ci_flow_engine_conf *engine_conf,
		struct rte_flow *rte_flow,
		struct rte_flow_error *error)
{
	struct ci_flow *flow = (struct ci_flow *)rte_flow;
	struct ci_flow_engine_ref engine_ref;
	struct rte_flow_error local_error = {0};
	struct rte_flow_error unreg_error = {0};
	int unreg_ret, ret = 0;

	if (error == NULL)
		error = &local_error;

	if (rte_flow == NULL) {
		CI_DRV_LOG(DEBUG, "flow handle is NULL");
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Flow handle cannot be NULL");
	}

	rte_rwlock_write_lock(&engine_conf->config_lock);

	if (!ci_flow_is_valid(flow, engine_conf)) {
		CI_DRV_LOG(DEBUG, "invalid flow handle");
		ret = rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Invalid flow handle");
		goto unlock;
	}
	engine_ref = ci_flow_engine_ref_from_idx(engine_conf, flow->engine_idx);
	CI_DRV_LOG(DEBUG, "uninstalling flow (engine '%s')",
			engine_ref.engine->name);

	ret = ci_flow_uninstall(engine_ref, flow, error);

	if (ret != 0) {
		CI_DRV_LOG(DEBUG, "uninstall failed: %s", error->message);
		goto unlock;
	}

	/* hardware is already clean, so untrack failure is not fatal */
	unreg_ret = ci_flow_unregister(engine_ref, flow, &unreg_error);
	if (unreg_ret != 0)
		CI_DRV_LOG(DEBUG, "engine '%s': failed to unregister flow: %s",
				engine_ref.engine->name, unreg_error.message);

	TAILQ_REMOVE(&engine_conf->flows, flow, node);
	ci_flow_free(engine_ref, flow);
unlock:
	rte_rwlock_write_unlock(&engine_conf->config_lock);

	return ret;
}

static inline int
ci_flow_flush(struct ci_flow_engine_conf *engine_conf,
		struct rte_flow_error *error)
{
	struct ci_flow *flow, *tmp;
	struct rte_flow_error local_error = {0};
	bool error_set = false;
	int ret = 0;

	if (error == NULL)
		error = &local_error;

	CI_DRV_LOG(DEBUG, "removing all flows");

	rte_rwlock_write_lock(&engine_conf->config_lock);

	RTE_TAILQ_FOREACH_SAFE(flow, &engine_conf->flows, node, tmp) {
		struct ci_flow_engine_ref engine_ref;
		struct rte_flow_error flow_error = {0};
		int uninstall_ret;
		int unreg_ret;

		/* this shouldn't happen */
		if (!ci_flow_is_valid(flow, engine_conf))
			continue;

		engine_ref = ci_flow_engine_ref_from_idx(engine_conf,
				flow->engine_idx);

		uninstall_ret = ci_flow_uninstall(engine_ref, flow, &flow_error);

		/* uninstall might legitimately be expected to fail at flush time */
		if (uninstall_ret != 0)
			CI_DRV_LOG(DEBUG, "engine '%s': failed to uninstall flow: %s",
					engine_ref.engine->name, flow_error.message);

		flow_error = (struct rte_flow_error){0};
		unreg_ret = ci_flow_unregister(engine_ref, flow, &flow_error);
		if (unreg_ret != 0) {
			CI_DRV_LOG(ERR, "engine '%s': failed to untrack flow: %s",
					engine_ref.engine->name, flow_error.message);
			if (!error_set) {
				ret = unreg_ret;
				*error = flow_error;
				error_set = true;
			}
		}

		TAILQ_REMOVE(&engine_conf->flows, flow, node);
		ci_flow_free(engine_ref, flow);
	}

	rte_rwlock_write_unlock(&engine_conf->config_lock);
	if (error_set)
		rte_errno = -ret;

	return ret;
}

static inline int
ci_flow_query(struct ci_flow_engine_conf *engine_conf,
		struct rte_flow *rte_flow,
		const struct rte_flow_action *action,
		void *data,
		struct rte_flow_error *error)
{
	struct ci_flow *flow = (struct ci_flow *)rte_flow;
	struct ci_flow_engine_ref engine_ref;
	struct rte_flow_error local_error = {0};
	int ret;

	if (error == NULL)
		error = &local_error;

	if (action == NULL || data == NULL) {
		CI_DRV_LOG(DEBUG, "action or data is NULL");
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Action or data cannot be NULL");
	}

	rte_rwlock_read_lock(&engine_conf->config_lock);

	if (!ci_flow_is_valid(flow, engine_conf)) {
		CI_DRV_LOG(DEBUG, "invalid flow handle");
		ret = rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Invalid flow handle");
		goto unlock;
	}
	engine_ref = ci_flow_engine_ref_from_idx(engine_conf, flow->engine_idx);
	if (engine_ref.engine->ops->flow_query != NULL) {
		ret = engine_ref.engine->ops->flow_query(flow, action, data, error);

		if (ret != 0)
			CI_DRV_LOG(DEBUG, "engine '%s' query failed: %s",
					engine_ref.engine->name, error->message);
	} else {
		CI_DRV_LOG(DEBUG, "engine '%s' does not support querying",
				engine_ref.engine->name);
		ret = rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Flow engine does not support querying");
	}
unlock:
	rte_rwlock_read_unlock(&engine_conf->config_lock);

	return ret;
}

#define CI_FLOW_DUMP_CHUNK_BYTES 32

static inline void
ci_flow_dump_one(FILE *file, const char *driver, const char *engine,
		const void *data, size_t data_len)
{
	const uint8_t *raw = (const uint8_t *)data;
	const size_t nchunks =
		(data_len + CI_FLOW_DUMP_CHUNK_BYTES - 1) /
		CI_FLOW_DUMP_CHUNK_BYTES;
	char title[64];
	size_t ci;

	fprintf(file, "FLOW DUMP: driver=%s engine=%s\n", driver, engine);
	fprintf(file, "FLOW DUMP: DATA size=%zu chunks=%zu chunk_bytes=%d\n",
		data_len, nchunks, CI_FLOW_DUMP_CHUNK_BYTES);

	for (ci = 0; ci < nchunks; ci++) {
		const size_t off = ci * CI_FLOW_DUMP_CHUNK_BYTES;
		const size_t clen =
			RTE_MIN((size_t)CI_FLOW_DUMP_CHUNK_BYTES,
				data_len - off);
		snprintf(title, sizeof(title), "FLOW DUMP: chunk %03zu/%03zu",
			 ci + 1, nchunks);
		rte_memdump(file, title, raw + off, clen);
	}
}

/* rte_flow_dev_dump implementation */
static inline int
ci_flow_dump(struct ci_flow_engine_conf *engine_conf,
		struct rte_flow *flow,
		FILE *file,
		struct rte_flow_error *error)
{
	struct ci_flow_engine_ref engine_ref;
	struct ci_flow *cur;
	struct rte_flow_error local_error = {0};
	bool found = false;
	const char *driver_name =
#ifdef RTE_COMPONENT_NAME
			RTE_STR(RTE_COMPONENT_NAME);
#else
			"unknown";
#endif

	if (error == NULL)
		error = &local_error;

	rte_rwlock_read_lock(&engine_conf->config_lock);

	TAILQ_FOREACH(cur, &engine_conf->flows, node) {
		const void *data;
		size_t data_len;

		if (flow != NULL && (struct rte_flow *)cur != flow)
			continue;

		if (!ci_flow_is_valid(cur, engine_conf)) {
			CI_DRV_LOG(DEBUG, "invalid flow handle: %p, skipping", cur);
			continue;
		}

		found = true;

		engine_ref = ci_flow_engine_ref_from_idx(engine_conf, cur->engine_idx);

		data = RTE_PTR_ADD(cur, sizeof(struct ci_flow));
		data_len = engine_ref.engine->flow_size - sizeof(struct ci_flow);

		/* skipping explicitly keeps static analyzers quiet */
		if (data_len == 0) {
			CI_DRV_LOG(DEBUG, "flow data length is 0, skipping dump");
			continue;
		}

		ci_flow_dump_one(file, driver_name, engine_ref.engine->name,
				data, data_len);
	}

	rte_rwlock_read_unlock(&engine_conf->config_lock);

	if (flow != NULL && !found) {
		return rte_flow_error_set(error, ENOENT,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Flow not found");
	}

	return 0;
}

#endif /* _COMMON_INTEL_FLOW_ENGINE_H_ */
