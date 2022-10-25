/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_flow.h>
#include <rte_bitmap.h>
#include "base/ice_type.h"
#include "base/ice_acl.h"
#include "ice_logs.h"
#include "ice_ethdev.h"
#include "ice_generic_flow.h"
#include "base/ice_flow.h"

#define MAX_ACL_SLOTS_ID 2048

#define ICE_ACL_INSET_ETH_IPV4 ( \
	ICE_INSET_SMAC | ICE_INSET_DMAC | \
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST)
#define ICE_ACL_INSET_ETH_IPV4_UDP ( \
	ICE_ACL_INSET_ETH_IPV4 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)
#define ICE_ACL_INSET_ETH_IPV4_TCP ( \
	ICE_ACL_INSET_ETH_IPV4 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)
#define ICE_ACL_INSET_ETH_IPV4_SCTP ( \
	ICE_ACL_INSET_ETH_IPV4 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

static struct ice_flow_parser ice_acl_parser;

struct acl_rule {
	enum ice_fltr_ptype flow_type;
	uint64_t entry_id[4];
};

static struct
ice_pattern_match_item ice_acl_pattern[] = {
	{pattern_eth_ipv4,	ICE_ACL_INSET_ETH_IPV4,		ICE_INSET_NONE,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,	ICE_ACL_INSET_ETH_IPV4_UDP,	ICE_INSET_NONE,	ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,	ICE_ACL_INSET_ETH_IPV4_TCP,	ICE_INSET_NONE,	ICE_INSET_NONE},
	{pattern_eth_ipv4_sctp,	ICE_ACL_INSET_ETH_IPV4_SCTP,	ICE_INSET_NONE,	ICE_INSET_NONE},
};

static int
ice_acl_prof_alloc(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype, fltr_ptype;

	if (!hw->acl_prof) {
		hw->acl_prof = (struct ice_fd_hw_prof **)
			ice_malloc(hw, ICE_FLTR_PTYPE_MAX *
				   sizeof(*hw->acl_prof));
		if (!hw->acl_prof)
			return -ENOMEM;
	}

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX; ptype++) {
		if (!hw->acl_prof[ptype]) {
			hw->acl_prof[ptype] = (struct ice_fd_hw_prof *)
				ice_malloc(hw, sizeof(**hw->acl_prof));
			if (!hw->acl_prof[ptype])
				goto fail_mem;
		}
	}

	return 0;

fail_mem:
	for (fltr_ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     fltr_ptype < ptype; fltr_ptype++) {
		rte_free(hw->acl_prof[fltr_ptype]);
		hw->acl_prof[fltr_ptype] = NULL;
	}

	rte_free(hw->acl_prof);
	hw->acl_prof = NULL;

	return -ENOMEM;
}

/**
 * ice_acl_setup - Reserve and initialize the ACL resources
 * @pf: board private structure
 */
static int
ice_acl_setup(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	uint32_t pf_num = hw->dev_caps.num_funcs;
	struct ice_acl_tbl_params params;
	uint16_t scen_id;
	int err = 0;

	memset(&params, 0, sizeof(params));

	/* create for IPV4 table */
	if (pf_num < 4)
		params.width = ICE_AQC_ACL_KEY_WIDTH_BYTES * 6;
	else
		params.width = ICE_AQC_ACL_KEY_WIDTH_BYTES * 3;

	params.depth = ICE_AQC_ACL_TCAM_DEPTH;
	params.entry_act_pairs = 1;
	params.concurr = false;

	err = ice_acl_create_tbl(hw, &params);
	if (err)
		return err;

	err = ice_acl_create_scen(hw, params.width, params.depth,
				  &scen_id);
	if (err)
		return err;

	return 0;
}

/**
 * ice_deinit_acl - Unroll the initialization of the ACL block
 * @pf: ptr to PF device
 *
 * returns 0 on success, negative on error
 */
static void ice_deinit_acl(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	ice_acl_destroy_tbl(hw);

	rte_free(hw->acl_tbl);
	hw->acl_tbl = NULL;

	if (pf->acl.slots) {
		rte_free(pf->acl.slots);
		pf->acl.slots = NULL;
	}
}

static void
acl_add_prof_prepare(struct ice_hw *hw, struct ice_flow_seg_info *seg,
		     bool is_l4, uint16_t src_port, uint16_t dst_port)
{
	uint16_t val_loc, mask_loc;

	if (hw->dev_caps.num_funcs < 4) {
		/* mac source address */
		val_loc = offsetof(struct ice_fdir_fltr,
				   ext_data.src_mac);
		mask_loc = offsetof(struct ice_fdir_fltr,
				    ext_mask.src_mac);
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_ETH_SA,
				 val_loc, mask_loc,
				 ICE_FLOW_FLD_OFF_INVAL, false);

		/* mac destination address */
		val_loc = offsetof(struct ice_fdir_fltr,
				   ext_data.dst_mac);
		mask_loc = offsetof(struct ice_fdir_fltr,
				    ext_mask.dst_mac);
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_ETH_DA,
				 val_loc, mask_loc,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	}

	/* IP source address */
	val_loc = offsetof(struct ice_fdir_fltr, ip.v4.src_ip);
	mask_loc = offsetof(struct ice_fdir_fltr, mask.v4.src_ip);
	ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_SA, val_loc,
			 mask_loc, ICE_FLOW_FLD_OFF_INVAL, false);

	/* IP destination address */
	val_loc = offsetof(struct ice_fdir_fltr, ip.v4.dst_ip);
	mask_loc = offsetof(struct ice_fdir_fltr, mask.v4.dst_ip);
	ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_DA, val_loc,
			 mask_loc, ICE_FLOW_FLD_OFF_INVAL, false);

	if (is_l4) {
		/* Layer 4 source port */
		val_loc = offsetof(struct ice_fdir_fltr, ip.v4.src_port);
		mask_loc = offsetof(struct ice_fdir_fltr, mask.v4.src_port);
		ice_flow_set_fld(seg, src_port, val_loc,
				 mask_loc, ICE_FLOW_FLD_OFF_INVAL, false);

		/* Layer 4 destination port */
		val_loc = offsetof(struct ice_fdir_fltr, ip.v4.dst_port);
		mask_loc = offsetof(struct ice_fdir_fltr, mask.v4.dst_port);
		ice_flow_set_fld(seg, dst_port, val_loc,
				 mask_loc, ICE_FLOW_FLD_OFF_INVAL, false);
	}
}

/**
 * ice_acl_prof_init - Initialize ACL profile
 * @pf: ice PF structure
 *
 * Returns 0 on success.
 */
static int
ice_acl_prof_init(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_flow_prof *prof_ipv4 = NULL;
	struct ice_flow_prof *prof_ipv4_udp = NULL;
	struct ice_flow_prof *prof_ipv4_tcp = NULL;
	struct ice_flow_prof *prof_ipv4_sctp = NULL;
	struct ice_flow_seg_info *seg;
	int i;
	int ret;

	seg = (struct ice_flow_seg_info *)
		 ice_malloc(hw, sizeof(*seg));
	if (!seg)
		return -ENOMEM;

	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4);
	acl_add_prof_prepare(hw, seg, false, 0, 0);
	ret = ice_flow_add_prof(hw, ICE_BLK_ACL, ICE_FLOW_RX,
				ICE_FLTR_PTYPE_NONF_IPV4_OTHER,
				seg, 1, NULL, 0, &prof_ipv4);
	if (ret)
		goto err_add_prof;

	ice_memset(seg, 0, sizeof(*seg), ICE_NONDMA_MEM);
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV4);
	acl_add_prof_prepare(hw, seg, true,
			     ICE_FLOW_FIELD_IDX_UDP_SRC_PORT,
			     ICE_FLOW_FIELD_IDX_UDP_DST_PORT);
	ret = ice_flow_add_prof(hw, ICE_BLK_ACL, ICE_FLOW_RX,
				ICE_FLTR_PTYPE_NONF_IPV4_UDP,
				seg, 1, NULL, 0, &prof_ipv4_udp);
	if (ret)
		goto err_add_prof_ipv4_udp;

	ice_memset(seg, 0, sizeof(*seg), ICE_NONDMA_MEM);
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV4);
	acl_add_prof_prepare(hw, seg, true,
			     ICE_FLOW_FIELD_IDX_TCP_SRC_PORT,
			     ICE_FLOW_FIELD_IDX_TCP_DST_PORT);
	ret = ice_flow_add_prof(hw, ICE_BLK_ACL, ICE_FLOW_RX,
				ICE_FLTR_PTYPE_NONF_IPV4_TCP,
				seg, 1, NULL, 0, &prof_ipv4_tcp);
	if (ret)
		goto err_add_prof_ipv4_tcp;

	ice_memset(seg, 0, sizeof(*seg), ICE_NONDMA_MEM);
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV4);
	acl_add_prof_prepare(hw, seg, true,
			     ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT,
			     ICE_FLOW_FIELD_IDX_SCTP_DST_PORT);
	ret = ice_flow_add_prof(hw, ICE_BLK_ACL, ICE_FLOW_RX,
				ICE_FLTR_PTYPE_NONF_IPV4_SCTP,
				seg, 1, NULL, 0, &prof_ipv4_sctp);
	if (ret)
		goto err_add_prof_ipv4_sctp;

	for (i = 0; i < pf->main_vsi->idx; i++) {
		ret = ice_flow_assoc_prof(hw, ICE_BLK_ACL, prof_ipv4, i);
		if (ret)
			goto err_assoc_prof;

		ret = ice_flow_assoc_prof(hw, ICE_BLK_ACL, prof_ipv4_udp, i);
		if (ret)
			goto err_assoc_prof;

		ret = ice_flow_assoc_prof(hw, ICE_BLK_ACL, prof_ipv4_tcp, i);
		if (ret)
			goto err_assoc_prof;

		ret = ice_flow_assoc_prof(hw, ICE_BLK_ACL, prof_ipv4_sctp, i);
		if (ret)
			goto err_assoc_prof;
	}
	return 0;

err_assoc_prof:
	ice_flow_rem_prof(hw, ICE_BLK_ACL, ICE_FLTR_PTYPE_NONF_IPV4_SCTP);
err_add_prof_ipv4_sctp:
	ice_flow_rem_prof(hw, ICE_BLK_ACL, ICE_FLTR_PTYPE_NONF_IPV4_TCP);
err_add_prof_ipv4_tcp:
	ice_flow_rem_prof(hw, ICE_BLK_ACL, ICE_FLTR_PTYPE_NONF_IPV4_UDP);
err_add_prof_ipv4_udp:
	ice_flow_rem_prof(hw, ICE_BLK_ACL, ICE_FLTR_PTYPE_NONF_IPV4_OTHER);
err_add_prof:
	ice_free(hw, seg);
	return ret;
}

/**
 * ice_acl_set_input_set - Helper function to set the input set for ACL
 * @hw: pointer to HW instance
 * @filter: pointer to ACL info
 * @input: filter structure
 *
 * Return error value or 0 on success.
 */
static int
ice_acl_set_input_set(struct ice_acl_conf *filter, struct ice_fdir_fltr *input)
{
	if (!input)
		return ICE_ERR_BAD_PTR;

	input->q_index = filter->input.q_index;
	input->dest_vsi = filter->input.dest_vsi;
	input->dest_ctl = filter->input.dest_ctl;
	input->fltr_status = ICE_FLTR_PRGM_DESC_FD_STATUS_FD_ID;
	input->flow_type = filter->input.flow_type;

	switch (input->flow_type) {
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		input->ip.v4.dst_port = filter->input.ip.v4.dst_port;
		input->ip.v4.src_port = filter->input.ip.v4.src_port;
		input->ip.v4.dst_ip = filter->input.ip.v4.dst_ip;
		input->ip.v4.src_ip = filter->input.ip.v4.src_ip;

		input->mask.v4.dst_port = filter->input.mask.v4.dst_port;
		input->mask.v4.src_port = filter->input.mask.v4.src_port;
		input->mask.v4.dst_ip = filter->input.mask.v4.dst_ip;
		input->mask.v4.src_ip = filter->input.mask.v4.src_ip;

		ice_memcpy(&input->ext_data.src_mac,
			   &filter->input.ext_data.src_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(&input->ext_mask.src_mac,
			   &filter->input.ext_mask.src_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(&input->ext_data.dst_mac,
			   &filter->input.ext_data.dst_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);
		ice_memcpy(&input->ext_mask.dst_mac,
			   &filter->input.ext_mask.dst_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);

		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		ice_memcpy(&input->ip.v4, &filter->input.ip.v4,
			   sizeof(struct ice_fdir_v4),
			   ICE_NONDMA_TO_NONDMA);
		ice_memcpy(&input->mask.v4, &filter->input.mask.v4,
			   sizeof(struct ice_fdir_v4),
			   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(&input->ext_data.src_mac,
			   &filter->input.ext_data.src_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);
		ice_memcpy(&input->ext_mask.src_mac,
			   &filter->input.ext_mask.src_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(&input->ext_data.dst_mac,
			   &filter->input.ext_data.dst_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);
		ice_memcpy(&input->ext_mask.dst_mac,
			   &filter->input.ext_mask.dst_mac,
			   RTE_ETHER_ADDR_LEN,
			   ICE_NONDMA_TO_NONDMA);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline int
ice_acl_alloc_slot_id(struct rte_bitmap *slots, uint32_t *slot_id)
{
	uint32_t pos = 0;
	uint64_t slab = 0;
	uint32_t i = 0;

	__rte_bitmap_scan_init(slots);
	if (!rte_bitmap_scan(slots, &pos, &slab))
		return -rte_errno;

	i = rte_bsf64(slab);
	pos += i;
	rte_bitmap_clear(slots, pos);

	*slot_id = pos;
	return 0;
}

static inline int
ice_acl_hw_set_conf(struct ice_pf *pf, struct ice_fdir_fltr *input,
		    struct ice_flow_action *acts, struct acl_rule *rule,
		    enum ice_fltr_ptype flow_type, int32_t entry_idx)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	enum ice_block blk = ICE_BLK_ACL;
	uint64_t entry_id, hw_entry;
	uint32_t slot_id = 0;
	int act_cnt = 1;
	int ret = 0;

	/* Allocate slot_id from bitmap table. */
	ret = ice_acl_alloc_slot_id(pf->acl.slots, &slot_id);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to alloc slot id.");
		return ret;
	}

	/* For IPV4_OTHER type, should add entry for all types.
	 * For IPV4_UDP/TCP/SCTP type, only add entry for each.
	 */
	if (slot_id < MAX_ACL_NORMAL_ENTRIES) {
		entry_id = ((uint64_t)flow_type << 32) | slot_id;
		ret = ice_flow_add_entry(hw, blk, flow_type,
					 entry_id, pf->main_vsi->idx,
					 ICE_FLOW_PRIO_NORMAL, input,
					 acts, act_cnt, &hw_entry);
		if (ret) {
			PMD_DRV_LOG(ERR, "Fail to add entry.");
			return ret;
		}
		rule->entry_id[entry_idx] = entry_id;
		pf->acl.hw_entry_id[slot_id] = hw_entry;
	} else {
		PMD_DRV_LOG(ERR, "Exceed the maximum entry number(%d)"
			    " HW supported!", MAX_ACL_NORMAL_ENTRIES);
		return -1;
	}

	return 0;
}

static inline void
ice_acl_del_entry(struct ice_hw *hw, uint64_t entry_id)
{
	uint64_t hw_entry;

	hw_entry = ice_flow_find_entry(hw, ICE_BLK_ACL, entry_id);
	ice_flow_rem_entry(hw, ICE_BLK_ACL, hw_entry);
}

static inline void
ice_acl_hw_rem_conf(struct ice_pf *pf, struct acl_rule *rule, int32_t entry_idx)
{
	uint32_t slot_id;
	int32_t i;
	uint64_t entry_id;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	for (i = 0; i < entry_idx; i++) {
		entry_id = rule->entry_id[i];
		slot_id = ICE_LO_DWORD(entry_id);
		rte_bitmap_set(pf->acl.slots, slot_id);
		ice_acl_del_entry(hw, entry_id);
	}
}

static int
ice_acl_create_filter(struct ice_adapter *ad,
		      struct rte_flow *flow,
		      void *meta,
		      struct rte_flow_error *error)
{
	struct ice_acl_conf *filter = meta;
	enum ice_fltr_ptype flow_type = filter->input.flow_type;
	struct ice_flow_action acts[1];
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_fltr *input;
	struct acl_rule *rule;
	int ret;

	rule = rte_zmalloc("acl_rule", sizeof(*rule), 0);
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory for acl rule");
		return -rte_errno;
	}

	input = rte_zmalloc("acl_entry", sizeof(*input), 0);
	if (!input) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory for acl input");
		ret = -rte_errno;
		goto err_acl_input_alloc;
	}

	ret = ice_acl_set_input_set(filter, input);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "failed to set input set.");
		ret = -rte_errno;
		goto err_acl_set_input;
	}

	if (filter->input.dest_ctl == ICE_FLTR_PRGM_DESC_DEST_DROP_PKT) {
		acts[0].type = ICE_FLOW_ACT_DROP;
		acts[0].data.acl_act.mdid = ICE_MDID_RX_PKT_DROP;
		acts[0].data.acl_act.prio = 0x3;
		acts[0].data.acl_act.value = CPU_TO_LE16(0x1);
	}

	input->acl_fltr = true;
	ret = ice_acl_hw_set_conf(pf, input, acts, rule, flow_type, 0);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "failed to set hw configure.");
		ret = -rte_errno;
		return ret;
	}

	if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_OTHER) {
		ret = ice_acl_hw_set_conf(pf, input, acts, rule,
					  ICE_FLTR_PTYPE_NONF_IPV4_UDP, 1);
		if (ret)
			goto err_acl_hw_set_conf_udp;
		ret = ice_acl_hw_set_conf(pf, input, acts, rule,
					  ICE_FLTR_PTYPE_NONF_IPV4_TCP, 2);
		if (ret)
			goto err_acl_hw_set_conf_tcp;
		ret = ice_acl_hw_set_conf(pf, input, acts, rule,
					  ICE_FLTR_PTYPE_NONF_IPV4_SCTP, 3);
		if (ret)
			goto err_acl_hw_set_conf_sctp;
	}

	rule->flow_type = flow_type;
	flow->rule = rule;
	return 0;

err_acl_hw_set_conf_sctp:
	ice_acl_hw_rem_conf(pf, rule, 3);
err_acl_hw_set_conf_tcp:
	ice_acl_hw_rem_conf(pf, rule, 2);
err_acl_hw_set_conf_udp:
	ice_acl_hw_rem_conf(pf, rule, 1);
err_acl_set_input:
	rte_free(input);
err_acl_input_alloc:
	rte_free(rule);
	return ret;
}

static int
ice_acl_destroy_filter(struct ice_adapter *ad,
		       struct rte_flow *flow,
		       struct rte_flow_error *error __rte_unused)
{
	struct acl_rule *rule = (struct acl_rule *)flow->rule;
	uint32_t slot_id, i;
	uint64_t entry_id;
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int ret = 0;

	switch (rule->flow_type) {
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		for (i = 0; i < 4; i++) {
			entry_id = rule->entry_id[i];
			slot_id = ICE_LO_DWORD(entry_id);
			rte_bitmap_set(pf->acl.slots, slot_id);
			ice_acl_del_entry(hw, entry_id);
		}
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		entry_id = rule->entry_id[0];
		slot_id = ICE_LO_DWORD(entry_id);
		rte_bitmap_set(pf->acl.slots, slot_id);
		ice_acl_del_entry(hw, entry_id);
		break;
	default:
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Unsupported flow type.");
		break;
	}

	flow->rule = NULL;
	rte_free(rule);
	return ret;
}

static void
ice_acl_filter_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
	flow->rule = NULL;
}

static int
ice_acl_parse_action(__rte_unused struct ice_adapter *ad,
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error,
		     struct ice_acl_conf *filter)
{
	uint32_t dest_num = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			dest_num++;

			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DROP_PKT;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action.");
			return -rte_errno;
		}
	}

	if (dest_num == 0 || dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Unsupported action combination");
		return -rte_errno;
	}

	return 0;
}

static int
ice_acl_parse_pattern(__rte_unused struct ice_adapter *ad,
		       const struct rte_flow_item pattern[],
		       struct rte_flow_error *error,
		       struct ice_acl_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	uint64_t input_set = ICE_INSET_NONE;
	uint8_t flow_type = ICE_FLTR_PTYPE_NONF_NONE;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		item_type = item->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			if (eth_spec && eth_mask) {
				if (rte_is_broadcast_ether_addr(&eth_mask->hdr.src_addr) ||
				    rte_is_broadcast_ether_addr(&eth_mask->hdr.dst_addr)) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid mac addr mask");
					return -rte_errno;
				}

				if (!rte_is_zero_ether_addr(&eth_spec->hdr.src_addr) &&
				    !rte_is_zero_ether_addr(&eth_mask->hdr.src_addr)) {
					input_set |= ICE_INSET_SMAC;
					ice_memcpy(&filter->input.ext_data.src_mac,
						   &eth_spec->hdr.src_addr,
						   RTE_ETHER_ADDR_LEN,
						   ICE_NONDMA_TO_NONDMA);
					ice_memcpy(&filter->input.ext_mask.src_mac,
						   &eth_mask->hdr.src_addr,
						   RTE_ETHER_ADDR_LEN,
						   ICE_NONDMA_TO_NONDMA);
				}

				if (!rte_is_zero_ether_addr(&eth_spec->hdr.dst_addr) &&
				    !rte_is_zero_ether_addr(&eth_mask->hdr.dst_addr)) {
					input_set |= ICE_INSET_DMAC;
					ice_memcpy(&filter->input.ext_data.dst_mac,
						   &eth_spec->hdr.dst_addr,
						   RTE_ETHER_ADDR_LEN,
						   ICE_NONDMA_TO_NONDMA);
					ice_memcpy(&filter->input.ext_mask.dst_mac,
						   &eth_mask->hdr.dst_addr,
						   RTE_ETHER_ADDR_LEN,
						   ICE_NONDMA_TO_NONDMA);
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			if (ipv4_spec && ipv4_mask) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
				    ipv4_mask->hdr.total_length ||
				    ipv4_mask->hdr.packet_id ||
				    ipv4_mask->hdr.fragment_offset ||
				    ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid IPv4 mask.");
					return -rte_errno;
				}

				if (ipv4_mask->hdr.src_addr == UINT32_MAX ||
				    ipv4_mask->hdr.dst_addr == UINT32_MAX) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid IPv4 mask.");
					return -rte_errno;
				}

				if (ipv4_mask->hdr.src_addr) {
					filter->input.ip.v4.src_ip =
						ipv4_spec->hdr.src_addr;
					filter->input.mask.v4.src_ip =
						ipv4_mask->hdr.src_addr;

					input_set |= ICE_INSET_IPV4_SRC;
				}

				if (ipv4_mask->hdr.dst_addr) {
					filter->input.ip.v4.dst_ip =
						ipv4_spec->hdr.dst_addr;
					filter->input.mask.v4.dst_ip =
						ipv4_mask->hdr.dst_addr;

					input_set |= ICE_INSET_IPV4_DST;
				}
			}

			flow_type = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_TCP;

			if (tcp_spec && tcp_mask) {
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
				    tcp_mask->hdr.recv_ack ||
				    tcp_mask->hdr.data_off ||
				    tcp_mask->hdr.tcp_flags ||
				    tcp_mask->hdr.rx_win ||
				    tcp_mask->hdr.cksum ||
				    tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port == UINT16_MAX ||
				    tcp_mask->hdr.dst_port == UINT16_MAX) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid TCP mask");
					return -rte_errno;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    tcp_mask->hdr.src_port) {
					input_set |= ICE_INSET_TCP_SRC_PORT;
					filter->input.ip.v4.src_port =
						tcp_spec->hdr.src_port;
					filter->input.mask.v4.src_port =
						tcp_mask->hdr.src_port;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    tcp_mask->hdr.dst_port) {
					input_set |= ICE_INSET_TCP_DST_PORT;
					filter->input.ip.v4.dst_port =
						tcp_spec->hdr.dst_port;
					filter->input.mask.v4.dst_port =
						tcp_mask->hdr.dst_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP;

			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port == UINT16_MAX ||
				    udp_mask->hdr.dst_port == UINT16_MAX) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid UDP mask");
					return -rte_errno;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    udp_mask->hdr.src_port) {
					input_set |= ICE_INSET_UDP_SRC_PORT;
					filter->input.ip.v4.src_port =
						udp_spec->hdr.src_port;
					filter->input.mask.v4.src_port =
						udp_mask->hdr.src_port;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    udp_mask->hdr.dst_port) {
					input_set |= ICE_INSET_UDP_DST_PORT;
					filter->input.ip.v4.dst_port =
						udp_spec->hdr.dst_port;
					filter->input.mask.v4.dst_port =
						udp_mask->hdr.dst_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;

			if (sctp_spec && sctp_mask) {
				if (sctp_mask->hdr.src_port == UINT16_MAX ||
				    sctp_mask->hdr.dst_port == UINT16_MAX) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid SCTP mask");
					return -rte_errno;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    sctp_mask->hdr.src_port) {
					input_set |= ICE_INSET_SCTP_SRC_PORT;
					filter->input.ip.v4.src_port =
						sctp_spec->hdr.src_port;
					filter->input.mask.v4.src_port =
						sctp_mask->hdr.src_port;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				    sctp_mask->hdr.dst_port) {
					input_set |= ICE_INSET_SCTP_DST_PORT;
					filter->input.ip.v4.dst_port =
						sctp_spec->hdr.dst_port;
					filter->input.mask.v4.dst_port =
						sctp_mask->hdr.dst_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item,
				"Invalid pattern item.");
			return -rte_errno;
		}
	}

	filter->input.flow_type = flow_type;
	filter->input_set = input_set;

	return 0;
}

static int
ice_acl_parse(struct ice_adapter *ad,
	       struct ice_pattern_match_item *array,
	       uint32_t array_len,
	       const struct rte_flow_item pattern[],
	       const struct rte_flow_action actions[],
	       uint32_t priority,
	       void **meta,
	       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_acl_conf *filter = &pf->acl.conf;
	struct ice_pattern_match_item *item = NULL;
	uint64_t input_set;
	int ret;

	if (priority >= 1)
		return -rte_errno;

	memset(filter, 0, sizeof(*filter));
	item = ice_search_pattern_match_item(ad, pattern, array, array_len,
					     error);
	if (!item)
		return -rte_errno;

	ret = ice_acl_parse_pattern(ad, pattern, error, filter);
	if (ret)
		goto error;
	input_set = filter->input_set;
	if (!input_set || input_set & ~item->input_set_mask_o) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		ret = -rte_errno;
		goto error;
	}

	ret = ice_acl_parse_action(ad, actions, error, filter);
	if (ret)
		goto error;

	if (meta)
		*meta = filter;

error:
	rte_free(item);
	return ret;
}

static int
ice_acl_bitmap_init(struct ice_pf *pf)
{
	uint32_t bmp_size;
	void *mem = NULL;
	struct rte_bitmap *slots;
	int ret = 0;
	bmp_size = rte_bitmap_get_memory_footprint(MAX_ACL_SLOTS_ID);
	mem = rte_zmalloc("create_acl_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for acl bitmap.");
		return -rte_errno;
	}

	slots = rte_bitmap_init_with_all_set(MAX_ACL_SLOTS_ID, mem, bmp_size);
	if (slots == NULL) {
		PMD_DRV_LOG(ERR, "Failed to initialize acl bitmap.");
		ret = -rte_errno;
		goto err_acl_mem_alloc;
	}
	pf->acl.slots = slots;
	return 0;

err_acl_mem_alloc:
	rte_free(mem);
	return ret;
}

static int
ice_acl_init(struct ice_adapter *ad)
{
	int ret = 0;
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_flow_parser *parser = &ice_acl_parser;

	if (!ad->hw.dcf_enabled)
		return 0;

	ret = ice_acl_prof_alloc(hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "Cannot allocate memory for "
			    "ACL profile.");
		return -ENOMEM;
	}

	ret = ice_acl_setup(pf);
	if (ret)
		return ret;

	ret = ice_acl_bitmap_init(pf);
	if (ret)
		return ret;

	ret = ice_acl_prof_init(pf);
	if (ret)
		return ret;

	return ice_register_parser(parser, ad);
}

static void
ice_acl_prof_free(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype;

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX; ptype++) {
		rte_free(hw->acl_prof[ptype]);
		hw->acl_prof[ptype] = NULL;
	}

	rte_free(hw->acl_prof);
	hw->acl_prof = NULL;
}

static void
ice_acl_uninit(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_flow_parser *parser = &ice_acl_parser;

	if (ad->hw.dcf_enabled) {
		ice_unregister_parser(parser, ad);
		ice_deinit_acl(pf);
		ice_acl_prof_free(hw);
	}
}

static struct
ice_flow_engine ice_acl_engine = {
	.init = ice_acl_init,
	.uninit = ice_acl_uninit,
	.create = ice_acl_create_filter,
	.destroy = ice_acl_destroy_filter,
	.free = ice_acl_filter_free,
	.type = ICE_FLOW_ENGINE_ACL,
};

static struct
ice_flow_parser ice_acl_parser = {
	.engine = &ice_acl_engine,
	.array = ice_acl_pattern,
	.array_len = RTE_DIM(ice_acl_pattern),
	.parse_pattern_action = ice_acl_parse,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(ice_acl_engine_init)
{
	struct ice_flow_engine *engine = &ice_acl_engine;
	ice_register_flow_engine(engine);
}
