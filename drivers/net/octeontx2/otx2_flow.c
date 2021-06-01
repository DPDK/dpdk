/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"
#include "otx2_ethdev_sec.h"
#include "otx2_flow.h"

enum flow_vtag_cfg_dir { VTAG_TX, VTAG_RX };

int
otx2_flow_free_all_resources(struct otx2_eth_dev *hw)
{
	struct otx2_npc_flow_info *npc = &hw->npc_flow;
	struct otx2_mbox *mbox = hw->mbox;
	struct otx2_mcam_ents_info *info;
	struct rte_bitmap *bmap;
	struct rte_flow *flow;
	int entry_count = 0;
	int rc, idx;

	for (idx = 0; idx < npc->flow_max_priority; idx++) {
		info = &npc->flow_entry_info[idx];
		entry_count += info->live_ent;
	}

	if (entry_count == 0)
		return 0;

	/* Free all MCAM entries allocated */
	rc = otx2_flow_mcam_free_all_entries(mbox);

	/* Free any MCAM counters and delete flow list */
	for (idx = 0; idx < npc->flow_max_priority; idx++) {
		while ((flow = TAILQ_FIRST(&npc->flow_list[idx])) != NULL) {
			if (flow->ctr_id != NPC_COUNTER_NONE)
				rc |= otx2_flow_mcam_free_counter(mbox,
							     flow->ctr_id);

			TAILQ_REMOVE(&npc->flow_list[idx], flow, next);
			rte_free(flow);
			bmap = npc->live_entries[flow->priority];
			rte_bitmap_clear(bmap, flow->mcam_id);
		}
		info = &npc->flow_entry_info[idx];
		info->free_ent = 0;
		info->live_ent = 0;
	}
	return rc;
}


static int
flow_program_npc(struct otx2_parse_state *pst, struct otx2_mbox *mbox,
		 struct otx2_npc_flow_info *flow_info)
{
	/* This is non-LDATA part in search key */
	uint64_t key_data[2] = {0ULL, 0ULL};
	uint64_t key_mask[2] = {0ULL, 0ULL};
	int intf = pst->flow->nix_intf;
	int key_len, bit = 0, index;
	int off, idx, data_off = 0;
	uint8_t lid, mask, data;
	uint16_t layer_info;
	uint64_t lt, flags;


	/* Skip till Layer A data start */
	while (bit < NPC_PARSE_KEX_S_LA_OFFSET) {
		if (flow_info->keyx_supp_nmask[intf] & (1 << bit))
			data_off++;
		bit++;
	}

	/* Each bit represents 1 nibble */
	data_off *= 4;

	index = 0;
	for (lid = 0; lid < NPC_MAX_LID; lid++) {
		/* Offset in key */
		off = NPC_PARSE_KEX_S_LID_OFFSET(lid);
		lt = pst->lt[lid] & 0xf;
		flags = pst->flags[lid] & 0xff;

		/* NPC_LAYER_KEX_S */
		layer_info = ((flow_info->keyx_supp_nmask[intf] >> off) & 0x7);

		if (layer_info) {
			for (idx = 0; idx <= 2 ; idx++) {
				if (layer_info & (1 << idx)) {
					if (idx == 2)
						data = lt;
					else if (idx == 1)
						data = ((flags >> 4) & 0xf);
					else
						data = (flags & 0xf);

					if (data_off >= 64) {
						data_off = 0;
						index++;
					}
					key_data[index] |= ((uint64_t)data <<
							    data_off);
					mask = 0xf;
					if (lt == 0)
						mask = 0;
					key_mask[index] |= ((uint64_t)mask <<
							    data_off);
					data_off += 4;
				}
			}
		}
	}

	otx2_npc_dbg("Npc prog key data0: 0x%" PRIx64 ", data1: 0x%" PRIx64,
		     key_data[0], key_data[1]);

	/* Copy this into mcam string */
	key_len = (pst->npc->keyx_len[intf] + 7) / 8;
	otx2_npc_dbg("Key_len  = %d", key_len);
	memcpy(pst->flow->mcam_data, key_data, key_len);
	memcpy(pst->flow->mcam_mask, key_mask, key_len);

	otx2_npc_dbg("Final flow data");
	for (idx = 0; idx < OTX2_MAX_MCAM_WIDTH_DWORDS; idx++) {
		otx2_npc_dbg("data[%d]: 0x%" PRIx64 ", mask[%d]: 0x%" PRIx64,
			     idx, pst->flow->mcam_data[idx],
			     idx, pst->flow->mcam_mask[idx]);
	}

	/*
	 * Now we have mcam data and mask formatted as
	 * [Key_len/4 nibbles][0 or 1 nibble hole][data]
	 * hole is present if key_len is odd number of nibbles.
	 * mcam data must be split into 64 bits + 48 bits segments
	 * for each back W0, W1.
	 */

	return otx2_flow_mcam_alloc_and_write(pst->flow, mbox, pst, flow_info);
}

static int
flow_parse_attr(struct rte_eth_dev *eth_dev,
		const struct rte_flow_attr *attr,
		struct rte_flow_error *error,
		struct rte_flow *flow)
{
	struct otx2_eth_dev *dev = eth_dev->data->dev_private;
	const char *errmsg = NULL;

	if (attr == NULL)
		errmsg = "Attribute can't be empty";
	else if (attr->group)
		errmsg = "Groups are not supported";
	else if (attr->priority >= dev->npc_flow.flow_max_priority)
		errmsg = "Priority should be with in specified range";
	else if ((!attr->egress && !attr->ingress) ||
		 (attr->egress && attr->ingress))
		errmsg = "Exactly one of ingress or egress must be set";

	if (errmsg != NULL) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR,
				   attr, errmsg);
		return -ENOTSUP;
	}

	if (attr->ingress)
		flow->nix_intf = OTX2_INTF_RX;
	else
		flow->nix_intf = OTX2_INTF_TX;

	flow->priority = attr->priority;
	return 0;
}

static inline int
flow_get_free_rss_grp(struct rte_bitmap *bmap,
		      uint32_t size, uint32_t *pos)
{
	for (*pos = 0; *pos < size; ++*pos) {
		if (!rte_bitmap_get(bmap, *pos))
			break;
	}

	return *pos < size ? 0 : -1;
}

static int
flow_configure_rss_action(struct otx2_eth_dev *dev,
			  const struct rte_flow_action_rss *rss,
			  uint8_t *alg_idx, uint32_t *rss_grp,
			  int mcam_index)
{
	struct otx2_npc_flow_info *flow_info = &dev->npc_flow;
	uint16_t reta[NIX_RSS_RETA_SIZE_MAX];
	uint32_t flowkey_cfg, grp_aval, i;
	uint16_t *ind_tbl = NULL;
	uint8_t flowkey_algx;
	int rc;

	rc = flow_get_free_rss_grp(flow_info->rss_grp_entries,
				   flow_info->rss_grps, &grp_aval);
	/* RSS group :0 is not usable for flow rss action */
	if (rc < 0 || grp_aval == 0)
		return -ENOSPC;

	*rss_grp = grp_aval;

	otx2_nix_rss_set_key(dev, (uint8_t *)(uintptr_t)rss->key,
			     rss->key_len);

	/* If queue count passed in the rss action is less than
	 * HW configured reta size, replicate rss action reta
	 * across HW reta table.
	 */
	if (dev->rss_info.rss_size > rss->queue_num) {
		ind_tbl = reta;

		for (i = 0; i < (dev->rss_info.rss_size / rss->queue_num); i++)
			memcpy(reta + i * rss->queue_num, rss->queue,
			       sizeof(uint16_t) * rss->queue_num);

		i = dev->rss_info.rss_size % rss->queue_num;
		if (i)
			memcpy(&reta[dev->rss_info.rss_size] - i,
			       rss->queue, i * sizeof(uint16_t));
	} else {
		ind_tbl = (uint16_t *)(uintptr_t)rss->queue;
	}

	rc = otx2_nix_rss_tbl_init(dev, *rss_grp, ind_tbl);
	if (rc) {
		otx2_err("Failed to init rss table rc = %d", rc);
		return rc;
	}

	flowkey_cfg = otx2_rss_ethdev_to_nix(dev, rss->types, rss->level);

	rc = otx2_rss_set_hf(dev, flowkey_cfg, &flowkey_algx,
			     *rss_grp, mcam_index);
	if (rc) {
		otx2_err("Failed to set rss hash function rc = %d", rc);
		return rc;
	}

	*alg_idx = flowkey_algx;

	rte_bitmap_set(flow_info->rss_grp_entries, *rss_grp);

	return 0;
}


static int
flow_program_rss_action(struct rte_eth_dev *eth_dev,
			const struct rte_flow_action actions[],
			struct rte_flow *flow)
{
	struct otx2_eth_dev *dev = eth_dev->data->dev_private;
	const struct rte_flow_action_rss *rss;
	uint32_t rss_grp;
	uint8_t alg_idx;
	int rc;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			rss = (const struct rte_flow_action_rss *)actions->conf;

			rc = flow_configure_rss_action(dev,
						       rss, &alg_idx, &rss_grp,
						       flow->mcam_id);
			if (rc)
				return rc;

			flow->npc_action &= (~(0xfULL));
			flow->npc_action |= NIX_RX_ACTIONOP_RSS;
			flow->npc_action |=
				((uint64_t)(alg_idx & NIX_RSS_ACT_ALG_MASK) <<
				 NIX_RSS_ACT_ALG_OFFSET) |
				((uint64_t)(rss_grp & NIX_RSS_ACT_GRP_MASK) <<
				 NIX_RSS_ACT_GRP_OFFSET);
		}
	}
	return 0;
}

static int
flow_free_rss_action(struct rte_eth_dev *eth_dev,
		     struct rte_flow *flow)
{
	struct otx2_eth_dev *dev = eth_dev->data->dev_private;
	struct otx2_npc_flow_info *npc = &dev->npc_flow;
	uint32_t rss_grp;

	if (flow->npc_action & NIX_RX_ACTIONOP_RSS) {
		rss_grp = (flow->npc_action >> NIX_RSS_ACT_GRP_OFFSET) &
			NIX_RSS_ACT_GRP_MASK;
		if (rss_grp == 0 || rss_grp >= npc->rss_grps)
			return -EINVAL;

		rte_bitmap_clear(npc->rss_grp_entries, rss_grp);
	}

	return 0;
}

static int
flow_update_sec_tt(struct rte_eth_dev *eth_dev,
		   const struct rte_flow_action actions[])
{
	int rc = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_SECURITY) {
			rc = otx2_eth_sec_update_tag_type(eth_dev);
			break;
		}
	}

	return rc;
}

static int
flow_parse_meta_items(__rte_unused struct otx2_parse_state *pst)
{
	otx2_npc_dbg("Meta Item");
	return 0;
}

/*
 * Parse function of each layer:
 *  - Consume one or more patterns that are relevant.
 *  - Update parse_state
 *  - Set parse_state.pattern = last item consumed
 *  - Set appropriate error code/message when returning error.
 */
typedef int (*flow_parse_stage_func_t)(struct otx2_parse_state *pst);

static int
flow_parse_pattern(struct rte_eth_dev *dev,
		   const struct rte_flow_item pattern[],
		   struct rte_flow_error *error,
		   struct rte_flow *flow,
		   struct otx2_parse_state *pst)
{
	flow_parse_stage_func_t parse_stage_funcs[] = {
		flow_parse_meta_items,
		otx2_flow_parse_higig2_hdr,
		otx2_flow_parse_la,
		otx2_flow_parse_lb,
		otx2_flow_parse_lc,
		otx2_flow_parse_ld,
		otx2_flow_parse_le,
		otx2_flow_parse_lf,
		otx2_flow_parse_lg,
		otx2_flow_parse_lh,
	};
	struct otx2_eth_dev *hw = dev->data->dev_private;
	uint8_t layer = 0;
	int key_offset;
	int rc;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
				   "pattern is NULL");
		return -EINVAL;
	}

	memset(pst, 0, sizeof(*pst));
	pst->npc = &hw->npc_flow;
	pst->error = error;
	pst->flow = flow;

	/* Use integral byte offset */
	key_offset = pst->npc->keyx_len[flow->nix_intf];
	key_offset = (key_offset + 7) / 8;

	/* Location where LDATA would begin */
	pst->mcam_data = (uint8_t *)flow->mcam_data;
	pst->mcam_mask = (uint8_t *)flow->mcam_mask;

	while (pattern->type != RTE_FLOW_ITEM_TYPE_END &&
	       layer < RTE_DIM(parse_stage_funcs)) {
		otx2_npc_dbg("Pattern type = %d", pattern->type);

		/* Skip place-holders */
		pattern = otx2_flow_skip_void_and_any_items(pattern);

		pst->pattern = pattern;
		otx2_npc_dbg("Is tunnel = %d, layer = %d", pst->tunnel, layer);
		rc = parse_stage_funcs[layer](pst);
		if (rc != 0)
			return -rte_errno;

		layer++;

		/*
		 * Parse stage function sets pst->pattern to
		 * 1 past the last item it consumed.
		 */
		pattern = pst->pattern;

		if (pst->terminate)
			break;
	}

	/* Skip trailing place-holders */
	pattern = otx2_flow_skip_void_and_any_items(pattern);

	/* Are there more items than what we can handle? */
	if (pattern->type != RTE_FLOW_ITEM_TYPE_END) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM, pattern,
				   "unsupported item in the sequence");
		return -ENOTSUP;
	}

	return 0;
}

static int
flow_parse_rule(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error,
		struct rte_flow *flow,
		struct otx2_parse_state *pst)
{
	int err;

	/* Check attributes */
	err = flow_parse_attr(dev, attr, error, flow);
	if (err)
		return err;

	/* Check actions */
	err = otx2_flow_parse_actions(dev, attr, actions, error, flow);
	if (err)
		return err;

	/* Check pattern */
	err = flow_parse_pattern(dev, pattern, error, flow, pst);
	if (err)
		return err;

	/* Check for overlaps? */
	return 0;
}

static int
otx2_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct otx2_parse_state parse_state;
	struct rte_flow flow;

	memset(&flow, 0, sizeof(flow));
	return flow_parse_rule(dev, attr, pattern, actions, error, &flow,
			       &parse_state);
}

static int
flow_program_vtag_action(struct rte_eth_dev *eth_dev,
			 const struct rte_flow_action actions[],
			 struct rte_flow *flow)
{
	uint16_t vlan_id = 0, vlan_ethtype = RTE_ETHER_TYPE_VLAN;
	struct otx2_eth_dev *dev = eth_dev->data->dev_private;
	union {
		uint64_t reg;
		struct nix_tx_vtag_action_s act;
	} tx_vtag_action;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_vtag_config *vtag_cfg;
	struct nix_vtag_config_rsp *rsp;
	bool vlan_insert_action = false;
	uint64_t rx_vtag_action = 0;
	uint8_t vlan_pcp = 0;
	int rc;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_OF_POP_VLAN) {
			if (dev->npc_flow.vtag_actions == 1) {
				vtag_cfg =
					otx2_mbox_alloc_msg_nix_vtag_cfg(mbox);
				vtag_cfg->cfg_type = VTAG_RX;
				vtag_cfg->rx.strip_vtag = 1;
				/* Always capture */
				vtag_cfg->rx.capture_vtag = 1;
				vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
				vtag_cfg->rx.vtag_type = 0;

				rc = otx2_mbox_process(mbox);
				if (rc)
					return rc;
			}

			rx_vtag_action |= (NIX_RX_VTAGACTION_VTAG_VALID << 15);
			rx_vtag_action |= (NPC_LID_LB << 8);
			rx_vtag_action |= NIX_RX_VTAGACTION_VTAG0_RELPTR;
			flow->vtag_action = rx_vtag_action;
		} else if (actions->type ==
			   RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) {
			const struct rte_flow_action_of_set_vlan_vid *vtag =
				(const struct rte_flow_action_of_set_vlan_vid *)
					actions->conf;
			vlan_id = rte_be_to_cpu_16(vtag->vlan_vid);
			if (vlan_id > 0xfff) {
				otx2_err("Invalid vlan_id for set vlan action");
				return -EINVAL;
			}
			vlan_insert_action = true;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN) {
			const struct rte_flow_action_of_push_vlan *ethtype =
				(const struct rte_flow_action_of_push_vlan *)
					actions->conf;
			vlan_ethtype = rte_be_to_cpu_16(ethtype->ethertype);
			if (vlan_ethtype != RTE_ETHER_TYPE_VLAN &&
			    vlan_ethtype != RTE_ETHER_TYPE_QINQ) {
				otx2_err("Invalid ethtype specified for push"
					 " vlan action");
				return -EINVAL;
			}
			vlan_insert_action = true;
		} else if (actions->type ==
			   RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) {
			const struct rte_flow_action_of_set_vlan_pcp *pcp =
				(const struct rte_flow_action_of_set_vlan_pcp *)
					actions->conf;
			vlan_pcp = pcp->vlan_pcp;
			if (vlan_pcp > 0x7) {
				otx2_err("Invalid PCP value for pcp action");
				return -EINVAL;
			}
			vlan_insert_action = true;
		}
	}

	if (vlan_insert_action) {
		vtag_cfg = otx2_mbox_alloc_msg_nix_vtag_cfg(mbox);
		vtag_cfg->cfg_type = VTAG_TX;
		vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
		vtag_cfg->tx.vtag0 =
			((vlan_ethtype << 16) | (vlan_pcp << 13) | vlan_id);
		vtag_cfg->tx.cfg_vtag0 = 1;
		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			return rc;

		tx_vtag_action.reg = 0;
		tx_vtag_action.act.vtag0_def = rsp->vtag0_idx;
		if (tx_vtag_action.act.vtag0_def < 0) {
			otx2_err("Failed to config TX VTAG action");
			return -EINVAL;
		}
		tx_vtag_action.act.vtag0_lid = NPC_LID_LA;
		tx_vtag_action.act.vtag0_op = NIX_TX_VTAGOP_INSERT;
		tx_vtag_action.act.vtag0_relptr =
			NIX_TX_VTAGACTION_VTAG0_RELPTR;
		flow->vtag_action = tx_vtag_action.reg;
	}
	return 0;
}

static struct rte_flow *
otx2_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	struct otx2_parse_state parse_state;
	struct otx2_mbox *mbox = hw->mbox;
	struct rte_flow *flow, *flow_iter;
	struct otx2_flow_list *list;
	int rc;

	flow = rte_zmalloc("otx2_rte_flow", sizeof(*flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Memory allocation failed");
		return NULL;
	}
	memset(flow, 0, sizeof(*flow));

	rc = flow_parse_rule(dev, attr, pattern, actions, error, flow,
			     &parse_state);
	if (rc != 0)
		goto err_exit;

	rc = flow_program_vtag_action(dev, actions, flow);
	if (rc != 0) {
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to program vlan action");
		goto err_exit;
	}

	parse_state.is_vf = otx2_dev_is_vf(hw);

	rc = flow_program_npc(&parse_state, mbox, &hw->npc_flow);
	if (rc != 0) {
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to insert filter");
		goto err_exit;
	}

	rc = flow_program_rss_action(dev, actions, flow);
	if (rc != 0) {
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to program rss action");
		goto err_exit;
	}

	if (hw->rx_offloads & DEV_RX_OFFLOAD_SECURITY) {
		rc = flow_update_sec_tt(dev, actions);
		if (rc != 0) {
			rte_flow_error_set(error, EIO,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "Failed to update tt with sec act");
			goto err_exit;
		}
	}

	list = &hw->npc_flow.flow_list[flow->priority];
	/* List in ascending order of mcam entries */
	TAILQ_FOREACH(flow_iter, list, next) {
		if (flow_iter->mcam_id > flow->mcam_id) {
			TAILQ_INSERT_BEFORE(flow_iter, flow, next);
			return flow;
		}
	}

	TAILQ_INSERT_TAIL(list, flow, next);
	return flow;

err_exit:
	rte_free(flow);
	return NULL;
}

static int
otx2_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	struct otx2_npc_flow_info *npc = &hw->npc_flow;
	struct otx2_mbox *mbox = hw->mbox;
	struct rte_bitmap *bmap;
	uint16_t match_id;
	int rc;

	match_id = (flow->npc_action >> NIX_RX_ACT_MATCH_OFFSET) &
		NIX_RX_ACT_MATCH_MASK;

	if (match_id && match_id < OTX2_FLOW_ACTION_FLAG_DEFAULT) {
		if (rte_atomic32_read(&npc->mark_actions) == 0)
			return -EINVAL;

		/* Clear mark offload flag if there are no more mark actions */
		if (rte_atomic32_sub_return(&npc->mark_actions, 1) == 0) {
			hw->rx_offload_flags &= ~NIX_RX_OFFLOAD_MARK_UPDATE_F;
			otx2_eth_set_rx_function(dev);
		}
	}

	if (flow->nix_intf == OTX2_INTF_RX && flow->vtag_action) {
		npc->vtag_actions--;
		if (npc->vtag_actions == 0) {
			if (hw->vlan_info.strip_on == 0) {
				hw->rx_offload_flags &=
					~NIX_RX_OFFLOAD_VLAN_STRIP_F;
				otx2_eth_set_rx_function(dev);
			}
		}
	}

	rc = flow_free_rss_action(dev, flow);
	if (rc != 0) {
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to free rss action");
	}

	rc = otx2_flow_mcam_free_entry(mbox, flow->mcam_id);
	if (rc != 0) {
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to destroy filter");
	}

	TAILQ_REMOVE(&npc->flow_list[flow->priority], flow, next);

	bmap = npc->live_entries[flow->priority];
	rte_bitmap_clear(bmap, flow->mcam_id);

	rte_free(flow);
	return 0;
}

static int
otx2_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	int rc;

	rc = otx2_flow_free_all_resources(hw);
	if (rc) {
		otx2_err("Error when deleting NPC MCAM entries "
				", counters");
		rte_flow_error_set(error, EIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to flush filter");
		return -rte_errno;
	}

	return 0;
}

static int
otx2_flow_isolate(struct rte_eth_dev *dev __rte_unused,
		  int enable __rte_unused,
		  struct rte_flow_error *error)
{
	/*
	 * If we support, we need to un-install the default mcam
	 * entry for this port.
	 */

	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL,
			   "Flow isolation not supported");

	return -rte_errno;
}

static int
otx2_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *action,
		void *data,
		struct rte_flow_error *error)
{
	struct otx2_eth_dev *hw = dev->data->dev_private;
	struct rte_flow_query_count *query = data;
	struct otx2_mbox *mbox = hw->mbox;
	const char *errmsg = NULL;
	int errcode = ENOTSUP;
	int rc;

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT) {
		errmsg = "Only COUNT is supported in query";
		goto err_exit;
	}

	if (flow->ctr_id == NPC_COUNTER_NONE) {
		errmsg = "Counter is not available";
		goto err_exit;
	}

	rc = otx2_flow_mcam_read_counter(mbox, flow->ctr_id, &query->hits);
	if (rc != 0) {
		errcode = EIO;
		errmsg = "Error reading flow counter";
		goto err_exit;
	}
	query->hits_set = 1;
	query->bytes_set = 0;

	if (query->reset)
		rc = otx2_flow_mcam_clear_counter(mbox, flow->ctr_id);
	if (rc != 0) {
		errcode = EIO;
		errmsg = "Error clearing flow counter";
		goto err_exit;
	}

	return 0;

err_exit:
	rte_flow_error_set(error, errcode,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL,
			   errmsg);
	return -rte_errno;
}

const struct rte_flow_ops otx2_flow_ops = {
	.validate = otx2_flow_validate,
	.create = otx2_flow_create,
	.destroy = otx2_flow_destroy,
	.flush = otx2_flow_flush,
	.query = otx2_flow_query,
	.isolate = otx2_flow_isolate,
};

static int
flow_supp_key_len(uint32_t supp_mask)
{
	int nib_count = 0;
	while (supp_mask) {
		nib_count++;
		supp_mask &= (supp_mask - 1);
	}
	return nib_count * 4;
}

/* Refer HRM register:
 * NPC_AF_INTF(0..1)_LID(0..7)_LT(0..15)_LD(0..1)_CFG
 * and
 * NPC_AF_INTF(0..1)_LDATA(0..1)_FLAGS(0..15)_CFG
 **/
#define BYTESM1_SHIFT	16
#define HDR_OFF_SHIFT	8
static void
flow_update_kex_info(struct npc_xtract_info *xtract_info,
		     uint64_t val)
{
	xtract_info->len = ((val >> BYTESM1_SHIFT) & 0xf) + 1;
	xtract_info->hdr_off = (val >> HDR_OFF_SHIFT) & 0xff;
	xtract_info->key_off = val & 0x3f;
	xtract_info->enable = ((val >> 7) & 0x1);
	xtract_info->flags_enable = ((val >> 6) & 0x1);
}

static void
flow_process_mkex_cfg(struct otx2_npc_flow_info *npc,
		      struct npc_get_kex_cfg_rsp *kex_rsp)
{
	volatile uint64_t (*q)[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT]
		[NPC_MAX_LD];
	struct npc_xtract_info *x_info = NULL;
	int lid, lt, ld, fl, ix;
	otx2_dxcfg_t *p;
	uint64_t keyw;
	uint64_t val;

	npc->keyx_supp_nmask[NPC_MCAM_RX] =
		kex_rsp->rx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_supp_nmask[NPC_MCAM_TX] =
		kex_rsp->tx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_len[NPC_MCAM_RX] =
		flow_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_RX]);
	npc->keyx_len[NPC_MCAM_TX] =
		flow_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_TX]);

	keyw = (kex_rsp->rx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_RX] = keyw;
	keyw = (kex_rsp->tx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_TX] = keyw;

	/* Update KEX_LD_FLAG */
	for (ix = 0; ix < NPC_MAX_INTF; ix++) {
		for (ld = 0; ld < NPC_MAX_LD; ld++) {
			for (fl = 0; fl < NPC_MAX_LFL; fl++) {
				x_info =
				    &npc->prx_fxcfg[ix][ld][fl].xtract[0];
				val = kex_rsp->intf_ld_flags[ix][ld][fl];
				flow_update_kex_info(x_info, val);
			}
		}
	}

	/* Update LID, LT and LDATA cfg */
	p = &npc->prx_dxcfg;
	q = (volatile uint64_t (*)[][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD])
			(&kex_rsp->intf_lid_lt_ld);
	for (ix = 0; ix < NPC_MAX_INTF; ix++) {
		for (lid = 0; lid < NPC_MAX_LID; lid++) {
			for (lt = 0; lt < NPC_MAX_LT; lt++) {
				for (ld = 0; ld < NPC_MAX_LD; ld++) {
					x_info = &(*p)[ix][lid][lt].xtract[ld];
					val = (*q)[ix][lid][lt][ld];
					flow_update_kex_info(x_info, val);
				}
			}
		}
	}
	/* Update LDATA Flags cfg */
	npc->prx_lfcfg[0].i = kex_rsp->kex_ld_flags[0];
	npc->prx_lfcfg[1].i = kex_rsp->kex_ld_flags[1];
}

static struct otx2_idev_kex_cfg *
flow_intra_dev_kex_cfg(void)
{
	static const char name[] = "octeontx2_intra_device_kex_conf";
	struct otx2_idev_kex_cfg *idev;
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz)
		return mz->addr;

	/* Request for the first time */
	mz = rte_memzone_reserve_aligned(name, sizeof(struct otx2_idev_kex_cfg),
					 SOCKET_ID_ANY, 0, OTX2_ALIGN);
	if (mz) {
		idev = mz->addr;
		rte_atomic16_set(&idev->kex_refcnt, 0);
		return idev;
	}
	return NULL;
}

static int
flow_fetch_kex_cfg(struct otx2_eth_dev *dev)
{
	struct otx2_npc_flow_info *npc = &dev->npc_flow;
	struct npc_get_kex_cfg_rsp *kex_rsp;
	struct otx2_mbox *mbox = dev->mbox;
	char mkex_pfl_name[MKEX_NAME_LEN];
	struct otx2_idev_kex_cfg *idev;
	int rc = 0;

	idev = flow_intra_dev_kex_cfg();
	if (!idev)
		return -ENOMEM;

	/* Is kex_cfg read by any another driver? */
	if (rte_atomic16_add_return(&idev->kex_refcnt, 1) == 1) {
		/* Call mailbox to get key & data size */
		(void)otx2_mbox_alloc_msg_npc_get_kex_cfg(mbox);
		otx2_mbox_msg_send(mbox, 0);
		rc = otx2_mbox_get_rsp(mbox, 0, (void *)&kex_rsp);
		if (rc) {
			otx2_err("Failed to fetch NPC keyx config");
			goto done;
		}
		memcpy(&idev->kex_cfg, kex_rsp,
		       sizeof(struct npc_get_kex_cfg_rsp));
	}

	otx2_mbox_memcpy(mkex_pfl_name,
			 idev->kex_cfg.mkex_pfl_name, MKEX_NAME_LEN);

	strlcpy((char *)dev->mkex_pfl_name,
		mkex_pfl_name, sizeof(dev->mkex_pfl_name));

	flow_process_mkex_cfg(npc, &idev->kex_cfg);

done:
	return rc;
}

#define OTX2_MCAM_TOT_ENTRIES_96XX (4096)
#define OTX2_MCAM_TOT_ENTRIES_98XX (16384)

static int otx2_mcam_tot_entries(struct otx2_eth_dev *dev)
{
	if (otx2_dev_is_98xx(dev))
		return OTX2_MCAM_TOT_ENTRIES_98XX;
	else
		return OTX2_MCAM_TOT_ENTRIES_96XX;
}

int
otx2_flow_init(struct otx2_eth_dev *hw)
{
	uint8_t *mem = NULL, *nix_mem = NULL, *npc_mem = NULL;
	struct otx2_npc_flow_info *npc = &hw->npc_flow;
	uint32_t bmap_sz, tot_mcam_entries = 0;
	int rc = 0, idx;

	rc = flow_fetch_kex_cfg(hw);
	if (rc) {
		otx2_err("Failed to fetch NPC keyx config from idev");
		return rc;
	}

	rte_atomic32_init(&npc->mark_actions);
	npc->vtag_actions = 0;

	tot_mcam_entries = otx2_mcam_tot_entries(hw);
	npc->mcam_entries = tot_mcam_entries >> npc->keyw[NPC_MCAM_RX];
	/* Free, free_rev, live and live_rev entries */
	bmap_sz = rte_bitmap_get_memory_footprint(npc->mcam_entries);
	mem = rte_zmalloc(NULL, 4 * bmap_sz * npc->flow_max_priority,
			  RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		otx2_err("Bmap alloc failed");
		rc = -ENOMEM;
		return rc;
	}

	npc->flow_entry_info = rte_zmalloc(NULL, npc->flow_max_priority
					   * sizeof(struct otx2_mcam_ents_info),
					   0);
	if (npc->flow_entry_info == NULL) {
		otx2_err("flow_entry_info alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->free_entries = rte_zmalloc(NULL, npc->flow_max_priority
					* sizeof(struct rte_bitmap *),
					0);
	if (npc->free_entries == NULL) {
		otx2_err("free_entries alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->free_entries_rev = rte_zmalloc(NULL, npc->flow_max_priority
					* sizeof(struct rte_bitmap *),
					0);
	if (npc->free_entries_rev == NULL) {
		otx2_err("free_entries_rev alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->live_entries = rte_zmalloc(NULL, npc->flow_max_priority
					* sizeof(struct rte_bitmap *),
					0);
	if (npc->live_entries == NULL) {
		otx2_err("live_entries alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->live_entries_rev = rte_zmalloc(NULL, npc->flow_max_priority
					* sizeof(struct rte_bitmap *),
					0);
	if (npc->live_entries_rev == NULL) {
		otx2_err("live_entries_rev alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->flow_list = rte_zmalloc(NULL, npc->flow_max_priority
					* sizeof(struct otx2_flow_list),
					0);
	if (npc->flow_list == NULL) {
		otx2_err("flow_list alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc_mem = mem;
	for (idx = 0; idx < npc->flow_max_priority; idx++) {
		TAILQ_INIT(&npc->flow_list[idx]);

		npc->free_entries[idx] =
			rte_bitmap_init(npc->mcam_entries, mem, bmap_sz);
		mem += bmap_sz;

		npc->free_entries_rev[idx] =
			rte_bitmap_init(npc->mcam_entries, mem, bmap_sz);
		mem += bmap_sz;

		npc->live_entries[idx] =
			rte_bitmap_init(npc->mcam_entries, mem, bmap_sz);
		mem += bmap_sz;

		npc->live_entries_rev[idx] =
			rte_bitmap_init(npc->mcam_entries, mem, bmap_sz);
		mem += bmap_sz;

		npc->flow_entry_info[idx].free_ent = 0;
		npc->flow_entry_info[idx].live_ent = 0;
		npc->flow_entry_info[idx].max_id = 0;
		npc->flow_entry_info[idx].min_id = ~(0);
	}

	npc->rss_grps = NIX_RSS_GRPS;

	bmap_sz = rte_bitmap_get_memory_footprint(npc->rss_grps);
	nix_mem = rte_zmalloc(NULL, bmap_sz,  RTE_CACHE_LINE_SIZE);
	if (nix_mem == NULL) {
		otx2_err("Bmap alloc failed");
		rc = -ENOMEM;
		goto err;
	}

	npc->rss_grp_entries = rte_bitmap_init(npc->rss_grps, nix_mem, bmap_sz);

	/* Group 0 will be used for RSS,
	 * 1 -7 will be used for rte_flow RSS action
	 */
	rte_bitmap_set(npc->rss_grp_entries, 0);

	return 0;

err:
	if (npc->flow_list)
		rte_free(npc->flow_list);
	if (npc->live_entries_rev)
		rte_free(npc->live_entries_rev);
	if (npc->live_entries)
		rte_free(npc->live_entries);
	if (npc->free_entries_rev)
		rte_free(npc->free_entries_rev);
	if (npc->free_entries)
		rte_free(npc->free_entries);
	if (npc->flow_entry_info)
		rte_free(npc->flow_entry_info);
	if (npc_mem)
		rte_free(npc_mem);
	return rc;
}

int
otx2_flow_fini(struct otx2_eth_dev *hw)
{
	struct otx2_npc_flow_info *npc = &hw->npc_flow;
	int rc;

	rc = otx2_flow_free_all_resources(hw);
	if (rc) {
		otx2_err("Error when deleting NPC MCAM entries, counters");
		return rc;
	}

	if (npc->flow_list)
		rte_free(npc->flow_list);
	if (npc->live_entries_rev)
		rte_free(npc->live_entries_rev);
	if (npc->live_entries)
		rte_free(npc->live_entries);
	if (npc->free_entries_rev)
		rte_free(npc->free_entries_rev);
	if (npc->free_entries)
		rte_free(npc->free_entries);
	if (npc->flow_entry_info)
		rte_free(npc->flow_entry_info);

	return 0;
}
