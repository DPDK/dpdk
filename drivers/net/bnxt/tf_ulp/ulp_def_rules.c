/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_tf_common.h"
#include "bnxt_ulp_utils.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_utils.h"
#include "ulp_port_db.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_rte_parser.h"
static void
ulp_l2_custom_tunnel_id_update(struct bnxt *bp,
			       struct bnxt_ulp_mapper_parms *params);

struct bnxt_ulp_def_param_handler {
	int32_t (*vfr_func)(struct bnxt_ulp_context *ulp_ctx,
			    struct ulp_tlv_param *param,
			    struct bnxt_ulp_mapper_parms *mapper_params);
};

static int32_t
ulp_set_svif_in_comp_fld(struct bnxt_ulp_context *ulp_ctx,
			 uint32_t  ifindex, uint8_t svif_type,
			 struct bnxt_ulp_mapper_parms *mapper_params)
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
			 struct bnxt_ulp_mapper_parms *mapper_params)
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
			  struct bnxt_ulp_mapper_parms *mapper_params)
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
			  struct bnxt_ulp_mapper_parms *mapper_params)
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
			 struct bnxt_ulp_mapper_parms *mapper_params)
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
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	struct ulp_rte_act_prop *act_prop = mapper_params->act_prop;

	if (ULP_BITMAP_ISSET(mapper_params->act_bitmap->bits,
			     BNXT_ULP_ACT_BIT_SET_VLAN_VID)) {
		BNXT_DRV_DBG(ERR,
			     "VLAN already set, multiple VLANs unsupported\n");
		return BNXT_TF_RC_ERROR;
	}

	port_id = rte_cpu_to_be_16(port_id);

	ULP_BITMAP_SET(mapper_params->act_bitmap->bits,
		       BNXT_ULP_ACT_BIT_SET_VLAN_VID);

	memcpy(&act_prop->act_details[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG],
	       &port_id, sizeof(port_id));

	return 0;
}

static int32_t
ulp_set_mark_in_act_prop(uint16_t port_id,
			 struct bnxt_ulp_mapper_parms *mapper_params)
{
	if (ULP_BITMAP_ISSET(mapper_params->act_bitmap->bits,
			     BNXT_ULP_ACT_BIT_MARK)) {
		BNXT_DRV_DBG(ERR,
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
			struct bnxt_ulp_mapper_parms *mapper_params)
{
	uint16_t port_id;
	uint16_t parif;
	uint32_t ifindex;
	int rc;

	port_id = param->value[0] | param->value[1];

	rc = ulp_port_db_dev_port_to_ulp_index(ulp_ctx, port_id, &ifindex);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Invalid port id\n");
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

	/* Note:
	 * We save the drv_func_parif into CF_IDX of phy_port_parif,
	 * since that index is currently referenced by ingress templates
	 * for datapath flows. If in the future we change the parser to
	 * save it in the CF_IDX of drv_func_parif we also need to update
	 * the template.
	 * WARNING: Two VFs on same parent PF will not work, as the parif is
	 * based on fw fid of the parent PF.
	 */
	parif = ULP_COMP_FLD_IDX_RD(mapper_params, BNXT_ULP_CF_IDX_DRV_FUNC_PARIF);
	ULP_COMP_FLD_IDX_WR(mapper_params, BNXT_ULP_CF_IDX_PHY_PORT_PARIF, parif);

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
			uint16_t port_id,
			uint32_t *flow_id)
{
	struct ulp_rte_hdr_field	hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	uint64_t			comp_fld[BNXT_ULP_CF_IDX_LAST];
	struct bnxt_ulp_mapper_parms mapper_params = { 0 };
	struct ulp_rte_act_prop		act_prop;
	struct ulp_rte_act_bitmap	act = { 0 };
	struct bnxt_ulp_context		*ulp_ctx;
	uint32_t type, ulp_flags = 0, fid;
	struct bnxt *bp = eth_dev->data->dev_private;
	uint16_t static_port = 0;
	int rc = 0;

	memset(&mapper_params, 0, sizeof(mapper_params));
	memset(hdr_field, 0, sizeof(hdr_field));
	memset(comp_fld, 0, sizeof(comp_fld));
	memset(&act_prop, 0, sizeof(act_prop));

	mapper_params.hdr_field = hdr_field;
	mapper_params.act_bitmap = &act;
	mapper_params.act_prop = &act_prop;
	mapper_params.comp_fld = comp_fld;
	mapper_params.class_tid = ulp_class_tid;
	mapper_params.flow_type = BNXT_ULP_FDB_TYPE_DEFAULT;
	mapper_params.port_id = eth_dev->data->port_id;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(eth_dev);
	if (!ulp_ctx) {
		BNXT_DRV_DBG(ERR,
			     "ULP is not init'ed. Fail to create dflt flow.\n");
		return -EINVAL;
	}

	/* update the vf rep flag */
	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(ulp_ctx, &ulp_flags)) {
		BNXT_DRV_DBG(ERR, "Error in getting ULP context flags\n");
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
				BNXT_DRV_DBG(ERR,
					    "Failed to create default flow.\n");
				return rc;
			}
		}

		param_list++;
		type = param_list->type;
	}

	/* Get the function id */
	if (ulp_port_db_port_func_id_get(ulp_ctx,
					 port_id,
					 &mapper_params.func_id)) {
		BNXT_DRV_DBG(ERR, "conversion of port to func id failed\n");
		goto err1;
	}

	/* update the VF meta function id  */
	ULP_COMP_FLD_IDX_WR(&mapper_params, BNXT_ULP_CF_IDX_VF_META_FID,
			    BNXT_ULP_META_VF_FLAG | mapper_params.func_id);

	/* update the upar id */
	ulp_l2_custom_tunnel_id_update(bp, &mapper_params);

	/* update the vxlan port */
	if (ULP_APP_STATIC_VXLAN_PORT_EN(ulp_ctx)) {
		static_port = bnxt_ulp_cntxt_vxlan_port_get(ulp_ctx);
		if (static_port) {
			ULP_COMP_FLD_IDX_WR(&mapper_params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    static_port);
			ULP_BITMAP_SET(mapper_params.cf_bitmap,
				       BNXT_ULP_CF_BIT_STATIC_VXLAN_PORT);
		} else {
			static_port = bnxt_ulp_cntxt_vxlan_ip_port_get(ulp_ctx);
			ULP_COMP_FLD_IDX_WR(&mapper_params,
					    BNXT_ULP_CF_IDX_TUNNEL_PORT,
					    static_port);
			ULP_BITMAP_SET(mapper_params.cf_bitmap,
				       BNXT_ULP_CF_BIT_STATIC_VXLAN_IP_PORT);
		}
	}

	BNXT_DRV_DBG(DEBUG, "Creating default flow with template id: %u\n",
		     ulp_class_tid);

	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		goto err1;
	}

	rc = ulp_flow_db_fid_alloc(ulp_ctx, mapper_params.flow_type,
				   mapper_params.func_id, &fid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to allocate flow table entry\n");
		goto err2;
	}

	mapper_params.flow_id = fid;
	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_params,
				    NULL);
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
	BNXT_DRV_DBG(ERR, "Failed to create default flow.\n");
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
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		return -EINVAL;
	}

	if (!flow_id) {
		BNXT_DRV_DBG(DEBUG, "invalid flow id zero\n");
		return rc;
	}

	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}
	rc = ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT,
				     flow_id,
				     NULL);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed to destroy flow.\n");
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	return rc;
}

static void
bnxt_ulp_destroy_group_rules(struct bnxt *bp, uint16_t port_id)
{
	struct bnxt_ulp_grp_rule_info *info;
	struct bnxt_ulp_grp_rule_info *grp_rules;
	uint16_t idx;

	grp_rules = bp->ulp_ctx->cfg_data->df_rule_info[port_id].grp_df_rule;

	for (idx = 0; idx < BNXT_ULP_MAX_GROUP_CNT; idx++) {
		info = &grp_rules[idx];
		if (!info->valid)
			continue;
		ulp_default_flow_destroy(bp->eth_dev, info->flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_grp_rule_info));
	}
}

void
bnxt_ulp_destroy_df_rules(struct bnxt *bp, bool global)
{
	struct bnxt_ulp_df_rule_info *info;
	uint16_t port_id;

	if (!BNXT_TRUFLOW_EN(bp) ||
	    rte_eth_dev_is_repr(bp->eth_dev))
		return;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	/* Delete default rules per port */
	if (!global) {
		port_id = bp->eth_dev->data->port_id;
		info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
		if (!info->valid)
			return;

		/* Delete the group default rules */
		bnxt_ulp_destroy_group_rules(bp, port_id);

		ulp_default_flow_destroy(bp->eth_dev,
					 info->def_port_flow_id);
		if (info->promisc_flow_id)
			ulp_default_flow_destroy(bp->eth_dev,
						 info->promisc_flow_id);
		memset(info, 0, sizeof(struct bnxt_ulp_df_rule_info));
		return;
	}

	/* Delete default rules for all ports */
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
		info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
		if (!info->valid)
			continue;

		/* Delete the group default rules */
		bnxt_ulp_destroy_group_rules(bp, port_id);

		ulp_default_flow_destroy(bp->eth_dev,
					 info->def_port_flow_id);
		if (info->promisc_flow_id)
			ulp_default_flow_destroy(bp->eth_dev,
						 info->promisc_flow_id);
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
				       port_id, flow_id);
}

int32_t
bnxt_ulp_create_df_rules(struct bnxt *bp)
{
	struct rte_eth_dev *dev = bp->eth_dev;
	struct bnxt_ulp_df_rule_info *info;
	uint16_t port_id;
	int rc = 0;

	if (!BNXT_TRUFLOW_EN(bp) ||
	    rte_eth_dev_is_repr(bp->eth_dev) || !bp->ulp_ctx)
		return 0;

	port_id = bp->eth_dev->data->port_id;
	info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];
	rc = bnxt_create_port_app_df_rule(bp,
					  BNXT_ULP_DF_TPL_DEFAULT_UPLINK_PORT,
					  &info->def_port_flow_id);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Failed to create port to app default rule\n");
		return rc;
	}

	/* If the template already set the bd_action, skip this */
	if (!bp->tx_cfa_action) {
		rc = ulp_default_flow_db_cfa_action_get(bp->ulp_ctx,
							info->def_port_flow_id,
							&bp->tx_cfa_action);
	}

	if (rc || BNXT_TESTPMD_EN(bp))
		bp->tx_cfa_action = 0;

	/* set or reset the promiscuous rule */
	bnxt_ulp_promisc_mode_set(bp, dev->data->promiscuous);

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
				       vfr_port_id,
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
		BNXT_DRV_DBG(ERR, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (info->valid) {
		BNXT_DRV_DBG(ERR, "VFR already allocated\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	rc = bnxt_create_port_vfr_default_rule(bp, BNXT_ULP_DF_TPL_DEFAULT_VFR,
					       vfr_port_id,
					       &info->vfr_flow_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create VFR default rule\n");
		goto error;
	}

	/* If the template already set the bd action, skip this */
	if (!vfr->vfr_tx_cfa_action) {
		rc = ulp_default_flow_db_cfa_action_get(bp->ulp_ctx,
							info->vfr_flow_id,
							&vfr->vfr_tx_cfa_action);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to get the tx cfa action\n");
			goto error;
		}
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
		BNXT_DRV_DBG(ERR, "Failed to get vfr ulp context\n");
		return -EINVAL;
	}

	if (!info->valid) {
		BNXT_DRV_DBG(ERR, "VFR already freed\n");
		return -EINVAL;
	}
	ulp_default_flow_destroy(bp->eth_dev, info->vfr_flow_id);
	vfr->vfr_tx_cfa_action = 0;
	memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	return 0;
}

static void
ulp_l2_custom_tunnel_id_update(struct bnxt *bp,
			       struct bnxt_ulp_mapper_parms *params)
{
	if (!bp->l2_etype_tunnel_cnt)
		return;

	if (bp->l2_etype_upar_in_use &
	    HWRM_TUNNEL_DST_PORT_QUERY_OUTPUT_UPAR_IN_USE_UPAR0) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L2_CUSTOM_UPAR_ID,
				    ULP_WP_SYM_TUN_HDR_TYPE_UPAR1);
	} else if (bp->l2_etype_upar_in_use &
		   HWRM_TUNNEL_DST_PORT_QUERY_OUTPUT_UPAR_IN_USE_UPAR1) {
		ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_L2_CUSTOM_UPAR_ID,
				    ULP_WP_SYM_TUN_HDR_TYPE_UPAR2);
	}
}

/*
 * Function to execute a specific template, this does not create flow id
 *
 * bp [in] Ptr to bnxt
 * param_list [in] Ptr to a list of parameters (Currently, only DPDK port_id).
 * ulp_class_tid [in] Class template ID number.
 *
 * Returns 0 on success or negative number on failure.
 */
static int32_t
ulp_flow_template_process(struct bnxt *bp,
			  struct ulp_tlv_param *param_list,
			  uint32_t ulp_class_tid,
			  uint16_t port_id,
			  uint32_t flow_id)
{
	struct ulp_rte_hdr_field	hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	uint64_t			comp_fld[BNXT_ULP_CF_IDX_LAST];
	struct bnxt_ulp_mapper_parms mapper_params = { 0 };
	struct ulp_rte_act_prop		act_prop;
	struct ulp_rte_act_bitmap	act = { 0 };
	struct bnxt_ulp_context		*ulp_ctx;
	uint32_t type;
	int rc = 0;

	memset(&mapper_params, 0, sizeof(mapper_params));
	memset(hdr_field, 0, sizeof(hdr_field));
	memset(comp_fld, 0, sizeof(comp_fld));
	memset(&act_prop, 0, sizeof(act_prop));

	mapper_params.hdr_field = hdr_field;
	mapper_params.act_bitmap = &act;
	mapper_params.act_prop = &act_prop;
	mapper_params.comp_fld = comp_fld;
	mapper_params.class_tid = ulp_class_tid;
	mapper_params.port_id = port_id;

	ulp_ctx = bp->ulp_ctx;
	if (!ulp_ctx) {
		BNXT_DRV_DBG(ERR,
			     "ULP is not init'ed. Fail to create dflt flow.\n");
		return -EINVAL;
	}

	type = param_list->type;
	while (type != BNXT_ULP_DF_PARAM_TYPE_LAST) {
		if (ulp_def_handler_tbl[type].vfr_func) {
			rc = ulp_def_handler_tbl[type].vfr_func(ulp_ctx,
								param_list,
								&mapper_params);
			if (rc) {
				BNXT_DRV_DBG(ERR,
					     "Failed to create default flow\n");
				return rc;
			}
		}

		param_list++;
		type = param_list->type;
	}
	/* Protect flow creation */
	if (bnxt_ulp_cntxt_acquire_fdb_lock(ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Flow db lock acquire failed\n");
		return -EINVAL;
	}

	mapper_params.flow_id = flow_id;
	rc = ulp_mapper_flow_create(ulp_ctx, &mapper_params,
				    NULL);
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
	return rc;
}

int32_t
bnxt_ulp_promisc_mode_set(struct bnxt *bp, uint8_t enable)
{
	uint32_t flow_type;
	struct bnxt_ulp_df_rule_info *info;
	uint16_t port_id;
	int rc = 0;

	if (!BNXT_TRUFLOW_EN(bp) || BNXT_ETH_DEV_IS_REPRESENTOR(bp->eth_dev) ||
	    !bp->ulp_ctx)
		return rc;

	if (!BNXT_CHIP_P5(bp))
		return rc;

	port_id = bp->eth_dev->data->port_id;
	info = &bp->ulp_ctx->cfg_data->df_rule_info[port_id];

	/* create the promiscuous rule */
	if (enable && !info->promisc_flow_id) {
		flow_type = BNXT_ULP_TEMPLATE_PROMISCUOUS_ENABLE;
		rc = bnxt_create_port_app_df_rule(bp, flow_type,
						  &info->promisc_flow_id);
		BNXT_DRV_DBG(DEBUG, "enable ulp promisc mode on port %u:%u\n",
			     port_id, info->promisc_flow_id);
	} else if (!enable && info->promisc_flow_id) {
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

		flow_type = BNXT_ULP_TEMPLATE_PROMISCUOUS_DISABLE;
		if (ulp_flow_template_process(bp, param_list, flow_type,
					      port_id, 0))
			return -EIO;

		rc = ulp_default_flow_destroy(bp->eth_dev,
					      info->promisc_flow_id);
		BNXT_DRV_DBG(DEBUG, "disable ulp promisc mode on port %u:%u\n",
			     port_id, info->promisc_flow_id);
		info->promisc_flow_id = 0;
	}
	return rc;
}

/* Function to create the rte flow for miss action. */
int32_t
bnxt_ulp_grp_miss_act_set(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_action actions[],
			  uint32_t *flow_id)
{
	struct bnxt_ulp_mapper_parms mparms = { 0 };
	struct ulp_rte_parser_params params;
	struct bnxt_ulp_context *ulp_ctx;
	int ret = BNXT_TF_RC_ERROR;
	uint16_t func_id;
	uint32_t fid;
	uint32_t group_id;

	ulp_ctx = bnxt_ulp_eth_dev_ptr2_cntxt_get(dev);
	if (unlikely(!ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "ULP context is not initialized\n");
		goto flow_error;
	}

	/* Initialize the parser params */
	memset(&params, 0, sizeof(struct ulp_rte_parser_params));
	params.ulp_ctx = ulp_ctx;
	params.port_id = dev->data->port_id;
	/* classid is the group action template*/
	params.class_id = BNXT_ULP_TEMPLATE_GROUP_MISS_ACTION;

	if (unlikely(bnxt_ulp_cntxt_app_id_get(params.ulp_ctx, &params.app_id))) {
		BNXT_DRV_DBG(ERR, "failed to get the app id\n");
		goto flow_error;
	}

	/* Set the flow attributes */
	bnxt_ulp_set_dir_attributes(&params, attr);

	if (unlikely(bnxt_ulp_set_prio_attribute(&params, attr)))
		goto flow_error;

	bnxt_ulp_init_parser_cf_defaults(&params, params.port_id);

	/* Get the function id */
	if (unlikely(ulp_port_db_port_func_id_get(ulp_ctx,
						  params.port_id,
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
	ret = ulp_flow_db_fid_alloc(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT,
				   func_id, &fid);
	if (unlikely(ret)) {
		BNXT_DRV_DBG(ERR, "Unable to allocate flow table entry\n");
		goto release_lock;
	}

	/* Update the implied SVIF */
	ulp_rte_parser_implicit_match_port_process(&params);

	/* Parse the rte flow action */
	ret = bnxt_ulp_rte_parser_act_parse(actions, &params);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	/* Verify the jump target group id */
	if (ULP_BITMAP_ISSET(params.act_bitmap.bits, BNXT_ULP_ACT_BIT_JUMP)) {
		memcpy(&group_id,
		       &params.act_prop.act_details[BNXT_ULP_ACT_PROP_IDX_JUMP],
		       BNXT_ULP_ACT_PROP_SZ_JUMP);
		if (rte_cpu_to_be_32(group_id) == attr->group) {
			BNXT_DRV_DBG(ERR, "Jump action cannot jump to its own group.\n");
			ret = BNXT_TF_RC_ERROR;
			goto free_fid;
		}
	}

	mparms.flow_id = fid;
	mparms.func_id = func_id;
	mparms.port_id = params.port_id;

	/* Perform the rte flow post process */
	bnxt_ulp_rte_parser_post_process(&params);

#ifdef	RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef	RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_PARSER
	/* Dump the rte flow action */
	ulp_parser_act_info_dump(&params);
#endif
#endif

	ret = ulp_matcher_action_match(&params, &params.act_tmpl);
	if (unlikely(ret != BNXT_TF_RC_SUCCESS))
		goto free_fid;

	bnxt_ulp_init_mapper_params(&mparms, &params,
				    BNXT_ULP_FDB_TYPE_DEFAULT);
	/* Call the ulp mapper to create the flow in the hardware. */
	ret = ulp_mapper_flow_create(ulp_ctx, &mparms, NULL);
	if (unlikely(ret))
		goto free_fid;

	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);

	*flow_id = fid;
	return 0;

free_fid:
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT, fid);
release_lock:
	bnxt_ulp_cntxt_release_fdb_lock(ulp_ctx);
flow_error:
	return ret;
}
