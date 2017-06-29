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

#include <rte_malloc.h>

#include "ixgbe_ethdev.h"

static int ixgbe_tm_capabilities_get(struct rte_eth_dev *dev,
				     struct rte_tm_capabilities *cap,
				     struct rte_tm_error *error);
static int ixgbe_shaper_profile_add(struct rte_eth_dev *dev,
				    uint32_t shaper_profile_id,
				    struct rte_tm_shaper_params *profile,
				    struct rte_tm_error *error);

const struct rte_tm_ops ixgbe_tm_ops = {
	.capabilities_get = ixgbe_tm_capabilities_get,
	.shaper_profile_add = ixgbe_shaper_profile_add,
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

void
ixgbe_tm_conf_init(struct rte_eth_dev *dev)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);

	/* initialize shaper profile list */
	TAILQ_INIT(&tm_conf->shaper_profile_list);
}

void
ixgbe_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_shaper_profile *shaper_profile;

	/* Remove all shaper profiles */
	while ((shaper_profile =
	       TAILQ_FIRST(&tm_conf->shaper_profile_list))) {
		TAILQ_REMOVE(&tm_conf->shaper_profile_list,
			     shaper_profile, node);
		rte_free(shaper_profile);
	}
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

static inline struct ixgbe_tm_shaper_profile *
ixgbe_shaper_profile_search(struct rte_eth_dev *dev,
			    uint32_t shaper_profile_id)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_shaper_profile_list *shaper_profile_list =
		&tm_conf->shaper_profile_list;
	struct ixgbe_tm_shaper_profile *shaper_profile;

	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		if (shaper_profile_id == shaper_profile->shaper_profile_id)
			return shaper_profile;
	}

	return NULL;
}

static int
ixgbe_shaper_profile_param_check(struct rte_tm_shaper_params *profile,
				 struct rte_tm_error *error)
{
	/* min rate not supported */
	if (profile->committed.rate) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		error->message = "committed rate not supported";
		return -EINVAL;
	}
	/* min bucket size not supported */
	if (profile->committed.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
		error->message = "committed bucket size not supported";
		return -EINVAL;
	}
	/* max bucket size not supported */
	if (profile->peak.size) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
		error->message = "peak bucket size not supported";
		return -EINVAL;
	}
	/* length adjustment not supported */
	if (profile->pkt_length_adjust) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN;
		error->message = "packet length adjustment not supported";
		return -EINVAL;
	}

	return 0;
}

static int
ixgbe_shaper_profile_add(struct rte_eth_dev *dev,
			 uint32_t shaper_profile_id,
			 struct rte_tm_shaper_params *profile,
			 struct rte_tm_error *error)
{
	struct ixgbe_tm_conf *tm_conf =
		IXGBE_DEV_PRIVATE_TO_TM_CONF(dev->data->dev_private);
	struct ixgbe_tm_shaper_profile *shaper_profile;
	int ret;

	if (!profile || !error)
		return -EINVAL;

	ret = ixgbe_shaper_profile_param_check(profile, error);
	if (ret)
		return ret;

	shaper_profile = ixgbe_shaper_profile_search(dev, shaper_profile_id);

	if (shaper_profile) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "profile ID exist";
		return -EINVAL;
	}

	shaper_profile = rte_zmalloc("ixgbe_tm_shaper_profile",
				     sizeof(struct ixgbe_tm_shaper_profile),
				     0);
	if (!shaper_profile)
		return -ENOMEM;
	shaper_profile->shaper_profile_id = shaper_profile_id;
	(void)rte_memcpy(&shaper_profile->profile, profile,
			 sizeof(struct rte_tm_shaper_params));
	TAILQ_INSERT_TAIL(&tm_conf->shaper_profile_list,
			  shaper_profile, node);

	return 0;
}
