/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <fnmatch.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_malloc.h>

#include "graph_private.h"

/* Capture all graphs of cluster */
struct cluster {
	rte_graph_t nb_graphs;
	rte_graph_t size;

	struct graph **graphs;
};

/* Capture same node ID across cluster  */
struct cluster_node {
	struct rte_graph_cluster_node_stats stat;
	rte_node_t nb_nodes;

	struct rte_node *nodes[];
};

struct rte_graph_cluster_stats {
	/* Header */
	rte_graph_cluster_stats_cb_t fn;
	uint32_t cluster_node_size; /* Size of struct cluster_node */
	rte_node_t max_nodes;
	int socket_id;
	void *cookie;
	size_t sz;

	struct cluster_node clusters[];
} __rte_cache_aligned;

#define boarder()                                                              \
	fprintf(f, "+-------------------------------+---------------+--------" \
		   "-------+---------------+---------------+---------------+-" \
		   "----------+\n")

static inline void
print_banner(FILE *f)
{
	boarder();
	fprintf(f, "%-32s%-16s%-16s%-16s%-16s%-16s%-16s\n", "|Node", "|calls",
		"|objs", "|realloc_count", "|objs/call", "|objs/sec(10E6)",
		"|cycles/call|");
	boarder();
}

static inline void
print_node(FILE *f, const struct rte_graph_cluster_node_stats *stat)
{
	double objs_per_call, objs_per_sec, cycles_per_call, ts_per_hz;
	const uint64_t prev_calls = stat->prev_calls;
	const uint64_t prev_objs = stat->prev_objs;
	const uint64_t cycles = stat->cycles;
	const uint64_t calls = stat->calls;
	const uint64_t objs = stat->objs;
	uint64_t call_delta;

	call_delta = calls - prev_calls;
	objs_per_call =
		call_delta ? (double)((objs - prev_objs) / call_delta) : 0;
	cycles_per_call =
		call_delta ? (double)((cycles - stat->prev_cycles) / call_delta)
			   : 0;
	ts_per_hz = (double)((stat->ts - stat->prev_ts) / stat->hz);
	objs_per_sec = ts_per_hz ? (objs - prev_objs) / ts_per_hz : 0;
	objs_per_sec /= 1000000;

	fprintf(f,
		"|%-31s|%-15" PRIu64 "|%-15" PRIu64 "|%-15" PRIu64
		"|%-15.3f|%-15.6f|%-11.4f|\n",
		stat->name, calls, objs, stat->realloc_count, objs_per_call,
		objs_per_sec, cycles_per_call);
}

static int
graph_cluster_stats_cb(bool is_first, bool is_last, void *cookie,
		       const struct rte_graph_cluster_node_stats *stat)
{
	FILE *f = cookie;

	if (unlikely(is_first))
		print_banner(f);
	if (stat->objs)
		print_node(f, stat);
	if (unlikely(is_last))
		boarder();

	return 0;
};

static struct rte_graph_cluster_stats *
stats_mem_init(struct cluster *cluster,
	       const struct rte_graph_cluster_stats_param *prm)
{
	size_t sz = sizeof(struct rte_graph_cluster_stats);
	struct rte_graph_cluster_stats *stats;
	rte_graph_cluster_stats_cb_t fn;
	int socket_id = prm->socket_id;
	uint32_t cluster_node_size;

	/* Fix up callback */
	fn = prm->fn;
	if (fn == NULL)
		fn = graph_cluster_stats_cb;

	cluster_node_size = sizeof(struct cluster_node);
	/* For a given cluster, max nodes will be the max number of graphs */
	cluster_node_size += cluster->nb_graphs * sizeof(struct rte_node *);
	cluster_node_size = RTE_ALIGN(cluster_node_size, RTE_CACHE_LINE_SIZE);

	stats = realloc(NULL, sz);
	if (stats) {
		memset(stats, 0, sz);
		stats->fn = fn;
		stats->cluster_node_size = cluster_node_size;
		stats->max_nodes = 0;
		stats->socket_id = socket_id;
		stats->cookie = prm->cookie;
		stats->sz = sz;
	}

	return stats;
}

static int
stats_mem_populate(struct rte_graph_cluster_stats **stats_in,
		   struct rte_graph *graph, struct graph_node *graph_node)
{
	struct rte_graph_cluster_stats *stats = *stats_in;
	rte_node_t id = graph_node->node->id;
	struct cluster_node *cluster;
	struct rte_node *node;
	rte_node_t count;

	cluster = stats->clusters;

	/* Iterate over cluster node array to find node ID match */
	for (count = 0; count < stats->max_nodes; count++) {
		/* Found an existing node in the reel */
		if (cluster->stat.id == id) {
			node = graph_node_id_to_ptr(graph, id);
			if (node == NULL)
				SET_ERR_JMP(
					ENOENT, err,
					"Failed to find node %s in graph %s",
					graph_node->node->name, graph->name);

			cluster->nodes[cluster->nb_nodes++] = node;
			return 0;
		}
		cluster = RTE_PTR_ADD(cluster, stats->cluster_node_size);
	}

	/* Hey, it is a new node, allocate space for it in the reel */
	stats = realloc(stats, stats->sz + stats->cluster_node_size);
	if (stats == NULL)
		SET_ERR_JMP(ENOMEM, err, "Realloc failed");
	*stats_in = NULL;

	/* Clear the new struct cluster_node area */
	cluster = RTE_PTR_ADD(stats, stats->sz),
	memset(cluster, 0, stats->cluster_node_size);
	memcpy(cluster->stat.name, graph_node->node->name, RTE_NODE_NAMESIZE);
	cluster->stat.id = graph_node->node->id;
	cluster->stat.hz = rte_get_timer_hz();
	node = graph_node_id_to_ptr(graph, id);
	if (node == NULL)
		SET_ERR_JMP(ENOENT, free, "Failed to find node %s in graph %s",
			    graph_node->node->name, graph->name);
	cluster->nodes[cluster->nb_nodes++] = node;

	stats->sz += stats->cluster_node_size;
	stats->max_nodes++;
	*stats_in = stats;

	return 0;
free:
	free(stats);
err:
	return -rte_errno;
}

static void
stats_mem_fini(struct rte_graph_cluster_stats *stats)
{
	free(stats);
}

static void
cluster_init(struct cluster *cluster)
{
	memset(cluster, 0, sizeof(*cluster));
}

static int
cluster_add(struct cluster *cluster, struct graph *graph)
{
	rte_graph_t count;
	size_t sz;

	/* Skip the if graph is already added to cluster */
	for (count = 0; count < cluster->nb_graphs; count++)
		if (cluster->graphs[count] == graph)
			return 0;

	/* Expand the cluster if required to store graph objects */
	if (cluster->nb_graphs + 1 > cluster->size) {
		cluster->size = RTE_MAX(1, cluster->size * 2);
		sz = sizeof(struct graph *) * cluster->size;
		cluster->graphs = realloc(cluster->graphs, sz);
		if (cluster->graphs == NULL)
			SET_ERR_JMP(ENOMEM, free, "Failed to realloc");
	}

	/* Add graph to cluster */
	cluster->graphs[cluster->nb_graphs++] = graph;
	return 0;

free:
	return -rte_errno;
}

static void
cluster_fini(struct cluster *cluster)
{
	free(cluster->graphs);
}

static int
expand_pattern_to_cluster(struct cluster *cluster, const char *pattern)
{
	struct graph_head *graph_head = graph_list_head_get();
	struct graph *graph;
	bool found = false;

	/* Check for pattern match */
	STAILQ_FOREACH(graph, graph_head, next) {
		if (fnmatch(pattern, graph->name, 0) == 0) {
			if (cluster_add(cluster, graph))
				goto fail;
			found = true;
		}
	}
	if (found == false)
		SET_ERR_JMP(EFAULT, fail, "Pattern %s graph not found",
			    pattern);

	return 0;
fail:
	return -rte_errno;
}

struct rte_graph_cluster_stats *
rte_graph_cluster_stats_create(const struct rte_graph_cluster_stats_param *prm)
{
	struct rte_graph_cluster_stats *stats, *rc = NULL;
	struct graph_node *graph_node;
	struct cluster cluster;
	struct graph *graph;
	const char *pattern;
	rte_graph_t i;

	/* Sanity checks */
	if (!rte_graph_has_stats_feature())
		SET_ERR_JMP(EINVAL, fail, "Stats feature is not enabled");

	if (prm == NULL)
		SET_ERR_JMP(EINVAL, fail, "Invalid param");

	if (prm->graph_patterns == NULL || prm->nb_graph_patterns == 0)
		SET_ERR_JMP(EINVAL, fail, "Invalid graph param");

	cluster_init(&cluster);

	graph_spinlock_lock();
	/* Expand graph pattern and add the graph to the cluster */
	for (i = 0; i < prm->nb_graph_patterns; i++) {
		pattern = prm->graph_patterns[i];
		if (expand_pattern_to_cluster(&cluster, pattern))
			goto bad_pattern;
	}

	/* Alloc the stats memory */
	stats = stats_mem_init(&cluster, prm);
	if (stats == NULL)
		SET_ERR_JMP(ENOMEM, bad_pattern, "Failed alloc stats memory");

	/* Iterate over M(Graph) x N (Nodes in graph) */
	for (i = 0; i < cluster.nb_graphs; i++) {
		graph = cluster.graphs[i];
		STAILQ_FOREACH(graph_node, &graph->node_list, next) {
			struct rte_graph *graph_fp = graph->graph;
			if (stats_mem_populate(&stats, graph_fp, graph_node))
				goto realloc_fail;
		}
	}

	/* Finally copy to hugepage memory to avoid pressure on rte_realloc */
	rc = rte_malloc_socket(NULL, stats->sz, 0, stats->socket_id);
	if (rc)
		rte_memcpy(rc, stats, stats->sz);
	else
		SET_ERR_JMP(ENOMEM, realloc_fail, "rte_malloc failed");

realloc_fail:
	stats_mem_fini(stats);
bad_pattern:
	graph_spinlock_unlock();
	cluster_fini(&cluster);
fail:
	return rc;
}

void
rte_graph_cluster_stats_destroy(struct rte_graph_cluster_stats *stat)
{
	return rte_free(stat);
}

static inline void
cluster_node_arregate_stats(struct cluster_node *cluster)
{
	uint64_t calls = 0, cycles = 0, objs = 0, realloc_count = 0;
	struct rte_graph_cluster_node_stats *stat = &cluster->stat;
	struct rte_node *node;
	rte_node_t count;

	for (count = 0; count < cluster->nb_nodes; count++) {
		node = cluster->nodes[count];

		calls += node->total_calls;
		objs += node->total_objs;
		cycles += node->total_cycles;
		realloc_count += node->realloc_count;
	}

	stat->calls = calls;
	stat->objs = objs;
	stat->cycles = cycles;
	stat->ts = rte_get_timer_cycles();
	stat->realloc_count = realloc_count;
}

static inline void
cluster_node_store_prev_stats(struct cluster_node *cluster)
{
	struct rte_graph_cluster_node_stats *stat = &cluster->stat;

	stat->prev_ts = stat->ts;
	stat->prev_calls = stat->calls;
	stat->prev_objs = stat->objs;
	stat->prev_cycles = stat->cycles;
}

void
rte_graph_cluster_stats_get(struct rte_graph_cluster_stats *stat, bool skip_cb)
{
	struct cluster_node *cluster;
	rte_node_t count;
	int rc = 0;

	cluster = stat->clusters;

	for (count = 0; count < stat->max_nodes; count++) {
		cluster_node_arregate_stats(cluster);
		if (!skip_cb)
			rc = stat->fn(!count, (count == stat->max_nodes - 1),
				      stat->cookie, &cluster->stat);
		cluster_node_store_prev_stats(cluster);
		if (rc)
			break;
		cluster = RTE_PTR_ADD(cluster, stat->cluster_node_size);
	}
}

void
rte_graph_cluster_stats_reset(struct rte_graph_cluster_stats *stat)
{
	struct cluster_node *cluster;
	rte_node_t count;

	cluster = stat->clusters;

	for (count = 0; count < stat->max_nodes; count++) {
		struct rte_graph_cluster_node_stats *node = &cluster->stat;

		node->ts = 0;
		node->calls = 0;
		node->objs = 0;
		node->cycles = 0;
		node->prev_ts = 0;
		node->prev_calls = 0;
		node->prev_objs = 0;
		node->prev_cycles = 0;
		node->realloc_count = 0;
		cluster = RTE_PTR_ADD(cluster, stat->cluster_node_size);
	}
}
