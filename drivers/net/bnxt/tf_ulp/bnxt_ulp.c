/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
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
#include "bnxt_ulp_utils.h"
#include "bnxt_tf_common.h"
#include "bnxt_hwrm.h"
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

bool
ulp_is_default_session_active(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(ulp_ctx == NULL || ulp_ctx->g_tfp[0] == NULL))
		return false;

	return true;
}

/*
 * Allow the deletion of context only for the bnxt device that
 * created the session.
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return false;

	if (!ulp_ctx->cfg_data->ref_cnt) {
		BNXT_DRV_DBG(DEBUG, "ulp ctx shall initiate deinit\n");
		return true;
	}

	return false;
}

/* The function to initialize bp flags with truflow features */
static int32_t
ulp_dparms_dev_port_intf_update(struct bnxt *bp,
				struct bnxt_ulp_context *ulp_ctx)
{
	enum bnxt_ulp_flow_mem_type mtype;

	if (unlikely(bnxt_ulp_cntxt_mem_type_get(ulp_ctx, &mtype)))
		return -EINVAL;
	/* Update the bp flag with gfid flag */
	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_EXT)
		bp->flags |= BNXT_FLAG_GFID_ENABLE;

	return 0;
}

int32_t
ulp_ctx_mh_get_session_name(struct bnxt *bp,
			    struct tf_open_session_parms *parms)
{
	int32_t	rc = 0;
	unsigned int domain = 0, bus = 0, slot = 0, device = 0;
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%u",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		/* PCI Domain not provided (optional in DPDK), thus we
		 * force domain to 0 and recheck.
		 */
		domain = 0;
		/* Check parsing of bus/slot/device */
		rc = sscanf(parms->ctrl_chan_name,
			    "%x:%x.%u",
			    &bus,
			    &slot,
			    &device);
		if (unlikely(rc != 3)) {
			BNXT_DRV_DBG(DEBUG,
				    "Failed to scan device ctrl_chan_name\n");
			return -EINVAL;
		}
	}

	/* change domain name for multi-host system */
	domain = domain + (0xf & bp->multi_host_pf_pci_id);
	sprintf(parms->ctrl_chan_name,
		"%x:%x:%x.%u",
		domain,
		bus,
		slot,
		device);
	BNXT_DRV_DBG(DEBUG,
		    "Session name for Multi-Host: ctrl_chan_name:%s\n", parms->ctrl_chan_name);
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
ulp_get_session(struct bnxt *bp, struct rte_pci_addr *pci_addr)
{
	struct bnxt_ulp_session_state *session;

	/* if multi root capability is enabled, then ignore the pci bus id */
	STAILQ_FOREACH(session, &bnxt_ulp_session_list, next) {
		if (BNXT_MULTIROOT_EN(bp)) {
			if (!memcmp(bp->dsn, session->dsn,
				    sizeof(session->dsn))) {
				return session;
			}
		} else if (session->pci_info.domain == pci_addr->domain &&
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

	session = ulp_get_session(bp, pci_addr);
	if (!session) {
		/* Not Found the session  Allocate a new one */
		session = rte_zmalloc("bnxt_ulp_session",
				      sizeof(struct bnxt_ulp_session_state),
				      0);
		if (!session) {
			BNXT_DRV_DBG(ERR,
				    "Allocation failed for bnxt_ulp_session\n");
			pthread_mutex_unlock(&bnxt_ulp_global_mutex);
			return NULL;

		} else {
			/* Add it to the queue */
			session->pci_info.domain = pci_addr->domain;
			session->pci_info.bus = pci_addr->bus;
			memcpy(session->dsn, bp->dsn, sizeof(session->dsn));
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

/* Internal function to delete all the flows belonging to the given port */
static void
bnxt_ulp_flush_port_flows(struct bnxt *bp)
{
	uint16_t func_id;

	/* it is assumed that port is either TVF or PF */
	if (unlikely(ulp_port_db_port_func_id_get(bp->ulp_ctx,
						  bp->eth_dev->data->port_id,
						  &func_id))) {
		BNXT_DRV_DBG(ERR, "Invalid argument\n");
		return;
	}
	(void)ulp_flow_db_function_flow_flush(bp->ulp_ctx, func_id);
}

/* Internal function to delete the VFR default flows */
void
bnxt_ulp_destroy_vfr_default_rules(struct bnxt *bp, bool global)
{
	struct bnxt_ulp_vfr_rule_info *info;
	uint16_t port_id;
	struct rte_eth_dev *vfr_eth_dev;
	struct bnxt_representor *vfr_bp;

	if (unlikely(!BNXT_TRUFLOW_EN(bp) ||
		     BNXT_ETH_DEV_IS_REPRESENTOR(bp->eth_dev)))
		return;

	if (unlikely(!bp->ulp_ctx || !bp->ulp_ctx->cfg_data))
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

static int
ulp_l2_etype_tunnel_alloc(struct bnxt *bp)
{
	int rc = 0;

	if (!ULP_APP_L2_ETYPE_SUPPORT(bp->ulp_ctx))
		return rc;

	if (bp->l2_etype_tunnel_cnt) {
		BNXT_DRV_DBG(DEBUG, "L2 ETYPE Custom Tunnel already allocated\n");
		return rc;
	}
	rc = bnxt_tunnel_dst_port_alloc(bp,
					BNXT_L2_ETYPE_TUNNEL_ID,
					HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_L2_ETYPE);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Failed to set global L2 ETYPE Custom Tunnel\n");
	else
		bp->l2_etype_tunnel_cnt++;

	return rc;
}

static const struct bnxt_ulp_core_ops *
bnxt_ulp_port_func_ops_get(struct bnxt *bp)
{
	int32_t rc;
	enum bnxt_ulp_device_id  dev_id;
	const struct bnxt_ulp_core_ops *func_ops;

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (unlikely(rc))
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &bnxt_ulp_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_STINGRAY:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		func_ops = &bnxt_ulp_tf_core_ops;
		break;
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
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
	uint32_t ulp_flags;
	int32_t rc = 0;
	enum bnxt_ulp_device_id dev_id;

	if (!BNXT_TRUFLOW_EN(bp)) {
		BNXT_DRV_DBG(DEBUG,
			     "Skip ulp init for port: %d, TF is not enabled\n",
			     bp->eth_dev->data->port_id);
		return rc;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		BNXT_DRV_DBG(DEBUG,
			     "Skip ulp init for port: %d, not a TVF or PF\n",
			     bp->eth_dev->data->port_id);
		return rc;
	}

	rc = bnxt_ulp_devid_get(bp, &dev_id);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(DEBUG, "Unsupported device %x\n", rc);
		return rc;
	}

	if (unlikely(bp->ulp_ctx)) {
		BNXT_DRV_DBG(DEBUG, "ulp ctx already allocated\n");
		return rc;
	}

	bp->ulp_ctx = rte_zmalloc("bnxt_ulp_ctx",
				  sizeof(struct bnxt_ulp_context), 0);
	if (unlikely(!bp->ulp_ctx)) {
		BNXT_DRV_DBG(ERR, "Failed to allocate ulp ctx\n");
		return -ENOMEM;
	}

	rc = bnxt_ulp_cntxt_bp_set(bp->ulp_ctx, bp);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to set bp in ulp_ctx\n");
		rte_free(bp->ulp_ctx);
		return -EIO;
	}

	/* This shouldn't fail, unless we have a unknown device */
	bp->ulp_ctx->ops = bnxt_ulp_port_func_ops_get(bp);
	if (unlikely(!bp->ulp_ctx->ops)) {
		BNXT_DRV_DBG(ERR, "Failed to get ulp ops\n");
		rte_free(bp->ulp_ctx);
		return -EIO;
	}

	/*
	 * Multiple uplink ports can be associated with a single vswitch.
	 * Make sure only the port that is started first will initialize
	 * the TF session.
	 */
	session = ulp_session_init(bp, &initialized);
	if (unlikely(!session)) {
		BNXT_DRV_DBG(ERR, "Failed to initialize the tf session\n");
		rc = -EIO;
		goto jump_to_error;
	}

	if (initialized) {
		/*
		 * If ULP is already initialized for a specific domain then
		 * simply assign the ulp context to this rte_eth_dev.
		 */
		rc = bp->ulp_ctx->ops->ulp_ctx_attach(bp, session);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to attach the ulp context\n");
			goto jump_to_error;
		}
	} else {
		rc = bp->ulp_ctx->ops->ulp_init(bp, session);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to initialize the ulp init\n");
			goto jump_to_error;
		}
	}

	/* setup the l2 etype tunnel for custom l2 encap/decap */
	rc = ulp_l2_etype_tunnel_alloc(bp);
	if (unlikely(rc))
		goto jump_to_error;


	/* Update bnxt driver flags */
	rc = ulp_dparms_dev_port_intf_update(bp, bp->ulp_ctx);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to update driver flags\n");
		goto jump_to_error;
	}

	/* update the port database for the given interface */
	rc = ulp_port_db_port_update(bp->ulp_ctx, bp->eth_dev);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to update port database\n");
		goto jump_to_error;
	}

	/* create the default rules */
	rc = bnxt_ulp_create_df_rules(bp);
	if (unlikely(rc)) {
		BNXT_DRV_DBG(ERR, "Failed to create default flow\n");
		goto jump_to_error;
	}

	/* set the unicast mode */
	if (unlikely(bnxt_ulp_cntxt_ptr2_ulp_flags_get(bp->ulp_ctx, &ulp_flags))) {
		BNXT_DRV_DBG(ERR, "Error in getting ULP context flags\n");
		goto jump_to_error;
	}
	if (ulp_flags & BNXT_ULP_APP_UNICAST_ONLY) {
		if (unlikely(bnxt_pmd_set_unicast_rxmask(bp->eth_dev))) {
			BNXT_DRV_DBG(ERR, "Error in setting unicast rxmode\n");
			goto jump_to_error;
		}
	}

	/* Make sure that custom header data is selected */
	if (dev_id > BNXT_ULP_DEVICE_ID_WH_PLUS) {
		struct bnxt_vnic_info *vnic = bp->vnic_info;
		vnic->metadata_format = HWRM_VNIC_UPDATE_INPUT_METADATA_FORMAT_TYPE_3;
		rc = bnxt_hwrm_vnic_update(bp,
					vnic,
					HWRM_VNIC_UPDATE_INPUT_ENABLES_METADATA_FORMAT_TYPE_VALID);
		if (unlikely(rc)) {
			BNXT_DRV_DBG(ERR, "Failed to set metadata format\n");
			goto jump_to_error;
		}
	}

	rc = ulp_l2_etype_tunnel_alloc(bp);
	if (unlikely(rc))
		goto jump_to_error;

	return rc;

jump_to_error:
	bnxt_ulp_port_deinit(bp);
	return rc;
}

static void
ulp_l2_etype_tunnel_free(struct bnxt *bp)
{
	int rc;

	if (!ULP_APP_L2_ETYPE_SUPPORT(bp->ulp_ctx))
		return;

	if (unlikely(bp->l2_etype_tunnel_cnt == 0)) {
		BNXT_DRV_DBG(DEBUG, "L2 ETYPE Custom Tunnel already freed\n");
		return;
	}
	rc = bnxt_tunnel_dst_port_free(bp,
				       BNXT_L2_ETYPE_TUNNEL_ID,
				       HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_L2_ETYPE);
	if (unlikely(rc))
		BNXT_DRV_DBG(ERR, "Failed to clear L2 ETYPE Custom Tunnel\n");

	bp->l2_etype_tunnel_cnt--;
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

	if (unlikely(!BNXT_TRUFLOW_EN(bp))) {
		BNXT_DRV_DBG(DEBUG,
			     "Skip ULP deinit for port:%d, TF is not enabled\n",
			     bp->eth_dev->data->port_id);
		return;
	}

	if (unlikely(!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp))) {
		BNXT_DRV_DBG(DEBUG,
			     "Skip ULP deinit port:%d, not a TVF or PF\n",
			     bp->eth_dev->data->port_id);
		return;
	}

	if (unlikely(!bp->ulp_ctx)) {
		BNXT_DRV_DBG(DEBUG, "ulp ctx already de-allocated\n");
		return;
	}

	BNXT_DRV_DBG(DEBUG, "BNXT Port:%d ULP port deinit\n",
		     bp->eth_dev->data->port_id);

	/* Get the session details  */
	pci_dev = RTE_DEV_TO_PCI(bp->eth_dev->device);
	pci_addr = &pci_dev->addr;
	pthread_mutex_lock(&bnxt_ulp_global_mutex);
	session = ulp_get_session(bp, pci_addr);
	pthread_mutex_unlock(&bnxt_ulp_global_mutex);

	/* session not found then just exit */
	if (unlikely(!session)) {
		/* Free the ulp context */
		rte_free(bp->ulp_ctx);
		bp->ulp_ctx = NULL;
		return;
	}

	/* Check the reference count to deinit or deattach*/
	if (bp->ulp_ctx->cfg_data && bp->ulp_ctx->cfg_data->ref_cnt) {
		bp->ulp_ctx->cfg_data->ref_cnt--;
		/* Free tunnels for each port */
		ulp_l2_etype_tunnel_free(bp);
		if (bp->ulp_ctx->cfg_data->ref_cnt) {
			/* Free the ulp context in the context entry list */
			bnxt_ulp_cntxt_list_del(bp->ulp_ctx);

			/* free the port details */
			/* Free the default flow rule associated to this port */
			bnxt_ulp_destroy_df_rules(bp, false);
			bnxt_ulp_destroy_vfr_default_rules(bp, false);

			/* free flows associated with this port */
			bnxt_ulp_flush_port_flows(bp);

			/* close the session associated with this port */
			bp->ulp_ctx->ops->ulp_ctx_detach(bp, session);
		} else {
			/* Free the ulp context in the context entry list */
			bnxt_ulp_cntxt_list_del(bp->ulp_ctx);

			/* clean up default flows */
			bnxt_ulp_destroy_df_rules(bp, true);

			/* clean up default VFR flows */
			bnxt_ulp_destroy_vfr_default_rules(bp, true);

			/* clean up regular flows */
			ulp_flow_db_flush_flows(bp->ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR);

			/* Perform ulp ctx deinit */
			bp->ulp_ctx->ops->ulp_deinit(bp, session);
		}
	}

	/* clean up the session */
	ulp_session_deinit(session);

	/* Free the ulp context */
	rte_free(bp->ulp_ctx);
	bp->ulp_ctx = NULL;
}

int32_t
bnxt_ulp_cntxt_list_init(void)
{
	/* Create the cntxt spin lock only once*/
	if (!bnxt_ulp_ctxt_lock_created)
		rte_spinlock_init(&bnxt_ulp_ctxt_lock);
	bnxt_ulp_ctxt_lock_created = 1;
	return 0;
}

int32_t
bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx)
{
	struct ulp_context_list_entry	*entry;

	entry = rte_zmalloc(NULL, sizeof(struct ulp_context_list_entry), 0);
	if (unlikely(entry == NULL)) {
		BNXT_DRV_DBG(ERR, "unable to allocate memory\n");
		return -ENOMEM;
	}

	rte_spinlock_lock(&bnxt_ulp_ctxt_lock);
	entry->ulp_ctx = ulp_ctx;
	TAILQ_INSERT_TAIL(&ulp_cntx_list, entry, next);
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
	return 0;
}

void
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

int
bnxt_ulp_cntxt_list_count(void)
{
	struct ulp_context_list_entry *entry, *temp;
	int count_1 = 0;

	rte_spinlock_lock(&bnxt_ulp_ctxt_lock);
	RTE_TAILQ_FOREACH_SAFE(entry, &ulp_cntx_list, next, temp) {
		count_1++;
	}
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
	return count_1;
}

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_acquire(void *arg)
{
	struct ulp_context_list_entry	*entry;

	/* take a lock and get the first ulp context available */
	if (rte_spinlock_trylock(&bnxt_ulp_ctxt_lock)) {
		TAILQ_FOREACH(entry, &ulp_cntx_list, next) {
			if (entry->ulp_ctx->cfg_data == arg)
				return entry->ulp_ctx;
		}
		rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
	}
	return NULL;
}

void
bnxt_ulp_cntxt_entry_release(void)
{
	rte_spinlock_unlock(&bnxt_ulp_ctxt_lock);
}
