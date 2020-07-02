/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <stdio.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_session.h"
#include "tf_tbl.h"
#include "tf_em.h"
#include "tf_rm.h"
#include "tf_msg.h"
#include "tfp.h"
#include "bitalloc.h"
#include "bnxt.h"
#include "rand.h"
#include "tf_common.h"
#include "hwrm_tf.h"

/**
 * Create EM Tbl pool of memory indexes.
 *
 * [in] session
 *   Pointer to session
 * [in] dir
 *   direction
 * [in] num_entries
 *   number of entries to write
 *
 * Return:
 *  0       - Success, entry allocated - no search support
 *  -ENOMEM -EINVAL -EOPNOTSUPP
 *          - Failure, entry not allocated, out of resources
 */
static int
tf_create_em_pool(struct tf_session *session,
		  enum tf_dir dir,
		  uint32_t num_entries)
{
	struct tfp_calloc_parms parms;
	uint32_t i, j;
	int rc = 0;
	struct stack *pool = &session->em_pool[dir];

	parms.nitems = num_entries;
	parms.size = sizeof(uint32_t);
	parms.alignment = 0;

	if (tfp_calloc(&parms) != 0) {
		TFP_DRV_LOG(ERR, "EM pool allocation failure %s\n",
			    strerror(-ENOMEM));
		return -ENOMEM;
	}

	/* Create empty stack
	 */
	rc = stack_init(num_entries, (uint32_t *)parms.mem_va, pool);

	if (rc != 0) {
		TFP_DRV_LOG(ERR, "EM pool stack init failure %s\n",
			    strerror(-rc));
		goto cleanup;
	}

	/* Fill pool with indexes
	 */
	j = num_entries - 1;

	for (i = 0; i < num_entries; i++) {
		rc = stack_push(pool, j);
		if (rc != 0) {
			TFP_DRV_LOG(ERR, "EM pool stack push failure %s\n",
				    strerror(-rc));
			goto cleanup;
		}
		j--;
	}

	if (!stack_is_full(pool)) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR, "EM pool stack failure %s\n",
			    strerror(-rc));
		goto cleanup;
	}

	return 0;
cleanup:
	tfp_free((void *)parms.mem_va);
	return rc;
}

/**
 * Create EM Tbl pool of memory indexes.
 *
 * [in] session
 *   Pointer to session
 * [in] dir
 *   direction
 *
 * Return:
 */
static void
tf_free_em_pool(struct tf_session *session,
		enum tf_dir dir)
{
	struct stack *pool = &session->em_pool[dir];
	uint32_t *ptr;

	ptr = stack_items(pool);

	tfp_free(ptr);
}

int
tf_open_session(struct tf                    *tfp,
		struct tf_open_session_parms *parms)
{
	int rc;
	struct tf_session *session;
	struct tfp_calloc_parms alloc_parms;
	unsigned int domain, bus, slot, device;
	uint8_t fw_session_id;
	int dir;

	TF_CHECK_PARMS(tfp, parms);

	/* Filter out any non-supported device types on the Core
	 * side. It is assumed that the Firmware will be supported if
	 * firmware open session succeeds.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_WH) {
		TFP_DRV_LOG(ERR,
			    "Unsupported device type %d\n",
			    parms->device_type);
		return -ENOTSUP;
	}

	/* Build the beginning of session_id */
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		TFP_DRV_LOG(ERR,
			    "Failed to scan device ctrl_chan_name\n");
		return -EINVAL;
	}

	/* open FW session and get a new session_id */
	rc = tf_msg_session_open(tfp,
				 parms->ctrl_chan_name,
				 &fw_session_id);
	if (rc) {
		/* Log error */
		if (rc == -EEXIST)
			TFP_DRV_LOG(ERR,
				    "Session is already open, rc:%s\n",
				    strerror(-rc));
		else
			TFP_DRV_LOG(ERR,
				    "Open message send failed, rc:%s\n",
				    strerror(-rc));

		parms->session_id.id = TF_FW_SESSION_ID_INVALID;
		return rc;
	}

	/* Allocate session */
	alloc_parms.nitems = 1;
	alloc_parms.size = sizeof(struct tf_session_info);
	alloc_parms.alignment = 0;
	rc = tfp_calloc(&alloc_parms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session info, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}

	tfp->session = (struct tf_session_info *)alloc_parms.mem_va;

	/* Allocate core data for the session */
	alloc_parms.nitems = 1;
	alloc_parms.size = sizeof(struct tf_session);
	alloc_parms.alignment = 0;
	rc = tfp_calloc(&alloc_parms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session data, rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}

	tfp->session->core_data = alloc_parms.mem_va;

	session = (struct tf_session *)tfp->session->core_data;
	tfp_memcpy(session->ctrl_chan_name,
		   parms->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	/* Initialize Session */
	session->dev = NULL;
	tf_rm_init(tfp);

	/* Construct the Session ID */
	session->session_id.internal.domain = domain;
	session->session_id.internal.bus = bus;
	session->session_id.internal.device = device;
	session->session_id.internal.fw_session_id = fw_session_id;

	/* Query for Session Config
	 */
	rc = tf_msg_session_qcfg(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Query config message send failed, rc:%s\n",
			    strerror(-rc));
		goto cleanup_close;
	}

	/* Shadow DB configuration */
	if (parms->shadow_copy) {
		/* Ignore shadow_copy setting */
		session->shadow_copy = 0;/* parms->shadow_copy; */
#if (TF_SHADOW == 1)
		rc = tf_rm_shadow_db_init(tfs);
		if (rc)
			TFP_DRV_LOG(ERR,
				    "Shadow DB Initialization failed\n, rc:%s",
				    strerror(-rc));
		/* Add additional processing */
#endif /* TF_SHADOW */
	}

	/* Adjust the Session with what firmware allowed us to get */
	rc = tf_rm_allocate_validate(tfp);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Rm allocate validate failed, rc:%s\n",
			    strerror(-rc));
		goto cleanup_close;
	}

	/* Initialize EM pool */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = tf_create_em_pool(session,
				       (enum tf_dir)dir,
				       TF_SESSION_EM_POOL_SIZE);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EM Pool initialization failed\n");
			goto cleanup_close;
		}
	}

	session->ref_count++;

	/* Return session ID */
	parms->session_id = session->session_id;

	TFP_DRV_LOG(INFO,
		    "Session created, session_id:%d\n",
		    parms->session_id.id);

	TFP_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    parms->session_id.internal.domain,
		    parms->session_id.internal.bus,
		    parms->session_id.internal.device,
		    parms->session_id.internal.fw_session_id);

	return 0;

 cleanup:
	tfp_free(tfp->session->core_data);
	tfp_free(tfp->session);
	tfp->session = NULL;
	return rc;

 cleanup_close:
	tf_close_session(tfp);
	return -EINVAL;
}

int
tf_open_session_new(struct tf *tfp,
		    struct tf_open_session_parms *parms)
{
	int rc;
	unsigned int domain, bus, slot, device;
	struct tf_session_open_session_parms oparms;

	TF_CHECK_PARMS(tfp, parms);

	/* Filter out any non-supported device types on the Core
	 * side. It is assumed that the Firmware will be supported if
	 * firmware open session succeeds.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_WH) {
		TFP_DRV_LOG(ERR,
			    "Unsupported device type %d\n",
			    parms->device_type);
		return -ENOTSUP;
	}

	/* Verify control channel and build the beginning of session_id */
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		TFP_DRV_LOG(ERR,
			    "Failed to scan device ctrl_chan_name\n");
		return -EINVAL;
	}

	parms->session_id.internal.domain = domain;
	parms->session_id.internal.bus = bus;
	parms->session_id.internal.device = device;
	oparms.open_cfg = parms;

	rc = tf_session_open_session(tfp, &oparms);
	/* Logging handled by tf_session_open_session */
	if (rc)
		return rc;

	TFP_DRV_LOG(INFO,
		    "Session created, session_id:%d\n",
		    parms->session_id.id);

	TFP_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    parms->session_id.internal.domain,
		    parms->session_id.internal.bus,
		    parms->session_id.internal.device,
		    parms->session_id.internal.fw_session_id);

	return 0;
}

int
tf_attach_session(struct tf *tfp __rte_unused,
		  struct tf_attach_session_parms *parms __rte_unused)
{
#if (TF_SHARED == 1)
	int rc;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	/* - Open the shared memory for the attach_chan_name
	 * - Point to the shared session for this Device instance
	 * - Check that session is valid
	 * - Attach to the firmware so it can record there is more
	 *   than one client of the session.
	 */

	if (tfp->session->session_id.id != TF_SESSION_ID_INVALID) {
		rc = tf_msg_session_attach(tfp,
					   parms->ctrl_chan_name,
					   parms->session_id);
	}
#endif /* TF_SHARED */
	return -1;
}

int
tf_attach_session_new(struct tf *tfp,
		      struct tf_attach_session_parms *parms)
{
	int rc;
	unsigned int domain, bus, slot, device;
	struct tf_session_attach_session_parms aparms;

	TF_CHECK_PARMS2(tfp, parms);

	/* Verify control channel */
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		TFP_DRV_LOG(ERR,
			    "Failed to scan device ctrl_chan_name\n");
		return -EINVAL;
	}

	/* Verify 'attach' channel */
	rc = sscanf(parms->attach_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		TFP_DRV_LOG(ERR,
			    "Failed to scan device attach_chan_name\n");
		return -EINVAL;
	}

	/* Prepare return value of session_id, using ctrl_chan_name
	 * device values as it becomes the session id.
	 */
	parms->session_id.internal.domain = domain;
	parms->session_id.internal.bus = bus;
	parms->session_id.internal.device = device;
	aparms.attach_cfg = parms;
	rc = tf_session_attach_session(tfp,
				       &aparms);
	/* Logging handled by dev_bind */
	if (rc)
		return rc;

	TFP_DRV_LOG(INFO,
		    "Attached to session, session_id:%d\n",
		    parms->session_id.id);

	TFP_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    parms->session_id.internal.domain,
		    parms->session_id.internal.bus,
		    parms->session_id.internal.device,
		    parms->session_id.internal.fw_session_id);

	return rc;
}

int
tf_close_session(struct tf *tfp)
{
	int rc;
	int rc_close = 0;
	struct tf_session *tfs;
	union tf_session_id session_id;
	int dir;

	TF_CHECK_TFP_SESSION(tfp);

	tfs = (struct tf_session *)(tfp->session->core_data);

	/* Cleanup if we're last user of the session */
	if (tfs->ref_count == 1) {
		/* Cleanup any outstanding resources */
		rc_close = tf_rm_close(tfp);
	}

	if (tfs->session_id.id != TF_SESSION_ID_INVALID) {
		rc = tf_msg_session_close(tfp);
		if (rc) {
			/* Log error */
			TFP_DRV_LOG(ERR,
				    "Message send failed, rc:%s\n",
				    strerror(-rc));
		}

		/* Update the ref_count */
		tfs->ref_count--;
	}

	session_id = tfs->session_id;

	/* Final cleanup as we're last user of the session */
	if (tfs->ref_count == 0) {
		/* Free EM pool */
		for (dir = 0; dir < TF_DIR_MAX; dir++)
			tf_free_em_pool(tfs, (enum tf_dir)dir);

		tfp_free(tfp->session->core_data);
		tfp_free(tfp->session);
		tfp->session = NULL;
	}

	TFP_DRV_LOG(INFO,
		    "Session closed, session_id:%d\n",
		    session_id.id);

	TFP_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    session_id.internal.domain,
		    session_id.internal.bus,
		    session_id.internal.device,
		    session_id.internal.fw_session_id);

	return rc_close;
}

int
tf_close_session_new(struct tf *tfp)
{
	int rc;
	struct tf_session_close_session_parms cparms = { 0 };
	union tf_session_id session_id = { 0 };
	uint8_t ref_count;

	TF_CHECK_PARMS1(tfp);

	cparms.ref_count = &ref_count;
	cparms.session_id = &session_id;
	rc = tf_session_close_session(tfp,
				      &cparms);
	/* Logging handled by tf_session_close_session */
	if (rc)
		return rc;

	TFP_DRV_LOG(INFO,
		    "Closed session, session_id:%d, ref_count:%d\n",
		    cparms.session_id->id,
		    *cparms.ref_count);

	TFP_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    cparms.session_id->internal.domain,
		    cparms.session_id->internal.bus,
		    cparms.session_id->internal.device,
		    cparms.session_id->internal.fw_session_id);

	return rc;
}

/** insert EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_insert_em_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms)
{
	struct tf_session      *tfs;
	struct tf_dev_info     *dev;
	int rc;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = dev->ops->tf_dev_insert_em_entry(tfp, parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: EM insert failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	return -EINVAL;
}

/** Delete EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_delete_em_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms)
{
	struct tf_session      *tfs;
	struct tf_dev_info     *dev;
	int rc;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = dev->ops->tf_dev_delete_em_entry(tfp, parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: EM delete failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	return rc;
}

int tf_alloc_identifier(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms)
{
	struct bitalloc *session_pool;
	struct tf_session *tfs;
	int id;
	int rc;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	tfs = (struct tf_session *)(tfp->session->core_data);

	switch (parms->ident_type) {
	case TF_IDENT_TYPE_L2_CTXT:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_L2_CTXT_REMAP_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_PROF_FUNC:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_PROF_FUNC_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_EM_PROF:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_EM_PROF_ID_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_WC_PROF:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_WC_TCAM_PROF_ID_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_L2_FUNC:
		TFP_DRV_LOG(ERR, "%s: unsupported %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	default:
		TFP_DRV_LOG(ERR, "%s: %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	}

	if (rc) {
		TFP_DRV_LOG(ERR, "%s: identifier pool %s failure, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type),
			    strerror(-rc));
		return rc;
	}

	id = ba_alloc(session_pool);

	if (id == BA_FAIL) {
		TFP_DRV_LOG(ERR, "%s: %s: No resource available\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		return -ENOMEM;
	}
	parms->id = id;
	return 0;
}

int
tf_alloc_identifier_new(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_ident_alloc_parms aparms;
	uint16_t id;

	TF_CHECK_PARMS2(tfp, parms);

	/* Can't do static initialization due to UT enum check */
	memset(&aparms, 0, sizeof(struct tf_ident_alloc_parms));

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_alloc_ident == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return -EOPNOTSUPP;
	}

	aparms.dir = parms->dir;
	aparms.ident_type = parms->ident_type;
	aparms.id = &id;
	rc = dev->ops->tf_dev_alloc_ident(tfp, &aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Identifier allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	parms->id = id;

	return 0;
}

int tf_free_identifier(struct tf *tfp,
		       struct tf_free_identifier_parms *parms)
{
	struct bitalloc *session_pool;
	int rc;
	int ba_rc;
	struct tf_session *tfs;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	tfs = (struct tf_session *)(tfp->session->core_data);

	switch (parms->ident_type) {
	case TF_IDENT_TYPE_L2_CTXT:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_L2_CTXT_REMAP_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_PROF_FUNC:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_PROF_FUNC_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_EM_PROF:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_EM_PROF_ID_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_WC_PROF:
		TF_RM_GET_POOLS(tfs, parms->dir, &session_pool,
				TF_WC_TCAM_PROF_ID_POOL_NAME,
				rc);
		break;
	case TF_IDENT_TYPE_L2_FUNC:
		TFP_DRV_LOG(ERR, "%s: unsupported %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	default:
		TFP_DRV_LOG(ERR, "%s: invalid %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	}
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: %s Identifier pool access failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type),
			    strerror(-rc));
		return rc;
	}

	ba_rc = ba_inuse(session_pool, (int)parms->id);

	if (ba_rc == BA_FAIL || ba_rc == BA_ENTRY_FREE) {
		TFP_DRV_LOG(ERR, "%s: %s: Entry %d already free",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type),
			    parms->id);
		return -EINVAL;
	}

	ba_free(session_pool, (int)parms->id);

	return 0;
}

int
tf_free_identifier_new(struct tf *tfp,
		       struct tf_free_identifier_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_ident_free_parms fparms;

	TF_CHECK_PARMS2(tfp, parms);

	/* Can't do static initialization due to UT enum check */
	memset(&fparms, 0, sizeof(struct tf_ident_free_parms));

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_free_ident == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return -EOPNOTSUPP;
	}

	fparms.dir = parms->dir;
	fparms.ident_type = parms->ident_type;
	fparms.id = parms->id;
	rc = dev->ops->tf_dev_free_ident(tfp, &fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Identifier allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_alloc_tcam_entry(struct tf *tfp,
		    struct tf_alloc_tcam_entry_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_tcam_alloc_parms aparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_alloc_tcam == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	aparms.dir = parms->dir;
	aparms.type = parms->tcam_tbl_type;
	aparms.key_size = TF_BITS2BYTES_WORD_ALIGN(parms->key_sz_in_bits);
	aparms.priority = parms->priority;
	rc = dev->ops->tf_dev_alloc_tcam(tfp, &aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	parms->idx = aparms.idx;

	return 0;
}

int
tf_set_tcam_entry(struct tf *tfp,
		  struct tf_set_tcam_entry_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_tcam_set_parms sparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_set_tcam == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	sparms.dir = parms->dir;
	sparms.type = parms->tcam_tbl_type;
	sparms.idx = parms->idx;
	sparms.key = parms->key;
	sparms.mask = parms->mask;
	sparms.key_size = TF_BITS2BYTES_WORD_ALIGN(parms->key_sz_in_bits);
	sparms.result = parms->result;
	sparms.result_size = TF_BITS2BYTES_WORD_ALIGN(parms->result_sz_in_bits);

	rc = dev->ops->tf_dev_set_tcam(tfp, &sparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM set failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_get_tcam_entry(struct tf *tfp __rte_unused,
		  struct tf_get_tcam_entry_parms *parms __rte_unused)
{
	TF_CHECK_PARMS_SESSION(tfp, parms);
	return -EOPNOTSUPP;
}

int
tf_free_tcam_entry(struct tf *tfp,
		   struct tf_free_tcam_entry_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_tcam_free_parms fparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_free_tcam == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	fparms.dir = parms->dir;
	fparms.type = parms->tcam_tbl_type;
	fparms.idx = parms->idx;
	rc = dev->ops->tf_dev_free_tcam(tfp, &fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	return 0;
}
