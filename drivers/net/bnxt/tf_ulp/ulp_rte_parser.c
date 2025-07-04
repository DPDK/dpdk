/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_tf_common.h"
#include "bnxt_tf_pmd_shim.h"
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
#define ULP_UDP_PORT_VXLAN_MASK		0xFFFF
#define ULP_UDP_PORT_VXLAN_GPE		4790
#define ULP_UDP_PORT_VXLAN_GPE_MASK	0xFFFF
#define ULP_UDP_PORT_GENEVE		6081
#define ULP_UDP_PORT_GENEVE_MASK	0xFFFF

/**
 * Geneve header first 16Bit
 * Version (2b), length of the options fields (6b), OAM packet (1b),
 * critical options present (1b), reserved 0 (6b).
 */
#define ULP_GENEVE_OPT_MAX_SIZE 6 /* HW only supports 6 words */
#define ULP_GENEVE_OPTLEN_MASK 0x3F
#define ULP_GENEVE_OPTLEN_SHIFT 8
#define ULP_GENEVE_OPTLEN_VAL(a) \
	    (((a) >> (ULP_GENEVE_OPTLEN_SHIFT)) & (ULP_GENEVE_OPTLEN_MASK))

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

/* Utility function to copy field spec items */
static inline struct ulp_rte_hdr_field *
ulp_rte_parser_fld_copy(struct ulp_rte_hdr_field *field,
			const void *buffer,
			uint32_t size)
{
	field->size = size;
	memcpy(field->spec, buffer, field->size);
	field++;
	return field;
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
static inline void
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
	if (!(prsr_act & ULP_PRSR_ACT_MASK_IGNORE) && mask_buff &&
	    spec_buff && ulp_bitmap_notzero(spec_buff, size)) {
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
static inline int32_t
ulp_rte_prsr_fld_size_validate(struct ulp_rte_parser_params *params,
			       uint32_t *idx,
			       uint32_t size)
{
	if (unlikely(params->field_idx + size >= BNXT_ULP_PROTO_HDR_MAX)) {
		BNXT_DRV_DBG(ERR, "OOB for field processing %u\n", *idx);
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

	/* Parse all the items in the pattern */
	while (item && item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (item->type >= (typeof(item->type))
		    BNXT_RTE_FLOW_ITEM_TYPE_END) {
			if (item->type >=
			    (typeof(item->type))BNXT_RTE_FLOW_ITEM_TYPE_LAST)
				goto hdr_parser_error;
			/* get the header information */
			hdr_info = &ulp_vendor_hdr_info[item->type -
				BNXT_RTE_FLOW_ITEM_TYPE_END];
		} else {
			if (item->type > RTE_FLOW_ITEM_TYPE_ECPRI)
				goto hdr_parser_error;
			hdr_info = &ulp_hdr_info[item->type];
		}
		if (hdr_info->hdr_type == BNXT_ULP_HDR_TYPE_NOT_SUPPORTED) {
			goto hdr_parser_error;
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

hdr_parser_error:
	BNXT_DRV_DBG(ERR, "Truflow parser does not support type %d\n",
		    item->type);
	return BNXT_TF_RC_PARSE_ERR;
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
		if (action_item->type >=
		    (typeof(action_item->type))BNXT_RTE_FLOW_ACTION_TYPE_END) {
			if (action_item->type >=
			    (typeof(action_item->type))BNXT_RTE_FLOW_ACTION_TYPE_LAST)
				goto act_parser_error;
			/* get the header information from bnxt actinfo table */
			hdr_info = &ulp_vendor_act_info[action_item->type -
				BNXT_RTE_FLOW_ACTION_TYPE_END];
		} else {
			if (action_item->type > RTE_FLOW_ACTION_TYPE_INDIRECT)
				goto act_parser_error;
			/* get the header information from the act info table */
			hdr_info = &ulp_act_info[action_item->type];
		}
		if (hdr_info->act_type == BNXT_ULP_ACT_TYPE_NOT_SUPPORTED) {
			goto act_parser_error;
		} else if (hdr_info->act_type == BNXT_ULP_ACT_TYPE_SUPPORTED) {
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

act_parser_error:
	BNXT_DRV_DBG(ERR, "Truflow parser does not support act %u\n",
		    action_item->type);
	return BNXT_TF_RC_ERROR;
}

/*
 * Function to handle the post processing of the computed
 * fields for the interface.
 */
static void
bnxt_ulp_comp_fld_intf_update(struct ulp_rte_parser_params *params)
{
	uint32_t ifindex;
	uint16_t port_id, parif, svif;
	uint32_t mtype;
	enum bnxt_ulp_direction_type dir;

	/* get the direction details */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);

	/* read the port id details */
	port_id = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_INCOMING_IF);
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      port_id,
					      &ifindex)) {
		BNXT_DRV_DBG(ERR, "ParseErr:Portid is not valid\n");
		return;
	}

	if (dir == BNXT_ULP_DIR_INGRESS) {
		/* Set port PARIF */
		if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
					  BNXT_ULP_DRV_FUNC_PARIF, &parif)) {
			BNXT_DRV_DBG(ERR, "ParseErr:ifindex is not valid\n");
			return;
		}
		/* Note:
		 * We save the drv_func_parif into CF_IDX of phy_port_parif,
		 * since that index is currently referenced by ingress templates
		 * for datapath flows. If in the future we change the parser to
		 * save it in the CF_IDX of drv_func_parif we also need to update
		 * the template.
		 * WARNING: Two VFs on same parent PF will not work, as the parif is
		 * based on fw fid of the parent PF.
		 */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
				    parif);
		/* Set port SVIF */
		if (ulp_port_db_svif_get(params->ulp_ctx, ifindex,
					  BNXT_ULP_PHY_PORT_SVIF, &svif)) {
			BNXT_DRV_DBG(ERR, "ParseErr:ifindex is not valid\n");
			return;
		}
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PHY_PORT_SVIF,
				    svif);
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
				BNXT_DRV_DBG(ERR,
					    "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_VF_FUNC_PARIF,
					    parif);

			/* Set VF func SVIF */
			if (ulp_port_db_svif_get(params->ulp_ctx, ifindex,
						 BNXT_ULP_CF_IDX_VF_FUNC_SVIF, &svif)) {
				BNXT_DRV_DBG(ERR, "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_VF_FUNC_SVIF,
					    svif);
		} else {
			/* Set DRV func PARIF */
			if (ulp_port_db_parif_get(params->ulp_ctx, ifindex,
						  BNXT_ULP_DRV_FUNC_PARIF,
						  &parif)) {
				BNXT_DRV_DBG(ERR,
					    "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
					    parif);

			/* Set DRV SVIF */
			if (ulp_port_db_svif_get(params->ulp_ctx, ifindex,
						 BNXT_ULP_DRV_FUNC_SVIF, &svif)) {
				BNXT_DRV_DBG(ERR, "ParseErr:ifindex is not valid\n");
				return;
			}
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DRV_FUNC_SVIF,
					    svif);
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
	} else {
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_ING);
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_ING);
	}

	/* Evaluate the VF to VF flag */
	if (act_port_set && act_port_type == BNXT_ULP_INTF_TYPE_VF_REP &&
	     match_port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
		if (!ULP_BITMAP_ISSET(params->act_bitmap.bits,
				      BNXT_ULP_ACT_BIT_MULTIPLE_PORT)) {
			ULP_BITMAP_SET(params->act_bitmap.bits,
				       BNXT_ULP_ACT_BIT_VF_TO_VF);
		} else {
			if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_MP_A_IS_VFREP) &&
			    ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_MP_B_IS_VFREP))
				ULP_BITMAP_SET(params->act_bitmap.bits,
					       BNXT_ULP_ACT_BIT_VF_TO_VF);
			else
				ULP_BITMAP_RESET(params->act_bitmap.bits,
						 BNXT_ULP_ACT_BIT_VF_TO_VF);
		}
	}

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

	/* set the L2 context usage shall change it later */
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_L2_CNTXT_ID);

	/* Update the computed interface parameters */
	bnxt_ulp_comp_fld_intf_update(params);

	/* TBD: Handle the flow rejection scenarios */
	return 0;
}

/*
 * Function to handle the post processing of the parsing details
 */
void
bnxt_ulp_rte_parser_post_process(struct ulp_rte_parser_params *params)
{
	ulp_post_process_normal_flow(params);
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
		else if (params->dir_attr & BNXT_ULP_FLOW_ATTR_EGRESS)
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
		else if (match_port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
		else
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_INGRESS);
	}
}

/* Function to handle the parsing of RTE Flow item PF Header. */
static int32_t
ulp_rte_parser_svif_set(struct ulp_rte_parser_params *params,
			uint32_t ifindex,
			uint16_t mask,
			enum bnxt_ulp_direction_type item_dir)
{
	uint16_t svif;
	enum bnxt_ulp_direction_type dir;
	struct ulp_rte_hdr_field *hdr_field;
	enum bnxt_ulp_svif_type svif_type;
	enum bnxt_ulp_intf_type port_type;

	if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_SVIF_FLAG) !=
	    BNXT_ULP_INVALID_SVIF_VAL) {
		BNXT_DRV_DBG(ERR,
			    "SVIF already set,multiple source not support'd\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Get port type details */
	port_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (port_type == BNXT_ULP_INTF_TYPE_INVALID) {
		BNXT_DRV_DBG(ERR, "Invalid port type\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the match port type */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE, port_type);

	/* compute the direction */
	bnxt_ulp_rte_parser_direction_compute(params);

	/* Get the computed direction */
	dir = (item_dir != BNXT_ULP_DIR_INVALID) ? item_dir :
		ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);
	if (dir == BNXT_ULP_DIR_INGRESS &&
	    port_type != BNXT_ULP_INTF_TYPE_VF_REP) {
		svif_type = BNXT_ULP_PHY_PORT_SVIF;
	} else {
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP &&
		    item_dir != BNXT_ULP_DIR_EGRESS)
			svif_type = BNXT_ULP_VF_FUNC_SVIF;
		else
			svif_type = BNXT_ULP_DRV_FUNC_SVIF;
	}

	if (ulp_port_db_svif_get(params->ulp_ctx, ifindex,
				 svif_type, &svif)) {
			BNXT_DRV_DBG(ERR, "ParseErr:ifindex is not valid\n");
			return BNXT_TF_RC_ERROR;
	}

	svif = rte_cpu_to_be_16(svif);
	mask = rte_cpu_to_be_16(mask);
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
		BNXT_DRV_DBG(ERR, "ParseErr:Portid is not valid\n");
		return rc;
	}

	/* Update the SVIF details */
	rc = ulp_rte_parser_svif_set(params, ifindex, svif_mask,
				     BNXT_ULP_DIR_INVALID);
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
	action_item.type = RTE_FLOW_ACTION_TYPE_PORT_ID;
	action_item.conf = &port_id;

	/* Update the action port based on incoming port */
	ulp_rte_port_act_handler(&action_item, params);

	/* Reset the action port set bit */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_IS_SET, 0);
	return BNXT_TF_RC_SUCCESS;
}

/* Parse items PORT_ID, PORT_REPRESENTOR and REPRESENTED_PORT. */
int32_t
ulp_rte_port_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	enum bnxt_ulp_direction_type item_dir;
	uint16_t ethdev_id;
	uint16_t mask = 0;
	uint32_t ifindex;
	int32_t rc = BNXT_TF_RC_PARSE_ERR;

	if (!item->spec) {
		BNXT_DRV_DBG(ERR, "ParseErr:Port spec is not valid\n");
		return rc;
	}
	if (!item->mask) {
		BNXT_DRV_DBG(ERR, "ParseErr:Port mask is not valid\n");
		return rc;
	}

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_PORT_ID: {
		const struct rte_flow_item_port_id *port_spec = item->spec;
		const struct rte_flow_item_port_id *port_mask = item->mask;

		item_dir = BNXT_ULP_DIR_INVALID;
		ethdev_id = port_spec->id;
		mask = port_mask->id;

		if (!port_mask->id) {
			ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_SVIF_IGNORE);
		}
		break;
	}
	case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR: {
		const struct rte_flow_item_ethdev *ethdev_spec = item->spec;
		const struct rte_flow_item_ethdev *ethdev_mask = item->mask;

		item_dir = BNXT_ULP_DIR_INGRESS;
		ethdev_id = ethdev_spec->port_id;
		mask = ethdev_mask->port_id;
		break;
	}
	case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT: {
		const struct rte_flow_item_ethdev *ethdev_spec = item->spec;
		const struct rte_flow_item_ethdev *ethdev_mask = item->mask;

		item_dir = BNXT_ULP_DIR_EGRESS;
		ethdev_id = ethdev_spec->port_id;
		mask = ethdev_mask->port_id;
		break;
	}
	default:
		BNXT_DRV_DBG(ERR, "ParseErr:Unexpected item\n");
		return rc;
	}

	/* perform the conversion from dpdk port to bnxt ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
					      ethdev_id,
					      &ifindex)) {
		BNXT_DRV_DBG(ERR, "ParseErr:Portid is not valid\n");
		return rc;
	}
	/* Update the SVIF details */
	return ulp_rte_parser_svif_set(params, ifindex, mask, item_dir);
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l2_proto_type_update(struct ulp_rte_parser_params *param,
			     uint16_t type, uint32_t in_flag,
			     uint32_t has_vlan, uint32_t has_vlan_mask)
{
#define ULP_RTE_ETHER_TYPE_ROE	0xfc3d

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
	} else if (type == tfp_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
		if (in_flag) {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_I_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_L3, 1);
		} else {
			ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_O_IPV6);
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_L3, 1);
		}
	} else if (type == tfp_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
		has_vlan_mask = 1;
		has_vlan = 1;
	} else if (type == tfp_cpu_to_be_16(RTE_ETHER_TYPE_ECPRI)) {
		/* Update the hdr_bitmap with eCPRI */
		ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				BNXT_ULP_HDR_BIT_O_ECPRI);
	} else if (type == tfp_cpu_to_be_16(ULP_RTE_ETHER_TYPE_ROE)) {
		/* Update the hdr_bitmap with RoE */
		ULP_BITMAP_SET(param->hdr_fp_bit.bits,
				BNXT_ULP_HDR_BIT_O_ROE);
	}

	if (has_vlan_mask) {
		if (in_flag) {
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_I_HAS_VTAG,
					    has_vlan);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_I_VLAN_NO_IGNORE,
					    1);
		} else {
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_O_HAS_VTAG,
					    has_vlan);
			ULP_COMP_FLD_IDX_WR(param,
					    BNXT_ULP_CF_IDX_O_VLAN_NO_IGNORE,
					    1);
		}
	}
}

/* Internal Function to identify broadcast or multicast packets */
static int32_t
ulp_rte_parser_is_bcmc_addr(const struct rte_ether_addr *eth_addr)
{
	if (rte_is_multicast_ether_addr(eth_addr) ||
	    rte_is_broadcast_ether_addr(eth_addr)) {
		BNXT_DRV_DBG(DEBUG,
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
	uint32_t idx = 0, dmac_idx = 0;
	uint32_t size;
	uint16_t eth_type = 0;
	uint32_t inner_flag = 0;
	uint32_t has_vlan = 0, has_vlan_mask = 0;

	/* Perform validations */
	if (eth_spec) {
		/* Avoid multicast and broadcast addr */
		if (!ULP_APP_BC_MC_SUPPORT(params->ulp_ctx) &&
		    ulp_rte_parser_is_bcmc_addr(&eth_spec->hdr.dst_addr))
			return BNXT_TF_RC_PARSE_ERR;

		if (!ULP_APP_BC_MC_SUPPORT(params->ulp_ctx) &&
		    ulp_rte_parser_is_bcmc_addr(&eth_spec->hdr.src_addr))
			return BNXT_TF_RC_PARSE_ERR;

		eth_type = eth_spec->hdr.ether_type;
		has_vlan = eth_spec->has_vlan;
	}

	/* If mask is not specified then use the default mask */
	if (eth_spec && !eth_mask)
		eth_mask = &rte_flow_item_eth_mask;

	if (eth_mask) {
		eth_type &= eth_mask->type;
		has_vlan_mask = eth_mask->has_vlan;
	}

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_ETH_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}
	/*
	 * Copy the rte_flow_item for eth into hdr_field using ethernet
	 * header fields
	 */
	dmac_idx = idx;
	size = sizeof(((struct rte_flow_item_eth *)NULL)->hdr.dst_addr.addr_bytes);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, hdr.dst_addr.addr_bytes),
			      ulp_deference_struct(eth_mask, hdr.dst_addr.addr_bytes),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->hdr.src_addr.addr_bytes);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, hdr.src_addr.addr_bytes),
			      ulp_deference_struct(eth_mask, hdr.src_addr.addr_bytes),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_eth *)NULL)->hdr.ether_type);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(eth_spec, hdr.ether_type),
			      ulp_deference_struct(eth_mask, hdr.ether_type),
			      (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MATCH_IGNORE);

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
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DMAC_ID,
				    dmac_idx);
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag,
				     has_vlan, has_vlan_mask);

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
		vlan_tag = ntohs(vlan_spec->hdr.vlan_tci);
		priority = htons(vlan_tag >> ULP_VLAN_PRIORITY_SHIFT);
		vlan_tag &= ULP_VLAN_TAG_MASK;
		vlan_tag = htons(vlan_tag);
		eth_type = vlan_spec->hdr.eth_proto;
	}

	/* assign default vlan mask if spec is valid and mask is not */
	if (vlan_spec && !vlan_mask)
		vlan_mask = &rte_flow_item_vlan_mask;

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

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_S_VLAN_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/*
	 * Copy the rte_flow_item for vlan into hdr_field using Vlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vlan *)NULL)->hdr.vlan_tci);
	/*
	 * The priority field is ignored since OVS is setting it as
	 * wild card match and it is not supported. This is a work
	 * around and shall be addressed in the future.
	 */
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      &priority,
			      (vlan_mask) ? &priority_mask : NULL,
			      ULP_PRSR_ACT_MASK_IGNORE);

	ulp_rte_prsr_fld_mask(params, &idx, size,
			      &vlan_tag,
			      (vlan_mask) ? &vlan_tag_mask : NULL,
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vlan *)NULL)->hdr.eth_proto);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vlan_spec, hdr.eth_proto),
			      ulp_deference_struct(vlan_mask, hdr.eth_proto),
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
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_HAS_VTAG, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OO_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_OO_VLAN_FB_VID, 1);

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
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_OI_VLAN_FB_VID, 1);

	} else if (ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_O_ETH) &&
		   ULP_BITMAP_ISSET(hdr_bit->bits, BNXT_ULP_HDR_BIT_I_ETH) &&
		   !inner_vtag_num) {
		/* update the vlan tag num */
		inner_vtag_num++;
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_VTAG_NUM,
				    inner_vtag_num);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_HAS_VTAG, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_ONE_VTAG, 1);
		ULP_BITMAP_SET(params->hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_IO_VLAN);
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_IO_VLAN_FB_VID, 1);
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
		if (vlan_mask && vlan_tag_mask)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_II_VLAN_FB_VID, 1);
		inner_flag = 1;
	} else {
		BNXT_DRV_DBG(ERR, "Error Parsing:Vlan hdr found without eth\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Update the field protocol hdr bitmap */
	ulp_rte_l2_proto_type_update(params, eth_type, inner_flag, 1, 1);
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
		if (ULP_BITMAP_ISSET(param->cf_bitmap,
				     BNXT_ULP_CF_BIT_IS_TUNNEL))
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_I_ICMP);
		else
			ULP_BITMAP_SET(param->hdr_bitmap.bits,
				       BNXT_ULP_HDR_BIT_O_ICMP);
	}

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

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0, dip_idx = 0;
	uint32_t size;
	uint8_t proto = 0;
	uint8_t ttl = 0;
	uint8_t proto_mask = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		BNXT_DRV_DBG(ERR, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_IPV4_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* If mask is not specified then use the default mask */
	if (ipv4_spec && !ipv4_mask)
		ipv4_mask = &rte_flow_item_ipv4_mask;

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
	 * match and it is not supported. An application can enable tos support.
	 */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.type_of_service);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec,
						   hdr.type_of_service),
			      ulp_deference_struct(ipv4_mask,
						   hdr.type_of_service),
			      (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MASK_IGNORE);

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
			      ULP_PRSR_ACT_MASK_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.time_to_live);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.time_to_live),
			      ulp_deference_struct(ipv4_mask, hdr.time_to_live),
			      ULP_PRSR_ACT_DEFAULT);
	if (ipv4_spec)
		ttl = ipv4_spec->hdr.time_to_live;
	if (!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_TTL, ttl);

	/* Ignore proto for matching templates */
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.next_proto_id);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec,
						   hdr.next_proto_id),
			      ulp_deference_struct(ipv4_mask,
						   hdr.next_proto_id),
			      (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MATCH_IGNORE);

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

	dip_idx = idx;
	size = sizeof(((struct rte_flow_item_ipv4 *)NULL)->hdr.dst_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv4_spec, hdr.dst_addr),
			      ulp_deference_struct(ipv4_mask, hdr.dst_addr),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv4 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
		/* Update the tunnel offload dest ip offset */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID,
				    dip_idx);
	}

	/* Some of the PMD applications may set the protocol field
	 * in the IPv4 spec but don't set the mask. So, consider
	 * the mask in the proto value calculation.
	 */
	if (ipv4_mask) {
		proto &= ipv4_mask->hdr.next_proto_id;
		proto_mask = ipv4_mask->hdr.next_proto_id;
	}

	/* Update the field protocol hdr bitmap */
	if (proto_mask)
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
	uint32_t idx = 0, dip_idx = 0;
	uint32_t size, vtc_flow;
	uint32_t ver_spec = 0, ver_mask = 0;
	uint32_t tc_spec = 0, tc_mask = 0;
	uint32_t lab_spec = 0, lab_mask = 0;
	uint8_t proto = 0;
	uint8_t proto_mask = 0;
	uint8_t ttl = 0;
	uint32_t inner_flag = 0;
	uint32_t cnt;

	/* validate there are no 3rd L3 header */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L3_HDR_CNT);
	if (cnt == 2) {
		BNXT_DRV_DBG(ERR, "Parse Err:Third L3 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_IPV6_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	/* If mask is not specified then use the default mask */
	if (ipv6_spec && !ipv6_mask)
		ipv6_mask = &rte_flow_item_ipv6_mask;

	/*
	 * Copy the rte_flow_item for ipv6 into hdr_field using ipv6
	 * header fields
	 */
	if (ipv6_spec) {
		vtc_flow = ntohl(ipv6_spec->hdr.vtc_flow);
		ver_spec = htonl(BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_spec = htonl(BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_spec = htonl(BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));
		proto = ipv6_spec->hdr.proto;
	}

	if (ipv6_mask) {
		vtc_flow = ntohl(ipv6_mask->hdr.vtc_flow);
		ver_mask = htonl(BNXT_ULP_GET_IPV6_VER(vtc_flow));
		tc_mask = htonl(BNXT_ULP_GET_IPV6_TC(vtc_flow));
		lab_mask = htonl(BNXT_ULP_GET_IPV6_FLOWLABEL(vtc_flow));

		/* Some of the PMD applications may set the protocol field
		 * in the IPv6 spec but don't set the mask. So, consider
		 * the mask in proto value calculation.
		 */
		proto &= ipv6_mask->hdr.proto;
		proto_mask = ipv6_mask->hdr.proto;
	}

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.vtc_flow);
	ulp_rte_prsr_fld_mask(params, &idx, size, &ver_spec, &ver_mask,
			      ULP_PRSR_ACT_DEFAULT);
	/*
	 * The TC and flow label field are ignored since OVS is
	 * setting it for match and it is not supported.
	 * This is a work around and
	 * shall be addressed in the future.
	 */
	ulp_rte_prsr_fld_mask(params, &idx, size, &tc_spec, &tc_mask,
			      (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MASK_IGNORE);
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
			      (ULP_APP_TOS_PROTO_SUPPORT(params->ulp_ctx)) ?
			      ULP_PRSR_ACT_DEFAULT : ULP_PRSR_ACT_MATCH_IGNORE);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.hop_limits);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.hop_limits),
			      ulp_deference_struct(ipv6_mask, hdr.hop_limits),
			      ULP_PRSR_ACT_DEFAULT);
	if (ipv6_spec)
		ttl = ipv6_spec->hdr.hop_limits;
	if (!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_TTL, ttl);

	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.src_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.src_addr),
			      ulp_deference_struct(ipv6_mask, hdr.src_addr),
			      ULP_PRSR_ACT_DEFAULT);

	dip_idx =  idx;
	size = sizeof(((struct rte_flow_item_ipv6 *)NULL)->hdr.dst_addr);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(ipv6_spec, hdr.dst_addr),
			      ulp_deference_struct(ipv6_mask, hdr.dst_addr),
			      ULP_PRSR_ACT_DEFAULT);

	/* Set the ipv6 header bitmap and computed l3 header bitmaps */
	if (ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV4) ||
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6) ||
	    ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL)) {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3, 1);
		inner_flag = 1;
	} else {
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_IPV6);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3, 1);
		/* Update the tunnel offload dest ip offset */
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUN_OFF_DIP_ID,
				    dip_idx);
	}

	/* Update the field protocol hdr bitmap */
	if (proto_mask)
		ulp_rte_l3_proto_type_update(params, proto, inner_flag);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L3_HDR_CNT, ++cnt);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the update of proto header based on field values */
static void
ulp_rte_l4_proto_type_update(struct ulp_rte_parser_params *params,
			     uint16_t src_port, uint16_t src_mask,
			     uint16_t dst_port, uint16_t dst_mask,
			     enum bnxt_ulp_hdr_bit hdr_bit)
{
	uint16_t stat_port = 0;
	struct bnxt *bp;

	switch (hdr_bit) {
	case BNXT_ULP_HDR_BIT_I_UDP:
	case BNXT_ULP_HDR_BIT_I_TCP:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT,
				    (uint64_t)rte_be_to_cpu_16(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT,
				    (uint64_t)rte_be_to_cpu_16(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_SRC_PORT_MASK,
				    (uint64_t)rte_be_to_cpu_16(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_DST_PORT_MASK,
				    (uint64_t)rte_be_to_cpu_16(dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_FB_SRC_PORT,
				    !!(src_port & src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L4_FB_DST_PORT,
				    !!(dst_port & dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_I_L3_PROTO_ID,
				    (hdr_bit == BNXT_ULP_HDR_BIT_I_UDP) ?
				    IPPROTO_UDP : IPPROTO_TCP);
		break;
	case BNXT_ULP_HDR_BIT_O_UDP:
	case BNXT_ULP_HDR_BIT_O_TCP:
		ULP_BITMAP_SET(params->hdr_bitmap.bits, hdr_bit);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4, 1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT,
				    (uint64_t)rte_be_to_cpu_16(src_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    (uint64_t)rte_be_to_cpu_16(dst_port));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_SRC_PORT_MASK,
				    (uint64_t)rte_be_to_cpu_16(src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    (uint64_t)rte_be_to_cpu_16(dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_FB_PROTO_ID,
				    1);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_FB_SRC_PORT,
				    !!(src_port & src_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_FB_DST_PORT,
				    !!(dst_port & dst_mask));
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L3_PROTO_ID,
				    (hdr_bit == BNXT_ULP_HDR_BIT_O_UDP) ?
				    IPPROTO_UDP : IPPROTO_TCP);
		break;
	default:
		break;
	}

	/* If it is not udp port then there is no need to set tunnel bits */
	if (hdr_bit != BNXT_ULP_HDR_BIT_O_UDP)
		return;

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_PORT,
			    tfp_be_to_cpu_16(dst_port));

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(params->ulp_ctx)) {
		stat_port = bnxt_ulp_cntxt_vxlan_ip_port_get(params->ulp_ctx);
		if (!stat_port)
			stat_port =
			bnxt_ulp_cntxt_vxlan_port_get(params->ulp_ctx);

		/* if udp and equal to static vxlan port then set tunnel bits*/
		if (stat_port && dst_port == tfp_cpu_to_be_16(stat_port)) {
			bp = bnxt_pmd_get_bp(params->port_id);
			if (bp == NULL) {
				BNXT_DRV_DBG(ERR, "Invalid bp\n");
				return;
			}
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
			if (bp->vxlan_ip_upar_in_use &
			    HWRM_TUNNEL_DST_PORT_QUERY_OUTPUT_UPAR_IN_USE_UPAR0) {
				ULP_COMP_FLD_IDX_WR(params,
						    BNXT_ULP_CF_IDX_VXLAN_IP_UPAR_ID,
						    ULP_WP_SYM_TUN_HDR_TYPE_UPAR1);
			}
		}
	} else {
		/* if dynamic Vxlan is enabled then skip dport checks */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx))
			return;

		/* Vxlan and GPE port check */
		if (dst_port == tfp_cpu_to_be_16(ULP_UDP_PORT_VXLAN_GPE)) {
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN_GPE);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
		} else if (dst_port == tfp_cpu_to_be_16(ULP_UDP_PORT_VXLAN)) {
			ULP_BITMAP_SET(params->hdr_fp_bit.bits,
				       BNXT_ULP_HDR_BIT_T_VXLAN);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_IS_TUNNEL);
		}
	}
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
	uint16_t dport_mask = 0, sport_mask = 0;
	uint32_t cnt;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_UDP;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_DRV_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (udp_spec) {
		sport = udp_spec->hdr.src_port;
		dport = udp_spec->hdr.dst_port;
	}
	if (udp_spec && !udp_mask)
		udp_mask = &rte_flow_item_udp_mask;

	if (udp_mask) {
		sport_mask = udp_mask->hdr.src_port;
		dport_mask = udp_mask->hdr.dst_port;
	}

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_UDP_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
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
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		out_l4 = BNXT_ULP_HDR_BIT_I_UDP;

	ulp_rte_l4_proto_type_update(params, sport, sport_mask, dport,
				     dport_mask, out_l4);
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
	uint16_t dport_mask = 0, sport_mask = 0;
	uint32_t size;
	uint32_t cnt;
	enum bnxt_ulp_hdr_bit out_l4 = BNXT_ULP_HDR_BIT_O_TCP;

	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt == 2) {
		BNXT_DRV_DBG(ERR, "Parse Err:Third L4 header not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (tcp_spec) {
		sport = tcp_spec->hdr.src_port;
		dport = tcp_spec->hdr.dst_port;
	}

	if (tcp_spec && !tcp_mask)
		tcp_mask = &rte_flow_item_tcp_mask;

	if (tcp_mask) {
		sport_mask = tcp_mask->hdr.src_port;
		dport_mask = tcp_mask->hdr.dst_port;
	}

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_TCP_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
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
	    ULP_BITMAP_ISSET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	    ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		out_l4 = BNXT_ULP_HDR_BIT_I_TCP;

	ulp_rte_l4_proto_type_update(params, sport, sport_mask, dport,
				     dport_mask, out_l4);
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
	struct bnxt_ulp_context *ulp_ctx = params->ulp_ctx;
	uint32_t idx = 0;
	uint16_t dport, stat_port;
	uint32_t size;

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_VXLAN_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (vxlan_spec && !vxlan_mask)
		vxlan_mask = &rte_flow_item_vxlan_mask;

	/* Update if the outer headers have any partial masks */
	if (!ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_WC_MATCH))
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_OUTER_EM_ONLY, 1);

	/*
	 * Copy the rte_flow_item for vxlan into hdr_field using vxlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->hdr.flags);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, hdr.flags),
			      ulp_deference_struct(vxlan_mask, hdr.flags),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->hdr.rsvd0);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, hdr.rsvd0),
			      ulp_deference_struct(vxlan_mask, hdr.rsvd0),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->hdr.vni);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, hdr.vni),
			      ulp_deference_struct(vxlan_mask, hdr.vni),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan *)NULL)->hdr.rsvd1);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_spec, hdr.rsvd1),
			      ulp_deference_struct(vxlan_mask, hdr.rsvd1),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with vxlan */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	/* if l4 protocol header updated it then reset it */
	ULP_BITMAP_RESET(params->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_T_VXLAN_GPE);

	dport = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT);
	if (!dport) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    ULP_UDP_PORT_VXLAN);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    ULP_UDP_PORT_VXLAN_MASK);
	}

	/* vxlan static customized port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(ulp_ctx)) {
		stat_port = bnxt_ulp_cntxt_vxlan_ip_port_get(ulp_ctx);
		if (!stat_port)
			stat_port = bnxt_ulp_cntxt_vxlan_port_get(ulp_ctx);

		/* validate that static ports match if not reject */
		if (dport != 0 && dport != tfp_cpu_to_be_16(stat_port)) {
			BNXT_DRV_DBG(ERR, "ParseErr:vxlan port is not valid\n");
			return BNXT_TF_RC_PARSE_ERR;
		} else if (dport == 0) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    tfp_cpu_to_be_16(stat_port));
		}
	} else {
		/* dynamic vxlan support */
		if (ULP_APP_DYNAMIC_VXLAN_PORT_EN(params->ulp_ctx)) {
			if (dport == 0) {
				BNXT_DRV_DBG(ERR,
					     "ParseErr:vxlan port is null\n");
				return BNXT_TF_RC_PARSE_ERR;
			}
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		} else if (dport != 0 && dport != ULP_UDP_PORT_VXLAN) {
			/* set the dynamic vxlan port check */
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_VXLAN_PORT);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
		} else {
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    ULP_UDP_PORT_VXLAN);
		}
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item Vxlan GPE Header. */
int32_t
ulp_rte_vxlan_gpe_hdr_handler(const struct rte_flow_item *item,
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_vxlan_gpe *vxlan_gpe_spec = item->spec;
	const struct rte_flow_item_vxlan_gpe *vxlan_gpe_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint16_t dport;
	uint32_t size;

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_VXLAN_GPE_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (vxlan_gpe_spec && !vxlan_gpe_mask)
		vxlan_gpe_mask = &rte_flow_item_vxlan_gpe_mask;
	/*
	 * Copy the rte_flow_item for vxlan gpe into hdr_field using vxlan
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_vxlan_gpe *)NULL)->flags);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_gpe_spec, flags),
			      ulp_deference_struct(vxlan_gpe_mask, flags),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan_gpe *)NULL)->rsvd0);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_gpe_spec, rsvd0),
			      ulp_deference_struct(vxlan_gpe_mask, rsvd0),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan_gpe *)NULL)->protocol);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_gpe_spec, protocol),
			      ulp_deference_struct(vxlan_gpe_mask, protocol),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan_gpe *)NULL)->vni);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_gpe_spec, vni),
			      ulp_deference_struct(vxlan_gpe_mask, vni),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_vxlan_gpe *)NULL)->rsvd1);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(vxlan_gpe_spec, rsvd1),
			      ulp_deference_struct(vxlan_gpe_mask, rsvd1),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with vxlan gpe*/
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_VXLAN_GPE);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	/* if l4 protocol header updated it then reset it */
	ULP_BITMAP_RESET(params->hdr_fp_bit.bits, BNXT_ULP_HDR_BIT_T_VXLAN);

	dport = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT);
	if (!dport) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT,
				    ULP_UDP_PORT_VXLAN_GPE);
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
				    ULP_UDP_PORT_VXLAN_GPE_MASK);
	}
	/* TBD: currently dynamic or static gpe port config is not supported */
	/* Update the tunnel port */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);

	/* Verify the vxlan gpe port */
	if (dport != 0 && dport != ULP_UDP_PORT_VXLAN_GPE) {
		BNXT_DRV_DBG(ERR, "ParseErr:vxlan gpe port is not valid\n");
		return BNXT_TF_RC_PARSE_ERR;
	}
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item GENEVE Header. */
int32_t
ulp_rte_geneve_hdr_handler(const struct rte_flow_item *item,
			      struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_geneve *geneve_spec = item->spec;
	const struct rte_flow_item_geneve *geneve_mask = item->mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0;
	uint16_t dport;
	uint32_t size;

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_GENEVE_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (geneve_spec && !geneve_mask)
		geneve_mask = &rte_flow_item_geneve_mask;

	/*
	 * Copy the rte_flow_item for geneve into hdr_field using geneve
	 * header fields
	 */
	size = sizeof(((struct rte_flow_item_geneve *)NULL)->ver_opt_len_o_c_rsvd0);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(geneve_spec, ver_opt_len_o_c_rsvd0),
			      ulp_deference_struct(geneve_mask, ver_opt_len_o_c_rsvd0),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_geneve *)NULL)->protocol);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(geneve_spec, protocol),
			      ulp_deference_struct(geneve_mask, protocol),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_geneve *)NULL)->vni);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(geneve_spec, vni),
			      ulp_deference_struct(geneve_mask, vni),
			      ULP_PRSR_ACT_DEFAULT);

	size = sizeof(((struct rte_flow_item_geneve *)NULL)->rsvd1);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(geneve_spec, rsvd1),
			      ulp_deference_struct(geneve_mask, rsvd1),
			      ULP_PRSR_ACT_DEFAULT);

	/* Update the hdr_bitmap with geneve */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_T_GENEVE);
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);

	/* update the tunnel port */
	dport = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L4_DST_PORT);
	if  (ULP_APP_DYNAMIC_GENEVE_PORT_EN(params->ulp_ctx)) {
		if (dport == 0) {
			BNXT_DRV_DBG(ERR, "ParseErr:geneve port is null\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		/* set the dynamic geneve port check */
		ULP_BITMAP_SET(params->cf_bitmap,
			       BNXT_ULP_CF_BIT_DYNAMIC_GENEVE_PORT);
		ULP_COMP_FLD_IDX_WR(params,
				    BNXT_ULP_CF_IDX_TUNNEL_PORT, dport);
	} else {
		if (dport == 0) {
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_DST_PORT,
					    ULP_UDP_PORT_GENEVE);
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_O_L4_DST_PORT_MASK,
					    ULP_UDP_PORT_GENEVE_MASK);
		} else if (dport != 0 && dport != ULP_UDP_PORT_GENEVE) {
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    dport);
			ULP_BITMAP_SET(params->cf_bitmap,
				       BNXT_ULP_CF_BIT_DYNAMIC_GENEVE_PORT);
		}
	}
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

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_GRE_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (gre_spec && !gre_mask)
		gre_mask = &rte_flow_item_gre_mask;

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
	ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL);
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

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_ICMP_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (icmp_spec && !icmp_mask)
		icmp_mask = &rte_flow_item_icmp_mask;

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
	if (ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
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

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_ICMP_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (icmp_spec && !icmp_mask)
		icmp_mask = &rte_flow_item_icmp6_mask;

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
		BNXT_DRV_DBG(ERR, "Error: incorrect icmp version\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the hdr_bitmap with ICMP */
	if (ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_I_ICMP);
	else
		ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ICMP);
	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item ECPRI Header. */
int32_t
ulp_rte_ecpri_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_item_ecpri *ecpri_spec = item->spec;
	const struct rte_flow_item_ecpri *ecpri_mask = item->mask;
	struct rte_flow_item_ecpri l_ecpri_spec, l_ecpri_mask;
	struct rte_flow_item_ecpri *p_ecpri_spec = &l_ecpri_spec;
	struct rte_flow_item_ecpri *p_ecpri_mask = &l_ecpri_mask;
	struct ulp_rte_hdr_bitmap *hdr_bitmap = &params->hdr_bitmap;
	uint32_t idx = 0, cnt;
	uint32_t size;

	if (unlikely(ulp_rte_prsr_fld_size_validate(params, &idx,
						    BNXT_ULP_PROTO_HDR_ECPRI_NUM))) {
		BNXT_DRV_DBG(ERR, "Error parsing protocol header\n");
		return BNXT_TF_RC_ERROR;
	}

	if (ecpri_spec && !ecpri_mask)
		ecpri_mask = &rte_flow_item_ecpri_mask;

	/* Figure out if eCPRI is within L4(UDP), unsupported, for now */
	cnt = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_L4_HDR_CNT);
	if (cnt >= 1) {
		BNXT_DRV_DBG(ERR, "Parse Err: L4 header stack >= 2 not supported\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ecpri_spec || !ecpri_mask)
		goto parser_set_ecpri_hdr_bit;

	memcpy(p_ecpri_spec, ecpri_spec, sizeof(*ecpri_spec));
	memcpy(p_ecpri_mask, ecpri_mask, sizeof(*ecpri_mask));

	p_ecpri_spec->hdr.common.u32 = rte_be_to_cpu_32(p_ecpri_spec->hdr.common.u32);
	p_ecpri_mask->hdr.common.u32 = rte_be_to_cpu_32(p_ecpri_mask->hdr.common.u32);

	/*
	 * Init eCPRI spec+mask to correct defaults, also clear masks of fields
	 * we ignore in the TCAM.
	 */

	l_ecpri_spec.hdr.common.size = 0;
	l_ecpri_spec.hdr.common.c = 0;
	l_ecpri_spec.hdr.common.res = 0;
	l_ecpri_spec.hdr.common.revision = 1;
	l_ecpri_mask.hdr.common.size = 0;
	l_ecpri_mask.hdr.common.c = 1;
	l_ecpri_mask.hdr.common.res = 0;
	l_ecpri_mask.hdr.common.revision = 0xf;

	switch (p_ecpri_spec->hdr.common.type) {
	case RTE_ECPRI_MSG_TYPE_IQ_DATA:
		l_ecpri_mask.hdr.type0.seq_id = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_BIT_SEQ:
		l_ecpri_mask.hdr.type1.seq_id = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_RTC_CTRL:
		l_ecpri_mask.hdr.type2.seq_id = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_GEN_DATA:
		l_ecpri_mask.hdr.type3.seq_id = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_RM_ACC:
		l_ecpri_mask.hdr.type4.rr = 0;
		l_ecpri_mask.hdr.type4.rw = 0;
		l_ecpri_mask.hdr.type4.rma_id = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_DLY_MSR:
		l_ecpri_spec.hdr.type5.act_type = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_RMT_RST:
		l_ecpri_spec.hdr.type6.rst_op = 0;
		break;

	case RTE_ECPRI_MSG_TYPE_EVT_IND:
		l_ecpri_spec.hdr.type7.evt_type = 0;
		l_ecpri_spec.hdr.type7.seq = 0;
		l_ecpri_spec.hdr.type7.number = 0;
		break;

	default:
		break;
	}

	p_ecpri_spec->hdr.common.u32 = rte_cpu_to_be_32(p_ecpri_spec->hdr.common.u32);
	p_ecpri_mask->hdr.common.u32 = rte_cpu_to_be_32(p_ecpri_mask->hdr.common.u32);

	/* Type */
	size = sizeof(((struct rte_flow_item_ecpri *)NULL)->hdr.common.u32);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(p_ecpri_spec, hdr.common.u32),
			      ulp_deference_struct(p_ecpri_mask, hdr.common.u32),
			      ULP_PRSR_ACT_DEFAULT);

	/* PC/RTC/MSR_ID */
	size = sizeof(((struct rte_flow_item_ecpri *)NULL)->hdr.dummy[0]);
	ulp_rte_prsr_fld_mask(params, &idx, size,
			      ulp_deference_struct(p_ecpri_spec, hdr.dummy),
			      ulp_deference_struct(p_ecpri_mask, hdr.dummy),
			      ULP_PRSR_ACT_DEFAULT);

parser_set_ecpri_hdr_bit:
	/* Update the hdr_bitmap with eCPRI */
	ULP_BITMAP_SET(hdr_bitmap->bits, BNXT_ULP_HDR_BIT_O_ECPRI);
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
	BNXT_DRV_DBG(ERR, "Parse Error: Mark arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action RSS Header. */
int32_t
ulp_rte_rss_act_handler(const struct rte_flow_action *action_item,
			struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_rss *rss;
	struct ulp_rte_act_prop *ap = &param->act_prop;
	uint64_t queue_list[BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE / sizeof(uint64_t)];
	uint32_t idx = 0, id;

	if (action_item == NULL || action_item->conf == NULL) {
		BNXT_DRV_DBG(ERR, "Parse Err: invalid rss configuration\n");
		return BNXT_TF_RC_ERROR;
	}

	rss = action_item->conf;
	/* Copy the rss into the specific action properties */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_FUNC], &rss->func,
	       BNXT_ULP_ACT_PROP_SZ_RSS_FUNC);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_TYPES], &rss->types,
	       BNXT_ULP_ACT_PROP_SZ_RSS_TYPES);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_LEVEL], &rss->level,
	       BNXT_ULP_ACT_PROP_SZ_RSS_LEVEL);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY_LEN],
	       &rss->key_len, BNXT_ULP_ACT_PROP_SZ_RSS_KEY_LEN);

	if (rss->key_len != 0 && rss->key_len != BNXT_ULP_ACT_PROP_SZ_RSS_KEY) {
		BNXT_DRV_DBG(ERR, "Parse Err: RSS key length must be 40 bytes\n");
		return BNXT_TF_RC_ERROR;
	}

	/* User may specify only key length. In that case, rss->key will be NULL.
	 * So, reject the flow if key_length is valid but rss->key is NULL.
	 * Also, copy the RSS hash key only when rss->key is valid.
	 */
	if (rss->key_len != 0 && rss->key == NULL) {
		BNXT_DRV_DBG(ERR,
			    "Parse Err: A valid RSS key must be provided with a valid key len.\n");
		return BNXT_TF_RC_ERROR;
	}
	if (rss->key)
		memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY], rss->key, rss->key_len);

	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE_NUM],
	       &rss->queue_num, BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE_NUM);

	if (rss->queue_num >= ULP_BYTE_2_BITS(BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE)) {
		BNXT_DRV_DBG(ERR, "Parse Err: RSS queue num too big\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Queues converted into a bitmap format */
	memset(queue_list, 0, sizeof(queue_list));
	for (idx = 0; idx < rss->queue_num; idx++) {
		id = rss->queue[idx];
		if (id >= ULP_BYTE_2_BITS(BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE)) {
			BNXT_DRV_DBG(ERR, "Parse Err: RSS queue id too big\n");
			return BNXT_TF_RC_ERROR;
		}
		if ((queue_list[id / ULP_INDEX_BITMAP_SIZE] >>
		    ((ULP_INDEX_BITMAP_SIZE - 1) -
		     (id % ULP_INDEX_BITMAP_SIZE)) & 1)) {
			BNXT_DRV_DBG(ERR, "Parse Err: duplicate queue ids\n");
			return BNXT_TF_RC_ERROR;
		}
		queue_list[id / ULP_INDEX_BITMAP_SIZE] |= (1UL <<
		((ULP_INDEX_BITMAP_SIZE - 1) - (id % ULP_INDEX_BITMAP_SIZE)));
	}
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE],
	       (uint8_t *)queue_list, BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE);

	/* set the RSS action header bit */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACT_BIT_RSS);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow item eth Header. */
static void
ulp_rte_enc_eth_hdr_handler(struct ulp_rte_parser_params *params,
			    const struct rte_flow_item_eth *eth_spec)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_ETH_DMAC];
	size = sizeof(eth_spec->hdr.dst_addr.addr_bytes);
	field = ulp_rte_parser_fld_copy(field, eth_spec->hdr.dst_addr.addr_bytes, size);

	size = sizeof(eth_spec->hdr.src_addr.addr_bytes);
	field = ulp_rte_parser_fld_copy(field, eth_spec->hdr.src_addr.addr_bytes, size);

	size = sizeof(eth_spec->hdr.ether_type);
	field = ulp_rte_parser_fld_copy(field, &eth_spec->hdr.ether_type, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_ETH);
}

/* Function to handle the parsing of RTE Flow item vlan Header. */
static void
ulp_rte_enc_vlan_hdr_handler(struct ulp_rte_parser_params *params,
			     const struct rte_flow_item_vlan *vlan_spec,
			     uint32_t inner)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;

	if (!inner) {
		field = &params->enc_field[BNXT_ULP_ENC_FIELD_O_VLAN_TCI];
		ULP_BITMAP_SET(params->enc_hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OO_VLAN);
	} else {
		field = &params->enc_field[BNXT_ULP_ENC_FIELD_I_VLAN_TCI];
		ULP_BITMAP_SET(params->enc_hdr_bitmap.bits,
			       BNXT_ULP_HDR_BIT_OI_VLAN);
	}

	size = sizeof(vlan_spec->hdr.vlan_tci);
	field = ulp_rte_parser_fld_copy(field, &vlan_spec->hdr.vlan_tci, size);

	size = sizeof(vlan_spec->hdr.eth_proto);
	field = ulp_rte_parser_fld_copy(field, &vlan_spec->hdr.eth_proto, size);
}

/* Function to handle the parsing of RTE Flow item ipv4 Header. */
static void
ulp_rte_enc_ipv4_hdr_handler(struct ulp_rte_parser_params *params,
			     const struct rte_flow_item_ipv4 *ip)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;
	uint8_t val8;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV4_IHL];
	size = sizeof(ip->hdr.version_ihl);
	if (!ip->hdr.version_ihl)
		val8 = RTE_IPV4_VHL_DEF;
	else
		val8 = ip->hdr.version_ihl;
	field = ulp_rte_parser_fld_copy(field, &val8, size);

	size = sizeof(ip->hdr.type_of_service);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.type_of_service, size);

	size = sizeof(ip->hdr.packet_id);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.packet_id, size);

	size = sizeof(ip->hdr.fragment_offset);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.fragment_offset, size);

	size = sizeof(ip->hdr.time_to_live);
	if (!ip->hdr.time_to_live)
		val8 = BNXT_ULP_DEFAULT_TTL;
	else
		val8 = ip->hdr.time_to_live;
	field = ulp_rte_parser_fld_copy(field, &val8, size);

	size = sizeof(ip->hdr.next_proto_id);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.next_proto_id, size);

	size = sizeof(ip->hdr.src_addr);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.src_addr, size);

	size = sizeof(ip->hdr.dst_addr);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.dst_addr, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV4);
}

/* Function to handle the parsing of RTE Flow item ipv6 Header. */
static void
ulp_rte_enc_ipv6_hdr_handler(struct ulp_rte_parser_params *params,
			     const struct rte_flow_item_ipv6 *ip)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;
	uint32_t val32;
	uint8_t val8;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW];
	size = sizeof(ip->hdr.vtc_flow);
	if (!ip->hdr.vtc_flow)
		val32 = rte_cpu_to_be_32(BNXT_ULP_IPV6_DFLT_VER);
	else
		val32 = ip->hdr.vtc_flow;
	field = ulp_rte_parser_fld_copy(field, &val32, size);

	size = sizeof(ip->hdr.proto);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.proto, size);

	size = sizeof(ip->hdr.hop_limits);
	if (!ip->hdr.hop_limits)
		val8 = BNXT_ULP_DEFAULT_TTL;
	else
		val8 = ip->hdr.hop_limits;
	field = ulp_rte_parser_fld_copy(field, &val8, size);

	size = sizeof(ip->hdr.src_addr);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.src_addr, size);

	size = sizeof(ip->hdr.dst_addr);
	field = ulp_rte_parser_fld_copy(field, &ip->hdr.dst_addr, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_IPV6);
}

/* Function to handle the parsing of RTE Flow item UDP Header. */
static void
ulp_rte_enc_udp_hdr_handler(struct ulp_rte_parser_params *params,
			    const struct rte_flow_item_udp *udp_spec)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;
	uint8_t type = IPPROTO_UDP;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_UDP_SPORT];
	size = sizeof(udp_spec->hdr.src_port);
	field = ulp_rte_parser_fld_copy(field, &udp_spec->hdr.src_port, size);

	size = sizeof(udp_spec->hdr.dst_port);
	field = ulp_rte_parser_fld_copy(field, &udp_spec->hdr.dst_port, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_O_UDP);

	/* Update the ip header protocol */
	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV4_PROTO];
	ulp_rte_parser_fld_copy(field, &type, sizeof(type));
	field = &params->enc_field[BNXT_ULP_ENC_FIELD_IPV6_PROTO];
	ulp_rte_parser_fld_copy(field, &type, sizeof(type));
}

/* Function to handle the parsing of RTE Flow item vxlan Header. */
static void
ulp_rte_enc_vxlan_hdr_handler(struct ulp_rte_parser_params *params,
			      struct rte_flow_item_vxlan *vxlan_spec)
{
	struct ulp_rte_hdr_field *field;
	uint32_t size;

	field = &params->enc_field[BNXT_ULP_ENC_FIELD_VXLAN_FLAGS];
	size = sizeof(vxlan_spec->hdr.flags);
	field = ulp_rte_parser_fld_copy(field, &vxlan_spec->hdr.flags, size);

	size = sizeof(vxlan_spec->hdr.rsvd0);
	field = ulp_rte_parser_fld_copy(field, &vxlan_spec->hdr.rsvd0, size);

	size = sizeof(vxlan_spec->hdr.vni);
	field = ulp_rte_parser_fld_copy(field, &vxlan_spec->hdr.vni, size);

	size = sizeof(vxlan_spec->hdr.rsvd1);
	field = ulp_rte_parser_fld_copy(field, &vxlan_spec->hdr.rsvd1, size);

	ULP_BITMAP_SET(params->enc_hdr_bitmap.bits, BNXT_ULP_HDR_BIT_T_VXLAN);
}

/* Function to handle the parsing of RTE Flow action vxlan_encap Header. */
int32_t
ulp_rte_vxlan_encap_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_vxlan_encap *vxlan_encap;
	const struct rte_flow_item *item;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv6 *ipv6_spec;
	struct rte_flow_item_vxlan vxlan_spec;
	uint32_t vlan_num = 0, vlan_size = 0;
	uint32_t ip_size = 0, ip_type = 0;
	uint32_t vxlan_size = 0;
	struct ulp_rte_act_bitmap *act = &params->act_bitmap;
	struct ulp_rte_act_prop *ap = &params->act_prop;

	vxlan_encap = action_item->conf;
	if (!vxlan_encap) {
		BNXT_DRV_DBG(ERR, "Parse Error: Vxlan_encap arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	item = vxlan_encap->definition;
	if (!item) {
		BNXT_DRV_DBG(ERR, "Parse Error: definition arg is invalid\n");
		return BNXT_TF_RC_ERROR;
	}

	if (!ulp_rte_item_skip_void(&item, 0))
		return BNXT_TF_RC_ERROR;

	/* must have ethernet header */
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH) {
		BNXT_DRV_DBG(ERR, "Parse Error:vxlan encap does not have eth\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Parse the ethernet header */
	if (item->spec)
		ulp_rte_enc_eth_hdr_handler(params, item->spec);

	/* Goto the next item */
	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* May have vlan header */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		if (item->spec)
			ulp_rte_enc_vlan_hdr_handler(params, item->spec, 0);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	}

	/* may have two vlan headers */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		vlan_num++;
		if (item->spec)
			ulp_rte_enc_vlan_hdr_handler(params, item->spec, 1);

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
		if (ipv4_spec)
			ulp_rte_enc_ipv4_hdr_handler(params, ipv4_spec);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_IPV6) {
		ipv6_spec = item->spec;
		ip_size = BNXT_ULP_ENCAP_IPV6_SIZE;

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
		if (ipv6_spec)
			ulp_rte_enc_ipv6_hdr_handler(params, ipv6_spec);

		if (!ulp_rte_item_skip_void(&item, 1))
			return BNXT_TF_RC_ERROR;
	} else {
		BNXT_DRV_DBG(ERR, "Parse Error: Vxlan Encap expects L3 hdr\n");
		return BNXT_TF_RC_ERROR;
	}

	/* L4 is UDP */
	if (item->type != RTE_FLOW_ITEM_TYPE_UDP) {
		BNXT_DRV_DBG(ERR, "vxlan encap does not have udp\n");
		return BNXT_TF_RC_ERROR;
	}
	if (item->spec)
		ulp_rte_enc_udp_hdr_handler(params, item->spec);

	if (!ulp_rte_item_skip_void(&item, 1))
		return BNXT_TF_RC_ERROR;

	/* Finally VXLAN */
	if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN) {
		BNXT_DRV_DBG(ERR, "vxlan encap does not have vni\n");
		return BNXT_TF_RC_ERROR;
	}
	vxlan_size = sizeof(struct rte_flow_item_vxlan);
	/* copy the vxlan details */
	memcpy(&vxlan_spec, item->spec, vxlan_size);
	vxlan_spec.hdr.flags = 0x08;
	vxlan_size = tfp_cpu_to_be_32(vxlan_size);
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ],
	       &vxlan_size, sizeof(uint32_t));

	ulp_rte_enc_vxlan_hdr_handler(params, &vxlan_spec);

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
		memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_COUNT],
		       &act_count->id,
		       BNXT_ULP_ACT_PROP_SZ_COUNT);
	}

	/* Update the hdr_bitmap with count */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_COUNT);
	return BNXT_TF_RC_SUCCESS;
}

static bool ulp_rte_parser_is_portb_vfrep(struct ulp_rte_parser_params *param)
{
	return ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_B_IS_VFREP);
}

/*
 * Swaps info related to multi-port:
 * common:
 *    BNXT_ULP_CF_IDX_MP_B_IS_VFREP, BNXT_ULP_CF_IDX_MP_A_IS_VFREP
 *    BNXT_ULP_CF_IDX_MP_PORT_A, BNXT_ULP_CF_IDX_MP_PORT_B
 *
 * ingress:
 *    BNXT_ULP_CF_IDX_MP_VNIC_B, BNXT_ULP_CF_IDX_MP_VNIC_A
 *
 * egress:
 *    BNXT_ULP_CF_IDX_MP_MDATA_B, BNXT_ULP_CF_IDX_MP_MDATA_A
 *    BNXT_ULP_CF_IDX_MP_VPORT_B, BNXT_ULP_CF_IDX_MP_VPORT_A
 *
 * Note: This is done as OVS could give us a non-VFREP port in port B, and we
 * cannot use that to mirror, so we swap out the ports so that a VFREP is now
 * in port B instead.
 */
static int32_t
ulp_rte_parser_normalize_port_info(struct ulp_rte_parser_params *param)
{
	uint16_t mp_port_a, mp_port_b, mp_mdata_a, mp_mdata_b,
		 mp_vport_a, mp_vport_b, mp_vnic_a, mp_vnic_b,
		 mp_is_vfrep_a, mp_is_vfrep_b;

	mp_is_vfrep_a = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_A_IS_VFREP);
	mp_is_vfrep_b = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_B_IS_VFREP);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_B_IS_VFREP, mp_is_vfrep_a);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_A_IS_VFREP, mp_is_vfrep_b);

	mp_port_a = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_PORT_A);
	mp_port_b = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_PORT_B);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_PORT_B, mp_port_a);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_PORT_A, mp_port_b);

	mp_vport_a = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_VPORT_A);
	mp_vport_b = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_VPORT_B);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_VPORT_B, mp_vport_a);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_VPORT_A, mp_vport_b);

	mp_vnic_a = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_VNIC_A);
	mp_vnic_b = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_VNIC_B);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_VNIC_B, mp_vnic_a);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_VNIC_A, mp_vnic_b);

	mp_mdata_a = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_MDATA_A);
	mp_mdata_b = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_MDATA_B);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_MDATA_B, mp_mdata_a);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_MDATA_A, mp_mdata_b);

	return BNXT_TF_RC_SUCCESS;
}


/* Function to handle the parsing of action ports. */
static int32_t
ulp_rte_parser_act_port_set(struct ulp_rte_parser_params *param,
			    uint32_t ifindex, bool multi_port,
			    enum bnxt_ulp_direction_type act_dir)
{
	enum bnxt_ulp_direction_type dir;
	uint16_t pid_s;
	uint8_t *p_mdata;
	uint32_t pid, port_index;
	struct ulp_rte_act_prop *act = &param->act_prop;
	enum bnxt_ulp_intf_type port_type;
	uint32_t vnic_type;

	/* Get the direction */
	/* If action implicitly specifies direction, use the specification. */
	dir = (act_dir == BNXT_ULP_DIR_INVALID) ?
		ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_DIRECTION) :
		act_dir;

	port_type = ULP_COMP_FLD_IDX_RD(param,
					BNXT_ULP_CF_IDX_ACT_PORT_TYPE);

	/* Update flag if Port A/B type is VF-REP */
	ULP_COMP_FLD_IDX_WR(param, multi_port ?
					BNXT_ULP_CF_IDX_MP_B_IS_VFREP :
					BNXT_ULP_CF_IDX_MP_A_IS_VFREP,
			    (port_type == BNXT_ULP_INTF_TYPE_VF_REP) ? 1 : 0);

	/* An egress flow where the action port is not another VF endpoint
	 * requires a VPORT.
	 */
	if (dir == BNXT_ULP_DIR_EGRESS) {
		/* For egress direction, fill vport */
		if (ulp_port_db_vport_get(param->ulp_ctx, ifindex, &pid_s))
			return BNXT_TF_RC_ERROR;

		pid = pid_s;
		pid = rte_cpu_to_be_32(pid);
		if (!multi_port)
			memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VPORT],
			       &pid, BNXT_ULP_ACT_PROP_SZ_VPORT);

		/* Fill metadata */
		if (port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
			port_index  = ULP_COMP_FLD_IDX_RD(param, multi_port ?
								 BNXT_ULP_CF_IDX_MP_PORT_B :
								 BNXT_ULP_CF_IDX_MP_PORT_A);
			if (ulp_port_db_port_meta_data_get(param->ulp_ctx,
							   port_index, &p_mdata))
				return BNXT_TF_RC_ERROR;
			/*
			 * Update appropriate port (A/B) metadata based on multi-port
			 * indication
			 */
			ULP_COMP_FLD_IDX_WR(param,
					    multi_port ?
						BNXT_ULP_CF_IDX_MP_MDATA_B :
						BNXT_ULP_CF_IDX_MP_MDATA_A,
					    rte_cpu_to_be_16(*((uint16_t *)p_mdata)));
		}
		/*
		 * Update appropriate port (A/B) VPORT based on multi-port
		 * indication.
		 */
		ULP_COMP_FLD_IDX_WR(param,
				    multi_port ?
					BNXT_ULP_CF_IDX_MP_VPORT_B :
					BNXT_ULP_CF_IDX_MP_VPORT_A,
				    pid_s);

		/* Setup the VF_TO_VF VNIC information */
		if (!multi_port && port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
			if (ulp_port_db_default_vnic_get(param->ulp_ctx,
							 ifindex,
							 BNXT_ULP_VF_FUNC_VNIC,
							 &pid_s))
				return BNXT_TF_RC_ERROR;
			pid = pid_s;

			/* Allows use of func_opcode with VNIC */
			ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_VNIC, pid);
		}
	} else {
		/* For ingress direction, fill vnic */
		/*
		 * Action               Destination
		 * ------------------------------------
		 * PORT_REPRESENTOR     Driver Function
		 * ------------------------------------
		 * REPRESENTED_PORT     VF
		 * ------------------------------------
		 * PORT_ID              VF
		 */
		if (act_dir != BNXT_ULP_DIR_INGRESS &&
		    port_type == BNXT_ULP_INTF_TYPE_VF_REP)
			vnic_type = BNXT_ULP_VF_FUNC_VNIC;
		else
			vnic_type = BNXT_ULP_DRV_FUNC_VNIC;

		if (ulp_port_db_default_vnic_get(param->ulp_ctx, ifindex,
						 vnic_type, &pid_s))
			return BNXT_TF_RC_ERROR;

		pid = pid_s;

		/* Allows use of func_opcode with VNIC */
		ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_VNIC, pid);

		pid = rte_cpu_to_be_32(pid);
		if (!multi_port)
			memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_VNIC],
			       &pid, BNXT_ULP_ACT_PROP_SZ_VNIC);
		/*
		 * Update appropriate port (A/B) VNIC based on multi-port
		 * indication.
		 */
		ULP_COMP_FLD_IDX_WR(param,
				    multi_port ?
					BNXT_ULP_CF_IDX_MP_VNIC_B :
					BNXT_ULP_CF_IDX_MP_VNIC_A,
				    pid_s);
	}

	if (multi_port && !ulp_rte_parser_is_portb_vfrep(param))
		ulp_rte_parser_normalize_port_info(param);

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
		BNXT_DRV_DBG(ERR, "Invalid port id\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Check the port is PF port */
	intf_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (intf_type != BNXT_ULP_INTF_TYPE_PF) {
		BNXT_DRV_DBG(ERR, "Port is not a PF port\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Update the action properties */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	return ulp_rte_parser_act_port_set(params, ifindex, false,
					   BNXT_ULP_DIR_INVALID);
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
		BNXT_DRV_DBG(ERR, "ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	if (vf_action->original) {
		BNXT_DRV_DBG(ERR, "ParseErr:VF Original not supported\n");
		return BNXT_TF_RC_PARSE_ERR;
	}

	bp = bnxt_pmd_get_bp(params->port_id);
	if (bp == NULL) {
		BNXT_DRV_DBG(ERR, "Invalid bp\n");
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
		BNXT_DRV_DBG(ERR, "VF is not valid interface\n");
		return BNXT_TF_RC_ERROR;
	}
	/* Check the port is VF port */
	intf_type = ulp_port_db_port_type_get(params->ulp_ctx, ifindex);
	if (intf_type != BNXT_ULP_INTF_TYPE_VF &&
	    intf_type != BNXT_ULP_INTF_TYPE_TRUSTED_VF) {
		BNXT_DRV_DBG(ERR, "Port is not a VF port\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Update the action properties */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	return ulp_rte_parser_act_port_set(params, ifindex, false,
					   BNXT_ULP_DIR_INVALID);
}

/* Parse actions PORT_ID, PORT_REPRESENTOR and REPRESENTED_PORT. */
int32_t
ulp_rte_port_act_handler(const struct rte_flow_action *act_item,
			 struct ulp_rte_parser_params *param)
{
	uint32_t ethdev_id;
	uint32_t ifindex;
	const struct rte_flow_action_port_id *port_id = act_item->conf;
	uint32_t num_ports;
	enum bnxt_ulp_intf_type intf_type;
	enum bnxt_ulp_direction_type act_dir;

	if (!act_item->conf) {
		BNXT_DRV_DBG(ERR,
				"ParseErr: Invalid Argument\n");
		return BNXT_TF_RC_PARSE_ERR;
	}
	switch (act_item->type) {
	case RTE_FLOW_ACTION_TYPE_PORT_ID: {
		const struct rte_flow_action_port_id *port_id = act_item->conf;

		if (port_id->original) {
			BNXT_DRV_DBG(ERR,
				    "ParseErr:Portid Original not supported\n");
			return BNXT_TF_RC_PARSE_ERR;
		}
		ethdev_id = port_id->id;
		act_dir = BNXT_ULP_DIR_INVALID;
		break;
	}
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR: {
		const struct rte_flow_action_ethdev *ethdev = act_item->conf;

		ethdev_id = ethdev->port_id;
		act_dir = BNXT_ULP_DIR_INGRESS;
		break;
	}
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT: {
		const struct rte_flow_action_ethdev *ethdev = act_item->conf;

		ethdev_id = ethdev->port_id;
		act_dir = BNXT_ULP_DIR_EGRESS;
		break;
	}
	default:
		BNXT_DRV_DBG(ERR, "Unknown port action\n");
		return BNXT_TF_RC_ERROR;
	}

	num_ports  = ULP_COMP_FLD_IDX_RD(param, BNXT_ULP_CF_IDX_MP_NPORTS);

	if (num_ports) {
		ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_PORT_B,
				    port_id->id);
		ULP_BITMAP_SET(param->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_MULTIPLE_PORT);
	} else {
		ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_PORT_A,
				    port_id->id);
	}

	/* Get the port db ifindex */
	if (ulp_port_db_dev_port_to_ulp_index(param->ulp_ctx, ethdev_id,
					      &ifindex)) {
		BNXT_DRV_DBG(ERR, "Invalid port id\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Get the intf type */
	intf_type = ulp_port_db_port_type_get(param->ulp_ctx, ifindex);
	if (!intf_type) {
		BNXT_DRV_DBG(ERR, "Invalid port type\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Set the action port */
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_ACT_PORT_TYPE, intf_type);
	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID,
			    ethdev_id);

	ULP_COMP_FLD_IDX_WR(param, BNXT_ULP_CF_IDX_MP_NPORTS, ++num_ports);
	return ulp_rte_parser_act_port_set(param, ifindex,
					   ULP_BITMAP_ISSET(param->act_bitmap.bits,
							    BNXT_ULP_ACT_BIT_MULTIPLE_PORT),
					   act_dir);
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
			BNXT_DRV_DBG(ERR,
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
	BNXT_DRV_DBG(ERR, "Parse Error: Push vlan arg is invalid\n");
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
	BNXT_DRV_DBG(ERR, "Parse Error: Vlan vid arg is invalid\n");
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
	BNXT_DRV_DBG(ERR, "Parse Error: Vlan pcp arg is invalid\n");
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
	BNXT_DRV_DBG(ERR, "Parse Error: set ipv4 src arg is invalid\n");
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
	BNXT_DRV_DBG(ERR, "Parse Error: set ipv4 dst arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set ipv6 src.*/
int32_t
ulp_rte_set_ipv6_src_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_ipv6 *set_ipv6;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_ipv6 = action_item->conf;
	if (set_ipv6) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC],
		       &set_ipv6->ipv6_addr, BNXT_ULP_ACT_PROP_SZ_SET_IPV6_SRC);
		/* Update the hdr_bitmap with set ipv4 src */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_IPV6_SRC);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_DRV_DBG(ERR, "Parse Error: set ipv6 src arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set ipv6 dst.*/
int32_t
ulp_rte_set_ipv6_dst_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_ipv6 *set_ipv6;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_ipv6 = action_item->conf;
	if (set_ipv6) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST],
		       &set_ipv6->ipv6_addr, BNXT_ULP_ACT_PROP_SZ_SET_IPV6_DST);
		/* Update the hdr_bitmap with set ipv6 dst */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_IPV6_DST);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_DRV_DBG(ERR, "Parse Error: set ipv6 dst arg is invalid\n");
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

	BNXT_DRV_DBG(ERR, "Parse Error: set tp src arg is invalid\n");
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

	BNXT_DRV_DBG(ERR, "Parse Error: set tp src arg is invalid\n");
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

/* Function to handle the parsing of RTE Flow action set ttl.*/
int32_t
ulp_rte_set_ttl_act_handler(const struct rte_flow_action *action_item,
			    struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_ttl *set_ttl;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_ttl = action_item->conf;
	if (set_ttl) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_TTL],
		       &set_ttl->ttl_value, BNXT_ULP_ACT_PROP_SZ_SET_TTL);
		/* Update the act_bitmap with dec ttl */
		/* Note: NIC HW not support the set_ttl action, here using dec_ttl to simulate
		 * the set_ttl action. And ensure the ttl field must be one more than the value
		 * of action set_ttl.
		 */
		if (ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_O_L3_TTL) ==
		    (uint32_t)(set_ttl->ttl_value + 1)) {
			ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_DEC_TTL);
			return BNXT_TF_RC_SUCCESS;
		}
		BNXT_DRV_DBG(ERR, "Parse Error: set_ttl value not match with flow ttl field.\n");
		return BNXT_TF_RC_ERROR;
	}

	BNXT_DRV_DBG(ERR, "Parse Error: set ttl arg is invalid.\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action JUMP */
int32_t
ulp_rte_jump_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_jump *jump_act;
	struct ulp_rte_act_prop *act = &params->act_prop;
	uint32_t group_id;

	jump_act = action_item->conf;
	if (jump_act) {
		group_id = rte_cpu_to_be_32(jump_act->group);
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_JUMP],
		       &group_id, BNXT_ULP_ACT_PROP_SZ_JUMP);
		ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_JUMP);
	}
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

int32_t
ulp_rte_action_hdlr_handler(const struct rte_flow_action *action_item,
			   struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_handle *handle;
	struct bnxt_ulp_shared_act_info *act_info;
	uint64_t action_bitmask;
	uint32_t shared_action_type;
	struct ulp_rte_act_prop *act = &params->act_prop;
	uint64_t tmp64;
	enum bnxt_ulp_direction_type dir, handle_dir;
	uint32_t act_info_entries = 0;
	int32_t ret;

	handle = action_item->conf;

	/* Have to use the computed direction since the params->dir_attr
	 * can be different (transfer, ingress, egress)
	 */
	dir = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_DIRECTION);

	/* direction of shared action must match direction of flow */
	ret = bnxt_get_action_handle_direction(handle, &handle_dir);
	if (unlikely(ret || dir != handle_dir)) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle or direction\n");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(bnxt_get_action_handle_type(handle, &shared_action_type))) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle\n");
		return BNXT_TF_RC_ERROR;
	}

	act_info = bnxt_ulp_shared_act_info_get(&act_info_entries);
	if (unlikely(shared_action_type >= act_info_entries || !act_info)) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle\n");
		return BNXT_TF_RC_ERROR;
	}

	action_bitmask = act_info[shared_action_type].act_bitmask;

	/* shared actions of the same type cannot be repeated */
	if (unlikely(params->act_bitmap.bits & action_bitmask)) {
		BNXT_DRV_DBG(ERR, "indirect actions cannot be repeated\n");
		return BNXT_TF_RC_ERROR;
	}

	tmp64 = tfp_cpu_to_be_64((uint64_t)bnxt_get_action_handle_index(handle));

	memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE],
	       &tmp64, BNXT_ULP_ACT_PROP_SZ_SHARED_HANDLE);

	ULP_BITMAP_SET(params->act_bitmap.bits, action_bitmask);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of bnxt vendor Flow action vxlan Header. */
int32_t
ulp_vendor_vxlan_decap_act_handler(const struct rte_flow_action *action_item,
				   struct ulp_rte_parser_params *params)
{
	/* Set the F1 flow header bit */
	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_F1);
	return ulp_rte_vxlan_decap_act_handler(action_item, params);
}

/* Function to handle the parsing of bnxt vendor Flow item vxlan Header. */
int32_t
ulp_rte_vendor_vxlan_decap_hdr_handler(const struct rte_flow_item *item,
				       struct ulp_rte_parser_params *params)
{
	RTE_SET_USED(item);
	/* Set the F2 flow header bit */
	ULP_BITMAP_SET(params->hdr_bitmap.bits, BNXT_ULP_HDR_BIT_F2);
	return ulp_rte_vxlan_decap_act_handler(NULL, params);
}

/* Function to handle the parsing of RTE Flow action queue. */
int32_t
ulp_rte_queue_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_rte_parser_params *param)
{
	const struct rte_flow_action_queue *q_info;
	struct ulp_rte_act_prop *ap = &param->act_prop;

	if (action_item == NULL || action_item->conf == NULL) {
		BNXT_DRV_DBG(ERR, "Parse Err: invalid queue configuration\n");
		return BNXT_TF_RC_ERROR;
	}

	q_info = action_item->conf;
	/* Copy the queue into the specific action properties */
	memcpy(&ap->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       &q_info->index, BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);

	/* set the queue action header bit */
	ULP_BITMAP_SET(param->act_bitmap.bits, BNXT_ULP_ACT_BIT_QUEUE);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action meter. */
int32_t
ulp_rte_meter_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_meter *meter;
	struct ulp_rte_act_prop *act_prop = &params->act_prop;
	uint32_t tmp_meter_id;

	if (unlikely(action_item == NULL || action_item->conf == NULL)) {
		BNXT_DRV_DBG(ERR, "Parse Err: invalid meter configuration\n");
		return BNXT_TF_RC_ERROR;
	}

	meter = action_item->conf;
	/* validate the mtr_id and update the reference counter */
	tmp_meter_id = tfp_cpu_to_be_32(meter->mtr_id);
	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_METER],
	       &tmp_meter_id,
	       BNXT_ULP_ACT_PROP_SZ_METER);

	/* set the meter action header bit */
	ULP_BITMAP_SET(params->act_bitmap.bits, BNXT_ULP_ACT_BIT_METER);

	return BNXT_TF_RC_SUCCESS;
}

/* Function to handle the parsing of RTE Flow action set mac src.*/
int32_t
ulp_rte_set_mac_src_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_mac *set_mac;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_mac = action_item->conf;
	if (likely(set_mac)) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC],
		       set_mac->mac_addr, BNXT_ULP_ACT_PROP_SZ_SET_MAC_SRC);
		/* Update the hdr_bitmap with set mac src */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_MAC_SRC);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_DRV_DBG(ERR, "Parse Error: set mac src arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}

/* Function to handle the parsing of RTE Flow action set mac dst.*/
int32_t
ulp_rte_set_mac_dst_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params)
{
	const struct rte_flow_action_set_mac *set_mac;
	struct ulp_rte_act_prop *act = &params->act_prop;

	set_mac = action_item->conf;
	if (likely(set_mac)) {
		memcpy(&act->act_details[BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST],
		       set_mac->mac_addr, BNXT_ULP_ACT_PROP_SZ_SET_MAC_DST);
		/* Update the hdr_bitmap with set ipv4 dst */
		ULP_BITMAP_SET(params->act_bitmap.bits,
			       BNXT_ULP_ACT_BIT_SET_MAC_DST);
		return BNXT_TF_RC_SUCCESS;
	}
	BNXT_DRV_DBG(ERR, "Parse Error: set mac dst arg is invalid\n");
	return BNXT_TF_RC_ERROR;
}
