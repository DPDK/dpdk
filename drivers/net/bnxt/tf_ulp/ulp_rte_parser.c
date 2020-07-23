/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"
#include "ulp_rte_parser.h"
#include "ulp_utils.h"
#include "tfp.h"
#include "ulp_port_db.h"

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
				   uint32_t idx)
{
	struct ulp_rte_hdr_field *field;

	field = &params->hdr_field[idx];
	if (ulp_bitmap_notzero(field->mask, field->size)) {
		ULP_INDEX_BITMAP_SET(params->fld_bitmap.bits, idx);
		/* Not exact match */
		if (!ulp_bitmap_is_ones(field->mask, field->size))
			ULP_BITMAP_SET(params->fld_bitmap.bits,
				       BNXT_ULP_MATCH_TYPE_BITMASK_WM);
	} else {
		ULP_INDEX_BITMAP_RESET(params->fld_bitmap.bits, idx);
	}
}

/* Utility function to copy field spec items */
static struct ulp_rte_hdr_field *
ulp_rte_parser_fld_copy(struct ulp_rte_hdr_field *field,
			const void *buffer,
			uint32_t size)
{
	field->size = size;
	memcpy(field->spec, buffer, field->size);
	field++;
	return field;
}

/* Utility function to copy field masks items */
static void
ulp_rte_prsr_mask_copy(struct ulp_rte_parser_params *params,
		       uint32_t *idx,
		       const void *buffer,
		       uint32_t size)
{
	struct ulp_rte_hdr_field *field = &params->hdr_field[*idx];

	memcpy(field->mask, buffer, size);
	ulp_rte_parser_field_bitmap_update(params, *idx);
	*idx = *idx + 1;
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
	port_id = ULP_COMP_FLD_IDX_RD(params,
				      BNXT_ULP_CF_IDX_INCOMING_IF);
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
	}
}

/*
 * Function to handle the post processing of the parsing details
 */
int32_t
bnxt_ulp_rte_parser_post_process(struct ulp_rte_parser_params *params)
{
	enum bnxt_ulp_direction_type dir;
	enum bnxt_ulp_intf_type match_port_type, act_port_type;
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
			     BNXT_ULP_ACTION_BIT_DEC_TTL)) {
		/*
		 * Check that vxlan proto is included and vxlan decap
		 * action is not set then decrement tunnel ttl.
		 * Similarly add GRE and NVGRE in future.
		 */
		if ((ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
				      BNXT_ULP_HDR_BIT_T_VXLAN) &&
		    !ULP_BITMAP_ISSET(params->act_bitmap.bits,
				      BNXT_ULP_ACTION_BIT_VXLAN_DECAP))) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_T_DEC_TTL, 1);
		} else {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_ACT_DEC_TTL, 1);
		}
	}

	/* Merge the hdr_fp_bit into the proto header bit */
	params->hdr_bitmap.bits |= params->hdr_fp_bit.bits;

	/* Update the computed interface parameters */
	bnxt_ulp_comp_fld_intf_update(params);

	/* TBD: Handle the flow rejection scenarios */
	return 0;
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

/* Function to handle the parsing of RTE Flow item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;

	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	if (eth_spec) {
		size = sizeof(eth_spec->dst.addr_bytes);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						eth_spec->dst.addr_bytes,
						size);
		size = sizeof(eth_spec->src.addr_bytes);
		field = ulp_rte_parser_fld_copy(field,
						eth_spec->src.addr_bytes,
						size);
		field = ulp_rte_parser_fld_copy(field,
						&eth_spec->type,
						sizeof(eth_spec->type));
		eth_type = eth_spec->type;
	}
	if (eth_mask) {
		ulp_rte_prsr_mask_copy(params, &idx, eth_mask->dst.addr_bytes,
				       sizeof(eth_mask->dst.addr_bytes));
		ulp_rte_prsr_mask_copy(params, &idx, eth_mask->src.addr_bytes,
				       sizeof(eth_mask->src.addr_bytes));
		ulp_rte_prsr_mask_copy(params, &idx, &eth_mask->type,
				       sizeof(eth_mask->type));
	}
	/* Add number of vlan header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_ETH_NUM;
	params->vlan_idx = params->field_idx;
	params->field_idx += BNXT_ULP_PROTO_HDR_VLAN_NUM;

	/* Update the protocol hdr bitmap */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH)) {
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
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap	*hdr_bit;
	uint32_t idx = params->vlan_idx;
	uint16_t vlan_tag, priority;
	uint32_t outer_vtag_num;
	uint32_t inner_vtag_num;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	if (vlan_spec) {
		vlan_tag = ntohs(vlan_spec->tci);
		priority = htons(vlan_tag >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag &= ULP_VLAN_TAG_MASK;
		vlan_tag = htons(vlan_tag);

		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&priority,
						sizeof(priority));
		field = ulp_rte_parser_fld_copy(field,
						&vlan_tag,
						sizeof(vlan_tag));
		field = ulp_rte_parser_fld_copy(field,
						&vlan_spec->inner_type,
						sizeof(vlan_spec->inner_type));
		eth_type = vlan_spec->inner_type;
	}

	if (vlan_mask) {
		vlan_tag = ntohs(vlan_mask->tci);
		priority = htons(vlan_tag >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag &= 0xfff;

		/*
		 * the storage for priority and vlan tag is 2 bytes
		 * The mask of priority which is 3 bits if it is all 1's
		 * then make the rest bits 13 bits as 1's
		 * so that it is matched as exact match.
		 */
		if (priority == ULP_VLAN_PRIORITY_MASK)
			priority |= ~ULP_VLAN_PRIORITY_MASK;
		if (vlan_tag == ULP_VLAN_TAG_MASK)
			vlan_tag |= ~ULP_VLAN_TAG_MASK;
		vlan_tag = htons(vlan_tag);

		/*
		 * The priority field is ignored since OVS is setting it as
		 * wild card match and it is not supported. This is a work
		 * around and shall be addressed in the future.
		 */
		idx += 1;

		ulp_rte_prsr_mask_copy(params, &idx, &vlan_tag,
				       sizeof(vlan_tag));
		ulp_rte_prsr_mask_copy(params, &idx, &vlan_mask->inner_type,
				       sizeof(vlan_mask->inner_type));
	}
	/* Set the vlan index to new incremented value */
	params->vlan_idx += BNXT_ULP_PROTO_HDR_S_VLAN_NUM;

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
		BNXT_TF_DBG(ERR, "Error Parsing:Vlan hdr found withtout eth\n");
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
	}
}

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
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

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (ipv4_spec) {
		size = sizeof(ipv4_spec->hdr.version_ihl);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&ipv4_spec->hdr.version_ihl,
						size);
		size = sizeof(ipv4_spec->hdr.type_of_service);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.type_of_service,
						size);
		size = sizeof(ipv4_spec->hdr.total_length);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.total_length,
						size);
		size = sizeof(ipv4_spec->hdr.packet_id);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.packet_id,
						size);
		size = sizeof(ipv4_spec->hdr.fragment_offset);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.fragment_offset,
						size);
		size = sizeof(ipv4_spec->hdr.time_to_live);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.time_to_live,
						size);
		size = sizeof(ipv4_spec->hdr.next_proto_id);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.next_proto_id,
						size);
		proto = ipv4_spec->hdr.next_proto_id;
		size = sizeof(ipv4_spec->hdr.hdr_checksum);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.hdr_checksum,
						size);
		size = sizeof(ipv4_spec->hdr.src_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.src_addr,
						size);
		size = sizeof(ipv4_spec->hdr.dst_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv4_spec->hdr.dst_addr,
						size);
	}
	if (ipv4_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.version_ihl,
				       sizeof(ipv4_mask->hdr.version_ihl));
		/*
		 * The tos field is ignored since OVS is setting it as wild card
		 * match and it is not supported. This is a work around and
		 * shall be addressed in the future.
		 */
		idx += 1;

		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.total_length,
				       sizeof(ipv4_mask->hdr.total_length));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.packet_id,
				       sizeof(ipv4_mask->hdr.packet_id));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.fragment_offset,
				       sizeof(ipv4_mask->hdr.fragment_offset));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.time_to_live,
				       sizeof(ipv4_mask->hdr.time_to_live));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.next_proto_id,
				       sizeof(ipv4_mask->hdr.next_proto_id));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.hdr_checksum,
				       sizeof(ipv4_mask->hdr.hdr_checksum));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.src_addr,
				       sizeof(ipv4_mask->hdr.src_addr));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv4_mask->hdr.dst_addr,
				       sizeof(ipv4_mask->hdr.dst_addr));
	}
	/* Add the number of ipv4 header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_IPV4_NUM;

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
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t vtcf, vtcf_mask;
	uint8_t proto = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv6 into hdr_field using ipv6
	 * header fields
	 */
	if (ipv6_spec) {
		size = sizeof(ipv6_spec->hdr.vtc_flow);

		vtcf = BNXT_ULP_GET_IPV6_VER(ipv6_spec->hdr.vtc_flow);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&vtcf,
						size);

		vtcf = BNXT_ULP_GET_IPV6_TC(ipv6_spec->hdr.vtc_flow);
		field = ulp_rte_parser_fld_copy(field,
						&vtcf,
						size);

		vtcf = BNXT_ULP_GET_IPV6_FLOWLABEL(ipv6_spec->hdr.vtc_flow);
		field = ulp_rte_parser_fld_copy(field,
						&vtcf,
						size);

		size = sizeof(ipv6_spec->hdr.payload_len);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.payload_len,
						size);
		size = sizeof(ipv6_spec->hdr.proto);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.proto,
						size);
		proto = ipv6_spec->hdr.proto;
		size = sizeof(ipv6_spec->hdr.hop_limits);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.hop_limits,
						size);
		size = sizeof(ipv6_spec->hdr.src_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.src_addr,
						size);
		size = sizeof(ipv6_spec->hdr.dst_addr);
		field = ulp_rte_parser_fld_copy(field,
						&ipv6_spec->hdr.dst_addr,
						size);
	}
	if (ipv6_mask) {
		size = sizeof(ipv6_mask->hdr.vtc_flow);

		vtcf_mask = BNXT_ULP_GET_IPV6_VER(ipv6_mask->hdr.vtc_flow);
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vtcf_mask,
				       size);

		vtcf_mask = BNXT_ULP_GET_IPV6_TC(ipv6_mask->hdr.vtc_flow);
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vtcf_mask,
				       size);

		vtcf_mask =
			BNXT_ULP_GET_IPV6_FLOWLABEL(ipv6_mask->hdr.vtc_flow);
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vtcf_mask,
				       size);

		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.payload_len,
				       sizeof(ipv6_mask->hdr.payload_len));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.proto,
				       sizeof(ipv6_mask->hdr.proto));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.hop_limits,
				       sizeof(ipv6_mask->hdr.hop_limits));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.src_addr,
				       sizeof(ipv6_mask->hdr.src_addr));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &ipv6_mask->hdr.dst_addr,
				       sizeof(ipv6_mask->hdr.dst_addr));
	}
	/* add number of ipv6 header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_IPV6_NUM;

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
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = item->mask;
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint16_t dst_port = 0;
	uint32_t cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (udp_spec) {
		size = sizeof(udp_spec->hdr.src_port);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&udp_spec->hdr.src_port,
						size);
		size = sizeof(udp_spec->hdr.dst_port);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dst_port,
						size);
		dst_port = udp_spec->hdr.dst_port;
		size = sizeof(udp_spec->hdr.dgram_len);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dgram_len,
						size);
		size = sizeof(udp_spec->hdr.dgram_cksum);
		field = ulp_rte_parser_fld_copy(field,
						&udp_spec->hdr.dgram_cksum,
						size);
	}
	if (udp_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.src_port,
				       sizeof(udp_mask->hdr.src_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dst_port,
				       sizeof(udp_mask->hdr.dst_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dgram_len,
				       sizeof(udp_mask->hdr.dgram_len));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &udp_mask->hdr.dgram_cksum,
				       sizeof(udp_mask->hdr.dgram_cksum));
	}

	/* Add number of UDP header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_UDP_NUM;

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
		/* Update the field protocol hdr bitmap */
		ulp_rte_l4_proto_type_update(params, dst_port);
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
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;
	uint32_t cnt;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_TF_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for ipv4 into hdr_field using ipv4
	 * header fields
	 */
	if (tcp_spec) {
		size = sizeof(tcp_spec->hdr.src_port);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&tcp_spec->hdr.src_port,
						size);
		size = sizeof(tcp_spec->hdr.dst_port);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.dst_port,
						size);
		size = sizeof(tcp_spec->hdr.sent_seq);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.sent_seq,
						size);
		size = sizeof(tcp_spec->hdr.recv_ack);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.recv_ack,
						size);
		size = sizeof(tcp_spec->hdr.data_off);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.data_off,
						size);
		size = sizeof(tcp_spec->hdr.tcp_flags);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.tcp_flags,
						size);
		size = sizeof(tcp_spec->hdr.rx_win);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.rx_win,
						size);
		size = sizeof(tcp_spec->hdr.cksum);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.cksum,
						size);
		size = sizeof(tcp_spec->hdr.tcp_urp);
		field = ulp_rte_parser_fld_copy(field,
						&tcp_spec->hdr.tcp_urp,
						size);
	} else {
		idx += BNXT_ULP_PROTO_HDR_TCP_NUM;
	}

	if (tcp_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.src_port,
				       sizeof(tcp_mask->hdr.src_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.dst_port,
				       sizeof(tcp_mask->hdr.dst_port));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.sent_seq,
				       sizeof(tcp_mask->hdr.sent_seq));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.recv_ack,
				       sizeof(tcp_mask->hdr.recv_ack));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.data_off,
				       sizeof(tcp_mask->hdr.data_off));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.tcp_flags,
				       sizeof(tcp_mask->hdr.tcp_flags));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.rx_win,
				       sizeof(tcp_mask->hdr.rx_win));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.cksum,
				       sizeof(tcp_mask->hdr.cksum));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &tcp_mask->hdr.tcp_urp,
				       sizeof(tcp_mask->hdr.tcp_urp));
	}
	/* add number of TCP header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_TCP_NUM;

	/* Set the udp header bitmap and computed l4 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_UDP) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
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
	struct ulp_rte_hdr_field *field;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = params->field_idx;
	uint32_t size;

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	if (vxlan_spec) {
		size = sizeof(vxlan_spec->flags);
		field = ulp_rte_parser_fld_copy(&params->hdr_field[idx],
						&vxlan_spec->flags,
						size);
		size = sizeof(vxlan_spec->rsvd0);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->rsvd0,
						size);
		size = sizeof(vxlan_spec->vni);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->vni,
						size);
		size = sizeof(vxlan_spec->rsvd1);
		field = ulp_rte_parser_fld_copy(field,
						&vxlan_spec->rsvd1,
						size);
	}
	if (vxlan_mask) {
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->flags,
				       sizeof(vxlan_mask->flags));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->rsvd0,
				       sizeof(vxlan_mask->rsvd0));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->vni,
				       sizeof(vxlan_mask->vni));
		ulp_rte_prsr_mask_copy(params, &idx,
				       &vxlan_mask->rsvd1,
				       sizeof(vxlan_mask->rsvd1));
	}
	/* Add number of vxlan header elements */
	params->field_idx += BNXT_ULP_PROTO_HDR_VXLAN_NUM;

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
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
		ULP_BITMAP_SET(act->bits, BNXT_ULP_ACTION_BIT_MARK);
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
	const struct rte_flow_action_rss *rss = action_item->conf;

	if (rss) {
		/* Update the hdr_bitmap with vxlan */
		ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACTION_BIT_RSS);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_TF_DBG(ERR, "Parse Error: RSS arg is invalid\n");
	return BNXT_TF_RC_ERROR;
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
	struct ulp_rte_act_bitmap *act = &params->act_bitmap;
	struct ulp_rte_act_prop *ap = &params->act_prop;

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
			      BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC);

	buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC];
	ulp_encap_buffer_copy(buff,
			      eth_spec->src.addr_bytes,
			      BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC);

	/* Goto the next item */
	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* May have vlan header */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG];
		ulp_encap_buffer_copy(buff,
				      item->spec,
				      sizeof(struct rte_flow_item_vlan));

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
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO);
		} else {
			const uint8_t *tmp_buff;

			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP];
			tmp_buff = (const uint8_t *)&ipv4_spec->hdr.packet_id;
			ulp_encap_buffer_copy(buff,
					      tmp_buff,
					      BNXT_ULP_ENCAP_IPV4_ID_PROTO);
			buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
			     BNXT_ULP_ENCAP_IPV4_ID_PROTO];
			ulp_encap_buffer_copy(buff,
					      &ipv4_spec->hdr.version_ihl,
					      BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS);
		}
		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP +
		    BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS +
		    BNXT_ULP_ENCAP_IPV4_ID_PROTO];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)&ipv4_spec->hdr.dst_addr,
				      BNXT_ULP_ENCAP_IPV4_DEST_IP);

		buff = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC];
		ulp_encap_buffer_copy(buff,
				      (const uint8_t *)&ipv4_spec->hdr.src_addr,
				      BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC);

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

		/* copy the ipv4 details */
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP],
		       ipv6_spec, BNXT_ULP_ENCAP_IPV6_SIZE);

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
			      item->spec, BNXT_ULP_ENCAP_UDP_SIZE);

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
	ulp_encap_buffer_copy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN],
			      (const uint8_t *)&vxlan_spec,
			      vxlan_size);
	vxlan_size = tfp_cpu_to_be_32(vxlan_size);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ],
	       &vxlan_size, sizeof(uint32_t));

	/* update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(act->bits, BNXT_ULP_ACTION_BIT_VXLAN_ENCAP);
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
		       BNXT_ULP_ACTION_BIT_VXLAN_DECAP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action drop Header. */
int32_t
ulp_rte_drop_act_handler(const struct rte_flow_action *action_item __rte_unused,
			 struct ulp_rte_parser_params *params)
{
	/* Update the hdr_bitmap with drop */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_DROP);
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
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_COUNT);
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
	uint32_t ifindex;
	enum bnxt_ulp_intf_type intf_type;

	vf_action = action_item->conf;
	if (!vf_action) {
		BNXT_TF_DBG(ERR, "ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	if (vf_action->original) {
		BNXT_TF_DBG(ERR, "ParseErr:VF Original not supported\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	/* Check the port is VF port */
	if (ulp_port_db_dev_func_id_to_ulp_index(params->ulp_ctx, vf_action->id,
						 &ifindex)) {
		BNXT_TF_DBG(ERR, "VF is not valid interface\n");
		return BNXT_TF_RC_ERROR;
	}
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
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_POP_VLAN);
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
			       BNXT_ULP_ACTION_BIT_PUSH_VLAN);
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
			       BNXT_ULP_ACTION_BIT_SET_VLAN_VID);
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
			       BNXT_ULP_ACTION_BIT_SET_VLAN_PCP);
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
			       BNXT_ULP_ACTION_BIT_SET_IPV4_SRC);
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
			       BNXT_ULP_ACTION_BIT_SET_IPV4_DST);
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
			       BNXT_ULP_ACTION_BIT_SET_TP_SRC);
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
			       BNXT_ULP_ACTION_BIT_SET_TP_DST);
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
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACTION_BIT_DEC_TTL);
	return BNXT_TF_RC_SUCCESS;
}
