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

	cap->mark_vlan_dei_supported[RTE_COLOR_GREEN] = false;
	cap->mark_ip_ecn_tcp_supported[RTE_COLOR_GREEN] = false;
	cap->mark_ip_ecn_sctp_supported[RTE_COLOR_GREEN] = false;
	cap->mark_ip_dscp_supported[RTE_COLOR_GREEN] = false;

	for (i = RTE_COLOR_YELLOW; i < RTE_COLORS; i++) {
		cap->mark_vlan_dei_supported[i] = true;
		cap->mark_ip_ecn_tcp_supported[i] = true;
		cap->mark_ip_ecn_sctp_supported[i] = true;
		cap->mark_ip_dscp_supported[i] = true;
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

static int
cnxk_nix_tm_shaper_profile_add(struct rte_eth_dev *eth_dev, uint32_t id,
			       struct rte_tm_shaper_params *params,
			       struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_nix_tm_shaper_profile *profile;
	struct roc_nix *nix = &dev->nix;
	int rc;

	if (roc_nix_tm_shaper_profile_get(nix, id)) {
		error->type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		error->message = "shaper profile ID exist";
		return -EINVAL;
	}

	profile = rte_zmalloc("cnxk_nix_tm_shaper_profile",
			      sizeof(struct cnxk_nix_tm_shaper_profile), 0);
	if (!profile)
		return -ENOMEM;
	profile->profile.id = id;
	profile->profile.commit_rate = params->committed.rate;
	profile->profile.peak_rate = params->peak.rate;
	profile->profile.commit_sz = params->committed.size;
	profile->profile.peak_sz = params->peak.size;
	/* If Byte mode, then convert to bps */
	if (!params->packet_mode) {
		profile->profile.commit_rate *= 8;
		profile->profile.peak_rate *= 8;
		profile->profile.commit_sz *= 8;
		profile->profile.peak_sz *= 8;
	}
	profile->profile.pkt_len_adj = params->pkt_length_adjust;
	profile->profile.pkt_mode = params->packet_mode;
	profile->profile.free_fn = rte_free;
	rte_memcpy(&profile->params, params,
		   sizeof(struct rte_tm_shaper_params));

	rc = roc_nix_tm_shaper_profile_add(nix, &profile->profile);

	/* fill error information based on return value */
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}

	return rc;
}

static int
cnxk_nix_tm_shaper_profile_delete(struct rte_eth_dev *eth_dev,
				  uint32_t profile_id,
				  struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_tm_shaper_profile_delete(nix, profile_id);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}

	return rc;
}

static int
cnxk_nix_tm_node_add(struct rte_eth_dev *eth_dev, uint32_t node_id,
		     uint32_t parent_node_id, uint32_t priority,
		     uint32_t weight, uint32_t lvl,
		     struct rte_tm_node_params *params,
		     struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_tm_shaper_profile *profile;
	struct roc_nix_tm_node *parent_node;
	struct roc_nix *nix = &dev->nix;
	struct cnxk_nix_tm_node *node;
	int rc;

	/* we don't support dynamic updates */
	if (roc_nix_tm_is_user_hierarchy_enabled(nix)) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "dynamic update not supported";
		return -EIO;
	}

	parent_node = roc_nix_tm_node_get(nix, parent_node_id);
	/* find the right level */
	if (lvl == RTE_TM_NODE_LEVEL_ID_ANY) {
		if (parent_node_id == RTE_TM_NODE_ID_NULL) {
			lvl = ROC_TM_LVL_ROOT;
		} else if (parent_node) {
			lvl = parent_node->lvl + 1;
		} else {
			/* Neither proper parent nor proper level id given */
			error->type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
			error->message = "invalid parent node id";
			return -ERANGE;
		}
	}

	node = rte_zmalloc("cnxk_nix_tm_node", sizeof(struct cnxk_nix_tm_node),
			   0);
	if (!node)
		return -ENOMEM;

	rte_memcpy(&node->params, params, sizeof(struct rte_tm_node_params));

	node->nix_node.id = node_id;
	node->nix_node.parent_id = parent_node_id;
	node->nix_node.priority = priority;
	node->nix_node.weight = weight;
	node->nix_node.lvl = lvl;
	node->nix_node.shaper_profile_id = params->shaper_profile_id;

	profile = roc_nix_tm_shaper_profile_get(nix, params->shaper_profile_id);
	/* Packet mode */
	if (!roc_nix_tm_lvl_is_leaf(nix, lvl) &&
	    ((profile && profile->pkt_mode) ||
	     (params->nonleaf.wfq_weight_mode &&
	      params->nonleaf.n_sp_priorities &&
	      !params->nonleaf.wfq_weight_mode[0])))
		node->nix_node.pkt_mode = 1;

	rc = roc_nix_tm_node_add(nix, &node->nix_node);
	if (rc < 0) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
		return rc;
	}
	error->type = RTE_TM_ERROR_TYPE_NONE;
	roc_nix_tm_shaper_default_red_algo(&node->nix_node, profile);

	return 0;
}

static int
cnxk_nix_tm_node_delete(struct rte_eth_dev *eth_dev, uint32_t node_id,
			struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct cnxk_nix_tm_node *node;
	int rc;

	/* we don't support dynamic updates yet */
	if (roc_nix_tm_is_user_hierarchy_enabled(nix)) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "hierarchy exists";
		return -EIO;
	}

	if (node_id == RTE_TM_NODE_ID_NULL) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "invalid node id";
		return -EINVAL;
	}

	node = (struct cnxk_nix_tm_node *)roc_nix_tm_node_get(nix, node_id);

	rc = roc_nix_tm_node_delete(nix, node_id, 0);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	} else {
		rte_free(node);
	}

	return rc;
}

static int
cnxk_nix_tm_node_suspend(struct rte_eth_dev *eth_dev, uint32_t node_id,
			 struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc;

	rc = roc_nix_tm_node_suspend_resume(&dev->nix, node_id, true);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}

	return rc;
}

static int
cnxk_nix_tm_node_resume(struct rte_eth_dev *eth_dev, uint32_t node_id,
			struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc;

	rc = roc_nix_tm_node_suspend_resume(&dev->nix, node_id, false);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}

	return rc;
}

static int
cnxk_nix_tm_hierarchy_commit(struct rte_eth_dev *eth_dev,
			     int clear_on_fail __rte_unused,
			     struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	if (roc_nix_tm_is_user_hierarchy_enabled(nix)) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "hierarchy exists";
		return -EIO;
	}

	if (roc_nix_tm_leaf_cnt(nix) < dev->nb_txq) {
		error->type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		error->message = "incomplete hierarchy";
		return -EINVAL;
	}

	rc = roc_nix_tm_hierarchy_disable(nix);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
		return -EIO;
	}

	rc = roc_nix_tm_hierarchy_enable(nix, ROC_NIX_TM_USER, true);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
		return -EIO;
	}
	error->type = RTE_TM_ERROR_TYPE_NONE;

	return 0;
}

static int
cnxk_nix_tm_node_shaper_update(struct rte_eth_dev *eth_dev, uint32_t node_id,
			       uint32_t profile_id, struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_tm_shaper_profile *profile;
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_tm_node *node;
	int rc;

	rc = roc_nix_tm_node_shaper_update(nix, node_id, profile_id, false);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
		return -EINVAL;
	}
	node = roc_nix_tm_node_get(nix, node_id);
	if (!node)
		return -EINVAL;

	profile = roc_nix_tm_shaper_profile_get(nix, profile_id);
	roc_nix_tm_shaper_default_red_algo(node, profile);

	return 0;
}

static int
cnxk_nix_tm_node_parent_update(struct rte_eth_dev *eth_dev, uint32_t node_id,
			       uint32_t new_parent_id, uint32_t priority,
			       uint32_t weight, struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	rc = roc_nix_tm_node_parent_update(nix, node_id, new_parent_id,
					   priority, weight);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
		return -EINVAL;
	}

	return 0;
}

static int
cnxk_nix_tm_node_stats_read(struct rte_eth_dev *eth_dev, uint32_t node_id,
			    struct rte_tm_node_stats *stats,
			    uint64_t *stats_mask, int clear,
			    struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_tm_node_stats nix_tm_stats;
	struct roc_nix *nix = &dev->nix;
	struct roc_nix_tm_node *node;
	int rc;

	node = roc_nix_tm_node_get(nix, node_id);
	if (!node) {
		error->type = RTE_TM_ERROR_TYPE_NODE_ID;
		error->message = "no such node";
		return -EINVAL;
	}

	if (roc_nix_tm_lvl_is_leaf(nix, node->lvl)) {
		struct roc_nix_stats_queue qstats;

		rc = roc_nix_stats_queue_get(nix, node->id, 0, &qstats);
		if (!rc) {
			stats->n_pkts = qstats.tx_pkts;
			stats->n_bytes = qstats.tx_octs;
			*stats_mask =
				RTE_TM_STATS_N_PKTS | RTE_TM_STATS_N_BYTES;
		}
		goto exit;
	}

	rc = roc_nix_tm_node_stats_get(nix, node_id, clear, &nix_tm_stats);
	if (!rc) {
		stats->leaf.n_pkts_dropped[RTE_COLOR_RED] =
			nix_tm_stats.stats[ROC_NIX_TM_NODE_PKTS_DROPPED];
		stats->leaf.n_bytes_dropped[RTE_COLOR_RED] =
			nix_tm_stats.stats[ROC_NIX_TM_NODE_BYTES_DROPPED];
		*stats_mask = RTE_TM_STATS_N_PKTS_RED_DROPPED |
			      RTE_TM_STATS_N_BYTES_RED_DROPPED;
	}

exit:
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}
	return rc;
}

int
cnxk_nix_tm_mark_vlan_dei(struct rte_eth_dev *eth_dev, int mark_green,
			  int mark_yellow, int mark_red,
			  struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	int rc;

	if (mark_green) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "Green VLAN marking not supported";
		return -EINVAL;
	}

	if (eth_dev->data->dev_started) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "VLAN DEI mark for running ports not "
				 "supported";
		return -EBUSY;
	}

	rc = roc_nix_tm_mark_config(roc_nix, ROC_NIX_TM_MARK_VLAN_DEI,
				    mark_yellow, mark_red);
	if (rc) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}
	return rc;
}

int
cnxk_nix_tm_mark_ip_ecn(struct rte_eth_dev *eth_dev, int mark_green,
			int mark_yellow, int mark_red,
			struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	int rc;

	if (mark_green) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "Green IP ECN marking not supported";
		return -EINVAL;
	}

	if (eth_dev->data->dev_started) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "IP ECN mark for running ports not "
				 "supported";
		return -EBUSY;
	}

	rc = roc_nix_tm_mark_config(roc_nix, ROC_NIX_TM_MARK_IPV4_ECN,
				    mark_yellow, mark_red);
	if (rc < 0)
		goto exit;

	rc = roc_nix_tm_mark_config(roc_nix, ROC_NIX_TM_MARK_IPV6_ECN,
				    mark_yellow, mark_red);
exit:
	if (rc < 0) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}
	return rc;
}

int
cnxk_nix_tm_mark_ip_dscp(struct rte_eth_dev *eth_dev, int mark_green,
			 int mark_yellow, int mark_red,
			 struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	int rc;

	if (mark_green) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "Green IP DSCP marking not supported";
		return -EINVAL;
	}

	if (eth_dev->data->dev_started) {
		error->type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		error->message = "IP DSCP mark for running ports not "
				 "supported";
		return -EBUSY;
	}

	rc = roc_nix_tm_mark_config(roc_nix, ROC_NIX_TM_MARK_IPV4_DSCP,
				    mark_yellow, mark_red);
	if (rc < 0)
		goto exit;

	rc = roc_nix_tm_mark_config(roc_nix, ROC_NIX_TM_MARK_IPV6_DSCP,
				    mark_yellow, mark_red);
exit:
	if (rc < 0) {
		error->type = roc_nix_tm_err_to_rte_err(rc);
		error->message = roc_error_msg_get(rc);
	}
	return rc;
}

struct rte_tm_ops cnxk_tm_ops = {
	.node_type_get = cnxk_nix_tm_node_type_get,
	.capabilities_get = cnxk_nix_tm_capa_get,
	.level_capabilities_get = cnxk_nix_tm_level_capa_get,
	.node_capabilities_get = cnxk_nix_tm_node_capa_get,

	.shaper_profile_add = cnxk_nix_tm_shaper_profile_add,
	.shaper_profile_delete = cnxk_nix_tm_shaper_profile_delete,

	.node_add = cnxk_nix_tm_node_add,
	.node_delete = cnxk_nix_tm_node_delete,
	.node_suspend = cnxk_nix_tm_node_suspend,
	.node_resume = cnxk_nix_tm_node_resume,
	.hierarchy_commit = cnxk_nix_tm_hierarchy_commit,

	.node_shaper_update = cnxk_nix_tm_node_shaper_update,
	.node_parent_update = cnxk_nix_tm_node_parent_update,
	.node_stats_read = cnxk_nix_tm_node_stats_read,

	.mark_vlan_dei = cnxk_nix_tm_mark_vlan_dei,
	.mark_ip_ecn = cnxk_nix_tm_mark_ip_ecn,
	.mark_ip_dscp = cnxk_nix_tm_mark_ip_dscp,
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
