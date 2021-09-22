/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <cnxk_ethdev.h>
#include <cnxk_tm.h>
#include <cnxk_utils.h>

static int
cnxk_nix_tm_node_type_get(struct rte_eth_dev *eth_dev, uint32_t node_id,
			  int *is_leaf, struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_tm_node *node;

	if (is_leaf == NULL) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		return -EINVAL;
	}

	node = roc_nix_tm_node_get(nix, node_id);
	if (node_id == RTE_TM_NODE_ID_NULL || !node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		return -EINVAL;
	}

	if (roc_nix_tm_lvl_is_leaf(nix, node->lvl))
		*is_leaf = true;
	else
		*is_leaf = false;

	return 0;
}

static int
cnxk_nix_tm_capa_get(struct rte_eth_dev *eth_dev,
		     struct rte_tm_capabilities *cap,
		     struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc, max_nr_nodes = 0, i, n_lvl;
	struct roc_nix *nix = &dev->nix;
	uint16_t schq[ROC_TM_LVL_MAX];

	memset(cap, 0, sizeof(*cap));

	rc = roc_nix_tm_rsrc_count(nix, schq);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	for (i = 0; i < NIX_TXSCH_LVL_TL1; i++)
		max_nr_nodes += schq[i];

	cap->n_nodes_max = max_nr_nodes + dev->nb_txq;

	n_lvl = roc_nix_tm_lvl_cnt_get(nix);
	/* Consider leaf level */
	cap->n_levels_max = n_lvl + 1;
	cap->non_leaf_nodes_identical = 1;
	cap->leaf_nodes_identical = 1;

	/* Shaper Capabilities */
	cap->shaper_private_n_max = max_nr_nodes;
	cap->shaper_n_max = max_nr_nodes;
	cap->shaper_private_dual_rate_n_max = max_nr_nodes;
	cap->shaper_private_rate_min = NIX_TM_MIN_SHAPER_RATE / 8;
	cap->shaper_private_rate_max = NIX_TM_MAX_SHAPER_RATE / 8;
	cap->shaper_private_packet_mode_supported = 1;
	cap->shaper_private_byte_mode_supported = 1;
	cap->shaper_pkt_length_adjust_min = NIX_TM_LENGTH_ADJUST_MIN;
	cap->shaper_pkt_length_adjust_max = NIX_TM_LENGTH_ADJUST_MAX;

	/* Schedule Capabilities */
	cap->sched_n_children_max = schq[n_lvl - 1];
	cap->sched_sp_n_priorities_max = NIX_TM_TLX_SP_PRIO_MAX;
	cap->sched_wfq_n_children_per_group_max = cap->sched_n_children_max;
	cap->sched_wfq_n_groups_max = 1;
	cap->sched_wfq_weight_max = roc_nix_tm_max_sched_wt_get();
	cap->sched_wfq_packet_mode_supported = 1;
	cap->sched_wfq_byte_mode_supported = 1;

	cap->dynamic_update_mask = RTE_TM_UPDATE_NODE_PARENT_KEEP_LEVEL |
				   RTE_TM_UPDATE_NODE_SUSPEND_RESUME;
	cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES |
			  RTE_TM_STATS_N_PKTS_RED_DROPPED |
			  RTE_TM_STATS_N_BYTES_RED_DROPPED;

	for (i = 0; i < RTE_COLORS; i++) {
		cap->mark_vlan_dei_supported[i] = false;
		cap->mark_ip_ecn_tcp_supported[i] = false;
		cap->mark_ip_dscp_supported[i] = false;
	}

	return 0;
}

static int
cnxk_nix_tm_level_capa_get(struct rte_eth_dev *eth_dev, uint32_t lvl,
			   struct rte_tm_level_capabilities *cap,
			   struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint16_t schq[ROC_TM_LVL_MAX];
	int rc, n_lvl;

	memset(cap, 0, sizeof(*cap));

	rc = roc_nix_tm_rsrc_count(nix, schq);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	n_lvl = roc_nix_tm_lvl_cnt_get(nix);

	if (roc_nix_tm_lvl_is_leaf(nix, lvl)) {
		/* Leaf */
		cap->n_nodes_max = dev->nb_txq;
		cap->n_nodes_leaf_max = dev->nb_txq;
		cap->leaf_nodes_identical = 1;
		cap->leaf.stats_mask =
			RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;

	} else if (lvl == ROC_TM_LVL_ROOT) {
		/* Root node, a.k.a. TL2(vf)/TL1(pf) */
		cap->n_nodes_max = 1;
		cap->n_nodes_nonleaf_max = 1;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported =
			roc_nix_tm_lvl_have_link_access(nix, lvl) ? false :
								    true;
		cap->nonleaf.shaper_private_rate_min =
			NIX_TM_MIN_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_rate_max =
			NIX_TM_MAX_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_packet_mode_supported = 1;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;

		cap->nonleaf.sched_n_children_max = schq[lvl];
		cap->nonleaf.sched_sp_n_priorities_max =
			roc_nix_tm_max_prio(nix, lvl) + 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max =
			roc_nix_tm_max_sched_wt_get();
		cap->nonleaf.sched_wfq_packet_mode_supported = 1;
		cap->nonleaf.sched_wfq_byte_mode_supported = 1;

		if (roc_nix_tm_lvl_have_link_access(nix, lvl))
			cap->nonleaf.stats_mask =
				RTE_TM_STATS_N_PKTS_RED_DROPPED |
				RTE_TM_STATS_N_BYTES_RED_DROPPED;
	} else if (lvl < ROC_TM_LVL_MAX) {
		/* TL2, TL3, TL4, MDQ */
		cap->n_nodes_max = schq[lvl];
		cap->n_nodes_nonleaf_max = cap->n_nodes_max;
		cap->non_leaf_nodes_identical = 1;

		cap->nonleaf.shaper_private_supported = true;
		cap->nonleaf.shaper_private_dual_rate_supported = true;
		cap->nonleaf.shaper_private_rate_min =
			NIX_TM_MIN_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_rate_max =
			NIX_TM_MAX_SHAPER_RATE / 8;
		cap->nonleaf.shaper_private_packet_mode_supported = 1;
		cap->nonleaf.shaper_private_byte_mode_supported = 1;

		/* MDQ doesn't support Strict Priority */
		if ((int)lvl == (n_lvl - 1))
			cap->nonleaf.sched_n_children_max = dev->nb_txq;
		else
			cap->nonleaf.sched_n_children_max = schq[lvl - 1];
		cap->nonleaf.sched_sp_n_priorities_max =
			roc_nix_tm_max_prio(nix, lvl) + 1;
		cap->nonleaf.sched_wfq_n_groups_max = 1;
		cap->nonleaf.sched_wfq_weight_max =
			roc_nix_tm_max_sched_wt_get();
		cap->nonleaf.sched_wfq_packet_mode_supported = 1;
		cap->nonleaf.sched_wfq_byte_mode_supported = 1;
	} else {
		/* unsupported level */
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		return rc;
	}
	return 0;
}

static int
cnxk_nix_tm_node_capa_get(struct rte_eth_dev *eth_dev, uint32_t node_id,
			  struct rte_tm_node_capabilities *cap,
			  struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_nix_tm_node *tm_node;
	struct roc_nix *nix = &dev->nix;
	uint16_t schq[ROC_TM_LVL_MAX];
	int rc, n_lvl, lvl;

	memset(cap, 0, sizeof(*cap));

	tm_node = (struct cnxk_nix_tm_node *)roc_nix_tm_node_get(nix, node_id);
	if (!tm_node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	lvl = tm_node->nix_node.lvl;
	n_lvl = roc_nix_tm_lvl_cnt_get(nix);

	/* Leaf node */
	if (roc_nix_tm_lvl_is_leaf(nix, lvl)) {
		cap->stats_mask = RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;
		return 0;
	}

	rc = roc_nix_tm_rsrc_count(nix, schq);
	if (rc) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "unexpected fatal error";
		return rc;
	}

	/* Non Leaf Shaper */
	cap->shaper_private_supported = true;
	cap->shaper_private_rate_min = NIX_TM_MIN_SHAPER_RATE / 8;
	cap->shaper_private_rate_max = NIX_TM_MAX_SHAPER_RATE / 8;
	cap->shaper_private_packet_mode_supported = 1;
	cap->shaper_private_byte_mode_supported = 1;

	/* Non Leaf Scheduler */
	if (lvl == (n_lvl - 1))
		cap->nonleaf.sched_n_children_max = dev->nb_txq;
	else
		cap->nonleaf.sched_n_children_max = schq[lvl - 1];

	cap->nonleaf.sched_sp_n_priorities_max =
		roc_nix_tm_max_prio(nix, lvl) + 1;
	cap->nonleaf.sched_wfq_n_children_per_group_max =
		cap->nonleaf.sched_n_children_max;
	cap->nonleaf.sched_wfq_n_groups_max = 1;
	cap->nonleaf.sched_wfq_weight_max = roc_nix_tm_max_sched_wt_get();
	cap->nonleaf.sched_wfq_packet_mode_supported = 1;
	cap->nonleaf.sched_wfq_byte_mode_supported = 1;

	cap->shaper_private_dual_rate_supported = true;
	if (roc_nix_tm_lvl_have_link_access(nix, lvl)) {
		cap->shaper_private_dual_rate_supported = false;
		cap->stats_mask = RTE_TM_STATS_N_PKTS_RED_DROPPED |
				  RTE_TM_STATS_N_BYTES_RED_DROPPED;
	}

	return 0;
}

const struct rte_tm_ops cnxk_tm_ops = {
	.node_type_get = cnxk_nix_tm_node_type_get,
	.capabilities_get = cnxk_nix_tm_capa_get,
	.level_capabilities_get = cnxk_nix_tm_level_capa_get,
	.node_capabilities_get = cnxk_nix_tm_node_capa_get,
};

int
cnxk_nix_tm_ops_get(struct rte_eth_dev *eth_dev __rte_unused, void *arg)
{
	if (!arg)
		return -EINVAL;

	/* Check for supported revisions */
	if (roc_model_is_cn96_ax() || roc_model_is_cn95_a0())
		return -EINVAL;

	*(const void **)arg = &cnxk_tm_ops;

	return 0;
}

int
cnxk_nix_tm_set_queue_rate_limit(struct rte_eth_dev *eth_dev,
				 uint16_t queue_idx, uint16_t tx_rate_mbps)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint64_t tx_rate = tx_rate_mbps * (uint64_t)1E6;
	struct roc_nix *nix = &dev->nix;
	int rc = -EINVAL;

	/* Check for supported revisions */
	if (roc_model_is_cn96_ax() || roc_model_is_cn95_a0())
		goto exit;

	if (queue_idx >= eth_dev->data->nb_tx_queues)
		goto exit;

	if ((roc_nix_tm_tree_type_get(nix) != ROC_NIX_TM_RLIMIT) &&
	    eth_dev->data->nb_tx_queues > 1) {
		/*
		 * Disable xmit will be enabled when
		 * new topology is available.
		 */
		rc = roc_nix_tm_hierarchy_disable(nix);
		if (rc)
			goto exit;

		rc = roc_nix_tm_prepare_rate_limited_tree(nix);
		if (rc)
			goto exit;

		rc = roc_nix_tm_hierarchy_enable(nix, ROC_NIX_TM_RLIMIT, true);
		if (rc)
			goto exit;
	}

	return roc_nix_tm_rlimit_sq(nix, queue_idx, tx_rate);
exit:
	return rc;
}
