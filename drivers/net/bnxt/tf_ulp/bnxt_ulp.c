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

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_port_db.h"
#include "ulp_tun.h"
#include "ulp_ha_mgr.h"
#include "bnxt_tf_pmd_shim.h"

/* Linked list of all TF sessions. */
STAILQ_HEAD(, bnxt_ulp_session_state) bnxt_ulp_session_list =
			STAILQ_HEAD_INITIALIZER(bnxt_ulp_session_list);

/* Mutex to synchronize bnxt_ulp_session_list operations. */
static pthread_mutex_t bnxt_ulp_global_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Spin lock to protect context global list */
uint32_t bnxt_ulp_ctxt_lock_created;
rte_spinlock_t bnxt_ulp_ctxt_lock;
TAILQ_HEAD(cntx_list_entry_list, ulp_context_list_entry);
static struct cntx_list_entry_list ulp_cntx_list =
	TAILQ_HEAD_INITIALIZER(ulp_cntx_list);

/* Static function declarations */
static int32_t bnxt_ulp_cntxt_list_init(void);
static int32_t bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx);
static void bnxt_ulp_cntxt_list_del(struct bnxt_ulp_context *ulp_ctx);

/*
 * Allow the deletion of context only for the bnxt device that
 * created the session.
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return false;

	if (!ulp_ctx->cfg_data->ref_cnt) {
		BNXT_TF_DBG(DEBUG, "ulp ctx shall initiate deinit\n");
		return true;
	}

	return false;
}

static int32_t
bnxt_ulp_devid_get(struct bnxt *bp,
		   enum bnxt_ulp_device_id  *ulp_dev_id)
{
	if (BNXT_CHIP_P5(bp)) {
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_THOR;
		return 0;
	}

	if (BNXT_STINGRAY(bp))
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_STINGRAY;
	else
		/* Assuming Whitney */
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_WH_PLUS;

	return 0;
}

struct bnxt_ulp_app_capabilities_info *
bnxt_ulp_app_cap_list_get(uint32_t *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_APP_CAP_TBL_MAX_SZ;
	return ulp_app_cap_info_list;
}

static struct bnxt_ulp_resource_resv_info *
bnxt_ulp_app_resource_resv_list_get(uint32_t *num_entries)
{
	if (num_entries == NULL)
		return NULL;
	*num_entries = BNXT_ULP_APP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_app_resource_resv_list;
}

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_resource_resv_list_get(uint32_t *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_resource_resv_list;
}

struct bnxt_ulp_glb_resource_info *
bnxt_ulp_app_glb_resource_info_list_get(uint32_t *num_entries)
{
	if (!num_entries)
		return NULL;
	*num_entries = BNXT_ULP_APP_GLB_RESOURCE_TBL_MAX_SZ;
	return ulp_app_glb_resource_tbl;
}

static int32_t
bnxt_ulp_named_resources_calc(struct bnxt_ulp_context *ulp_ctx,
			      struct bnxt_ulp_glb_resource_info *info,
			      uint32_t num,
			      struct tf_session_resources *res)
{
	uint32_t dev_id = BNXT_ULP_DEVICE_ID_LAST, res_type, i;
	enum tf_dir dir;
	uint8_t app_id;
	int32_t rc = 0;

	if (ulp_ctx == NULL || info == NULL || res == NULL || num == 0) {
		BNXT_TF_DBG(ERR, "Invalid parms to named resources calc.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (dev_id != info[i].device_id || app_id != info[i].app_id)
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
			BNXT_TF_DBG(ERR, "Unknown resource func (0x%x)\n,",
				    info[i].resource_func);
			continue;
		}
	}

	return 0;
}

static int32_t
bnxt_ulp_unnamed_resources_calc(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_resource_resv_info *info,
				uint32_t num,
				struct tf_session_resources *res)
{
	uint32_t dev_id, res_type, i;
	enum tf_dir dir;
	uint8_t app_id;
	int32_t rc = 0;

	if (ulp_ctx == NULL || res == NULL || info == NULL || num == 0) {
		BNXT_TF_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_app_id_get(ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the dev id from ulp.\n");
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (app_id != info[i].app_id || dev_id != info[i].device_id)
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
bnxt_ulp_tf_resources_get(struct bnxt_ulp_context *ulp_ctx,
			  struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed = NULL;
	uint32_t unum;
	int32_t rc = 0;

	if (ulp_ctx == NULL || res == NULL) {
		BNXT_TF_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	unnamed = bnxt_ulp_resource_resv_list_get(&unum);
	if (unnamed == NULL) {
		BNXT_TF_DBG(ERR, "Unable to get resource resv list.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_unnamed_resources_calc(ulp_ctx, unnamed, unum, res);
	if (rc)
		BNXT_TF_DBG(ERR, "Unable to calc resources for session.\n");

	return rc;
}

static int32_t
bnxt_ulp_tf_shared_session_resources_get(struct bnxt_ulp_context *ulp_ctx,
					 struct tf_session_resources *res)
{
	struct bnxt_ulp_resource_resv_info *unnamed;
	struct bnxt_ulp_glb_resource_info *named;
	uint32_t unum, nnum;
	int32_t rc;

	if (ulp_ctx == NULL || res == NULL) {
		BNXT_TF_DBG(ERR, "Invalid arguments to get resources.\n");
		return -EINVAL;
	}

	/* Make sure the resources are zero before accumulating. */
	memset(res, 0, sizeof(struct tf_session_resources));

	/*
	 * Shared resources are comprised of both named and unnamed resources.
	 * First get the unnamed counts, and then add the named to the result.
	 */
	/* Get the baseline counts */
	unnamed = bnxt_ulp_app_resource_resv_list_get(&unum);
	if (unnamed == NULL) {
		BNXT_TF_DBG(ERR, "Unable to get shared resource resv list.\n");
		return -EINVAL;
	}
	rc = bnxt_ulp_unnamed_resources_calc(ulp_ctx, unnamed, unum, res);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to calc resources for shared session.\n");
		return -EINVAL;
	}

	/* Get the named list and add the totals */
	named = bnxt_ulp_app_glb_resource_info_list_get(&nnum);
	if (named == NULL) {
		BNXT_TF_DBG(ERR, "Unable to get app global resource list\n");
		return -EINVAL;
	}
	rc = bnxt_ulp_named_resources_calc(ulp_ctx, named, nnum, res);
	if (rc)
		BNXT_TF_DBG(ERR, "Unable to calc named resources\n");

	return rc;
}

int32_t
bnxt_ulp_cntxt_app_caps_init(struct bnxt *bp,
			     uint8_t app_id, uint32_t dev_id)
{
	struct bnxt_ulp_app_capabilities_info *info;
	uint32_t num = 0;
	uint16_t i;
	bool found = false;
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;

	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(ulp_ctx->cfg_data->ulp_flags)) {
		BNXT_TF_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		return -EINVAL;
	}

	info = bnxt_ulp_app_cap_list_get(&num);
	if (!info || !num) {
		BNXT_TF_DBG(ERR, "Failed to get app capabilities.\n");
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
		if (info[i].flags & BNXT_ULP_APP_CAP_SOCKET_DIRECT) {
			/* Enable socket direction only if MR is enabled in fw*/
			if (BNXT_MULTIROOT_EN(bp)) {
				ulp_ctx->cfg_data->ulp_flags |=
					BNXT_ULP_APP_SOCKET_DIRECT;
				BNXT_TF_DBG(DEBUG,
					    "Socket Direct feature is enabled");
			}
		}
	}
	if (!found) {
		BNXT_TF_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_APP_DEV_UNSUPPORTED;
		return -EINVAL;
	}

	return 0;
}

static void
ulp_ctx_shared_session_close(struct bnxt *bp,
			     struct bnxt_ulp_session_state *session)
{
	struct tf *tfp;
	int32_t rc;

	if (!bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx))
		return;

	tfp = bnxt_ulp_cntxt_shared_tfp_get(bp->ulp_ctx);
	if (!tfp) {
		/*
		 * Log it under debug since this is likely a case of the
		 * shared session not being created.  For example, a failed
		 * initialization.
		 */
		BNXT_TF_DBG(DEBUG, "Failed to get shared tfp on close.\n");
		return;
	}
	rc = tf_close_session(tfp);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to close the shared session rc=%d.\n",
			    rc);
	(void)bnxt_ulp_cntxt_shared_tfp_set(bp->ulp_ctx, NULL);

	session->g_shared_tfp.session = NULL;
}

static int32_t
ulp_ctx_shared_session_open(struct bnxt *bp,
			    struct bnxt_ulp_session_state *session)
{
	struct rte_eth_dev *ethdev = bp->eth_dev;
	struct tf_session_resources *resources;
	struct tf_open_session_parms parms;
	size_t copy_nbytes;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	int32_t	rc = 0;
	uint8_t app_id;

	/* only perform this if shared session is enabled. */
	if (!bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx))
		return 0;

	memset(&parms, 0, sizeof(parms));

	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  parms.ctrl_chan_name);
	if (rc) {
		BNXT_TF_DBG(ERR, "Invalid port %d, rc = %d\n",
			    ethdev->data->port_id, rc);
		return rc;
	}
	resources = &parms.resources;

	/*
	 * Need to account for size of ctrl_chan_name and 1 extra for Null
	 * terminator
	 */
	copy_nbytes = sizeof(parms.ctrl_chan_name) -
		strlen(parms.ctrl_chan_name) - 1;

	/*
	 * Build the ctrl_chan_name with shared token.
	 * When HA is enabled, the WC TCAM needs extra management by the core,
	 * so add the wc_tcam string to the control channel.
	 */
	if (bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx))
		strncat(parms.ctrl_chan_name, "-tf_shared-wc_tcam",
			copy_nbytes);
	else
		strncat(parms.ctrl_chan_name, "-tf_shared", copy_nbytes);

	rc = bnxt_ulp_tf_shared_session_resources_get(bp->ulp_ctx, resources);
	if (rc)
		return rc;

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	switch (ulp_dev_id) {
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		parms.device_type = TF_DEVICE_TYPE_WH;
		break;
	case BNXT_ULP_DEVICE_ID_STINGRAY:
		parms.device_type = TF_DEVICE_TYPE_SR;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
		parms.device_type = TF_DEVICE_TYPE_THOR;
		break;
	default:
		BNXT_TF_DBG(ERR, "Unable to determine dev for opening session.\n");
		return rc;
	}

	parms.shadow_copy = true;
	parms.bp = bp;
	if (app_id == 0)
		parms.wc_num_slices = TF_WC_TCAM_2_SLICE_PER_ROW;
	else
		parms.wc_num_slices = TF_WC_TCAM_1_SLICE_PER_ROW;

	/*
	 * Open the session here, but the collect the resources during the
	 * mapper initialization.
	 */
	rc = tf_open_session(&bp->tfp_shared, &parms);
	if (rc)
		return rc;

	if (parms.shared_session_creator)
		BNXT_TF_DBG(DEBUG, "Shared session creator.\n");
	else
		BNXT_TF_DBG(DEBUG, "Shared session attached.\n");

	/* Save the shared session in global data */
	if (!session->g_shared_tfp.session)
		session->g_shared_tfp.session = bp->tfp_shared.session;

	rc = bnxt_ulp_cntxt_shared_tfp_set(bp->ulp_ctx, &bp->tfp_shared);
	if (rc)
		BNXT_TF_DBG(ERR, "Failed to add shared tfp to ulp (%d)\n", rc);

	return rc;
}

static int32_t
ulp_ctx_shared_session_attach(struct bnxt *bp,
			      struct bnxt_ulp_session_state *session)
{
	int32_t rc = 0;

	/* Simply return success if shared session not enabled */
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		bp->tfp_shared.session = session->g_shared_tfp.session;
		rc = ulp_ctx_shared_session_open(bp, session);
	}

	return rc;
}

static void
ulp_ctx_shared_session_detach(struct bnxt *bp)
{
	if (bnxt_ulp_cntxt_shared_session_enabled(bp->ulp_ctx)) {
		if (bp->tfp_shared.session) {
			tf_close_session(&bp->tfp_shared);
			bp->tfp_shared.session = NULL;
		}
	}
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
ulp_ctx_session_open(struct bnxt *bp,
		     struct bnxt_ulp_session_state *session)
{
	struct rte_eth_dev		*ethdev = bp->eth_dev;
	int32_t				rc = 0;
	struct tf_open_session_parms	params;
	struct tf_session_resources	*resources;
	uint32_t ulp_dev_id = BNXT_ULP_DEVICE_ID_LAST;
	uint8_t app_id;

	memset(&params, 0, sizeof(params));

	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  params.ctrl_chan_name);
	if (rc) {
		BNXT_TF_DBG(ERR, "Invalid port %d, rc = %d\n",
			    ethdev->data->port_id, rc);
		return rc;
	}

	params.shadow_copy = true;

	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &ulp_dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get device id from ulp.\n");
		return rc;
	}

	switch (ulp_dev_id) {
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		params.device_type = TF_DEVICE_TYPE_WH;
		break;
	case BNXT_ULP_DEVICE_ID_STINGRAY:
		params.device_type = TF_DEVICE_TYPE_SR;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
		params.device_type = TF_DEVICE_TYPE_THOR;
		break;
	default:
		BNXT_TF_DBG(ERR, "Unable to determine device for opening session.\n");
		return rc;
	}

	resources = &params.resources;
	rc = bnxt_ulp_tf_resources_get(bp->ulp_ctx, resources);
	if (rc)
		return rc;

	params.bp = bp;
	if (app_id == 0)
		params.wc_num_slices = TF_WC_TCAM_2_SLICE_PER_ROW;
	else
		params.wc_num_slices = TF_WC_TCAM_1_SLICE_PER_ROW;

	rc = tf_open_session(&bp->tfp, &params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to open TF session - %s, rc = %d\n",
			    params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	if (!session->session_opened) {
		session->session_opened = 1;
		session->g_tfp = rte_zmalloc("bnxt_ulp_session_tfp",
					     sizeof(struct tf), 0);
		session->g_tfp->session = bp->tfp.session;
	}
	return rc;
}

/*
 * Close the ULP session.
 * It takes the ulp context pointer.
 */
static void
ulp_ctx_session_close(struct bnxt *bp,
		      struct bnxt_ulp_session_state *session)
{
	/* close the session in the hardware */
	if (session->session_opened)
		tf_close_session(&bp->tfp);
	session->session_opened = 0;
	rte_free(session->g_tfp);
	session->g_tfp = NULL;
}

static void
bnxt_init_tbl_scope_parms(struct bnxt *bp,
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
	BNXT_TF_DBG(INFO, "Table Scope initialized with %uK flows.\n",
		    params->rx_num_flows_in_k);
}

/* Initialize Extended Exact Match host memory. */
static int32_t
ulp_eem_tbl_scope_init(struct bnxt *bp)
{
	struct tf_alloc_tbl_scope_parms params = {0};
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	uint32_t dev_id;
	int rc;

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(ERR, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(bp->ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		BNXT_TF_DBG(INFO, "Table Scope alloc is not required\n");
		return 0;
	}

	bnxt_init_tbl_scope_parms(bp, &params);
	rc = tf_alloc_tbl_scope(&bp->tfp, &params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to allocate eem table scope rc = %d\n",
			    rc);
		return rc;
	}
	rc = bnxt_ulp_cntxt_tbl_scope_id_set(bp->ulp_ctx, params.tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to set table scope id\n");
		return rc;
	}

	return 0;
}

/* Free Extended Exact Match host memory */
static int32_t
ulp_eem_tbl_scope_deinit(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
{
	struct tf_free_tbl_scope_parms	params = {0};
	struct tf			*tfp;
	int32_t				rc = 0;
	struct bnxt_ulp_device_params *dparms;
	enum bnxt_ulp_flow_mem_type mtype;
	uint32_t dev_id;

	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx, BNXT_ULP_SHARED_SESSION_NO);
	if (!tfp) {
		BNXT_TF_DBG(ERR, "Failed to get the truflow pointer\n");
		return -EINVAL;
	}

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id)) {
		BNXT_TF_DBG(ERR, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(ERR, "could not fetch the device params\n");
		return -ENODEV;
	}

	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype))
		return -EINVAL;
	if (mtype != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
		BNXT_TF_DBG(INFO, "Table Scope free is not required\n");
		return 0;
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_get(ulp_ctx, &params.tbl_scope_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get the table scope id\n");
		return -EINVAL;
	}

	rc = tf_free_tbl_scope(tfp, &params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to free table scope\n");
		return -EINVAL;
	}
	return rc;
}

/* The function to free and deinit the ulp context data. */
static int32_t
ulp_ctx_deinit(struct bnxt *bp,
	       struct bnxt_ulp_session_state *session)
{
	/* close the tf session */
	ulp_ctx_session_close(bp, session);

	/* The shared session must be closed last. */
	ulp_ctx_shared_session_close(bp, session);

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
ulp_ctx_init(struct bnxt *bp,
	     struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_data	*ulp_data;
	int32_t			rc = 0;
	enum bnxt_ulp_device_id devid;

	/* Initialize the context entries list */
	bnxt_ulp_cntxt_list_init();

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add the context list entry\n");
		return -ENOMEM;
	}

	/* Allocate memory to hold ulp context data. */
	ulp_data = rte_zmalloc("bnxt_ulp_data",
			       sizeof(struct bnxt_ulp_data), 0);
	if (!ulp_data) {
		BNXT_TF_DBG(ERR, "Failed to allocate memory for ulp data\n");
		return -ENOMEM;
	}

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = ulp_data;
	session->cfg_data = ulp_data;
	ulp_data->ref_cnt++;
	ulp_data->ulp_flags |= BNXT_ULP_VF_REP_ENABLED;

	rc = bnxt_ulp_devid_get(bp, &devid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to determine device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_dev_id_set(bp->ulp_ctx, devid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to set device for ULP init.\n");
		goto error_deinit;
	}

	rc = bnxt_ulp_cntxt_app_id_set(bp->ulp_ctx, bp->app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to set app_id for ULP init.\n");
		goto error_deinit;
	}
	BNXT_TF_DBG(DEBUG, "Ulp initialized with app id %d\n", bp->app_id);

	rc = bnxt_ulp_cntxt_app_caps_init(bp, bp->app_id, devid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to set caps for app(%x)/dev(%x)\n",
			    bp->app_id, devid);
		goto error_deinit;
	}

	/*
	 * Shared session must be created before first regular session but after
	 * the ulp_ctx is valid.
	 */
	rc = ulp_ctx_shared_session_open(bp, session);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to open shared session (%d)\n", rc);
		goto error_deinit;
	}

	/* Open the ulp session. */
	rc = ulp_ctx_session_open(bp, session);
	if (rc)
		goto error_deinit;

	bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, &bp->tfp);
	return rc;

error_deinit:
	session->session_opened = 1;
	(void)ulp_ctx_deinit(bp, session);
	return rc;
}

/* The function to initialize ulp dparms with devargs */
static int32_t
ulp_dparms_init(struct bnxt *bp, struct bnxt_ulp_context *ulp_ctx)
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
		BNXT_TF_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(DEBUG, "Failed to get device parms\n");
		return -EINVAL;
	}

	/* num_flows = max_num_kflows * 1024 */
	dparms->ext_flow_db_num_entries = bp->max_num_kflows * 1024;
	/* GFID =  2 * num_flows */
	dparms->mark_db_gfid_entries = dparms->ext_flow_db_num_entries * 2;
	BNXT_TF_DBG(DEBUG, "Set the number of flows = %" PRIu64 "\n",
		    dparms->ext_flow_db_num_entries);

	return 0;
}

/* The function to initialize bp flags with truflow features */
static int32_t
ulp_dparms_dev_port_intf_update(struct bnxt *bp,
				struct bnxt_ulp_context *ulp_ctx)
{
	enum bnxt_ulp_flow_mem_type mtype;

	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype))
		return -EINVAL;
	/* Update the bp flag with gfid flag */
	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT)
		bp->flags |= BNXT_FLAG_GFID_ENABLE;

	return 0;
}

static int32_t
ulp_ctx_attach(struct bnxt *bp,
	       struct bnxt_ulp_session_state *session)
{
	int32_t rc = 0;
	uint32_t flags, dev_id = BNXT_ULP_DEVICE_ID_LAST;
	uint8_t app_id;

	/* Increment the ulp context data reference count usage. */
	bp->ulp_ctx->cfg_data = session->cfg_data;
	bp->ulp_ctx->cfg_data->ref_cnt++;

	/* update the session details in bnxt tfp */
	bp->tfp.session = session->g_tfp->session;

	/* Add the context to the context entries list */
	rc = bnxt_ulp_cntxt_list_add(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to add the context list entry\n");
		return -EINVAL;
	}

	/*
	 * The supported flag will be set during the init. Use it now to
	 * know if we should go through the attach.
	 */
	rc = bnxt_ulp_cntxt_app_id_get(bp->ulp_ctx, &app_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to get the app id from ulp.\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_dev_id_get(bp->ulp_ctx, &dev_id);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable do get the dev_id.\n");
		return -EINVAL;
	}

	flags = bp->ulp_ctx->cfg_data->ulp_flags;
	if (ULP_APP_DEV_UNSUPPORTED_ENABLED(flags)) {
		BNXT_TF_DBG(ERR, "APP ID %d, Device ID: 0x%x not supported.\n",
			    app_id, dev_id);
		return -EINVAL;
	}

	/* Create a TF Client */
	rc = ulp_ctx_session_open(bp, session);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to open ctxt session, rc:%d\n", rc);
		bp->tfp.session = NULL;
		return rc;
	}

	bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, &bp->tfp);
	return rc;
}

static void
ulp_ctx_detach(struct bnxt *bp)
{
	if (bp->tfp.session) {
		tf_close_session(&bp->tfp);
		bp->tfp.session = NULL;
	}
}

/*
 * Initialize the state of an ULP session.
 * If the state of an ULP session is not initialized, set it's state to
 * initialized. If the state is already initialized, do nothing.
 */
static void
ulp_context_initialized(struct bnxt_ulp_session_state *session, bool *init)
{
	pthread_mutex_lock(&session->bnxt_ulp_mutex);

	if (!session->bnxt_ulp_init) {
		session->bnxt_ulp_init = true;
		*init = false;
	} else {
		*init = true;
	}

	pthread_mutex_unlock(&session->bnxt_ulp_mutex);
}

/*
 * Check if an ULP session is already allocated for a specific PCI
 * domain & bus. If it is already allocated simply return the session
 * pointer, otherwise allocate a new session.
 */
static struct bnxt_ulp_session_state *
ulp_get_session(struct bnxt *bp, struct rte_pci_addr *pci_addr)
{
	struct bnxt_ulp_session_state *session;

	/* if multi root capability is enabled, then ignore the pci bus id */
	STAILQ_FOREACH(session, &bnxt_ulp_session_list, next) {
		if (session->pci_info.domain == pci_addr->domain &&
		    (BNXT_MULTIROOT_EN(bp) ||
		    session->pci_info.bus == pci_addr->bus)) {
			return session;
		}
	}
	return NULL;
}

/*
 * Allocate and Initialize an ULP session and set it's state to INITIALIZED.
 * If it's already initialized simply return the already existing session.
 */
static struct bnxt_ulp_session_state *
ulp_session_init(struct bnxt *bp,
		 bool *init)
{
	struct rte_pci_device		*pci_dev;
	struct rte_pci_addr		*pci_addr;
	struct bnxt_ulp_session_state	*session;
	int rc = 0;

	if (!bp)
		return NULL;

	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;

	pthread_mutex_lock(&bnxt_ulp_global_mutex);

	session = ulp_get_session(bp, pci_addr);
	if (!session) {
		/* Not Found the session  Allocate a new one */
		session = rte_zmalloc("bnxt_ulp_session",
				      sizeof(struct bnxt_ulp_session_state),
				      0);
		if (!session) {
			BNXT_TF_DBG(ERR,
				    "Allocation failed for bnxt_ulp_session\n");
			pthread_mutex_unlock(&bnxt_ulp_global_mutex);
			return NULL;

		} else {
			/* Add it to the queue */
			session->pci_info.domain = pci_addr->domain;
			session->pci_info.bus = pci_addr->bus;
			rc = pthread_mutex_init(&session->bnxt_ulp_mutex, NULL);
			if (rc) {
				BNXT_TF_DBG(ERR, "mutex create failed\n");
				pthread_mutex_unlock(&bnxt_ulp_global_mutex);
				return NULL;
			}
			STAILQ_INSERT_TAIL(&bnxt_ulp_session_list,
					   session, next);
		}
	}
	ulp_context_initialized(session, init);
	pthread_mutex_unlock(&bnxt_ulp_global_mutex);
	return session;
}

/*
 * When a device is closed, remove it's associated session from the global
 * session list.
 */
static void
ulp_session_deinit(struct bnxt_ulp_session_state *session)
{
	if (!session)
		return;

	if (!session->cfg_data) {
		pthread_mutex_lock(&bnxt_ulp_global_mutex);
		STAILQ_REMOVE(&bnxt_ulp_session_list, session,
			      bnxt_ulp_session_state, next);
		pthread_mutex_destroy(&session->bnxt_ulp_mutex);
		rte_free(session);
		pthread_mutex_unlock(&bnxt_ulp_global_mutex);
	}
}

/*
 * Internal api to enable NAT feature.
 * Set set_flag to 1 to set the value or zero to reset the value.
 * returns 0 on success.
 */
static int32_t
bnxt_ulp_global_cfg_update(struct bnxt *bp,
			   enum tf_dir dir,
			   enum tf_global_config_type type,
			   uint32_t offset,
			   uint32_t value,
			   uint32_t set_flag)
{
	uint32_t global_cfg = 0;
	int rc;
	struct tf_global_cfg_parms parms = { 0 };

	/* Initialize the params */
	parms.dir = dir,
	parms.type = type,
	parms.offset = offset,
	parms.config = (uint8_t *)&global_cfg,
	parms.config_sz_in_bytes = sizeof(global_cfg);

	rc = tf_get_global_cfg(&bp->tfp, &parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to get global cfg 0x%x rc:%d\n",
			    type, rc);
		return rc;
	}

	if (set_flag)
		global_cfg |= value;
	else
		global_cfg &= ~value;

	/* SET the register RE_CFA_REG_ACT_TECT */
	rc = tf_set_global_cfg(&bp->tfp, &parms);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to set global cfg 0x%x rc:%d\n",
			    type, rc);
		return rc;
	}
	return rc;
}

/* Internal function to delete all the flows belonging to the given port */
static void
bnxt_ulp_flush_port_flows(struct bnxt *bp)
{
	uint16_t func_id;

	/* it is assumed that port is either TVF or PF */
	if (ulp_port_db_port_func_id_get(bp->ulp_ctx,
					 bp->eth_dev->data->port_id,
					 &func_id)) {
		BNXT_TF_DBG(ERR, "Invalid argument\n");
		return;
	}
	(void)ulp_flow_db_function_flow_flush(bp->ulp_ctx, func_id);
}

/* Internal function to delete the VFR default flows */
static void
bnxt_ulp_destroy_vfr_default_rules(struct bnxt *bp, bool global)
{
	struct bnxt_ulp_vfr_rule_info *info;
	uint16_t port_id;
	struct rte_eth_dev *vfr_eth_dev;
	struct bnxt_representor *vfr_bp;

	if (!BNXT_TRUFLOW_EN(bp) || BNXT_ETH_DEV_IS_REPRESENTOR(bp->eth_dev))
		return;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	/* Delete default rules for all ports */
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
		info = &bp->ulp_ctx->cfg_data->vfr_rule_info[port_id];
		if (!info->valid)
			continue;

		if (!global && info->parent_port_id !=
		    bp->eth_dev->data->port_id)
			continue;

		/* Destroy the flows */
		ulp_default_flow_destroy(bp->eth_dev, info->vfr_flow_id);
		/* Clean up the tx action pointer */
		vfr_eth_dev = &rte_eth_devices[port_id];
		if (vfr_eth_dev) {
			vfr_bp = vfr_eth_dev->data->dev_private;
			vfr_bp->vfr_tx_cfa_action = 0;
		}
		memset(info, 0, sizeof(struct bnxt_ulp_vfr_rule_info));
	}
}

/*
 * When a port is deinit'ed by dpdk. This function is called
 * and this function clears the ULP context and rest of the
 * infrastructure associated with it.
 */
static void
bnxt_ulp_deinit(struct bnxt *bp,
		struct bnxt_ulp_session_state *session)
{
	bool ha_enabled;

	if (!bp->ulp_ctx || !bp->ulp_ctx->cfg_data)
		return;

	ha_enabled = bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx);
	if (ha_enabled && session->session_opened) {
		int32_t rc = ulp_ha_mgr_close(bp->ulp_ctx);
		if (rc)
			BNXT_TF_DBG(ERR, "Failed to close HA (%d)\n", rc);
	}

	/* clean up default flows */
	bnxt_ulp_destroy_df_rules(bp, true);

	/* clean up default VFR flows */
	bnxt_ulp_destroy_vfr_default_rules(bp, true);

	/* clean up regular flows */
	ulp_flow_db_flush_flows(bp->ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR);

	/* cleanup the eem table scope */
	ulp_eem_tbl_scope_deinit(bp, bp->ulp_ctx);

	/* cleanup the flow database */
	ulp_flow_db_deinit(bp->ulp_ctx);

	/* Delete the Mark database */
	ulp_mark_db_deinit(bp->ulp_ctx);

	/* cleanup the ulp mapper */
	ulp_mapper_deinit(bp->ulp_ctx);

	/* Delete the Flow Counter Manager */
	ulp_fc_mgr_deinit(bp->ulp_ctx);

	/* Delete the Port database */
	ulp_port_db_deinit(bp->ulp_ctx);

	/* Disable NAT feature */
	(void)bnxt_ulp_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
					 TF_TUNNEL_ENCAP_NAT,
					 BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	(void)bnxt_ulp_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
					 TF_TUNNEL_ENCAP_NAT,
					 BNXT_ULP_NAT_OUTER_MOST_FLAGS, 0);

	/* free the flow db lock */
	pthread_mutex_destroy(&bp->ulp_ctx->cfg_data->flow_db_lock);

	if (ha_enabled)
		ulp_ha_mgr_deinit(bp->ulp_ctx);

	/* Delete the ulp context and tf session and free the ulp context */
	ulp_ctx_deinit(bp, session);
	BNXT_TF_DBG(DEBUG, "ulp ctx has been deinitialized\n");
}

/*
 * When a port is initialized by dpdk. This functions is called
 * and this function initializes the ULP context and rest of the
 * infrastructure associated with it.
 */
static int32_t
bnxt_ulp_init(struct bnxt *bp,
	      struct bnxt_ulp_session_state *session)
{
	int rc;

	/* Allocate and Initialize the ulp context. */
	rc = ulp_ctx_init(bp, session);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	rc = pthread_mutex_init(&bp->ulp_ctx->cfg_data->flow_db_lock, NULL);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to initialize flow db lock\n");
		goto jump_to_error;
	}

	/* Initialize ulp dparms with values devargs passed */
	rc = ulp_dparms_init(bp, bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize the dparms\n");
		goto jump_to_error;
	}

	/* create the port database */
	rc = ulp_port_db_init(bp->ulp_ctx, bp->port_cnt);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the port database\n");
		goto jump_to_error;
	}

	/* Create the Mark database. */
	rc = ulp_mark_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the mark database\n");
		goto jump_to_error;
	}

	/* Create the flow database. */
	rc = ulp_flow_db_init(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the flow database\n");
		goto jump_to_error;
	}

	/* Create the eem table scope. */
	rc = ulp_eem_tbl_scope_init(bp);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the eem scope table\n");
		goto jump_to_error;
	}

	rc = ulp_mapper_init(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize ulp mapper\n");
		goto jump_to_error;
	}

	rc = ulp_fc_mgr_init(bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to initialize ulp flow counter mgr\n");
		goto jump_to_error;
	}

	/*
	 * Enable NAT feature. Set the global configuration register
	 * Tunnel encap to enable NAT with the reuse of existing inner
	 * L2 header smac and dmac
	 */
	rc = bnxt_ulp_global_cfg_update(bp, TF_DIR_RX, TF_TUNNEL_ENCAP,
					TF_TUNNEL_ENCAP_NAT,
					BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to set rx global configuration\n");
		goto jump_to_error;
	}

	rc = bnxt_ulp_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
					TF_TUNNEL_ENCAP_NAT,
					BNXT_ULP_NAT_OUTER_MOST_FLAGS, 1);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to set tx global configuration\n");
		goto jump_to_error;
	}

	if (bnxt_ulp_cntxt_ha_enabled(bp->ulp_ctx)) {
		rc = ulp_ha_mgr_init(bp->ulp_ctx);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to initialize HA %d\n", rc);
			goto jump_to_error;
		}
		rc = ulp_ha_mgr_open(bp->ulp_ctx);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to Process HA Open %d\n", rc);
			goto jump_to_error;
		}
	}
	BNXT_TF_DBG(DEBUG, "ulp ctx has been initialized\n");
	return rc;

jump_to_error:
	bnxt_ulp_deinit(bp, session);
	return rc;
}

/*
 * When a port is initialized by dpdk. This functions sets up
 * the port specific details.
 */
int32_t
bnxt_ulp_port_init(struct bnxt *bp)
{
	struct bnxt_ulp_session_state *session;
	bool initialized;
	enum bnxt_ulp_device_id devid = BNXT_ULP_DEVICE_ID_LAST;
	uint32_t ulp_flags;
	int32_t rc = 0;

	if (!BNXT_TRUFLOW_EN(bp)) {
		BNXT_TF_DBG(DEBUG,
			    "Skip ulp init for port: %d, TF is not enabled\n",
			    bp->eth_dev->data->port_id);
		return rc;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		BNXT_TF_DBG(DEBUG,
			    "Skip ulp init for port: %d, not a TVF or PF\n",
			    bp->eth_dev->data->port_id);
		return rc;
	}

	if (bp->ulp_ctx) {
		BNXT_TF_DBG(DEBUG, "ulp ctx already allocated\n");
		return rc;
	}

	bp->ulp_ctx = rte_zmalloc("bnxt_ulp_ctx",
				  sizeof(struct bnxt_ulp_context), 0);
	if (!bp->ulp_ctx) {
		BNXT_TF_DBG(ERR, "Failed to allocate ulp ctx\n");
		return -ENOMEM;
	}

	/*
	 * Multiple uplink ports can be associated with a single vswitch.
	 * Make sure only the port that is started first will initialize
	 * the TF session.
	 */
	session = ulp_session_init(bp, &initialized);
	if (!session) {
		BNXT_TF_DBG(ERR, "Failed to initialize the tf session\n");
		rc = -EIO;
		goto jump_to_error;
	}

	if (initialized) {
		/*
		 * If ULP is already initialized for a specific domain then
		 * simply assign the ulp context to this rte_eth_dev.
		 */
		rc = ulp_ctx_attach(bp, session);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to attach the ulp context\n");
			goto jump_to_error;
		}

		/*
		 * Attach to the shared session, must be called after the
		 * ulp_ctx_attach in order to ensure that ulp data is available
		 * for attaching.
		 */
		rc = ulp_ctx_shared_session_attach(bp, session);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed attach to shared session (%d)", rc);
			goto jump_to_error;
		}
	} else {
		rc = bnxt_ulp_init(bp, session);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to initialize the ulp init\n");
			goto jump_to_error;
		}
	}

	/* Update bnxt driver flags */
	rc = ulp_dparms_dev_port_intf_update(bp, bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to update driver flags\n");
		goto jump_to_error;
	}

	/* update the port database for the given interface */
	rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp->eth_dev);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to update port database\n");
		goto jump_to_error;
	}
	/* create the default rules */
	rc = bnxt_ulp_create_df_rules(bp);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create default flow\n");
		goto jump_to_error;
	}

	rc = bnxt_ulp_devid_get(bp, &devid);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to determine device for ULP port init.\n");
		goto jump_to_error;
	}

	/* set the unicast mode */
	if (bnxt_ulp_cntxt_ptr2_ulp_flags_get(bp->ulp_ctx, &ulp_flags)) {
		BNXT_TF_DBG(ERR, "Error in getting ULP context flags\n");
		goto jump_to_error;
	}
	if (ulp_flags & BNXT_ULP_APP_UNICAST_ONLY) {
		if (bnxt_pmd_set_unicast_rxmask(bp->eth_dev)) {
			BNXT_TF_DBG(ERR, "Error in setting unicast rxmode\n");
			goto jump_to_error;
		}
	}

	return rc;

jump_to_error:
	bnxt_ulp_port_deinit(bp);
	return rc;
}

/*
 * When a port is de-initialized by dpdk. This functions clears up
 * the port specific details.
 */
void
bnxt_ulp_port_deinit(struct bnxt *bp)
{
	struct bnxt_ulp_session_state *session;
	struct rte_pci_device *pci_dev;
	struct rte_pci_addr *pci_addr;

	if (!BNXT_TRUFLOW_EN(bp)) {
		BNXT_TF_DBG(DEBUG,
			    "Skip ULP deinit for port:%d, TF is not enabled\n",
			    bp->eth_dev->data->port_id);
		return;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		BNXT_TF_DBG(DEBUG,
			    "Skip ULP deinit port:%d, not a TVF or PF\n",
			    bp->eth_dev->data->port_id);
		return;
	}

	if (!bp->ulp_ctx) {
		BNXT_TF_DBG(DEBUG, "ulp ctx already de-allocated\n");
		return;
	}

	BNXT_TF_DBG(DEBUG, "BNXT Port:%d ULP port deinit\n",
		    bp->eth_dev->data->port_id);

	/* Get the session details  */
	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;
	pthread_mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(bp, pci_addr);
	pthread_mutex_unlock(&bnxt_ulp_global_mutex);

	/* session not found then just exit */
	if (!session) {
		/* Free the ulp context */
		rte_free(bp->ulp_ctx);
		bp->ulp_ctx = NULL;
		return;
	}

	/* Check the reference count to deinit or deattach*/
	if (bp->ulp_ctx->cfg_data && bp->ulp_ctx->cfg_data->ref_cnt) {
		bp->ulp_ctx->cfg_data->ref_cnt--;
		if (bp->ulp_ctx->cfg_data->ref_cnt) {
			/* free the port details */
			/* Free the default flow rule associated to this port */
			bnxt_ulp_destroy_df_rules(bp, false);
			bnxt_ulp_destroy_vfr_default_rules(bp, false);

			/* free flows associated with this port */
			bnxt_ulp_flush_port_flows(bp);

			/* close the session associated with this port */
			ulp_ctx_detach(bp);

			/* always detach/close shared after the session. */
			ulp_ctx_shared_session_detach(bp);
		} else {
			/* Perform ulp ctx deinit */
			bnxt_ulp_deinit(bp, session);
		}
	}

	/* Free the ulp context in the context entry list */
	bnxt_ulp_cntxt_list_del(bp->ulp_ctx);

	/* clean up the session */
	ulp_session_deinit(session);

	/* Free the ulp context */
	rte_free(bp->ulp_ctx);
	bp->ulp_ctx = NULL;
}

/* Below are the access functions to access internal data of ulp context. */
/* Function to set the Mark DB into the context */
int32_t
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->mark_tbl = mark_tbl;

	return 0;
}

/* Function to retrieve the Mark DB from the context. */
struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->mark_tbl;
}

bool
bnxt_ulp_cntxt_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	return ULP_SHARED_SESSION_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags);
}

int32_t
bnxt_ulp_cntxt_app_id_set(struct bnxt_ulp_context *ulp_ctx, uint8_t app_id)
{
	if (!ulp_ctx)
		return -EINVAL;
	ulp_ctx->cfg_data->app_id = app_id;
	return 0;
}

int32_t
bnxt_ulp_cntxt_app_id_get(struct bnxt_ulp_context *ulp_ctx, uint8_t *app_id)
{
	/* Default APP id is zero */
	if (!ulp_ctx || !app_id)
		return -EINVAL;
	*app_id = ulp_ctx->cfg_data->app_id;
	return 0;
}

/* Function to set the device id of the hardware. */
int32_t
bnxt_ulp_cntxt_dev_id_set(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t dev_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->dev_id = dev_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to get the device id of the hardware. */
int32_t
bnxt_ulp_cntxt_dev_id_get(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t *dev_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*dev_id = ulp_ctx->cfg_data->dev_id;
		return 0;
	}
	*dev_id = BNXT_ULP_DEVICE_ID_LAST;
	BNXT_TF_DBG(ERR, "Failed to read dev_id from ulp ctxt\n");
	return -EINVAL;
}

int32_t
bnxt_ulp_cntxt_mem_type_set(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type mem_type)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->mem_type = mem_type;
		return 0;
	}
	BNXT_TF_DBG(ERR, "Failed to write mem_type in ulp ctxt\n");
	return -EINVAL;
}

int32_t
bnxt_ulp_cntxt_mem_type_get(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type *mem_type)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*mem_type = ulp_ctx->cfg_data->mem_type;
		return 0;
	}
	*mem_type = BNXT_ULP_FLOW_MEM_TYPE_LAST;
	BNXT_TF_DBG(ERR, "Failed to read mem_type in ulp ctxt\n");
	return -EINVAL;
}

/* Function to get the table scope id of the EEM table. */
int32_t
bnxt_ulp_cntxt_tbl_scope_id_get(struct bnxt_ulp_context *ulp_ctx,
				uint32_t *tbl_scope_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		*tbl_scope_id = ulp_ctx->cfg_data->tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the table scope id of the EEM table. */
int32_t
bnxt_ulp_cntxt_tbl_scope_id_set(struct bnxt_ulp_context *ulp_ctx,
				uint32_t tbl_scope_id)
{
	if (ulp_ctx && ulp_ctx->cfg_data) {
		ulp_ctx->cfg_data->tbl_scope_id = tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the shared tfp session details from the ulp context. */
int32_t
bnxt_ulp_cntxt_shared_tfp_set(struct bnxt_ulp_context *ulp, struct tf *tfp)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return -EINVAL;
	}

	if (tfp == NULL) {
		if (ulp->cfg_data->num_shared_clients > 0)
			ulp->cfg_data->num_shared_clients--;
	} else {
		ulp->cfg_data->num_shared_clients++;
	}

	ulp->g_shared_tfp = tfp;
	return 0;
}

/* Function to get the shared tfp session details from the ulp context. */
struct tf *
bnxt_ulp_cntxt_shared_tfp_get(struct bnxt_ulp_context *ulp)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return NULL;
	}
	return ulp->g_shared_tfp;
}

/* Function to get the number of shared clients attached */
uint8_t
bnxt_ulp_cntxt_num_shared_clients_get(struct bnxt_ulp_context *ulp)
{
	if (ulp == NULL || ulp->cfg_data == NULL) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return 0;
	}
	return ulp->cfg_data->num_shared_clients;
}

/* Function to set the tfp session details from the ulp context. */
int32_t
bnxt_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp, struct tf *tfp)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return -EINVAL;
	}

	ulp->g_tfp = tfp;
	return 0;
}

/* Function to get the tfp session details from the ulp context. */
struct tf *
bnxt_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_shared_session shared)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return NULL;
	}
	if (shared)
		return ulp->g_shared_tfp;
	else
		return ulp->g_tfp;
}

/*
 * Get the device table entry based on the device id.
 *
 * dev_id [in] The device id of the hardware
 *
 * Returns the pointer to the device parameters.
 */
struct bnxt_ulp_device_params *
bnxt_ulp_device_params_get(uint32_t dev_id)
{
	if (dev_id < BNXT_ULP_MAX_NUM_DEVICES)
		return &ulp_device_params[dev_id];
	return NULL;
}

/* Function to set the flow database to the ulp context. */
int32_t
bnxt_ulp_cntxt_ptr2_flow_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_flow_db	*flow_db)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->flow_db = flow_db;
	return 0;
}

/* Function to get the flow database from the ulp context. */
struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->flow_db;
}

/* Function to get the tunnel cache table info from the ulp context. */
struct bnxt_tun_cache_entry *
bnxt_ulp_cntxt_ptr2_tun_tbl_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->tun_tbl;
}

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_eth_dev_ptr2_cntxt_get(struct rte_eth_dev	*dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	if (BNXT_ETH_DEV_IS_REPRESENTOR(dev)) {
		struct bnxt_representor *vfr = dev->data->dev_private;

		bp = vfr->parent_dev->data->dev_private;
	}

	if (!bp) {
		BNXT_TF_DBG(ERR, "Bnxt private data is not initialized\n");
		return NULL;
	}
	return bp->ulp_ctx;
}

int32_t
bnxt_ulp_cntxt_ptr2_mapper_data_set(struct bnxt_ulp_context *ulp_ctx,
				    void *mapper_data)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->mapper_data = mapper_data;
	return 0;
}

void *
bnxt_ulp_cntxt_ptr2_mapper_data_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return NULL;
	}

	return ulp_ctx->cfg_data->mapper_data;
}

/* Function to set the port database to the ulp context. */
int32_t
bnxt_ulp_cntxt_ptr2_port_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_port_db	*port_db)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	ulp_ctx->cfg_data->port_db = port_db;
	return 0;
}

/* Function to get the port database from the ulp context. */
struct bnxt_ulp_port_db *
bnxt_ulp_cntxt_ptr2_port_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->port_db;
}

/* Function to set the flow counter info into the context */
int32_t
bnxt_ulp_cntxt_ptr2_fc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_fc_info *ulp_fc_info)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->fc_info = ulp_fc_info;

	return 0;
}

/* Function to retrieve the flow counter info from the context. */
struct bnxt_ulp_fc_info *
bnxt_ulp_cntxt_ptr2_fc_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return NULL;

	return ulp_ctx->cfg_data->fc_info;
}

/* Function to get the ulp flags from the ulp context. */
int32_t
bnxt_ulp_cntxt_ptr2_ulp_flags_get(struct bnxt_ulp_context *ulp_ctx,
				  uint32_t *flags)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -1;

	*flags =  ulp_ctx->cfg_data->ulp_flags;
	return 0;
}

/* Function to get the ulp vfr info from the ulp context. */
struct bnxt_ulp_vfr_rule_info*
bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(struct bnxt_ulp_context *ulp_ctx,
				     uint32_t port_id)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data || port_id >= RTE_MAX_ETHPORTS)
		return NULL;

	return &ulp_ctx->cfg_data->vfr_rule_info[port_id];
}

/* Function to acquire the flow database lock from the ulp context. */
int32_t
bnxt_ulp_cntxt_acquire_fdb_lock(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -1;

	if (pthread_mutex_lock(&ulp_ctx->cfg_data->flow_db_lock)) {
		BNXT_TF_DBG(ERR, "unable to acquire fdb lock\n");
		return -1;
	}
	return 0;
}

/* Function to release the flow database lock from the ulp context. */
void
bnxt_ulp_cntxt_release_fdb_lock(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return;

	pthread_mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
}

/* Function to set the ha info into the context */
int32_t
bnxt_ulp_cntxt_ptr2_ha_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_ha_mgr_info *ulp_ha_info)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}
	ulp_ctx->cfg_data->ha_info = ulp_ha_info;
	return 0;
}

/* Function to retrieve the ha info from the context. */
struct bnxt_ulp_ha_mgr_info *
bnxt_ulp_cntxt_ptr2_ha_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return NULL;
	return ulp_ctx->cfg_data->ha_info;
}

bool
bnxt_ulp_cntxt_ha_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)
		return false;
	return !!ULP_HIGH_AVAIL_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags);
}

static int32_t
bnxt_ulp_cntxt_list_init(void)
{
	/* Create the cntxt spin lock only once*/
	if (!bnxt_ulp_ctxt_lock_created)
		rte_spinlock_init(&bnxt_ulp_ctxt_lock);
	bnxt_ulp_ctxt_lock_created = 1;
	return 0;
}

static int32_t
bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx)
{
	struct ulp_context_list_entry	*entry;

	entry = rte_zmalloc(NULL, sizeof(struct ulp_context_list_entry), 0);
	if (entry == NULL) {
		BNXT_TF_DBG(ERR, "unable to allocate memory\n");
		return -ENOMEM;
	}

	rte_spinlock_lock(&bnxt_ulp_ctxt_lock);
	entry->ulp_ctx = ulp_ctx;
	TAILQ_INSERT_TAIL(&ulp_cntx_list, entry, next);
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
	return 0;
}

static void
bnxt_ulp_cntxt_list_del(struct bnxt_ulp_context *ulp_ctx)
{
	struct ulp_context_list_entry	*entry, *temp;

	rte_spinlock_lock(&bnxt_ulp_ctxt_lock);
	RTE_TAILQ_FOREACH_SAFE(entry, &ulp_cntx_list, next, temp) {
		if (entry->ulp_ctx == ulp_ctx) {
			TAILQ_REMOVE(&ulp_cntx_list, entry, next);
			rte_free(entry);
			break;
		}
	}
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
}

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_acquire(void *arg)
{
	struct ulp_context_list_entry	*entry;

	/* take a lock and get the first ulp context available */
	if (rte_spinlock_trylock(&bnxt_ulp_ctxt_lock)) {
		TAILQ_FOREACH(entry, &ulp_cntx_list, next)
			if (entry->ulp_ctx->cfg_data == arg)
				return entry->ulp_ctx;
		rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
	}
	return NULL;
}

void
bnxt_ulp_cntxt_entry_release(void)
{
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
}

/* Function to get the app tunnel details from the ulp context. */
struct bnxt_flow_app_tun_ent *
bnxt_ulp_cntxt_ptr2_app_tun_list_get(struct bnxt_ulp_context *ulp)
{
	if (!ulp || !ulp->cfg_data)
		return NULL;

	return ulp->cfg_data->app_tun;
}
