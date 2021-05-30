/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "ulp_rte_parser.h"
#include "ulp_matcher.h"
#include "ulp_utils.h"
#include "tfp.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_tun.h"
#include "ulp_template_db_tbl.h"

/* Local defines for the parsing functions */
#define ULP_VLAN_PRIORITY_SHIFT		13 /* First 3 bits */
#define ULP_VLAN_PRIORITY_MASK		0x700
#define ULP_VLAN_TAG_MASK		0xFFF /* Last 12 bits*/
#define ULP_UDP_PORT_VXLAN		4789

/* Utility function to skip the void items. */
static inline int32_t
ulp_rte_item_skip_void(const struct rte_flow_item **item, uint32_t increment)
{
	if (!*item)
		return 0;
	if (increment)
		(*item)++;
	while ((*item) && (*item)->type == RTE_FLOW_ITEM_TYPE_VOID)
		(*item)++;
	if (*item)
		return 1;
	return 0;
}

/* Utility function to update the field_bitmap */
static void
ulp_rte_parser_field_bitmap_update(struct ulp_rte_parser_params *params,
				   uint32_t idx,
				   enum bnxt_ulp_prsr_action prsr_act)
{
	struct ulp_rte_hdr_field *field;

	field = &params->hdr_field[idx];
	if (ulp_bitmap_notzero(field->mask, field->size)) {
		ULP_INDEX_BITMAP_SET(params->fld_bitmap.bits, idx);
		if (!(prsr_act & ULP_PRSR_ACT_MATCH_IGNORE))
			ULP_INDEX_BITMAP_SET(params->fld_s_bitmap.bits, idx);
		/* Not exact match */
		if (!ulp_bitmap_is_ones(field->mask, field->size))
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_WC_MATCH, 1);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

#define ulp_deference_struct(x, y) ((x) ? &((x)->y) : NULL)
/* Utility function to copy field spec and masks items */
static void
ulp_rte_prsr_fld_mask(struct ulp_rte_parser_params *params,
		      uint32_t *idx,
		      uint32_t size,
		      const void *spec_buff,
		      const void *mask_buff,
		      enum bnxt_ulp_prsr_action prsr_act)
{
	struct ulp_rte_hdr_field *field = &params->hdr_field[*idx];

	/* update the field size */
	field->size = size;

	/* copy the mask specifications only if mask is not null */
	if (!(prsr_act & ULP_PRSR_ACT_MASK_IGNORE) && mask_buff) {
		memcpy(field->mask, mask_buff, size);
		ulp_rte_parser_field_bitmap_update(params, *idx, prsr_act);
	}

	/* copy the protocol specifications only if mask is not null*/
	if (spec_buff && mask_buff && ulp_bitmap_notzero(mask_buff, size))
		memcpy(field->spec, spec_buff, size);

	/* Increment the index */
	*idx = *idx + 1;
}

/* Utility function to copy field spec and masks items */
static int32_t
ulp_rte_prsr_fld_size_validate(struct ulp_rte_parser_params *params,
			       uint32_t *idx,
			       uint32_t size)
{
	if (params->field_idx + size >= BNXT_ULP_PROTO_HDR_MAX) {
		BNXT_TF_DBG(ERR, "OOB for field processing %u\n", *idx);
		return -EINVAL;
	}
	*idx = params->field_idx;
	params->field_idx += size;
	return 0;
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_hdr_parse(const struct rte_flow_item pattern[],
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item *item = pattern;
	struct bnxt_ulp_rte_hdr_info *hdr_info;

	params->field_idx = BNXT_ULP_PROTO_HDR_SVIF_NUM;

	/* Set the computed flags for no vlan tags before parsing */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_NO_VTAG, 1);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_NO_VTAG, 1);

	/* Parse all the items in the pattern */
	while (item && item->type != RTE_FLOW_ITEM_TYPE_END) {
		/* get the header information from the flow_hdr_info table */
		hdr_info = &ulp_hdr_info[item->type];
		if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
			BNXT_TF_DBG(ERR,
				    "Truflow parser does not support type %d\n",
				    item->type);
			return BNXT_TF_RC_PARSE_ERR;
		} else if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_hdr_func) {
				if (hdr_info->proto_hdr_func(item, params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		item++;
	}
	/* update the implied SVIF */
	return ulp_rte_parser_implicit_match_port_process(params);
}

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow actions into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_act_parse(const struct rte_flow_action actions[],
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action *action_item = actions;
	struct bnxt_ulp_rte_act_info *hdr_info;

	/* Parse all the items in the pattern */
	while (action_item && action_item->type != RTE_FLOW_ACTION_TYPE_END) {
		/* get the header information from the flow_hdr_info table */
		hdr_info = &ulp_act_info[action_item->type];
		if (hdr_info->act_type ==
		    BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			BNXT_TF_DBG(ERR,
				    "Truflow parser does not support act %u\n",
				    action_item->type);
			return BNXT_TF_RC_ERROR;
		} else if (hdr_info->act_type ==
		    BNXT_ULP_ACT_TYPE_SUPPORTED) {
			/* call the registered callback handler */
			if (hdr_info->proto_act_func) {
				if (hdr_info->proto_act_func(action_item,
							     params) !=
				    BNXT_TF_RC_SUCCESS) {
					return BNXT_TF_RC_ERROR;
				}
			}
		}
		action_item++;
	}
	/* update the implied port details */
	ulp_rte_parser_implicit_act_port_process(params);
	return BNXT_TF_RC_SUCCESS;
}

/*
 * Function to handle the post processing of the computed
 * fields for the interface.
 */
static void
bnxt_ulp_comp_fld_intf_update(struct ulp_rte_parser_params *params)
{
	uint32_t ifindex;
	uint16_t port_id, parif;
	uint32_t mtype;
	enum bnxt_ulp_direction_type dir;

	/* get the direction details */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);

	/* read the port id details */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      port_id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "ParseErr:Portid is not valid\n");
		return;
	}

	if (dir == BNXT_ULP_DIR_INGRESS) {
		/* Set port PARIF */
		if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
					  BNXT_ULP_PHY_PORT_PARIF, &parif)) {
			BNXT_TF_DBG(ERR, "ParseErr:ifindex is not valid\n");
			return;
		}
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
				    parif);
	} else {
		/* Get the match port type */
		mtype = ULP_COMP_FLD_IDX_RD(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
		if (mtype == BNXT_ULP_INTF_TYPE_VF_REP) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP,
					    1);
			/* Set VF func PARIF */
			if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
						  BNXT_ULP_VF_FUNC_PARIF,
						  &parif)) {
				BNXT_TF_DBG(ERR,
					    "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_VF_FUNC_PARIF,
					    parif);

		} else {
			/* Set DRV func PARIF */
			if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
						  BNXT_ULP_DRV_FUNC_PARIF,
						  &parif)) {
				BNXT_TF_DBG(ERR,
					    "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
					    parif);
		}
		if (mtype == BNXT_ULP_INTF_TYPE_PF) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_MATCH_PORT_IS_PF,
					    1);
		}
	}
}

static int32_t
ulp_post_process_normal_flow(struct ulp_rte_parser_params *params)
{
	enum bnxt_ulp_intf_type match_port_type, act_port_type;
	enum bnxt_ulp_direction_type dir;
	uint32_t act_port_set;

	/* Get the computed details */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	match_port_type = ULP_COMP_FLD_IDX_RD(params,
					      BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);
	act_port_type = ULP_COMP_FLD_IDX_RD(params,
					    BNXT_ULP_CF_IDX_ACT_PORT_TYPE);
	act_port_set = ULP_COMP_FLD_IDX_RD(params,
					   BNXT_ULP_CF_IDX_ACT_PORT_IS_SET);

	/* set the flow direction in the proto and action header */
	if (dir == BNXT_ULP_DIR_EGRESS) {
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	}

	/* calculate the VF to VF flag */
	if (act_port_set && act_port_type == BNXT_ULP_INTF_TYPE_VF_REP &&
	    match_port_type == BNXT_ULP_INTF_TYPE_VF_REP)
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_VF_TO_VF, 1);

	/* Update the decrement ttl computational fields */
	if (ULP_BITMAP_ISSET(params->act_bitmap.bits,
			     BNXT_ULP_ACT_BIT_DEC_TTL)) {
		/*
		 * Check that vxlan proto is included and vxlan decap
		 * action is not set then decrement tunnel ttl.
		 * Similarly add GRE and NVGRE in future.
		 */
		if ((ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
				      BNXT_ULP_HDR_BIT_T_VXLAN) &&
		    !ULP_BITMAP_ISSET(params->act_bitmap.bits,
				      BNXT_ULP_ACT_BIT_VXLAN_DECAP))) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_T_DEC_TTL, 1);
		} else {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_DEC_TTL, 1);
		}
	}

	/* Merge the hdr_fp_bit into the proto header bit */
	params->hdr_bitmap.bits |= params->hdr_fp_bit.bits;

	/* Update the comp fld fid */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FID, params->fid);

	/* Update the computed interface parameters */
	bnxt_ulp_comp_fld_intf_update(params);

	/* TBD: Handle the flow rejection scenarios */
	return 0;
}

/*
 * Function to handle the post processing of the parsing details
 */
int32_t
bnxt_ulp_rte_parser_post_process(struct ulp_rte_parser_params *params)
{
	ulp_post_process_normal_flow(params);
	return ulp_post_process_tun_flow(params);
}

/*
 * Function to compute the flow direction based on the match port details
 */
static void
bnxt_ulp_rte_parser_direction_compute(struct ulp_rte_parser_params *params)
{
	enum bnxt_ulp_intf_type match_port_type;

	/* Get the match port type */
	match_port_type = ULP_COMP_FLD_IDX_RD(params,
					      BNXT_ULP_CF_IDX_MATCH_PORT_TYPE);

	/* If ingress flow and matchport is vf rep then dir is egress*/
	if ((params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) &&
	    match_port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
				    BNXT_ULP_DIR_EGRESS);
	} else {
		/* Assign the input direction */
		if (params->dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS)
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_INGRESS);
		else
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
	}
}

/* Function to handle the parsing of RTE Flow item PF Header. */
static int32_t
ulp_rte_parser_svif_set(struct ulp_rte_parser_params *params,
			uint32_t ifindex,
			uint16_t mask)
{
	uint16_t svif;
	enum bnxt_ulp_direction_type dir;
	struct ulp_rte_hdr_field *hdr_field;
	enum bnxt_ulp_svif_type svif_type;
	enum bnxt_ulp_intf_type port_type;

	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_SVIF_FLAG) !=
	    BNXT_ULP_INVALID_SVIF_VAL) {
		BNXT_TF_DBG(ERR,
			    "SVIF already set,multiple source not support'd\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Get port type details */
	port_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (port_type == BNXT_ULP_INTF_TYPE_INVALID) {
		BNXT_TF_DBG(ERR, "Invalid port type\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the match port type */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE, port_type);

	/* compute the direction */
	bnxt_ulp_rte_parser_direction_compute(params);

	/* Get the computed direction */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir == BNXT_ULP_DIR_INGRESS) {
		svif_type = BNXT_ULP_PHY_PORT_SVIF;
	} else {
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			svif_type = BNXT_ULP_VF_FUNC_SVIF;
		else
			svif_type = BNXT_ULP_DRV_FUNC_SVIF;
	}
	ulp_port_db_svif_get(params->ulp_ctx, ifindex, svif_type,
			     &svif);
	svif = rte_cpu_to_be_16(svif);
	hdr_field = &params->hdr_field[BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX];
	memcpy(hdr_field->spec, &svif, sizeof(svif));
	memcpy(hdr_field->mask, &mask, sizeof(mask));
	hdr_field->size = sizeof(svif);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    rte_be_to_cpu_16(svif));
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of the RTE port id */
int32_t
ulp_rte_parser_implicit_match_port_process(struct ulp_rte_parser_params *params)
{
	uint16_t port_id = 0;
	uint16_t svif_mask = 0xFFFF;
	uint32_t ifindex;
	int32_t rc = BNXT_TF_RC_ERROR;

	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_SVIF_FLAG) !=
	    BNXT_ULP_INVALID_SVIF_VAL)
		return BNXT_TF_RC_SUCCESS;

	/* SVIF not set. So get the port id */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);

	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      port_id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "ParseErr:Portid is not valid\n");
		return rc;
	}

	/* Update the SVIF details */
	rc = ulp_rte_parser_svif_set(params, ifindex, svif_mask);
	return rc;
}

/* Function to handle the implicit action port id */
int32_t
ulp_rte_parser_implicit_act_port_process(struct ulp_rte_parser_params *params)
{
	struct rte_flow_action action_item = {0};
	struct rte_flow_action_port_id port_id = {0};

	/* Read the action port set bit */
	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET)) {
		/* Already set, so just exit */
		return BNXT_TF_RC_SUCCESS;
	}
	port_id.id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);
	action_item.conf = &port_id;

	/* Update the action port based on incoming port */
	ulp_rte_port_id_act_handler(&action_item, params);

	/* Reset the action port set bit */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 0);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item PF Header. */
int32_t
ulp_rte_pf_hdr_handler(const struct rte_flow_item *item __rte_unused,
		       struct ulp_rte_parser_params *params)
{
	uint16_t port_id = 0;
	uint16_t svif_mask = 0xFFFF;
	uint32_t ifindex;

	/* Get the implicit port id */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);

	/* perform the conversion from dpdk port to bnxt ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      port_id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "ParseErr:Portid is not valid\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the SVIF details */
	return  ulp_rte_parser_svif_set(params, ifindex, svif_mask);
}

/* Function to handle the parsing of RTE Flow item VF Header. */
int32_t
ulp_rte_vf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vf *vf_spec = item->spec;
	const struct rte_flow_item_vf *vf_mask = item->mask;
	uint16_t mask = 0;
	uint32_t ifindex;
	int32_t rc = BNXT_TF_RC_PARSE_ERR;

	/* Get VF rte_flow_item for Port details */
	if (!vf_spec) {
		BNXT_TF_DBG(ERR, "ParseErr:VF id is not valid\n");
		return rc;
	}
	if (!vf_mask) {
		BNXT_TF_DBG(ERR, "ParseErr:VF mask is not valid\n");
		return rc;
	}
	mask = vf_mask->id;

	/* perform the conversion from VF Func id to bnxt ifindex */
	if (ulp_port_db_dev_func_id_to_ulp_index(params->ulp_ctx,
						 vf_spec->id,
						 &ifindex)) {
		BNXT_TF_DBG(ERR, "ParseErr:Portid is not valid\n");
		return rc;
	}
	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, ifindex, mask);
}

/* Function to handle the parsing of RTE Flow item port id  Header. */
int32_t
ulp_rte_port_id_hdr_handler(const struct rte_flow_item *item,
			    struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_port_id *port_spec = item->spec;
	const struct rte_flow_item_port_id *port_mask = item->mask;
	uint16_t mask = 0;
	int32_t rc = BNXT_TF_RC_PARSE_ERR;
	uint32_t ifindex;

	if (!port_spec) {
		BNXT_TF_DBG(ERR, "ParseErr:Port id is not valid\n");
		return rc;
	}
	if (!port_mask) {
		BNXT_TF_DBG(ERR, "ParseErr:Phy Port mask is not valid\n");
		return rc;
	}
	mask = port_mask->id;

	/* perform the conversion from dpdk port to bnxt ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      port_spec->id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "ParseErr:Portid is not valid\n");
		return rc;
	}
	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, ifindex, mask);
}

/* Function to handle the parsing of RTE Flow item phy port Header. */
int32_t
ulp_rte_phy_port_hdr_handler(const struct rte_flow_item *item,
			     struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_phy_port *port_spec = item->spec;
	const struct rte_flow_item_phy_port *port_mask = item->mask;
	uint16_t mask = 0;
	int32_t rc = BNXT_TF_RC_ERROR;
	uint16_t svif;
	enum bnxt_ulp_direction_type dir;
	struct ulp_rte_hdr_field *hdr_field;

	/* Copy the rte_flow_item for phy port into hdr_field */
	if (!port_spec) {
		BNXT_TF_DBG(ERR, "ParseErr:Phy Port id is not valid\n");
		return rc;
	}
	if (!port_mask) {
		BNXT_TF_DBG(ERR, "ParseErr:Phy Port mask is not valid\n");
		return rc;
	}
	mask = port_mask->index;

	/* Update the match port type */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE,
			    BNXT_ULP_INTF_TYPE_PHY_PORT);

	/* Compute the Hw direction */
	bnxt_ulp_rte_parser_direction_compute(params);

	/* Direction validation */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir == BNXT_ULP_DIR_EGRESS) {
		BNXT_TF_DBG(ERR,
			    "Parse Err:Phy ports are valid only for ingress\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	/* Get the physical port details from port db */
	rc = ulp_port_db_phy_port_svif_get(params->ulp_ctx, port_spec->index,
					   &svif);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get port details\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	/* Update the SVIF details */
	svif = rte_cpu_to_be_16(svif);
	hdr_field = &params->hdr_field[BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX];
	memcpy(hdr_field->spec, &svif, sizeof(svif));
	memcpy(hdr_field->mask, &mask, sizeof(mask));
	hdr_field->size = sizeof(svif);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    rte_be_to_cpu_16(svif));
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l2_proto_type_update(struct ulp_rte_parser_params *param,
			     uint16_t type, uint32_t in_flag)
{
	if (type == tfp_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV4);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	} else if (type == tfp_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))  {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	}
}

/* Internal Function to identify broadcast or multicast packets */
static int32_t
ulp_rte_parser_is_bcmc_addr(const struct rte_ether_addr *eth_addr)
{
	if (rte_is_multicast_ether_addr(eth_addr) ||
	    rte_is_broadcast_ether_addr(eth_addr)) {
		BNXT_TF_DBG(DEBUG,
			    "No support for bcast or mcast addr offload\n");
		return 1;
	}
	return 0;
}

/* Function to handle the parsing of RTE Flow item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = item->mask;
	uint32_t idx = 0;
	uint32_t size;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;

	/* Perform validations */
	if (eth_spec) {
		/* Todo: work around to avoid multicast and broadcast addr */
		if (ulp_rte_parser_is_bcmc_addr(&eth_spec->dst))
			return BNXT_TF_RC_PARSE_ERR;

		if (ulp_rte_parser_is_bcmc_addr(&eth_spec->src))
			return BNXT_TF_RC_PARSE_ERR;

		eth_type = eth_spec->type;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_ETH_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}
	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_eth *)NULL)->dst.addr_bytes);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, dst.addr_bytes),
			      ulp_deference_struct(eth_mask, dst.addr_bytes),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->src.addr_bytes);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, src.addr_bytes),
			      ulp_deference_struct(eth_mask, src.addr_bytes),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->type);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, type),
			      ulp_deference_struct(eth_mask, type),
			      ULP_PRSR_ACT_MATCH_IGNORE);

	/* Update the protocol hdr bitmap */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_ETH) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_I_ETH);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vlan Header. */
int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = item->mask;
	struct ulp_rte_hdr_bitmap	*hdr_bit;
	uint32_t idx = 0;
	uint16_t vlan_tag = 0, priority = 0;
	uint16_t vlan_tag_mask = 0, priority_mask = 0;
	uint32_t outer_vtag_num;
	uint32_t inner_vtag_num;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;
	uint32_t size;

	if (vlan_spec) {
		vlan_tag = ntohs(vlan_spec->tci);
		priority = htons(vlan_tag >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag &= ULP_VLAN_TAG_MASK;
		vlan_tag = htons(vlan_tag);
		eth_type = vlan_spec->inner_type;
	}

	if (vlan_mask) {
		vlan_tag_mask = ntohs(vlan_mask->tci);
		priority_mask = htons(vlan_tag_mask >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag_mask &= 0xfff;

		/*
		 * the storage for priority and vlan tag is 2 bytes
		 * The mask of priority which is 3 bits if it is all 1's
		 * then make the rest bits 13 bits as 1's
		 * so that it is matched as exact match.
		 */
		if (priority_mask == ULP_VLAN_PRIORITY_MASK)
			priority_mask |= ~ULP_VLAN_PRIORITY_MASK;
		if (vlan_tag_mask == ULP_VLAN_TAG_MASK)
			vlan_tag_mask |= ~ULP_VLAN_TAG_MASK;
		vlan_tag_mask = htons(vlan_tag_mask);
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_S_VLAN_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vlan *)NULL)->tci);
	/*
	 * The priority field is ignored since OVS is setting it as
	 * wild card match and it is not supported. This is a work
	 * around and shall be addressed in the future.
	 */
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      &priority,
			      &priority_mask,
			      ULP_PRSR_ACT_MASK_IGNORE);

	ulp_rte_prsr_fld_mask(params, &idx, size,
			      &vlan_tag,
			      &vlan_tag_mask,
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vlan *)NULL)->inner_type);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vlan_spec, inner_type),
			      ulp_deference_struct(vlan_mask, inner_type),
			      ULP_PRSR_ACT_MATCH_IGNORE);

	/* Get the outer tag and inner tag counts */
	outer_vtag_num = ULP_COMP_FLD_IDX_RD(params,
					     BNXT_ULP_CF_IDX_O_VTAG_NUM);
	inner_vtag_num = ULP_COMP_FLD_IDX_RD(params,
					     BNXT_ULP_CF_IDX_I_VTAG_NUM);

	/* Update the hdr_bitmap of the vlans */
	hdr_bit = &params->hdr_bitmap;
	if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
	    !outer_vtag_num) {
		/* Update the vlan tag num */
		outer_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_NO_VTAG, 0);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OO_VLAN);
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   !ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   outer_vtag_num == 1) {
		/* update the vlan tag num */
		outer_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_VTAG_NUM,
				    outer_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_TWO_VTAGS, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_ONE_VTAG, 0);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OI_VLAN);
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   !inner_vtag_num) {
		/* update the vlan tag num */
		inner_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_NO_VTAG, 0);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_IO_VLAN);
		inner_flag = 1;
	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   inner_vtag_num == 1) {
		/* update the vlan tag num */
		inner_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_TWO_VTAGS, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_ONE_VTAG, 0);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_II_VLAN);
		inner_flag = 1;
	} else {
		BNXT_TF_DBG(ERR, "Error Parsing:Vlan hdr found without eth\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l3_proto_type_update(struct ulp_rte_parser_params *param,
			     uint8_t proto, uint32_t in_flag)
{
	if (proto == IPPROTO_UDP) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_UDP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L4, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_UDP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L4, 1);
		}
	} else if (proto == IPPROTO_TCP) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_TCP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L4, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_TCP);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L4, 1);
		}
	} else if (proto == IPPROTO_GRE) {
		ULP_BITMAP_SET(param->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_T_GRE);
	} else if (proto == IPPROTO_ICMP) {
		if (ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_L3_TUN))
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_I_ICMP);
		else
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_O_ICMP);
	}
	if (proto) {
		if (in_flag) {
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
					    1);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
					    proto);
		} else {
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
					    1);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
					    proto);
		}
	}
}

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;
	uint8_t proto = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_I_ETH)) {
		/* Since F2 flow does not include eth item, when parser detects
		 * IPv4/IPv6 item list and it belongs to the outer header; i.e.,
		 * o_ipv4/o_ipv6, check if O_ETH and I_ETH is set. If not set,
		 * then add offset sizeof(o_eth/oo_vlan/oi_vlan) to the index.
		 * This will allow the parser post processor to update the
		 * t_dmac in hdr_field[o_eth.dmac]
		 */
		idx += (BNXT_ULP_PROTO_HDR_ETH_NUM +
			BNXT_ULP_PROTO_HDR_VLAN_NUM);
		params->field_idx = idx;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_IPV4_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.version_ihl);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.version_ihl),
			      ulp_deference_struct(ipv4_mask, hdr.version_ihl),
			      ULP_PRSR_ACT_DEFAULT);

	/*
	 * The tos field is ignored since OVS is setting it as wild card
	 * match and it is not supported. This is a work around and
	 * shall be addressed in the future.
	 */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.type_of_service);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec,
						   hdr.type_of_service),
			      ulp_deference_struct(ipv4_mask,
						   hdr.type_of_service),
			      ULP_PRSR_ACT_MASK_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.total_length);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.total_length),
			      ulp_deference_struct(ipv4_mask, hdr.total_length),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.packet_id);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.packet_id),
			      ulp_deference_struct(ipv4_mask, hdr.packet_id),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.fragment_offset);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec,
						   hdr.fragment_offset),
			      ulp_deference_struct(ipv4_mask,
						   hdr.fragment_offset),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.time_to_live);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.time_to_live),
			      ulp_deference_struct(ipv4_mask, hdr.time_to_live),
			      ULP_PRSR_ACT_DEFAULT);

	/* Ignore proto for matching templates */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.next_proto_id);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec,
						   hdr.next_proto_id),
			      ulp_deference_struct(ipv4_mask,
						   hdr.next_proto_id),
			      ULP_PRSR_ACT_MATCH_IGNORE);
	if (ipv4_spec)
		proto = ipv4_spec->hdr.next_proto_id;

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.hdr_checksum);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.hdr_checksum),
			      ulp_deference_struct(ipv4_mask, hdr.hdr_checksum),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.src_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.src_addr),
			      ulp_deference_struct(ipv4_mask, hdr.src_addr),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.dst_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.dst_addr),
			      ulp_deference_struct(ipv4_mask, hdr.dst_addr),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv4 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
	}

	/* Some of the PMD applications may set the protocol field
	 * in the IPv4 spec but don't set the mask. So, consider
	 * the mask in the proto value calculation.
	 */
	if (ipv4_mask)
		proto &= ipv4_mask->hdr.next_proto_id;

	/* Update the field protocol hdr bitmap */
	ulp_rte_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item IPV6 Header */
int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv6	*ipv6_spec = item->spec;
	const struct rte_flow_item_ipv6	*ipv6_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;
	uint32_t ver_spec = 0, ver_mask = 0;
	uint32_t tc_spec = 0, tc_mask = 0;
	uint32_t lab_spec = 0, lab_mask = 0;
	uint8_t proto = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_O_ETH) &&
	    !ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			      BNXT_ULP_HDR_BIT_I_ETH)) {
		/* Since F2 flow does not include eth item, when parser detects
		 * IPv4/IPv6 item list and it belongs to the outer header; i.e.,
		 * o_ipv4/o_ipv6, check if O_ETH and I_ETH is set. If not set,
		 * then add offset sizeof(o_eth/oo_vlan/oi_vlan) to the index.
		 * This will allow the parser post processor to update the
		 * t_dmac in hdr_field[o_eth.dmac]
		 */
		idx += (BNXT_ULP_PROTO_HDR_ETH_NUM +
			BNXT_ULP_PROTO_HDR_VLAN_NUM);
		params->field_idx = idx;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_IPV6_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv6 into hdr_field using ipv6
	 * header fields
	 */
	if (ipv6_spec) {
		ver_spec = BNXT_ULP_GET_IPV6_VER(ipv6_spec->hdr.vtc_flow);
		tc_spec = BNXT_ULP_GET_IPV6_TC(ipv6_spec->hdr.vtc_flow);
		lab_spec = BNXT_ULP_GET_IPV6_FLOWLABEL(ipv6_spec->hdr.vtc_flow);
		proto = ipv6_spec->hdr.proto;
	}

	if (ipv6_mask) {
		ver_mask = BNXT_ULP_GET_IPV6_VER(ipv6_mask->hdr.vtc_flow);
		tc_mask = BNXT_ULP_GET_IPV6_TC(ipv6_mask->hdr.vtc_flow);
		lab_mask = BNXT_ULP_GET_IPV6_FLOWLABEL(ipv6_mask->hdr.vtc_flow);

		/* Some of the PMD applications may set the protocol field
		 * in the IPv6 spec but don't set the mask. So, consider
		 * the mask in proto value calculation.
		 */
		proto &= ipv6_mask->hdr.proto;
	}

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.vtc_flow);
	ulp_rte_prsr_fld_mask(params, &idx, size, &ver_spec, &ver_mask,
			      ULP_PRSR_ACT_DEFAULT);
	/*
	 * The TC and flow label field are ignored since OVS is setting
	 * it for match and it is not supported.
	 * This is a work around and
	 * shall be addressed in the future.
	 */
	ulp_rte_prsr_fld_mask(params, &idx, size, &tc_spec, &tc_mask,
			      ULP_PRSR_ACT_MASK_IGNORE);
	ulp_rte_prsr_fld_mask(params, &idx, size, &lab_spec, &lab_mask,
			      ULP_PRSR_ACT_MASK_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.payload_len);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.payload_len),
			      ulp_deference_struct(ipv6_mask, hdr.payload_len),
			      ULP_PRSR_ACT_DEFAULT);

	/* Ignore proto for template matching */
	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.proto);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.proto),
			      ulp_deference_struct(ipv6_mask, hdr.proto),
			      ULP_PRSR_ACT_MATCH_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.hop_limits);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.hop_limits),
			      ulp_deference_struct(ipv6_mask, hdr.hop_limits),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.src_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.src_addr),
			      ulp_deference_struct(ipv6_mask, hdr.src_addr),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.dst_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.dst_addr),
			      ulp_deference_struct(ipv6_mask, hdr.dst_addr),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv6 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
	}

	/* Update the field protocol hdr bitmap */
	ulp_rte_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l4_proto_type_update(struct ulp_rte_parser_params *param,
			     uint16_t dst_port)
{
	if (dst_port == tfp_cpu_to_be_16(ULP_UDP_PORT_VXLAN))
		ULP_BITMAP_SET(param->hdr_fp_bit.bits,
			       BNXT_ULP_HDR_BIT_T_VXLAN);

	if (ULP_BITMAP_ISSET(param->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_T_VXLAN) ||
	    ULP_BITMAP_ISSET(param->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_T_GRE))
		ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_L3_TUN, 1);
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;
	uint16_t dport = 0, sport = 0;
	uint32_t cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (udp_spec) {
		sport = udp_spec->hdr.src_port;
		dport = udp_spec->hdr.dst_port;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_UDP_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.src_port);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(udp_spec, hdr.src_port),
			      ulp_deference_struct(udp_mask, hdr.src_port),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dst_port);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(udp_spec, hdr.dst_port),
			      ulp_deference_struct(udp_mask, hdr.dst_port),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dgram_len);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(udp_spec, hdr.dgram_len),
			      ulp_deference_struct(udp_mask, hdr.dgram_len),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_udp *)NULL)->hdr.dgram_cksum);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(udp_spec, hdr.dgram_cksum),
			      ulp_deference_struct(udp_mask, hdr.dgram_cksum),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT,
				    (uint32_t)rte_be_to_cpu_16(sport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT,
				    (uint32_t)rte_be_to_cpu_16(dport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
				    IPPROTO_UDP);
		if (udp_mask && udp_mask->hdr.src_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_I_L4_FB_SRC_PORT,
					    1);
		if (udp_mask && udp_mask->hdr.dst_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_I_L4_FB_DST_PORT,
					    1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT,
				    (uint32_t)rte_be_to_cpu_16(sport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    (uint32_t)rte_be_to_cpu_16(dport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
				    IPPROTO_UDP);
		if (udp_mask && udp_mask->hdr.src_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_FB_SRC_PORT,
					    1);
		if (udp_mask && udp_mask->hdr.dst_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_FB_DST_PORT,
					    1);

		/* Update the field protocol hdr bitmap */
		ulp_rte_l4_proto_type_update(params, dport);
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item TCP Header. */
int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint16_t dport = 0, sport = 0;
	uint32_t size;
	uint32_t cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (tcp_spec) {
		sport = tcp_spec->hdr.src_port;
		dport = tcp_spec->hdr.dst_port;
	}

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_TCP_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.src_port);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.src_port),
			      ulp_deference_struct(tcp_mask, hdr.src_port),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.dst_port);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.dst_port),
			      ulp_deference_struct(tcp_mask, hdr.dst_port),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.sent_seq);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.sent_seq),
			      ulp_deference_struct(tcp_mask, hdr.sent_seq),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.recv_ack);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.recv_ack),
			      ulp_deference_struct(tcp_mask, hdr.recv_ack),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.data_off);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.data_off),
			      ulp_deference_struct(tcp_mask, hdr.data_off),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.tcp_flags);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.tcp_flags),
			      ulp_deference_struct(tcp_mask, hdr.tcp_flags),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.rx_win);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.rx_win),
			      ulp_deference_struct(tcp_mask, hdr.rx_win),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.cksum);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.cksum),
			      ulp_deference_struct(tcp_mask, hdr.cksum),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_tcp *)NULL)->hdr.tcp_urp);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(tcp_spec, hdr.tcp_urp),
			      ulp_deference_struct(tcp_mask, hdr.tcp_urp),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT,
				    (uint32_t)rte_be_to_cpu_16(sport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT,
				    (uint32_t)rte_be_to_cpu_16(dport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
				    IPPROTO_TCP);
		if (tcp_mask && tcp_mask->hdr.src_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_I_L4_FB_SRC_PORT,
					    1);
		if (tcp_mask && tcp_mask->hdr.dst_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_I_L4_FB_DST_PORT,
					    1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT,
				    (uint32_t)rte_be_to_cpu_16(sport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    (uint32_t)rte_be_to_cpu_16(dport));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
				    IPPROTO_TCP);
		if (tcp_mask && tcp_mask->hdr.src_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_FB_SRC_PORT,
					    1);
		if (tcp_mask && tcp_mask->hdr.dst_port)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_FB_DST_PORT,
					    1);
	}
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L4_HDR_CNT, ++cnt);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vxlan *vxlan_spec = item->spec;
	const struct rte_flow_item_vxlan *vxlan_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_VXLAN_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->flags);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, flags),
			      ulp_deference_struct(vxlan_mask, flags),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->rsvd0);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, rsvd0),
			      ulp_deference_struct(vxlan_mask, rsvd0),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->vni);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, vni),
			      ulp_deference_struct(vxlan_mask, vni),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->rsvd1);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, rsvd1),
			      ulp_deference_struct(vxlan_mask, rsvd1),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	ulp_rte_l4_proto_type_update(params, 0);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item GRE Header. */
int32_t
ulp_rte_gre_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_gre *gre_spec = item->spec;
	const struct rte_flow_item_gre *gre_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_GRE_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(((struct rte_flow_item_gre *)NULL)->c_rsvd0_ver);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(gre_spec, c_rsvd0_ver),
			      ulp_deference_struct(gre_mask, c_rsvd0_ver),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_gre *)NULL)->protocol);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(gre_spec, protocol),
			      ulp_deference_struct(gre_mask, protocol),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with GRE */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_GRE);
	ulp_rte_l4_proto_type_update(params, 0);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item ANY. */
int32_t
ulp_rte_item_any_handler(const struct rte_flow_item *item __rte_unused,
			 struct ulp_rte_parser_params *params __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item ICMP Header. */
int32_t
ulp_rte_icmp_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_icmp *icmp_spec = item->spec;
	const struct rte_flow_item_icmp *icmp_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_ICMP_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(((struct rte_flow_item_icmp *)NULL)->hdr.icmp_type);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, hdr.icmp_type),
			      ulp_deference_struct(icmp_mask, hdr.icmp_type),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp *)NULL)->hdr.icmp_code);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, hdr.icmp_code),
			      ulp_deference_struct(icmp_mask, hdr.icmp_code),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp *)NULL)->hdr.icmp_cksum);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, hdr.icmp_cksum),
			      ulp_deference_struct(icmp_mask, hdr.icmp_cksum),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp *)NULL)->hdr.icmp_ident);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, hdr.icmp_ident),
			      ulp_deference_struct(icmp_mask, hdr.icmp_ident),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp *)NULL)->hdr.icmp_seq_nb);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, hdr.icmp_seq_nb),
			      ulp_deference_struct(icmp_mask, hdr.icmp_seq_nb),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with ICMP */
	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_ICMP);
	else
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ICMP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item ICMP6 Header. */
int32_t
ulp_rte_icmp6_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_icmp6 *icmp_spec = item->spec;
	const struct rte_flow_item_icmp6 *icmp_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint32_t size;

	if (ulp_rte_prsr_fld_size_validate(params, &idx,
					   BNXT_ULP_PROTO_HDR_ICMP_NUM)) {
		BNXT_TF_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	size = sizeof(((struct rte_flow_item_icmp6 *)NULL)->type);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, type),
			      ulp_deference_struct(icmp_mask, type),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp6 *)NULL)->code);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, code),
			      ulp_deference_struct(icmp_mask, code),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_icmp6 *)NULL)->checksum);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(icmp_spec, checksum),
			      ulp_deference_struct(icmp_mask, checksum),
			      ULP_PRSR_ACT_DEFAULT);

	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4)) {
		BNXT_TF_DBG(ERR, "Error: incorrect icmp version\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the hdr_bitmap with ICMP */
	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_TUN))
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_ICMP);
	else
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ICMP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item void Header */
int32_t
ulp_rte_void_hdr_handler(const struct rte_flow_item *item __rte_unused,
			 struct ulp_rte_parser_params *params __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action void Header. */
int32_t
ulp_rte_void_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params __rte_unused)
{
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action Mark Header. */
int32_t
ulp_rte_mark_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_mark *mark;
	struct ulp_rte_act_bitmap *act = &param->act_bitmap;
	uint32_t mark_id;

	mark = action_item->conf;
	if (mark) {
		mark_id = tfp_cpu_to_be_32(mark->id);
		memcpy(&param->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_MARK],
		       &mark_id, BNXT_ULP_ACT_PROP_SZ_MARK);

		/* Update the hdr_bitmap with vxlan */
		ULP_BITMAP_SET(act->bits, BNXT_ULP_ACT_BIT_MARK);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: Mark arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action RSS Header. */
int32_t
ulp_rte_rss_act_handler(const struct rte_flow_action *action_item,
			struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_rss *rss;
	struct ulp_rte_act_prop *ap = &param->act_prop;

	if (action_item == NULL || action_item->conf == NULL) {
		BNXT_TF_DBG(ERR, "Parse Err: invalid rss configuration\n");
		return BNXT_TF_RC_ERROR;
	}

	rss = action_item->conf;
	/* Copy the rss into the specific action properties */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_TYPES], &rss->types,
	       BNXT_ULP_ACT_PROP_SZ_RSS_TYPES);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_LEVEL], &rss->level,
	       BNXT_ULP_ACT_PROP_SZ_RSS_LEVEL);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY_LEN],
	       &rss->key_len, BNXT_ULP_ACT_PROP_SZ_RSS_KEY_LEN);

	if (rss->key_len > BNXT_ULP_ACT_PROP_SZ_RSS_KEY) {
		BNXT_TF_DBG(ERR, "Parse Err: RSS key too big\n");
		return BNXT_TF_RC_ERROR;
	}
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY], rss->key,
	       rss->key_len);

	/* set the RSS action header bit */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACT_BIT_RSS);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action vxlan_encap Header. */
int32_t
ulp_rte_vxlan_encap_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_vxlan_encap *vxlan_encap;
	const struct rte_flow_item *item;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv6 *ipv6_spec;
	struct rte_flow_item_vxlan vxlan_spec;
	uint32_t vlan_num = 0, vlan_size = 0;
	uint32_t ip_size = 0, ip_type = 0;
	uint32_t vxlan_size = 0;
	uint8_t *buff;
	/* IP header per byte - ver/hlen, TOS, ID, ID, FRAG, FRAG, TTL, PROTO */
	const uint8_t def_ipv4_hdr[] = {0x45, 0x00, 0x00, 0x01, 0x00,
				    0x00, 0x40, 0x11};
	/* IPv6 header per byte - vtc-flow,flow,zero,nexthdr-ttl */
	const uint8_t def_ipv6_hdr[] = {0x60, 0x00, 0x00, 0x01, 0x00,
				0x00, 0x11, 0xf6};
	struct ulp_rte_act_bitmap *act = &params->act_bitmap;
	struct ulp_rte_act_prop *ap = &params->act_prop;
	const uint8_t *tmp_buff;

	vxlan_encap = action_item->conf;
	if (!vxlan_encap) {
		BNXT_TF_DBG(ERR, "Parse Error: Vxlan_encap arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	item = vxlan_encap->definition;
	if (!item) {
		BNXT_TF_DBG(ERR, "Parse Error: definition arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ulp_rte_item_skip_void(&item, 0))
		return BNXT_TF_RC_ERROR;

	/* must have ethernet header */
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		BNXT_TF_DBG(ERR, "Parse Error:vxlan encap does not have eth\n");
		return BNXT_TF_RC_ERROR;
	}
	eth_spec = item->spec;
	buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC];
	ulp_encap_buffer_copy(buff,
			      eth_spec->dst.addr_bytes,
			      BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC,
			      ULP_BUFFER_ALIGN_8_BYTE);

	buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC];
	ulp_encap_buffer_copy(buff,
			      eth_spec->src.addr_bytes,
			      BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC,
			      ULP_BUFFER_ALIGN_8_BYTE);

	/* Goto the next item */
	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* May have vlan header */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG];
		ulp_encap_buffer_copy(buff,
				      item->spec,
				      sizeof(struct rte_flow_item_vlan),
				      ULP_BUFFER_ALIGN_8_BYTE);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	}

	/* may have two vlan headers */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG +
		       sizeof(struct rte_flow_item_vlan)],
		       item->spec,
		       sizeof(struct rte_flow_item_vlan));
		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	}
	/* Update the vlan count and size of more than one */
	if (vlan_num) {
		vlan_size = vlan_num * sizeof(struct rte_flow_item_vlan);
		vlan_num = tfp_cpu_to_be_32(vlan_num);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM],
		       &vlan_num,
		       sizeof(uint32_t));
		vlan_size = tfp_cpu_to_be_32(vlan_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ],
		       &vlan_size,
		       sizeof(uint32_t));
	}

	/* L3 must be IPv4, IPv6 */
	if (item->type == RTE_FLOW_ITEM_TYPE_IPV4) {
		ipv4_spec = item->spec;
		ip_size = BNXT_ULP_ENCAP_IPV4_SIZE;

		/* copy the ipv4 details */
		if (ulp_buffer_is_empty(&ipv4_spec->hdr.version_ihl,
					BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS)) {
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			ulp_encap_buffer_copy(buff,
					      def_ipv4_hdr,
					      BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS +
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO,
					      ULP_BUFFER_ALIGN_8_BYTE);
		} else {
			/* Total length being ignored in the ip hdr. */
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			tmp_buff = (const uint8_t *)&ipv4_spec->hdr.packet_id;
			ulp_encap_buffer_copy(buff,
					      tmp_buff,
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO,
					      ULP_BUFFER_ALIGN_8_BYTE);
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
			     BNXT_ULP_ENCAP_IPV4_ID_PROTO];
			ulp_encap_buffer_copy(buff,
					      &ipv4_spec->hdr.version_ihl,
					      BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS,
					      ULP_BUFFER_ALIGN_8_BYTE);
		}

		/* Update the dst ip address in ip encap buffer */
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
		    BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS +
		    BNXT_ULP_ENCAP_IPV4_ID_PROTO];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)&ipv4_spec->hdr.dst_addr,
				      sizeof(ipv4_spec->hdr.dst_addr),
				      ULP_BUFFER_ALIGN_8_BYTE);

		/* Update the src ip address */
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC +
			BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC -
			sizeof(ipv4_spec->hdr.src_addr)];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)&ipv4_spec->hdr.src_addr,
				      sizeof(ipv4_spec->hdr.src_addr),
				      ULP_BUFFER_ALIGN_8_BYTE);

		/* Update the ip size details */
		ip_size = tfp_cpu_to_be_32(ip_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
		       &ip_size, sizeof(uint32_t));

		/* update the ip type */
		ip_type = rte_cpu_to_be_32(BNXT_ULP_ETH_IPV4);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
		       &ip_type, sizeof(uint32_t));

		/* update the computed field to notify it is ipv4 header */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG,
				    1);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		ipv6_spec = item->spec;
		ip_size = BNXT_ULP_ENCAP_IPV6_SIZE;

		/* copy the ipv6 details */
		tmp_buff = (const uint8_t *)&ipv6_spec->hdr.vtc_flow;
		if (ulp_buffer_is_empty(tmp_buff,
					BNXT_ULP_ENCAP_IPV6_VTC_FLOW)) {
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			ulp_encap_buffer_copy(buff,
					      def_ipv6_hdr,
					      sizeof(def_ipv6_hdr),
					      ULP_BUFFER_ALIGN_8_BYTE);
		} else {
			/* The payload length being ignored in the ip hdr. */
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			tmp_buff = (const uint8_t *)&ipv6_spec->hdr.proto;
			ulp_encap_buffer_copy(buff,
					      tmp_buff,
					      BNXT_ULP_ENCAP_IPV6_PROTO_TTL,
					      ULP_BUFFER_ALIGN_8_BYTE);
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
				BNXT_ULP_ENCAP_IPV6_PROTO_TTL +
				BNXT_ULP_ENCAP_IPV6_DO];
			tmp_buff = (const uint8_t *)&ipv6_spec->hdr.vtc_flow;
			ulp_encap_buffer_copy(buff,
					      tmp_buff,
					      BNXT_ULP_ENCAP_IPV6_VTC_FLOW,
					      ULP_BUFFER_ALIGN_8_BYTE);
		}
		/* Update the dst ip address in ip encap buffer */
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
			sizeof(def_ipv6_hdr)];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)ipv6_spec->hdr.dst_addr,
				      sizeof(ipv6_spec->hdr.dst_addr),
				      ULP_BUFFER_ALIGN_8_BYTE);

		/* Update the src ip address */
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)ipv6_spec->hdr.src_addr,
				      sizeof(ipv6_spec->hdr.src_addr),
				      ULP_BUFFER_ALIGN_16_BYTE);

		/* Update the ip size details */
		ip_size = tfp_cpu_to_be_32(ip_size);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ],
		       &ip_size, sizeof(uint32_t));

		 /* update the ip type */
		ip_type = rte_cpu_to_be_32(BNXT_ULP_ETH_IPV6);
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE],
		       &ip_type, sizeof(uint32_t));

		/* update the computed field to notify it is ipv6 header */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG,
				    1);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else {
		BNXT_TF_DBG(ERR, "Parse Error: Vxlan Encap expects L3 hdr\n");
		return BNXT_TF_RC_ERROR;
	}

	/* L4 is UDP */
	if (item->type != RTE_FLOW_ITEM_TYPE_UDP) {
		BNXT_TF_DBG(ERR, "vxlan encap does not have udp\n");
		return BNXT_TF_RC_ERROR;
	}
	/* copy the udp details */
	ulp_encap_buffer_copy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP],
			      item->spec, BNXT_ULP_ENCAP_UDP_SIZE,
			      ULP_BUFFER_ALIGN_8_BYTE);

	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* Finally VXLAN */
	if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
		BNXT_TF_DBG(ERR, "vxlan encap does not have vni\n");
		return BNXT_TF_RC_ERROR;
	}
	vxlan_size = sizeof(struct rte_flow_item_vxlan);
	/* copy the vxlan details */
	memcpy(&vxlan_spec, item->spec, vxlan_size);
	vxlan_spec.flags = 0x08;
	buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN];
	if (ip_type == rte_cpu_to_be_32(BNXT_ULP_ETH_IPV4)) {
		ulp_encap_buffer_copy(buff, (const uint8_t *)&vxlan_spec,
				      vxlan_size, ULP_BUFFER_ALIGN_8_BYTE);
	} else {
		ulp_encap_buffer_copy(buff, (const uint8_t *)&vxlan_spec,
				      vxlan_size / 2, ULP_BUFFER_ALIGN_8_BYTE);
		ulp_encap_buffer_copy(buff + (vxlan_size / 2),
				      (const uint8_t *)&vxlan_spec.vni,
				      vxlan_size / 2, ULP_BUFFER_ALIGN_8_BYTE);
	}
	vxlan_size = tfp_cpu_to_be_32(vxlan_size);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ],
	       &vxlan_size, sizeof(uint32_t));

	/* update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(act->bits, BNXT_ULP_ACT_BIT_VXLAN_ENCAP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action vxlan_encap Header */
int32_t
ulp_rte_vxlan_decap_act_handler(const struct rte_flow_action *action_item
				__rte_unused,
				struct ulp_rte_parser_params *params)
{
	/* update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(params->act_bitmap.bits,
		       BNXT_ULP_ACT_BIT_VXLAN_DECAP);
	/* Update computational field with tunnel decap info */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN_DECAP, 1);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_TUN, 1);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action drop Header. */
int32_t
ulp_rte_drop_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params)
{
	/* Update the hdr_bitmap with drop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DROP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action count. */
int32_t
ulp_rte_count_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_count *act_count;
	struct ulp_rte_act_prop *act_prop = &params->act_prop;

	act_count = action_item->conf;
	if (act_count) {
		if (act_count->shared) {
			BNXT_TF_DBG(ERR,
				    "Parse Error:Shared count not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_COUNT],
		       &act_count->id,
		       BNXT_ULP_ACT_PROP_SZ_COUNT);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of action ports. */
static int32_t
ulp_rte_parser_act_port_set(struct ulp_rte_parser_params *param,
			    uint32_t ifindex)
{
	enum bnxt_ulp_direction_type dir;
	uint16_t pid_s;
	uint32_t pid;
	struct ulp_rte_act_prop *act = &param->act_prop;
	enum bnxt_ulp_intf_type port_type;
	uint32_t vnic_type;

	/* Get the direction */
	dir = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir == BNXT_ULP_DIR_EGRESS) {
		/* For egress direction, fill vport */
		if (ulp_port_db_vport_get(param->ulp_ctx, ifindex, &pid_s))
			return BNXT_TF_RC_ERROR;

		pid = pid_s;
		pid = rte_cpu_to_be_32(pid);
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VPORT],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VPORT);
	} else {
		/* For ingress direction, fill vnic */
		port_type = ULP_COMP_FLD_IDX_RD(param,
						BNXT_ULP_CF_IDX_ACT_PORT_TYPE);
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			vnic_type = BNXT_ULP_VF_FUNC_VNIC;
		else
			vnic_type = BNXT_ULP_DRV_FUNC_VNIC;

		if (ulp_port_db_default_vnic_get(param->ulp_ctx, ifindex,
						 vnic_type, &pid_s))
			return BNXT_TF_RC_ERROR;

		pid = pid_s;
		pid = rte_cpu_to_be_32(pid);
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
		       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);
	}

	/* Update the action port set bit */
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 1);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action PF. */
int32_t
ulp_rte_pf_act_handler(const struct rte_flow_action *action_item __rte_unused,
		       struct ulp_rte_parser_params *params)
{
	uint32_t port_id;
	uint32_t ifindex;
	enum bnxt_ulp_intf_type intf_type;

	/* Get the port id of the current device */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);

	/* Get the port db ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx, port_id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "Invalid port id\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Check the port is PF port */
	intf_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (intf_type != BNXT_ULP_INTF_TYPE_PF) {
		BNXT_TF_DBG(ERR, "Port is not a PF port\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Update the action properties */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	return ulp_rte_parser_act_port_set(params, ifindex);
}

/* Function to handle the parsing of RTE Flow action VF. */
int32_t
ulp_rte_vf_act_handler(const struct rte_flow_action *action_item,
		       struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_vf *vf_action;
	enum bnxt_ulp_intf_type intf_type;
	uint32_t ifindex;
	struct bnxt *bp;

	vf_action = action_item->conf;
	if (!vf_action) {
		BNXT_TF_DBG(ERR, "ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	if (vf_action->original) {
		BNXT_TF_DBG(ERR, "ParseErr:VF Original not supported\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	bp = bnxt_get_bp(params->port_id);
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp\n");
		return BNXT_TF_RC_ERROR;
	}

	/* vf_action->id is a logical number which in this case is an
	 * offset from the first VF. So, to get the absolute VF id, the
	 * offset must be added to the absolute first vf id of that port.
	 */
	if (ulp_port_db_dev_func_id_to_ulp_index(params->ulp_ctx,
						 bp->first_vf_id +
						 vf_action->id,
						 &ifindex)) {
		BNXT_TF_DBG(ERR, "VF is not valid interface\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Check the port is VF port */
	intf_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (intf_type != BNXT_ULP_INTF_TYPE_VF &&
	    intf_type != BNXT_ULP_INTF_TYPE_TRUSTED_VF) {
		BNXT_TF_DBG(ERR, "Port is not a VF port\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the action properties */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	return ulp_rte_parser_act_port_set(params, ifindex);
}

/* Function to handle the parsing of RTE Flow action port_id. */
int32_t
ulp_rte_port_id_act_handler(const struct rte_flow_action *act_item,
			    struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_port_id *port_id = act_item->conf;
	uint32_t ifindex;
	enum bnxt_ulp_intf_type intf_type;

	if (!port_id) {
		BNXT_TF_DBG(ERR,
			    "ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}
	if (port_id->original) {
		BNXT_TF_DBG(ERR,
			    "ParseErr:Portid Original not supported\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	/* Get the port db ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(param->ulp_ctx, port_id->id,
					      &ifindex)) {
		BNXT_TF_DBG(ERR, "Invalid port id\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Get the intf type */
	intf_type = ulp_port_db_port_type_get(param->ulp_ctx, ifindex);
	if (!intf_type) {
		BNXT_TF_DBG(ERR, "Invalid port type\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Set the action port */
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	return ulp_rte_parser_act_port_set(param, ifindex);
}

/* Function to handle the parsing of RTE Flow action phy_port. */
int32_t
ulp_rte_phy_port_act_handler(const struct rte_flow_action *action_item,
			     struct ulp_rte_parser_params *prm)
{
	const struct rte_flow_action_phy_port *phy_port;
	uint32_t pid;
	int32_t rc;
	uint16_t pid_s;
	enum bnxt_ulp_direction_type dir;

	phy_port = action_item->conf;
	if (!phy_port) {
		BNXT_TF_DBG(ERR,
			    "ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	if (phy_port->original) {
		BNXT_TF_DBG(ERR,
			    "Parse Err:Port Original not supported\n");
		return BNXT_TF_RC_PARSE_ERR;
	}
	dir = ULP_COMP_FLD_IDX_RD(prm, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir != BNXT_ULP_DIR_EGRESS) {
		BNXT_TF_DBG(ERR,
			    "Parse Err:Phy ports are valid only for egress\n");
		return BNXT_TF_RC_PARSE_ERR;
	}
	/* Get the physical port details from port db */
	rc = ulp_port_db_phy_port_vport_get(prm->ulp_ctx, phy_port->index,
					    &pid_s);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get port details\n");
		return -EINVAL;
	}

	pid = pid_s;
	pid = rte_cpu_to_be_32(pid);
	memcpy(&prm->act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_VPORT],
	       &pid, BNXT_ULP_ACT_PROP_SZ_VPORT);

	/* Update the action port set bit */
	ULP_COMP_FLD_IDX_WR(prm, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 1);
	ULP_COMP_FLD_IDX_WR(prm, BNXT_ULP_CF_IDX_ACT_PORT_TYPE,
			    BNXT_ULP_INTF_TYPE_PHY_PORT);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action pop vlan. */
int32_t
ulp_rte_of_pop_vlan_act_handler(const struct rte_flow_action *a __rte_unused,
				struct ulp_rte_parser_params *params)
{
	/* Update the act_bitmap with pop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_POP_VLAN);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action push vlan. */
int32_t
ulp_rte_of_push_vlan_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_of_push_vlan *push_vlan;
	uint16_t ethertype;
	struct ulp_rte_act_prop *act = &params->act_prop;

	push_vlan = action_item->conf;
	if (push_vlan) {
		ethertype = push_vlan->ethertype;
		if (tfp_cpu_to_be_16(ethertype) != RTE_ETHER_TYPE_VLAN) {
			BNXT_TF_DBG(ERR,
				    "Parse Err: Ethertype not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN],
		       &ethertype, BNXT_ULP_ACT_PROP_SZ_PUSH_VLAN);
		/* Update the hdr_bitmap with push vlan */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_PUSH_VLAN);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: Push vlan arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set vlan id. */
int32_t
ulp_rte_of_set_vlan_vid_act_handler(const struct rte_flow_action *action_item,
				    struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_of_set_vlan_vid *vlan_vid;
	uint32_t vid;
	struct ulp_rte_act_prop *act = &params->act_prop;

	vlan_vid = action_item->conf;
	if (vlan_vid && vlan_vid->vlan_vid) {
		vid = vlan_vid->vlan_vid;
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID],
		       &vid, BNXT_ULP_ACT_PROP_SZ_SET_VLAN_VID);
		/* Update the hdr_bitmap with vlan vid */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_VLAN_VID);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: Vlan vid arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set vlan pcp. */
int32_t
ulp_rte_of_set_vlan_pcp_act_handler(const struct rte_flow_action *action_item,
				    struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_of_set_vlan_pcp *vlan_pcp;
	uint8_t pcp;
	struct ulp_rte_act_prop *act = &params->act_prop;

	vlan_pcp = action_item->conf;
	if (vlan_pcp) {
		pcp = vlan_pcp->vlan_pcp;
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP],
		       &pcp, BNXT_ULP_ACT_PROP_SZ_SET_VLAN_PCP);
		/* Update the hdr_bitmap with vlan vid */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_VLAN_PCP);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: Vlan pcp arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set ipv4 src.*/
int32_t
ulp_rte_set_ipv4_src_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_ipv4 *set_ipv4;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_ipv4 = action_item->conf;
	if (set_ipv4) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC],
		       &set_ipv4->ipv4_addr, BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC);
		/* Update the hdr_bitmap with set ipv4 src */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_IPV4_SRC);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: set ipv4 src arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set ipv4 dst.*/
int32_t
ulp_rte_set_ipv4_dst_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_ipv4 *set_ipv4;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_ipv4 = action_item->conf;
	if (set_ipv4) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST],
		       &set_ipv4->ipv4_addr, BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST);
		/* Update the hdr_bitmap with set ipv4 dst */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_IPV4_DST);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: set ipv4 dst arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set tp src.*/
int32_t
ulp_rte_set_tp_src_act_handler(const struct rte_flow_action *action_item,
			       struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_tp *set_tp;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_tp = action_item->conf;
	if (set_tp) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC],
		       &set_tp->port, BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC);
		/* Update the hdr_bitmap with set tp src */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_TP_SRC);
		return BNXT_TF_RC_SUCCESS;
	}

	BNXT_TF_DBG(ERR, "Parse Error: set tp src arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set tp dst.*/
int32_t
ulp_rte_set_tp_dst_act_handler(const struct rte_flow_action *action_item,
			       struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_tp *set_tp;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_tp = action_item->conf;
	if (set_tp) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_TP_DST],
		       &set_tp->port, BNXT_ULP_ACT_PROP_SZ_SET_TP_DST);
		/* Update the hdr_bitmap with set tp dst */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_TP_DST);
		return BNXT_TF_RC_SUCCESS;
	}

	BNXT_TF_DBG(ERR, "Parse Error: set tp src arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action dec ttl.*/
int32_t
ulp_rte_dec_ttl_act_handler(const struct rte_flow_action *act __rte_unused,
			    struct ulp_rte_parser_params *params)
{
	/* Update the act_bitmap with dec ttl */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DEC_TTL);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action JUMP */
int32_t
ulp_rte_jump_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params)
{
	/* Update the act_bitmap with dec ttl */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_JUMP);
	return BNXT_TF_RC_SUCCESS;
}

int32_t
ulp_rte_sample_act_handler(const struct rte_flow_action *action_item,
			   struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_sample *sample;
	int ret;

	sample = action_item->conf;

	/* if SAMPLE bit is set it means this sample action is nested within the
	 * actions of another sample action; this is not allowed
	 */
	if (ULP_BITMAP_ISSET(params->act_bitmap.bits,
			     BNXT_ULP_ACT_BIT_SAMPLE))
		return BNXT_TF_RC_ERROR;

	/* a sample action is only allowed as a shared action */
	if (!ULP_BITMAP_ISSET(params->act_bitmap.bits,
			      BNXT_ULP_ACT_BIT_SHARED))
		return BNXT_TF_RC_ERROR;

	/* only a ratio of 1 i.e. 100% is supported */
	if (sample->ratio != 1)
		return BNXT_TF_RC_ERROR;

	if (!sample->actions)
		return BNXT_TF_RC_ERROR;

	/* parse the nested actions for a sample action */
	ret = bnxt_ulp_rte_parser_act_parse(sample->actions, params);
	if (ret == BNXT_TF_RC_SUCCESS)
		/* Update the act_bitmap with sample */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SAMPLE);

	return ret;
}
