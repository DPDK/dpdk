/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include "test.h"

#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_errno.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_node_list_dump(void)
{
	printf("node_list_dump not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <rte_random.h>

static uint16_t test_node_worker_source(struct rte_graph *graph,
					struct rte_node *node, void **objs,
					uint16_t nb_objs);

static uint16_t test_node0_worker(struct rte_graph *graph,
				  struct rte_node *node, void **objs,
				  uint16_t nb_objs);

static uint16_t test_node1_worker(struct rte_graph *graph,
				  struct rte_node *node, void **objs,
				  uint16_t nb_objs);

static uint16_t test_node2_worker(struct rte_graph *graph,
				  struct rte_node *node, void **objs,
				  uint16_t nb_objs);

static uint16_t test_node3_worker(struct rte_graph *graph,
				  struct rte_node *node, void **objs,
				  uint16_t nb_objs);

#define MBUFF_SIZE 512
#define MAX_NODES  4

typedef uint64_t graph_dynfield_t;
static int graph_dynfield_offset = -1;

static inline graph_dynfield_t *
graph_field(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, \
			graph_dynfield_offset, graph_dynfield_t *);
}

static struct rte_mbuf mbuf[MAX_NODES + 1][MBUFF_SIZE];
static void *mbuf_p[MAX_NODES + 1][MBUFF_SIZE];
static rte_graph_t graph_id;
static uint64_t obj_stats[MAX_NODES + 1];
static uint64_t fn_calls[MAX_NODES + 1];

const char *node_patterns[] = {
	"test_node_source1",	   "test_node00",
	"test_node00-test_node11", "test_node00-test_node22",
	"test_node00-test_node33",
};

const char *node_names[] = {
	"test_node00",
	"test_node00-test_node11",
	"test_node00-test_node22",
	"test_node00-test_node33",
};

struct test_node_register {
	char name[RTE_NODE_NAMESIZE];
	rte_node_process_t process;
	uint16_t nb_edges;
	const char *next_nodes[MAX_NODES];
};

typedef struct {
	uint32_t idx;
	struct test_node_register node;
} test_node_t;

typedef struct {
	test_node_t test_node[MAX_NODES];
} test_main_t;

static test_main_t test_main = {
	.test_node = {
		{
			.node = {
					.name = "test_node00",
					.process = test_node0_worker,
					.nb_edges = 2,
					.next_nodes = {"test_node00-"
						       "test_node11",
						       "test_node00-"
						       "test_node22"},
				},
		},
		{
			.node = {
					.name = "test_node11",
					.process = test_node1_worker,
					.nb_edges = 1,
					.next_nodes = {"test_node00-"
						       "test_node22"},
				},
		},
		{
			.node = {
					.name = "test_node22",
					.process = test_node2_worker,
					.nb_edges = 1,
					.next_nodes = {"test_node00-"
						       "test_node33"},
				},
		},
		{
			.node = {
					.name = "test_node33",
					.process = test_node3_worker,
					.nb_edges = 1,
					.next_nodes = {"test_node00"},
				},
		},
	},
};

static int
node_init(const struct rte_graph *graph, struct rte_node *node)
{
	RTE_SET_USED(graph);
	*(uint32_t *)node->ctx = node->id;

	return 0;
}

static struct rte_node_register test_node_source = {
	.name = "test_node_source1",
	.process = test_node_worker_source,
	.flags = RTE_NODE_SOURCE_F,
	.nb_edges = 2,
	.init = node_init,
	.next_nodes = {"test_node00", "test_node00-test_node11"},
};
RTE_NODE_REGISTER(test_node_source);

static struct rte_node_register test_node0 = {
	.name = "test_node00",
	.process = test_node0_worker,
	.init = node_init,
};
RTE_NODE_REGISTER(test_node0);

uint16_t
test_node_worker_source(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	uint32_t obj_node0 = rte_rand() % 100, obj_node1;
	test_main_t *tm = &test_main;
	struct rte_mbuf *data;
	void **next_stream;
	rte_node_t next;
	uint32_t i;

	RTE_SET_USED(objs);
	nb_objs = RTE_GRAPH_BURST_SIZE;

	/* Prepare stream for next node 0 */
	obj_node0 = nb_objs * obj_node0 * 0.01;
	next = 0;
	next_stream = rte_node_next_stream_get(graph, node, next, obj_node0);
	for (i = 0; i < obj_node0; i++) {
		data = &mbuf[0][i];
		*graph_field(data) = ((uint64_t)tm->test_node[0].idx << 32) | i;
		if ((i + 1) == obj_node0)
			*graph_field(data) |= (1 << 16);
		next_stream[i] = &mbuf[0][i];
	}
	rte_node_next_stream_put(graph, node, next, obj_node0);

	/* Prepare stream for next node 1 */
	obj_node1 = nb_objs - obj_node0;
	next = 1;
	next_stream = rte_node_next_stream_get(graph, node, next, obj_node1);
	for (i = 0; i < obj_node1; i++) {
		data = &mbuf[0][obj_node0 + i];
		*graph_field(data) = ((uint64_t)tm->test_node[1].idx << 32) | i;
		if ((i + 1) == obj_node1)
			*graph_field(data) |= (1 << 16);
		next_stream[i] = &mbuf[0][obj_node0 + i];
	}

	rte_node_next_stream_put(graph, node, next, obj_node1);
	obj_stats[0] += nb_objs;
	fn_calls[0] += 1;
	return nb_objs;
}

uint16_t
test_node0_worker(struct rte_graph *graph, struct rte_node *node, void **objs,
		  uint16_t nb_objs)
{
	test_main_t *tm = &test_main;

	if (*(uint32_t *)node->ctx == test_node0.id) {
		uint32_t obj_node0 = rte_rand() % 100, obj_node1;
		struct rte_mbuf *data;
		uint8_t second_pass = 0;
		uint32_t count = 0;
		uint32_t i;

		obj_stats[1] += nb_objs;
		fn_calls[1] += 1;

		for (i = 0; i < nb_objs; i++) {
			data = (struct rte_mbuf *)objs[i];
			if ((*graph_field(data) >> 32) != tm->test_node[0].idx) {
				printf("Data idx miss match at node 0, expected"
				       " = %u got = %u\n",
				       tm->test_node[0].idx,
				       (uint32_t)(*graph_field(data) >> 32));
				goto end;
			}

			if ((*graph_field(data) & 0xffff) != (i - count)) {
				printf("Expected buff count miss match at "
				       "node 0\n");
				goto end;
			}

			if (*graph_field(data) & (0x1 << 16))
				count = i + 1;
			if (*graph_field(data) & (0x1 << 17))
				second_pass = 1;
		}

		if (count != i) {
			printf("Count mismatch at node 0\n");
			goto end;
		}

		obj_node0 = nb_objs * obj_node0 * 0.01;
		for (i = 0; i < obj_node0; i++) {
			data = &mbuf[1][i];
			*graph_field(data) =
				((uint64_t)tm->test_node[1].idx << 32) | i;
			if ((i + 1) == obj_node0)
				*graph_field(data) |= (1 << 16);
			if (second_pass)
				*graph_field(data) |= (1 << 17);
		}
		rte_node_enqueue(graph, node, 0, (void **)&mbuf_p[1][0],
				 obj_node0);

		obj_node1 = nb_objs - obj_node0;
		for (i = 0; i < obj_node1; i++) {
			data = &mbuf[1][obj_node0 + i];
			*graph_field(data) =
				((uint64_t)tm->test_node[2].idx << 32) | i;
			if ((i + 1) == obj_node1)
				*graph_field(data) |= (1 << 16);
			if (second_pass)
				*graph_field(data) |= (1 << 17);
		}
		rte_node_enqueue(graph, node, 1, (void **)&mbuf_p[1][obj_node0],
				 obj_node1);

	} else if (*(uint32_t *)node->ctx == tm->test_node[1].idx) {
		test_node1_worker(graph, node, objs, nb_objs);
	} else if (*(uint32_t *)node->ctx == tm->test_node[2].idx) {
		test_node2_worker(graph, node, objs, nb_objs);
	} else if (*(uint32_t *)node->ctx == tm->test_node[3].idx) {
		test_node3_worker(graph, node, objs, nb_objs);
	} else {
		printf("Unexpected node context\n");
	}

end:
	return nb_objs;
}

uint16_t
test_node1_worker(struct rte_graph *graph, struct rte_node *node, void **objs,
		  uint16_t nb_objs)
{
	test_main_t *tm = &test_main;
	uint8_t second_pass = 0;
	uint32_t obj_node0 = 0;
	struct rte_mbuf *data;
	uint32_t count = 0;
	uint32_t i;

	obj_stats[2] += nb_objs;
	fn_calls[2] += 1;
	for (i = 0; i < nb_objs; i++) {
		data = (struct rte_mbuf *)objs[i];
		if ((*graph_field(data) >> 32) != tm->test_node[1].idx) {
			printf("Data idx miss match at node 1, expected = %u"
			       " got = %u\n",
			       tm->test_node[1].idx,
			       (uint32_t)(*graph_field(data) >> 32));
			goto end;
		}

		if ((*graph_field(data) & 0xffff) != (i - count)) {
			printf("Expected buff count miss match at node 1\n");
			goto end;
		}

		if (*graph_field(data) & (0x1 << 16))
			count = i + 1;
		if (*graph_field(data) & (0x1 << 17))
			second_pass = 1;
	}

	if (count != i) {
		printf("Count mismatch at node 1\n");
		goto end;
	}

	obj_node0 = nb_objs;
	for (i = 0; i < obj_node0; i++) {
		data = &mbuf[2][i];
		*graph_field(data) = ((uint64_t)tm->test_node[2].idx << 32) | i;
		if ((i + 1) == obj_node0)
			*graph_field(data) |= (1 << 16);
		if (second_pass)
			*graph_field(data) |= (1 << 17);
	}
	rte_node_enqueue(graph, node, 0, (void **)&mbuf_p[2][0], obj_node0);

end:
	return nb_objs;
}

uint16_t
test_node2_worker(struct rte_graph *graph, struct rte_node *node, void **objs,
		  uint16_t nb_objs)
{
	test_main_t *tm = &test_main;
	uint8_t second_pass = 0;
	struct rte_mbuf *data;
	uint32_t count = 0;
	uint32_t obj_node0;
	uint32_t i;

	obj_stats[3] += nb_objs;
	fn_calls[3] += 1;
	for (i = 0; i < nb_objs; i++) {
		data = (struct rte_mbuf *)objs[i];
		if ((*graph_field(data) >> 32) != tm->test_node[2].idx) {
			printf("Data idx miss match at node 2, expected = %u"
			       " got = %u\n",
			       tm->test_node[2].idx,
			       (uint32_t)(*graph_field(data) >> 32));
			goto end;
		}

		if ((*graph_field(data) & 0xffff) != (i - count)) {
			printf("Expected buff count miss match at node 2\n");
			goto end;
		}

		if (*graph_field(data) & (0x1 << 16))
			count = i + 1;
		if (*graph_field(data) & (0x1 << 17))
			second_pass = 1;
	}

	if (count != i) {
		printf("Count mismatch at node 2\n");
		goto end;
	}

	if (!second_pass) {
		obj_node0 = nb_objs;
		for (i = 0; i < obj_node0; i++) {
			data = &mbuf[3][i];
			*graph_field(data) =
				((uint64_t)tm->test_node[3].idx << 32) | i;
			if ((i + 1) == obj_node0)
				*graph_field(data) |= (1 << 16);
		}
		rte_node_enqueue(graph, node, 0, (void **)&mbuf_p[3][0],
				 obj_node0);
	}

end:
	return nb_objs;
}

uint16_t
test_node3_worker(struct rte_graph *graph, struct rte_node *node, void **objs,
		  uint16_t nb_objs)
{
	test_main_t *tm = &test_main;
	uint8_t second_pass = 0;
	struct rte_mbuf *data;
	uint32_t count = 0;
	uint32_t obj_node0;
	uint32_t i;

	obj_stats[4] += nb_objs;
	fn_calls[4] += 1;
	for (i = 0; i < nb_objs; i++) {
		data = (struct rte_mbuf *)objs[i];
		if ((*graph_field(data) >> 32) != tm->test_node[3].idx) {
			printf("Data idx miss match at node 3, expected = %u"
			       " got = %u\n",
			       tm->test_node[3].idx,
			       (uint32_t)(*graph_field(data) >> 32));
			goto end;
		}

		if ((*graph_field(data) & 0xffff) != (i - count)) {
			printf("Expected buff count miss match at node 3\n");
			goto end;
		}

		if (*graph_field(data) & (0x1 << 16))
			count = i + 1;
		if (*graph_field(data) & (0x1 << 17))
			second_pass = 1;
	}

	if (count != i) {
		printf("Count mismatch at node 3\n");
		goto end;
	}

	if (second_pass) {
		printf("Unexpected buffers are at node 3\n");
		goto end;
	} else {
		obj_node0 = nb_objs * 2;
		for (i = 0; i < obj_node0; i++) {
			data = &mbuf[4][i];
			*graph_field(data) =
				((uint64_t)tm->test_node[0].idx << 32) | i;
			*graph_field(data) |= (1 << 17);
			if ((i + 1) == obj_node0)
				*graph_field(data) |= (1 << 16);
		}
		rte_node_enqueue(graph, node, 0, (void **)&mbuf_p[4][0],
				 obj_node0);
	}

end:
	return nb_objs;
}

static int
test_lookup_functions(void)
{
	test_main_t *tm = &test_main;
	int i;

	/* Verify the name with ID */
	for (i = 1; i < MAX_NODES; i++) {
		char *name = rte_node_id_to_name(tm->test_node[i].idx);
		if (strcmp(name, node_names[i]) != 0) {
			printf("Test node name verify by ID = %d failed "
			       "Expected = %s, got %s\n",
			       i, node_names[i], name);
			return -1;
		}
	}

	/* Verify by name */
	for (i = 1; i < MAX_NODES; i++) {
		uint32_t idx = rte_node_from_name(node_names[i]);
		if (idx != tm->test_node[i].idx) {
			printf("Test node ID verify by name = %s failed "
			       "Expected = %d, got %d\n",
			       node_names[i], tm->test_node[i].idx, idx);
			return -1;
		}
	}

	/* Verify edge count */
	for (i = 1; i < MAX_NODES; i++) {
		uint32_t count = rte_node_edge_count(tm->test_node[i].idx);
		if (count != tm->test_node[i].node.nb_edges) {
			printf("Test number of edges for node = %s failed Expected = %d, got = %d\n",
			       tm->test_node[i].node.name,
			       tm->test_node[i].node.nb_edges, count);
			return -1;
		}
	}

	/* Verify edge names */
	for (i = 1; i < MAX_NODES; i++) {
		uint32_t j, count;
		char **next_edges;

		count = rte_node_edge_get(tm->test_node[i].idx, NULL);
		if (count != tm->test_node[i].node.nb_edges * sizeof(char *)) {
			printf("Test number of edge count for node = %s failed Expected = %d, got = %d\n",
			       tm->test_node[i].node.name,
			       tm->test_node[i].node.nb_edges, count);
			return -1;
		}
		next_edges = malloc(count);
		count = rte_node_edge_get(tm->test_node[i].idx, next_edges);
		if (count != tm->test_node[i].node.nb_edges) {
			printf("Test number of edges for node = %s failed Expected = %d, got %d\n",
			       tm->test_node[i].node.name,
			       tm->test_node[i].node.nb_edges, count);
			free(next_edges);
			return -1;
		}

		for (j = 0; j < count; j++) {
			if (strcmp(next_edges[j],
				   tm->test_node[i].node.next_nodes[j]) != 0) {
				printf("Edge name miss match, expected = %s got = %s\n",
				       tm->test_node[i].node.next_nodes[j],
				       next_edges[j]);
				free(next_edges);
				return -1;
			}
		}
		free(next_edges);
	}

	return 0;
}

static int
test_node_clone(void)
{
	test_main_t *tm = &test_main;
	uint32_t node_id, dummy_id;
	int i;

	node_id = rte_node_from_name("test_node00");
	tm->test_node[0].idx = node_id;

	dummy_id = rte_node_clone(node_id, "test_node00");
	if (rte_node_is_invalid(dummy_id)) {
		printf("Got invalid id when clone, Expecting fail\n");
		return -1;
	}

	/* Clone with same name, should fail */
	dummy_id = rte_node_clone(node_id, "test_node00");
	if (!rte_node_is_invalid(dummy_id)) {
		printf("Got valid id when clone with same name, Expecting fail\n");
		return -1;
	}

	for (i = 1; i < MAX_NODES; i++) {
		tm->test_node[i].idx =
			rte_node_clone(node_id, tm->test_node[i].node.name);
		if (rte_node_is_invalid(tm->test_node[i].idx)) {
			printf("Got invalid node id\n");
			return -1;
		}
	}

	/* Clone from cloned node should fail */
	dummy_id = rte_node_clone(tm->test_node[1].idx, "dummy_node");
	if (!rte_node_is_invalid(dummy_id)) {
		printf("Got valid node id when cloning from cloned node, expected fail\n");
		return -1;
	}

	return 0;
}

static int
test_update_edges(void)
{
	test_main_t *tm = &test_main;
	uint32_t node_id;
	uint16_t count;
	int i;

	node_id = rte_node_from_name("test_node00");
	count = rte_node_edge_update(node_id, 0,
				     tm->test_node[0].node.next_nodes,
				     tm->test_node[0].node.nb_edges);
	if (count != tm->test_node[0].node.nb_edges) {
		printf("Update edges failed expected: %d got = %d\n",
		       tm->test_node[0].node.nb_edges, count);
		return -1;
	}

	for (i = 1; i < MAX_NODES; i++) {
		count = rte_node_edge_update(tm->test_node[i].idx, 0,
					     tm->test_node[i].node.next_nodes,
					     tm->test_node[i].node.nb_edges);
		if (count != tm->test_node[i].node.nb_edges) {
			printf("Update edges failed expected: %d got = %d\n",
			       tm->test_node[i].node.nb_edges, count);
			return -1;
		}

		count = rte_node_edge_shrink(tm->test_node[i].idx,
					     tm->test_node[i].node.nb_edges);
		if (count != tm->test_node[i].node.nb_edges) {
			printf("Shrink edges failed\n");
			return -1;
		}
	}

	return 0;
}

static int
test_create_graph(void)
{
	static const char *node_patterns_dummy[] = {
		"test_node_source1",	   "test_node00",
		"test_node00-test_node11", "test_node00-test_node22",
		"test_node00-test_node33", "test_node00-dummy_node",
	};
	struct rte_graph_param gconf = {
		.socket_id = SOCKET_ID_ANY,
		.nb_node_patterns = 6,
		.node_patterns = node_patterns_dummy,
	};
	uint32_t dummy_node_id;
	uint32_t node_id;

	node_id = rte_node_from_name("test_node00");
	dummy_node_id = rte_node_clone(node_id, "dummy_node");
	if (rte_node_is_invalid(dummy_node_id)) {
		printf("Got invalid node id\n");
		return -1;
	}

	graph_id = rte_graph_create("worker0", &gconf);
	if (graph_id != RTE_GRAPH_ID_INVALID) {
		printf("Graph creation success with isolated node, expected graph creation fail\n");
		return -1;
	}

	gconf.nb_node_patterns = 5;
	gconf.node_patterns = node_patterns;
	graph_id = rte_graph_create("worker0", &gconf);
	if (graph_id == RTE_GRAPH_ID_INVALID) {
		printf("Graph creation failed with error = %d\n", rte_errno);
		return -1;
	}
	return 0;
}

static int
test_graph_clone(void)
{
	rte_graph_t cloned_graph_id = RTE_GRAPH_ID_INVALID;
	rte_graph_t main_graph_id = RTE_GRAPH_ID_INVALID;
	struct rte_graph_param graph_conf = {0};
	int ret = 0;

	main_graph_id = rte_graph_from_name("worker0");
	if (main_graph_id == RTE_GRAPH_ID_INVALID) {
		printf("Must create main graph first\n");
		ret = -1;
	}

	graph_conf.dispatch.mp_capacity = 1024;
	graph_conf.dispatch.wq_size_max = 32;

	cloned_graph_id = rte_graph_clone(main_graph_id, "cloned-test0", &graph_conf);

	if (cloned_graph_id == RTE_GRAPH_ID_INVALID) {
		printf("Graph creation failed with error = %d\n", rte_errno);
		ret = -1;
	}

	if (strcmp(rte_graph_id_to_name(cloned_graph_id), "worker0-cloned-test0")) {
		printf("Cloned graph should name as %s but get %s\n", "worker0-cloned-test",
		       rte_graph_id_to_name(cloned_graph_id));
		ret = -1;
	}

	rte_graph_destroy(cloned_graph_id);

	return ret;
}

static int
test_graph_model_mcore_dispatch_node_lcore_affinity_set(void)
{
	rte_graph_t cloned_graph_id = RTE_GRAPH_ID_INVALID;
	unsigned int worker_lcore = RTE_MAX_LCORE;
	struct rte_graph_param graph_conf = {0};
	rte_node_t nid = RTE_NODE_ID_INVALID;
	char node_name[64] = "test_node00";
	struct rte_node *node;
	int ret = 0;

	worker_lcore = rte_get_next_lcore(worker_lcore, true, 1);
	ret = rte_graph_model_mcore_dispatch_node_lcore_affinity_set(node_name, worker_lcore);
	if (ret == 0)
		printf("Set node %s affinity to lcore %u\n", node_name, worker_lcore);

	nid = rte_node_from_name(node_name);
	cloned_graph_id = rte_graph_clone(graph_id, "cloned-test1", &graph_conf);
	node = rte_graph_node_get(cloned_graph_id, nid);

	if (node->dispatch.lcore_id != worker_lcore) {
		printf("set node affinity failed\n");
		ret = -1;
	}

	rte_graph_destroy(cloned_graph_id);

	return ret;
}

static int
test_graph_model_mcore_dispatch_core_bind_unbind(void)
{
	rte_graph_t cloned_graph_id = RTE_GRAPH_ID_INVALID;
	unsigned int worker_lcore = RTE_MAX_LCORE;
	struct rte_graph_param graph_conf = {0};
	struct rte_graph *graph;
	int ret = 0;

	worker_lcore = rte_get_next_lcore(worker_lcore, true, 1);
	cloned_graph_id = rte_graph_clone(graph_id, "cloned-test2", &graph_conf);

	ret = rte_graph_worker_model_set(RTE_GRAPH_MODEL_MCORE_DISPATCH);
	if (ret != 0) {
		printf("Set graph mcore dispatch model failed\n");
		ret = -1;
	}

	ret = rte_graph_model_mcore_dispatch_core_bind(cloned_graph_id, worker_lcore);
	if (ret != 0) {
		printf("bind graph %d to lcore %u failed\n", graph_id, worker_lcore);
		ret = -1;
	}

	graph = rte_graph_lookup("worker0-cloned-test2");

	if (graph->dispatch.lcore_id != worker_lcore) {
		printf("bind graph %s(id:%d) with lcore %u failed\n",
		       graph->name, graph->id, worker_lcore);
		ret = -1;
	}

	rte_graph_model_mcore_dispatch_core_unbind(cloned_graph_id);
	if (graph->dispatch.lcore_id != RTE_MAX_LCORE) {
		printf("unbind graph %s(id:%d) failed %d\n",
		       graph->name, graph->id, graph->dispatch.lcore_id);
		ret = -1;
	}

	rte_graph_destroy(cloned_graph_id);

	return ret;
}

static int
test_graph_worker_model_set_get(void)
{
	rte_graph_t cloned_graph_id = RTE_GRAPH_ID_INVALID;
	struct rte_graph_param graph_conf = {0};
	struct rte_graph *graph;
	int ret = 0;

	cloned_graph_id = rte_graph_clone(graph_id, "cloned-test3", &graph_conf);
	ret = rte_graph_worker_model_set(RTE_GRAPH_MODEL_MCORE_DISPATCH);
	if (ret != 0) {
		printf("Set graph mcore dispatch model failed\n");
		ret = -1;
	}

	graph = rte_graph_lookup("worker0-cloned-test3");
	if (rte_graph_worker_model_get(graph) != RTE_GRAPH_MODEL_MCORE_DISPATCH) {
		printf("Get graph worker model failed\n");
		ret = -1;
	}

	rte_graph_destroy(cloned_graph_id);

	return 0;
}

static int
test_graph_walk(void)
{
	struct rte_graph *graph = rte_graph_lookup("worker0");
	int i;

	if (!graph) {
		printf("Graph lookup failed\n");
		return -1;
	}

	for (i = 0; i < 5; i++)
		rte_graph_walk(graph);
	return 0;
}

static int
test_graph_lookup_functions(void)
{
	test_main_t *tm = &test_main;
	struct rte_node *node;
	int i;

	for (i = 0; i < MAX_NODES; i++) {
		node = rte_graph_node_get(graph_id, tm->test_node[i].idx);
		if (!node) {
			printf("rte_graph_node_get, failed for node = %d\n",
			       tm->test_node[i].idx);
			return -1;
		}

		if (tm->test_node[i].idx != node->id) {
			printf("Node id didn't match, expected = %d got = %d\n",
			       tm->test_node[i].idx, node->id);
			return 0;
		}

		if (strncmp(node->name, node_names[i], RTE_NODE_NAMESIZE)) {
			printf("Node name didn't match, expected = %s got %s\n",
			       node_names[i], node->name);
			return -1;
		}
	}

	for (i = 0; i < MAX_NODES; i++) {
		node = rte_graph_node_get_by_name("worker0", node_names[i]);
		if (!node) {
			printf("rte_graph_node_get, failed for node = %d\n",
			       tm->test_node[i].idx);
			return -1;
		}

		if (tm->test_node[i].idx != node->id) {
			printf("Node id didn't match, expected = %d got = %d\n",
			       tm->test_node[i].idx, node->id);
			return 0;
		}

		if (strncmp(node->name, node_names[i], RTE_NODE_NAMESIZE)) {
			printf("Node name didn't match, expected = %s got %s\n",
			       node_names[i], node->name);
			return -1;
		}
	}

	return 0;
}

static int
graph_cluster_stats_cb_t(bool is_first, bool is_last, void *cookie,
			 const struct rte_graph_cluster_node_stats *st)
{
	int i;

	RTE_SET_USED(is_first);
	RTE_SET_USED(is_last);
	RTE_SET_USED(cookie);

	for (i = 0; i < MAX_NODES + 1; i++) {
		rte_node_t id = rte_node_from_name(node_patterns[i]);
		if (id == st->id) {
			if (obj_stats[i] != st->objs) {
				printf("Obj count miss match for node = %s expected = %"PRId64", got=%"PRId64"\n",
				       node_patterns[i], obj_stats[i],
				       st->objs);
				return -1;
			}

			if (fn_calls[i] != st->calls) {
				printf("Func call miss match for node = %s expected = %"PRId64", got = %"PRId64"\n",
				       node_patterns[i], fn_calls[i],
				       st->calls);
				return -1;
			}
		}
	}
	return 0;
}

static int
test_print_stats(void)
{
	struct rte_graph_cluster_stats_param s_param;
	struct rte_graph_cluster_stats *stats;
	const char *pattern = "worker0";

	if (!rte_graph_has_stats_feature())
		return 0;

	/* Prepare stats object */
	memset(&s_param, 0, sizeof(s_param));
	s_param.f = stdout;
	s_param.socket_id = SOCKET_ID_ANY;
	s_param.graph_patterns = &pattern;
	s_param.nb_graph_patterns = 1;
	s_param.fn = graph_cluster_stats_cb_t;

	stats = rte_graph_cluster_stats_create(&s_param);
	if (stats == NULL) {
		printf("Unable to get stats\n");
		return -1;
	}
	/* Clear screen and move to top left */
	rte_graph_cluster_stats_get(stats, 0);
	rte_graph_cluster_stats_destroy(stats);

	return 0;
}

static int
graph_setup(void)
{
	int i, j;

	static const struct rte_mbuf_dynfield graph_dynfield_desc = {
		.name = "test_graph_dynfield",
		.size = sizeof(graph_dynfield_t),
		.align = __alignof__(graph_dynfield_t),
	};
	graph_dynfield_offset =
		rte_mbuf_dynfield_register(&graph_dynfield_desc);
	if (graph_dynfield_offset < 0) {
		printf("Cannot register mbuf field\n");
		return TEST_FAILED;
	}

	for (i = 0; i <= MAX_NODES; i++) {
		for (j = 0; j < MBUFF_SIZE; j++)
			mbuf_p[i][j] = &mbuf[i][j];
	}
	if (test_node_clone()) {
		printf("test_node_clone: fail\n");
		return -1;
	}
	printf("test_node_clone: pass\n");

	return 0;
}

static void
graph_teardown(void)
{
	int id;

	id = rte_graph_destroy(rte_graph_from_name("worker0"));
	if (id)
		printf("Graph Destroy failed\n");
}

static struct unit_test_suite graph_testsuite = {
	.suite_name = "Graph library test suite",
	.setup = graph_setup,
	.teardown = graph_teardown,
	.unit_test_cases = {
		TEST_CASE(test_update_edges),
		TEST_CASE(test_lookup_functions),
		TEST_CASE(test_create_graph),
		TEST_CASE(test_graph_clone),
		TEST_CASE(test_graph_model_mcore_dispatch_node_lcore_affinity_set),
		TEST_CASE(test_graph_model_mcore_dispatch_core_bind_unbind),
		TEST_CASE(test_graph_worker_model_set_get),
		TEST_CASE(test_graph_lookup_functions),
		TEST_CASE(test_graph_walk),
		TEST_CASE(test_print_stats),
		TEST_CASES_END(), /**< NULL terminate unit test array */
	},
};

static int
graph_autotest_fn(void)
{
	return unit_test_suite_runner(&graph_testsuite);
}

REGISTER_TEST_COMMAND(graph_autotest, graph_autotest_fn);

static int
test_node_list_dump(void)
{
	rte_node_list_dump(stdout);

	return TEST_SUCCESS;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(node_list_dump, test_node_list_dump);
