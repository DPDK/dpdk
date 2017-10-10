/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_malloc.h>

#include "rte_eth_softnic_internals.h"
#include "rte_eth_softnic.h"

#define BYTES_IN_MBPS (1000 * 1000 / 8)

int
tm_params_check(struct pmd_params *params, uint32_t hard_rate)
{
	uint64_t hard_rate_bytes_per_sec = hard_rate * BYTES_IN_MBPS;
	uint32_t i;

	/* rate */
	if (params->soft.tm.rate) {
		if (params->soft.tm.rate > hard_rate_bytes_per_sec)
			return -EINVAL;
	} else {
		params->soft.tm.rate =
			(hard_rate_bytes_per_sec > UINT32_MAX) ?
				UINT32_MAX : hard_rate_bytes_per_sec;
	}

	/* nb_queues */
	if (params->soft.tm.nb_queues == 0)
		return -EINVAL;

	if (params->soft.tm.nb_queues < RTE_SCHED_QUEUES_PER_PIPE)
		params->soft.tm.nb_queues = RTE_SCHED_QUEUES_PER_PIPE;

	params->soft.tm.nb_queues =
		rte_align32pow2(params->soft.tm.nb_queues);

	/* qsize */
	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
		if (params->soft.tm.qsize[i] == 0)
			return -EINVAL;

		params->soft.tm.qsize[i] =
			rte_align32pow2(params->soft.tm.qsize[i]);
	}

	/* enq_bsz, deq_bsz */
	if (params->soft.tm.enq_bsz == 0 ||
		params->soft.tm.deq_bsz == 0 ||
		params->soft.tm.deq_bsz >= params->soft.tm.enq_bsz)
		return -EINVAL;

	return 0;
}

int
tm_init(struct pmd_internals *p,
	struct pmd_params *params,
	int numa_node)
{
	uint32_t enq_bsz = params->soft.tm.enq_bsz;
	uint32_t deq_bsz = params->soft.tm.deq_bsz;

	p->soft.tm.pkts_enq = rte_zmalloc_socket(params->soft.name,
		2 * enq_bsz * sizeof(struct rte_mbuf *),
		0,
		numa_node);

	if (p->soft.tm.pkts_enq == NULL)
		return -ENOMEM;

	p->soft.tm.pkts_deq = rte_zmalloc_socket(params->soft.name,
		deq_bsz * sizeof(struct rte_mbuf *),
		0,
		numa_node);

	if (p->soft.tm.pkts_deq == NULL) {
		rte_free(p->soft.tm.pkts_enq);
		return -ENOMEM;
	}

	return 0;
}

void
tm_free(struct pmd_internals *p)
{
	rte_free(p->soft.tm.pkts_enq);
	rte_free(p->soft.tm.pkts_deq);
}

int
tm_start(struct pmd_internals *p)
{
	struct tm_params *t = &p->soft.tm.params;
	uint32_t n_subports, subport_id;
	int status;

	/* Port */
	p->soft.tm.sched = rte_sched_port_config(&t->port_params);
	if (p->soft.tm.sched == NULL)
		return -1;

	/* Subport */
	n_subports = t->port_params.n_subports_per_port;
	for (subport_id = 0; subport_id < n_subports; subport_id++) {
		uint32_t n_pipes_per_subport =
			t->port_params.n_pipes_per_subport;
		uint32_t pipe_id;

		status = rte_sched_subport_config(p->soft.tm.sched,
			subport_id,
			&t->subport_params[subport_id]);
		if (status) {
			rte_sched_port_free(p->soft.tm.sched);
			return -1;
		}

		/* Pipe */
		n_pipes_per_subport = t->port_params.n_pipes_per_subport;
		for (pipe_id = 0; pipe_id < n_pipes_per_subport; pipe_id++) {
			int pos = subport_id * TM_MAX_PIPES_PER_SUBPORT +
				pipe_id;
			int profile_id = t->pipe_to_profile[pos];

			if (profile_id < 0)
				continue;

			status = rte_sched_pipe_config(p->soft.tm.sched,
				subport_id,
				pipe_id,
				profile_id);
			if (status) {
				rte_sched_port_free(p->soft.tm.sched);
				return -1;
			}
		}
	}

	return 0;
}

void
tm_stop(struct pmd_internals *p)
{
	if (p->soft.tm.sched)
		rte_sched_port_free(p->soft.tm.sched);
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

static uint32_t
tm_level_get_max_nodes(struct rte_eth_dev *dev, enum tm_node_level level)
{
	struct pmd_internals *p = dev->data->dev_private;
	uint32_t n_queues_max = p->params.soft.tm.nb_queues;
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

		.nonleaf = {
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
		},
	},

	[TM_NODE_LEVEL_SUBPORT] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		.nonleaf = {
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
		},
	},

	[TM_NODE_LEVEL_PIPE] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		.nonleaf = {
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
		},
	},

	[TM_NODE_LEVEL_TC] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = UINT32_MAX,
		.n_nodes_leaf_max = 0,
		.non_leaf_nodes_identical = 1,
		.leaf_nodes_identical = 0,

		.nonleaf = {
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
		},
	},

	[TM_NODE_LEVEL_QUEUE] = {
		.n_nodes_max = UINT32_MAX,
		.n_nodes_nonleaf_max = 0,
		.n_nodes_leaf_max = UINT32_MAX,
		.non_leaf_nodes_identical = 0,
		.leaf_nodes_identical = 1,

		.leaf = {
			.shaper_private_supported = 0,
			.shaper_private_dual_rate_supported = 0,
			.shaper_private_rate_min = 0,
			.shaper_private_rate_max = 0,
			.shaper_shared_n_max = 0,

			.cman_head_drop_supported = 0,
			.cman_wred_context_private_supported = WRED_SUPPORTED,
			.cman_wred_context_shared_n_max = 0,

			.stats_mask = STATS_MASK_QUEUE,
		},
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

		.nonleaf = {
			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = 1,
		},

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_SUBPORT] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 0,

		.nonleaf = {
			.sched_n_children_max = UINT32_MAX,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max = UINT32_MAX,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = UINT32_MAX,
		},

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_PIPE] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 0,

		.nonleaf = {
			.sched_n_children_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_sp_n_priorities_max =
				RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE,
			.sched_wfq_n_children_per_group_max = 1,
			.sched_wfq_n_groups_max = 0,
			.sched_wfq_weight_max = 1,
		},

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_TC] = {
		.shaper_private_supported = 1,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 1,
		.shaper_private_rate_max = UINT32_MAX,
		.shaper_shared_n_max = 1,

		.nonleaf = {
			.sched_n_children_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_sp_n_priorities_max = 1,
			.sched_wfq_n_children_per_group_max =
				RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS,
			.sched_wfq_n_groups_max = 1,
			.sched_wfq_weight_max = UINT32_MAX,
		},

		.stats_mask = STATS_MASK_DEFAULT,
	},

	[TM_NODE_LEVEL_QUEUE] = {
		.shaper_private_supported = 0,
		.shaper_private_dual_rate_supported = 0,
		.shaper_private_rate_min = 0,
		.shaper_private_rate_max = 0,
		.shaper_shared_n_max = 0,


		.leaf = {
			.cman_head_drop_supported = 0,
			.cman_wred_context_private_supported = WRED_SUPPORTED,
			.cman_wred_context_shared_n_max = 0,
		},

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

const struct rte_tm_ops pmd_tm_ops = {
	.capabilities_get = pmd_tm_capabilities_get,
	.level_capabilities_get = pmd_tm_level_capabilities_get,
	.node_capabilities_get = pmd_tm_node_capabilities_get,
};
