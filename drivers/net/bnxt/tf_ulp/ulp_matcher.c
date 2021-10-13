/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include "ulp_matcher.h"
#include "ulp_utils.h"

/* Utility function to calculate the class matcher hash */
static uint32_t
ulp_matcher_class_hash_calculate(uint64_t hi_sig, uint64_t lo_sig)
{
	uint64_t hash;

	hi_sig |= ((hi_sig % BNXT_ULP_CLASS_HID_HIGH_PRIME) <<
		   BNXT_ULP_CLASS_HID_SHFTL);
	lo_sig |= ((lo_sig % BNXT_ULP_CLASS_HID_LOW_PRIME) <<
		   (BNXT_ULP_CLASS_HID_SHFTL + 2));
	hash = hi_sig ^ lo_sig;
	hash = (hash >> BNXT_ULP_CLASS_HID_SHFTR) & BNXT_ULP_CLASS_HID_MASK;
	return (uint32_t)hash;
}

/* Utility function to calculate the action matcher hash */
static uint32_t
ulp_matcher_action_hash_calculate(uint64_t hi_sig, uint64_t app_id)
{
	uint64_t hash;

	hi_sig |= ((hi_sig % BNXT_ULP_ACT_HID_HIGH_PRIME) <<
		   BNXT_ULP_ACT_HID_SHFTL);
	app_id |= ((app_id % BNXT_ULP_CLASS_HID_LOW_PRIME) <<
		   (BNXT_ULP_CLASS_HID_SHFTL + 2));
	hash = hi_sig ^ app_id;
	hash = (hash >> BNXT_ULP_ACT_HID_SHFTR) & BNXT_ULP_ACT_HID_MASK;
	return (uint32_t)hash;
}

/*
 * Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int32_t
ulp_matcher_pattern_match(struct ulp_rte_parser_params *params,
			  uint32_t *class_id)
{
	struct bnxt_ulp_class_match_info *class_match;
	uint32_t class_hid;
	uint8_t vf_to_vf;
	uint16_t tmpl_id;

	/* Get vf to vf flow */
	vf_to_vf = ULP_COMP_FLD_IDX_RD(params, BNXT_ULP_CF_IDX_VF_TO_VF);

	/* calculate the hash of the given flow */
	class_hid = ulp_matcher_class_hash_calculate((params->hdr_bitmap.bits ^
						     params->app_id),
						     params->fld_s_bitmap.bits);

	/* validate the calculate hash values */
	if (class_hid >= BNXT_ULP_CLASS_SIG_TBL_MAX_SZ)
		goto error;
	tmpl_id = ulp_class_sig_tbl[class_hid];
	if (!tmpl_id)
		goto error;

	class_match = &ulp_class_match_list[tmpl_id];
	if (ULP_BITMAP_CMP(&params->hdr_bitmap, &class_match->hdr_sig)) {
		BNXT_TF_DBG(DEBUG, "Proto Header does not match\n");
		goto error;
	}
	if (ULP_BITMAP_CMP(&params->fld_s_bitmap, &class_match->field_sig)) {
		BNXT_TF_DBG(DEBUG, "Field signature does not match\n");
		goto error;
	}

	/* Match the application id before proceeding */
	if (params->app_id != class_match->app_sig) {
		BNXT_TF_DBG(DEBUG, "Field to match the app id %u:%u\n",
			    params->app_id, class_match->app_sig);
		goto error;
	}

	if (vf_to_vf != class_match->act_vnic) {
		BNXT_TF_DBG(DEBUG, "Vnic Match failed\n");
		goto error;
	}
	BNXT_TF_DBG(DEBUG, "Found matching pattern template %d\n",
		    class_match->class_tid);
	*class_id = class_match->class_tid;
	params->hdr_sig_id = class_match->hdr_sig_id;
	params->flow_sig_id = class_match->flow_sig_id;
	params->flow_pattern_id = class_match->flow_pattern_id;
	return BNXT_TF_RC_SUCCESS;

error:
	BNXT_TF_DBG(DEBUG, "Did not find any matching template\n");
	*class_id = 0;
	return BNXT_TF_RC_ERROR;
}

/*
 * Function to handle the matching of RTE Flows and validating
 * the action against the flow templates.
 */
int32_t
ulp_matcher_action_match(struct ulp_rte_parser_params *params,
			 uint32_t *act_id)
{
	uint32_t act_hid;
	uint16_t tmpl_id;
	struct bnxt_ulp_act_match_info *act_match;

	/* calculate the hash of the given flow action */
	act_hid = ulp_matcher_action_hash_calculate(params->act_bitmap.bits,
						    params->app_id);

	/* validate the calculate hash values */
	if (act_hid >= BNXT_ULP_ACT_SIG_TBL_MAX_SZ)
		goto error;
	tmpl_id = ulp_act_sig_tbl[act_hid];
	if (!tmpl_id)
		goto error;

	act_match = &ulp_act_match_list[tmpl_id];
	if (ULP_BITMAP_CMP(&params->act_bitmap, &act_match->act_sig)) {
		BNXT_TF_DBG(DEBUG, "Action Header does not match\n");
		goto error;
	}

	/* Match the application id before proceeding */
	if (params->app_id != act_match->app_sig) {
		BNXT_TF_DBG(DEBUG, "Field to match the app id %u:%u\n",
			    params->app_id, act_match->app_sig);
		goto error;
	}

	*act_id = act_match->act_tid;
	params->act_pattern_id = act_match->act_pattern_id;
	BNXT_TF_DBG(DEBUG, "Found matching action template %u\n", *act_id);
	return BNXT_TF_RC_SUCCESS;

error:
	BNXT_TF_DBG(DEBUG, "Did not find any matching action template\n");
	*act_id = 0;
	return BNXT_TF_RC_ERROR;
}
