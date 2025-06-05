/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <rte_graph_feature_arc_worker.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <eal_export.h>
#include "graph_private.h"

#define GRAPH_FEATURE_MAX_NUM_PER_ARC  (64)

#define connect_graph_nodes(node1, node2, edge, arc_name) \
	__connect_graph_nodes(node1, node2, edge, arc_name, __LINE__)

#define FEATURE_ARC_MEMZONE_NAME "__rte_feature_arc_main_mz"

#define NUM_EXTRA_FEATURE_DATA   (2)

#define feat_dbg graph_dbg

#define FEAT_COND_ERR(cond, ...)                                           \
	do {                                                               \
		if (cond)                                                  \
			graph_err(__VA_ARGS__);                            \
	} while (0)

#define FEAT_ERR(fn, ln, ...)                                              \
		GRAPH_LOG2(ERR, fn, ln, __VA_ARGS__)

#define FEAT_ERR_JMP(_err, fn, ln, ...)                                    \
	do {                                                               \
		FEAT_ERR(fn, ln, __VA_ARGS__);                             \
		rte_errno = _err;                                          \
	} while (0)

#define COND_ERR_JMP(_err, cond, fn, ln, ...)                              \
	do {                                                               \
		if (cond)                                                  \
			FEAT_ERR(fn, ln, __VA_ARGS__);                     \
		rte_errno = _err;                                          \
	} while (0)


static struct rte_mbuf_dynfield rte_graph_feature_arc_mbuf_desc = {
	.name = RTE_GRAPH_FEATURE_ARC_DYNFIELD_NAME,
	.size = sizeof(struct rte_graph_feature_arc_mbuf_dynfields),
	.align = alignof(struct rte_graph_feature_arc_mbuf_dynfields),
};

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_graph_feature_arc_main, 25.07)
rte_graph_feature_arc_main_t *__rte_graph_feature_arc_main;

/* global feature arc list */
static STAILQ_HEAD(, rte_graph_feature_arc_register) feature_arc_list =
					STAILQ_HEAD_INITIALIZER(feature_arc_list);

/* global feature arc list */
static STAILQ_HEAD(, rte_graph_feature_register) feature_list =
					STAILQ_HEAD_INITIALIZER(feature_list);

/* feature registration validate */
static int
feature_registration_validate(struct rte_graph_feature_register *feat_entry,
			      const char *caller_name, int lineno,
			      int check_node_reg_id, /* check feature_node->id */
			      int check_feat_reg_id, /* check feature->feature_node_id */
			      bool verbose /* print error */)
{
	if (!feat_entry) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno, "NULL feature reg");
		return -1;
	}

	if (!feat_entry->feature_name) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "%p: NULL feature name", feat_entry);
		return -1;
	}

	if (!feat_entry->arc_name) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature-\"%s\": No associated arc provided",
			     feat_entry->feature_name);
		return -1;
	}

	if (!feat_entry->feature_process_fn) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature-\"%s\": No process function provided",
			     feat_entry->feature_name);
		return -1;
	}

	if (!feat_entry->feature_node) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature-\"%s\": No feature_node provided",
			     feat_entry->feature_name);
		return -1;
	}

	if (check_node_reg_id && (feat_entry->feature_node->id == RTE_NODE_ID_INVALID)) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature-\"%s\": feature_node with invalid node-id found",
			     feat_entry->feature_name);
		return -1;
	}

	if (check_feat_reg_id && (feat_entry->feature_node_id == RTE_NODE_ID_INVALID)) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature-\"%s\": feature_node_id found invalid",
			     feat_entry->feature_name);
		return -1;
	}
	if (check_feat_reg_id && feat_entry->feature_node) {
		if (feat_entry->feature_node_id != feat_entry->feature_node->id) {
			COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
				     "feature-\"%s\": feature_node_id(%u) not corresponding to %s->id(%u)",
				     feat_entry->feature_name, feat_entry->feature_node_id,
				     feat_entry->feature_node->name, feat_entry->feature_node->id);
			return -1;
		}
	}

	return 0;
}

/* validate arc registration */
static int
arc_registration_validate(struct rte_graph_feature_arc_register *reg,
			  const char *caller_name, int lineno,
			  bool verbose)
{

	if (reg == NULL) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc registration cannot be NULL");
		return -1;
	}

	if (!reg->arc_name) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "feature_arc name cannot be NULL");
		return -1;
	}

	if (reg->max_features > GRAPH_FEATURE_MAX_NUM_PER_ARC) {
		COND_ERR_JMP(EAGAIN, verbose, caller_name, lineno,
			     "arc-\"%s\", invalid number of features (found: %u, exp: %u)",
			     reg->arc_name, reg->max_features, GRAPH_FEATURE_MAX_NUM_PER_ARC);
		return -1;
	}

	if (!reg->max_indexes) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc-\"%s\": Zero max_indexes found",
			     reg->arc_name);
		return -1;
	}

	if (!reg->start_node) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc-\"%s\": start node cannot be NULL",
			     reg->arc_name);
		return -1;
	}

	if (!reg->start_node_feature_process_fn) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc-\"%s\": start node feature_process_fn() cannot be NULL",
			     reg->arc_name);
		return -1;
	}

	if (!reg->end_feature) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc-\"%s\": end_feature cannot be NULL",
			     reg->arc_name);
		return -1;
	}

	if (feature_registration_validate(reg->end_feature, caller_name, lineno, 1, 0, verbose))
		return -1;

	if (strncmp(reg->arc_name, reg->end_feature->arc_name,
		    RTE_GRAPH_FEATURE_ARC_NAMELEN)) {
		COND_ERR_JMP(EINVAL, verbose, caller_name, lineno,
			     "arc-\"%s\"/feature-\"%s\": mismatch in arc_name in end_feature",
			     reg->arc_name, reg->end_feature->feature_name);
		return -1;
	}

	return 0;
}

/* lookup arc registration by name */
static int arc_registration_num(void)
{
	struct rte_graph_feature_arc_register *entry = NULL;
	int num = 0;

	STAILQ_FOREACH(entry, &feature_arc_list, next_arc)
		num++;

	return num;
}


/* lookup arc registration by name */
static int arc_registration_lookup(const char *arc_name,
				   struct rte_graph_feature_arc_register **arc_entry,
				   bool verbose /* print error */)
{
	struct rte_graph_feature_arc_register *entry = NULL;

	STAILQ_FOREACH(entry, &feature_arc_list, next_arc) {
		if (arc_registration_validate(entry, __func__, __LINE__, verbose) < 0)
			continue;

		if (!strncmp(entry->arc_name, arc_name, RTE_GRAPH_FEATURE_ARC_NAMELEN)) {
			if (arc_entry)
				*arc_entry = entry;
			return 1;
		}
	}

	return 0;
}


/* Number of features registered for an ARC
 *
 * i.e number of RTE_GRAPH_FEATURE_REGISTER() for an arc
 */
static int
arc_registered_features_num(const char *arc_name, uint16_t *num_features)
{
	struct rte_graph_feature_arc_register *arc_reg = NULL;
	struct rte_graph_feature_register *feat_entry = NULL;
	uint16_t num = 0;

	/* Check if arc is registered with end_feature */
	if (!arc_registration_lookup(arc_name, &arc_reg, false))
		return -1;

	if (arc_reg->end_feature)
		num++;

	/* Calculate features other than end_feature added in arc */
	STAILQ_FOREACH(feat_entry, &feature_list, next_feature) {
		if (feature_registration_validate(feat_entry, __func__, __LINE__, 1, 0, false) < 0)
			continue;

		if (!strncmp(feat_entry->arc_name, arc_name, strlen(feat_entry->arc_name)))
			num++;
	}

	if (num_features)
		*num_features = num;

	return 0;
}

static int
arc_max_index_get(const char *arc_name, uint16_t *max_indexes)
{
	struct rte_graph_feature_arc_register *arc_reg = NULL;
	struct rte_graph_feature_register *feat_entry = NULL;
	uint16_t index;

	if (!max_indexes)
		return -1;

	/* Check if arc is registered with end_feature */
	if (!arc_registration_lookup(arc_name, &arc_reg, false))
		return -1;

	index = *max_indexes;

	/* Call features override_index_cb(), if set */
	STAILQ_FOREACH(feat_entry, &feature_list, next_feature) {
		if (!feat_entry->override_index_cb)
			continue;

		if (feature_registration_validate(feat_entry, __func__, __LINE__, 1, 0, false) < 0)
			continue;

		index = RTE_MAX(index, feat_entry->override_index_cb());
	}

	*max_indexes = index;

	return 0;
}

/* calculate arc size to be allocated */
static int
feature_arc_reg_calc_size(struct rte_graph_feature_arc_register *reg, size_t *sz,
			  uint16_t *feat_off, uint16_t *fdata_off, uint32_t *fsz,
			  uint16_t *num_index)
{
	size_t ff_size = 0, fdata_size = 0;

	/* first feature array per index */
	ff_size = RTE_ALIGN_CEIL(sizeof(rte_graph_feature_data_t) * reg->max_indexes,
				 RTE_CACHE_LINE_SIZE);


	/* fdata size per feature */
	*fsz = (uint32_t)RTE_ALIGN_CEIL(sizeof(struct rte_graph_feature_data) * reg->max_indexes,
					RTE_CACHE_LINE_SIZE);

	*num_index = (*fsz)/sizeof(struct rte_graph_feature_data);

	/* Allocate feature_data extra by 2.
	 * 0th index is used for first feature data from start_node
	 * Last feature data is used for extra_fdata for end_feature
	 */
	fdata_size = (*fsz) * (reg->max_features + NUM_EXTRA_FEATURE_DATA);

	if (sz)
		*sz = fdata_size + ff_size + sizeof(struct rte_graph_feature_arc);
	if (feat_off)
		*feat_off = sizeof(struct rte_graph_feature_arc);
	if (fdata_off)
		*fdata_off = ff_size + sizeof(struct rte_graph_feature_arc);

	return 0;
}

static rte_graph_feature_data_t *
graph_first_feature_data_ptr_get(struct rte_graph_feature_arc *arc,
				 uint32_t index)
{
	return (rte_graph_feature_data_t *)((uint8_t *)arc + arc->fp_first_feature_offset +
					    (sizeof(rte_graph_feature_data_t) * index));
}

static int
feature_arc_data_reset(struct rte_graph_feature_arc *arc)
{
	rte_graph_feature_data_t *f = NULL;
	uint16_t index;

	arc->runtime_enabled_features = 0;

	for (index = 0; index < arc->max_indexes; index++) {
		f = graph_first_feature_data_ptr_get(arc, index);
		*f = RTE_GRAPH_FEATURE_DATA_INVALID;
	}

	return 0;
}

/*
 * lookup feature name and get control path node_list as well as feature index
 * at which it is inserted
 */
static int
nodeinfo_lkup_by_name(struct rte_graph_feature_arc *arc, const char *feat_name,
		      struct rte_graph_feature_node_list **ffinfo, uint32_t *slot)
{
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t fi = 0;

	if (!feat_name)
		return -1;

	if (slot)
		*slot = UINT32_MAX;

	STAILQ_FOREACH(finfo, &arc->all_features, next_feature) {
		RTE_VERIFY(finfo->feature_arc == arc);
		if (!strncmp(finfo->feature_name, feat_name, strlen(finfo->feature_name))) {
			if (ffinfo)
				*ffinfo = finfo;
			if (slot)
				*slot = fi;
			return 0;
		}
		fi++;
	}
	return -1;
}

/* Lookup used only during rte_graph_feature_add() */
static int
nodeinfo_add_lookup(struct rte_graph_feature_arc *arc, const char *feat_node_name,
		    struct rte_graph_feature_node_list **ffinfo, uint32_t *slot)
{
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t fi = 0;

	if (!feat_node_name)
		return -1;

	if (slot)
		*slot = 0;

	STAILQ_FOREACH(finfo, &arc->all_features, next_feature) {
		RTE_VERIFY(finfo->feature_arc == arc);
		if (!strncmp(finfo->feature_name, feat_node_name, strlen(finfo->feature_name))) {
			if (ffinfo)
				*ffinfo = finfo;
			if (slot)
				*slot = fi;
			return 0;
		}
		/* Update slot where new feature can be added */
		if (slot)
			*slot = fi;
		fi++;
	}

	return -1;
}

/* Get control path node info from provided input feature_index */
static int
nodeinfo_lkup_by_index(struct rte_graph_feature_arc *arc, uint32_t feature_index,
		       struct rte_graph_feature_node_list **ppfinfo,
		       const int do_sanity_check)
{
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t index = 0;

	if (!ppfinfo)
		return -1;

	*ppfinfo = NULL;
	STAILQ_FOREACH(finfo, &arc->all_features, next_feature) {
		/* Check sanity */
		if (do_sanity_check)
			if (finfo->finfo_index != index)
				RTE_VERIFY(0);
		if (index == feature_index) {
			*ppfinfo = finfo;
			return 0;
		}
		index++;
	}
	return -1;
}

/* get existing edge from parent_node -> child_node */
static int
get_existing_edge(const char *arc_name, rte_node_t parent_node,
		  rte_node_t child_node, rte_edge_t *_edge)
{
	char **next_edges = NULL;
	uint32_t i, count = 0;

	RTE_SET_USED(arc_name);

	count = rte_node_edge_get(parent_node, NULL);

	if (!count)
		return -1;

	next_edges = malloc(count);

	if (!next_edges)
		return -1;

	count = rte_node_edge_get(parent_node, next_edges);
	for (i = 0; i < count; i++) {
		if (strstr(rte_node_id_to_name(child_node), next_edges[i])) {
			if (_edge)
				*_edge = (rte_edge_t)i;

			free(next_edges);
			return 0;
		}
	}
	free(next_edges);

	return -1;
}

/* feature arc sanity */
static int
feature_arc_sanity(rte_graph_feature_arc_t _arc)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	rte_graph_feature_arc_main_t *dm = __rte_graph_feature_arc_main;
	uint16_t iter;

	if (!__rte_graph_feature_arc_main)
		return -1;

	if (!arc)
		return -1;

	for (iter = 0; iter < dm->max_feature_arcs; iter++) {
		if (arc == rte_graph_feature_arc_get(iter)) {
			if (arc->feature_arc_index != iter)
				return -1;
			if (arc->feature_arc_main != dm)
				return -1;

			return 0;
		}
	}
	return -1;
}

/* create or retrieve already existing edge from parent_node -> child_node */
static int
__connect_graph_nodes(rte_node_t parent_node, rte_node_t child_node,
		    rte_edge_t *_edge, char *arc_name, int lineno)
{
	const char *next_node = NULL;
	rte_edge_t edge;

	if (!get_existing_edge(arc_name, parent_node, child_node, &edge)) {
		feat_dbg("\t%s/%d: %s[%u]: \"%s\", edge reused", arc_name, lineno,
			 rte_node_id_to_name(parent_node), edge, rte_node_id_to_name(child_node));

		if (_edge)
			*_edge = edge;

		return 0;
	}

	/* Node to be added */
	next_node = rte_node_id_to_name(child_node);

	edge = rte_node_edge_update(parent_node, RTE_EDGE_ID_INVALID, &next_node, 1);

	if (edge == RTE_EDGE_ID_INVALID) {
		graph_err("edge invalid");
		return -1;
	}
	edge = rte_node_edge_count(parent_node) - 1;

	feat_dbg("\t%s/%d: %s[%u]: \"%s\", new edge added", arc_name, lineno,
		 rte_node_id_to_name(parent_node), edge, rte_node_id_to_name(child_node));

	if (_edge)
		*_edge = edge;

	return 0;
}

/* feature arc initialization */
static int
feature_arc_main_init(rte_graph_feature_arc_main_t **pfl, uint32_t max_feature_arcs)
{
	rte_graph_feature_arc_main_t *pm = NULL;
	const struct rte_memzone *mz = NULL;
	uint32_t i;
	size_t sz;

	if (!pfl) {
		graph_err("Invalid input");
		return -1;
	}

	sz = sizeof(rte_graph_feature_arc_main_t) +
		(sizeof(pm->feature_arcs[0]) * max_feature_arcs);

	mz = rte_memzone_reserve(FEATURE_ARC_MEMZONE_NAME, sz, SOCKET_ID_ANY, 0);
	if (!mz) {
		graph_err("memzone reserve failed for feature arc main");
		return -1;
	}

	pm = mz->addr;
	memset(pm, 0, sz);

	pm->arc_mbuf_dyn_offset = -1;
	pm->arc_mbuf_dyn_offset = rte_mbuf_dynfield_register(&rte_graph_feature_arc_mbuf_desc);

	if (pm->arc_mbuf_dyn_offset < 0) {
		graph_err("rte_graph_feature_arc_dynfield_register failed");
		rte_memzone_free(mz);
		return -1;
	}
	for (i = 0; i < max_feature_arcs; i++)
		pm->feature_arcs[i] = GRAPH_FEATURE_ARC_PTR_INITIALIZER;

	pm->max_feature_arcs = max_feature_arcs;

	*pfl = pm;

	return 0;
}

/* feature arc initialization, public API */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_init, 25.07)
int
rte_graph_feature_arc_init(uint16_t num_feature_arcs)
{
	struct rte_graph_feature_arc_register *arc_reg = NULL;
	struct rte_graph_feature_register *feat_reg = NULL;
	const struct rte_memzone *mz = NULL;
	int max_feature_arcs;
	int rc = -1;

	if (!__rte_graph_feature_arc_main) {
		mz = rte_memzone_lookup(FEATURE_ARC_MEMZONE_NAME);
		if (mz) {
			__rte_graph_feature_arc_main = mz->addr;
			return 0;
		}
		max_feature_arcs = num_feature_arcs + arc_registration_num();
		if (!max_feature_arcs) {
			graph_err("No feature arcs registered");
			return -1;
		}
		rc = feature_arc_main_init(&__rte_graph_feature_arc_main, max_feature_arcs);
		if (rc < 0)
			return rc;
	}

	STAILQ_FOREACH(arc_reg, &feature_arc_list, next_arc) {
		if (arc_registration_validate(arc_reg, __func__, __LINE__, true) < 0)
			continue;

		/* arc lookup validates feature and arc both*/
		if (!arc_registration_lookup(arc_reg->arc_name, NULL, false))
			continue;

		/* If feature name not set, use node name as feature */
		if (!arc_reg->end_feature->feature_name)
			arc_reg->end_feature->feature_name =
				rte_node_id_to_name(arc_reg->end_feature->feature_node_id);

		/* Compute number of max_features if not provided */
		if (!arc_reg->max_features)
			arc_registered_features_num(arc_reg->arc_name, &arc_reg->max_features);

		rc = arc_max_index_get(arc_reg->arc_name, &arc_reg->max_indexes);
		if (rc < 0) {
			graph_err("arc_max_index_get failed for arc: %s",
				  arc_reg->arc_name);
			continue;
		}

		arc_reg->end_feature->feature_node_id = arc_reg->end_feature->feature_node->id;

		rc = rte_graph_feature_arc_create(arc_reg, NULL);

		if (rc < 0)
			goto arc_cleanup;
	}

	/* First add those features which has no runs_after and runs_before restriction */
	STAILQ_FOREACH(feat_reg, &feature_list, next_feature) {
		/* Skip if arc not registered yet */
		if (!arc_registration_lookup(feat_reg->arc_name, NULL, false))
			continue;

		if (feat_reg->runs_after || feat_reg->runs_before)
			continue;

		if (feature_registration_validate(feat_reg, __func__, __LINE__, 1, 0, false) < 0)
			continue;

		feat_reg->feature_node_id = feat_reg->feature_node->id;

		rc = rte_graph_feature_add(feat_reg);

		if (rc < 0)
			goto arc_cleanup;
	}
	/* Add those features which has either runs_after or runs_before restrictions */
	STAILQ_FOREACH(feat_reg, &feature_list, next_feature) {
		/* Skip if arc not registered yet */
		if (!arc_registration_lookup(feat_reg->arc_name, NULL, false))
			continue;

		if (!feat_reg->runs_after && !feat_reg->runs_before)
			continue;

		if (feat_reg->runs_after && feat_reg->runs_before)
			continue;

		if (feature_registration_validate(feat_reg, __func__, __LINE__, 1, 0, false) < 0)
			continue;

		feat_reg->feature_node_id = feat_reg->feature_node->id;

		rc = rte_graph_feature_add(feat_reg);

		if (rc < 0)
			goto arc_cleanup;
	}
	/* Add those features with both runs_after and runs_before restrictions */
	STAILQ_FOREACH(feat_reg, &feature_list, next_feature) {
		/* Skip if arc not registered yet */
		if (!arc_registration_lookup(feat_reg->arc_name, NULL, false))
			continue;

		if (!feat_reg->runs_after && !feat_reg->runs_before)
			continue;

		if ((feat_reg->runs_after && !feat_reg->runs_before) ||
		    (!feat_reg->runs_after && feat_reg->runs_before))
			continue;

		if (feature_registration_validate(feat_reg, __func__, __LINE__, 1, 0, false) < 0)
			continue;

		feat_reg->feature_node_id = feat_reg->feature_node->id;

		rc = rte_graph_feature_add(feat_reg);

		if (rc < 0)
			goto arc_cleanup;
	}

	return 0;

arc_cleanup:
	rte_graph_feature_arc_cleanup();

	return rc;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_create, 25.07)
int
rte_graph_feature_arc_create(struct rte_graph_feature_arc_register *reg,
			     rte_graph_feature_arc_t *_arc)
{
	rte_graph_feature_arc_main_t *dfm = NULL;
	struct rte_graph_feature_arc *arc = NULL;
	uint16_t first_feat_off, fdata_off;
	const struct rte_memzone *mz = NULL;
	uint16_t iter, arc_index, num_index;
	uint32_t feat_sz = 0;
	size_t sz;

	if (arc_registration_validate(reg, __func__, __LINE__, true) < 0)
		return -1;

	if (!reg->end_feature ||
	    (feature_registration_validate(reg->end_feature, __func__, __LINE__, 0, 1, true) < 0))
		return -1;

	if (!reg->max_features)
		graph_err("Zero features found for arc \"%s\" create",
			  reg->arc_name);

	if (!__rte_graph_feature_arc_main) {
		graph_err("Call to rte_graph_feature_arc_init() API missing");
		return -1;
	}

	/* See if arc memory is already created */
	mz = rte_memzone_lookup(reg->arc_name);
	if (mz) {
		graph_err("Feature arc %s already created", reg->arc_name);
		arc = mz->addr;
		return -1;
	}

	dfm = __rte_graph_feature_arc_main;

	/* threshold check */
	if (dfm->num_feature_arcs > (dfm->max_feature_arcs - 1))
		SET_ERR_JMP(EAGAIN, arc_create_err,
			    "%s: max number (%u) of feature arcs reached",
			    reg->arc_name, dfm->max_feature_arcs);

	/* Find the free slot for feature arc */
	for (iter = 0; iter < dfm->max_feature_arcs; iter++) {
		if (dfm->feature_arcs[iter] == GRAPH_FEATURE_ARC_PTR_INITIALIZER)
			break;
	}
	arc_index = iter;

	if (arc_index >= dfm->max_feature_arcs) {
		graph_err("No free slot found for num_feature_arc");
		return -1;
	}

	/* This should not happen */
	if (dfm->feature_arcs[arc_index] != GRAPH_FEATURE_ARC_PTR_INITIALIZER) {
		graph_err("Free arc_index: %u is not found free: %p",
			  arc_index, (void *)dfm->feature_arcs[arc_index]);
		return -1;
	}

	/* Calculate size of feature arc */
	feature_arc_reg_calc_size(reg, &sz, &first_feat_off, &fdata_off, &feat_sz, &num_index);

	mz = rte_memzone_reserve(reg->arc_name, sz, SOCKET_ID_ANY, 0);

	if (!mz) {
		graph_err("memzone reserve failed for arc: %s of size: %"PRIu64,
			  reg->arc_name, (uint64_t)sz);
		return -1;
	}

	arc = mz->addr;

	memset(arc, 0, sz);

	arc->feature_bit_mask_by_index = rte_malloc(reg->arc_name,
						    sizeof(uint64_t) * num_index, 0);

	if (!arc->feature_bit_mask_by_index) {
		graph_err("%s: rte_malloc failed for feature_bit_mask_alloc", reg->arc_name);
		goto mz_free;
	}

	memset(arc->feature_bit_mask_by_index, 0, sizeof(uint64_t) * num_index);

	/* override process function with start_node */
	if (node_override_process_func(reg->start_node->id, reg->start_node_feature_process_fn)) {
		graph_err("node_override_process_func failed for %s", reg->start_node->name);
		goto feat_bitmask_free;
	}
	feat_dbg("arc-%s: node-%s process() overridden with %p",
		  reg->arc_name, reg->start_node->name,
		  reg->start_node_feature_process_fn);

	/* Initialize rte_graph port group fixed variables */
	STAILQ_INIT(&arc->all_features);
	rte_strscpy(arc->feature_arc_name, reg->arc_name, RTE_GRAPH_FEATURE_ARC_NAMELEN - 1);
	arc->feature_arc_main = (void *)dfm;
	arc->start_node = reg->start_node;
	memcpy(&arc->end_feature, reg->end_feature, sizeof(arc->end_feature));
	arc->arc_start_process = reg->start_node_feature_process_fn;
	arc->feature_arc_index = arc_index;
	arc->arc_size = sz;

	/* reset fast path arc variables */
	arc->max_features = reg->max_features;
	arc->max_indexes = num_index;
	arc->fp_first_feature_offset = first_feat_off;
	arc->fp_feature_data_offset = fdata_off;
	arc->feature_size = feat_sz;
	arc->mbuf_dyn_offset = dfm->arc_mbuf_dyn_offset;

	feature_arc_data_reset(arc);

	dfm->feature_arcs[arc->feature_arc_index] = (uintptr_t)arc;
	dfm->num_feature_arcs++;

	if (rte_graph_feature_add(reg->end_feature) < 0)
		goto arc_destroy;

	if (_arc)
		*_arc = (rte_graph_feature_arc_t)arc_index;

	feat_dbg("Feature arc %s[%p] created with max_features: %u and indexes: %u",
		 arc->feature_arc_name, (void *)arc, arc->max_features, arc->max_indexes);

	return 0;

arc_destroy:
	rte_graph_feature_arc_destroy(arc_index);
feat_bitmask_free:
	rte_free(arc->feature_bit_mask_by_index);
mz_free:
	rte_memzone_free(mz);
arc_create_err:
	return -1;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_add, 25.07)
int
rte_graph_feature_add(struct rte_graph_feature_register *freg)
{
	struct rte_graph_feature_node_list *after_finfo = NULL, *before_finfo = NULL;
	struct rte_graph_feature_node_list *temp = NULL, *finfo = NULL;
	char feature_name[3 * RTE_GRAPH_FEATURE_ARC_NAMELEN];
	const char *runs_after = NULL, *runs_before = NULL;
	struct rte_graph_feature_arc *arc = NULL;
	uint32_t slot = UINT32_MAX, add_flag;
	rte_graph_feature_arc_t _arc;
	uint32_t num_features = 0;
	const char *nodename = NULL;
	rte_edge_t edge = -1;
	int rc = 0;

	if (feature_registration_validate(freg, __func__, __LINE__, 0, 1, true) < 0)
		return -1;

	/* arc is valid */
	if (rte_graph_feature_arc_lookup_by_name(freg->arc_name, &_arc)) {
		graph_err("%s_add: feature arc %s not found",
			  freg->feature_name, freg->arc_name);
		return -1;
	}

	if (feature_arc_sanity(_arc)) {
		graph_err("invalid feature arc: 0x%x", _arc);
		return -1;
	}

	arc = rte_graph_feature_arc_get(_arc);

	if (arc->runtime_enabled_features) {
		graph_err("adding features after enabling any one of them is not supported");
		return -1;
	}

	/* When application calls rte_graph_feature_add() directly*/
	if (freg->feature_node_id == RTE_NODE_ID_INVALID) {
		graph_err("%s/%s: Invalid feature_node_id set for %s",
			  freg->arc_name, freg->feature_name, __func__);
		return -1;
	}

	if ((freg->runs_after != NULL) && (freg->runs_before != NULL) &&
	    (freg->runs_after == freg->runs_before)) {
		graph_err("runs_after and runs_before cannot be same '%s:%s]", freg->runs_after,
			  freg->runs_before);
		return -1;
	}

	num_features = rte_graph_feature_arc_num_features(_arc);
	if (num_features) {
		nodeinfo_lkup_by_index(arc, num_features - 1, &temp, 0);
		/* Check if feature is not added after end_feature */
		if ((freg->runs_after != NULL) &&
		    (strncmp(freg->runs_after, temp->feature_name,
			     RTE_GRAPH_FEATURE_ARC_NAMELEN) == 0)) {
			graph_err("Feature %s cannot be added after end_feature %s",
				  freg->feature_name, freg->runs_after);
			return -1;
		}
	}

	if (!nodeinfo_add_lookup(arc, freg->feature_name, &finfo, &slot)) {
		graph_err("%s/%s feature already added", arc->feature_arc_name, freg->feature_name);
		return -1;
	}

	if (slot >= arc->max_features) {
		graph_err("%s: Max features %u added to feature arc",
			  arc->feature_arc_name, slot);
		return -1;
	}

	if (freg->feature_node_id == arc->start_node->id) {
		graph_err("%s/%s: Feature node and start node are same %u",
			  freg->arc_name, freg->feature_name, freg->feature_node_id);
		return -1;
	}

	nodename = rte_node_id_to_name(freg->feature_node_id);

	feat_dbg("%s: adding feature node: %s at feature index: %u", arc->feature_arc_name,
		 nodename, slot);

	if (connect_graph_nodes(arc->start_node->id, freg->feature_node_id, &edge,
				arc->feature_arc_name)) {
		graph_err("unable to connect %s -> %s", arc->start_node->name, nodename);
		return -1;
	}

	snprintf(feature_name, sizeof(feature_name), "%s-%s-finfo",
		 arc->feature_arc_name, freg->feature_name);

	finfo = rte_malloc(feature_name, sizeof(*finfo), 0);
	if (!finfo) {
		graph_err("%s/%s: rte_malloc failed", arc->feature_arc_name, freg->feature_name);
		return -1;
	}

	memset(finfo, 0, sizeof(*finfo));

	rte_strscpy(finfo->feature_name, freg->feature_name, RTE_GRAPH_FEATURE_ARC_NAMELEN - 1);
	finfo->feature_arc = (void *)arc;
	finfo->feature_node_id = freg->feature_node_id;
	finfo->feature_node_process_fn = freg->feature_process_fn;
	finfo->edge_to_this_feature = RTE_EDGE_ID_INVALID;
	finfo->edge_to_last_feature = RTE_EDGE_ID_INVALID;
	finfo->notifier_cb = freg->notifier_cb;

	runs_before = freg->runs_before;
	runs_after = freg->runs_after;

	/*
	 * if no constraints given and provided feature is not the first feature,
	 * explicitly set "runs_before" as end_feature.
	 *
	 * Handles the case:
	 * arc_create(f1);
	 * add(f2, NULL, NULL);
	 */
	if (!runs_after && !runs_before && num_features)
		runs_before = rte_graph_feature_arc_feature_to_name(_arc, num_features - 1);

	/* Check for before and after constraints */
	if (runs_before) {
		/* runs_before sanity */
		if (nodeinfo_lkup_by_name(arc, runs_before, &before_finfo, NULL))
			SET_ERR_JMP(EINVAL, finfo_free,
				     "runs_before feature name: %s does not exist", runs_before);

		if (!before_finfo)
			SET_ERR_JMP(EINVAL, finfo_free,
				     "runs_before %s does not exist", runs_before);

		/*
		 * Starting from 0 to runs_before, continue connecting edges
		 */
		add_flag = 1;
		STAILQ_FOREACH(temp, &arc->all_features, next_feature) {
			if (!add_flag)
				/* Nodes after seeing "runs_before", finfo connects to temp*/
				connect_graph_nodes(finfo->feature_node_id, temp->feature_node_id,
						    NULL, arc->feature_arc_name);
			/*
			 * As soon as we see runs_before. stop adding edges
			 */
			if (!strncmp(temp->feature_name, runs_before, RTE_GRAPH_NAMESIZE)) {
				if (!connect_graph_nodes(finfo->feature_node_id,
							 temp->feature_node_id,
							 &edge, arc->feature_arc_name))
					add_flag = 0;
			}

			if (add_flag)
				/* Nodes before seeing "run_before" are connected to finfo */
				connect_graph_nodes(temp->feature_node_id, finfo->feature_node_id,
						    NULL, arc->feature_arc_name);
		}
	}

	if (runs_after) {
		if (nodeinfo_lkup_by_name(arc, runs_after, &after_finfo, NULL))
			SET_ERR_JMP(EINVAL, finfo_free,
				     "Invalid after feature_name %s", runs_after);

		if (!after_finfo)
			SET_ERR_JMP(EINVAL, finfo_free,
				     "runs_after %s does not exist", runs_after);

		/* Starting from runs_after to end continue connecting edges */
		add_flag = 0;
		STAILQ_FOREACH(temp, &arc->all_features, next_feature) {
			if (add_flag)
				/* We have already seen runs_after now */
				/* Add all features as next node to current feature*/
				connect_graph_nodes(finfo->feature_node_id, temp->feature_node_id,
						    NULL, arc->feature_arc_name);
			else
				/* Connect initial nodes to newly added node*/
				connect_graph_nodes(temp->feature_node_id, finfo->feature_node_id,
						    NULL, arc->feature_arc_name);

			/* as soon as we see runs_after. start adding edges
			 * from next iteration
			 */
			if (!strncmp(temp->feature_name, runs_after, RTE_GRAPH_NAMESIZE))
				add_flag = 1;
		}

		/* add feature next to runs_after */
		STAILQ_INSERT_AFTER(&arc->all_features, after_finfo, finfo, next_feature);
	} else {
		if (before_finfo) {
			/* add finfo before "before_finfo" element in the list */
			after_finfo = NULL;
			STAILQ_FOREACH(temp, &arc->all_features, next_feature) {
				if (before_finfo == temp) {
					if (after_finfo)
						STAILQ_INSERT_AFTER(&arc->all_features, after_finfo,
								    finfo, next_feature);
					else
						STAILQ_INSERT_HEAD(&arc->all_features, finfo,
								   next_feature);

					/* override node process fn */
					rc = node_override_process_func(finfo->feature_node_id,
									freg->feature_process_fn);

					if (rc < 0) {
						graph_err("node_override_process_func failed for %s",
							  freg->feature_name);
						goto finfo_free;
					}
					return 0;
				}
				after_finfo = temp;
			}
		} else {
			/* Very first feature just needs to be added to list */
			STAILQ_INSERT_TAIL(&arc->all_features, finfo, next_feature);
		}
	}
	/* override node_process_fn */
	rc = node_override_process_func(finfo->feature_node_id, freg->feature_process_fn);
	if (rc < 0) {
		graph_err("node_override_process_func failed for %s", freg->feature_name);
		goto finfo_free;
	}

	if (freg->feature_node)
		feat_dbg("arc-%s: feature %s node %s process() overridden with %p",
			  freg->arc_name, freg->feature_name, freg->feature_node->name,
			  freg->feature_process_fn);
	else
		feat_dbg("arc-%s: feature %s nodeid %u process() overriding with %p",
			  freg->arc_name, freg->feature_name,
			  freg->feature_node_id, freg->feature_process_fn);

	return 0;
finfo_free:
	rte_free(finfo);

	return -1;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_lookup, 25.07)
int
rte_graph_feature_lookup(rte_graph_feature_arc_t _arc, const char *feature_name,
			 rte_graph_feature_t *feat)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t slot;

	if (!arc)
		return -1;

	if (!nodeinfo_lkup_by_name(arc, feature_name, &finfo, &slot)) {
		*feat = (rte_graph_feature_t) slot;
		return 0;
	}

	return -1;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_destroy, 25.07)
int
rte_graph_feature_arc_destroy(rte_graph_feature_arc_t _arc)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	rte_graph_feature_arc_main_t *dm = __rte_graph_feature_arc_main;
	struct rte_graph_feature_node_list *node_info = NULL;
	int iter;

	if (!arc) {
		graph_err("Invalid feature arc: 0x%x", _arc);
		return -1;
	}

	while (!STAILQ_EMPTY(&arc->all_features)) {
		node_info = STAILQ_FIRST(&arc->all_features);
		STAILQ_REMOVE_HEAD(&arc->all_features, next_feature);
		/* Notify application */
		if (node_info->notifier_cb) {
			for (iter = 0; iter < arc->max_indexes; iter++) {
				/* If feature is not enabled on this index, skip */
				if (!(arc->feature_bit_mask_by_index[iter] &
				    RTE_BIT64(node_info->finfo_index)))
					continue;

				node_info->notifier_cb(arc->feature_arc_name,
						       node_info->feature_name,
						       node_info->feature_node_id,
						       iter, false /* disable */,
						       UINT16_MAX /* invalid cookie */);
			}
		}
		rte_free(node_info);
	}

	dm->feature_arcs[arc->feature_arc_index] = GRAPH_FEATURE_ARC_PTR_INITIALIZER;

	rte_free(arc->feature_data_by_index);

	rte_free(arc->feature_bit_mask_by_index);

	rte_memzone_free(rte_memzone_lookup(arc->feature_arc_name));

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_cleanup, 25.07)
int
rte_graph_feature_arc_cleanup(void)
{
	rte_graph_feature_arc_main_t *dm = __rte_graph_feature_arc_main;
	struct rte_graph_feature_arc *arc = NULL;
	uint32_t iter;

	if (!__rte_graph_feature_arc_main)
		return -1;

	for (iter = 0; iter < dm->max_feature_arcs; iter++) {
		arc = rte_graph_feature_arc_get(iter);

		if (!arc)
			continue;

		rte_graph_feature_arc_destroy(arc->feature_arc_index);
	}
	rte_memzone_free(rte_memzone_lookup(FEATURE_ARC_MEMZONE_NAME));
	__rte_graph_feature_arc_main = NULL;

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_lookup_by_name, 25.07)
int
rte_graph_feature_arc_lookup_by_name(const char *arc_name, rte_graph_feature_arc_t *_arc)
{
	struct rte_graph_feature_arc *arc = NULL;
	const struct rte_memzone *mz = NULL;
	rte_graph_feature_arc_main_t *dm;
	uint32_t iter;

	if (_arc)
		*_arc = RTE_GRAPH_FEATURE_ARC_INITIALIZER;

	if (!__rte_graph_feature_arc_main) {
		mz = rte_memzone_lookup(FEATURE_ARC_MEMZONE_NAME);
		if (mz)
			__rte_graph_feature_arc_main = mz->addr;
		else
			return -1;
	}

	dm = __rte_graph_feature_arc_main;

	for (iter = 0; iter < dm->max_feature_arcs; iter++) {
		arc = rte_graph_feature_arc_get(iter);
		if (!arc)
			continue;

		if ((strstr(arc->feature_arc_name, arc_name)) &&
		    (strlen(arc->feature_arc_name) == strlen(arc_name))) {
			if (_arc)
				*_arc = arc->feature_arc_index;
			return 0;
		}
	}

	return -1;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_num_features, 25.07)
uint32_t
rte_graph_feature_arc_num_features(rte_graph_feature_arc_t _arc)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t count = 0;

	if (!arc) {
		graph_err("Invalid feature arc: 0x%x", _arc);
		return 0;
	}

	STAILQ_FOREACH(finfo, &arc->all_features, next_feature)
		count++;

	return count;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_feature_to_name, 25.07)
char *
rte_graph_feature_arc_feature_to_name(rte_graph_feature_arc_t _arc, rte_graph_feature_t feat)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t slot = feat;

	if (!arc)
		return NULL;

	if (feat >= rte_graph_feature_arc_num_features(_arc)) {
		graph_err("%s: feature %u does not exist", arc->feature_arc_name, feat);
		return NULL;
	}
	if (!nodeinfo_lkup_by_index(arc, slot, &finfo, 0/* ignore sanity*/))
		return finfo->feature_name;

	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_feature_to_node, 25.07)
int
rte_graph_feature_arc_feature_to_node(rte_graph_feature_arc_t _arc, rte_graph_feature_t feat,
				      rte_node_t *node)
{
	struct rte_graph_feature_arc *arc = rte_graph_feature_arc_get(_arc);
	struct rte_graph_feature_node_list *finfo = NULL;
	uint32_t slot = feat;

	if (!arc)
		return -1;

	if (node)
		*node = RTE_NODE_ID_INVALID;

	if (feat >= rte_graph_feature_arc_num_features(_arc)) {
		graph_err("%s: feature %u does not exist", arc->feature_arc_name, feat);
		return -1;
	}
	if (!nodeinfo_lkup_by_index(arc, slot, &finfo, 0/* ignore sanity*/)) {
		if (node)
			*node = finfo->feature_node_id;
		return 0;
	}
	return -1;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_graph_feature_arc_register, 25.07)
void __rte_graph_feature_arc_register(struct rte_graph_feature_arc_register *reg,
				      const char *caller_name, int lineno)
{
	RTE_SET_USED(caller_name);
	RTE_SET_USED(lineno);
	/* Do not validate arc registration here but as part of rte_graph_feature_arc_init() */
	STAILQ_INSERT_TAIL(&feature_arc_list, reg, next_arc);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_graph_feature_register, 25.07)
void __rte_graph_feature_register(struct rte_graph_feature_register *reg,
				  const char *caller_name, int lineno)
{
	if (feature_registration_validate(reg, caller_name, lineno, 0, 0, true) < 0)
		return;

	/* Add to the feature_list*/
	STAILQ_INSERT_TAIL(&feature_list, reg, next_feature);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_graph_feature_arc_names_get, 25.07)
uint32_t
rte_graph_feature_arc_names_get(char *arc_names[])
{
	rte_graph_feature_arc_main_t *dm = __rte_graph_feature_arc_main;
	struct rte_graph_feature_arc *arc = NULL;
	uint32_t count, num_arcs;

	if (!__rte_graph_feature_arc_main)
		return 0;

	for (count = 0, num_arcs = 0; count < dm->max_feature_arcs; count++)
		if (dm->feature_arcs[count] != GRAPH_FEATURE_ARC_PTR_INITIALIZER)
			num_arcs++;

	if (!num_arcs)
		return 0;

	if (!arc_names)
		return sizeof(char *) * num_arcs;

	for (count = 0, num_arcs = 0; count < dm->max_feature_arcs; count++) {
		if (dm->feature_arcs[count] != GRAPH_FEATURE_ARC_PTR_INITIALIZER) {
			arc = rte_graph_feature_arc_get(count);
			arc_names[num_arcs] = arc->feature_arc_name;
			num_arcs++;
		}
	}
	return num_arcs;
}
