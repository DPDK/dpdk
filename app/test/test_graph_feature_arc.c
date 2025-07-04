/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell International Ltd.
 */

#include "test.h"

#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_errno.h>

#ifndef RTE_EXEC_ENV_WINDOWS
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <rte_random.h>
#include <rte_graph_feature_arc.h>
#include <rte_graph_feature_arc_worker.h>

#define MBUF_NUM 256
#define TEST_ARC1_NAME "arc1"
#define TEST_ARC2_NAME "arc2"
#define TEST_FAILED_ARC "failed_arc"
#define MAX_INDEXES 10
#define OVERRIDE_MAX_INDEX MBUF_NUM
#define MAX_FEATURES 5

#define dbg(...)

#define SOURCE1 "arc_source1_node"
#define INPUT_STATIC "arc_input_static_node"
#define OUTPUT_STATIC "arc_output_static_node"
#define PKT_FREE_STATIC "arc_pkt_free_static_node"
#define ARC1_FEATURE1 "arc1_feature1_node"
#define ARC1_FEATURE2 "arc1_feature2_node"
#define ARC2_FEATURE1 "arc2_feature1_node"
#define ARC2_FEATURE2 "arc2_feature2_node"
#define ARC2_FEATURE3 "arc2_feature3_node"
#define DUMMY1_STATIC "arc_dummy1_static_node"
#define DUMMY2_STATIC "arc_dummy2_static_node"

#define feature_cast(x) ((rte_graph_feature_t)(x))

/* (Node index, Node Name, feature app_cookie base */
#define FOREACH_TEST_NODE_ARC {			\
		R(0, SOURCE1, 64)		\
		R(1, INPUT_STATIC, 128)		\
		R(2, OUTPUT_STATIC, 256)	\
		R(3, PKT_FREE_STATIC, 512)	\
		R(4, ARC1_FEATURE1, 1024)	\
		R(5, ARC1_FEATURE2, 2048)	\
		R(6, ARC2_FEATURE1, 4096)	\
		R(7, ARC2_FEATURE2, 8192)	\
		R(8, ARC2_FEATURE3, 16384)	\
		R(9, DUMMY1_STATIC, 32768)	\
		R(10, DUMMY2_STATIC, 65536)	\
	}

/**
 * ARC1: Feature arc on ingress interface
 * ARC2: Feature arc on egress interface
 * XX_static: Static nodes
 * XX_featureX: Feature X on arc
 *
 *            -----> ARC1_FEATURE1
 *           |        |         |
 *           |        |         v
 *           |        |   ARC1_FEATURE2
 *           |        |         |
 *           |        v         v
 *  SOURCE1 ->-----> INPUT_STATIC --> OUTPUT_STATIC -----> PKT_FREE_STATIC
 *                                     |   |  |             ^      ^     ^
 *                                     |   |  |             |      |     |
 *                                     |   |   --> ARC2_FEATURE1   |     |
 *                                     |   |          ^     ^      |     |
 *                                     |   |          |     |      |     |
 *                                     |    ----------c-> ARC2_FEATURE2  |
 *                                     |              |     ^            |
 *                                     |              |     |            |
 *                                      ----------> ARC2_FEATURE3 -------
 */
const char *node_names_feature_arc[] = {
	SOURCE1, INPUT_STATIC, OUTPUT_STATIC, PKT_FREE_STATIC,
	ARC1_FEATURE1, ARC1_FEATURE2, ARC2_FEATURE1, ARC2_FEATURE2, ARC2_FEATURE3,
	DUMMY1_STATIC, DUMMY2_STATIC
};

const char *arc_names[] = {
	TEST_ARC1_NAME, TEST_ARC2_NAME,
};

const char *arc1_feature_seq[] = {
	ARC1_FEATURE1, ARC1_FEATURE2, INPUT_STATIC,
};

const char *arc2_feature_seq[] = {
	ARC2_FEATURE1, ARC2_FEATURE2, ARC2_FEATURE3, PKT_FREE_STATIC,
};

#define MAX_NODES  RTE_DIM(node_names_feature_arc)

typedef enum {
	START_NODE,
	INTERMEDIATE_FEATURE,
	END_FEATURE,
} fp_node_type_t;

static int fp_test_case_result_failed;
static const char *fp_error_node;
static uint64_t fp_end_feature_counters;
static uint64_t sp_enable_counters, sp_enable_cb_counters;
static uint64_t sp_disable_counters, sp_disable_cb_counters;

/* Function declarations */
static uint16_t
source1_fn(struct rte_graph *graph, struct rte_node *node,
	   void **objs, uint16_t nb_objs);
static uint16_t
input_fa_fn(struct rte_graph *graph, struct rte_node *node,
	    void **objs, uint16_t nb_objs);
static uint16_t
output_fa_fn(struct rte_graph *graph, struct rte_node *node,
	     void **objs, uint16_t nb_objs);
static uint16_t
pkt_free_fn(struct rte_graph *graph, struct rte_node *node,
	    void **objs, uint16_t nb_objs);
static uint16_t
pkt_free_fa_fn(struct rte_graph *graph, struct rte_node *node,
	       void **objs, uint16_t nb_objs);
static uint16_t
dummy1_fn(struct rte_graph *graph, struct rte_node *node,
	  void **objs, uint16_t nb_objs);
static uint16_t
dummy2_fn(struct rte_graph *graph, struct rte_node *node,
	  void **objs, uint16_t nb_objs);
static uint16_t
arc1_feature1_fn(struct rte_graph *graph, struct rte_node *node,
		 void **objs, uint16_t nb_objs);
static uint16_t
arc1_feature1_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs);
static uint16_t
arc1_feature2_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs);
static uint16_t
arc2_feature1_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs);
static uint16_t
arc2_feature2_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs);
static uint16_t
arc2_feature3_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs);
static int
common_arc1_init(const struct rte_graph *graph, struct rte_node *node);
static int
common_arc2_init(const struct rte_graph *graph, struct rte_node *node);
static uint16_t
common_process_fn(struct rte_graph *graph, struct rte_node *node,
		  void **objs, uint16_t nb_objs, uint16_t max_indexes,
		  uint16_t mbuf_arr_offset, fp_node_type_t type);

typedef struct test_node_priv {
	int stop_enqueue;

	/* rte_graph node id */
	uint32_t node_id;

	rte_graph_feature_arc_t arc;
} test_node_priv_t;

typedef struct {
	rte_graph_feature_t feature;
	uint16_t egress_interface;
	uint16_t ingress_interface;
} graph_dynfield_t;

#define MAX_FEATURE_ARCS 4
static rte_graph_feature_arc_t arcs[MAX_FEATURE_ARCS];
static struct rte_mbuf mbufs[MAX_NODES + 1][MBUF_NUM];
static void *mbuf_p[MAX_NODES + 1][MBUF_NUM];
static rte_graph_t graph_id = RTE_GRAPH_ID_INVALID;

static uint16_t
compute_unique_user_data(const char *parent, const char *child, uint32_t interface_index)
{
	uint16_t user_data = interface_index;

	RTE_SET_USED(parent);
#define R(idx, node, node_cookie) {				\
		if (!strcmp(child, node)) {			\
			user_data += node_cookie + idx;		\
		}						\
	}

	FOREACH_TEST_NODE_ARC
#undef R

	return user_data;
}

static  void update_sp_counters(const char *arc_name, const char *feature_name,
				rte_node_t feature_node_id, uint32_t index,
				bool enable_disable, uint16_t app_cookie)

{
	RTE_SET_USED(feature_node_id);

	if (app_cookie != UINT16_MAX) {
		if (app_cookie != compute_unique_user_data(arc_name, feature_name, index)) {
			printf("cb():%s/%s/%u: app_cookie mismatch %u != %u\n",
			       arc_name, feature_name, index, app_cookie,
			       compute_unique_user_data(arc_name, feature_name, index));
			sp_enable_counters = UINT16_MAX;
			sp_disable_counters = UINT16_MAX;
		}
	}
	if (enable_disable)
		sp_enable_cb_counters++;
	else
		sp_disable_cb_counters++;
}

static int
get_edge(struct rte_node_register *parent_node,
	 struct rte_node_register *child_node, rte_edge_t *_edge)
{
	char **next_edges = NULL;
	uint32_t count, i;

	count = rte_node_edge_get(parent_node->id, NULL);

	if (!count)
		return -1;

	next_edges = malloc(count);

	if (!next_edges)
		return -1;

	count = rte_node_edge_get(parent_node->id, next_edges);
	for (i = 0; i < count; i++) {
		if (strstr(child_node->name, next_edges[i])) {
			if (_edge)
				*_edge = (rte_edge_t)i;

			free(next_edges);
			return 0;
		}
	}
	free(next_edges);

	return -1;
}

int
common_arc1_init(const struct rte_graph *graph, struct rte_node *node)
{
	test_node_priv_t *priv = (test_node_priv_t *)node->ctx;

	RTE_SET_USED(graph);

	priv->node_id = node->id;
	priv->stop_enqueue = 0;
	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC1_NAME, &priv->arc) < 0)
		return -1;

	return 0;
}

int
common_arc2_init(const struct rte_graph *graph, struct rte_node *node)
{
	test_node_priv_t *priv = (test_node_priv_t *)node->ctx;

	RTE_SET_USED(graph);

	priv->node_id = node->id;
	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC2_NAME, &priv->arc) < 0)
		return -1;

	return 0;
}

uint16_t
common_process_fn(struct rte_graph *graph, struct rte_node *node,
		  void **objs, uint16_t nb_objs, uint16_t max_indexes,
		  uint16_t mbuf_arr_offset,
		  fp_node_type_t ndtype)
{
	test_node_priv_t *priv = (test_node_priv_t *)node->ctx;
	struct rte_graph_feature_arc *arc =
		rte_graph_feature_arc_get(priv->arc);
	struct rte_graph_feature_arc_mbuf_dynfields *mbfields = NULL;
	struct rte_mbuf *bufs[MBUF_NUM];
	rte_edge_t edges[MBUF_NUM];
	uint32_t i, idx;

	RTE_SET_USED(objs);
	if (priv->stop_enqueue)
		return 0;

	if (ndtype == START_NODE)
		nb_objs = MBUF_NUM;

	for (i = 0; i < nb_objs; i++) {
		switch (ndtype) {
		case START_NODE:
			edges[i] = UINT16_MAX;
			bufs[i] = mbuf_p[mbuf_arr_offset][i];
			break;
		case INTERMEDIATE_FEATURE:
			edges[i] = UINT16_MAX;
			bufs[i] = (struct rte_mbuf *)objs[i];
			break;
		default:
			edges[i] = 0;
			bufs[i] = (struct rte_mbuf *)objs[i];
			break;
		}
		mbfields = rte_graph_feature_arc_mbuf_dynfields_get(bufs[i], arc->mbuf_dyn_offset);
		if (ndtype == START_NODE) {
			idx = i % max_indexes;
			if (!rte_graph_feature_data_first_feature_get(arc, idx,
								      &mbfields->feature_data,
								      &edges[i])) {
				fp_test_case_result_failed = 1;
				fp_error_node = node->name;
				priv->stop_enqueue = 1;
				return i;
			}
		} else if (ndtype == INTERMEDIATE_FEATURE) {
			if (!rte_graph_feature_data_next_feature_get(arc,
								     &mbfields->feature_data,
								     &edges[i])) {
				fp_test_case_result_failed = 1;
				fp_error_node = node->name;
				return i;
			}
		} else {
			/* Send pkts from arc1_end to arc2_start*/
			if (mbuf_arr_offset == 0)
				edges[i] = 0;
			fp_end_feature_counters++;
		}
		if (edges[i] == UINT16_MAX)
			RTE_VERIFY(0);
	}
	if (ndtype == START_NODE)
		priv->stop_enqueue = 1;

	if (!fp_test_case_result_failed && (!mbuf_arr_offset || (ndtype != END_FEATURE)))
		rte_node_enqueue_next(graph, node, edges, (void **)bufs, nb_objs);

	return nb_objs;
}

uint16_t
source1_fn(struct rte_graph *graph, struct rte_node *node,
	   void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs, MAX_INDEXES, 0, START_NODE);
}

static struct rte_node_register node_source1 = {
	.name = SOURCE1,
	.process = source1_fn,
	.flags = RTE_NODE_SOURCE_F,
	.nb_edges = 3,
	.init = common_arc1_init,
	.next_nodes = {INPUT_STATIC, DUMMY1_STATIC, DUMMY2_STATIC},
};
RTE_NODE_REGISTER(node_source1);

uint16_t
input_fa_fn(struct rte_graph *graph, struct rte_node *node,
		void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs, MAX_INDEXES, 0, END_FEATURE);
}

static struct rte_node_register node_input = {
	.name = INPUT_STATIC,
	.process = input_fa_fn,
	.nb_edges = 2,
	.init = common_arc1_init,
	.next_nodes = {OUTPUT_STATIC, DUMMY1_STATIC},
};
RTE_NODE_REGISTER(node_input);

struct rte_graph_feature_register arc1_end_feature = {
	.feature_name = INPUT_STATIC,
	.arc_name = TEST_ARC1_NAME,
	.feature_process_fn = input_fa_fn,
	.feature_node = &node_input,
	.notifier_cb = update_sp_counters,
};

static struct rte_graph_feature_arc_register arc1_input = {
	.arc_name = TEST_ARC1_NAME,
	.max_indexes = MAX_INDEXES,
	.max_features = MAX_FEATURES,
	.start_node = &node_source1,
	.start_node_feature_process_fn = source1_fn,
	.end_feature = &arc1_end_feature,
};

uint16_t
output_fa_fn(struct rte_graph *graph, struct rte_node *node,
	     void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 OVERRIDE_MAX_INDEX, 1, START_NODE);
}

static struct rte_node_register node_output = {
	.name = OUTPUT_STATIC,
	.process = output_fa_fn,
	.nb_edges = 3,
	.init = common_arc2_init,
	.next_nodes = {DUMMY1_STATIC, PKT_FREE_STATIC, DUMMY2_STATIC},
};
RTE_NODE_REGISTER(node_output);

uint16_t
pkt_free_fn(struct rte_graph *graph, struct rte_node *node,
		void **objs, uint16_t nb_objs)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	fp_test_case_result_failed = 1;
	fp_error_node = node->name;
	return 0;
}

uint16_t
pkt_free_fa_fn(struct rte_graph *graph, struct rte_node *node,
	       void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 OVERRIDE_MAX_INDEX, 1, END_FEATURE);
}

static struct rte_node_register node_pkt_free = {
	.name = PKT_FREE_STATIC,
	.process = pkt_free_fn,
	.nb_edges = 1,
	.init = common_arc2_init,
	.next_nodes = {DUMMY1_STATIC},
};
RTE_NODE_REGISTER(node_pkt_free);

static uint16_t override_arc2_index(void)
{
	return OVERRIDE_MAX_INDEX;
}

struct rte_graph_feature_register arc2_end_feature = {
	.feature_name = PKT_FREE_STATIC,
	.arc_name = TEST_ARC2_NAME,
	.feature_process_fn = pkt_free_fa_fn,
	.feature_node = &node_pkt_free,
	.notifier_cb = update_sp_counters,
};

static struct rte_graph_feature_arc_register arc2_output = {
	.arc_name = TEST_ARC2_NAME,
	.max_indexes = 1, /* Let feature override with bigger value */
	/* max_features not provided. End feature is a feature hence should work*/
	.start_node = &node_output,
	.start_node_feature_process_fn = output_fa_fn,
	.end_feature = &arc2_end_feature,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(arc2_output);

/** Dummy nodes */
uint16_t
dummy1_fn(struct rte_graph *graph, struct rte_node *node,
	  void **objs, uint16_t nb_objs)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);
	return 0;
}

static struct rte_node_register dummy1 = {
	.name = DUMMY1_STATIC,
	.process = dummy1_fn,
	.nb_edges = 0,
	.init = common_arc2_init,
};
RTE_NODE_REGISTER(dummy1);

uint16_t
dummy2_fn(struct rte_graph *graph, struct rte_node *node,
	  void **objs, uint16_t nb_objs)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);
	return 0;
}

static struct rte_node_register dummy2 = {
	.name = DUMMY2_STATIC,
	.process = dummy2_fn,
	.nb_edges = 5,
	.init = common_arc2_init,
	.next_nodes = { ARC1_FEATURE1, ARC1_FEATURE2, ARC2_FEATURE1,
			ARC2_FEATURE2, ARC2_FEATURE3},
};
RTE_NODE_REGISTER(dummy2);

/** Failed registrations */

struct rte_graph_feature_register sink_valid_feature = {
	.feature_name = "sink_valid_feature",
	.arc_name = TEST_FAILED_ARC,
	.feature_process_fn = dummy2_fn,
	.feature_node = &dummy2,
};

struct rte_graph_feature_register sink_mismatch_arc_name = {
	.feature_name = "sink_mismatch_arc_name",
	.arc_name = "mismatch_arc_name",
	.feature_process_fn = dummy2_fn,
	.feature_node = &dummy2,
};

struct rte_graph_feature_register sink_no_feature_node = {
	.feature_name = "sink_no_feature_node",
	.arc_name = TEST_FAILED_ARC,
	.feature_process_fn = dummy2_fn,
};

struct rte_graph_feature_register sink_no_feature_fn = {
	.feature_name = "sink_no_feature_fn",
	.arc_name = TEST_FAILED_ARC,
	.feature_node = &dummy2,
};

/* failed_arc1
 *  - max_features not provided
 *  - end_feature not provided
 */
static struct rte_graph_feature_arc_register failed_arc1 = {
	.arc_name = TEST_FAILED_ARC,
	.max_indexes = 1, /* Let features override it*/
	/* max_features field is missing */
	/* end_feature missing */
	.start_node = &dummy1,
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc1);

/* failed_arc2
 * - max_indexes not provided
 */
static struct rte_graph_feature_arc_register failed_arc2 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_valid_feature,
	/* max_index not provided and neither override_index_cb() in any feature */
	.start_node = &dummy1,
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc2);

/* failed_arc3
 * - start_node not provided
 */
static struct rte_graph_feature_arc_register failed_arc3 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_valid_feature,
	.max_indexes = 1,
	/* start_node not provided */
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc3)

/* failed_arc4
 * - start_node not provided
 */
static struct rte_graph_feature_arc_register failed_arc4 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_valid_feature,
	.max_indexes = 5,
	.start_node = &dummy1,
	/* start_node fn not provided */
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc4);

/* failed_arc5
 * - end_feature has mismatch arc_name
 */
static struct rte_graph_feature_arc_register failed_arc5 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_mismatch_arc_name,
	.max_indexes = 5,
	.start_node = &dummy1,
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc5);

/* failed_arc6
 * - end_feature does not have feature_node
 */
static struct rte_graph_feature_arc_register failed_arc6 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_no_feature_node,
	.max_indexes = 5,
	.start_node = &dummy1,
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc6);

/* failed_arc7
 * - end_feature does not have feature_fn
 */
static struct rte_graph_feature_arc_register failed_arc7 = {
	.arc_name = TEST_FAILED_ARC,
	.end_feature = &sink_no_feature_fn,
	.max_indexes = 5,
	.start_node = &dummy1,
	.start_node_feature_process_fn = dummy1_fn,
};
RTE_GRAPH_FEATURE_ARC_REGISTER(failed_arc7);

uint16_t
arc1_feature1_fn(struct rte_graph *graph, struct rte_node *node,
		 void **objs, uint16_t nb_objs)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	/* Feature on an arc cannot be called if feature arc is disabled */
	fp_test_case_result_failed = 1;
	fp_error_node = node->name;
	return 0;
}

uint16_t
arc1_feature1_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 MAX_INDEXES, 0, INTERMEDIATE_FEATURE);
}

static struct rte_node_register arc1_feature1 = {
	.name = ARC1_FEATURE1,
	.process = arc1_feature1_fn,
	.nb_edges = 0,
	.init = common_arc1_init,
};
RTE_NODE_REGISTER(arc1_feature1);

static struct rte_graph_feature_register feat_arc1_feature1 = {
	.feature_name = ARC1_FEATURE1,
	.arc_name = TEST_ARC1_NAME,
	.feature_process_fn = arc1_feature1_fa_fn,
	.feature_node = &arc1_feature1,
	.override_index_cb = override_arc2_index,
	.notifier_cb = update_sp_counters,
};
RTE_GRAPH_FEATURE_REGISTER(feat_arc1_feature1);

uint16_t
arc1_feature2_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 MAX_INDEXES, 0, INTERMEDIATE_FEATURE);
}

static struct rte_node_register arc1_feature2 = {
	.name = ARC1_FEATURE2,
	.process = arc1_feature2_fa_fn,
	.nb_edges = 0,
	.init = common_arc1_init,
};
RTE_NODE_REGISTER(arc1_feature2);

/* add arc1_feature2 later using rte_graph_feature_add()*/
static struct rte_graph_feature_register feat_arc1_feature2 = {
	.feature_name = ARC1_FEATURE2,
	.arc_name = TEST_ARC1_NAME,
	.feature_process_fn = arc1_feature2_fa_fn,
	.feature_node = &arc1_feature2,
	.runs_after = ARC1_FEATURE1,
	.notifier_cb = update_sp_counters,
};

uint16_t
arc2_feature1_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 OVERRIDE_MAX_INDEX, 1, INTERMEDIATE_FEATURE);
}

static struct rte_node_register arc2_feature1 = {
	.name = ARC2_FEATURE1,
	.process = arc2_feature1_fa_fn,
	.nb_edges = 0,
	.init = common_arc2_init,
};
RTE_NODE_REGISTER(arc2_feature1);

static struct rte_graph_feature_register feat_arc2_feature1 = {
	.feature_name = ARC2_FEATURE1,
	.arc_name = TEST_ARC2_NAME,
	.feature_process_fn = arc2_feature1_fa_fn,
	.feature_node = &arc2_feature1,
	.notifier_cb = update_sp_counters,
};
RTE_GRAPH_FEATURE_REGISTER(feat_arc2_feature1);

uint16_t
arc2_feature2_fa_fn(struct rte_graph *graph, struct rte_node *node,
		    void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 OVERRIDE_MAX_INDEX, 1, INTERMEDIATE_FEATURE);
}

static struct rte_node_register arc2_feature2 = {
	.name = ARC2_FEATURE2,
	.process = arc2_feature2_fa_fn,
	.nb_edges = 0,
	.init = common_arc2_init,
};
RTE_NODE_REGISTER(arc2_feature2);

static struct rte_graph_feature_register feat_arc2_feature2 = {
	.feature_name = ARC2_FEATURE2,
	.arc_name = TEST_ARC2_NAME,
	.feature_process_fn = arc2_feature2_fa_fn,
	.feature_node = &arc2_feature2,
	.runs_after = ARC2_FEATURE1,
	.runs_before = ARC2_FEATURE3,
	.override_index_cb = override_arc2_index,
	.notifier_cb = update_sp_counters,
};
RTE_GRAPH_FEATURE_REGISTER(feat_arc2_feature2);

uint16_t
arc2_feature3_fa_fn(struct rte_graph *graph, struct rte_node *node,
		 void **objs, uint16_t nb_objs)
{
	return common_process_fn(graph, node, objs, nb_objs,
				 OVERRIDE_MAX_INDEX, 1, INTERMEDIATE_FEATURE);
}

static struct rte_node_register arc2_feature3 = {
	.name = ARC2_FEATURE3,
	.process = arc2_feature3_fa_fn,
	.nb_edges = 0,
	.init = common_arc2_init,
};
RTE_NODE_REGISTER(arc2_feature3);

static struct rte_graph_feature_register feat_arc2_feature3 = {
	.feature_name = ARC2_FEATURE3,
	.arc_name = TEST_ARC2_NAME,
	.feature_process_fn = arc2_feature3_fa_fn,
	.feature_node = &arc2_feature3,
	.runs_after = ARC2_FEATURE1,
	.notifier_cb = update_sp_counters,
};
RTE_GRAPH_FEATURE_REGISTER(feat_arc2_feature3);

static int
create_graph(void)
{
	struct rte_graph_param gconf = {
		.socket_id = SOCKET_ID_ANY,
		.nb_node_patterns = RTE_DIM(node_names_feature_arc),
		.node_patterns = node_names_feature_arc,
	};

	graph_id = rte_graph_create("worker0", &gconf);
	if (graph_id == RTE_GRAPH_ID_INVALID) {
		printf("Graph creation failed with error = %d\n", rte_errno);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
destroy_graph(void)
{
	if (graph_id != RTE_GRAPH_ID_INVALID) {
		rte_graph_destroy(graph_id);
		graph_id = RTE_GRAPH_ID_INVALID;
	}

	return TEST_SUCCESS;
}

static int test_perf(uint64_t end_feature_counters)
{
	struct rte_graph *graph = NULL;
	int i;

	fp_test_case_result_failed = 0;
	fp_error_node = NULL;
	fp_end_feature_counters = 0;

	if (create_graph() < 0) {
		printf("Graph creation failed while walk\n");
		return TEST_FAILED;
	}
	graph = rte_graph_lookup("worker0");
	if (graph) {
		for (i = 0; i < 10; i++)
			rte_graph_walk(graph);
	}
	if (destroy_graph() < 0) {
		printf("Graph destroy failed while walk\n");
		return TEST_FAILED;
	}
	if (fp_test_case_result_failed) {
		printf("Feature arc test failed for node %s\n", fp_error_node);
		return TEST_FAILED;
	}

	if (sp_enable_counters &&
	    (sp_enable_cb_counters != sp_enable_counters)) {
		printf("sp_enable_cb_counters mismatch %"PRIu64" != %"PRIu64"\n",
		       sp_enable_cb_counters, sp_enable_counters);
		return TEST_FAILED;
	}
	if (sp_disable_counters &&
	    (sp_disable_cb_counters != sp_disable_counters)) {
		printf("sp_disable_cb_counters mismatch %"PRIu64" != %"PRIu64"\n",
		       sp_disable_cb_counters, sp_disable_counters);
		return TEST_FAILED;
	}
	if (fp_end_feature_counters != end_feature_counters) {
		printf("end_bufs_counters mismatch %"PRIu64" != %"PRIu64"\n",
		       fp_end_feature_counters, end_feature_counters);
		return TEST_FAILED;
	}
	sp_enable_counters = 0;
	sp_enable_cb_counters = 0;
	sp_disable_counters = 0;
	sp_disable_cb_counters = 0;
	return TEST_SUCCESS;
}

static int
__test_create_feature_arc(rte_graph_feature_arc_t *arcs, uint16_t max_arcs)
{
	struct rte_graph_feature_arc_register reg;
	struct rte_graph_feature_register feat;
	rte_graph_feature_arc_t arc;
	const char *sample_arc_name = "sample_arc";
	char arc_name[256];
	int n_arcs;

	if (rte_graph_feature_arc_init(max_arcs) < 0) {
		printf("Feature arc creation failed for arc: %u\n", max_arcs);
		return TEST_FAILED;
	}

	feat.feature_name = DUMMY2_STATIC;
	feat.feature_process_fn = dummy2_fn;
	feat.feature_node = &dummy2;
	feat.feature_node_id = dummy2.id;
	feat.runs_after = NULL;
	feat.runs_before = NULL;

	reg.max_indexes = MAX_INDEXES;
	reg.max_features = 3;
	reg.start_node = &dummy1;
	reg.start_node_feature_process_fn = dummy1_fn;
	reg.end_feature = &feat;
	for (n_arcs = 0; n_arcs < max_arcs; n_arcs++) {
		snprintf(arc_name, sizeof(arc_name), "%s-%u", sample_arc_name, n_arcs);
		feat.arc_name = arc_name;
		reg.arc_name = arc_name;
		if (rte_graph_feature_arc_create(&reg, &arcs[n_arcs]) < 0) {
			printf("%s: Test failed for creating arc at index %u\n", arc_name, n_arcs);
			return TEST_FAILED;
		}
	}

	if (rte_graph_feature_arc_create(&arc2_output, NULL) == 0) {
		printf("Failure: Feature arc %s should have already been created\n",
		       arc2_output.arc_name);
		return TEST_FAILED;
	}
	/* Verify feature arc created more than max_arcs must fail */
	for (n_arcs = 0; n_arcs < max_arcs; n_arcs++) {
		snprintf(arc_name, sizeof(arc_name), "%s-%u", sample_arc_name, n_arcs);
		arc = RTE_GRAPH_FEATURE_ARC_INITIALIZER;
		if (rte_graph_feature_arc_lookup_by_name(arc_name, &arc) == 0) {
			if (arc != arcs[n_arcs]) {
				printf("%s: Feature arc lookup mismatch for arc [%u, exp: %u]\n",
				       arc_name, arc, arcs[n_arcs]);
				return TEST_FAILED;
			}
			dbg("Feature arc lookup %s succeeded\n", arc_name);
		} else {
			printf("Feature arc lookup %s failed after creation\n", arc_name);
			return TEST_FAILED;
		}
	}
	return TEST_SUCCESS;
}

static int
test_graph_feature_arc_create(void)
{
	int ret = 0;

	ret = __test_create_feature_arc(arcs, MAX_FEATURE_ARCS);
	if (ret) {
		printf("Feature arc creation test failed for %u number of arc create\n",
		       MAX_FEATURE_ARCS);
		goto test_failed;
	}
	if (rte_graph_feature_arc_lookup_by_name(TEST_FAILED_ARC, NULL) == 0) {
		printf("Feature arc %s should not have been created\n", TEST_FAILED_ARC);
		goto test_failed;
	}

	/* Cleanup to verify recreation */
	rte_graph_feature_arc_cleanup();

	dbg("Feature arc cleanup done\n");

	return TEST_SUCCESS;

test_failed:
	return TEST_FAILED;
}

static int
graph_feature_arc_setup(void)
{
	unsigned long i, j;

	for (i = 0; i <= MAX_NODES; i++) {
		for (j = 0; j < MBUF_NUM; j++)
			mbuf_p[i][j] = &mbufs[i][j];
	}

	return TEST_SUCCESS;

}
static int
test_graph_feature_arc_features_add(void)
{
	struct rte_graph_feature_arc *arc = NULL;
	rte_graph_feature_t temp;

	if (rte_graph_feature_arc_init(1) < 0) {
		printf("Feature arc creation failed just for adding features\n");
		return TEST_FAILED;
	}

	/* see if duplicates fails */
	if (rte_graph_feature_add(&feat_arc2_feature1) == 0) {
		printf("%s: Feature add failed for adding feature %s\n",
		       feat_arc2_feature1.arc_name, feat_arc2_feature1.feature_name);
		goto test_failed;
	}

	/* Create new arc1 */
	arc1_input.end_feature->feature_node_id = arc1_input.end_feature->feature_node->id;
	if (rte_graph_feature_arc_create(&arc1_input, &arcs[0]) < 0) {
		printf("%s: Feature arc creation failed\n", TEST_ARC2_NAME);
		goto test_failed;
	}

	/* Add features to arc1 */
	feat_arc1_feature1.feature_node_id = feat_arc1_feature1.feature_node->id;
	if (rte_graph_feature_add(&feat_arc1_feature1) < 0) {
		printf("%s: Feature add failed for adding feature %s\n",
		       feat_arc1_feature1.arc_name, feat_arc1_feature1.feature_name);
		goto test_failed;
	}

	feat_arc1_feature2.feature_node_id = feat_arc1_feature2.feature_node->id;
	if (rte_graph_feature_add(&feat_arc1_feature2) < 0) {
		printf("%s: Feature add failed for adding feature %s\n",
		       feat_arc1_feature2.arc_name, feat_arc1_feature2.feature_name);
	}

	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC2_NAME, &arcs[1]) < 0) {
		printf("%s: Feature arc lookup failed while adding features\n",
		       TEST_ARC2_NAME);
		goto test_failed;
	}

	arc = rte_graph_feature_arc_get(arcs[1]);
	if (!arc) {
		printf("%s: Feature arc ptr cannot be NULL\n",
		       TEST_ARC2_NAME);
		goto test_failed;
	}

	/* Make sure number of indexes are overridden by feature*/
	if (arc->max_indexes < OVERRIDE_MAX_INDEX) {
		printf("%s: Feature arc max_indexes mismatch %u < %u\n",
		       TEST_ARC2_NAME, arc->max_indexes, OVERRIDE_MAX_INDEX);
		goto test_failed;
	}

	/* arc1_feature1 must be first feature to arcs[0] */
	if (!strstr(ARC1_FEATURE1,
		    rte_graph_feature_arc_feature_to_name(arcs[0],
							  feature_cast(0)))) {
		printf("%s: %s is not the first feature instead %s\n",
		       TEST_ARC1_NAME, ARC1_FEATURE1,
		       rte_graph_feature_arc_feature_to_name(arcs[0], feature_cast(0)));
		goto test_failed;
	}

	/* arc1_feature2 must be second feature to arcs[0] */
	if (!strstr(ARC1_FEATURE2,
		    rte_graph_feature_arc_feature_to_name(arcs[0],
							  feature_cast(1)))) {
		printf("%s: %s is not the second feature instead %s\n",
		       TEST_ARC1_NAME, ARC1_FEATURE2,
		       rte_graph_feature_arc_feature_to_name(arcs[0], feature_cast(1)));
		goto test_failed;
	}

	/* Make sure INPUT_STATIC is the last feature in arcs[0] */
	temp = rte_graph_feature_arc_num_features(arcs[0]);
	if (!strstr(INPUT_STATIC,
		    rte_graph_feature_arc_feature_to_name(arcs[0],
							  temp - feature_cast(1)))) {
		printf("%s: %s is not the last feature instead %s\n",
		       TEST_ARC1_NAME, INPUT_STATIC,
		       rte_graph_feature_arc_feature_to_name(arcs[0],
							     temp - feature_cast(1)));
		goto test_failed;
	}

	/* arc2_feature1 must be first feature to arcs[1] */
	if (!strstr(ARC2_FEATURE1,
		    rte_graph_feature_arc_feature_to_name(arcs[1],
							  feature_cast(0)))) {
		printf("%s: %s is not the first feature instead %s\n",
		       TEST_ARC2_NAME, ARC2_FEATURE1,
		       rte_graph_feature_arc_feature_to_name(arcs[1], feature_cast(0)));
		goto test_failed;
	}

	/* arc2_feature2 must be second feature to arcs[1] */
	if (!strstr(ARC2_FEATURE2,
		    rte_graph_feature_arc_feature_to_name(arcs[1],
							  feature_cast(1)))) {
		printf("%s: %s is not the second feature instead %s\n",
		       TEST_ARC2_NAME, ARC2_FEATURE2,
		       rte_graph_feature_arc_feature_to_name(arcs[1], feature_cast(1)));
		goto test_failed;
	}

	/* arc2_feature3 must be third feature to arcs[1] */
	if (!strstr(ARC2_FEATURE3,
		    rte_graph_feature_arc_feature_to_name(arcs[1],
							  feature_cast(2)))) {
		printf("%s: %s is not the third feature instead %s\n",
		       TEST_ARC2_NAME, ARC2_FEATURE3,
		       rte_graph_feature_arc_feature_to_name(arcs[1], feature_cast(2)));
		goto test_failed;
	}

	/* Make sure PKT_FREE is the last feature in arcs[1] */
	temp = rte_graph_feature_arc_num_features(arcs[1]);
	if (!strstr(PKT_FREE_STATIC,
		    rte_graph_feature_arc_feature_to_name(arcs[1],
							  temp - feature_cast(1)))) {
		printf("%s: %s is not the last feature instead %s\n",
		       TEST_ARC2_NAME, PKT_FREE_STATIC,
		       rte_graph_feature_arc_feature_to_name(arcs[1],
							     temp - feature_cast(1)));
		goto test_failed;
	}

	/* Make sure feature is connected to end feature */
	if (get_edge(&arc2_feature1, &node_pkt_free, NULL)) {
		printf("%s: Edge not found between %s and %s\n",
		       TEST_ARC2_NAME, ARC2_FEATURE1, PKT_FREE_STATIC);
		goto test_failed;
	}

	if (create_graph() < 0) {
		printf("Graph creation failed while adding features\n");
		goto test_failed;
	}
	if (destroy_graph() < 0) {
		printf("Graph destroy failed while adding features\n");
		goto test_failed;
	}
	return TEST_SUCCESS;
test_failed:
	rte_graph_feature_arc_cleanup();
	return TEST_FAILED;
}

static int
test_graph_feature_arc_feature_enable(void)
{
	uint32_t  n_indexes, n_features, count = 0;
	struct rte_node_register *parent, *child;
	struct rte_graph_feature_arc *arc;
	rte_graph_feature_data_t fdata;
	char *feature_name = NULL;
	rte_edge_t edge1, edge2;
	uint16_t user_data;

	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC1_NAME, &arcs[0]) < 0) {
		printf("%s: Feature arc lookup failed while adding features\n",
		       TEST_ARC1_NAME);
		return TEST_FAILED;
	}

	arc = rte_graph_feature_arc_get(arcs[0]);

	if (rte_graph_feature_arc_is_any_feature_enabled(arc)) {
		printf("%s: Feature arc should not have any feature enabled by now\n",
		       TEST_ARC1_NAME);
		return TEST_FAILED;
	}

	if (rte_graph_feature_arc_num_enabled_features(arcs[0])) {
		printf("%s: Feature arc should not have any_feature() enabled by now\n",
		       TEST_ARC1_NAME);
		return TEST_FAILED;
	}
	/*
	 * On interface 0, enable feature 0,
	 * On interface 1, enable feature 1 and so on so forth
	 *
	 * later verify first feature on every interface index is unique
	 * and check [rte_edge, user_data] retrieved via fast path APIs
	 */
	for (n_indexes = 0; n_indexes < arc->max_indexes; n_indexes++) {
		n_features = n_indexes % RTE_DIM(arc1_feature_seq);
		feature_name = rte_graph_feature_arc_feature_to_name(arcs[0], n_features);
		if (strncmp(feature_name, arc1_feature_seq[n_features],
			    RTE_GRAPH_NAMESIZE)) {
			printf("%s: Feature(%u) seq mismatch [found: %s, exp: %s]\n",
			       TEST_ARC1_NAME, n_features, feature_name,
			       arc1_feature_seq[n_features]);
			return TEST_FAILED;
		}
		user_data = compute_unique_user_data(arc->start_node->name, feature_name,
						     n_indexes);
		/* negative test case. enable feature on invalid index */
		if (!n_indexes && (rte_graph_feature_enable(arcs[0], 65535, feature_name,
							    user_data, NULL) == 0)) {
			printf("%s: Feature %s should not be enabled on invalid index\n",
			       TEST_ARC1_NAME, feature_name);
			return TEST_FAILED;
		}
		if (rte_graph_feature_enable(arcs[0], n_indexes, feature_name,
					 user_data, NULL) < 0) {
			printf("%s: Feature enable failed for %s on index %u\n",
			       TEST_ARC1_NAME, feature_name, n_indexes);
			return TEST_FAILED;
		}
		sp_enable_counters++;
		/* has any feature should be valid */
		if (!rte_graph_feature_arc_is_any_feature_enabled(arc)) {
			printf("%s: Feature arc should have any_feature enabled by now\n",
			       TEST_ARC1_NAME);
			return TEST_FAILED;
		}
		if ((count + 1) != rte_graph_feature_arc_num_enabled_features(arcs[0])) {
			printf("%s: Number of enabled mismatches [found: %u, exp: %u]\n",
			       TEST_ARC1_NAME,
			       rte_graph_feature_arc_num_enabled_features(arcs[0]),
			       count + 1);
			return TEST_FAILED;
		}
		count++;
	}
	/* Negative test case */
	user_data = compute_unique_user_data(arc->start_node->name, ARC2_FEATURE1, 1);
	if (!rte_graph_feature_enable(arcs[0], 1 /* index */, ARC2_FEATURE1, user_data, NULL)) {
		printf("%s: Invalid feature %s is enabled on index 1\n",
		       TEST_ARC1_NAME, ARC2_FEATURE1);
		return TEST_FAILED;
	}
	/* Duplicate enable */
	if (!rte_graph_feature_enable(arcs[0], 1 /* index */, ARC1_FEATURE2, user_data, NULL)) {
		printf("%s: Duplicate feature %s shouldn't be enabled again on index 1\n",
		       TEST_ARC1_NAME, ARC1_FEATURE2);
		return TEST_FAILED;
	}
	for (n_indexes = 0; n_indexes < arc->max_indexes; n_indexes++) {
		if (!rte_graph_feature_data_first_feature_get(arc, n_indexes, &fdata, &edge2)) {
			printf("%s: FP first feature data get failed for index %u\n",
			       TEST_ARC1_NAME, n_indexes);
			return TEST_FAILED;
		}
		parent = arc->start_node;
		if (0 == (n_indexes % RTE_DIM(arc1_feature_seq)))
			child = &arc1_feature1;
		else if (1 == (n_indexes % RTE_DIM(arc1_feature_seq)))
			child = &arc1_feature2;
		else
			child = &node_input;

		if (get_edge(parent, child, &edge1)) {
			printf("%s: Edge not found between %s and %s\n",
			       TEST_ARC1_NAME, parent->name, child->name);
			return TEST_FAILED;
		}
		if (edge1 != edge2) {
			printf("%s: Edge mismatch for first feature on index %u [%u, exp: %u]\n",
			       TEST_ARC1_NAME, n_indexes, edge2, edge1);
			return TEST_FAILED;
		}
		if (compute_unique_user_data(parent->name, child->name, n_indexes) !=
		    rte_graph_feature_data_app_cookie_get(arc, fdata)) {
			printf("%s/idx: %u: Cookie mismatch for first feature [%u, exp: %u]\n",
			       TEST_ARC1_NAME, n_indexes,
			       rte_graph_feature_data_app_cookie_get(arc, fdata),
			       compute_unique_user_data(parent->name, child->name, n_indexes));
			return TEST_FAILED;
		}
	}
	arc = rte_graph_feature_arc_get(arcs[1]);
	for (n_indexes = 0; n_indexes < arc->max_indexes; n_indexes++) {
		n_features = n_indexes % RTE_DIM(arc2_feature_seq);
		feature_name = rte_graph_feature_arc_feature_to_name(arcs[1], n_features);
		if (strncmp(feature_name, arc2_feature_seq[n_features],
			    RTE_GRAPH_NAMESIZE)) {
			printf("%s: Feature(%u) seq mismatch [found: %s, exp: %s]\n",
			       arc->feature_arc_name, n_features, feature_name,
			       arc2_feature_seq[n_features]);
			return TEST_FAILED;
		}
		user_data = compute_unique_user_data(arc->start_node->name, feature_name,
						     n_indexes);
		if (rte_graph_feature_enable(arcs[1], n_indexes, feature_name,
					 user_data, NULL) < 0) {
			printf("%s: Feature enable failed for %s on index %u\n",
			       TEST_ARC2_NAME, feature_name, n_indexes);
			return TEST_FAILED;
		}
		sp_enable_counters++;
	}

	return test_perf((2 * MBUF_NUM));
}

static int
test_graph_feature_arc_feature_disable(void)
{
	struct rte_graph_feature_arc *arc;
	uint32_t  n_indexes, n_features;
	char *feature_name = NULL;

	arc = rte_graph_feature_arc_get(arcs[0]);
	for (n_indexes = 0; n_indexes < arc->max_indexes; n_indexes++) {
		n_features = n_indexes % RTE_DIM(arc1_feature_seq);
		feature_name = rte_graph_feature_arc_feature_to_name(arcs[0], n_features);
		if (rte_graph_feature_disable(arcs[0], n_indexes, feature_name, NULL)) {
			printf("%s: feature disable failed for %s on index %u\n",
			       arc->feature_arc_name, feature_name, n_indexes);
			return TEST_FAILED;
		}
		sp_disable_counters++;
	}

	arc = rte_graph_feature_arc_get(arcs[1]);
	for (n_indexes = 0; n_indexes < arc->max_indexes; n_indexes++) {
		n_features = n_indexes % RTE_DIM(arc2_feature_seq);
		feature_name = rte_graph_feature_arc_feature_to_name(arcs[1], n_features);
		if (rte_graph_feature_disable(arcs[1], n_indexes, feature_name, NULL)) {
			printf("%s: feature disable failed for %s on index %u\n",
			       arc->feature_arc_name, feature_name, n_indexes);
			return TEST_FAILED;
		}
		sp_disable_counters++;
	}
	/* All packets should exit */
	return test_perf((2 * MBUF_NUM));
}

static int
test_graph_feature_arc_destroy(void)
{
	rte_graph_feature_arc_t arc;

	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC1_NAME, &arc)) {
		printf("Feature arc lookup failed for %s\n", TEST_ARC1_NAME);
		return TEST_FAILED;
	}

	if (arc != arcs[0]) {
		printf("Feature arc lookup mismatch for %s [%u, exp: %u]\n",
		       TEST_ARC1_NAME, arc, arcs[0]);
		return TEST_FAILED;
	}

	if (rte_graph_feature_arc_destroy(arc)) {
		printf("Feature arc destroy failed for %s\n", TEST_ARC1_NAME);
		return TEST_FAILED;
	}

	if (rte_graph_feature_arc_lookup_by_name(TEST_ARC2_NAME, &arc)) {
		printf("Feature arc lookup success after destroy for %s\n", TEST_ARC2_NAME);
		return TEST_FAILED;
	}

	if (arc != arcs[1]) {
		printf("Feature arc lookup mismatch for %s [%u, exp: %u]\n",
		       TEST_ARC2_NAME, arc, arcs[1]);
		return TEST_FAILED;
	}
	if (rte_graph_feature_arc_destroy(arc)) {
		printf("Feature arc destroy failed for %s\n", TEST_ARC2_NAME);
		return TEST_FAILED;
	}
	return TEST_SUCCESS;
}

static void
graph_feature_arc_teardown(void)
{
	if (graph_id != RTE_GRAPH_ID_INVALID)
		rte_graph_destroy(graph_id);

	rte_graph_feature_arc_cleanup();
}

static struct unit_test_suite graph_feature_arc_testsuite = {
	.suite_name = "Graph Feature arc library test suite",
	.setup = graph_feature_arc_setup,
	.teardown = graph_feature_arc_teardown,
	.unit_test_cases = {
		TEST_CASE(test_graph_feature_arc_create),
		TEST_CASE(test_graph_feature_arc_features_add),
		TEST_CASE(test_graph_feature_arc_feature_enable),
		TEST_CASE(test_graph_feature_arc_feature_disable),
		TEST_CASE(test_graph_feature_arc_destroy),
		TEST_CASES_END(), /**< NULL terminate unit test array */
	},
};

static int
graph_feature_arc_autotest_fn(void)
{
	return unit_test_suite_runner(&graph_feature_arc_testsuite);
}

REGISTER_FAST_TEST(graph_feature_arc_autotest, true, true, graph_feature_arc_autotest_fn);

#endif /* !RTE_EXEC_ENV_WINDOWS */
