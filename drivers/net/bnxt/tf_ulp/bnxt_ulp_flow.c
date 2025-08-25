/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "bnxt_ulp_utils.h"
#include "ulp_rte_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_ha_mgr.h"
#include "ulp_tun.h"
#include <rte_malloc.h>
#include "ulp_template_db_tbl.h"
#include "tfp.h"

static int32_t
bnxt_ulp_flow_validate_args(const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error)
{
	/* Perform the validation of the arguments for null */
	if (unlikely(!error))
		return BNXT_TF_RC_ERROR;

	if (unlikely(!pattern)) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL,
				   "NULL pattern.");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(!actions)) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL,
				   "NULL action.");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(!attr)) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL,
				   "NULL attribute.");
		return BNXT_TF_RC_ERROR;
	}

	if (unlikely(attr->egress && attr->ingress)) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   attr,
				   "EGRESS AND INGRESS UNSUPPORTED");
		return BNXT_TF_RC_ERROR;
	}
	return BNXT_TF_RC_SUCCESS;
}

void
bnxt_ulp_set_dir_attributes(struct ulp_rte_parser_params *params,
			    const struct rte_flow_attr *attr)
{
	/* Set the flow attributes */
	if (attr->egress)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_EGRESS;
	if (attr->ingress)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;
	if (attr->transfer)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_TRANSFER;
	if (attr->group) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_GROUP_ID,
				    rte_cpu_to_le_32(attr->group));
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_GROUP_ID);
	}
}

int32_t
bnxt_ulp_set_prio_attribute(struct ulp_rte_parser_params *params,
			    const struct rte_flow_attr *attr)
{
	uint32_t max_p = bnxt_ulp_max_flow_priority_get(params->ulp_ctx);
	uint32_t min_p = bnxt_ulp_min_flow_priority_get(params->ulp_ctx);

	if (max_p < min_p) {
		if (unlikely(attr->priority > min_p || attr->priority < max_p)) {
			BNXT_DRV_DBG(ERR, "invalid prio, not in range %u:%u\n",
				     max_p, min_p);
			return -EINVAL;
		}
		params->priority = attr->priority;
	} else {
		if (unlikely(attr->priority > max_p || attr->priority < min_p)) {
			BNXT_DRV_DBG(ERR, "invalid prio, not in range %u:%u\n",
				     min_p, max_p);
			return -EINVAL;
		}
		params->priority = max_p - attr->priority;
	}
	/* flows with priority zero is considered as highest and put in EM */
	if (attr->priority >=
	    bnxt_ulp_default_app_priority_get(params->ulp_ctx) &&
	    attr->priority <= bnxt_ulp_max_def_priority_get(params->ulp_ctx)) {
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_DEF_PRIO);
	}
	return 0;
}

void
bnxt_ulp_init_parser_cf_defaults(struct ulp_rte_parser_params *params,
				 uint16_t port_id)
{
	/* Set up defaults for Comp field */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_INCOMING_IF, port_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_DEV_PORT_ID, port_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SVIF_FLAG,
			    BNXT_ULP_INVALID_SVIF_VAL);
}

static void
bnxt_ulp_init_cf_header_bitmap(struct bnxt_ulp_mapper_parms *params)
{
	uint64_t hdr_bits = 0;

	/* Remove the internal tunnel bits */
	hdr_bits = params->hdr_bitmap->bits;
	ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_F2);

	/* Add untag bits */
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_OO_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_OO_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_OI_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_OI_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_IO_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_IO_UNTAGGED);
	if (!ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_II_VLAN))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_II_UNTAGGED);

	/* Add non-tunnel bit */
	if (!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL))
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_NON_TUNNEL);

	/* Add l2 only bit */
	if ((!ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_IPV4) &&
	     !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_IPV6)) ||
	    (ULP_BITMAP_ISSET(params->cf_bitmap, BNXT_ULP_CF_BIT_IS_TUNNEL) &&
	    !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_IPV4) &&
	    !ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_IPV6))) {
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_L2_ONLY);
		ULP_BITMAP_SET(params->cf_bitmap, BNXT_ULP_CF_BIT_L2_ONLY);
	}

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_PROFILE_BITMAP, hdr_bits);

	/* Update the l4 protocol bits */
	if ((ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_TCP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_O_UDP))) {
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_O_TCP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_O_UDP);
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_O_L4_FLOW);
	}

	if ((ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_TCP) ||
	     ULP_BITMAP_ISSET(hdr_bits, BNXT_ULP_HDR_BIT_I_UDP))) {
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_I_TCP);
		ULP_BITMAP_RESET(hdr_bits, BNXT_ULP_HDR_BIT_I_UDP);
		ULP_BITMAP_SET(hdr_bits, BNXT_ULP_HDR_BIT_I_L4_FLOW);
	}
	/*update the comp field header bits */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_HDR_BITMAP, hdr_bits);
}

void
bnxt_ulp_init_mapper_params(struct bnxt_ulp_mapper_parms *mparms,
			    struct ulp_rte_parser_params *params,
			    enum bnxt_ulp_fdb_type flow_type)
{
	uint32_t ulp_flags = 0;

	mparms->flow_type = flow_type;
	mparms->app_priority = params->priority;
	mparms->class_tid = params->class_id;
	mparms->act_tid =  params->act_tmpl;
	mparms->func_id = params->func_id;
	mparms->hdr_bitmap = &params->hdr_bitmap;
	mparms->enc_hdr_bitmap = &params->enc_hdr_bitmap;
	mparms->hdr_field = params->hdr_field;
	mparms->enc_field = params->enc_field;
	mparms->comp_fld = params->comp_fld;
	mparms->act_bitmap = &params->act_bitmap;
	mparms->act_prop = &params->act_prop;
	mparms->parent_flow = params->parent_flow;
	mparms->child_flow = params->child_flow;
	mparms->fld_bitmap = &params->fld_bitmap;
	mparms->flow_pattern_id = params->flow_pattern_id;
	mparms->act_pattern_id = params->act_pattern_id;
	mparms->wc_field_bitmap = params->wc_field_bitmap;
	mparms->app_id = params->app_id;
	mparms->tun_idx = params->tun_idx;
	mparms->cf_bitmap = params->cf_bitmap;
	mparms->exclude_field_bitmap = params->exclude_field_bitmap;

	/* update the signature fields into the computed field list */
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_HDR_SIG_ID,
			    params->class_info_idx);

	/* update the header bitmap */
	bnxt_ulp_init_cf_header_bitmap(mparms);

	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FLOW_SIG_ID,
			    params->flow_sig_id);
	ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_FUNCTION_ID,
			    params->func_id);

	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(params->ulp_ctx, &ulp_flags))
		return;

	/* update the WC Priority flag */
	if (ULP_HIGH_AVAIL_IS_ENABLED(ulp_flags)) {
		enum ulp_ha_mgr_region region = ULP_HA_REGION_LOW;
		int32_t rc;

		rc = ulp_ha_mgr_region_get(params->ulp_ctx, &region);
		if (rc)
			BNXT_DRV_DBG(ERR, "Unable to get WC region\n");
		if (region == ULP_HA_REGION_HI)
			ULP_COMP_FLD_IDX_WR(params,
					    BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG,
					    1);
	} else {
		ULP_COMP_FLD_IDX_WR(params,
				    BNXT_ULP_CF_IDX_HA_SUPPORT_DISABLED,
				    1);
	}

	/* Update the socket direct flag */
	if (ULP_BITMAP_ISSET(params->hdr_bitmap.bits,
			     BNXT_ULP_HDR_BIT_SVIF_IGNORE)) {
		uint32_t ifindex;
		uint16_t vport;

		/* Get the port db ifindex */
		if (unlikely(ulp_port_db_dev_port_to_ulp_index(params->ulp_ctx,
							       params->port_id,
							       &ifindex))) {
			BNXT_DRV_DBG(ERR, "Invalid port id %u\n",
				     params->port_id);
			return;
		}
		/* Update the phy port of the other interface */
		if (unlikely(ulp_port_db_vport_get(params->ulp_ctx, ifindex, &vport))) {
			BNXT_DRV_DBG(ERR, "Invalid port if index %u\n",
				     ifindex);
			return;
		}
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SOCKET_DIRECT_VPORT,
				    (vport == 1) ? 2 : 1);
	}

	/* Update the socket direct svif when socket_direct feature enabled. */
	if (ULP_BITMAP_ISSET(bnxt_ulp_feature_bits_get(params->ulp_ctx),
			     BNXT_ULP_FEATURE_BIT_SOCKET_DIRECT)) {
		enum bnxt_ulp_intf_type intf_type;
		/* For ingress flow on trusted_vf port */
		intf_type = bnxt_pmd_get_interface_type(params->port_id);
		if (intf_type == BNXT_ULP_INTF_TYPE_TRUSTED_VF) {
			uint16_t svif;
			/* Get the socket direct svif of the given dev port */
			if (unlikely(ulp_port_db_dev_port_socket_direct_svif_get(params->ulp_ctx,
										 params->port_id,
										 &svif))) {
				BNXT_DRV_DBG(ERR, "Invalid port id %u\n",
					     params->port_id);
				return;
			}
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_SOCKET_DIRECT_SVIF, svif);
		}
	}
}

/* Function to create the rte flow. */
static struct rte_flow *
bnxt_ulp_flow_create(struct rte_eth_dev *dev,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	struct ulp_rte_parser_params params;
	struct bnxt_ulp_context *ulp_ctx;
	int rc, ret = BNXT_TF_RC_ERROR;
	struct rte_flow *flow_id;
	uint16_t func_id;
	uint32_t fid;

	if (error != NULL)
		error->type = RTE_FLOW_ERROR_TYPE_NONE;

	if (unlikely(bnxt_ulp_flow_validate_args(attr,
						 pattern, actions,
						 error) == BNXT_TF_RC_ERROR)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments being passed\n");
		goto flow_error;
	}

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto flow_error;
	}

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;
	params.port_id = dev->data->port_id;

	if (unlikely(bnxt_ulp_cntxt_app_id_get(params.ulp_ctx, &params.app_id))) {
		BNXT_DRV_DBG(ERR, "failed to get the app id\n");
		goto flow_error;
	}

	/* Set the flow attributes */
	bnxt_ulp_set_dir_attributes(&params, attr);

	if (unlikely(bnxt_ulp_set_prio_attribute(&params, attr)))
		goto flow_error;

	bnxt_ulp_init_parser_cf_defaults(&params, dev->data->port_id);

	/* Get the function id */
	if (unlikely(ulp_port_db_port_func_id_get(ulp_ctx,
						  dev->data->port_id,
						  &func_id))) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto flow_error;
	}

	/* Protect flow creation */
	if (unlikely(bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx))) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto flow_error;
	}

	/* Allocate a Flow ID for attaching all resources for the flow to.
	 * Once allocated, all errors have to walk the list of resources and
	 * free each of them.
	 */
	rc = ulp_flow_db_fid_alloc(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				   func_id, &fid);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Unable to allocate flow table entry\n");
		goto release_lock;
	}

	/* Parse the rte flow pattern */
	ret = bnxt_ulp_rte_parser_hdr_parse(pattern, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	/* Parse the rte flow action */
	ret = bnxt_ulp_rte_parser_act_parse(actions, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	mparms.flow_id = fid;
	mparms.func_id = func_id;
	mparms.port_id = dev->data->port_id;

	/* Perform the rte flow post process */
	bnxt_ulp_rte_parser_post_process(&params);

	/* do the tunnel offload process if any */
	ret = ulp_tunnel_offload_process(&params);
	if (unlikely(ret == BNXT_TF_RC_ERROR))
		goto free_fid;

	ret = ulp_matcher_pattern_match(&params, &params.class_id);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	ret = ulp_matcher_action_match(&params, &params.act_tmpl);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	/* Call the ulp mapper to create the flow in the hardware. */
	ret = ulp_mapper_flow_create(ulp_ctx, &mparms,
				     (void *)error);
	if (unlikely(ret))
		goto free_fid;

	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	flow_id = (struct rte_flow *)((uintptr_t)fid);
	return flow_id;

free_fid:
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, fid);
release_lock:
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
flow_error:
	if (unlikely(error != NULL &&
		     error->type == RTE_FLOW_ERROR_TYPE_NONE))
		rte_flow_error_set(error, ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	return NULL;
}

/* Function to validate the rte flow. */
static int
bnxt_ulp_flow_validate(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error)
{
	struct ulp_rte_parser_params params;
	struct bnxt_ulp_context *ulp_ctx;
	uint32_t class_id, act_tmpl;
	int ret = BNXT_TF_RC_ERROR;

	if (unlikely(bnxt_ulp_flow_validate_args(attr,
						 pattern, actions,
						 error) == BNXT_TF_RC_ERROR)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments being passed\n");
		goto parse_error;
	}

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto parse_error;
	}

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;

	if (unlikely(bnxt_ulp_cntxt_app_id_get(params.ulp_ctx, &params.app_id))) {
		BNXT_DRV_DBG(ERR, "failed to get the app id\n");
		goto parse_error;
	}

	/* Set the flow attributes */
	bnxt_ulp_set_dir_attributes(&params, attr);

	if (unlikely(bnxt_ulp_set_prio_attribute(&params, attr)))
		goto parse_error;

	bnxt_ulp_init_parser_cf_defaults(&params, dev->data->port_id);

	/* Parse the rte flow pattern */
	ret = bnxt_ulp_rte_parser_hdr_parse(pattern, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	/* Parse the rte flow action */
	ret = bnxt_ulp_rte_parser_act_parse(actions, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	/* Perform the rte flow post process */
	bnxt_ulp_rte_parser_post_process(&params);

	/* do the tunnel offload process if any */
	ret = ulp_tunnel_offload_process(&params);
	if (unlikely(ret == BNXT_TF_RC_ERROR))
		goto parse_error;

	ret = ulp_matcher_pattern_match(&params, &class_id);

	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	ret = ulp_matcher_action_match(&params, &act_tmpl);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	/* all good return success */
	return ret;

parse_error:
	rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to validate flow.");
	return -EINVAL;
}

/* Function to destroy the rte flow. */
int
bnxt_ulp_flow_destroy(struct rte_eth_dev *dev,
		      struct rte_flow *flow,
		      struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	uint32_t flow_id;
	uint16_t func_id;
	int ret;

	if (error != NULL)
		error->type = RTE_FLOW_ERROR_TYPE_NONE;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		if (error)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to destroy flow.");
		return -EINVAL;
	}

	flow_id = (uint32_t)(uintptr_t)flow;

	if (unlikely(ulp_port_db_port_func_id_get(ulp_ctx,
						  dev->data->port_id,
						  &func_id))) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		if (error)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to destroy flow.");
		return -EINVAL;
	}

	if (unlikely(ulp_flow_db_validate_flow_func(ulp_ctx, flow_id, func_id) ==
		     false)) {
		BNXT_DRV_DBG(ERR, "Incorrect device params\n");
		if (error)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to destroy flow.");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx))) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}
	ret = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				      flow_id, (void *)error);
	if (unlikely(ret)) {
		BNXT_DRV_DBG(ERR, "Failed to destroy flow.\n");
		if (error != NULL &&
		    error->type == RTE_FLOW_ERROR_TYPE_NONE)
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to destroy flow.");
	}
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	return ret;
}

/* Function to destroy the rte flows. */
static int32_t
bnxt_ulp_flow_flush(struct rte_eth_dev *eth_dev,
		    struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	int32_t ret = 0;
	uint16_t func_id;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(!ulp_ctx))
		return ret;

	/* Free the resources for the last device */
	if (ulp_ctx_deinit_allowed(ulp_ctx)) {
		ret = ulp_flow_db_session_flow_flush(ulp_ctx);
	} else if (bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx)) {
		ret = ulp_port_db_port_func_id_get(ulp_ctx,
						   eth_dev->data->port_id,
						   &func_id);
		if (!ret)
			ret = ulp_flow_db_function_flow_flush(ulp_ctx, func_id);
		else
			BNXT_DRV_DBG(ERR, "convert port to func id failed\n");
	}
	if (unlikely(ret))
		rte_flow_error_set(error, ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush flow.");
	return ret;
}

/*
 * Fill the rte_flow_query_rss 'rss_conf' argument passed
 * in the rte_flow_query() with the values obtained and
 * accumulated locally.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * flow_id [in] The HW flow ID
 *
 * rss_conf [out] The rte_flow_query_count 'data' that is set
 *
 */
static int ulp_flow_query_rss_get(struct bnxt_ulp_context *ctxt,
			   uint32_t flow_id,
			   struct rte_flow_action_rss *rss_conf)
{
	struct ulp_flow_db_res_params params;
	uint32_t nxt_resource_index = 0;
	bool found_cntr_resource = false;
	struct bnxt *bp;
	uint16_t vnic_id = 0;
	int rc = 0;

	bp = bnxt_ulp_cntxt_bp_get(ctxt);
	if (unlikely(!bp)) {
		BNXT_DRV_DBG(ERR, "Failed to get bp from ulp cntxt\n");
		return -EINVAL;
	}

	if (unlikely(bnxt_ulp_cntxt_acquire_fdb_lock(ctxt))) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}
	do {
		rc = ulp_flow_db_resource_get(ctxt,
					      BNXT_ULP_FDB_TYPE_REGULAR,
					      flow_id,
					      &nxt_resource_index,
					      &params);
		if (params.resource_func ==
		     BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE &&
		     (params.resource_sub_type ==
		       BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_RSS ||
		      params.resource_sub_type ==
		       BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE)) {
			vnic_id = params.resource_hndl;
			found_cntr_resource = true;
			break;
		}

	} while (!rc && nxt_resource_index);

	if (found_cntr_resource)
		bnxt_vnic_rss_query_info_fill(bp, rss_conf, vnic_id);

	bnxt_ulp_cntxt_release_fdb_lock(ctxt);

	return rc;
}

/* Function to query the rte flows. */
static int32_t
bnxt_ulp_flow_query(struct rte_eth_dev *eth_dev,
		    struct rte_flow *flow,
		    const struct rte_flow_action *action,
		    void *data,
		    struct rte_flow_error *error)
{
	int rc = 0;
	struct bnxt_ulp_context *ulp_ctx;
	struct rte_flow_action_rss *rss_conf;
	struct rte_flow_query_count *count;
	enum bnxt_ulp_device_id  dev_id;
	uint32_t flow_id;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to query flow.");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Can't identify the device\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to query flow.");
		return -EINVAL;
	}

	flow_id = (uint32_t)(uintptr_t)flow;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
		rss_conf = (struct rte_flow_action_rss *)data;
		rc = ulp_flow_query_rss_get(ulp_ctx, flow_id, rss_conf);
		if (unlikely(rc)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to query RSS info.");
		}

		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		count = data;
		if (dev_id == BNXT_ULP_DEVICE_ID_THOR2)
			rc = ulp_sc_mgr_query_count_get(ulp_ctx, flow_id, count);
		else
			rc = ulp_fc_mgr_query_count_get(ulp_ctx, flow_id, count);

		if (unlikely(rc)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to query flow.");
		}
		break;
	default:
		rte_flow_error_set(error, -rc, RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "Unsupported action item");
	}

	return rc;
}

static int32_t
bnxt_ulp_action_handle_chk_args(const struct rte_flow_action *action,
				const struct rte_flow_indir_action_conf *conf)
{
	if (!action || !conf)
		return BNXT_TF_RC_ERROR;
	/* shared action only allowed to have one direction */
	if (conf->ingress == 1 && conf->egress ==  1)
		return BNXT_TF_RC_ERROR;
	/* shared action must have at least one direction */
	if (conf->ingress == 0 && conf->egress ==  0)
		return BNXT_TF_RC_ERROR;
	return BNXT_TF_RC_SUCCESS;
}

static inline void
bnxt_ulp_set_action_handle_dir_attr(struct ulp_rte_parser_params *params,
				    const struct rte_flow_indir_action_conf *conf)
{
	if (conf->ingress == 1)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_INGRESS;
	else if (conf->egress == 1)
		params->dir_attr |= BNXT_ULP_FLOW_ATTR_EGRESS;
}

static struct rte_flow_action_handle *
bnxt_ulp_action_handle_create(struct rte_eth_dev *dev,
			      const struct rte_flow_indir_action_conf *conf,
			      const struct rte_flow_action *action,
			      struct rte_flow_error *error)
{
	enum bnxt_ulp_intf_type port_type = BNXT_ULP_INTF_TYPE_INVALID;
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	struct ulp_rte_parser_params params;
	struct bnxt_ulp_context *ulp_ctx;
	uint32_t act_tid;
	uint16_t func_id;
	uint32_t ifindex;
	int ret = BNXT_TF_RC_ERROR;
	const struct rte_flow_action actions[2] = {
		{
			.type = action->type,
			.conf = action->conf
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END
		}
	};

	if (error != NULL)
		error->type = RTE_FLOW_ERROR_TYPE_NONE;

	if (unlikely(bnxt_ulp_action_handle_chk_args(action, conf) != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto parse_error;
	}

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;

	ULP_BITMAP_SET(params.act_bitmap.bits, BNXT_ULP_ACT_BIT_SHARED);

	/* Set the shared action direction attribute */
	bnxt_ulp_set_action_handle_dir_attr(&params, conf);

	/* perform the conversion from dpdk port to bnxt ifindex */
	if (unlikely(ulp_port_db_dev_port_to_ulp_index(ulp_ctx,
						       dev->data->port_id,
						       &ifindex))) {
		BNXT_DRV_DBG(ERR, "Port id is not valid\n");
		goto parse_error;
	}
	port_type = ulp_port_db_port_type_get(ulp_ctx, ifindex);
	if (unlikely(port_type == BNXT_ULP_INTF_TYPE_INVALID)) {
		BNXT_DRV_DBG(ERR, "Port type is not valid\n");
		goto parse_error;
	}

	bnxt_ulp_init_parser_cf_defaults(&params, dev->data->port_id);

	/* Emulating the match port for direction processing */
	ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE,
			    port_type);

	if ((params.dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) &&
	    port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
		ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
				    BNXT_ULP_DIR_EGRESS);
	} else {
		/* Assign the input direction */
		if (params.dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS)
			ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_INGRESS);
		else
			ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
	}

	/* perform the conversion from dpdk port to bnxt ifindex */
	if (unlikely(ulp_port_db_dev_port_to_ulp_index(ulp_ctx,
						       dev->data->port_id,
						       &ifindex))) {
		BNXT_DRV_DBG(ERR, "Port id is not valid\n");
		goto parse_error;
	}
	port_type = ulp_port_db_port_type_get(ulp_ctx, ifindex);
	if (unlikely(port_type == BNXT_ULP_INTF_TYPE_INVALID)) {
		BNXT_DRV_DBG(ERR, "Port type is not valid\n");
		goto parse_error;
	}

	bnxt_ulp_init_parser_cf_defaults(&params, dev->data->port_id);

	/* Emulating the match port for direction processing */
	ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_MATCH_PORT_TYPE,
			    port_type);

	if ((params.dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS) &&
	    port_type == BNXT_ULP_INTF_TYPE_VF_REP) {
		ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
				    BNXT_ULP_DIR_EGRESS);
	} else {
		/* Assign the input direction */
		if (params.dir_attr & BNXT_ULP_FLOW_ATTR_INGRESS)
			ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_INGRESS);
		else
			ULP_COMP_FLD_IDX_WR(&params, BNXT_ULP_CF_IDX_DIRECTION,
					    BNXT_ULP_DIR_EGRESS);
	}

	/* Parse the shared action */
	ret = bnxt_ulp_rte_parser_act_parse(actions, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	/* Perform the rte flow post process */
	bnxt_ulp_rte_parser_post_process(&params);

	/* do the tunnel offload process if any */
	ret = ulp_tunnel_offload_process(&params);
	if (unlikely(ret == BNXT_TF_RC_ERROR))
		goto parse_error;

	ret = ulp_matcher_action_match(&params, &act_tid);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	/* Get the function id */
	if (unlikely(ulp_port_db_port_func_id_get(ulp_ctx,
						  dev->data->port_id,
						  &func_id))) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto parse_error;
	}

	/* Protect flow creation */
	if (unlikely(bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx))) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(params.ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	if (unlikely(ret))
		goto parse_error;

	return (struct rte_flow_action_handle *)((uintptr_t)mparms.shared_hndl);

parse_error:
	if (error != NULL &&
	    error->type == RTE_FLOW_ERROR_TYPE_NONE)
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create shared action.");
	return NULL;
}

static int
bnxt_ulp_action_handle_destroy(struct rte_eth_dev *dev,
			       struct rte_flow_action_handle *shared_hndl,
			       struct rte_flow_error *error)
{
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	struct bnxt_ulp_shared_act_info *act_info;
	struct ulp_rte_parser_params params;
	struct ulp_rte_act_prop *act_prop;
	struct bnxt_ulp_context *ulp_ctx;
	enum bnxt_ulp_direction_type dir;
	uint32_t act_tid, act_info_entries;
	int ret = BNXT_TF_RC_ERROR;
	uint32_t shared_action_type;
	uint64_t tmp64;

	if (error != NULL)
		error->type = RTE_FLOW_ERROR_TYPE_NONE;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto parse_error;
	}

	if (unlikely(!shared_hndl)) {
		BNXT_DRV_DBG(ERR, "Invalid argument of shared handle\n");
		goto parse_error;
	}

	act_prop = &params.act_prop;
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;

	if (unlikely(bnxt_ulp_cntxt_app_id_get(ulp_ctx, &params.app_id))) {
		BNXT_DRV_DBG(ERR, "failed to get the app id\n");
		goto parse_error;
	}
	/* The template will delete the entry if there are no references */
	if (unlikely(bnxt_get_action_handle_type(shared_hndl, &shared_action_type))) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle\n");
		goto parse_error;
	}

	act_info_entries = 0;
	act_info = bnxt_ulp_shared_act_info_get(&act_info_entries);
	if (unlikely(shared_action_type >= act_info_entries || !act_info)) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle\n");
		goto parse_error;
	}

	ULP_BITMAP_SET(params.act_bitmap.bits,
		       act_info[shared_action_type].act_bitmask);
	ULP_BITMAP_SET(params.act_bitmap.bits, BNXT_ULP_ACT_BIT_DELETE);

	ret = bnxt_get_action_handle_direction(shared_hndl, &dir);
	if (unlikely(ret)) {
		BNXT_DRV_DBG(ERR, "Invalid shared handle dir\n");
		goto parse_error;
	}

	if (dir == BNXT_ULP_DIR_EGRESS) {
		params.dir_attr = BNXT_ULP_FLOW_ATTR_EGRESS;
		ULP_BITMAP_SET(params.act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_EGR);
	} else {
		params.dir_attr = BNXT_ULP_FLOW_ATTR_INGRESS;
		ULP_BITMAP_SET(params.act_bitmap.bits,
			       BNXT_ULP_FLOW_DIR_BITMASK_ING);
	}

	tmp64 = tfp_cpu_to_be_64((uint64_t)
				 bnxt_get_action_handle_index(shared_hndl));

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE],
	       &tmp64, BNXT_ULP_ACT_PROP_SZ_SHARED_HANDLE);

	ret = ulp_matcher_action_match(&params, &act_tid);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto parse_error;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_REGULAR);
	mparms.act_tid = act_tid;

	if (unlikely(bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx))) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto parse_error;
	}

	ret = ulp_mapper_flow_create(ulp_ctx, &mparms,
				     (void *)error);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
	if (unlikely(ret))
		goto parse_error;

	return 0;

parse_error:
	if (error != NULL &&
	    error->type == RTE_FLOW_ERROR_TYPE_NONE)
		rte_flow_error_set(error, BNXT_TF_RC_ERROR,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to destroy shared action.");
	return -EINVAL;
}

/* Tunnel offload Apis */
#define BNXT_ULP_TUNNEL_OFFLOAD_NUM_ITEMS	1

static int
bnxt_ulp_tunnel_decap_set(struct rte_eth_dev *eth_dev,
			  struct rte_flow_tunnel *tunnel,
			  struct rte_flow_action **pmd_actions,
			  uint32_t *num_of_actions,
			  struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_flow_app_tun_ent *tun_entry;
	int32_t rc = 0;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(ulp_ctx == NULL)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "ULP context uninitialized");
		return -EINVAL;
	}

	if (unlikely(tunnel == NULL)) {
		BNXT_DRV_DBG(ERR, "No tunnel specified\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "no tunnel specified");
		return -EINVAL;
	}

	if (unlikely(tunnel->type != RTE_FLOW_ITEM_TYPE_VXLAN)) {
		BNXT_DRV_DBG(ERR, "Tunnel type unsupported\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "tunnel type unsupported");
		return -EINVAL;
	}

	rc = ulp_app_tun_search_entry(ulp_ctx, tunnel, &tun_entry);
	if (unlikely(rc < 0)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "tunnel decap set failed");
		return -EINVAL;
	}

	rc = ulp_app_tun_entry_set_decap_action(tun_entry);
	if (unlikely(rc < 0)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "tunnel decap set failed");
		return -EINVAL;
	}

	*pmd_actions = &tun_entry->action;
	*num_of_actions = BNXT_ULP_TUNNEL_OFFLOAD_NUM_ITEMS;
	return 0;
}

static int
bnxt_ulp_tunnel_match(struct rte_eth_dev *eth_dev,
		      struct rte_flow_tunnel *tunnel,
		      struct rte_flow_item **pmd_items,
		      uint32_t *num_of_items,
		      struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_flow_app_tun_ent *tun_entry;
	int32_t rc = 0;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(ulp_ctx == NULL)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "ULP context uninitialized");
		return -EINVAL;
	}

	if (unlikely(tunnel == NULL)) {
		BNXT_DRV_DBG(ERR, "No tunnel specified\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "no tunnel specified");
		return -EINVAL;
	}

	if (unlikely(tunnel->type != RTE_FLOW_ITEM_TYPE_VXLAN)) {
		BNXT_DRV_DBG(ERR, "Tunnel type unsupported\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "tunnel type unsupported");
		return -EINVAL;
	}

	rc = ulp_app_tun_search_entry(ulp_ctx, tunnel, &tun_entry);
	if (unlikely(rc < 0)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "tunnel match set failed");
		return -EINVAL;
	}

	rc = ulp_app_tun_entry_set_decap_item(tun_entry);
	if (unlikely(rc < 0)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "tunnel match set failed");
		return -EINVAL;
	}

	*pmd_items = &tun_entry->item;
	*num_of_items = BNXT_ULP_TUNNEL_OFFLOAD_NUM_ITEMS;
	return 0;
}

static int
bnxt_ulp_tunnel_decap_release(struct rte_eth_dev *eth_dev,
			      struct rte_flow_action *pmd_actions,
			      uint32_t num_actions,
			      struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_flow_app_tun_ent *tun_entry;
	const struct rte_flow_action *action_item = pmd_actions;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(ulp_ctx == NULL)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "ULP context uninitialized");
		return -EINVAL;
	}
	if (unlikely(num_actions != BNXT_ULP_TUNNEL_OFFLOAD_NUM_ITEMS)) {
		BNXT_DRV_DBG(ERR, "num actions is invalid\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "num actions is invalid");
		return -EINVAL;
	}
	while (action_item && action_item->type != RTE_FLOW_ACTION_TYPE_END) {
		if (unlikely(action_item->type == (typeof(tun_entry->action.type))
			     BNXT_RTE_FLOW_ACTION_TYPE_VXLAN_DECAP)) {
			tun_entry = ulp_app_tun_match_entry(ulp_ctx,
							    action_item->conf);
			ulp_app_tun_entry_delete(tun_entry);
		}
		action_item++;
	}
	return 0;
}

static int
bnxt_ulp_tunnel_item_release(struct rte_eth_dev *eth_dev,
			     struct rte_flow_item *pmd_items,
			     uint32_t num_items,
			     struct rte_flow_error *error)
{
	struct bnxt_ulp_context *ulp_ctx;
	struct bnxt_flow_app_tun_ent *tun_entry;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (unlikely(ulp_ctx == NULL)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "ULP context uninitialized");
		return -EINVAL;
	}
	if (unlikely(num_items != BNXT_ULP_TUNNEL_OFFLOAD_NUM_ITEMS)) {
		BNXT_DRV_DBG(ERR, "num items is invalid\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "num items is invalid");
		return -EINVAL;
	}

	tun_entry = ulp_app_tun_match_entry(ulp_ctx, pmd_items->spec);
	ulp_app_tun_entry_delete(tun_entry);
	return 0;
}

const struct rte_flow_ops bnxt_ulp_rte_flow_ops = {
	.validate = bnxt_ulp_flow_validate,
	.create = bnxt_ulp_flow_create,
	.destroy = bnxt_ulp_flow_destroy,
	.flush = bnxt_ulp_flow_flush,
	.query = bnxt_ulp_flow_query,
	.isolate = NULL,
	.action_handle_create = bnxt_ulp_action_handle_create,
	.action_handle_destroy = bnxt_ulp_action_handle_destroy,
	/* Tunnel offload callbacks */
	.tunnel_decap_set = bnxt_ulp_tunnel_decap_set,
	.tunnel_match = bnxt_ulp_tunnel_match,
	.tunnel_action_decap_release = bnxt_ulp_tunnel_decap_release,
	.tunnel_item_release = bnxt_ulp_tunnel_item_release,
	.get_restore_info = NULL
};
