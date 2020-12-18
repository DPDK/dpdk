/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <rte_malloc.h>

#include "txgbe_ethdev.h"

void
txgbe_tm_conf_init(struct rte_eth_dev *dev)
{
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);

	/* initialize shaper profile list */
	TAILQ_INIT(&tm_conf->shaper_profile_list);

	/* initialize node configuration */
	tm_conf->root = NULL;
	TAILQ_INIT(&tm_conf->queue_list);
	TAILQ_INIT(&tm_conf->tc_list);
	tm_conf->nb_tc_node = 0;
	tm_conf->nb_queue_node = 0;
	tm_conf->committed = false;
}

void
txgbe_tm_conf_uninit(struct rte_eth_dev *dev)
{
	struct txgbe_tm_conf *tm_conf = TXGBE_DEV_TM_CONF(dev);
	struct txgbe_tm_shaper_profile *shaper_profile;
	struct txgbe_tm_node *tm_node;

	/* clear node configuration */
	while ((tm_node = TAILQ_FIRST(&tm_conf->queue_list))) {
		TAILQ_REMOVE(&tm_conf->queue_list, tm_node, node);
		rte_free(tm_node);
	}
	tm_conf->nb_queue_node = 0;
	while ((tm_node = TAILQ_FIRST(&tm_conf->tc_list))) {
		TAILQ_REMOVE(&tm_conf->tc_list, tm_node, node);
		rte_free(tm_node);
	}
	tm_conf->nb_tc_node = 0;
	if (tm_conf->root) {
		rte_free(tm_conf->root);
		tm_conf->root = NULL;
	}

	/* Remove all shaper profiles */
	while ((shaper_profile =
	       TAILQ_FIRST(&tm_conf->shaper_profile_list))) {
		TAILQ_REMOVE(&tm_conf->shaper_profile_list,
			     shaper_profile, node);
		rte_free(shaper_profile);
	}
}

