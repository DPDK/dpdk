/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"

static int i40e_tm_capabilities_get(struct rte_eth_dev *dev,
				    struct rte_tm_capabilities *cap,
				    struct rte_tm_error *error);

const struct rte_tm_ops i40e_tm_ops = {
	.capabilities_get = i40e_tm_capabilities_get,
};

int
i40e_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &i40e_tm_ops;

	return 0;
}

static inline uint16_t
i40e_tc_nb_get(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	uint16_t sum = 0;
	int i;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (main_vsi->enabled_tc & BIT_ULL(i))
			sum++;
	}

	return sum;
}

static int
i40e_tm_capabilities_get(struct rte_eth_dev *dev,
			 struct rte_tm_capabilities *cap,
			 struct rte_tm_error *error)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t tc_nb = i40e_tc_nb_get(dev);

	if (!cap || !error)
		return -EINVAL;

	if (tc_nb > hw->func_caps.num_tx_qp)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	/**
	 * support port + TCs + queues
	 * here shows the max capability not the current configuration.
	 */
	cap->n_nodes_max = 1 + I40E_MAX_TRAFFIC_CLASS + hw->func_caps.num_tx_qp;
	cap->n_levels_max = 3; /* port, TC, queue */
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->n_nodes_max;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 0;
	/* 40Gbps -> 5GBps */
	cap->shaper_private_rate_max = 5000000000ull;
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->sched_n_children_max = hw->func_caps.num_tx_qp;
	/**
	 * HW supports SP. But no plan to support it now.
	 * So, all the nodes should have the same priority.
	 */
	cap->sched_sp_n_priorities_max = 1;
	cap->sched_wfq_n_children_per_group_max = 0;
	cap->sched_wfq_n_groups_max = 0;
	/**
	 * SW only supports fair round robin now.
	 * So, all the nodes should have the same weight.
	 */
	cap->sched_wfq_weight_max = 1;
	cap->cman_head_drop_supported = 0;
	cap->dynamic_update_mask = 0;
	cap->shaper_pkt_length_adjust_min = RTE_TM_ETH_FRAMING_OVERHEAD;
	cap->shaper_pkt_length_adjust_max = RTE_TM_ETH_FRAMING_OVERHEAD_FCS;
	cap->cman_wred_context_n_max = 0;
	cap->cman_wred_context_private_n_max = 0;
	cap->cman_wred_context_shared_n_max = 0;
	cap->cman_wred_context_shared_n_nodes_per_context_max = 0;
	cap->cman_wred_context_shared_n_contexts_per_node_max = 0;
	cap->stats_mask = 0;

	return 0;
}
