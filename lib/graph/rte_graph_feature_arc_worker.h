/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_FEATURE_ARC_WORKER_H_
#define _RTE_GRAPH_FEATURE_ARC_WORKER_H_

#include <stddef.h>
#include <rte_graph_feature_arc.h>
#include <rte_bitops.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

/**
 * @file rte_graph_feature_arc_worker.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 *
 * Fast path Graph feature arc API
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 *
 * Slow path feature node info list
 */
struct rte_graph_feature_node_list {
	/** Next feature */
	STAILQ_ENTRY(rte_graph_feature_node_list) next_feature;

	/** Name of the feature */
	char feature_name[RTE_GRAPH_FEATURE_ARC_NAMELEN];

	/** node id representing feature */
	rte_node_t feature_node_id;

	/** How many indexes/interfaces using this feature */
	int32_t ref_count;

	/**
	 * feature arc process function overrides to feature node's original
	 * process function
	 */
	rte_node_process_t feature_node_process_fn;

	/** Callback for freeing application resources when */
	rte_graph_feature_change_notifier_cb_t notifier_cb;

	/** finfo_index in list. same as rte_graph_feature_t */
	uint32_t finfo_index;

	/** Back pointer to feature arc */
	void *feature_arc;

	/** rte_edge_t to this feature node from feature_arc->start_node */
	rte_edge_t edge_to_this_feature;

	/** rte_edge_t from this feature node to last feature node */
	rte_edge_t edge_to_last_feature;
};

/**
 * rte_graph Feature arc object
 *
 * Feature arc object holds control plane and fast path information for all
 * features and all interface index information for steering packets across
 * feature nodes
 *
 * Within a feature arc, only RTE_GRAPH_FEATURE_MAX_PER_ARC features can be
 * added. If more features needs to be added, another feature arc can be
 * created
 *
 * In fast path, rte_graph_feature_arc_t can be translated to (struct
 * rte_graph_feature_arc *) via rte_graph_feature_arc_get(). Later is needed to
 * add as an input argument to all fast path feature arc APIs
 */
struct __rte_cache_aligned rte_graph_feature_arc {
	/** Slow path variables follows*/
	RTE_MARKER slow_path_variables;

	/** All feature lists */
	STAILQ_HEAD(, rte_graph_feature_node_list) all_features;

	/** feature arc name */
	char feature_arc_name[RTE_GRAPH_FEATURE_ARC_NAMELEN];

	/** control plane counter to track enabled features */
	uint32_t runtime_enabled_features;

	/** maximum number of features supported by this arc
	 *  Immutable during fast path
	 */
	uint16_t max_features;

	/** index in feature_arc_main */
	rte_graph_feature_arc_t feature_arc_index;

	/** Back pointer to feature_arc_main */
	void *feature_arc_main;

	/** Arc's start/end node */
	struct rte_node_register *start_node;
	struct rte_graph_feature_register end_feature;

	/** arc start process function */
	rte_node_process_t arc_start_process;

	/** total arc_size allocated */
	size_t arc_size;

	/** slow path: feature data array maintained per [feature, index] */
	rte_graph_feature_data_t *feature_data_by_index;

	/**
	 * Size of all feature data for each feature
	 * ALIGN(sizeof(struct rte_graph_feature_data) * arc->max_indexes)
	 * Not used in fastpath
	 */
	uint32_t feature_size;

	/** Slow path bit mask per feature per index */
	uint64_t *feature_bit_mask_by_index;

	/** Cache aligned fast path variables */
	alignas(RTE_CACHE_LINE_SIZE) RTE_MARKER fast_path_variables;

	/**
	 * Quick fast path bitmask indicating if any feature enabled. Each bit
	 * corresponds to single feature. Helps in optimally process packets for
	 * the case when features are added but not enabled
	 */
	RTE_ATOMIC(uint64_t) fp_feature_enable_bitmask;

	/**
	 * Number of added features. <= max_features
	 */
	uint16_t num_added_features;
	/** maximum number of index supported by this arc
	 *  Immutable during fast path
	 */
	uint16_t max_indexes;

	/** first feature offset in fast path
	 * Immutable during fast path
	 */
	uint16_t fp_first_feature_offset;

	/** arc + fp_feature_data_arr_offset
	 * Immutable during fast path
	 */
	uint16_t fp_feature_data_offset;

	/**
	 * mbuf dynamic offset saved for faster access
	 * See rte_graph_feature_arc_mbuf_dynfields_get() for more details
	 */
	int mbuf_dyn_offset;

	/** Fast path arc data starts */
	/*
	 * Arc specific fast path data
	 * It accommodates:
	 *
	 *	1. first enabled feature data for every index (rte_graph_feature_data_t or fdata)
	 *	+--------------------------------------------------------------+ <- cache_aligned
	 *	|  0th Index    | 1st Index   |  ... | max_index - 1           |
	 *	+--------------------------------------------------------------+
	 *	|  Startfdata0  | Startfdata1 |  ... | Startfdata(max_index-1) |
	 *	+--------------------------------------------------------------+
	 *
	 *	2. struct rte_graph_feature_data per index per feature
	 *	+----------------------------------------+ ^ <- Start (Reserved, cache aligned)
	 *	|  struct rte_graph_feature_data[Index0] | |
	 *	+----------------------------------------+ | feature_size
	 *	|  struct rte_graph_feature_data[Index1] | |
	 *	+----------------------------------------+ ^ <- Feature-0 (cache_aligned)
	 *	|  struct rte_graph_feature_data[Index0] | |
	 *	+----------------------------------------+ | feature_size
	 *	|  struct rte_graph_feature_data[Index1] | |
	 *	+----------------------------------------+ v <- Feature-1 (cache aligned)
	 *	|  struct rte_graph_feature_data[Index0] | ^
	 *	+----------------------------------------+ | feature_size
	 *	|  struct rte_graph_feature_data[Index1] | |
	 *	+----------------------------------------+ v
	 *	|         ...            ....            |
	 *	|         ...            ....            |
	 *	|         ...            ....            |
	 *	+----------------------------------------+ v <- Feature Index-1 (cache aligned)
	 *	|  struct rte_graph_feature_data[Index0] | ^
	 *	+----------------------------------------+ | feature_size
	 *	|  struct rte_graph_feature_data[Index1] | |
	 *	+----------------------------------------+ v <- Extra (Reserved, cache aligned)
	 *	|  struct rte_graph_feature_data[Index0] | ^
	 *	+----------------------------------------+ | feature_size
	 *	|  struct rte_graph_feature_data[Index1] | |
	 *	+----------------------------------------+ v
	 */
	RTE_MARKER8 fp_arc_data;
};

/**
 * Feature arc main object
 *
 * Holds all feature arcs created by application
 */
typedef struct rte_feature_arc_main {
	/** number of feature arcs created by application */
	uint32_t num_feature_arcs;

	/** max features arcs allowed */
	uint32_t max_feature_arcs;

	/** arc_mbuf_dyn_offset for saving feature arc specific
	 * mbuf dynfield offset.
	 *
	 * See rte_graph_feature_arc_mbuf_dynfields_get() for more details
	 */
	int arc_mbuf_dyn_offset;

	/** Pointer to all feature arcs */
	uintptr_t feature_arcs[];
} rte_graph_feature_arc_main_t;

/**
 *  Fast path feature data object
 *
 *  Used by fast path inline feature arc APIs
 *  Corresponding to rte_graph_feature_data_t
 *  It holds
 *  - edge to reach to next feature node
 *  - next_feature_data corresponding to next enabled feature
 *  - app_cookie set by application in rte_graph_feature_enable()
 */
struct rte_graph_feature_data {
	/** edge from this feature node to next enabled feature node */
	RTE_ATOMIC(rte_edge_t) next_edge;

	/**
	 * app_cookie set by application in rte_graph_feature_enable() for
	 * current feature data
	 */
	RTE_ATOMIC(uint16_t) app_cookie;

	/** Next feature data from this feature data */
	RTE_ATOMIC(rte_graph_feature_data_t) next_feature_data;
};

/** feature arc specific mbuf dynfield structure. */
struct rte_graph_feature_arc_mbuf_dynfields {
	/** each mbuf carries feature data */
	rte_graph_feature_data_t feature_data;
};

/** Name of dynamic mbuf field offset registered in rte_graph_feature_arc_init() */
#define RTE_GRAPH_FEATURE_ARC_DYNFIELD_NAME    "__rte_graph_feature_arc_mbuf_dynfield"

/** log2(sizeof (struct rte_graph_feature_data)) */
#define RTE_GRAPH_FEATURE_DATA_SIZE_LOG2	3

/** Number of struct rte_graph_feature_data per feature*/
#define RTE_GRAPH_FEATURE_DATA_NUM_PER_FEATURE(arc)				\
	(arc->feature_size >> RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)

/** Get rte_graph_feature_data_t from rte_graph_feature_t */
#define RTE_GRAPH_FEATURE_TO_FEATURE_DATA(arc, feature, index)			\
		((rte_graph_feature_data_t)					\
		 ((RTE_GRAPH_FEATURE_DATA_NUM_PER_FEATURE(arc) * (feature)) + (index)))

/**
 * @internal macro
 */
#define GRAPH_FEATURE_ARC_PTR_INITIALIZER  ((uintptr_t)UINTPTR_MAX)

/** extern variables */
extern rte_graph_feature_arc_main_t *__rte_graph_feature_arc_main;

/**
 * Get dynfield offset to feature arc specific fields in mbuf
 *
 * Feature arc mbuf dynamic field is separate to utilize mbuf->dynfield2
 * instead of dynfield1
 *
 * This arc specific dynamic offset is registered as part of
 * rte_graph_feature_arc_init() and copied in each arc for fast path access.
 * This avoids node maintaining dynamic offset for feature arc and if we are
 * lucky, field would be allocated from mbuf->dynfield2. Otherwise each node
 * has to maintain at least two dynamic offset in fast path
 *
 * @param mbuf
 *  Pointer to mbuf
 * @param dyn_offset
 *  Retrieved from arc->mbuf_dyn_offset
 *
 * @return
 *  NULL: On Failure
 *  Non-NULL pointer on Success
 */
__rte_experimental
static __rte_always_inline struct rte_graph_feature_arc_mbuf_dynfields *
rte_graph_feature_arc_mbuf_dynfields_get(struct rte_mbuf *mbuf,
					 const int dyn_offset)
{
	return RTE_MBUF_DYNFIELD(mbuf, dyn_offset,
				 struct rte_graph_feature_arc_mbuf_dynfields *);
}

/**
 * API to know if feature is valid or not
 *
 * @param feature
 *  rte_graph_feature_t
 *
 * @return
 *  1: If feature is valid
 *  0: If feature is invalid
 */
__rte_experimental
static __rte_always_inline int
rte_graph_feature_is_valid(rte_graph_feature_t feature)
{
	return (feature != RTE_GRAPH_FEATURE_INVALID);
}

/**
 * API to know if feature data is valid or not
 *
 * @param feature_data
 *  rte_graph_feature_data_t
 *
 * @return
 *  1: If feature data is valid
 *  0: If feature data is invalid
 */
__rte_experimental
static __rte_always_inline int
rte_graph_feature_data_is_valid(rte_graph_feature_data_t feature_data)
{
	return (feature_data != RTE_GRAPH_FEATURE_DATA_INVALID);
}

/**
 * Get pointer to feature arc object from rte_graph_feature_arc_t
 *
 * @param arc
 *  feature arc
 *
 * @return
 *  NULL: On Failure
 *  Non-NULL pointer on Success
 */
__rte_experimental
static __rte_always_inline struct rte_graph_feature_arc *
rte_graph_feature_arc_get(rte_graph_feature_arc_t arc)
{
	uintptr_t fa = GRAPH_FEATURE_ARC_PTR_INITIALIZER;
	rte_graph_feature_arc_main_t *fm = NULL;

	fm = __rte_graph_feature_arc_main;

	if (likely((fm != NULL) && (arc < fm->max_feature_arcs)))
		fa = fm->feature_arcs[arc];

	return (fa == GRAPH_FEATURE_ARC_PTR_INITIALIZER) ?
		NULL : (struct rte_graph_feature_arc *)fa;
}

/**
 * Get rte_graph_feature_t from feature arc object without any checks
 *
 * @param arc
 *  feature arc
 * @param fdata
 *  feature data object
 *
 * @return
 *   Pointer to feature data object
 */
__rte_experimental
static __rte_always_inline struct rte_graph_feature_data*
__rte_graph_feature_data_get(struct rte_graph_feature_arc *arc,
			     rte_graph_feature_data_t fdata)
{
	return ((struct rte_graph_feature_data *) ((uint8_t *)arc + arc->fp_feature_data_offset +
						   (fdata << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));
}

/**
 * Get next edge from feature data pointer, without any check
 *
 * @param fdata
 *  feature data object
 *
 * @return
 *  next edge
 */
__rte_experimental
static __rte_always_inline rte_edge_t
__rte_graph_feature_data_edge_get(struct rte_graph_feature_data *fdata)
{
	return rte_atomic_load_explicit(&fdata->next_edge, rte_memory_order_relaxed);
}

/**
 * Get app_cookie from feature data pointer, without any check
 *
 * @param fdata
 *  feature data object
 *
 * @return
 *  app_cookie set by caller in rte_graph_feature_enable() API
 */
__rte_experimental
static __rte_always_inline uint16_t
__rte_graph_feature_data_app_cookie_get(struct rte_graph_feature_data *fdata)
{
	return rte_atomic_load_explicit(&fdata->app_cookie, rte_memory_order_relaxed);
}

/**
 * Get next_enabled_feature_data from pointer to feature data, without any check
 *
 * @param fdata
 *  feature data object
 *
 * @return
 *  next enabled feature data from this feature data
 */
__rte_experimental
static __rte_always_inline rte_graph_feature_data_t
__rte_graph_feature_data_next_feature_get(struct rte_graph_feature_data *fdata)
{
	return rte_atomic_load_explicit(&fdata->next_feature_data, rte_memory_order_relaxed);
}


/**
 * Get app_cookie from feature data object with checks
 *
 * @param arc
 *  feature arc
 * @param fdata
 *  feature data object
 *
 * @return
 *  app_cookie set by caller in rte_graph_feature_enable() API
 */
__rte_experimental
static __rte_always_inline uint16_t
rte_graph_feature_data_app_cookie_get(struct rte_graph_feature_arc *arc,
				      rte_graph_feature_data_t fdata)
{
	struct rte_graph_feature_data *fdata_obj = ((struct rte_graph_feature_data *)
						    ((uint8_t *)arc + arc->fp_feature_data_offset +
						    (fdata << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));


	return rte_atomic_load_explicit(&fdata_obj->app_cookie, rte_memory_order_relaxed);
}

/**
 * Get next_enabled_feature_data from current feature data object with checks
 *
 * @param arc
 *  feature arc
 * @param fdata
 *  Pointer to feature data object
 * @param[out] next_edge
 *  next_edge from current feature to next enabled feature
 *
 * @return
 *  1: if next feature enabled on index
 *  0: if no feature is enabled on index
 */
__rte_experimental
static __rte_always_inline int
rte_graph_feature_data_next_feature_get(struct rte_graph_feature_arc *arc,
					rte_graph_feature_data_t *fdata,
					rte_edge_t *next_edge)
{
	struct rte_graph_feature_data *fdptr = ((struct rte_graph_feature_data *)
						((uint8_t *)arc + arc->fp_feature_data_offset +
						((*fdata) << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));
	*fdata = rte_atomic_load_explicit(&fdptr->next_feature_data, rte_memory_order_relaxed);
	*next_edge = rte_atomic_load_explicit(&fdptr->next_edge, rte_memory_order_relaxed);


	return ((*fdata) != RTE_GRAPH_FEATURE_DATA_INVALID);
}

/**
 * Get struct rte_graph_feature_data from rte_graph_feature_dat_t
 *
 * @param arc
 *   feature arc
 * @param fdata
 *  feature data object
 *
 * @return
 *   NULL: On Failure
 *   Non-NULL pointer on Success
 */
__rte_experimental
static __rte_always_inline struct rte_graph_feature_data*
rte_graph_feature_data_get(struct rte_graph_feature_arc *arc,
			   rte_graph_feature_data_t fdata)
{
	if (fdata != RTE_GRAPH_FEATURE_DATA_INVALID)
		return ((struct rte_graph_feature_data *)
			((uint8_t *)arc + arc->fp_feature_data_offset +
			(fdata << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));
	else
		return NULL;
}

/**
 * Get feature data corresponding to first enabled feature on index
 * @param arc
 *   feature arc
 * @param index
 *   Interface index
 * @param[out] fdata
 *  feature data object
 * @param[out] edge
 *  rte_edge object
 *
 * @return
 *  1: if any feature enabled on index, return corresponding valid feature data
 *  0: if no feature is enabled on index
 */
__rte_experimental
static __rte_always_inline int
rte_graph_feature_data_first_feature_get(struct rte_graph_feature_arc *arc,
					 uint32_t index,
					 rte_graph_feature_data_t *fdata,
					 rte_edge_t *edge)
{
	struct rte_graph_feature_data *fdata_obj = NULL;
	rte_graph_feature_data_t *fd;

	fd = (rte_graph_feature_data_t *)((uint8_t *)arc + arc->fp_first_feature_offset +
					  (sizeof(rte_graph_feature_data_t) * index));

	if ((*fd) != RTE_GRAPH_FEATURE_DATA_INVALID) {
		fdata_obj = ((struct rte_graph_feature_data *)
			     ((uint8_t *)arc + arc->fp_feature_data_offset +
			     ((*fd) << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));

		*edge = rte_atomic_load_explicit(&fdata_obj->next_edge,
						 rte_memory_order_relaxed);

		*fdata = rte_atomic_load_explicit(&fdata_obj->next_feature_data,
						  rte_memory_order_relaxed);
		return 1;
	}

	return 0;
}

/**
 * Fast path API to check if any feature enabled on a feature arc
 * Typically from arc->start_node process function
 *
 * @param arc
 *   Feature arc object
 *
 * @return
 *  0: If no feature enabled
 *  Non-Zero: Bitmask of features enabled.
 */
__rte_experimental
static __rte_always_inline uint64_t
rte_graph_feature_arc_is_any_feature_enabled(struct rte_graph_feature_arc *arc)
{
	if (unlikely(arc == NULL))
		return 0;

	return (rte_atomic_load_explicit(&arc->fp_feature_enable_bitmask,
					 rte_memory_order_relaxed));
}

/**
 * Prefetch feature arc fast path cache line
 *
 * @param arc
 *   RTE_GRAPH feature arc object
 */
__rte_experimental
static __rte_always_inline void
rte_graph_feature_arc_prefetch(struct rte_graph_feature_arc *arc)
{
	rte_prefetch0((void *)arc->fast_path_variables);
}

/**
 * Prefetch feature data related fast path cache line
 *
 * @param arc
 *   RTE_GRAPH feature arc object
 * @param fdata
 *   Pointer to feature data object
 */
__rte_experimental
static __rte_always_inline void
rte_graph_feature_arc_feature_data_prefetch(struct rte_graph_feature_arc *arc,
					    rte_graph_feature_data_t fdata)
{
	struct rte_graph_feature_data *fdata_obj = ((struct rte_graph_feature_data *)
						    ((uint8_t *)arc + arc->fp_feature_data_offset +
						    (fdata << RTE_GRAPH_FEATURE_DATA_SIZE_LOG2)));
	rte_prefetch0((void *)fdata_obj);
}

#ifdef __cplusplus
}
#endif

#endif
