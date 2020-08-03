/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

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

/* Linked list of all TF sessions. */
STAILQ_HEAD(, bnxt_ulp_session_state) bnxt_ulp_session_list =
			STAILQ_HEAD_INITIALIZER(bnxt_ulp_session_list);

/* Mutex to synchronize bnxt_ulp_session_list operations. */
static pthread_mutex_t bnxt_ulp_global_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Allow the deletion of context only for the bnxt device that
 * created the session
 * TBD - The implementation of the function should change to
 * using the reference count once tf_session_attach functionality
 * is fixed.
 */
bool
ulp_ctx_deinit_allowed(void *ptr)
{
	struct bnxt *bp = (struct bnxt *)ptr;

	if (!bp)
		return 0;

	if (&bp->tfp == bp->ulp_ctx->g_tfp)
		return 1;

	return 0;
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

	memset(&params, 0, sizeof(params));

	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  params.ctrl_chan_name);
	if (rc) {
		BNXT_TF_DBG(ERR, "Invalid port %d, rc = %d\n",
			    ethdev->data->port_id, rc);
		return rc;
	}

	params.shadow_copy = true;
	params.device_type = TF_DEVICE_TYPE_WH;
	resources = &params.resources;
	/** RX **/
	/* Identifiers */
	resources->ident_cnt[TF_DIR_RX].cnt[TF_IDENT_TYPE_L2_CTXT_HIGH] = 422;
	resources->ident_cnt[TF_DIR_RX].cnt[TF_IDENT_TYPE_L2_CTXT_LOW] = 6;
	resources->ident_cnt[TF_DIR_RX].cnt[TF_IDENT_TYPE_WC_PROF] = 8;
	resources->ident_cnt[TF_DIR_RX].cnt[TF_IDENT_TYPE_PROF_FUNC] = 8;
	resources->ident_cnt[TF_DIR_RX].cnt[TF_IDENT_TYPE_EM_PROF] = 8;

	/* Table Types */
	resources->tbl_cnt[TF_DIR_RX].cnt[TF_TBL_TYPE_FULL_ACT_RECORD] = 8192;
	resources->tbl_cnt[TF_DIR_RX].cnt[TF_TBL_TYPE_ACT_STATS_64] = 8192;
	resources->tbl_cnt[TF_DIR_RX].cnt[TF_TBL_TYPE_ACT_MODIFY_IPV4] = 1023;

	/* ENCAP */
	resources->tbl_cnt[TF_DIR_RX].cnt[TF_TBL_TYPE_ACT_ENCAP_8B] = 16;
	resources->tbl_cnt[TF_DIR_RX].cnt[TF_TBL_TYPE_ACT_ENCAP_16B] = 63;

	/* TCAMs */
	resources->tcam_cnt[TF_DIR_RX].cnt[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH] =
		422;
	resources->tcam_cnt[TF_DIR_RX].cnt[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW] =
		6;
	resources->tcam_cnt[TF_DIR_RX].cnt[TF_TCAM_TBL_TYPE_PROF_TCAM] = 8;
	resources->tcam_cnt[TF_DIR_RX].cnt[TF_TCAM_TBL_TYPE_WC_TCAM] = 88;

	/* EM */
	resources->em_cnt[TF_DIR_RX].cnt[TF_EM_TBL_TYPE_EM_RECORD] = 13176;

	/* EEM */
	resources->em_cnt[TF_DIR_RX].cnt[TF_EM_TBL_TYPE_TBL_SCOPE] = 1;

	/** TX **/
	/* Identifiers */
	resources->ident_cnt[TF_DIR_TX].cnt[TF_IDENT_TYPE_L2_CTXT_HIGH] = 292;
	resources->ident_cnt[TF_DIR_TX].cnt[TF_IDENT_TYPE_L2_CTXT_LOW] = 144;
	resources->ident_cnt[TF_DIR_TX].cnt[TF_IDENT_TYPE_WC_PROF] = 8;
	resources->ident_cnt[TF_DIR_TX].cnt[TF_IDENT_TYPE_PROF_FUNC] = 8;
	resources->ident_cnt[TF_DIR_TX].cnt[TF_IDENT_TYPE_EM_PROF] = 8;

	/* Table Types */
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_FULL_ACT_RECORD] = 8192;
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_ACT_STATS_64] = 8192;
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_ACT_MODIFY_IPV4] = 1023;

	/* ENCAP */
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_ACT_ENCAP_64B] = 511;
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_ACT_ENCAP_16B] = 200;

	/* TCAMs */
	resources->tcam_cnt[TF_DIR_TX].cnt[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH] =
		292;
	resources->tcam_cnt[TF_DIR_TX].cnt[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW] =
		144;
	resources->tcam_cnt[TF_DIR_TX].cnt[TF_TCAM_TBL_TYPE_PROF_TCAM] = 8;
	resources->tcam_cnt[TF_DIR_TX].cnt[TF_TCAM_TBL_TYPE_WC_TCAM] = 8;

	/* EM */
	resources->em_cnt[TF_DIR_TX].cnt[TF_EM_TBL_TYPE_EM_RECORD] = 15232;

	/* EEM */
	resources->em_cnt[TF_DIR_TX].cnt[TF_EM_TBL_TYPE_TBL_SCOPE] = 1;

	/* SP */
	resources->tbl_cnt[TF_DIR_TX].cnt[TF_TBL_TYPE_ACT_SP_SMAC_IPV4] = 488;

	rc = tf_open_session(&bp->tfp, &params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to open TF session - %s, rc = %d\n",
			    params.ctrl_chan_name, rc);
		return -EINVAL;
	}
	session->session_opened = 1;
	session->g_tfp = &bp->tfp;
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
	session->g_tfp = NULL;
	bp->ulp_ctx->g_tfp = NULL;
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
		params->rx_tbl_if_id = BNXT_ULP_RX_TBL_IF_ID;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k = BNXT_ULP_TX_NUM_FLOWS;
		params->tx_tbl_if_id = BNXT_ULP_TX_TBL_IF_ID;
	} else {
		params->rx_max_key_sz_in_bits = BNXT_ULP_DFLT_RX_MAX_KEY;
		params->rx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_RX_MAX_ACTN_ENTRY;
		params->rx_mem_size_in_mb = BNXT_ULP_DFLT_RX_MEM;
		params->rx_num_flows_in_k = dparms->flow_db_num_entries / 1024;
		params->rx_tbl_if_id = BNXT_ULP_RX_TBL_IF_ID;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k = dparms->flow_db_num_entries / 1024;
		params->tx_tbl_if_id = BNXT_ULP_TX_TBL_IF_ID;
	}
}

/* Initialize Extended Exact Match host memory. */
static int32_t
ulp_eem_tbl_scope_init(struct bnxt *bp)
{
	struct tf_alloc_tbl_scope_parms params = {0};
	uint32_t dev_id;
	struct bnxt_ulp_device_params *dparms;
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

	if (dparms->flow_mem_type != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
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
	uint32_t dev_id;

	if (!ulp_ctx || !ulp_ctx->cfg_data)
		return -EINVAL;

	/* Free the resources for the last device */
	if (!ulp_ctx_deinit_allowed(bp))
		return rc;

	tfp = bnxt_ulp_cntxt_tfp_get(ulp_ctx);
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

	if (dparms->flow_mem_type != BNXT_ULP_FLOW_MEM_TYPE_EXT) {
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
	if (!session || !bp) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* close the tf session */
	ulp_ctx_session_close(bp, session);

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

	if (!session || !bp) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
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

	/* Open the ulp session. */
	rc = ulp_ctx_session_open(bp, session);
	if (rc) {
		(void)ulp_ctx_deinit(bp, session);
		return rc;
	}

	bnxt_ulp_cntxt_tfp_set(bp->ulp_ctx, session->g_tfp);
	return rc;
}

/* The function to initialize ulp dparms with devargs */
static int32_t
ulp_dparms_init(struct bnxt *bp,
		struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_device_params *dparms;
	uint32_t dev_id;

	if (!bp->max_num_kflows)
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
	dparms->flow_db_num_entries = bp->max_num_kflows * 1024;
	/* GFID =  2 * num_flows */
	dparms->mark_db_gfid_entries = dparms->flow_db_num_entries * 2;
	BNXT_TF_DBG(DEBUG, "Set the number of flows = %"PRIu64"\n",
		    dparms->flow_db_num_entries);

	return 0;
}

/* The function to initialize bp flags with truflow features */
static int32_t
ulp_dparms_dev_port_intf_update(struct bnxt *bp,
				struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_device_params *dparms;
	uint32_t dev_id;

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id)) {
		BNXT_TF_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_TF_DBG(DEBUG, "Failed to get device parms\n");
		return -EINVAL;
	}

	/* Update the bp flag with gfid flag */
	if (dparms->flow_mem_type == BNXT_ULP_FLOW_MEM_TYPE_EXT)
		bp->flags |= BNXT_FLAG_GFID_ENABLE;

	return 0;
}

static int32_t
ulp_ctx_attach(struct bnxt_ulp_context *ulp_ctx,
	       struct bnxt_ulp_session_state *session)
{
	if (!ulp_ctx || !session) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* Increment the ulp context data reference count usage. */
	ulp_ctx->cfg_data = session->cfg_data;
	ulp_ctx->cfg_data->ref_cnt++;

	/* TBD call TF_session_attach. */
	ulp_ctx->g_tfp = session->g_tfp;
	return 0;
}

static int32_t
ulp_ctx_detach(struct bnxt *bp,
	       struct bnxt_ulp_session_state *session)
{
	struct bnxt_ulp_context *ulp_ctx;

	if (!bp || !session) {
		BNXT_TF_DBG(ERR, "Invalid Arguments\n");
		return -EINVAL;
	}
	ulp_ctx = bp->ulp_ctx;

	if (!ulp_ctx->cfg_data)
		return 0;

	/* TBD call TF_session_detach */

	/* Increment the ulp context data reference count usage. */
	if (ulp_ctx->cfg_data->ref_cnt >= 1) {
		ulp_ctx->cfg_data->ref_cnt--;
		if (ulp_ctx_deinit_allowed(bp))
			ulp_ctx_deinit(bp, session);
		ulp_ctx->cfg_data = NULL;
		ulp_ctx->g_tfp = NULL;
		return 0;
	}
	BNXT_TF_DBG(ERR, "context deatach on invalid data\n");
	return 0;
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
ulp_get_session(struct rte_pci_addr *pci_addr)
{
	struct bnxt_ulp_session_state *session;

	STAILQ_FOREACH(session, &bnxt_ulp_session_list, next) {
		if (session->pci_info.domain == pci_addr->domain &&
		    session->pci_info.bus == pci_addr->bus) {
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

	if (!bp)
		return NULL;

	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;

	pthread_mutex_lock(&bnxt_ulp_global_mutex);

	session = ulp_get_session(pci_addr);
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
			pthread_mutex_init(&session->bnxt_ulp_mutex, NULL);
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
	struct tf_global_cfg_parms parms;

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

/*
 * When a port is initialized by dpdk. This functions is called
 * and this function initializes the ULP context and rest of the
 * infrastructure associated with it.
 */
int32_t
bnxt_ulp_init(struct bnxt *bp)
{
	struct bnxt_ulp_session_state *session;
	bool init;
	int rc;

	if (!BNXT_TRUFLOW_EN(bp))
		return 0;

	if (bp->ulp_ctx) {
		BNXT_TF_DBG(DEBUG, "ulp ctx already allocated\n");
		return -EINVAL;
	}

	/*
	 * Multiple uplink ports can be associated with a single vswitch.
	 * Make sure only the port that is started first will initialize
	 * the TF session.
	 */
	session = ulp_session_init(bp, &init);
	if (!session) {
		BNXT_TF_DBG(ERR, "Failed to initialize the tf session\n");
		return -EINVAL;
	}

	bp->ulp_ctx = rte_zmalloc("bnxt_ulp_ctx",
				  sizeof(struct bnxt_ulp_context), 0);
	if (!bp->ulp_ctx) {
		BNXT_TF_DBG(ERR, "Failed to allocate ulp ctx\n");
		ulp_session_deinit(session);
		return -ENOMEM;
	}

	/*
	 * If ULP is already initialized for a specific domain then simply
	 * assign the ulp context to this rte_eth_dev.
	 */
	if (init) {
		rc = ulp_ctx_attach(bp->ulp_ctx, session);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to attach the ulp context\n");
			ulp_session_deinit(session);
			rte_free(bp->ulp_ctx);
			return rc;
		}

		/* Update bnxt driver flags */
		rc = ulp_dparms_dev_port_intf_update(bp, bp->ulp_ctx);
		if (rc) {
			BNXT_TF_DBG(ERR, "Failed to update driver flags\n");
			ulp_ctx_detach(bp, session);
			ulp_session_deinit(session);
			rte_free(bp->ulp_ctx);
			return rc;
		}

		/* update the port database */
		rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp->eth_dev);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to update port database\n");
			ulp_ctx_detach(bp, session);
			ulp_session_deinit(session);
			rte_free(bp->ulp_ctx);
		}
		return rc;
	}

	/* Allocate and Initialize the ulp context. */
	rc = ulp_ctx_init(bp, session);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	/* Initialize ulp dparms with values devargs passed */
	rc = ulp_dparms_init(bp, bp->ulp_ctx);

	/* create the port database */
	rc = ulp_port_db_init(bp->ulp_ctx, bp->port_cnt);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the port database\n");
		goto jump_to_error;
	}

	/* Update bnxt driver flags */
	rc = ulp_dparms_dev_port_intf_update(bp, bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to update driver flags\n");
		goto jump_to_error;
	}

	/* update the port database */
	rc = ulp_port_db_dev_port_intf_update(bp->ulp_ctx, bp->eth_dev);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to update port database\n");
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
					(BNXT_ULP_NAT_INNER_L2_HEADER_SMAC |
					BNXT_ULP_NAT_INNER_L2_HEADER_DMAC), 1);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to set rx global configuration\n");
		goto jump_to_error;
	}

	rc = bnxt_ulp_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
					TF_TUNNEL_ENCAP_NAT,
					(BNXT_ULP_NAT_INNER_L2_HEADER_SMAC |
					BNXT_ULP_NAT_INNER_L2_HEADER_DMAC), 1);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to set tx global configuration\n");
		goto jump_to_error;
	}

	return rc;

jump_to_error:
	bnxt_ulp_deinit(bp);
	return -ENOMEM;
}

/* Below are the access functions to access internal data of ulp context. */

/*
 * When a port is deinit'ed by dpdk. This function is called
 * and this function clears the ULP context and rest of the
 * infrastructure associated with it.
 */
void
bnxt_ulp_deinit(struct bnxt *bp)
{
	struct bnxt_ulp_session_state	*session;
	struct rte_pci_device		*pci_dev;
	struct rte_pci_addr		*pci_addr;

	if (!BNXT_TRUFLOW_EN(bp))
		return;

	/* Get the session first */
	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;
	pthread_mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(pci_addr);
	pthread_mutex_unlock(&bnxt_ulp_global_mutex);

	/* session not found then just exit */
	if (!session)
		return;

	/* clean up default flows */
	bnxt_ulp_destroy_df_rules(bp, true);

	/* clean up regular flows */
	ulp_flow_db_flush_flows(bp->ulp_ctx, BNXT_ULP_REGULAR_FLOW_TABLE);

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
					 (BNXT_ULP_NAT_INNER_L2_HEADER_SMAC |
					  BNXT_ULP_NAT_INNER_L2_HEADER_DMAC),
					 0);

	(void)bnxt_ulp_global_cfg_update(bp, TF_DIR_TX, TF_TUNNEL_ENCAP,
					 TF_TUNNEL_ENCAP_NAT,
					 (BNXT_ULP_NAT_INNER_L2_HEADER_SMAC |
					  BNXT_ULP_NAT_INNER_L2_HEADER_DMAC),
					 0);

	/* Delete the ulp context and tf session */
	ulp_ctx_detach(bp, session);

	/* Finally delete the bnxt session*/
	ulp_session_deinit(session);

	rte_free(bp->ulp_ctx);
}

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

/* Function to set the tfp session details from the ulp context. */
int32_t
bnxt_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp, struct tf *tfp)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return -EINVAL;
	}

	/* TBD The tfp should be removed once tf_attach is implemented. */
	ulp->g_tfp = tfp;
	return 0;
}

/* Function to get the tfp session details from the ulp context. */
struct tf *
bnxt_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp)
{
	if (!ulp) {
		BNXT_TF_DBG(ERR, "Invalid arguments\n");
		return NULL;
	}
	/* TBD The tfp should be removed once tf_attach is implemented. */
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

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_eth_dev_ptr2_cntxt_get(struct rte_eth_dev	*dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	if (BNXT_ETH_DEV_IS_REPRESENTOR(dev)) {
		struct bnxt_vf_representor *vfr = dev->data->dev_private;

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
