/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"
#include "rte_eth_softnic.h"

#define SUBPORT_TC_PERIOD	10
#define PIPE_TC_PERIOD		40

int
softnic_tmgr_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->tmgr_port_list);

	return 0;
}

void
softnic_tmgr_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_tmgr_port *tmgr_port;

		tmgr_port = TAILQ_FIRST(&p->tmgr_port_list);
		if (tmgr_port == NULL)
			break;

		TAILQ_REMOVE(&p->tmgr_port_list, tmgr_port, node);
		rte_sched_port_free(tmgr_port->s);
		free(tmgr_port);
	}
}

struct softnic_tmgr_port *
softnic_tmgr_port_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_tmgr_port *tmgr_port;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(tmgr_port, &p->tmgr_port_list, node)
		if (strcmp(tmgr_port->name, name) == 0)
			return tmgr_port;

	return NULL;
}

struct softnic_tmgr_port *
softnic_tmgr_port_create(struct pmd_internals *p,
	const char *name)
{
	struct softnic_tmgr_port *tmgr_port;
	struct tm_params *t = &p->soft.tm.params;
	struct rte_sched_port *sched;
	uint32_t n_subports, subport_id;

	/* Check input params */
	if (name == NULL ||
		softnic_tmgr_port_find(p, name))
		return NULL;

	/*
	 * Resource
	 */

	/* Is hierarchy frozen? */
	if (p->soft.tm.hierarchy_frozen == 0)
		return NULL;

	/* Port */
	sched = rte_sched_port_config(&t->port_params);
	if (sched == NULL)
		return NULL;

	/* Subport */
	n_subports = t->port_params.n_subports_per_port;
	for (subport_id = 0; subport_id < n_subports; subport_id++) {
		uint32_t n_pipes_per_subport = t->port_params.n_pipes_per_subport;
		uint32_t pipe_id;
		int status;

		status = rte_sched_subport_config(sched,
			subport_id,
			&t->subport_params[subport_id]);
		if (status) {
			rte_sched_port_free(sched);
			return NULL;
		}

		/* Pipe */
		for (pipe_id = 0; pipe_id < n_pipes_per_subport; pipe_id++) {
			int pos = subport_id * TM_MAX_PIPES_PER_SUBPORT + pipe_id;
			int profile_id = t->pipe_to_profile[pos];

			if (profile_id < 0)
				continue;

			status = rte_sched_pipe_config(sched,
				subport_id,
				pipe_id,
				profile_id);
			if (status) {
				rte_sched_port_free(sched);
				return NULL;
			}
		}
	}

	/* Node allocation */
	tmgr_port = calloc(1, sizeof(struct softnic_tmgr_port));
	if (tmgr_port == NULL) {
		rte_sched_port_free(sched);
		return NULL;
	}

	/* Node fill in */
	strlcpy(tmgr_port->name, name, sizeof(tmgr_port->name));
	tmgr_port->s = sched;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->tmgr_port_list, tmgr_port, node);

	return tmgr_port;
}

static struct rte_sched_port *
SCHED(struct pmd_internals *p)
{
	struct softnic_tmgr_port *tmgr_port;

	tmgr_port = softnic_tmgr_port_find(p, "TMGR");
	if (tmgr_port == NULL)
		return NULL;

	return tmgr_port->s;
}

void
tm_hierarchy_init(struct pmd_internals *p)
{
	memset(&p->soft.tm, 0, sizeof(p->soft.tm));

	/* Initialize shaper profile list */
	TAILQ_INIT(&p->soft.tm.h.shaper_profiles);

	/* Initialize shared shaper list */
	TAILQ_INIT(&p->soft.tm.h.shared_shapers);

	/* Initialize wred profile list */
	TAILQ_INIT(&p->soft.tm.h.wred_profiles);

	/* Initialize TM node list */
	TAILQ_INIT(&p->soft.tm.h.nodes);
}

void
tm_hierarchy_free(struct pmd_internals *p)
{
	/* Remove all nodes*/
	for ( ; ; ) {
		struct tm_node *tm_node;

		tm_node = TAILQ_FIRST(&p->soft.tm.h.nodes);
		if (tm_node == NULL)
			break;

		TAILQ_REMOVE(&p->soft.tm.h.nodes, tm_node, node);
		free(tm_node);
	}

	/* Remove all WRED profiles */
	for ( ; ; ) {
		struct tm_wred_profile *wred_profile;

		wred_profile = TAILQ_FIRST(&p->soft.tm.h.wred_profiles);
		if (wred_profile == NULL)
			break;

		TAILQ_REMOVE(&p->soft.tm.h.wred_profiles, wred_profile, node);
		free(wred_profile);
	}

	/* Remove all shared shapers */
	for ( ; ; ) {
		struct tm_shared_shaper *shared_shaper;

		shared_shaper = TAILQ_FIRST(&p->soft.tm.h.shared_shapers);
		if (shared_shaper == NULL)
			break;

		TAILQ_REMOVE(&p->soft.tm.h.shared_shapers, shared_shaper, node);
		free(shared_shaper);
	}

	/* Remove all shaper profiles */
	for ( ; ; ) {
		struct tm_shaper_profile *shaper_profile;

		shaper_profile = TAILQ_FIRST(&p->soft.tm.h.shaper_profiles);
		if (shaper_profile == NULL)
			break;

		TAILQ_REMOVE(&p->soft.tm.h.shaper_profiles,
			shaper_profile, node);
		free(shaper_profile);
	}

	tm_hierarchy_init(p);
}

static struct tm_shaper_profile *
tm_shaper_profile_search(struct rte_eth_dev *dev, uint32_t shaper_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shaper_profile_list *spl = &p->soft.tm.h.shaper_profiles;
	struct tm_shaper_profile *sp;

	TAILQ_FOREACH(sp, spl, node)
		if (shaper_profile_id == sp->shaper_profile_id)
			return sp;

	return NULL;
}

static struct tm_shared_shaper *
tm_shared_shaper_search(struct rte_eth_dev *dev, uint32_t shared_shaper_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shared_shaper_list *ssl = &p->soft.tm.h.shared_shapers;
	struct tm_shared_shaper *ss;

	TAILQ_FOREACH(ss, ssl, node)
		if (shared_shaper_id == ss->shared_shaper_id)
			return ss;

	return NULL;
}

static struct tm_wred_profile *
tm_wred_profile_search(struct rte_eth_dev *dev, uint32_t wred_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_wred_profile_list *wpl = &p->soft.tm.h.wred_profiles;
	struct tm_wred_profile *wp;

	TAILQ_FOREACH(wp, wpl, node)
		if (wred_profile_id == wp->wred_profile_id)
			return wp;

	return NULL;
}

static struct tm_node *
tm_node_search(struct rte_eth_dev *dev, uint32_t node_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *n;

	TAILQ_FOREACH(n, nl, node)
		if (n->node_id == node_id)
			return n;

	return NULL;
}

static struct tm_node *
tm_root_node_present(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *n;

	TAILQ_FOREACH(n, nl, node)
		if (n->parent_node_id == RTE_TM_NODE_ID_NULL)
			return n;

	return NULL;
}

static uint32_t
tm_node_subport_id(struct rte_eth_dev *dev, struct tm_node *subport_node)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *ns;
	uint32_t subport_id;

	subport_id = 0;
	TAILQ_FOREACH(ns, nl, node) {
		if (ns->level != TM_NODE_LEVEL_SUBPORT)
			continue;

		if (ns->node_id == subport_node->node_id)
			return subport_id;

		subport_id++;
	}

	return UINT32_MAX;
}

static uint32_t
tm_node_pipe_id(struct rte_eth_dev *dev, struct tm_node *pipe_node)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *np;
	uint32_t pipe_id;

	pipe_id = 0;
	TAILQ_FOREACH(np, nl, node) {
		if (np->level != TM_NODE_LEVEL_PIPE ||
			np->parent_node_id != pipe_node->parent_node_id)
			continue;

		if (np->node_id == pipe_node->node_id)
			return pipe_id;

		pipe_id++;
	}

	return UINT32_MAX;
}

static uint32_t
tm_node_tc_id(struct rte_eth_dev *dev __rte_unused, struct tm_node *tc_node)
{
	return tc_node->priority;
}

static uint32_t
tm_node_queue_id(struct rte_eth_dev *dev, struct tm_node *queue_node)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *nq;
	uint32_t queue_id;

	queue_id = 0;
	TAILQ_FOREACH(nq, nl, node) {
		if (nq->level != TM_NODE_LEVEL_QUEUE ||
			nq->parent_node_id != queue_node->parent_node_id)
			continue;

		if (nq->node_id == queue_node->node_id)
			return queue_id;

		queue_id++;
	}

	return UINT32_MAX;
}

static uint32_t
tm_level_get_max_nodes(struct rte_eth_dev *dev, enum tm_node_level level)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t n_queues_max = p->params.tm.n_queues;
	uint32_t n_tc_max = n_queues_max / RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS;
	uint32_t n_pipes_max = n_tc_max / RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE;
	uint32_t n_subports_max = n_pipes_max;
	uint32_t n_root_max = 1;

	switch (level) {
	case TM_NODE_LEVEL_PORT:
		return n_root_max;
	case TM_NODE_LEVEL_SUBPORT:
		return n_subports_max;
	case TM_NODE_LEVEL_PIPE:
		return n_pipes_max;
	case TM_NODE_LEVEL_TC:
		return n_tc_max;
	case TM_NODE_LEVEL_QUEUE:
	default:
		return n_queues_max;
	}
}

/* Traffic manager node type get */
static int
pmd_tm_node_type_get(struct rte_eth_dev *dev,
	uint32_t node_id,
	int *is_leaf,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;

	if (is_leaf == NULL)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_UNSPECIFIED,
		   NULL,
		   rte_strerror(EINVAL));

	if (node_id == RTE_TM_NODE_ID_NULL ||
		(tm_node_search(dev, node_id) == NULL))
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_NODE_ID,
		   NULL,
		   rte_strerror(EINVAL));

	*is_leaf = node_id < p->params.tm.n_queues;

	return 0;
}

#ifdef RTE_SCHED_RED
#define WRED_SUPPORTED						1
#else
#define WRED_SUPPORTED						0
#endif

#define STATS_MASK_DEFAULT					\
	(RTE_TM_STATS_N_PKTS |					\
	RTE_TM_STATS_N_BYTES |					\
	RTE_TM_STATS_N_PKTS_GREEN_DROPPED |			\
	RTE_TM_STATS_N_BYTES_GREEN_DROPPED)

#define STATS_MASK_QUEUE						\
	(STATS_MASK_DEFAULT |					\
	RTE_TM_STATS_N_PKTS_QUEUED)

static const struct rte_tm_capabilities tm_cap = {
	.n_nodes_max = UINT32_MAX,
	.n_levels_max = TM_NODE_LEVEL_MAX,

	.non_leaf_nodes_identical = 0,
	.leaf_nodes_identical = 1,

	.shaper_n_max = UINT32_MAX,
	.shaper_private_n_max = UINT32_MAX,
	.shaper_private_dual_rate_n_max = 0,
	.shaper_private_rate_min = 1,
	.shaper_private_rate_max = UINT32_MAX,

	.shaper_shared_n_max = UINT32_MAX,
	.shaper_shared_n_nodes_per_shaper_max = UINT32_MAX,
	.shaper_shared_n_shapers_per_node_max = 1,
	.shaper_shared_dual_rate_n_max = 0,
	.shaper_shared_rate_min = 1,
	.shaper_shared_rate_max = UINT32_MAX,

	.shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD_FCS,
	.shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS,

	.sched_n_children_max = UINT32_MAX,
	.sched_sp_n_priorities_max = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
	.sched_wfq_n_children_per_group_max = UINT32_MAX,
	.sched_wfq_n_groups_max = 1,
	.sched_wfq_weight_max = UINT32_MAX,

	.cman_wred_packet_mode_supported = WRED_SUPPORTED,
	.cman_wred_byte_mode_supported = 0,
	.cman_head_drop_supported = 0,
	.cman_wred_context_n_max = 0,
	.cman_wred_context_private_n_max = 0,
	.cman_wred_context_shared_n_max = 0,
	.cman_wred_context_shared_n_nodes_per_context_max = 0,
	.cman_wred_context_shared_n_contexts_per_node_max = 0,

	.mark_vlan_dei_supported = {0, 0, 0},
	.mark_ip_ecn_tcp_supported = {0, 0, 0},
	.mark_ip_ecn_sctp_supported = {0, 0, 0},
	.mark_ip_dscp_supported = {0, 0, 0},

	.dynamic_update_mask = 0,

	.stats_mask = STATS_MASK_QUEUE,
};

/* Traffic manager capabilities get */
static int
pmd_tm_capabilities_get(struct rte_eth_dev *dev __rte_unused,
	struct rte_tm_capabilities *cap,
	struct rte_tm_error *error)
{
	if (cap == NULL)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_CAPABILITIES,
		   NULL,
		   rte_strerror(EINVAL));

	memcpy(cap, &tm_cap, sizeof(*cap));

	cap->n_nodes_max = tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PORT) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_SUBPORT) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PIPE) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_TC) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_QUEUE);

	cap->shaper_private_n_max =
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PORT) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_SUBPORT) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PIPE) +
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_TC);

	cap->shaper_shared_n_max = RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE *
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_SUBPORT);

	cap->shaper_n_max = cap->shaper_private_n_max +
		cap->shaper_shared_n_max;

	cap->shaper_shared_n_nodes_per_shaper_max =
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PIPE);

	cap->sched_n_children_max = RTE_MAX(
		tm_level_get_max_nodes(dev, TM_NODE_LEVEL_PIPE),
		(uint32_t)RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE);

	cap->sched_wfq_n_children_per_group_max = cap->sched_n_children_max;

	if (WRED_SUPPORTED)
		cap->cman_wred_context_private_n_max =
			tm_level_get_max_nodes(dev, TM_NODE_LEVEL_QUEUE);

	cap->cman_wred_context_n_max = cap->cman_wred_context_private_n_max +
		cap->cman_wred_context_shared_n_max;

	return 0;
}

static const struct rte_tm_level_capabilities tm_level_cap[] = {
	[TM_NODE_LEVEL_PORT] = {
		.n_nodes_max = 1,
		.n_nodes_nonleaf_max = 1,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		{.nonleaf = {
			.shaper_private_supported = 1,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 1,
			.shaper_private_rate_max = UINT32_MAX,
			.shaper_shared_n_max = 0,

			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = 1,

			.stats_mask = STATS_MASK_DEFAULT,
		} },
	},

	[TM_NODE_LEVEL_SUBPORT] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		{.nonleaf = {
			.shaper_private_supported = 1,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 1,
			.shaper_private_rate_max = UINT32_MAX,
			.shaper_shared_n_max = 0,

			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
#ifdef RTE_SCHED_SUBPORT_TC_OV
			.sched_wfq_weight_max = UINT32_MAX,
#else
			.sched_wfq_weight_max = 1,
#endif
			.stats_mask = STATS_MASK_DEFAULT,
		} },
	},

	[TM_NODE_LEVEL_PIPE] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		{.nonleaf = {
			.shaper_private_supported = 1,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 1,
			.shaper_private_rate_max = UINT32_MAX,
			.shaper_shared_n_max = 0,

			.sched_n_children_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_sp_n_priorities_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_wfq_n_children_per_group_max = 1,
			.sched_wfq_n_groups_max = 0,
			.sched_wfq_weight_max = 1,

			.stats_mask = STATS_MASK_DEFAULT,
		} },
	},

	[TM_NODE_LEVEL_TC] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		{.nonleaf = {
			.shaper_private_supported = 1,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 1,
			.shaper_private_rate_max = UINT32_MAX,
			.shaper_shared_n_max = 1,

			.sched_n_children_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = UINT32_MAX,

			.stats_mask = STATS_MASK_DEFAULT,
		} },
	},

	[TM_NODE_LEVEL_QUEUE] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = 0,
		.n_nodes_leaf_max = UINT32_MAX,
		.non_leaf_nodes_identical = 0,
		.leaf_nodes_identical = 1,

		{.leaf = {
			.shaper_private_supported = 0,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 0,
			.shaper_private_rate_max = 0,
			.shaper_shared_n_max = 0,

			.cman_head_drop_supported = 0,
			.cman_wred_packet_mode_supported = WRED_SUPPORTED,
			.cman_wred_byte_mode_supported = 0,
			.cman_wred_context_private_supported = WRED_SUPPORTED,
			.cman_wred_context_shared_n_max = 0,

			.stats_mask = STATS_MASK_QUEUE,
		} },
	},
};

/* Traffic manager level capabilities get */
static int
pmd_tm_level_capabilities_get(struct rte_eth_dev *dev __rte_unused,
	uint32_t level_id,
	struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error)
{
	if (cap == NULL)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_CAPABILITIES,
		   NULL,
		   rte_strerror(EINVAL));

	if (level_id >= TM_NODE_LEVEL_MAX)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_LEVEL_ID,
		   NULL,
		   rte_strerror(EINVAL));

	memcpy(cap, &tm_level_cap[level_id], sizeof(*cap));

	switch (level_id) {
	case TM_NODE_LEVEL_PORT:
		cap->nonleaf.sched_n_children_max =
			tm_level_get_max_nodes(dev,
				TM_NODE_LEVEL_SUBPORT);
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			cap->nonleaf.sched_n_children_max;
		break;

	case TM_NODE_LEVEL_SUBPORT:
		cap->n_nodes_max = tm_level_get_max_nodes(dev,
			TM_NODE_LEVEL_SUBPORT);
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		cap->nonleaf.sched_n_children_max =
			tm_level_get_max_nodes(dev,
				TM_NODE_LEVEL_PIPE);
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			cap->nonleaf.sched_n_children_max;
		break;

	case TM_NODE_LEVEL_PIPE:
		cap->n_nodes_max = tm_level_get_max_nodes(dev,
			TM_NODE_LEVEL_PIPE);
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		break;

	case TM_NODE_LEVEL_TC:
		cap->n_nodes_max = tm_level_get_max_nodes(dev,
			TM_NODE_LEVEL_TC);
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		break;

	case TM_NODE_LEVEL_QUEUE:
	default:
		cap->n_nodes_max = tm_level_get_max_nodes(dev,
			TM_NODE_LEVEL_QUEUE);
		cap->n_nodes_leaf_max = cap->n_nodes_max;
		break;
	}

	return 0;
}

static const struct rte_tm_node_capabilities tm_node_cap[] = {
	[TM_NODE_LEVEL_PORT] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 0,

		{.nonleaf = {
			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = 1,
		} },

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_SUBPORT] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 0,

		{.nonleaf = {
			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = UINT32_MAX,
		} },

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_PIPE] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 0,

		{.nonleaf = {
			.sched_n_children_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_sp_n_priorities_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_wfq_n_children_per_group_max = 1,
			.sched_wfq_n_groups_max = 0,
			.sched_wfq_weight_max = 1,
		} },

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_TC] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 1,

		{.nonleaf = {
			.sched_n_children_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = UINT32_MAX,
		} },

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_QUEUE] = {
		.shaper_private_supported = 0,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 0,
		.shaper_private_rate_max = 0,
		.shaper_shared_n_max = 0,


		{.leaf = {
			.cman_head_drop_supported = 0,
			.cman_wred_packet_mode_supported = WRED_SUPPORTED,
			.cman_wred_byte_mode_supported = 0,
			.cman_wred_context_private_supported = WRED_SUPPORTED,
			.cman_wred_context_shared_n_max = 0,
		} },

		.stats_mask = STATS_MASK_QUEUE,
	},
};

/* Traffic manager node capabilities get */
static int
pmd_tm_node_capabilities_get(struct rte_eth_dev *dev __rte_unused,
	uint32_t node_id,
	struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error)
{
	struct tm_node *tm_node;

	if (cap == NULL)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_CAPABILITIES,
		   NULL,
		   rte_strerror(EINVAL));

	tm_node = tm_node_search(dev, node_id);
	if (tm_node == NULL)
		return -rte_tm_error_set(error,
		   EINVAL,
		   RTE_TM_ERROR_TYPE_NODE_ID,
		   NULL,
		   rte_strerror(EINVAL));

	memcpy(cap, &tm_node_cap[tm_node->level], sizeof(*cap));

	switch (tm_node->level) {
	case TM_NODE_LEVEL_PORT:
		cap->nonleaf.sched_n_children_max =
			tm_level_get_max_nodes(dev,
				TM_NODE_LEVEL_SUBPORT);
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			cap->nonleaf.sched_n_children_max;
		break;

	case TM_NODE_LEVEL_SUBPORT:
		cap->nonleaf.sched_n_children_max =
			tm_level_get_max_nodes(dev,
				TM_NODE_LEVEL_PIPE);
		cap->nonleaf.sched_wfq_n_children_per_group_max =
			cap->nonleaf.sched_n_children_max;
		break;

	case TM_NODE_LEVEL_PIPE:
	case TM_NODE_LEVEL_TC:
	case TM_NODE_LEVEL_QUEUE:
	default:
		break;
	}

	return 0;
}

static int
shaper_profile_check(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error)
{
	struct tm_shaper_profile *sp;

	/* Shaper profile ID must not be NONE. */
	if (shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper profile must not exist. */
	sp = tm_shaper_profile_search(dev, shaper_profile_id);
	if (sp)
		return -rte_tm_error_set(error,
			EEXIST,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EEXIST));

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE,
			NULL,
			rte_strerror(EINVAL));

	/* Peak rate: non-zero, 32-bit */
	if (profile->peak.rate == 0 ||
		profile->peak.rate >= UINT32_MAX)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
			NULL,
			rte_strerror(EINVAL));

	/* Peak size: non-zero, 32-bit */
	if (profile->peak.size == 0 ||
		profile->peak.size >= UINT32_MAX)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
			NULL,
			rte_strerror(EINVAL));

	/* Dual-rate profiles are not supported. */
	if (profile->committed.rate != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE,
			NULL,
			rte_strerror(EINVAL));

	/* Packet length adjust: 24 bytes */
	if (profile->pkt_length_adjust != RTE_TM_ETH_FRAMING_OVERHEAD_FCS)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

/* Traffic manager shaper profile add */
static int
pmd_tm_shaper_profile_add(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shaper_profile_list *spl = &p->soft.tm.h.shaper_profiles;
	struct tm_shaper_profile *sp;
	int status;

	/* Check input params */
	status = shaper_profile_check(dev, shaper_profile_id, profile, error);
	if (status)
		return status;

	/* Memory allocation */
	sp = calloc(1, sizeof(struct tm_shaper_profile));
	if (sp == NULL)
		return -rte_tm_error_set(error,
			ENOMEM,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENOMEM));

	/* Fill in */
	sp->shaper_profile_id = shaper_profile_id;
	memcpy(&sp->params, profile, sizeof(sp->params));

	/* Add to list */
	TAILQ_INSERT_TAIL(spl, sp, node);
	p->soft.tm.h.n_shaper_profiles++;

	return 0;
}

/* Traffic manager shaper profile delete */
static int
pmd_tm_shaper_profile_delete(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shaper_profile *sp;

	/* Check existing */
	sp = tm_shaper_profile_search(dev, shaper_profile_id);
	if (sp == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Check unused */
	if (sp->n_users)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EBUSY));

	/* Remove from list */
	TAILQ_REMOVE(&p->soft.tm.h.shaper_profiles, sp, node);
	p->soft.tm.h.n_shaper_profiles--;
	free(sp);

	return 0;
}

static struct tm_node *
tm_shared_shaper_get_tc(struct rte_eth_dev *dev,
	struct tm_shared_shaper *ss)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *n;

	/* Subport: each TC uses shared shaper  */
	TAILQ_FOREACH(n, nl, node) {
		if (n->level != TM_NODE_LEVEL_TC ||
			n->params.n_shared_shapers == 0 ||
			n->params.shared_shaper_id[0] != ss->shared_shaper_id)
			continue;

		return n;
	}

	return NULL;
}

static int
update_subport_tc_rate(struct rte_eth_dev *dev,
	struct tm_node *nt,
	struct tm_shared_shaper *ss,
	struct tm_shaper_profile *sp_new)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t tc_id = tm_node_tc_id(dev, nt);

	struct tm_node *np = nt->parent_node;

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	struct rte_sched_subport_params subport_params;

	struct tm_shaper_profile *sp_old = tm_shaper_profile_search(dev,
		ss->shaper_profile_id);

	/* Derive new subport configuration. */
	memcpy(&subport_params,
		&p->soft.tm.params.subport_params[subport_id],
		sizeof(subport_params));
	subport_params.tc_rate[tc_id] = sp_new->params.peak.rate;

	/* Update the subport configuration. */
	if (rte_sched_subport_config(SCHED(p),
		subport_id, &subport_params))
		return -1;

	/* Commit changes. */
	sp_old->n_users--;

	ss->shaper_profile_id = sp_new->shaper_profile_id;
	sp_new->n_users++;

	memcpy(&p->soft.tm.params.subport_params[subport_id],
		&subport_params,
		sizeof(subport_params));

	return 0;
}

/* Traffic manager shared shaper add/update */
static int
pmd_tm_shared_shaper_add_update(struct rte_eth_dev *dev,
	uint32_t shared_shaper_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shared_shaper *ss;
	struct tm_shaper_profile *sp;
	struct tm_node *nt;

	/* Shaper profile must be valid. */
	sp = tm_shaper_profile_search(dev, shaper_profile_id);
	if (sp == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/**
	 * Add new shared shaper
	 */
	ss = tm_shared_shaper_search(dev, shared_shaper_id);
	if (ss == NULL) {
		struct tm_shared_shaper_list *ssl =
			&p->soft.tm.h.shared_shapers;

		/* Hierarchy must not be frozen */
		if (p->soft.tm.hierarchy_frozen)
			return -rte_tm_error_set(error,
				EBUSY,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EBUSY));

		/* Memory allocation */
		ss = calloc(1, sizeof(struct tm_shared_shaper));
		if (ss == NULL)
			return -rte_tm_error_set(error,
				ENOMEM,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(ENOMEM));

		/* Fill in */
		ss->shared_shaper_id = shared_shaper_id;
		ss->shaper_profile_id = shaper_profile_id;

		/* Add to list */
		TAILQ_INSERT_TAIL(ssl, ss, node);
		p->soft.tm.h.n_shared_shapers++;

		return 0;
	}

	/**
	 * Update existing shared shaper
	 */
	/* Hierarchy must be frozen (run-time update) */
	if (p->soft.tm.hierarchy_frozen == 0)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));


	/* Propagate change. */
	nt = tm_shared_shaper_get_tc(dev, ss);
	if (update_subport_tc_rate(dev, nt, ss, sp))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

/* Traffic manager shared shaper delete */
static int
pmd_tm_shared_shaper_delete(struct rte_eth_dev *dev,
	uint32_t shared_shaper_id,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shared_shaper *ss;

	/* Check existing */
	ss = tm_shared_shaper_search(dev, shared_shaper_id);
	if (ss == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHARED_SHAPER_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Check unused */
	if (ss->n_users)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_SHARED_SHAPER_ID,
			NULL,
			rte_strerror(EBUSY));

	/* Remove from list */
	TAILQ_REMOVE(&p->soft.tm.h.shared_shapers, ss, node);
	p->soft.tm.h.n_shared_shapers--;
	free(ss);

	return 0;
}

static int
wred_profile_check(struct rte_eth_dev *dev,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	struct tm_wred_profile *wp;
	enum rte_tm_color color;

	/* WRED profile ID must not be NONE. */
	if (wred_profile_id == RTE_TM_WRED_PROFILE_ID_NONE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* WRED profile must not exist. */
	wp = tm_wred_profile_search(dev, wred_profile_id);
	if (wp)
		return -rte_tm_error_set(error,
			EEXIST,
			RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
			NULL,
			rte_strerror(EEXIST));

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_WRED_PROFILE,
			NULL,
			rte_strerror(EINVAL));

        /* WRED profile should be in packet mode */
        if (profile->packet_mode == 0)
                return -rte_tm_error_set(error,
                        ENOTSUP,
                        RTE_TM_ERROR_TYPE_WRED_PROFILE,
                        NULL,
                        rte_strerror(ENOTSUP));

	/* min_th <= max_th, max_th > 0  */
	for (color = RTE_TM_GREEN; color < RTE_TM_COLORS; color++) {
		uint32_t min_th = profile->red_params[color].min_th;
		uint32_t max_th = profile->red_params[color].max_th;

		if (min_th > max_th ||
			max_th == 0 ||
			min_th > UINT16_MAX ||
			max_th > UINT16_MAX)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_WRED_PROFILE,
				NULL,
				rte_strerror(EINVAL));
	}

	return 0;
}

/* Traffic manager WRED profile add */
static int
pmd_tm_wred_profile_add(struct rte_eth_dev *dev,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_wred_profile_list *wpl = &p->soft.tm.h.wred_profiles;
	struct tm_wred_profile *wp;
	int status;

	/* Check input params */
	status = wred_profile_check(dev, wred_profile_id, profile, error);
	if (status)
		return status;

	/* Memory allocation */
	wp = calloc(1, sizeof(struct tm_wred_profile));
	if (wp == NULL)
		return -rte_tm_error_set(error,
			ENOMEM,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENOMEM));

	/* Fill in */
	wp->wred_profile_id = wred_profile_id;
	memcpy(&wp->params, profile, sizeof(wp->params));

	/* Add to list */
	TAILQ_INSERT_TAIL(wpl, wp, node);
	p->soft.tm.h.n_wred_profiles++;

	return 0;
}

/* Traffic manager WRED profile delete */
static int
pmd_tm_wred_profile_delete(struct rte_eth_dev *dev,
	uint32_t wred_profile_id,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_wred_profile *wp;

	/* Check existing */
	wp = tm_wred_profile_search(dev, wred_profile_id);
	if (wp == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Check unused */
	if (wp->n_users)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
			NULL,
			rte_strerror(EBUSY));

	/* Remove from list */
	TAILQ_REMOVE(&p->soft.tm.h.wred_profiles, wp, node);
	p->soft.tm.h.n_wred_profiles--;
	free(wp);

	return 0;
}

static int
node_add_check_port(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id __rte_unused,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id __rte_unused,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_shaper_profile *sp = tm_shaper_profile_search(dev,
		params->shaper_profile_id);

	/* node type: non-leaf */
	if (node_id < p->params.tm.n_queues)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Priority must be 0 */
	if (priority != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* Weight must be 1 */
	if (weight != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper must be valid */
	if (params->shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE ||
		sp == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* No shared shapers */
	if (params->n_shared_shapers != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
			NULL,
			rte_strerror(EINVAL));

	/* Number of SP priorities must be 1 */
	if (params->nonleaf.n_sp_priorities != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
			NULL,
			rte_strerror(EINVAL));

	/* Stats */
	if (params->stats_mask & ~STATS_MASK_DEFAULT)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

static int
node_add_check_subport(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id __rte_unused,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id __rte_unused,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;

	/* node type: non-leaf */
	if (node_id < p->params.tm.n_queues)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Priority must be 0 */
	if (priority != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* Weight must be 1 */
	if (weight != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper must be valid */
	if (params->shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE ||
		(!tm_shaper_profile_search(dev, params->shaper_profile_id)))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* No shared shapers */
	if (params->n_shared_shapers != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
			NULL,
			rte_strerror(EINVAL));

	/* Number of SP priorities must be 1 */
	if (params->nonleaf.n_sp_priorities != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
			NULL,
			rte_strerror(EINVAL));

	/* Stats */
	if (params->stats_mask & ~STATS_MASK_DEFAULT)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

static int
node_add_check_pipe(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id __rte_unused,
	uint32_t priority,
	uint32_t weight __rte_unused,
	uint32_t level_id __rte_unused,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;

	/* node type: non-leaf */
	if (node_id < p->params.tm.n_queues)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Priority must be 0 */
	if (priority != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper must be valid */
	if (params->shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE ||
		(!tm_shaper_profile_search(dev, params->shaper_profile_id)))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* No shared shapers */
	if (params->n_shared_shapers != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
			NULL,
			rte_strerror(EINVAL));

	/* Number of SP priorities must be 4 */
	if (params->nonleaf.n_sp_priorities !=
		RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
			NULL,
			rte_strerror(EINVAL));

	/* WFQ mode must be byte mode */
	if (params->nonleaf.wfq_weight_mode != NULL &&
		params->nonleaf.wfq_weight_mode[0] != 0 &&
		params->nonleaf.wfq_weight_mode[1] != 0 &&
		params->nonleaf.wfq_weight_mode[2] != 0 &&
		params->nonleaf.wfq_weight_mode[3] != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE,
			NULL,
			rte_strerror(EINVAL));

	/* Stats */
	if (params->stats_mask & ~STATS_MASK_DEFAULT)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

static int
node_add_check_tc(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id __rte_unused,
	uint32_t priority __rte_unused,
	uint32_t weight,
	uint32_t level_id __rte_unused,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;

	/* node type: non-leaf */
	if (node_id < p->params.tm.n_queues)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Weight must be 1 */
	if (weight != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper must be valid */
	if (params->shaper_profile_id == RTE_TM_SHAPER_PROFILE_ID_NONE ||
		(!tm_shaper_profile_search(dev, params->shaper_profile_id)))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Single valid shared shaper */
	if (params->n_shared_shapers > 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
			NULL,
			rte_strerror(EINVAL));

	if (params->n_shared_shapers == 1 &&
		(params->shared_shaper_id == NULL ||
		(!tm_shared_shaper_search(dev, params->shared_shaper_id[0]))))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Number of priorities must be 1 */
	if (params->nonleaf.n_sp_priorities != 1)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
			NULL,
			rte_strerror(EINVAL));

	/* Stats */
	if (params->stats_mask & ~STATS_MASK_DEFAULT)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

static int
node_add_check_queue(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id __rte_unused,
	uint32_t priority,
	uint32_t weight __rte_unused,
	uint32_t level_id __rte_unused,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;

	/* node type: leaf */
	if (node_id >= p->params.tm.n_queues)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Priority must be 0 */
	if (priority != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* No shaper */
	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* No shared shapers */
	if (params->n_shared_shapers != 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
			NULL,
			rte_strerror(EINVAL));

	/* Congestion management must not be head drop */
	if (params->leaf.cman == RTE_TM_CMAN_HEAD_DROP)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN,
			NULL,
			rte_strerror(EINVAL));

	/* Congestion management set to WRED */
	if (params->leaf.cman == RTE_TM_CMAN_WRED) {
		uint32_t wred_profile_id = params->leaf.wred.wred_profile_id;
		struct tm_wred_profile *wp = tm_wred_profile_search(dev,
			wred_profile_id);

		/* WRED profile (for private WRED context) must be valid */
		if (wred_profile_id == RTE_TM_WRED_PROFILE_ID_NONE ||
			wp == NULL)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID,
				NULL,
				rte_strerror(EINVAL));

		/* No shared WRED contexts */
		if (params->leaf.wred.n_shared_wred_contexts != 0)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS,
				NULL,
				rte_strerror(EINVAL));
	}

	/* Stats */
	if (params->stats_mask & ~STATS_MASK_QUEUE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
			NULL,
			rte_strerror(EINVAL));

	return 0;
}

static int
node_add_check(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct tm_node *pn;
	uint32_t level;
	int status;

	/* node_id, parent_node_id:
	 *    -node_id must not be RTE_TM_NODE_ID_NULL
	 *    -node_id must not be in use
	 *    -root node add (parent_node_id is RTE_TM_NODE_ID_NULL):
	 *        -root node must not exist
	 *    -non-root node add (parent_node_id is not RTE_TM_NODE_ID_NULL):
	 *        -parent_node_id must be valid
	 */
	if (node_id == RTE_TM_NODE_ID_NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	if (tm_node_search(dev, node_id))
		return -rte_tm_error_set(error,
			EEXIST,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EEXIST));

	if (parent_node_id == RTE_TM_NODE_ID_NULL) {
		pn = NULL;
		if (tm_root_node_present(dev))
			return -rte_tm_error_set(error,
				EEXIST,
				RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
				NULL,
				rte_strerror(EEXIST));
	} else {
		pn = tm_node_search(dev, parent_node_id);
		if (pn == NULL)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
				NULL,
				rte_strerror(EINVAL));
	}

	/* priority: must be 0 .. 3 */
	if (priority >= RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* weight: must be 1 .. 255 */
	if (weight == 0 || weight >= UINT8_MAX)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));

	/* level_id: if valid, then
	 *    -root node add (parent_node_id is RTE_TM_NODE_ID_NULL):
	 *        -level_id must be zero
	 *    -non-root node add (parent_node_id is not RTE_TM_NODE_ID_NULL):
	 *        -level_id must be parent level ID plus one
	 */
	level = (pn == NULL) ? 0 : pn->level + 1;
	if (level_id != RTE_TM_NODE_LEVEL_ID_ANY && level_id != level)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_LEVEL_ID,
			NULL,
			rte_strerror(EINVAL));

	/* params: must not be NULL */
	if (params == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARAMS,
			NULL,
			rte_strerror(EINVAL));

	/* params: per level checks */
	switch (level) {
	case TM_NODE_LEVEL_PORT:
		status = node_add_check_port(dev, node_id,
			parent_node_id, priority, weight, level_id,
			params, error);
		if (status)
			return status;
		break;

	case TM_NODE_LEVEL_SUBPORT:
		status = node_add_check_subport(dev, node_id,
			parent_node_id, priority, weight, level_id,
			params, error);
		if (status)
			return status;
		break;

	case TM_NODE_LEVEL_PIPE:
		status = node_add_check_pipe(dev, node_id,
			parent_node_id, priority, weight, level_id,
			params, error);
		if (status)
			return status;
		break;

	case TM_NODE_LEVEL_TC:
		status = node_add_check_tc(dev, node_id,
			parent_node_id, priority, weight, level_id,
			params, error);
		if (status)
			return status;
		break;

	case TM_NODE_LEVEL_QUEUE:
		status = node_add_check_queue(dev, node_id,
			parent_node_id, priority, weight, level_id,
			params, error);
		if (status)
			return status;
		break;

	default:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_LEVEL_ID,
			NULL,
			rte_strerror(EINVAL));
	}

	return 0;
}

/* Traffic manager node add */
static int
pmd_tm_node_add(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *n;
	uint32_t i;
	int status;

	/* Checks */
	if (p->soft.tm.hierarchy_frozen)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	status = node_add_check(dev, node_id, parent_node_id, priority, weight,
		level_id, params, error);
	if (status)
		return status;

	/* Memory allocation */
	n = calloc(1, sizeof(struct tm_node));
	if (n == NULL)
		return -rte_tm_error_set(error,
			ENOMEM,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENOMEM));

	/* Fill in */
	n->node_id = node_id;
	n->parent_node_id = parent_node_id;
	n->priority = priority;
	n->weight = weight;

	if (parent_node_id != RTE_TM_NODE_ID_NULL) {
		n->parent_node = tm_node_search(dev, parent_node_id);
		n->level = n->parent_node->level + 1;
	}

	if (params->shaper_profile_id != RTE_TM_SHAPER_PROFILE_ID_NONE)
		n->shaper_profile = tm_shaper_profile_search(dev,
			params->shaper_profile_id);

	if (n->level == TM_NODE_LEVEL_QUEUE &&
		params->leaf.cman == RTE_TM_CMAN_WRED)
		n->wred_profile = tm_wred_profile_search(dev,
			params->leaf.wred.wred_profile_id);

	memcpy(&n->params, params, sizeof(n->params));

	/* Add to list */
	TAILQ_INSERT_TAIL(nl, n, node);
	p->soft.tm.h.n_nodes++;

	/* Update dependencies */
	if (n->parent_node)
		n->parent_node->n_children++;

	if (n->shaper_profile)
		n->shaper_profile->n_users++;

	for (i = 0; i < params->n_shared_shapers; i++) {
		struct tm_shared_shaper *ss;

		ss = tm_shared_shaper_search(dev, params->shared_shaper_id[i]);
		ss->n_users++;
	}

	if (n->wred_profile)
		n->wred_profile->n_users++;

	p->soft.tm.h.n_tm_nodes[n->level]++;

	return 0;
}

/* Traffic manager node delete */
static int
pmd_tm_node_delete(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node *n;
	uint32_t i;

	/* Check hierarchy changes are currently allowed */
	if (p->soft.tm.hierarchy_frozen)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	/* Check existing */
	n = tm_node_search(dev, node_id);
	if (n == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Check unused */
	if (n->n_children)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EBUSY));

	/* Update dependencies */
	p->soft.tm.h.n_tm_nodes[n->level]--;

	if (n->wred_profile)
		n->wred_profile->n_users--;

	for (i = 0; i < n->params.n_shared_shapers; i++) {
		struct tm_shared_shaper *ss;

		ss = tm_shared_shaper_search(dev,
				n->params.shared_shaper_id[i]);
		ss->n_users--;
	}

	if (n->shaper_profile)
		n->shaper_profile->n_users--;

	if (n->parent_node)
		n->parent_node->n_children--;

	/* Remove from list */
	TAILQ_REMOVE(&p->soft.tm.h.nodes, n, node);
	p->soft.tm.h.n_nodes--;
	free(n);

	return 0;
}


static void
pipe_profile_build(struct rte_eth_dev *dev,
	struct tm_node *np,
	struct rte_sched_pipe_params *pp)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_node_list *nl = &h->nodes;
	struct tm_node *nt, *nq;

	memset(pp, 0, sizeof(*pp));

	/* Pipe */
	pp->tb_rate = np->shaper_profile->params.peak.rate;
	pp->tb_size = np->shaper_profile->params.peak.size;

	/* Traffic Class (TC) */
	pp->tc_period = PIPE_TC_PERIOD;

#ifdef RTE_SCHED_SUBPORT_TC_OV
	pp->tc_ov_weight = np->weight;
#endif

	TAILQ_FOREACH(nt, nl, node) {
		uint32_t queue_id = 0;

		if (nt->level != TM_NODE_LEVEL_TC ||
			nt->parent_node_id != np->node_id)
			continue;

		pp->tc_rate[nt->priority] =
			nt->shaper_profile->params.peak.rate;

		/* Queue */
		TAILQ_FOREACH(nq, nl, node) {
			uint32_t pipe_queue_id;

			if (nq->level != TM_NODE_LEVEL_QUEUE ||
				nq->parent_node_id != nt->node_id)
				continue;

			pipe_queue_id = nt->priority *
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + queue_id;
			pp->wrr_weights[pipe_queue_id] = nq->weight;

			queue_id++;
		}
	}
}

static int
pipe_profile_free_exists(struct rte_eth_dev *dev,
	uint32_t *pipe_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_params *t = &p->soft.tm.params;

	if (t->n_pipe_profiles < RTE_SCHED_PIPE_PROFILES_PER_PORT) {
		*pipe_profile_id = t->n_pipe_profiles;
		return 1;
	}

	return 0;
}

static int
pipe_profile_exists(struct rte_eth_dev *dev,
	struct rte_sched_pipe_params *pp,
	uint32_t *pipe_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_params *t = &p->soft.tm.params;
	uint32_t i;

	for (i = 0; i < t->n_pipe_profiles; i++)
		if (memcmp(&t->pipe_profiles[i], pp, sizeof(*pp)) == 0) {
			if (pipe_profile_id)
				*pipe_profile_id = i;
			return 1;
		}

	return 0;
}

static void
pipe_profile_install(struct rte_eth_dev *dev,
	struct rte_sched_pipe_params *pp,
	uint32_t pipe_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_params *t = &p->soft.tm.params;

	memcpy(&t->pipe_profiles[pipe_profile_id], pp, sizeof(*pp));
	t->n_pipe_profiles++;
}

static void
pipe_profile_mark(struct rte_eth_dev *dev,
	uint32_t subport_id,
	uint32_t pipe_id,
	uint32_t pipe_profile_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_params *t = &p->soft.tm.params;
	uint32_t n_pipes_per_subport, pos;

	n_pipes_per_subport = h->n_tm_nodes[TM_NODE_LEVEL_PIPE] /
		h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT];
	pos = subport_id * n_pipes_per_subport + pipe_id;

	t->pipe_to_profile[pos] = pipe_profile_id;
}

static struct rte_sched_pipe_params *
pipe_profile_get(struct rte_eth_dev *dev, struct tm_node *np)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_params *t = &p->soft.tm.params;
	uint32_t n_pipes_per_subport = h->n_tm_nodes[TM_NODE_LEVEL_PIPE] /
		h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT];

	uint32_t subport_id = tm_node_subport_id(dev, np->parent_node);
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	uint32_t pos = subport_id * n_pipes_per_subport + pipe_id;
	uint32_t pipe_profile_id = t->pipe_to_profile[pos];

	return &t->pipe_profiles[pipe_profile_id];
}

static int
pipe_profiles_generate(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_node_list *nl = &h->nodes;
	struct tm_node *ns, *np;
	uint32_t subport_id;

	/* Objective: Fill in the following fields in struct tm_params:
	 *    - pipe_profiles
	 *    - n_pipe_profiles
	 *    - pipe_to_profile
	 */

	subport_id = 0;
	TAILQ_FOREACH(ns, nl, node) {
		uint32_t pipe_id;

		if (ns->level != TM_NODE_LEVEL_SUBPORT)
			continue;

		pipe_id = 0;
		TAILQ_FOREACH(np, nl, node) {
			struct rte_sched_pipe_params pp;
			uint32_t pos;

			if (np->level != TM_NODE_LEVEL_PIPE ||
				np->parent_node_id != ns->node_id)
				continue;

			pipe_profile_build(dev, np, &pp);

			if (!pipe_profile_exists(dev, &pp, &pos)) {
				if (!pipe_profile_free_exists(dev, &pos))
					return -1;

				pipe_profile_install(dev, &pp, pos);
			}

			pipe_profile_mark(dev, subport_id, pipe_id, pos);

			pipe_id++;
		}

		subport_id++;
	}

	return 0;
}

static struct tm_wred_profile *
tm_tc_wred_profile_get(struct rte_eth_dev *dev, uint32_t tc_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_node_list *nl = &h->nodes;
	struct tm_node *nq;

	TAILQ_FOREACH(nq, nl, node) {
		if (nq->level != TM_NODE_LEVEL_QUEUE ||
			nq->parent_node->priority != tc_id)
			continue;

		return nq->wred_profile;
	}

	return NULL;
}

#ifdef RTE_SCHED_RED

static void
wred_profiles_set(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct rte_sched_port_params *pp = &p->soft.tm.params.port_params;
	uint32_t tc_id;
	enum rte_tm_color color;

	for (tc_id = 0; tc_id < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; tc_id++)
		for (color = RTE_TM_GREEN; color < RTE_TM_COLORS; color++) {
			struct rte_red_params *dst =
				&pp->red_params[tc_id][color];
			struct tm_wred_profile *src_wp =
				tm_tc_wred_profile_get(dev, tc_id);
			struct rte_tm_red_params *src =
				&src_wp->params.red_params[color];

			memcpy(dst, src, sizeof(*dst));
		}
}

#else

#define wred_profiles_set(dev)

#endif

static struct tm_shared_shaper *
tm_tc_shared_shaper_get(struct rte_eth_dev *dev, struct tm_node *tc_node)
{
	return (tc_node->params.n_shared_shapers) ?
		tm_shared_shaper_search(dev,
			tc_node->params.shared_shaper_id[0]) :
		NULL;
}

static struct tm_shared_shaper *
tm_subport_tc_shared_shaper_get(struct rte_eth_dev *dev,
	struct tm_node *subport_node,
	uint32_t tc_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_node_list *nl = &p->soft.tm.h.nodes;
	struct tm_node *n;

	TAILQ_FOREACH(n, nl, node) {
		if (n->level != TM_NODE_LEVEL_TC ||
			n->parent_node->parent_node_id !=
				subport_node->node_id ||
			n->priority != tc_id)
			continue;

		return tm_tc_shared_shaper_get(dev, n);
	}

	return NULL;
}

static int
hierarchy_commit_check(struct rte_eth_dev *dev, struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	struct tm_node_list *nl = &h->nodes;
	struct tm_shared_shaper_list *ssl = &h->shared_shapers;
	struct tm_wred_profile_list *wpl = &h->wred_profiles;
	struct tm_node *nr = tm_root_node_present(dev), *ns, *np, *nt, *nq;
	struct tm_shared_shaper *ss;

	uint32_t n_pipes_per_subport;

	/* Root node exists. */
	if (nr == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_LEVEL_ID,
			NULL,
			rte_strerror(EINVAL));

	/* There is at least one subport, max is not exceeded. */
	if (nr->n_children == 0 || nr->n_children > TM_MAX_SUBPORTS)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_LEVEL_ID,
			NULL,
			rte_strerror(EINVAL));

	/* There is at least one pipe. */
	if (h->n_tm_nodes[TM_NODE_LEVEL_PIPE] == 0)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_LEVEL_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Number of pipes is the same for all subports. Maximum number of pipes
	 * per subport is not exceeded.
	 */
	n_pipes_per_subport = h->n_tm_nodes[TM_NODE_LEVEL_PIPE] /
		h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT];

	if (n_pipes_per_subport > TM_MAX_PIPES_PER_SUBPORT)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));

	TAILQ_FOREACH(ns, nl, node) {
		if (ns->level != TM_NODE_LEVEL_SUBPORT)
			continue;

		if (ns->n_children != n_pipes_per_subport)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
	}

	/* Each pipe has exactly 4 TCs, with exactly one TC for each priority */
	TAILQ_FOREACH(np, nl, node) {
		uint32_t mask = 0, mask_expected =
			RTE_LEN2MASK(RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
				uint32_t);

		if (np->level != TM_NODE_LEVEL_PIPE)
			continue;

		if (np->n_children != RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));

		TAILQ_FOREACH(nt, nl, node) {
			if (nt->level != TM_NODE_LEVEL_TC ||
				nt->parent_node_id != np->node_id)
				continue;

			mask |= 1 << nt->priority;
		}

		if (mask != mask_expected)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
	}

	/* Each TC has exactly 4 packet queues. */
	TAILQ_FOREACH(nt, nl, node) {
		if (nt->level != TM_NODE_LEVEL_TC)
			continue;

		if (nt->n_children != RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
	}

	/**
	 * Shared shapers:
	 *    -For each TC #i, all pipes in the same subport use the same
	 *     shared shaper (or no shared shaper) for their TC#i.
	 *    -Each shared shaper needs to have at least one user. All its
	 *     users have to be TC nodes with the same priority and the same
	 *     subport.
	 */
	TAILQ_FOREACH(ns, nl, node) {
		struct tm_shared_shaper *s[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
		uint32_t id;

		if (ns->level != TM_NODE_LEVEL_SUBPORT)
			continue;

		for (id = 0; id < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; id++)
			s[id] = tm_subport_tc_shared_shaper_get(dev, ns, id);

		TAILQ_FOREACH(nt, nl, node) {
			struct tm_shared_shaper *subport_ss, *tc_ss;

			if (nt->level != TM_NODE_LEVEL_TC ||
				nt->parent_node->parent_node_id !=
					ns->node_id)
				continue;

			subport_ss = s[nt->priority];
			tc_ss = tm_tc_shared_shaper_get(dev, nt);

			if (subport_ss == NULL && tc_ss == NULL)
				continue;

			if ((subport_ss == NULL && tc_ss != NULL) ||
				(subport_ss != NULL && tc_ss == NULL) ||
				subport_ss->shared_shaper_id !=
					tc_ss->shared_shaper_id)
				return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));
		}
	}

	TAILQ_FOREACH(ss, ssl, node) {
		struct tm_node *nt_any = tm_shared_shaper_get_tc(dev, ss);
		uint32_t n_users = 0;

		if (nt_any != NULL)
			TAILQ_FOREACH(nt, nl, node) {
				if (nt->level != TM_NODE_LEVEL_TC ||
					nt->priority != nt_any->priority ||
					nt->parent_node->parent_node_id !=
					nt_any->parent_node->parent_node_id)
					continue;

				n_users++;
			}

		if (ss->n_users == 0 || ss->n_users != n_users)
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
	}

	/* Not too many pipe profiles. */
	if (pipe_profiles_generate(dev))
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));

	/**
	 * WRED (when used, i.e. at least one WRED profile defined):
	 *    -Each WRED profile must have at least one user.
	 *    -All leaf nodes must have their private WRED context enabled.
	 *    -For each TC #i, all leaf nodes must use the same WRED profile
	 *     for their private WRED context.
	 */
	if (h->n_wred_profiles) {
		struct tm_wred_profile *wp;
		struct tm_wred_profile *w[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
		uint32_t id;

		TAILQ_FOREACH(wp, wpl, node)
			if (wp->n_users == 0)
				return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));

		for (id = 0; id < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; id++) {
			w[id] = tm_tc_wred_profile_get(dev, id);

			if (w[id] == NULL)
				return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));
		}

		TAILQ_FOREACH(nq, nl, node) {
			uint32_t id;

			if (nq->level != TM_NODE_LEVEL_QUEUE)
				continue;

			id = nq->parent_node->priority;

			if (nq->wred_profile == NULL ||
				nq->wred_profile->wred_profile_id !=
					w[id]->wred_profile_id)
				return -rte_tm_error_set(error,
					EINVAL,
					RTE_TM_ERROR_TYPE_UNSPECIFIED,
					NULL,
					rte_strerror(EINVAL));
		}
	}

	return 0;
}

static void
hierarchy_blueprints_create(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_params *t = &p->soft.tm.params;
	struct tm_hierarchy *h = &p->soft.tm.h;

	struct tm_node_list *nl = &h->nodes;
	struct tm_node *root = tm_root_node_present(dev), *n;

	uint32_t subport_id;

	t->port_params = (struct rte_sched_port_params) {
		.name = dev->data->name,
		.socket = dev->data->numa_node,
		.rate = root->shaper_profile->params.peak.rate,
		.mtu = dev->data->mtu,
		.frame_overhead =
			root->shaper_profile->params.pkt_length_adjust,
		.n_subports_per_port = root->n_children,
		.n_pipes_per_subport = h->n_tm_nodes[TM_NODE_LEVEL_PIPE] /
			h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT],
		.qsize = {p->params.tm.qsize[0],
			p->params.tm.qsize[1],
			p->params.tm.qsize[2],
			p->params.tm.qsize[3],
		},
		.pipe_profiles = t->pipe_profiles,
		.n_pipe_profiles = t->n_pipe_profiles,
	};

	wred_profiles_set(dev);

	subport_id = 0;
	TAILQ_FOREACH(n, nl, node) {
		uint64_t tc_rate[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE];
		uint32_t i;

		if (n->level != TM_NODE_LEVEL_SUBPORT)
			continue;

		for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
			struct tm_shared_shaper *ss;
			struct tm_shaper_profile *sp;

			ss = tm_subport_tc_shared_shaper_get(dev, n, i);
			sp = (ss) ? tm_shaper_profile_search(dev,
				ss->shaper_profile_id) :
				n->shaper_profile;
			tc_rate[i] = sp->params.peak.rate;
		}

		t->subport_params[subport_id] =
			(struct rte_sched_subport_params) {
				.tb_rate = n->shaper_profile->params.peak.rate,
				.tb_size = n->shaper_profile->params.peak.size,

				.tc_rate = {tc_rate[0],
					tc_rate[1],
					tc_rate[2],
					tc_rate[3],
			},
			.tc_period = SUBPORT_TC_PERIOD,
		};

		subport_id++;
	}
}

/* Traffic manager hierarchy commit */
static int
pmd_tm_hierarchy_commit(struct rte_eth_dev *dev,
	int clear_on_fail,
	struct rte_tm_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	int status;

	/* Checks */
	if (p->soft.tm.hierarchy_frozen)
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	status = hierarchy_commit_check(dev, error);
	if (status) {
		if (clear_on_fail)
			tm_hierarchy_free(p);

		return status;
	}

	/* Create blueprints */
	hierarchy_blueprints_create(dev);

	/* Freeze hierarchy */
	p->soft.tm.hierarchy_frozen = 1;

	return 0;
}

#ifdef RTE_SCHED_SUBPORT_TC_OV

static int
update_pipe_weight(struct rte_eth_dev *dev, struct tm_node *np, uint32_t weight)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	struct rte_sched_pipe_params *profile0 = pipe_profile_get(dev, np);
	struct rte_sched_pipe_params profile1;
	uint32_t pipe_profile_id;

	/* Derive new pipe profile. */
	memcpy(&profile1, profile0, sizeof(profile1));
	profile1.tc_ov_weight = (uint8_t)weight;

	/* Since implementation does not allow adding more pipe profiles after
	 * port configuration, the pipe configuration can be successfully
	 * updated only if the new profile is also part of the existing set of
	 * pipe profiles.
	 */
	if (pipe_profile_exists(dev, &profile1, &pipe_profile_id) == 0)
		return -1;

	/* Update the pipe profile used by the current pipe. */
	if (rte_sched_pipe_config(SCHED(p), subport_id, pipe_id,
		(int32_t)pipe_profile_id))
		return -1;

	/* Commit changes. */
	pipe_profile_mark(dev, subport_id, pipe_id, pipe_profile_id);
	np->weight = weight;

	return 0;
}

#endif

static int
update_queue_weight(struct rte_eth_dev *dev,
	struct tm_node *nq, uint32_t weight)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t queue_id = tm_node_queue_id(dev, nq);

	struct tm_node *nt = nq->parent_node;
	uint32_t tc_id = tm_node_tc_id(dev, nt);

	struct tm_node *np = nt->parent_node;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	uint32_t pipe_queue_id =
		tc_id * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + queue_id;

	struct rte_sched_pipe_params *profile0 = pipe_profile_get(dev, np);
	struct rte_sched_pipe_params profile1;
	uint32_t pipe_profile_id;

	/* Derive new pipe profile. */
	memcpy(&profile1, profile0, sizeof(profile1));
	profile1.wrr_weights[pipe_queue_id] = (uint8_t)weight;

	/* Since implementation does not allow adding more pipe profiles after
	 * port configuration, the pipe configuration can be successfully
	 * updated only if the new profile is also part of the existing set
	 * of pipe profiles.
	 */
	if (pipe_profile_exists(dev, &profile1, &pipe_profile_id) == 0)
		return -1;

	/* Update the pipe profile used by the current pipe. */
	if (rte_sched_pipe_config(SCHED(p), subport_id, pipe_id,
		(int32_t)pipe_profile_id))
		return -1;

	/* Commit changes. */
	pipe_profile_mark(dev, subport_id, pipe_id, pipe_profile_id);
	nq->weight = weight;

	return 0;
}

/* Traffic manager node parent update */
static int
pmd_tm_node_parent_update(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	struct rte_tm_error *error)
{
	struct tm_node *n;

	/* Port must be started and TM used. */
	if (dev->data->dev_started == 0 && (tm_used(dev) == 0))
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	/* Node must be valid */
	n = tm_node_search(dev, node_id);
	if (n == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Parent node must be the same */
	if (n->parent_node_id != parent_node_id)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Priority must be the same */
	if (n->priority != priority)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_PRIORITY,
			NULL,
			rte_strerror(EINVAL));

	/* weight: must be 1 .. 255 */
	if (weight == 0 || weight >= UINT8_MAX)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));

	switch (n->level) {
	case TM_NODE_LEVEL_PORT:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));
		/* fall-through */
	case TM_NODE_LEVEL_SUBPORT:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));
		/* fall-through */
	case TM_NODE_LEVEL_PIPE:
#ifdef RTE_SCHED_SUBPORT_TC_OV
		if (update_pipe_weight(dev, n, weight))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
#else
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));
#endif
		/* fall-through */
	case TM_NODE_LEVEL_TC:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_WEIGHT,
			NULL,
			rte_strerror(EINVAL));
		/* fall-through */
	case TM_NODE_LEVEL_QUEUE:
		/* fall-through */
	default:
		if (update_queue_weight(dev, n, weight))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
	}
}

static int
update_subport_rate(struct rte_eth_dev *dev,
	struct tm_node *ns,
	struct tm_shaper_profile *sp)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	struct rte_sched_subport_params subport_params;

	/* Derive new subport configuration. */
	memcpy(&subport_params,
		&p->soft.tm.params.subport_params[subport_id],
		sizeof(subport_params));
	subport_params.tb_rate = sp->params.peak.rate;
	subport_params.tb_size = sp->params.peak.size;

	/* Update the subport configuration. */
	if (rte_sched_subport_config(SCHED(p), subport_id,
		&subport_params))
		return -1;

	/* Commit changes. */
	ns->shaper_profile->n_users--;

	ns->shaper_profile = sp;
	ns->params.shaper_profile_id = sp->shaper_profile_id;
	sp->n_users++;

	memcpy(&p->soft.tm.params.subport_params[subport_id],
		&subport_params,
		sizeof(subport_params));

	return 0;
}

static int
update_pipe_rate(struct rte_eth_dev *dev,
	struct tm_node *np,
	struct tm_shaper_profile *sp)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	struct rte_sched_pipe_params *profile0 = pipe_profile_get(dev, np);
	struct rte_sched_pipe_params profile1;
	uint32_t pipe_profile_id;

	/* Derive new pipe profile. */
	memcpy(&profile1, profile0, sizeof(profile1));
	profile1.tb_rate = sp->params.peak.rate;
	profile1.tb_size = sp->params.peak.size;

	/* Since implementation does not allow adding more pipe profiles after
	 * port configuration, the pipe configuration can be successfully
	 * updated only if the new profile is also part of the existing set of
	 * pipe profiles.
	 */
	if (pipe_profile_exists(dev, &profile1, &pipe_profile_id) == 0)
		return -1;

	/* Update the pipe profile used by the current pipe. */
	if (rte_sched_pipe_config(SCHED(p), subport_id, pipe_id,
		(int32_t)pipe_profile_id))
		return -1;

	/* Commit changes. */
	pipe_profile_mark(dev, subport_id, pipe_id, pipe_profile_id);
	np->shaper_profile->n_users--;
	np->shaper_profile = sp;
	np->params.shaper_profile_id = sp->shaper_profile_id;
	sp->n_users++;

	return 0;
}

static int
update_tc_rate(struct rte_eth_dev *dev,
	struct tm_node *nt,
	struct tm_shaper_profile *sp)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t tc_id = tm_node_tc_id(dev, nt);

	struct tm_node *np = nt->parent_node;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	struct rte_sched_pipe_params *profile0 = pipe_profile_get(dev, np);
	struct rte_sched_pipe_params profile1;
	uint32_t pipe_profile_id;

	/* Derive new pipe profile. */
	memcpy(&profile1, profile0, sizeof(profile1));
	profile1.tc_rate[tc_id] = sp->params.peak.rate;

	/* Since implementation does not allow adding more pipe profiles after
	 * port configuration, the pipe configuration can be successfully
	 * updated only if the new profile is also part of the existing set of
	 * pipe profiles.
	 */
	if (pipe_profile_exists(dev, &profile1, &pipe_profile_id) == 0)
		return -1;

	/* Update the pipe profile used by the current pipe. */
	if (rte_sched_pipe_config(SCHED(p), subport_id, pipe_id,
		(int32_t)pipe_profile_id))
		return -1;

	/* Commit changes. */
	pipe_profile_mark(dev, subport_id, pipe_id, pipe_profile_id);
	nt->shaper_profile->n_users--;
	nt->shaper_profile = sp;
	nt->params.shaper_profile_id = sp->shaper_profile_id;
	sp->n_users++;

	return 0;
}

/* Traffic manager node shaper update */
static int
pmd_tm_node_shaper_update(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error)
{
	struct tm_node *n;
	struct tm_shaper_profile *sp;

	/* Port must be started and TM used. */
	if (dev->data->dev_started == 0 && (tm_used(dev) == 0))
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	/* Node must be valid */
	n = tm_node_search(dev, node_id);
	if (n == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	/* Shaper profile must be valid. */
	sp = tm_shaper_profile_search(dev, shaper_profile_id);
	if (sp == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_SHAPER_PROFILE,
			NULL,
			rte_strerror(EINVAL));

	switch (n->level) {
	case TM_NODE_LEVEL_PORT:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));
		/* fall-through */
	case TM_NODE_LEVEL_SUBPORT:
		if (update_subport_rate(dev, n, sp))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
		/* fall-through */
	case TM_NODE_LEVEL_PIPE:
		if (update_pipe_rate(dev, n, sp))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
		/* fall-through */
	case TM_NODE_LEVEL_TC:
		if (update_tc_rate(dev, n, sp))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
		/* fall-through */
	case TM_NODE_LEVEL_QUEUE:
		/* fall-through */
	default:
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EINVAL));
	}
}

static inline uint32_t
tm_port_queue_id(struct rte_eth_dev *dev,
	uint32_t port_subport_id,
	uint32_t subport_pipe_id,
	uint32_t pipe_tc_id,
	uint32_t tc_queue_id)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	uint32_t n_pipes_per_subport = h->n_tm_nodes[TM_NODE_LEVEL_PIPE] /
			h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT];

	uint32_t port_pipe_id =
		port_subport_id * n_pipes_per_subport + subport_pipe_id;
	uint32_t port_tc_id =
		port_pipe_id * RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE + pipe_tc_id;
	uint32_t port_queue_id =
		port_tc_id * RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS + tc_queue_id;

	return port_queue_id;
}

static int
read_port_stats(struct rte_eth_dev *dev,
	struct tm_node *nr,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct tm_hierarchy *h = &p->soft.tm.h;
	uint32_t n_subports_per_port = h->n_tm_nodes[TM_NODE_LEVEL_SUBPORT];
	uint32_t subport_id;

	for (subport_id = 0; subport_id < n_subports_per_port; subport_id++) {
		struct rte_sched_subport_stats s;
		uint32_t tc_ov, id;

		/* Stats read */
		int status = rte_sched_subport_read_stats(SCHED(p),
			subport_id,
			&s,
			&tc_ov);
		if (status)
			return status;

		/* Stats accumulate */
		for (id = 0; id < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; id++) {
			nr->stats.n_pkts +=
				s.n_pkts_tc[id] - s.n_pkts_tc_dropped[id];
			nr->stats.n_bytes +=
				s.n_bytes_tc[id] - s.n_bytes_tc_dropped[id];
			nr->stats.leaf.n_pkts_dropped[RTE_TM_GREEN] +=
				s.n_pkts_tc_dropped[id];
			nr->stats.leaf.n_bytes_dropped[RTE_TM_GREEN] +=
				s.n_bytes_tc_dropped[id];
		}
	}

	/* Stats copy */
	if (stats)
		memcpy(stats, &nr->stats, sizeof(*stats));

	if (stats_mask)
		*stats_mask = STATS_MASK_DEFAULT;

	/* Stats clear */
	if (clear)
		memset(&nr->stats, 0, sizeof(nr->stats));

	return 0;
}

static int
read_subport_stats(struct rte_eth_dev *dev,
	struct tm_node *ns,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t subport_id = tm_node_subport_id(dev, ns);
	struct rte_sched_subport_stats s;
	uint32_t tc_ov, tc_id;

	/* Stats read */
	int status = rte_sched_subport_read_stats(SCHED(p),
		subport_id,
		&s,
		&tc_ov);
	if (status)
		return status;

	/* Stats accumulate */
	for (tc_id = 0; tc_id < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; tc_id++) {
		ns->stats.n_pkts +=
			s.n_pkts_tc[tc_id] - s.n_pkts_tc_dropped[tc_id];
		ns->stats.n_bytes +=
			s.n_bytes_tc[tc_id] - s.n_bytes_tc_dropped[tc_id];
		ns->stats.leaf.n_pkts_dropped[RTE_TM_GREEN] +=
			s.n_pkts_tc_dropped[tc_id];
		ns->stats.leaf.n_bytes_dropped[RTE_TM_GREEN] +=
			s.n_bytes_tc_dropped[tc_id];
	}

	/* Stats copy */
	if (stats)
		memcpy(stats, &ns->stats, sizeof(*stats));

	if (stats_mask)
		*stats_mask = STATS_MASK_DEFAULT;

	/* Stats clear */
	if (clear)
		memset(&ns->stats, 0, sizeof(ns->stats));

	return 0;
}

static int
read_pipe_stats(struct rte_eth_dev *dev,
	struct tm_node *np,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear)
{
	struct pmd_internals *p = dev->data->dev_private;

	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	uint32_t i;

	/* Stats read */
	for (i = 0; i < RTE_SCHED_QUEUES_PER_PIPE; i++) {
		struct rte_sched_queue_stats s;
		uint16_t qlen;

		uint32_t qid = tm_port_queue_id(dev,
			subport_id,
			pipe_id,
			i / RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			i % RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS);

		int status = rte_sched_queue_read_stats(SCHED(p),
			qid,
			&s,
			&qlen);
		if (status)
			return status;

		/* Stats accumulate */
		np->stats.n_pkts += s.n_pkts - s.n_pkts_dropped;
		np->stats.n_bytes += s.n_bytes - s.n_bytes_dropped;
		np->stats.leaf.n_pkts_dropped[RTE_TM_GREEN] += s.n_pkts_dropped;
		np->stats.leaf.n_bytes_dropped[RTE_TM_GREEN] +=
			s.n_bytes_dropped;
		np->stats.leaf.n_pkts_queued = qlen;
	}

	/* Stats copy */
	if (stats)
		memcpy(stats, &np->stats, sizeof(*stats));

	if (stats_mask)
		*stats_mask = STATS_MASK_DEFAULT;

	/* Stats clear */
	if (clear)
		memset(&np->stats, 0, sizeof(np->stats));

	return 0;
}

static int
read_tc_stats(struct rte_eth_dev *dev,
	struct tm_node *nt,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear)
{
	struct pmd_internals *p = dev->data->dev_private;

	uint32_t tc_id = tm_node_tc_id(dev, nt);

	struct tm_node *np = nt->parent_node;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	uint32_t i;

	/* Stats read */
	for (i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
		struct rte_sched_queue_stats s;
		uint16_t qlen;

		uint32_t qid = tm_port_queue_id(dev,
			subport_id,
			pipe_id,
			tc_id,
			i);

		int status = rte_sched_queue_read_stats(SCHED(p),
			qid,
			&s,
			&qlen);
		if (status)
			return status;

		/* Stats accumulate */
		nt->stats.n_pkts += s.n_pkts - s.n_pkts_dropped;
		nt->stats.n_bytes += s.n_bytes - s.n_bytes_dropped;
		nt->stats.leaf.n_pkts_dropped[RTE_TM_GREEN] += s.n_pkts_dropped;
		nt->stats.leaf.n_bytes_dropped[RTE_TM_GREEN] +=
			s.n_bytes_dropped;
		nt->stats.leaf.n_pkts_queued = qlen;
	}

	/* Stats copy */
	if (stats)
		memcpy(stats, &nt->stats, sizeof(*stats));

	if (stats_mask)
		*stats_mask = STATS_MASK_DEFAULT;

	/* Stats clear */
	if (clear)
		memset(&nt->stats, 0, sizeof(nt->stats));

	return 0;
}

static int
read_queue_stats(struct rte_eth_dev *dev,
	struct tm_node *nq,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct rte_sched_queue_stats s;
	uint16_t qlen;

	uint32_t queue_id = tm_node_queue_id(dev, nq);

	struct tm_node *nt = nq->parent_node;
	uint32_t tc_id = tm_node_tc_id(dev, nt);

	struct tm_node *np = nt->parent_node;
	uint32_t pipe_id = tm_node_pipe_id(dev, np);

	struct tm_node *ns = np->parent_node;
	uint32_t subport_id = tm_node_subport_id(dev, ns);

	/* Stats read */
	uint32_t qid = tm_port_queue_id(dev,
		subport_id,
		pipe_id,
		tc_id,
		queue_id);

	int status = rte_sched_queue_read_stats(SCHED(p),
		qid,
		&s,
		&qlen);
	if (status)
		return status;

	/* Stats accumulate */
	nq->stats.n_pkts += s.n_pkts - s.n_pkts_dropped;
	nq->stats.n_bytes += s.n_bytes - s.n_bytes_dropped;
	nq->stats.leaf.n_pkts_dropped[RTE_TM_GREEN] += s.n_pkts_dropped;
	nq->stats.leaf.n_bytes_dropped[RTE_TM_GREEN] +=
		s.n_bytes_dropped;
	nq->stats.leaf.n_pkts_queued = qlen;

	/* Stats copy */
	if (stats)
		memcpy(stats, &nq->stats, sizeof(*stats));

	if (stats_mask)
		*stats_mask = STATS_MASK_QUEUE;

	/* Stats clear */
	if (clear)
		memset(&nq->stats, 0, sizeof(nq->stats));

	return 0;
}

/* Traffic manager read stats counters for specific node */
static int
pmd_tm_node_stats_read(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_tm_error *error)
{
	struct tm_node *n;

	/* Port must be started and TM used. */
	if (dev->data->dev_started == 0 && (tm_used(dev) == 0))
		return -rte_tm_error_set(error,
			EBUSY,
			RTE_TM_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(EBUSY));

	/* Node must be valid */
	n = tm_node_search(dev, node_id);
	if (n == NULL)
		return -rte_tm_error_set(error,
			EINVAL,
			RTE_TM_ERROR_TYPE_NODE_ID,
			NULL,
			rte_strerror(EINVAL));

	switch (n->level) {
	case TM_NODE_LEVEL_PORT:
		if (read_port_stats(dev, n, stats, stats_mask, clear))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;

	case TM_NODE_LEVEL_SUBPORT:
		if (read_subport_stats(dev, n, stats, stats_mask, clear))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;

	case TM_NODE_LEVEL_PIPE:
		if (read_pipe_stats(dev, n, stats, stats_mask, clear))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;

	case TM_NODE_LEVEL_TC:
		if (read_tc_stats(dev, n, stats, stats_mask, clear))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;

	case TM_NODE_LEVEL_QUEUE:
	default:
		if (read_queue_stats(dev, n, stats, stats_mask, clear))
			return -rte_tm_error_set(error,
				EINVAL,
				RTE_TM_ERROR_TYPE_UNSPECIFIED,
				NULL,
				rte_strerror(EINVAL));
		return 0;
	}
}

const struct rte_tm_ops pmd_tm_ops = {
	.node_type_get = pmd_tm_node_type_get,
	.capabilities_get = pmd_tm_capabilities_get,
	.level_capabilities_get = pmd_tm_level_capabilities_get,
	.node_capabilities_get = pmd_tm_node_capabilities_get,

	.wred_profile_add = pmd_tm_wred_profile_add,
	.wred_profile_delete = pmd_tm_wred_profile_delete,
	.shared_wred_context_add_update = NULL,
	.shared_wred_context_delete = NULL,

	.shaper_profile_add = pmd_tm_shaper_profile_add,
	.shaper_profile_delete = pmd_tm_shaper_profile_delete,
	.shared_shaper_add_update = pmd_tm_shared_shaper_add_update,
	.shared_shaper_delete = pmd_tm_shared_shaper_delete,

	.node_add = pmd_tm_node_add,
	.node_delete = pmd_tm_node_delete,
	.node_suspend = NULL,
	.node_resume = NULL,
	.hierarchy_commit = pmd_tm_hierarchy_commit,

	.node_parent_update = pmd_tm_node_parent_update,
	.node_shaper_update = pmd_tm_node_shaper_update,
	.node_shared_shaper_update = NULL,
	.node_stats_update = NULL,
	.node_wfq_weight_mode_update = NULL,
	.node_cman_update = NULL,
	.node_wred_context_update = NULL,
	.node_shared_wred_context_update = NULL,

	.node_stats_read = pmd_tm_node_stats_read,
};
