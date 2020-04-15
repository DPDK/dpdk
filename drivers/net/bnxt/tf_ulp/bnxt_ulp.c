/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "bnxt.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_flow_db.h"

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

	if (&bp->tfp == bp->ulp_ctx.g_tfp)
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

	memset(&params, 0, sizeof(params));

	rc = rte_eth_dev_get_name_by_port(ethdev->data->port_id,
					  params.ctrl_chan_name);
	if (rc) {
		BNXT_TF_DBG(ERR, "Invalid port %d, rc = %d\n",
			    ethdev->data->port_id, rc);
		return rc;
	}

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
	bp->ulp_ctx.g_tfp = NULL;
}

static void
bnxt_init_tbl_scope_parms(struct bnxt *bp,
			  struct tf_alloc_tbl_scope_parms *params)
{
	struct bnxt_ulp_device_params	*dparms;
	uint32_t dev_id;
	int rc;

	rc = bnxt_ulp_cntxt_dev_id_get(&bp->ulp_ctx, &dev_id);
	if (rc)
		/* TBD: For now, just use default. */
		dparms = 0;
	else
		dparms = bnxt_ulp_device_params_get(dev_id);

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
		params->rx_num_flows_in_k = dparms->num_flows / (1024);
		params->rx_tbl_if_id = BNXT_ULP_RX_TBL_IF_ID;

		params->tx_max_key_sz_in_bits = BNXT_ULP_DFLT_TX_MAX_KEY;
		params->tx_max_action_entry_sz_in_bits =
			BNXT_ULP_DFLT_TX_MAX_ACTN_ENTRY;
		params->tx_mem_size_in_mb = BNXT_ULP_DFLT_TX_MEM;
		params->tx_num_flows_in_k = dparms->num_flows / (1024);
		params->tx_tbl_if_id = BNXT_ULP_TX_TBL_IF_ID;
	}
}

/* Initialize Extended Exact Match host memory. */
static int32_t
ulp_eem_tbl_scope_init(struct bnxt *bp)
{
	struct tf_alloc_tbl_scope_parms params = {0};
	int rc;

	bnxt_init_tbl_scope_parms(bp, &params);

	rc = tf_alloc_tbl_scope(&bp->tfp, &params);
	if (rc) {
		BNXT_TF_DBG(ERR, "Unable to allocate eem table scope rc = %d\n",
			    rc);
		return rc;
	}

	rc = bnxt_ulp_cntxt_tbl_scope_id_set(&bp->ulp_ctx, params.tbl_scope_id);
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
		bp->ulp_ctx.cfg_data = NULL;
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
	bp->ulp_ctx.cfg_data = ulp_data;
	session->cfg_data = ulp_data;
	ulp_data->ref_cnt++;

	/* Open the ulp session. */
	rc = ulp_ctx_session_open(bp, session);
	if (rc) {
		(void)ulp_ctx_deinit(bp, session);
		return rc;
	}
	bnxt_ulp_cntxt_tfp_set(&bp->ulp_ctx, session->g_tfp);
	return rc;
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
	ulp_ctx = &bp->ulp_ctx;

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

	/*
	 * If ULP is already initialized for a specific domain then simply
	 * assign the ulp context to this rte_eth_dev.
	 */
	if (init) {
		rc = ulp_ctx_attach(&bp->ulp_ctx, session);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Failed to attach the ulp context\n");
		}
		return rc;
	}

	/* Allocate and Initialize the ulp context. */
	rc = ulp_ctx_init(bp, session);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the ulp context\n");
		goto jump_to_error;
	}

	/* Create the Mark database. */
	rc = ulp_mark_db_init(&bp->ulp_ctx);
	if (rc) {
		BNXT_TF_DBG(ERR, "Failed to create the mark database\n");
		goto jump_to_error;
	}

	/* Create the flow database. */
	rc = ulp_flow_db_init(&bp->ulp_ctx);
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

	/* Get the session first */
	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;
	pthread_mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(pci_addr);
	pthread_mutex_unlock(&bnxt_ulp_global_mutex);

	/* session not found then just exit */
	if (!session)
		return;

	/* cleanup the eem table scope */
	ulp_eem_tbl_scope_deinit(bp, &bp->ulp_ctx);

	/* cleanup the flow database */
	ulp_flow_db_deinit(&bp->ulp_ctx);

	/* Delete the Mark database */
	ulp_mark_db_deinit(&bp->ulp_ctx);

	/* Delete the ulp context and tf session */
	ulp_ctx_detach(bp, session);

	/* Finally delete the bnxt session*/
	ulp_session_deinit(session);
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
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->flow_db = flow_db;
	return 0;
}

/* Function to get the flow database from the ulp context. */
struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (!ulp_ctx || !ulp_ctx->cfg_data) {
		BNXT_TF_DBG(ERR, "Invalid ulp context data\n");
		return NULL;
	}

	return ulp_ctx->cfg_data->flow_db;
}

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_eth_dev_ptr2_cntxt_get(struct rte_eth_dev	*dev)
{
	struct bnxt	*bp;

	bp = (struct bnxt *)dev->data->dev_private;
	if (!bp) {
		BNXT_TF_DBG(ERR, "Bnxt private data is not initialized\n");
		return NULL;
	}
	return &bp->ulp_ctx;
}
