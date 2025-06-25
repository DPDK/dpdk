/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <fnmatch.h>
#include <stdbool.h>
#include <stdlib.h>

#include <eal_export.h>
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

struct __rte_cache_aligned rte_graph_cluster_stats {
	/* Header */
	rte_graph_cluster_stats_cb_t fn;
	uint32_t cluster_node_size; /* Size of struct cluster_node */
	rte_node_t max_nodes;
	int socket_id;
	bool dispatch;
	void *cookie;
	size_t sz;

	struct cluster_node clusters[];
};

#define boarder_model_dispatch()                                                              \
	fprintf(f, "+-------------------------------+---------------+--------" \
		   "-------+---------------+---------------+---------------+" \
		   "---------------+---------------+-" \
		   "----------+\n")

#define boarder()                                                              \
	fprintf(f, "+-------------------------------+---------------+--------" \
		   "-------+---------------+---------------+---------------+-" \
		   "----------+\n")

static inline void
print_banner_default(FILE *f)
{
	boarder();
	fprintf(f, "%-32s%-16s%-16s%-16s%-16s%-16s%-16s\n", "|Node", "|calls",
		"|objs", "|realloc_count", "|objs/call", "|objs/sec(10E6)",
		"|cycles/call|");
	boarder();
}

static inline void
print_banner_dispatch(FILE *f)
{
	boarder_model_dispatch();
	fprintf(f, "%-32s%-16s%-16s%-16s%-16s%-16s%-16s%-16s%-16s\n",
		"|Node", "|calls",
		"|objs", "|sched objs", "|sched fail",
		"|realloc_count", "|objs/call", "|objs/sec(10E6)",
		"|cycles/call|");
	boarder_model_dispatch();
}

static inline void
print_banner(FILE *f, bool dispatch)
{
	if (dispatch)
		print_banner_dispatch(f);
	else
		print_banner_default(f);
}

static inline void
print_node(FILE *f, const struct rte_graph_cluster_node_stats *stat, bool dispatch)
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

	if (dispatch) {
		fprintf(f,
			"|%-31s|%-15" PRIu64 "|%-15" PRIu64 "|%-15" PRIu64
			"|%-15" PRIu64 "|%-15" PRIu64
			"|%-15.3f|%-15.6f|%-11.4f|\n",
			stat->name, calls, objs, stat->dispatch.sched_objs,
			stat->dispatch.sched_fail, stat->realloc_count, objs_per_call,
			objs_per_sec, cycles_per_call);
	} else {
		fprintf(f,
			"|%-31s|%-15" PRIu64 "|%-15" PRIu64 "|%-15" PRIu64
			"|%-15.3f|%-15.6f|%-11.4f|\n",
			stat->name, calls, objs, stat->realloc_count, objs_per_call,
			objs_per_sec, cycles_per_call);
	}
}

static inline void
print_xstat(FILE *f, const struct rte_graph_cluster_node_stats *stat, bool dispatch)
{
	int i;

	if (dispatch) {
		for (i = 0; i < stat->xstat_cntrs; i++)
			fprintf(f,
				"|\t%-24s|%15s|%-15" PRIu64 "|%15s|%15s|%15s|%15s|%15s|%11.4s|\n",
				stat->xstat_desc[i], "", stat->xstat_count[i], "", "", "", "", "",
				"");
	} else {
		for (i = 0; i < stat->xstat_cntrs; i++)
			fprintf(f,
				"|\t%-24s|%15s|%-15" PRIu64 "|%15s|%15.3s|%15.6s|%11.4s|\n",
				stat->xstat_desc[i], "", stat->xstat_count[i], "", "", "", "");
	}
}

static int
graph_cluster_stats_cb(bool dispatch, bool is_first, bool is_last, void *cookie,
		       const struct rte_graph_cluster_node_stats *stat)
{
	FILE *f = cookie;

	if (unlikely(is_first))
		print_banner(f, dispatch);
	if (stat->objs) {
		print_node(f, stat, dispatch);
		if (stat->xstat_cntrs)
			print_xstat(f, stat, dispatch);
	}
	if (unlikely(is_last)) {
		if (dispatch)
			boarder_model_dispatch();
		else
			boarder();
	}

	return 0;
};

static int
graph_cluster_stats_cb_rtc(bool is_first, bool is_last, void *cookie,
			   const struct rte_graph_cluster_node_stats *stat)
{
	return graph_cluster_stats_cb(false, is_first, is_last, cookie, stat);
};

static int
graph_cluster_stats_cb_dispatch(bool is_first, bool is_last, void *cookie,
				const struct rte_graph_cluster_node_stats *stat)
{
	return graph_cluster_stats_cb(true, is_first, is_last, cookie, stat);
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
	if (fn == NULL) {
		const struct rte_graph *graph = cluster->graphs[0]->graph;
		if (graph->model == RTE_GRAPH_MODEL_MCORE_DISPATCH)
			fn = graph_cluster_stats_cb_dispatch;
		else
			fn = graph_cluster_stats_cb_rtc;
	}

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
	uint8_t i;

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
	if (graph_node->node->xstats) {
		cluster->stat.xstat_cntrs = graph_node->node->xstats->nb_xstats;
		cluster->stat.xstat_count = rte_zmalloc_socket(NULL,
			sizeof(uint64_t) * graph_node->node->xstats->nb_xstats,
			RTE_CACHE_LINE_SIZE, stats->socket_id);
		if (cluster->stat.xstat_count == NULL)
			SET_ERR_JMP(ENOMEM, free, "Failed to allocate memory node %s graph %s",
				    graph_node->node->name, graph->name);

		cluster->stat.xstat_desc = rte_zmalloc_socket(NULL,
			sizeof(RTE_NODE_XSTAT_DESC_SIZE) * graph_node->node->xstats->nb_xstats,
			RTE_CACHE_LINE_SIZE, stats->socket_id);
		if (cluster->stat.xstat_desc == NULL) {
			rte_free(cluster->stat.xstat_count);
			SET_ERR_JMP(ENOMEM, free, "Failed to allocate memory node %s graph %s",
				    graph_node->node->name, graph->name);
		}

		for (i = 0; i < cluster->stat.xstat_cntrs; i++) {
			if (rte_strscpy(cluster->stat.xstat_desc[i],
					graph_node->node->xstats->xstat_desc[i],
					RTE_NODE_XSTAT_DESC_SIZE) < 0) {
				rte_free(cluster->stat.xstat_count);
				rte_free(cluster->stat.xstat_desc);
				SET_ERR_JMP(E2BIG, free,
					    "Error description overflow node %s graph %s",
					    graph_node->node->name, graph->name);
			}
		}
	}

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

RTE_EXPORT_SYMBOL(rte_graph_cluster_stats_create)
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
		if (graph->graph->model == RTE_GRAPH_MODEL_MCORE_DISPATCH)
			stats->dispatch = true;
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

RTE_EXPORT_SYMBOL(rte_graph_cluster_stats_destroy)
void
rte_graph_cluster_stats_destroy(struct rte_graph_cluster_stats *stat)
{
	struct cluster_node *cluster;
	rte_node_t count;

	cluster = stat->clusters;
	for (count = 0; count < stat->max_nodes; count++) {
		if (cluster->stat.xstat_cntrs) {
			rte_free(cluster->stat.xstat_count);
			rte_free(cluster->stat.xstat_desc);
		}

		cluster = RTE_PTR_ADD(cluster, stat->cluster_node_size);
	}
	return rte_free(stat);
}

static inline void
cluster_node_arregate_stats(struct cluster_node *cluster, bool dispatch)
{
	uint64_t calls = 0, cycles = 0, objs = 0, realloc_count = 0;
	struct rte_graph_cluster_node_stats *stat = &cluster->stat;
	uint64_t sched_objs = 0, sched_fail = 0;
	struct rte_node *node;
	rte_node_t count;
	uint64_t *xstat;
	uint8_t i;

	if (stat->xstat_cntrs != 0)
		memset(stat->xstat_count, 0, sizeof(uint64_t) * stat->xstat_cntrs);
	for (count = 0; count < cluster->nb_nodes; count++) {
		node = cluster->nodes[count];

		if (dispatch) {
			sched_objs += node->dispatch.total_sched_objs;
			sched_fail += node->dispatch.total_sched_fail;
		}

		calls += node->total_calls;
		objs += node->total_objs;
		cycles += node->total_cycles;
		realloc_count += node->realloc_count;

		if (node->xstat_off == 0)
			continue;
		xstat = RTE_PTR_ADD(node, node->xstat_off);
		for (i = 0; i < stat->xstat_cntrs; i++)
			stat->xstat_count[i] += xstat[i];
	}

	stat->calls = calls;
	stat->objs = objs;
	stat->cycles = cycles;

	if (dispatch) {
		stat->dispatch.sched_objs = sched_objs;
		stat->dispatch.sched_fail = sched_fail;
	}

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

RTE_EXPORT_SYMBOL(rte_graph_cluster_stats_get)
void
rte_graph_cluster_stats_get(struct rte_graph_cluster_stats *stat, bool skip_cb)
{
	struct cluster_node *cluster;
	rte_node_t count;
	int rc = 0;

	cluster = stat->clusters;

	for (count = 0; count < stat->max_nodes; count++) {
		cluster_node_arregate_stats(cluster, stat->dispatch);
		if (!skip_cb)
			rc = stat->fn(!count, (count == stat->max_nodes - 1),
				      stat->cookie, &cluster->stat);
		cluster_node_store_prev_stats(cluster);
		if (rc)
			break;
		cluster = RTE_PTR_ADD(cluster, stat->cluster_node_size);
	}
}

RTE_EXPORT_SYMBOL(rte_graph_cluster_stats_reset)
void
rte_graph_cluster_stats_reset(struct rte_graph_cluster_stats *stat)
{
	struct cluster_node *cluster;
	rte_node_t count;
	uint8_t i;

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
		for (i = 0; i < node->xstat_cntrs; i++)
			node->xstat_count[i] = 0;
		cluster = RTE_PTR_ADD(cluster, stat->cluster_node_size);
	}
}
