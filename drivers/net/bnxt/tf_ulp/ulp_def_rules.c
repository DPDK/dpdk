/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include "bnxt_tf_common.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_utils.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"

struct bnxt_ulp_def_param_handler {
	int32_t (*vfr_func)(struct bnxt_ulp_context *ulp_ctx,
			    struct ulp_tlv_param *param,
			    struct bnxt_ulp_mapper_create_parms *mapper_params);
};

static int32_t
ulp_set_svif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 uint32_t  ifindex, uint8_t svif_type,
			 struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t svif;
	uint8_t idx;
	int rc;

	rc = ulp_port_db_svif_get(ulp_ctx, ifindex, svif_type, &svif);
	if (rc)
		return rc;

	if (svif_type == BNXT_ULP_PHY_PORT_SVIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_SVIF;
	else if (svif_type == BNXT_ULP_DRV_FUNC_SVIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_SVIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_SVIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, svif);

	return 0;
}

static int32_t
ulp_set_spif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 uint32_t  ifindex, uint8_t spif_type,
			 struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t spif;
	uint8_t idx;
	int rc;

	rc = ulp_port_db_spif_get(ulp_ctx, ifindex, spif_type, &spif);
	if (rc)
		return rc;

	if (spif_type == BNXT_ULP_PHY_PORT_SPIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_SPIF;
	else if (spif_type == BNXT_ULP_DRV_FUNC_SPIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_SPIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_SPIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, spif);

	return 0;
}

static int32_t
ulp_set_parif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t  ifindex, uint8_t parif_type,
			  struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t parif;
	uint8_t idx;
	int rc;

	rc = ulp_port_db_parif_get(ulp_ctx, ifindex, parif_type, &parif);
	if (rc)
		return rc;

	if (parif_type == BNXT_ULP_PHY_PORT_PARIF)
		idx = BNXT_ULP_CF_IDX_PHY_PORT_PARIF;
	else if (parif_type == BNXT_ULP_DRV_FUNC_PARIF)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_PARIF;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, parif);

	return 0;
}

static int32_t
ulp_set_vport_in_comp_fld(struct bnxt_ulp_context *ulp_ctx, uint32_t ifindex,
			  struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t vport;
	int rc;

	rc = ulp_port_db_vport_get(ulp_ctx, ifindex, &vport);
	if (rc)
		return rc;

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_PHY_PORT_VPORT,
			    vport);
	return 0;
}

static int32_t
ulp_set_vnic_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 uint32_t  ifindex, uint8_t vnic_type,
			 struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t vnic;
	uint8_t idx;
	int rc;

	rc = ulp_port_db_default_vnic_get(ulp_ctx, ifindex, vnic_type, &vnic);
	if (rc)
		return rc;

	if (vnic_type == BNXT_ULP_DRV_FUNC_VNIC)
		idx = BNXT_ULP_CF_IDX_DRV_FUNC_VNIC;
	else
		idx = BNXT_ULP_CF_IDX_VF_FUNC_VNIC;

	ULP_COMP_FLD_IDX_WR(mapper_params, idx, vnic);

	return 0;
}

static int32_t
ulp_set_vlan_in_act_prop(uint16_t port_id,
			 struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	struct ulp_rte_act_prop *act_prop = mapper_params->act_prop;

	if (ULP_BITMAP_ISSET(mapper_params->act->bits,
			     BNXT_ULP_ACT_BIT_SET_VLAN_VID)) {
		BNXT_TF_DBG(ERR,
			    "VLAN already set, multiple VLANs unsupported\n");
		return BNXT_TF_RC_ERROR;
	}

	port_id = rte_cpu_to_be_16(port_id);

	ULP_BITMAP_SET(mapper_params->act->bits,
		       BNXT_ULP_ACT_BIT_SET_VLAN_VID);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG],
	       &port_id, sizeof(port_id));

	return 0;
}

static int32_t
ulp_set_mark_in_act_prop(uint16_t port_id,
			 struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	if (ULP_BITMAP_ISSET(mapper_params->act->bits,
			     BNXT_ULP_ACT_BIT_MARK)) {
		BNXT_TF_DBG(ERR,
			    "MARK already set, multiple MARKs unsupported\n");
		return BNXT_TF_RC_ERROR;
	}

	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_DEV_PORT_ID,
			    port_id);

	return 0;
}

static int32_t
ulp_df_dev_port_handler(struct bnxt_ulp_context *ulp_ctx,
			struct ulp_tlv_param *param,
			struct bnxt_ulp_mapper_create_parms *mapper_params)
{
	uint16_t port_id;
	uint32_t ifindex;
	int rc;

	port_id = param->value[0] | param->value[1];

	rc = ulp_port_db_dev_port_to_ulp_index(ulp_ctx, port_id, &ifindex);
	if (rc) {
		BNXT_TF_DBG(ERR,
				"Invalid port id\n");
		return BNXT_TF_RC_ERROR;
	}

	/* Set port SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_PHY_PORT_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set VF Func SVIF */
	rc = ulp_set_svif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_VF_FUNC_SVIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set port SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_PHY_PORT_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set VF Func SPIF */
	rc = ulp_set_spif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_DRV_FUNC_SPIF,
				      mapper_params);
	if (rc)
		return rc;

	/* Set port PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex,
				       BNXT_ULP_PHY_PORT_PARIF, mapper_params);
	if (rc)
		return rc;

	/* Set DRV Func PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex,
				       BNXT_ULP_DRV_FUNC_PARIF, mapper_params);
	if (rc)
		return rc;

	/* Set VF Func PARIF */
	rc = ulp_set_parif_in_comp_fld(ulp_ctx, ifindex, BNXT_ULP_VF_FUNC_PARIF,
				       mapper_params);
	if (rc)
		return rc;

	/* Set uplink VNIC */
	rc = ulp_set_vnic_in_comp_fld(ulp_ctx, ifindex, true, mapper_params);
	if (rc)
		return rc;

	/* Set VF VNIC */
	rc = ulp_set_vnic_in_comp_fld(ulp_ctx, ifindex, false, mapper_params);
	if (rc)
		return rc;

	/* Set VPORT */
	rc = ulp_set_vport_in_comp_fld(ulp_ctx, ifindex, mapper_params);
	if (rc)
		return rc;

	/* Set VLAN */
	rc = ulp_set_vlan_in_act_prop(port_id, mapper_params);
	if (rc)
		return rc;

	/* Set MARK */
	rc = ulp_set_mark_in_act_prop(port_id, mapper_params);
	if (rc)
		return rc;

	return 0;
}

struct bnxt_ulp_def_param_handler ulp_def_handler_tbl[] = {
	[BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID] = {
			.vfr_func = ulp_df_dev_port_handler }
};

/*
 * Function to create default rules for the following paths
 * 1) Device PORT to DPDK App
 * 2) DPDK App to Device PORT
 * 3) VF Representor to VF
 * 4) VF to VF Representor
 *
 * eth_dev [in] Ptr to rte eth device.
 * param_list [in] Ptr to a list of parameters (Currently, only DPDK port_id).
 * ulp_class_tid [in] Class template ID number.
 * flow_id [out] Ptr to flow identifier.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_default_flow_create(struct rte_eth_dev *eth_dev,
			struct ulp_tlv_param *param_list,
			uint32_t ulp_class_tid,
			uint32_t *flow_id)
{
	struct ulp_rte_hdr_field	hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	uint64_t			comp_fld[BNXT_ULP_CF_IDX_LAST];
	struct bnxt_ulp_mapper_create_parms mapper_params = { 0 };
	struct ulp_rte_act_prop		act_prop;
	struct ulp_rte_act_bitmap	act = { 0 };
	struct bnxt_ulp_context		*ulp_ctx;
	uint32_t type, ulp_flags = 0, fid;
	int rc = 0;

	memset(&mapper_params, 0, sizeof(mapper_params));
	memset(hdr_field, 0, sizeof(hdr_field));
	memset(comp_fld, 0, sizeof(comp_fld));
	memset(&act_prop, 0, sizeof(act_prop));

	mapper_params.hdr_field = hdr_field;
	mapper_params.act = &act;
	mapper_params.act_prop = &act_prop;
	mapper_params.comp_fld = comp_fld;
	mapper_params.class_tid = ulp_class_tid;
	mapper_params.flow_type = BNXT_ULP_FDB_TYPE_DEFAULT;
	mapper_params.port_id = eth_dev->data->port_id;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR,
			    "ULP is not init'ed. Fail to create dflt flow.\n");
		return -EINVAL;
	}

	/* update the vf rep flag */
	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(ulp_ctx, &ulp_flags)) {
		BNXT_TF_DBG(ERR, "Error in getting ULP context flags\n");
		return -EINVAL;
	}
	if (ULP_VF_REP_IS_ENABLED(ulp_flags))
		ULP_COMP_FLD_IDX_WR(&mapper_params,
				    BNXT_ULP_CF_IDX_VFR_MODE, 1);

	type = param_list->type;
	while (type != BNXT_ULP_DF_PARAM_TYPE_LAST) {
		if (ulp_def_handler_tbl[type].vfr_func) {
			rc = ulp_def_handler_tbl[type].vfr_func(ulp_ctx,
								param_list,
								&mapper_params);
			if (rc) {
				BNXT_TF_DBG(ERR,
					    "Failed to create default flow.\n");
				return rc;
			}
		}

		param_list++;
		type = param_list->type;
	}

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 eth_dev->data->port_id,
					 &mapper_params.func_id)) {
		BNXT_TF_DBG(ERR, "conversion of port to func id failed\n");
		goto err1;
	}

	BNXT_TF_DBG(DEBUG, "Creating default flow with template id: %u\n",
		    ulp_class_tid);

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_TF_DBG(ERR, "Flow db lock acquire failed\n");
		goto err1;
	}

	rc = ulp_flow_db_fid_alloc(ulp_ctx, mapper_params.flow_type,
				   mapper_params.func_id, &fid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to allocate flow table entry\n");
		goto err2;
	}

	mapper_params.flow_id = fid;
	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_params);
	if (rc)
		goto err3;

	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
	*flow_id = fid;
	return 0;

err3:
	ulp_flow_db_fid_free(ulp_ctx, mapper_params.flow_type, fid);
err2:
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
err1:
	BNXT_TF_DBG(ERR, "Failed to create default flow.\n");
	return rc;
}

/*
 * Function to destroy default rules for the following paths
 * 1) Device PORT to DPDK App
 * 2) DPDK App to Device PORT
 * 3) VF Representor to VF
 * 4) VF to VF Representor
 *
 * eth_dev [in] Ptr to rte eth device.
 * flow_id [in] Flow identifier.
 *
 * Returns 0 on success or negative number on failure.
 */
int32_t
ulp_default_flow_destroy(struct rte_eth_dev *eth_dev, uint32_t flow_id)
{
	struct bnxt_ulp_context *ulp_ctx;
	int rc = 0;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (!ulp_ctx) {
		BNXT_TF_DBG(ERR, "ULP context is not initialized\n");
		return -EINVAL;
	}

	if (!flow_id) {
		BNXT_TF_DBG(DEBUG, "invalid flow id zero\n");
		return rc;
	}

	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_TF_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}
	rc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT,
				     flow_id);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to destroy flow.\n");
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	return rc;
}

void
bnxt_ulp_destroy_df_rules(struct bnxt *bp, bool global)
{
	struct bnxt_ulp_df_rule_info *info;
	uint16_t port_id;

	if (!BNXT_TRUFLOW_EN(bp) ||
	    BNXT_ETH_DEV_IS_REPRESENTOR(bp->eth_dev))
		return;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	/* Delete default rules per port */
	if (!global) {
		port_id = bp->eth_dev->data->port_id;
		info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
		if (!info->valid)
			return;

		ulp_default_flow_destroy(bp->eth_dev,
					 info->def_port_flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_df_rule_info));
		return;
	}

	/* Delete default rules for all ports */
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
		info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
		if (!info->valid)
			continue;

		ulp_default_flow_destroy(bp->eth_dev,
					 info->def_port_flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_df_rule_info));
	}
}

static int32_t
bnxt_create_port_app_df_rule(struct bnxt *bp, uint8_t flow_type,
			     uint32_t *flow_id)
{
	uint16_t port_id = bp->eth_dev->data->port_id;
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(port_id >> 8) & 0xff, port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};

	if (!flow_type) {
		*flow_id = 0;
		return 0;
	}
	return ulp_default_flow_create(bp->eth_dev, param_list, flow_type,
				       flow_id);
}

int32_t
bnxt_ulp_create_df_rules(struct bnxt *bp)
{
	struct bnxt_ulp_df_rule_info *info;
	uint16_t port_id;
	int rc = 0;

	if (!BNXT_TRUFLOW_EN(bp) ||
	    BNXT_ETH_DEV_IS_REPRESENTOR(bp->eth_dev) || !bp->ulp_ctx)
		return 0;

	port_id = bp->eth_dev->data->port_id;
	info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
	rc = bnxt_create_port_app_df_rule(bp,
					  BNXT_ULP_DF_TPL_DEFAULT_UPLINK_PORT,
					  &info->def_port_flow_id);
	if (rc) {
		BNXT_TF_DBG(ERR,
			    "Failed to create port to app default rule\n");
		return rc;
	}

	rc = ulp_default_flow_db_cfa_action_get(bp->ulp_ctx,
						info->def_port_flow_id,
						&bp->tx_cfa_action);
	if (rc)
		bp->tx_cfa_action = 0;
	info->valid = true;
	return 0;
}

static int32_t
bnxt_create_port_vfr_default_rule(struct bnxt *bp,
				  uint8_t flow_type,
				  uint16_t vfr_port_id,
				  uint32_t *flow_id)
{
	struct ulp_tlv_param param_list[] = {
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID,
			.length = 2,
			.value = {(vfr_port_id >> 8) & 0xff, vfr_port_id & 0xff}
		},
		{
			.type = BNXT_ULP_DF_PARAM_TYPE_LAST,
			.length = 0,
			.value = {0}
		}
	};
	return ulp_default_flow_create(bp->eth_dev, param_list, flow_type,
				       flow_id);
}

int32_t
bnxt_ulp_create_vfr_default_rules(struct rte_eth_dev *vfr_ethdev)
{
	struct bnxt_ulp_vfr_rule_info *info;
	struct bnxt_representor *vfr = vfr_ethdev->data->dev_private;
	struct rte_eth_dev *parent_dev = vfr->parent_dev;
	struct bnxt *bp = parent_dev->data->dev_private;
	uint16_t vfr_port_id = vfr_ethdev->data->port_id;
	uint16_t port_id;
	int rc;

	if (!bp || !BNXT_TRUFLOW_EN(bp))
		return 0;

	port_id = vfr_ethdev->data->port_id;
	info = bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(bp->ulp_ctx, port_id);

	if (!info) {
		BNXT_TF_DBG(ERR, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (info->valid) {
		BNXT_TF_DBG(ERR, "VFR already allocated\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	rc = bnxt_create_port_vfr_default_rule(bp, BNXT_ULP_DF_TPL_DEFAULT_VFR,
					       vfr_port_id,
					       &info->vfr_flow_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create VFR default rule\n");
		goto error;
	}
	rc = ulp_default_flow_db_cfa_action_get(bp->ulp_ctx,
						info->vfr_flow_id,
						&vfr->vfr_tx_cfa_action);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get the tx cfa action\n");
		goto error;
	}

	/* Update the other details */
	info->valid = true;
	info->parent_port_id =  bp->eth_dev->data->port_id;
	return 0;

error:
	if (info->vfr_flow_id)
		ulp_default_flow_destroy(bp->eth_dev, info->vfr_flow_id);
	return rc;
}

int32_t
bnxt_ulp_delete_vfr_default_rules(struct bnxt_representor *vfr)
{
	struct bnxt_ulp_vfr_rule_info *info;
	struct rte_eth_dev *parent_dev = vfr->parent_dev;
	struct bnxt *bp = parent_dev->data->dev_private;

	if (!bp || !BNXT_TRUFLOW_EN(bp))
		return 0;
	info = bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(bp->ulp_ctx,
						    vfr->dpdk_port_id);
	if (!info) {
		BNXT_TF_DBG(ERR, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (!info->valid) {
		BNXT_TF_DBG(ERR, "VFR already freed\n");
		return -EINVAL;
	}
	ulp_default_flow_destroy(bp->eth_dev, info->vfr_flow_id);
	vfr->vfr_tx_cfa_action = 0;
	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	return 0;
}
