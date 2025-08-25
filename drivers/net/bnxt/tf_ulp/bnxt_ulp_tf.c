/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>
#include <rte_spinlock.h>
#include <rte_mtr.h>
#include <rte_version.h>
#include <rte_hash_crc.h>

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_ulp_tf.h"
#include "bnxt_tf_common.h"
#include "hsi_struct_def_dpdk.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"
#include "ulp_tun.h"
#include "ulp_ha_mgr.h"
#include "bnxt_tf_pmd_shim.h"
#include "ulp_template_db_tbl.h"

/* Function to set the tfp session details from the ulp context. */
int32_t
bnxt_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type,
		       struct tf *tfp)
{
	uint32_t idx = 0;
	enum bnxt_ulp_tfo_type tfo_type = BNXT_ULP_TFO_TYPE_TF;

	if (ulp == NULL)
		return -EINVAL;

	if (ULP_MULTI_SHARED_IS_SUPPORTED(ulp)) {
		if (s_type & BNXT_ULP_SESSION_TYPE_SHARED)
			idx = 1;
		else if (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
			idx = 2;

	} else {
		if ((s_type & BNXT_ULP_SESSION_TYPE_SHARED) ||
		    (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
			idx = 1;
	}

	ulp->g_tfp[idx] = tfp;

	if (tfp == NULL) {
		uint32_t i = 0;
		while (i < BNXT_ULP_SESSION_MAX && ulp->g_tfp[i] == NULL)
			i++;
		if (i == BNXT_ULP_SESSION_MAX)
			ulp->tfo_type = BNXT_ULP_TFO_TYPE_INVALID;
	} else {
		ulp->tfo_type = tfo_type;
	}
	return 0;
}

/* Function to get the tfp session details from the ulp context. */
struct tf *
bnxt_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type)
{
	uint32_t idx = 0;

	if (ulp == NULL)
		return NULL;

	if (ulp->tfo_type != BNXT_ULP_TFO_TYPE_TF) {
		BNXT_DRV_DBG(ERR, "Wrong tf type %d != %d\n",
			     ulp->tfo_type, BNXT_ULP_TFO_TYPE_TF);
		return NULL;
	}

	if (ULP_MULTI_SHARED_IS_SUPPORTED(ulp)) {
		if (s_type & BNXT_ULP_SESSION_TYPE_SHARED)
			idx = 1;
		else if (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
			idx = 2;
	} else {
		if ((s_type & BNXT_ULP_SESSION_TYPE_SHARED) ||
		    (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
			idx = 1;
	}
	return (struct tf *)ulp->g_tfp[idx];
}

struct tf *bnxt_get_tfp_session(struct bnxt *bp, enum bnxt_session_type type)
{
	return (type >= BNXT_SESSION_TYPE_LAST) ?
		&bp->tfp[BNXT_SESSION_TYPE_REGULAR] : &bp->tfp[type];
}

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type)
{
	enum bnxt_session_type btype;

	if (type & BNXT_ULP_SESSION_TYPE_SHARED)
		btype = BNXT_SESSION_TYPE_SHARED_COMMON;
	else if (type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
		btype = BNXT_SESSION_TYPE_SHARED_WC;
	else
		btype = BNXT_SESSION_TYPE_REGULAR;

	return bnxt_get_tfp_session(bp, btype);
}

static int32_t
ulp_tf_named_resources_calc(struct bnxt_ulp_context *ulp_ctx,
			    struct bnxt_ulp_glb_resource_info *info,
			    uint32_t num,
			    enum bnxt_ulp_session_type stype,
			    struct tf_session_resources *res)
{
	uint32_t dev_id = BNXT_ULP_DEVICE_ID_LAST, res_type, i;
	enum tf_dir dir;
	uint8_t app_id;
	int32_t rc = 0;

	if (ulp_ctx == NULL || info == NULL || res == NULL || num == 0) {
		BNXT_DRV_DBG(ERR, "Invalid parms to named resources calc.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (dev_id != info[i].device_id || app_id != info[i].app_id)
			continue;
		/* check to see if the session type matches only then include */
		if ((stype || info[i].session_type) &&
		    !(info[i].session_type & stype))
			continue;

		dir = info[i].direction;
		res_type = info[i].resource_type;

		switch (info[i].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			res->ident_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			res->tbl_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			res->tcam_cnt[dir].cnt[res_type]++;
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			res->em_cnt[dir].cnt[res_type]++;
			break;
		default:
			BNXT_DRV_DBG(ERR, "Unknown resource func (0x%x)\n,",
				     info[i].resource_func);
			continue;
		}
	}

	return 0;
}

static int32_t
ulp_tf_unnamed_resources_calc(struct bnxt_ulp_context *ulp_ctx,
			      struct bnxt_ulp_resource_resv_info *info,
			      uint32_t num,
			      enum bnxt_ulp_session_type stype,
			      struct tf_session_resources *res)
{
	uint32_t dev_id, res_type, i;
	enum tf_dir dir;
	uint8_t app_id;
	int32_t rc = 0;

	if (ulp_ctx == NULL || res == NULL || info == NULL || num == 0) {
		BNXT_DRV_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (app_id != info[i].app_id || dev_id != info[i].device_id)
			continue;

		/* check to see if the session type matches only then include */
		if ((stype || info[i].session_type) &&
		    !(info[i].session_type & stype))
			continue;

		dir = info[i].direction;
		res_type = info[i].resource_type;

		switch (info[i].resource_func) {
		case BNXT_ULP_RESOURCE_FUNC_IDENTIFIER:
			res->ident_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE:
			res->tbl_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE:
			res->tcam_cnt[dir].cnt[res_type] = info[i].count;
			break;
		case BNXT_ULP_RESOURCE_FUNC_EM_TABLE:
			res->em_cnt[dir].cnt[res_type] = info[i].count;
			break;
		default:
			break;
		}
	}
	return 0;
}

static int32_t
ulp_tf_resources_get(struct bnxt_ulp_context *ulp_ctx,
		     enum bnxt_ulp_session_type stype,
		     struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed = NULL;
	uint32_t unum;
	int32_t rc = 0;

	if (ulp_ctx == NULL || res == NULL) {
		BNXT_DRV_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	/* use DEFAULT_NON_HA instead of DEFAULT resources if HA is disabled */
	if (ULP_APP_HA_IS_DYNAMIC(ulp_ctx))
		stype = ulp_ctx->cfg_data->def_session_type;

	unnamed = bnxt_ulp_resource_resv_list_get(&unum);
	if (unnamed == NULL) {
		BNXT_DRV_DBG(ERR, "Unable to get resource resv list.\n");
		return -EINVAL;
	}

	rc = ulp_tf_unnamed_resources_calc(ulp_ctx, unnamed, unum, stype, res);
	if (rc)
		BNXT_DRV_DBG(ERR, "Unable to calc resources for session.\n");

	return rc;
}

static int32_t
ulp_tf_shared_session_resources_get(struct bnxt_ulp_context *ulp_ctx,
				    enum bnxt_ulp_session_type stype,
				    struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed;
	struct bnxt_ulp_glb_resource_info *named;
	uint32_t unum = 0, nnum = 0;
	int32_t rc;

	if (ulp_ctx == NULL || res == NULL) {
		BNXT_DRV_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	/* Make sure the resources are zero before accumulating. */
	memset(res, 0, sizeof(struct tf_session_resources));

	if (bnxt_ulp_cntxt_ha_enabled(ulp_ctx) &&
	    stype == BNXT_ULP_SESSION_TYPE_SHARED)
		stype = ulp_ctx->cfg_data->hu_session_type;

	/*
	 * Shared resources are comprised of both named and unnamed resources.
	 * First get the unnamed counts, and then add the named to the result.
	 */
	/* Get the baseline counts */
	unnamed = bnxt_ulp_app_resource_resv_list_get(&unum);
	if (unum) {
		rc = ulp_tf_unnamed_resources_calc(ulp_ctx, unnamed,
						   unum, stype, res);
		if (rc) {
			BNXT_DRV_DBG(ERR,
				     "Unable to calc resources for shared session.\n");
			return -EINVAL;
		}
	}

	/* Get the named list and add the totals */
	named = bnxt_ulp_app_glb_resource_info_list_get(&nnum);
	/* No need to calc resources, none to calculate */
	if (!nnum)
		return 0;

	rc = ulp_tf_named_resources_calc(ulp_ctx, named, nnum, stype, res);
	if (rc)
		BNXT_DRV_DBG(ERR, "Unable to calc named resources\n");

	return rc;
}

/* Function to set the hot upgrade support into the context */
static int
ulp_tf_multi_shared_session_support_set(struct bnxt *bp,
					enum bnxt_ulp_device_id devid,
					uint32_t fw_hu_update)
{
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct tf_get_version_parms v_params = { 0 };
	struct tf *tfp;
	int32_t rc = 0;
	int32_t new_fw = 0;

	v_params.device_type = bnxt_ulp_cntxt_convert_dev_id(devid);
	v_params.bp = bp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_get_version(tfp, &v_params);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get tf version.\n");
		return rc;
	}

	if (v_params.major == 1 && v_params.minor == 0 &&
	    v_params.update == 1) {
		new_fw = 1;
	}
	/* if the version update is greater than 0 then set support for
	 * multiple version
	 */
	if (new_fw) {
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_MULTI_SHARED_SUPPORT;
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED;
	}
	if (!new_fw && fw_hu_update) {
		ulp_ctx->cfg_data->ulp_flags &= ~BNXT_ULP_HIGH_AVAIL_ENABLED;
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED |
			BNXT_ULP_SESSION_TYPE_SHARED_OWC;
	}

	if (!new_fw && !fw_hu_update) {
		ulp_ctx->cfg_data->hu_session_type =
			BNXT_ULP_SESSION_TYPE_SHARED |
			BNXT_ULP_SESSION_TYPE_SHARED_OWC;
	}

	return rc;
}

static int32_t
ulp_tf_cntxt_app_caps_init(struct bnxt *bp,
			   uint8_t app_id, uint32_t dev_id)
{
	struct bnxt_ulp_app_capabilities_info *info;
	uint32_t num = 0, fw = 0;
	uint16_t i;
	bool found = false;
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;

	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			     app_id, dev_id);
		return -EINVAL;
	}

	info = bnxt_ulp_app_cap_list_get(&num);
	if (!info || !num) {
		BNXT_DRV_DBG(ERR, "Failed to get app capabilities.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (info[i].app_id != app_id || info[i].device_id != dev_id)
			continue;
		found = true;
		if (info[i].flags & BNXT_ULP_APP_CAP_SHARED_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_SHARED_SESSION_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_HOT_UPGRADE_EN)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_HIGH_AVAIL_ENABLED;
		if (info[i].flags & BNXT_ULP_APP_CAP_UNICAST_ONLY)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_UNICAST_ONLY;
		if (info[i].flags & BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_TOS_PROTO_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_BC_MC_SUPPORT)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_BC_MC_SUPPORT;
		if (info[i].flags & BNXT_ULP_APP_CAP_SOCKET_DIRECT) {
			/* Enable socket direction only if MR is enabled in fw*/
			if (BNXT_MULTIROOT_EN(bp)) {
				ulp_ctx->cfg_data->ulp_flags |=
					BNXT_ULP_APP_SOCKET_DIRECT;
				BNXT_DRV_DBG(INFO,
					     "Socket Direct feature is enabled\n");
			}
		}
		if (info[i].flags & BNXT_ULP_APP_CAP_HA_DYNAMIC) {
			/* Read the environment variable to determine hot up */
			if (!bnxt_pmd_get_hot_up_config()) {
				ulp_ctx->cfg_data->ulp_flags |=
					BNXT_ULP_APP_HA_DYNAMIC;
				/* reset Hot upgrade, dynamically disabled */
				ulp_ctx->cfg_data->ulp_flags &=
					~BNXT_ULP_HIGH_AVAIL_ENABLED;
				ulp_ctx->cfg_data->def_session_type =
					BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA;
				BNXT_DRV_DBG(INFO, "Hot upgrade disabled.\n");
			}
		}
		if (info[i].flags & BNXT_ULP_APP_CAP_SRV6)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_SRV6;

		if (info[i].flags & BNXT_ULP_APP_CAP_L2_ETYPE)
			ulp_ctx->cfg_data->ulp_flags |=
				BNXT_ULP_APP_L2_ETYPE;

		bnxt_ulp_cntxt_vxlan_ip_port_set(ulp_ctx, info[i].vxlan_ip_port);
		bnxt_ulp_cntxt_vxlan_port_set(ulp_ctx, info[i].vxlan_port);
		bnxt_ulp_cntxt_ecpri_udp_port_set(ulp_ctx, info[i].ecpri_udp_port);
		bnxt_ulp_vxlan_gpe_next_proto_set(ulp_ctx, info[i].tunnel_next_proto);
		bnxt_ulp_num_key_recipes_set(ulp_ctx,
					     info[i].num_key_recipes_per_dir);

		/* set the shared session support from firmware */
		fw = info[i].upgrade_fw_update;
		if (ULP_HIGH_AVAIL_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags) &&
		    ulp_tf_multi_shared_session_support_set(bp, dev_id, fw)) {
			BNXT_DRV_DBG(ERR,
				     "Unable to get shared session support\n");
			return -EINVAL;
		}
		bnxt_ulp_cntxt_ha_reg_set(ulp_ctx, info[i].ha_reg_state,
				    info[i].ha_reg_cnt);
		ulp_ctx->cfg_data->ha_pool_id = info[i].ha_pool_id;
		bnxt_ulp_default_app_priority_set(ulp_ctx,
						  info[i].default_priority);
		bnxt_ulp_max_def_priority_set(ulp_ctx,
					      info[i].max_def_priority);
		bnxt_ulp_min_flow_priority_set(ulp_ctx,
					       info[i].min_flow_priority);
		bnxt_ulp_max_flow_priority_set(ulp_ctx,
					       info[i].max_flow_priority);
		/* Update the capability feature bits*/
		if (bnxt_ulp_cap_feat_process(info[i].feature_bits,
					      &ulp_ctx->cfg_data->feature_bits))
			return -EINVAL;

		bnxt_ulp_cntxt_ptr2_default_class_bits_set(ulp_ctx,
							   info[i].default_class_bits);
		bnxt_ulp_cntxt_ptr2_default_act_bits_set(ulp_ctx,
							 info[i].default_act_bits);
	}
	if (!found) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			     app_id, dev_id);
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_APP_DEV_UNSUPPORTED;
		return -EINVAL;
	}

	return 0;
}

static inline uint32_t
ulp_tf_session_idx_get(enum bnxt_ulp_session_type session_type) {
	if (session_type & BNXT_ULP_SESSION_TYPE_SHARED)
		return 1;
	else if (session_type & BNXT_ULP_SESSION_TYPE_SHARED_WC)
		return 2;
	return 0;
}

/* Function to set the tfp session details in session */
static int32_t
ulp_tf_session_tfp_set(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type,
		       struct tf *tfp)
{
	uint32_t idx = ulp_tf_session_idx_get(session_type);
	struct tf *local_tfp;
	int32_t rc = 0;

	if (!session->session_opened[idx]) {
		local_tfp = rte_zmalloc("bnxt_ulp_session_tfp",
					sizeof(struct tf), 0);

		if (local_tfp == NULL) {
			BNXT_DRV_DBG(DEBUG, "Failed to alloc session tfp\n");
			return -ENOMEM;
		}
		local_tfp->session = tfp->session;
		session->g_tfp[idx] = local_tfp;
		session->session_opened[idx] = 1;
	}
	return rc;
}

/* Function to get the tfp session details in session */
static struct tf_session_info *
ulp_tf_session_tfp_get(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type)
{
	uint32_t idx = ulp_tf_session_idx_get(session_type);
	struct tf *local_tfp = session->g_tfp[idx];

	if (session->session_opened[idx])
		return local_tfp->session;
	return NULL;
}

static uint32_t
ulp_tf_session_is_open(struct bnxt_ulp_session_state *session,
		       enum bnxt_ulp_session_type session_type)
{
	uint32_t idx = ulp_tf_session_idx_get(session_type);

	return session->session_opened[idx];
}

/* Function to reset the tfp session details in session */
static void
ulp_tf_session_tfp_reset(struct bnxt_ulp_session_state *session,
			 enum bnxt_ulp_session_type session_type)
{
	uint32_t idx = ulp_tf_session_idx_get(session_type);

	if (session->session_opened[idx]) {
		session->session_opened[idx] = 0;
		rte_free(session->g_tfp[idx]);
		session->g_tfp[idx] = NULL;
	}
}

static void
ulp_tf_ctx_shared_session_close(struct bnxt *bp,
				enum bnxt_ulp_session_type session_type,
				struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;
	int32_t rc;

	tfp = bnxt_ulp_cntxt_tfp_get(bp->ulp_ctx, session_type);
	if (!tfp) {
		/*
		 * Log it under debug since this is likely a case of the
		 * shared session not being created.  For example, a failed
		 * initialization.
		 */
		BNXT_DRV_DBG(DEBUG, "Failed to get shared tfp on close.\n");
		return;
	}
	rc = tf_close_session(tfp);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed to close the shared session rc=%d.\n",
			     rc);
	(void)bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, session_type, NULL);
	ulp_tf_session_tfp_reset(session, session_type);
}

static int32_t
ulp_tf_ctx_shared_session_open(struct bnxt *bp,
			       enum bnxt_ulp_session_type session_type,
			       struct bnxt_ulp_session_state *session)
{
	struct rte_eth_dev *ethdev = bp->eth_dev;
	struct tf_session_resources *resources;
	struct tf_open_session_parms parms;
	size_t nb;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int32_t	rc = 0;
	uint8_t app_id;
	struct tf *tfp;
	uint8_t pool_id;

	memset(&parms, 0, sizeof(parms));
	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  parms.ctrl_chan_name);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Invalid port %d, rc = %d\n",
			     ethdev->data->port_id, rc);
		return rc;
	}

	/* On multi-host system, adjust ctrl_chan_name to avoid confliction */
	if (BNXT_MH(bp)) {
		rc = ulp_ctx_mh_get_session_name(bp, &parms);
		if (rc)
			return rc;
	}

	resources = &parms.resources;

	/*
	 * Need to account for size of ctrl_chan_name and 1 extra for Null
	 * terminator
	 */
	nb = sizeof(parms.ctrl_chan_name) - strlen(parms.ctrl_chan_name) - 1;

	/*
	 * Build the ctrl_chan_name with shared token.
	 * When HA is enabled, the WC TCAM needs extra management by the core,
	 * so add the wc_tcam string to the control channel.
	 */
	pool_id = bp->ulp_ctx->cfg_data->ha_pool_id;
	if (!bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		if (bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx))
			strncat(parms.ctrl_chan_name, "-tf_shared-wc_tcam", nb);
		else
			strncat(parms.ctrl_chan_name, "-tf_shared", nb);
	} else if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		if (session_type == BNXT_ULP_SESSION_TYPE_SHARED) {
			strncat(parms.ctrl_chan_name, "-tf_shared", nb);
		} else if (session_type == BNXT_ULP_SESSION_TYPE_SHARED_WC) {
			char session_pool_name[64];

			sprintf(session_pool_name, "-tf_shared-pool%d",
				pool_id);

			if (nb >= strlen(session_pool_name)) {
				strncat(parms.ctrl_chan_name, session_pool_name, nb);
			} else {
				BNXT_DRV_DBG(ERR, "No space left for session_name\n");
				return -EINVAL;
			}
		}
	}

	rc = ulp_tf_shared_session_resources_get(bp->ulp_ctx, session_type,
						 resources);
	if (rc)
		return rc;

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	tfp = bnxt_ulp_bp_tfp_get(bp, session_type);
	parms.device_type = bnxt_ulp_cntxt_convert_dev_id(ulp_dev_id);
	parms.bp = bp;

	/*
	 * Open the session here, but the collect the resources during the
	 * mapper initialization.
	 */
	rc = tf_open_session(tfp, &parms);
	if (rc)
		return rc;

	if (parms.shared_session_creator)
		BNXT_DRV_DBG(DEBUG, "Shared session creator.\n");
	else
		BNXT_DRV_DBG(DEBUG, "Shared session attached.\n");

	/* Save the shared session in global data */
	rc = ulp_tf_session_tfp_set(session, session_type, tfp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add shared tfp to session\n");
		return rc;
	}

	rc = bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, session_type, tfp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add shared tfp to ulp (%d)\n", rc);
		return rc;
	}

	return rc;
}

static int32_t
ulp_tf_ctx_shared_session_attach(struct bnxt *bp,
				 struct bnxt_ulp_session_state *ses)
{
	enum bnxt_ulp_session_type type;
	struct tf *tfp;
	int32_t rc = 0;

	/* Simply return success if shared session not enabled */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		type = BNXT_ULP_SESSION_TYPE_SHARED;
		tfp = bnxt_ulp_bp_tfp_get(bp, type);
		tfp->session = ulp_tf_session_tfp_get(ses, type);
		rc = ulp_tf_ctx_shared_session_open(bp, type, ses);
	}

	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		type = BNXT_ULP_SESSION_TYPE_SHARED_WC;
		tfp = bnxt_ulp_bp_tfp_get(bp, type);
		tfp->session = ulp_tf_session_tfp_get(ses, type);
		rc = ulp_tf_ctx_shared_session_open(bp, type, ses);
	}

	if (!rc)
		bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, true);

	return rc;
}

static void
ulp_tf_ctx_shared_session_detach(struct bnxt *bp)
{
	struct tf *tfp;

	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_SHARED);
		if (tfp->session) {
			tf_close_session(tfp);
			tfp->session = NULL;
		}
	}
	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_SHARED_WC);
		if (tfp->session) {
			tf_close_session(tfp);
			tfp->session = NULL;
		}
	}
	bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, false);
}

/*
 * Initialize an ULP session.
 * An ULP session will contain all the resources needed to support rte flow
 * offloads. A session is initialized as part of rte_eth_device start.
 * A single vswitch instance can have multiple uplinks which means
 * rte_eth_device start will be called for each of these devices.
 * ULP session manager will make sure that a single ULP session is only
 * initialized once. Apart from this, it also initializes MARK database,
 * EEM table & flow database. ULP session manager also manages a list of
 * all opened ULP sessions.
 */
static int32_t
ulp_tf_ctx_session_open(struct bnxt *bp,
			struct bnxt_ulp_session_state *session)
{
	struct rte_eth_dev		*ethdev = bp->eth_dev;
	int32_t				rc = 0;
	struct tf_open_session_parms	params;
	struct tf_session_resources	*resources;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	uint8_t app_id;
	struct tf *tfp;

	memset(&params, 0, sizeof(params));

	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  params.ctrl_chan_name);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Invalid port %d, rc = %d\n",
			     ethdev->data->port_id, rc);
		return rc;
	}

	/* On multi-host system, adjust ctrl_chan_name to avoid confliction */
	if (BNXT_MH(bp)) {
		rc = ulp_ctx_mh_get_session_name(bp, &params);
		if (rc)
			return rc;
	}

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	params.device_type = bnxt_ulp_cntxt_convert_dev_id(ulp_dev_id);
	resources = &params.resources;
	rc = ulp_tf_resources_get(bp->ulp_ctx,
				  BNXT_ULP_SESSION_TYPE_DEFAULT,
				  resources);
	if (rc)
		return rc;

	params.bp = bp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_open_session(tfp, &params);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to open TF session - %s, rc = %d\n",
			     params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	rc = ulp_tf_session_tfp_set(session, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set TF session - %s, rc = %d\n",
			     params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	return rc;
}

/*
 * Close the ULP session.
 * It takes the ulp context pointer.
 */
static void
ulp_tf_ctx_session_close(struct bnxt *bp,
			 struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;

	/* close the session in the hardware */
	if (ulp_tf_session_is_open(session, BNXT_ULP_SESSION_TYPE_DEFAULT)) {
		tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
		tf_close_session(tfp);
	}
	ulp_tf_session_tfp_reset(session, BNXT_ULP_SESSION_TYPE_DEFAULT);
}

static void
ulp_tf_init_tbl_scope_parms(struct bnxt *bp,
			    struct tf_alloc_tbl_scope_parms *params)
{
	struct bnxt_ulp_device_params	*dparms;
	uint32_t dev_id;
	int rc;

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id);
	if (rc)
		/* TBD: For now, just use default. */
		dparms = 0;
	else
		dparms = bnxt_ulp_device_params_get(dev_id);

	/*
	 * Set the flush timer for EEM entries. The value is in 100ms intervals,
	 * so 100 is 10s.
	 */
	params->hw_flow_cache_flush_timer = 100;

	if (!dparms) {
		params->rx_max_key_sz_in_bits = BNXT_ULP_DFLT_RX_MAX_KEY;
		params->rx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY;
		params->rx_mem_size_in_mb = BNXT_ULP_DFLT_RX_MEM;
		params->rx_num_flows_in_k = BNXT_ULP_RX_NUM_FLOWS;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k = BNXT_ULP_TX_NUM_FLOWS;
	} else {
		params->rx_max_key_sz_in_bits = BNXT_ULP_DFLT_RX_MAX_KEY;
		params->rx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY;
		params->rx_mem_size_in_mb = BNXT_ULP_DFLT_RX_MEM;
		params->rx_num_flows_in_k =
			dparms->ext_flow_db_num_entries / 1024;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k =
			dparms->ext_flow_db_num_entries / 1024;
	}
	BNXT_DRV_DBG(INFO, "Table Scope initialized with %uK flows.\n",
		     params->rx_num_flows_in_k);
}

/* Initialize Extended Exact Match host memory. */
static int32_t
ulp_tf_eem_tbl_scope_init(struct bnxt *bp)
{
	struct tf_alloc_tbl_scope_parms params = {0};
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	uint32_t dev_id;
	struct tf *tfp;
	int rc;

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		BNXT_DRV_DBG(ERR, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(ERR, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(bp->ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		BNXT_DRV_DBG(INFO, "Table Scope alloc is not required\n");
		return 0;
	}

	ulp_tf_init_tbl_scope_parms(bp, &params);
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_alloc_tbl_scope(tfp, &params);
	if (rc) {
		BNXT_DRV_DBG(ERR,
			     "Unable to allocate eem table scope rc = %d\n",
			     rc);
		return rc;
	}

#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
	BNXT_DRV_DBG(DEBUG, "TableScope=0x%0x %d\n",
		     params.tbl_scope_id,
		     params.tbl_scope_id);
#endif

	rc = bnxt_ulp_cntxt_tbl_scope_id_set(bp->ulp_ctx, params.tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set table scope id\n");
		return rc;
	}

	return 0;
}

/* Free Extended Exact Match host memory */
static int32_t
ulp_tf_eem_tbl_scope_deinit(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
{
	struct tf_free_tbl_scope_parms	params = {0};
	struct tf			*tfp;
	int32_t				rc = 0;
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	uint32_t dev_id;

	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfp) {
		BNXT_DRV_DBG(ERR, "Failed to get the truflow pointer\n");
		return -EINVAL;
	}

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		BNXT_DRV_DBG(ERR, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(ERR, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		BNXT_DRV_DBG(INFO, "Table Scope free is not required\n");
		return 0;
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp_ctx, &params.tbl_scope_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get the table scope id\n");
		return -EINVAL;
	}

	rc = tf_free_tbl_scope(tfp, &params);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to free table scope\n");
		return -EINVAL;
	}
	return rc;
}

/* The function to free and deinit the ulp context data. */
static int32_t
ulp_tf_ctx_deinit(struct bnxt *bp,
		  struct bnxt_ulp_session_state *session)
{
	/* close the tf session */
	ulp_tf_ctx_session_close(bp, session);

	/* The shared session must be closed last. */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx))
		ulp_tf_ctx_shared_session_close(bp, BNXT_ULP_SESSION_TYPE_SHARED,
						session);

	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx))
		ulp_tf_ctx_shared_session_close(bp,
						BNXT_ULP_SESSION_TYPE_SHARED_WC,
						session);

	bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, false);

	/* Free the contents */
	if (session->cfg_data) {
		rte_free(session->cfg_data);
		bp->ulp_ctx->cfg_data = NULL;
		session->cfg_data = NULL;
	}
	return 0;
}

/* The function to allocate and initialize the ulp context data. */
static int32_t
ulp_tf_ctx_init(struct bnxt *bp,
		struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_data	*ulp_data;
	int32_t			rc = 0;
	enum bnxt_ulp_device_id devid;
	enum bnxt_ulp_session_type stype;
	struct tf *tfp;

	/* Initialize the context entries list */
	bnxt_ulp_cntxt_list_init();

	/* Allocate memory to hold ulp context data. */
	ulp_data = rte_zmalloc("bnxt_ulp_data",
			       sizeof(struct bnxt_ulp_data), 0);
	if (!ulp_data) {
		BNXT_DRV_DBG(ERR, "Failed to allocate memory for ulp data\n");
		return -ENOMEM;
	}

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = ulp_data;
	session->cfg_data = ulp_data;
	ulp_data->ref_cnt++;
	ulp_data->ulp_flags |= BNXT_ULP_VF_REP_ENABLED;

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add the context list entry\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_devid_get(bp, &devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to determine device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_dev_id_set(bp->ulp_ctx, devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_app_id_set(bp->ulp_ctx, bp->app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set app_id for ULP init.\n");
		goto error_deinit;
	}
	BNXT_DRV_DBG(DEBUG, "Ulp initialized with app id %d\n", bp->app_id);

	rc = ulp_tf_cntxt_app_caps_init(bp, bp->app_id, devid);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to set caps for app(%x)/dev(%x)\n",
			     bp->app_id, devid);
		goto error_deinit;
	}

	if (BNXT_TESTPMD_EN(bp)) {
		ulp_data->ulp_flags &= ~BNXT_ULP_VF_REP_ENABLED;
		BNXT_DRV_DBG(ERR, "Enabled Testpmd forward mode\n");
	}

	/*
	 * Shared session must be created before first regular session but after
	 * the ulp_ctx is valid.
	 */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		rc = ulp_tf_ctx_shared_session_open(bp,
						    BNXT_ULP_SESSION_TYPE_SHARED,
						    session);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Unable to open shared session (%d)\n",
				     rc);
			goto error_deinit;
		}
	}

	/* Multiple session support */
	if (bnxt_ulp_cntxt_multi_shared_session_enabled(bp->ulp_ctx)) {
		stype = BNXT_ULP_SESSION_TYPE_SHARED_WC;
		rc = ulp_tf_ctx_shared_session_open(bp, stype, session);
		if (rc) {
			BNXT_DRV_DBG(ERR,
				     "Unable to open shared wc session (%d)\n",
				     rc);
			goto error_deinit;
		}
	}
	bnxt_ulp_cntxt_num_shared_clients_set(bp->ulp_ctx, true);

	/* Open the ulp session. */
	rc = ulp_tf_ctx_session_open(bp, session);
	if (rc)
		goto error_deinit;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);
	return rc;

error_deinit:
	session->session_opened[BNXT_ULP_SESSION_TYPE_DEFAULT] = 1;
	(void)ulp_tf_ctx_deinit(bp, session);
	return rc;
}

/* The function to initialize ulp dparms with devargs */
static int32_t
ulp_tf_dparms_init(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_device_params *dparms;
	uint32_t dev_id = BNXT_ULP_DEVICE_ID_LAST;

	if (!bp->max_num_kflows) {
		/* Defaults to Internal */
		bnxt_ulp_cntxt_mem_type_set(ulp_ctx,
					    BNXT_ULP_FLOW_MEM_TYPE_INT);
		return 0;
	}

	/* The max_num_kflows were set, so move to external */
	if (bnxt_ulp_cntxt_mem_type_set(ulp_ctx, BNXT_ULP_FLOW_MEM_TYPE_EXT))
		return -EINVAL;

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id)) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device parms\n");
		return -EINVAL;
	}

	/* num_flows = max_num_kflows * 1024 */
	dparms->ext_flow_db_num_entries = bp->max_num_kflows * 1024;
	/* GFID =  2 * num_flows */
	dparms->mark_db_gfid_entries = dparms->ext_flow_db_num_entries * 2;
	BNXT_DRV_DBG(DEBUG, "Set the number of flows = %" PRIu64 "\n",
		     dparms->ext_flow_db_num_entries);

	return 0;
}

static int32_t
ulp_tf_ctx_attach(struct bnxt *bp,
	       struct bnxt_ulp_session_state *session)
{
	int32_t rc = 0;
	uint32_t flags, dev_id = BNXT_ULP_DEVICE_ID_LAST;
	struct tf *tfp;
	uint8_t app_id;

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = session->cfg_data;
	bp->ulp_ctx->cfg_data->ref_cnt++;

	/* update the session details in bnxt tfp */
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	tfp->session = ulp_tf_session_tfp_get(session,
					      BNXT_ULP_SESSION_TYPE_DEFAULT);

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to add the context list entry\n");
		return -EINVAL;
	}

	/*
	 * The supported flag will be set during the init. Use it now to
	 * know if we should go through the attach.
	 */
	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable do get the dev_id.\n");
		return -EINVAL;
	}

	flags = bp->ulp_ctx->cfg_data->ulp_flags;
	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(flags)) {
		BNXT_DRV_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			     app_id, dev_id);
		return -EINVAL;
	}

	/* Create a TF Client */
	rc = ulp_tf_ctx_session_open(bp, session);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to open ctxt session, rc:%d\n", rc);
		tfp->session = NULL;
		return rc;
	}
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT, tfp);

	/*
	 * Attach to the shared session, must be called after the
	 * ulp_ctx_attach in order to ensure that ulp data is available
	 * for attaching.
	 */
	rc = ulp_tf_ctx_shared_session_attach(bp, session);
	if (rc)
		BNXT_DRV_DBG(ERR, "Failed attach to shared session (%d)", rc);

	return rc;
}

static void
ulp_tf_ctx_detach(struct bnxt *bp,
		  struct bnxt_ulp_session_state *session __rte_unused)
{
	struct tf *tfp;

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (tfp->session) {
		tf_close_session(tfp);
		tfp->session = NULL;
	}

	/* always detach/close shared after the session. */
	ulp_tf_ctx_shared_session_detach(bp);
}

/*
 * Internal api to enable NAT feature.
 * Set set_flag to 1 to set the value or zero to reset the value.
 * returns 0 on success.
 */
static int32_t
ulp_tf_global_cfg_update(struct bnxt *bp,
			 enum tf_dir dir,
			 enum tf_global_config_type type,
			 uint32_t offset,
			 uint32_t value,
			 uint32_t set_flag)
{
	uint32_t global_cfg = 0;
	int rc;
	struct tf_global_cfg_parms parms = { 0 };
	struct tf *tfp;

	/* Initialize the params */
	parms.dir = dir,
	parms.type = type,
	parms.offset = offset,
	parms.config = (uint8_t *)&global_cfg,
	parms.config_sz_in_bytes = sizeof(global_cfg);

	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_get_global_cfg(tfp, &parms);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to get global cfg 0x%x rc:%d\n",
			     type, rc);
		return rc;
	}

	if (set_flag)
		global_cfg |= value;
	else
		global_cfg &= ~value;

	/* SET the register RE_CFA_REG_ACT_TECT */
	rc = tf_set_global_cfg(tfp, &parms);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set global cfg 0x%x rc:%d\n",
			     type, rc);
		return rc;
	}
	return rc;
}

/**
 * When a port is initialized by dpdk. This functions is called
 * to enable the meter and initializes the meter global configurations.
 */
#define BNXT_THOR_FMTCR_NUM_MET_MET_1K (0x7UL << 20)
#define BNXT_THOR_FMTCR_CNTRS_ENABLE (0x1UL << 25)
#define BNXT_THOR_FMTCR_INTERVAL_1K (1024)
static int32_t
ulp_tf_flow_mtr_init(struct bnxt *bp)
{
	int rc = 0;

	/*
	 * Enable metering. Set the meter global configuration register.
	 * Set number of meter to 1K. Disable the drop counter for now.
	 */
	rc = ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_METER_CFG,
				      0,
				      BNXT_THOR_FMTCR_NUM_MET_MET_1K,
				      1);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set rx meter configuration\n");
		goto jump_to_error;
	}

	rc = ulp_tf_global_cfg_update(bp, TF_DIR_TX, TF_METER_CFG,
				      0,
				      BNXT_THOR_FMTCR_NUM_MET_MET_1K,
				      1);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set tx meter configuration\n");
		goto jump_to_error;
	}

	/*
	 * Set meter refresh rate to 1024 clock cycle. This value works for
	 * most bit rates especially for high rates.
	 */
	rc = ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_METER_INTERVAL_CFG,
				      0,
				      BNXT_THOR_FMTCR_INTERVAL_1K,
				      1);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set rx meter interval\n");
		goto jump_to_error;
	}

	rc = bnxt_flow_mtr_init(bp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to config meter\n");
		goto jump_to_error;
	}

	return rc;

jump_to_error:
	return rc;
}

/*
 * When a port is deinit'ed by dpdk. This function is called
 * and this function clears the ULP context and rest of the
 * infrastructure associated with it.
 */
static void
ulp_tf_deinit(struct bnxt *bp,
	      struct bnxt_ulp_session_state *session)
{
	bool ha_enabled;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	ha_enabled = bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx);
	if (ha_enabled &&
	    ulp_tf_session_is_open(session, BNXT_ULP_SESSION_TYPE_DEFAULT)) {
		int32_t rc = ulp_ha_mgr_close(bp->ulp_ctx);
		if (rc)
			BNXT_DRV_DBG(ERR, "Failed to close HA (%d)\n", rc);
	}

	/* cleanup the eem table scope */
	ulp_tf_eem_tbl_scope_deinit(bp, bp->ulp_ctx);

	/* cleanup the flow database */
	ulp_flow_db_deinit(bp->ulp_ctx);

	/* Delete the Mark database */
	ulp_mark_db_deinit(bp->ulp_ctx);

	/* cleanup the ulp mapper */
	ulp_mapper_deinit(bp->ulp_ctx);

	/* cleanup the ulp matcher */
	ulp_matcher_deinit(bp->ulp_ctx);

	/* Delete the Flow Counter Manager */
	ulp_fc_mgr_deinit(bp->ulp_ctx);

	/* Delete the Port database */
	ulp_port_db_deinit(bp->ulp_ctx);

	/* Disable NAT feature */
	(void)ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
				       TF_TUNNEL_ENCAP_NAT,
				       BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	(void)ulp_tf_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
				       TF_TUNNEL_ENCAP_NAT,
				       BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	/* free the flow db lock */
	pthread_mutex_destroy(&bp->ulp_ctx->cfg_data->flow_db_lock);

	if (ha_enabled)
		ulp_ha_mgr_deinit(bp->ulp_ctx);

	/* Delete the ulp context and tf session and free the ulp context */
	ulp_tf_ctx_deinit(bp, session);
	BNXT_DRV_DBG(DEBUG, "ulp ctx has been deinitialized\n");
}

/*
 * When a port is initialized by dpdk. This functions is called
 * and this function initializes the ULP context and rest of the
 * infrastructure associated with it.
 */
static int32_t
ulp_tf_init(struct bnxt *bp,
	    struct bnxt_ulp_session_state *session)
{
	int rc;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;

	/* Select 64bit SSE4.2 intrinsic if available */
	rte_hash_crc_set_alg(CRC32_SSE42_x64);

	/* Allocate and Initialize the ulp context. */
	rc = ulp_tf_ctx_init(bp, session);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	pthread_mutex_init(&bp->ulp_ctx->cfg_data->flow_db_lock, NULL);

	/* Initialize ulp dparms with values devargs passed */
	rc = ulp_tf_dparms_init(bp, bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize the dparms\n");
		goto jump_to_error;
	}

	/* create the port database */
	rc = ulp_port_db_init(bp->ulp_ctx, bp->port_cnt);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the port database\n");
		goto jump_to_error;
	}

	/* Create the Mark database. */
	rc = ulp_mark_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the mark database\n");
		goto jump_to_error;
	}

	/* Create the flow database. */
	rc = ulp_flow_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the flow database\n");
		goto jump_to_error;
	}

	/* Create the eem table scope. */
	rc = ulp_tf_eem_tbl_scope_init(bp);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to create the eem scope table\n");
		goto jump_to_error;
	}

	rc = ulp_matcher_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp matcher\n");
		goto jump_to_error;
	}

	rc = ulp_mapper_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp mapper\n");
		goto jump_to_error;
	}

	rc = ulp_fc_mgr_init(bp->ulp_ctx);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize ulp flow counter mgr\n");
		goto jump_to_error;
	}

	/*
	 * Enable NAT feature. Set the global configuration register
	 * Tunnel encap to enable NAT with the reuse of existing inner
	 * L2 header smac and dmac
	 */
	rc = ulp_tf_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
				      TF_TUNNEL_ENCAP_NAT,
				      BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set rx global configuration\n");
		goto jump_to_error;
	}

	rc = ulp_tf_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
				      TF_TUNNEL_ENCAP_NAT,
				      BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to set tx global configuration\n");
		goto jump_to_error;
	}

	if (bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx)) {
		rc = ulp_ha_mgr_init(bp->ulp_ctx);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to initialize HA %d\n", rc);
			goto jump_to_error;
		}
		rc = ulp_ha_mgr_open(bp->ulp_ctx);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to Process HA Open %d\n", rc);
			goto jump_to_error;
		}
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	if (ulp_dev_id == BNXT_ULP_DEVICE_ID_THOR) {
		rc = ulp_tf_flow_mtr_init(bp);
		if (rc) {
			BNXT_DRV_DBG(ERR, "Failed to config meter\n");
			goto jump_to_error;
		}
	}

	BNXT_DRV_DBG(DEBUG, "ulp ctx has been initialized\n");
	return rc;

jump_to_error:
	bp->ulp_ctx->ops->ulp_deinit(bp, session);
	return rc;
}

/**
 * Get meter capabilities.
 */
#define MAX_FLOW_PER_METER 1024
#define MAX_METER_RATE_100GBPS ((1ULL << 30) * 100 / 8)
static int
ulp_tf_mtr_cap_get(struct bnxt *bp,
		   struct rte_mtr_capabilities *cap)
{
	struct tf_get_session_info_parms iparms;
	struct tf *tfp;
	int32_t rc = 0;

	/* Get number of meter reserved for this session */
	memset(&iparms, 0, sizeof(iparms));
	tfp = bnxt_ulp_bp_tfp_get(bp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	rc = tf_get_session_info(tfp, &iparms);
	if (rc != 0) {
		BNXT_DRV_DBG(ERR, "Failed to get session resource info\n");
		return rc;
	}

	memset(cap, 0, sizeof(struct rte_mtr_capabilities));

	cap->n_max = iparms.session_info.tbl[TF_DIR_RX].info[TF_TBL_TYPE_METER_INST].stride;
	if (!cap->n_max) {
		BNXT_DRV_DBG(ERR, "Meter is not supported\n");
		return -EINVAL;
	}

	cap->srtcm_rfc2697_byte_mode_supported = 1;
	cap->n_shared_max = cap->n_max;
	/* No meter is identical */
	cap->identical = 1;
	cap->shared_identical = 1;
	cap->shared_n_flows_per_mtr_max = MAX_FLOW_PER_METER;
	cap->chaining_n_mtrs_per_flow_max = 1; /* Chaining is not supported. */
	cap->meter_srtcm_rfc2697_n_max = cap->n_max;
	cap->meter_rate_max = MAX_METER_RATE_100GBPS;
	/* No stats supported now */
	cap->stats_mask = 0;

	return 0;
}

const struct bnxt_ulp_core_ops bnxt_ulp_tf_core_ops = {
	.ulp_ctx_attach = ulp_tf_ctx_attach,
	.ulp_ctx_detach = ulp_tf_ctx_detach,
	.ulp_deinit =  ulp_tf_deinit,
	.ulp_init =  ulp_tf_init,
	.ulp_vfr_session_fid_add = NULL,
	.ulp_vfr_session_fid_rem = NULL,
	.ulp_mtr_cap_get = ulp_tf_mtr_cap_get
};
