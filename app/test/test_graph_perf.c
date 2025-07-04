/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include "test.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_graph_perf_func(void)
{
	printf("graph_perf not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#define TEST_GRAPH_PERF_MZ	     "graph_perf_data"
#define TEST_GRAPH_SRC_NAME	     "test_graph_perf_source"
#define TEST_GRAPH_SRC_BRST_ONE_NAME "test_graph_perf_source_one"
#define TEST_GRAPH_WRK_NAME	     "test_graph_perf_worker"
#define TEST_GRAPH_SNK_NAME	     "test_graph_perf_sink"

#define SOURCES(map)	     RTE_DIM(map)
#define STAGES(map)	     RTE_DIM(map)
#define NODES_PER_STAGE(map) RTE_DIM(map[0])
#define SINKS(map)	     RTE_DIM(map[0])

#define MAX_EDGES_PER_NODE 7

struct test_node_data {
	uint8_t node_id;
	uint8_t is_sink;
	uint8_t next_nodes[MAX_EDGES_PER_NODE];
	uint8_t next_percentage[MAX_EDGES_PER_NODE];
};

struct test_graph_perf {
	uint16_t nb_nodes;
	rte_graph_t graph_id;
	struct test_node_data *node_data;
};

struct graph_lcore_data {
	uint8_t done;
	rte_graph_t graph_id;
};

static struct test_node_data *
graph_get_node_data(struct test_graph_perf *graph_data, rte_node_t id)
{
	struct test_node_data *node_data = NULL;
	int i;

	for (i = 0; i < graph_data->nb_nodes; i++)
		if (graph_data->node_data[i].node_id == id) {
			node_data = &graph_data->node_data[i];
			break;
		}

	return node_data;
}

static int
test_node_ctx_init(const struct rte_graph *graph, struct rte_node *node)
{
	struct test_graph_perf *graph_data;
	struct test_node_data *node_data;
	const struct rte_memzone *mz;
	rte_node_t nid = node->id;
	rte_edge_t edge = 0;
	int i;

	RTE_SET_USED(graph);

	mz = rte_memzone_lookup(TEST_GRAPH_PERF_MZ);
	if (mz == NULL)
		return -ENOMEM;
	graph_data = mz->addr;
	node_data = graph_get_node_data(graph_data, nid);
	node->ctx[0] = node->nb_edges;
	for (i = 0; i < node->nb_edges && !node_data->is_sink; i++, edge++) {
		node->ctx[i + 1] = edge;
		node->ctx[i + 9] = node_data->next_percentage[i];
	}

	return 0;
}

/* Source node function */
static uint16_t
test_perf_node_worker_source(struct rte_graph *graph, struct rte_node *node,
			     void **objs, uint16_t nb_objs)
{
	uint16_t count;
	int i;

	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	/* Create a proportional stream for every next */
	for (i = 0; i < node->ctx[0]; i++) {
		count = (node->ctx[i + 9] * RTE_GRAPH_BURST_SIZE) / 100;
		rte_node_next_stream_get(graph, node, node->ctx[i + 1], count);
		rte_node_next_stream_put(graph, node, node->ctx[i + 1], count);
	}

	return RTE_GRAPH_BURST_SIZE;
}

static struct rte_node_register test_graph_perf_source = {
	.name = TEST_GRAPH_SRC_NAME,
	.process = test_perf_node_worker_source,
	.flags = RTE_NODE_SOURCE_F,
	.init = test_node_ctx_init,
};

RTE_NODE_REGISTER(test_graph_perf_source);

static uint16_t
test_perf_node_worker_source_burst_one(struct rte_graph *graph,
				       struct rte_node *node, void **objs,
				       uint16_t nb_objs)
{
	uint16_t count;
	int i;

	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	/* Create a proportional stream for every next */
	for (i = 0; i < node->ctx[0]; i++) {
		count = (node->ctx[i + 9]) / 100;
		rte_node_next_stream_get(graph, node, node->ctx[i + 1], count);
		rte_node_next_stream_put(graph, node, node->ctx[i + 1], count);
	}

	return 1;
}

static struct rte_node_register test_graph_perf_source_burst_one = {
	.name = TEST_GRAPH_SRC_BRST_ONE_NAME,
	.process = test_perf_node_worker_source_burst_one,
	.flags = RTE_NODE_SOURCE_F,
	.init = test_node_ctx_init,
};

RTE_NODE_REGISTER(test_graph_perf_source_burst_one);

/* Worker node function */
static uint16_t
test_perf_node_worker(struct rte_graph *graph, struct rte_node *node,
		      void **objs, uint16_t nb_objs)
{
	uint16_t next = 0;
	uint16_t enq = 0;
	uint16_t count;
	int i;

	/* Move stream for single next node */
	if (node->ctx[0] == 1) {
		rte_node_next_stream_move(graph, node, node->ctx[1]);
		return nb_objs;
	}

	/* Enqueue objects to next nodes proportionally */
	for (i = 0; i < node->ctx[0]; i++) {
		next = node->ctx[i + 1];
		count = (node->ctx[i + 9] * nb_objs) / 100;
		enq += count;
		while (count) {
			switch (count & (4 - 1)) {
			case 0:
				rte_node_enqueue_x4(graph, node, next, objs[0],
						    objs[1], objs[2], objs[3]);
				objs += 4;
				count -= 4;
				break;
			case 1:
				rte_node_enqueue_x1(graph, node, next, objs[0]);
				objs += 1;
				count -= 1;
				break;
			case 2:
				rte_node_enqueue_x2(graph, node, next, objs[0],
						    objs[1]);
				objs += 2;
				count -= 2;
				break;
			case 3:
				rte_node_enqueue_x2(graph, node, next, objs[0],
						    objs[1]);
				rte_node_enqueue_x1(graph, node, next, objs[0]);
				objs += 3;
				count -= 3;
				break;
			}
		}
	}

	if (enq != nb_objs)
		rte_node_enqueue(graph, node, next, objs, nb_objs - enq);

	return nb_objs;
}

static struct rte_node_register test_graph_perf_worker = {
	.name = TEST_GRAPH_WRK_NAME,
	.process = test_perf_node_worker,
	.init = test_node_ctx_init,
};

RTE_NODE_REGISTER(test_graph_perf_worker);

/* Last node in graph a.k.a sink node */
static uint16_t
test_perf_node_sink(struct rte_graph *graph, struct rte_node *node, void **objs,
		    uint16_t nb_objs)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);
	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	return nb_objs;
}

static struct rte_node_register test_graph_perf_sink = {
	.name = TEST_GRAPH_SNK_NAME,
	.process = test_perf_node_sink,
	.init = test_node_ctx_init,
};

RTE_NODE_REGISTER(test_graph_perf_sink);

static int
graph_perf_setup(void)
{
	if (rte_lcore_count() < 2) {
		printf("Test requires at least 2 lcores\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static void
graph_perf_teardown(void)
{
}

static inline rte_node_t
graph_node_get(const char *pname, char *nname)
{
	rte_node_t pnode_id = rte_node_from_name(pname);
	char lookup_name[RTE_NODE_NAMESIZE];
	rte_node_t node_id;

	snprintf(lookup_name, RTE_NODE_NAMESIZE, "%s-%s", pname, nname);
	node_id = rte_node_from_name(lookup_name);

	if (node_id != RTE_NODE_ID_INVALID) {
		if (rte_node_edge_count(node_id))
			rte_node_edge_shrink(node_id, 0);
		return node_id;
	}

	return rte_node_clone(pnode_id, nname);
}

static uint16_t
graph_node_count_edges(uint32_t stage, uint16_t node, uint16_t nodes_per_stage,
		       uint8_t edge_map[][nodes_per_stage][nodes_per_stage],
		       char *ename[], struct test_node_data *node_data,
		       rte_node_t **node_map)
{
	uint8_t total_percent = 0;
	uint16_t edges = 0;
	int i;

	for (i = 0; i < nodes_per_stage && edges < MAX_EDGES_PER_NODE; i++) {
		if (edge_map[stage + 1][i][node]) {
			ename[edges] = malloc(sizeof(char) * RTE_NODE_NAMESIZE);
			snprintf(ename[edges], RTE_NODE_NAMESIZE, "%s",
				 rte_node_id_to_name(node_map[stage + 1][i]));
			node_data->next_nodes[edges] = node_map[stage + 1][i];
			node_data->next_percentage[edges] =
				edge_map[stage + 1][i][node];
			edges++;
			total_percent += edge_map[stage + 1][i][node];
		}
	}

	if (edges >= MAX_EDGES_PER_NODE || (edges && total_percent != 100)) {
		for (i = 0; i < edges; i++)
			free(ename[i]);
		return RTE_EDGE_ID_INVALID;
	}

	return edges;
}

static int
graph_init(const char *gname, uint8_t nb_srcs, uint8_t nb_sinks,
	   uint32_t stages, uint16_t nodes_per_stage,
	   uint8_t src_map[][nodes_per_stage], uint8_t snk_map[][nb_sinks],
	   uint8_t edge_map[][nodes_per_stage][nodes_per_stage],
	   uint8_t burst_one)
{
	struct test_graph_perf *graph_data;
	char nname[RTE_NODE_NAMESIZE / 2];
	struct test_node_data *node_data;
	char *ename[nodes_per_stage];
	struct rte_graph_param gconf = {0};
	const struct rte_memzone *mz;
	uint8_t total_percent = 0;
	rte_node_t *src_nodes;
	rte_node_t *snk_nodes;
	rte_node_t **node_map;
	char **node_patterns;
	rte_graph_t graph_id;
	rte_edge_t edges;
	rte_edge_t count;
	uint32_t i, j, k;

	mz = rte_memzone_reserve(TEST_GRAPH_PERF_MZ,
				 sizeof(struct test_graph_perf), 0, 0);
	if (mz == NULL) {
		printf("Failed to allocate graph common memory\n");
		return -ENOMEM;
	}

	graph_data = mz->addr;
	graph_data->nb_nodes = 0;
	graph_data->node_data =
		malloc(sizeof(struct test_node_data) *
		       (nb_srcs + nb_sinks + stages * nodes_per_stage));
	if (graph_data->node_data == NULL) {
		printf("Failed to reserve memzone for graph data\n");
		goto memzone_free;
	}

	node_patterns = malloc(sizeof(char *) *
			       (nb_srcs + nb_sinks + stages * nodes_per_stage));
	if (node_patterns == NULL) {
		printf("Failed to reserve memory for node patterns\n");
		goto data_free;
	}

	src_nodes = malloc(sizeof(rte_node_t) * nb_srcs);
	if (src_nodes == NULL) {
		printf("Failed to reserve memory for src nodes\n");
		goto pattern_free;
	}

	snk_nodes = malloc(sizeof(rte_node_t) * nb_sinks);
	if (snk_nodes == NULL) {
		printf("Failed to reserve memory for snk nodes\n");
		goto src_free;
	}

	node_map = malloc(sizeof(rte_node_t *) * stages +
			  sizeof(rte_node_t) * nodes_per_stage * stages);
	if (node_map == NULL) {
		printf("Failed to reserve memory for node map\n");
		goto snk_free;
	}

	/* Setup the Graph */
	for (i = 0; i < stages; i++) {
		node_map[i] =
			(rte_node_t *)(node_map + stages) + nodes_per_stage * i;
		for (j = 0; j < nodes_per_stage; j++) {
			total_percent = 0;
			for (k = 0; k < nodes_per_stage; k++)
				total_percent += edge_map[i][j][k];
			if (!total_percent)
				continue;
			node_patterns[graph_data->nb_nodes] =
				malloc(RTE_NODE_NAMESIZE);
			if (node_patterns[graph_data->nb_nodes] == NULL) {
				printf("Failed to create memory for pattern\n");
				goto pattern_name_free;
			}

			/* Clone a worker node */
			snprintf(nname, sizeof(nname), "%d-%d", i, j);
			node_map[i][j] =
				graph_node_get(TEST_GRAPH_WRK_NAME, nname);
			if (node_map[i][j] == RTE_NODE_ID_INVALID) {
				printf("Failed to create node[%s]\n", nname);
				graph_data->nb_nodes++;
				goto pattern_name_free;
			}
			snprintf(node_patterns[graph_data->nb_nodes],
				 RTE_NODE_NAMESIZE, "%s",
				 rte_node_id_to_name(node_map[i][j]));
			node_data =
				&graph_data->node_data[graph_data->nb_nodes];
			node_data->node_id = node_map[i][j];
			node_data->is_sink = false;
			graph_data->nb_nodes++;
		}
	}

	for (i = 0; i < stages - 1; i++) {
		for (j = 0; j < nodes_per_stage; j++) {
			/* Count edges i.e connections of worker node to next */
			node_data =
				graph_get_node_data(graph_data, node_map[i][j]);
			edges = graph_node_count_edges(i, j, nodes_per_stage,
						       edge_map, ename,
						       node_data, node_map);
			if (edges == RTE_EDGE_ID_INVALID) {
				printf("Invalid edge configuration\n");
				goto pattern_name_free;
			}
			if (!edges)
				continue;

			/* Connect a node in stage 'i' to nodes
			 * in stage 'i + 1' with edges.
			 */
			count = rte_node_edge_update(
				node_map[i][j], 0,
				(const char **)(uintptr_t)ename, edges);
			for (k = 0; k < edges; k++)
				free(ename[k]);
			if (count != edges) {
				printf("Couldn't add edges %d %d\n", edges,
				       count);
				goto pattern_name_free;
			}
		}
	}

	/* Setup Source nodes */
	for (i = 0; i < nb_srcs; i++) {
		edges = 0;
		total_percent = 0;
		node_patterns[graph_data->nb_nodes] = malloc(RTE_NODE_NAMESIZE);
		if (node_patterns[graph_data->nb_nodes] == NULL) {
			printf("Failed to create memory for pattern\n");
			goto pattern_name_free;
		}
		/* Clone a source node */
		snprintf(nname, sizeof(nname), "%d", i);
		src_nodes[i] =
			graph_node_get(burst_one ? TEST_GRAPH_SRC_BRST_ONE_NAME
						 : TEST_GRAPH_SRC_NAME,
				       nname);
		if (src_nodes[i] == RTE_NODE_ID_INVALID) {
			printf("Failed to create node[%s]\n", nname);
			graph_data->nb_nodes++;
			goto pattern_name_free;
		}
		snprintf(node_patterns[graph_data->nb_nodes], RTE_NODE_NAMESIZE,
			 "%s", rte_node_id_to_name(src_nodes[i]));
		node_data = &graph_data->node_data[graph_data->nb_nodes];
		node_data->node_id = src_nodes[i];
		node_data->is_sink = false;
		graph_data->nb_nodes++;

		/* Prepare next node list  to connect to */
		for (j = 0; j < nodes_per_stage; j++) {
			if (!src_map[i][j])
				continue;
			ename[edges] = malloc(sizeof(char) * RTE_NODE_NAMESIZE);
			snprintf(ename[edges], RTE_NODE_NAMESIZE, "%s",
				 rte_node_id_to_name(node_map[0][j]));
			node_data->next_nodes[edges] = node_map[0][j];
			node_data->next_percentage[edges] = src_map[i][j];
			edges++;
			total_percent += src_map[i][j];
		}

		if (!edges)
			continue;
		if (edges >= MAX_EDGES_PER_NODE || total_percent != 100) {
			printf("Invalid edge configuration\n");
			for (j = 0; j < edges; j++)
				free(ename[j]);
			goto pattern_name_free;
		}

		/* Connect to list of next nodes using edges */
		count = rte_node_edge_update(src_nodes[i], 0,
					     (const char **)(uintptr_t)ename,
					     edges);
		for (k = 0; k < edges; k++)
			free(ename[k]);
		if (count != edges) {
			printf("Couldn't add edges %d %d\n", edges, count);
			goto pattern_name_free;
		}
	}

	/* Setup Sink nodes */
	for (i = 0; i < nb_sinks; i++) {
		node_patterns[graph_data->nb_nodes] = malloc(RTE_NODE_NAMESIZE);
		if (node_patterns[graph_data->nb_nodes] == NULL) {
			printf("Failed to create memory for pattern\n");
			goto pattern_name_free;
		}

		/* Clone a sink node */
		snprintf(nname, sizeof(nname), "%d", i);
		snk_nodes[i] = graph_node_get(TEST_GRAPH_SNK_NAME, nname);
		if (snk_nodes[i] == RTE_NODE_ID_INVALID) {
			printf("Failed to create node[%s]\n", nname);
			graph_data->nb_nodes++;
			goto pattern_name_free;
		}
		snprintf(node_patterns[graph_data->nb_nodes], RTE_NODE_NAMESIZE,
			 "%s", rte_node_id_to_name(snk_nodes[i]));
		node_data = &graph_data->node_data[graph_data->nb_nodes];
		node_data->node_id = snk_nodes[i];
		node_data->is_sink = true;
		graph_data->nb_nodes++;
	}

	/* Connect last stage worker nodes to sink nodes */
	for (i = 0; i < nodes_per_stage; i++) {
		edges = 0;
		total_percent = 0;
		node_data = graph_get_node_data(graph_data,
						node_map[stages - 1][i]);
		/* Prepare list of sink nodes to connect to */
		for (j = 0; j < nb_sinks; j++) {
			if (!snk_map[i][j])
				continue;
			ename[edges] = malloc(sizeof(char) * RTE_NODE_NAMESIZE);
			snprintf(ename[edges], RTE_NODE_NAMESIZE, "%s",
				 rte_node_id_to_name(snk_nodes[j]));
			node_data->next_nodes[edges] = snk_nodes[j];
			node_data->next_percentage[edges] = snk_map[i][j];
			edges++;
			total_percent += snk_map[i][j];
		}
		if (!edges)
			continue;
		if (edges >= MAX_EDGES_PER_NODE || total_percent != 100) {
			printf("Invalid edge configuration\n");
			for (j = 0; j < edges; j++)
				free(ename[i]);
			goto pattern_name_free;
		}

		/* Connect a worker node to a list of sink nodes */
		count = rte_node_edge_update(node_map[stages - 1][i], 0,
					     (const char **)(uintptr_t)ename,
					     edges);
		for (k = 0; k < edges; k++)
			free(ename[k]);
		if (count != edges) {
			printf("Couldn't add edges %d %d\n", edges, count);
			goto pattern_name_free;
		}
	}

	/* Create a Graph */
	gconf.socket_id = SOCKET_ID_ANY;
	gconf.nb_node_patterns = graph_data->nb_nodes;
	gconf.node_patterns = (const char **)(uintptr_t)node_patterns;

	graph_id = rte_graph_create(gname, &gconf);
	if (graph_id == RTE_GRAPH_ID_INVALID) {
		printf("Graph creation failed with error = %d\n", rte_errno);
		goto pattern_name_free;
	}
	graph_data->graph_id = graph_id;

	free(node_map);
	for (i = 0; i < graph_data->nb_nodes; i++)
		free(node_patterns[i]);
	free(snk_nodes);
	free(src_nodes);
	free(node_patterns);
	return 0;

pattern_name_free:
	free(node_map);
	for (i = 0; i < graph_data->nb_nodes; i++)
		free(node_patterns[i]);
snk_free:
	free(snk_nodes);
src_free:
	free(src_nodes);
pattern_free:
	free(node_patterns);
data_free:
	free(graph_data->node_data);
memzone_free:
	rte_memzone_free(mz);
	return -ENOMEM;
}

/* Worker thread function */
static int
_graph_perf_wrapper(void *args)
{
	struct graph_lcore_data *data = args;
	struct rte_graph *graph;

	/* Lookup graph */
	graph = rte_graph_lookup(rte_graph_id_to_name(data->graph_id));

	/* Graph walk until done */
	while (!data->done)
		rte_graph_walk(graph);

	return 0;
}

static int
measure_perf_get(rte_graph_t graph_id)
{
	const char *pattern = rte_graph_id_to_name(graph_id);
	uint32_t lcore_id = rte_get_next_lcore(-1, 1, 0);
	struct rte_graph_cluster_stats_param param;
	struct rte_graph_cluster_stats *stats;
	struct graph_lcore_data *data;

	data = rte_zmalloc("Graph_perf", sizeof(struct graph_lcore_data),
			   RTE_CACHE_LINE_SIZE);
	data->graph_id = graph_id;
	data->done = 0;

	/* Run graph worker thread function */
	rte_eal_remote_launch(_graph_perf_wrapper, data, lcore_id);

	/* Collect stats for few msecs */
	if (rte_graph_has_stats_feature()) {
		memset(&param, 0, sizeof(param));
		param.f = stdout;
		param.socket_id = SOCKET_ID_ANY;
		param.graph_patterns = &pattern;
		param.nb_graph_patterns = 1;

		stats = rte_graph_cluster_stats_create(&param);
		if (stats == NULL) {
			printf("Failed to create stats\n");
			return -ENOMEM;
		}

		rte_delay_ms(3E2);
		rte_graph_cluster_stats_get(stats, true);
		rte_delay_ms(1E3);
		rte_graph_cluster_stats_get(stats, false);
		rte_graph_cluster_stats_destroy(stats);
	} else
		rte_delay_ms(1E3);

	data->done = 1;
	rte_eal_wait_lcore(lcore_id);

	return 0;
}

static inline void
graph_fini(void)
{
	const struct rte_memzone *mz = rte_memzone_lookup(TEST_GRAPH_PERF_MZ);
	struct test_graph_perf *graph_data;

	if (mz == NULL)
		return;
	graph_data = mz->addr;

	rte_graph_destroy(graph_data->graph_id);
	free(graph_data->node_data);
	rte_memzone_free(rte_memzone_lookup(TEST_GRAPH_PERF_MZ));
}

static int
measure_perf(void)
{
	const struct rte_memzone *mz;
	struct test_graph_perf *graph_data;

	mz = rte_memzone_lookup(TEST_GRAPH_PERF_MZ);
	if (mz == NULL)
		return -ENOMEM;
	graph_data = mz->addr;

	return measure_perf_get(graph_data->graph_id);
}

static inline int
graph_hr_4s_1n_1src_1snk(void)
{
	return measure_perf();
}

static inline int
graph_hr_4s_1n_1src_1snk_brst_one(void)
{
	return measure_perf();
}

static inline int
graph_hr_4s_1n_2src_1snk(void)
{
	return measure_perf();
}

static inline int
graph_hr_4s_1n_1src_2snk(void)
{
	return measure_perf();
}

static inline int
graph_tree_4s_4n_1src_4snk(void)
{
	return measure_perf();
}

static inline int
graph_reverse_tree_3s_4n_1src_1snk(void)
{
	return measure_perf();
}

static inline int
graph_parallel_tree_5s_4n_4src_4snk(void)
{
	return measure_perf();
}

/* Graph Topology
 * nodes per stage:	1
 * stages:		4
 * src:			1
 * sink:		1
 */
static inline int
graph_init_hr(void)
{
	uint8_t edge_map[][1][1] = {
		{ {100} },
		{ {100} },
		{ {100} },
		{ {100} },
	};
	uint8_t src_map[][1] = { {100} };
	uint8_t snk_map[][1] = { {100} };

	return graph_init("graph_hr", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/* Graph Topology
 * nodes per stage:	1
 * stages:		4
 * src:			1
 * sink:		1
 */
static inline int
graph_init_hr_brst_one(void)
{
	uint8_t edge_map[][1][1] = {
		{ {100} },
		{ {100} },
		{ {100} },
		{ {100} },
	};
	uint8_t src_map[][1] = { {100} };
	uint8_t snk_map[][1] = { {100} };

	return graph_init("graph_hr", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 1);
}

/* Graph Topology
 * nodes per stage:	1
 * stages:		4
 * src:			2
 * sink:		1
 */
static inline int
graph_init_hr_multi_src(void)
{
	uint8_t edge_map[][1][1] = {
		{ {100} },
		{ {100} },
		{ {100} },
		{ {100} },
	};
	uint8_t src_map[][1] = {
		{100}, {100}
	};
	uint8_t snk_map[][1] = { {100} };

	return graph_init("graph_hr", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/* Graph Topology
 * nodes per stage:	1
 * stages:		4
 * src:			1
 * sink:		2
 */
static inline int
graph_init_hr_multi_snk(void)
{
	uint8_t edge_map[][1][1] = {
		{ {100} },
		{ {100} },
		{ {100} },
		{ {100} },
	};
	uint8_t src_map[][1] = { {100} };
	uint8_t snk_map[][2] = { {50, 50} };

	return graph_init("graph_hr", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/* Graph Topology
 * nodes per stage:	4
 * stages:		4
 * src:			1
 * sink:		4
 */
static inline int
graph_init_tree(void)
{
	uint8_t edge_map[][4][4] = {
		{
			{100, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0}
		},
		{
			{50, 0, 0, 0},
			{50, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0}
		},
		{
			{33, 33, 0, 0},
			{34, 34, 0, 0},
			{33, 33, 0, 0},
			{0, 0, 0, 0}
		},
		{
			{25, 25, 25, 0},
			{25, 25, 25, 0},
			{25, 25, 25, 0},
			{25, 25, 25, 0}
		}
	};
	uint8_t src_map[][4] = { {100, 0, 0, 0} };
	uint8_t snk_map[][4] = {
		{100, 0, 0, 0},
		{0, 100, 0, 0},
		{0, 0, 100, 0},
		{0, 0, 0, 100}
	};

	return graph_init("graph_full_split", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/* Graph Topology
 * nodes per stage:	4
 * stages:		3
 * src:			1
 * sink:		1
 */
static inline int
graph_init_reverse_tree(void)
{
	uint8_t edge_map[][4][4] = {
		{
			{25, 25, 25, 25},
			{25, 25, 25, 25},
			{25, 25, 25, 25},
			{25, 25, 25, 25}
		},
		{
			{33, 33, 33, 33},
			{33, 33, 33, 33},
			{34, 34, 34, 34},
			{0, 0, 0, 0}
		},
		{
			{50, 50, 50, 0},
			{50, 50, 50, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0}
		},
	};
	uint8_t src_map[][4] = { {25, 25, 25, 25} };
	uint8_t snk_map[][1] = { {100}, {100}, {0}, {0} };

	return graph_init("graph_full_split", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/* Graph Topology
 * nodes per stage:	4
 * stages:		5
 * src:			4
 * sink:		4
 */
static inline int
graph_init_parallel_tree(void)
{
	uint8_t edge_map[][4][4] = {
		{
			{100, 0, 0, 0},
			{0, 100, 0, 0},
			{0, 0, 100, 0},
			{0, 0, 0, 100}
		},
		{
			{100, 0, 0, 0},
			{0, 100, 0, 0},
			{0, 0, 100, 0},
			{0, 0, 0, 100}
		},
		{
			{100, 0, 0, 0},
			{0, 100, 0, 0},
			{0, 0, 100, 0},
			{0, 0, 0, 100}
		},
		{
			{100, 0, 0, 0},
			{0, 100, 0, 0},
			{0, 0, 100, 0},
			{0, 0, 0, 100}
		},
		{
			{100, 0, 0, 0},
			{0, 100, 0, 0},
			{0, 0, 100, 0},
			{0, 0, 0, 100}
		},
	};
	uint8_t src_map[][4] = {
		{100, 0, 0, 0},
		{0, 100, 0, 0},
		{0, 0, 100, 0},
		{0, 0, 0, 100}
	};
	uint8_t snk_map[][4] = {
		{100, 0, 0, 0},
		{0, 100, 0, 0},
		{0, 0, 100, 0},
		{0, 0, 0, 100}
	};

	return graph_init("graph_parallel", SOURCES(src_map), SINKS(snk_map),
			  STAGES(edge_map), NODES_PER_STAGE(edge_map), src_map,
			  snk_map, edge_map, 0);
}

/** Graph Creation cheat sheet
 *  edge_map -> dictates graph flow from worker stage 0 to worker stage n-1.
 *  src_map  -> dictates source nodes enqueue percentage to worker stage 0.
 *  snk_map  -> dictates stage n-1 enqueue percentage to sink.
 *
 *  Layout:
 *  edge_map[<nb_stages>][<nodes_per_stg>][<nodes_in_nxt_stg = nodes_per_stg>]
 *  src_map[<nb_sources>][<nodes_in_stage0 = nodes_per_stage>]
 *  snk_map[<nodes_in_stage(n-1) = nodes_per_stage>][<nb_sinks>]
 *
 *  The last array dictates the percentage of received objs to enqueue to next
 *  stage.
 *
 *  Note: edge_map[][0][] will always be unused as it will receive from source
 *
 *  Example:
 *	Graph:
 *	http://bit.ly/2PqbqOy
 *	Each stage(n) connects to all nodes in the next stage in decreasing
 *	order.
 *	Since we can't resize the edge_map dynamically we get away by creating
 *	dummy nodes and assigning 0 percentages.
 *	Max nodes across all stages = 4
 *	stages = 3
 *	nb_src = 1
 *	nb_snk = 1
 *			   // Stages
 *	edge_map[][4][4] = {
 *		// Nodes per stage
 *		{
 *		    {25, 25, 25, 25},
 *		    {25, 25, 25, 25},
 *		    {25, 25, 25, 25},
 *		    {25, 25, 25, 25}
 *		},	// This will be unused.
 *		{
 *		    // Nodes enabled in current stage + prev stage enq %
 *		    {33, 33, 33, 33},
 *		    {33, 33, 33, 33},
 *		    {34, 34, 34, 34},
 *		    {0, 0, 0, 0}
 *		},
 *		{
 *		    {50, 50, 50, 0},
 *		    {50, 50, 50, 0},
 *		    {0, 0, 0, 0},
 *		    {0, 0, 0, 0}
 *		},
 *	};
 *	Above, each stage tells how much it should receive from previous except
 *	from stage_0.
 *
 *	src_map[][4] = { {25, 25, 25, 25} };
 *	Here, we tell each source the % it has to send to stage_0 nodes. In
 *	case we want 2 source node we can declare as
 *	src_map[][4] = { {25, 25, 25, 25}, {25, 25, 25, 25} };
 *
 *	snk_map[][1] = { {100}, {100}, {0}, {0} }
 *	Here, we tell stage - 1 nodes how much to enqueue to sink_0.
 *	If we have 2 sinks we can do as follows
 *	snk_map[][2] = { {50, 50}, {50, 50}, {0, 0}, {0, 0} }
 */

static struct unit_test_suite graph_perf_testsuite = {
	.suite_name = "Graph library performance test suite",
	.setup = graph_perf_setup,
	.teardown = graph_perf_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(graph_init_hr, graph_fini,
			     graph_hr_4s_1n_1src_1snk),
		TEST_CASE_ST(graph_init_hr_brst_one, graph_fini,
			     graph_hr_4s_1n_1src_1snk_brst_one),
		TEST_CASE_ST(graph_init_hr_multi_src, graph_fini,
			     graph_hr_4s_1n_2src_1snk),
		TEST_CASE_ST(graph_init_hr_multi_snk, graph_fini,
			     graph_hr_4s_1n_1src_2snk),
		TEST_CASE_ST(graph_init_tree, graph_fini,
			     graph_tree_4s_4n_1src_4snk),
		TEST_CASE_ST(graph_init_reverse_tree, graph_fini,
			     graph_reverse_tree_3s_4n_1src_1snk),
		TEST_CASE_ST(graph_init_parallel_tree, graph_fini,
			     graph_parallel_tree_5s_4n_4src_4snk),
		TEST_CASES_END(), /**< NULL terminate unit test array */
	},
};

static int
test_graph_perf_func(void)
{
	return unit_test_suite_runner(&graph_perf_testsuite);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_PERF_TEST(graph_perf_autotest, test_graph_perf_func);
