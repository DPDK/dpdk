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

#include "ixgbe_ethdev.h"

static int ixgbe_tm_capabilities_get(struct rte_eth_dev *dev,
				     struct rte_tm_capabilities *cap,
				     struct rte_tm_error *error);

const struct rte_tm_ops ixgbe_tm_ops = {
	.capabilities_get = ixgbe_tm_capabilities_get,
};

int
ixgbe_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		 void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &ixgbe_tm_ops;

	return 0;
}

static inline uint8_t
ixgbe_tc_nb_get(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *eth_conf;
	uint8_t nb_tcs = 0;

	eth_conf = &dev->data->dev_conf;
	if (eth_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
		nb_tcs = eth_conf->tx_adv_conf.dcb_tx_conf.nb_tcs;
	} else if (eth_conf->txmode.mq_mode == ETH_MQ_TX_VMDQ_DCB) {
		if (eth_conf->tx_adv_conf.vmdq_dcb_tx_conf.nb_queue_pools ==
		    ETH_32_POOLS)
			nb_tcs = ETH_4_TCS;
		else
			nb_tcs = ETH_8_TCS;
	} else {
		nb_tcs = 1;
	}

	return nb_tcs;
}

static int
ixgbe_tm_capabilities_get(struct rte_eth_dev *dev,
			  struct rte_tm_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint8_t tc_nb = ixgbe_tc_nb_get(dev);

	if (!cap || !error)
		return -EINVAL;

	if (tc_nb > hw->mac.max_tx_queues)
		return -EINVAL;

	error->type = RTE_TM_ERROR_TYPE_NONE;

	/* set all the parameters to 0 first. */
	memset(cap, 0, sizeof(struct rte_tm_capabilities));

	/**
	 * here is the max capability not the current configuration.
	 */
	/* port + TCs + queues */
	cap->n_nodes_max = 1 + IXGBE_DCB_MAX_TRAFFIC_CLASS +
			   hw->mac.max_tx_queues;
	cap->n_levels_max = 3;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;
	cap->shaper_n_max = cap->n_nodes_max;
	cap->shaper_private_n_max = cap->n_nodes_max;
	cap->shaper_private_dual_rate_n_max = 0;
	cap->shaper_private_rate_min = 0;
	/* 10Gbps -> 1.25GBps */
	cap->shaper_private_rate_max = 1250000000ull;
	cap->shaper_shared_n_max = 0;
	cap->shaper_shared_n_nodes_per_shaper_max = 0;
	cap->shaper_shared_n_shapers_per_node_max = 0;
	cap->shaper_shared_dual_rate_n_max = 0;
	cap->shaper_shared_rate_min = 0;
	cap->shaper_shared_rate_max = 0;
	cap->sched_n_children_max = hw->mac.max_tx_queues;
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
