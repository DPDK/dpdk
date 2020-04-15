/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <stdio.h>

#include "tf_core.h"
#include "tf_session.h"
#include "tf_tbl.h"
#include "tf_em.h"
#include "tf_rm.h"
#include "tf_msg.h"
#include "tfp.h"
#include "bitalloc.h"
#include "bnxt.h"
#include "rand.h"

static inline uint32_t SWAP_WORDS32(uint32_t val32)
{
	return (((val32 & 0x0000ffff) << 16) |
		((val32 & 0xffff0000) >> 16));
}

static void tf_seeds_init(struct tf_session *session)
{
	int i;
	uint32_t r;

	/* Initialize the lfsr */
	rand_init();

	/* RX and TX use the same seed values */
	session->lkup_lkup3_init_cfg[TF_DIR_RX] =
		session->lkup_lkup3_init_cfg[TF_DIR_TX] =
						SWAP_WORDS32(rand32());

	for (i = 0; i < TF_LKUP_SEED_MEM_SIZE / 2; i++) {
		r = SWAP_WORDS32(rand32());
		session->lkup_em_seed_mem[TF_DIR_RX][i * 2] = r;
		session->lkup_em_seed_mem[TF_DIR_TX][i * 2] = r;
		r = SWAP_WORDS32(rand32());
		session->lkup_em_seed_mem[TF_DIR_RX][i * 2 + 1] = (r & 0x1);
		session->lkup_em_seed_mem[TF_DIR_TX][i * 2 + 1] = (r & 0x1);
	}
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

	if (tfp == NULL || parms == NULL)
		return -EINVAL;

	/* Filter out any non-supported device types on the Core
	 * side. It is assumed that the Firmware will be supported if
	 * firmware open session succeeds.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_WH)
		return -ENOTSUP;

	/* Build the beginning of session_id */
	rc = sscanf(parms->ctrl_chan_name,
		    "%x:%x:%x.%d",
		    &domain,
		    &bus,
		    &slot,
		    &device);
	if (rc != 4) {
		PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR,
				    "Session is already open, rc:%d\n",
				    rc);
		else
			PMD_DRV_LOG(ERR,
				    "Open message send failed, rc:%d\n",
				    rc);

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
		PMD_DRV_LOG(ERR,
			    "Failed to allocate session info, rc:%d\n",
			    rc);
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
		PMD_DRV_LOG(ERR,
			    "Failed to allocate session data, rc:%d\n",
			    rc);
		goto cleanup;
	}

	tfp->session->core_data = alloc_parms.mem_va;

	session = (struct tf_session *)tfp->session->core_data;
	tfp_memcpy(session->ctrl_chan_name,
		   parms->ctrl_chan_name,
		   TF_SESSION_NAME_MAX);

	/* Initialize Session */
	session->device_type = parms->device_type;
	tf_rm_init(tfp);

	/* Construct the Session ID */
	session->session_id.internal.domain = domain;
	session->session_id.internal.bus = bus;
	session->session_id.internal.device = device;
	session->session_id.internal.fw_session_id = fw_session_id;

	rc = tf_msg_session_qcfg(tfp);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR,
			    "Query config message send failed, rc:%d\n",
			    rc);
		goto cleanup_close;
	}

	/* Shadow DB configuration */
	if (parms->shadow_copy) {
		/* Ignore shadow_copy setting */
		session->shadow_copy = 0;/* parms->shadow_copy; */
#if (TF_SHADOW == 1)
		rc = tf_rm_shadow_db_init(tfs);
		if (rc)
			PMD_DRV_LOG(ERR,
				    "Shadow DB Initialization failed\n, rc:%d",
				    rc);
		/* Add additional processing */
#endif /* TF_SHADOW */
	}

	/* Adjust the Session with what firmware allowed us to get */
	rc = tf_rm_allocate_validate(tfp);
	if (rc) {
		/* Log error */
		goto cleanup_close;
	}

	/* Setup hash seeds */
	tf_seeds_init(session);

	/* Initialize external pool data structures */
	tf_init_tbl_pool(session);

	session->ref_count++;

	/* Return session ID */
	parms->session_id = session->session_id;

	PMD_DRV_LOG(INFO,
		    "Session created, session_id:%d\n",
		    parms->session_id.id);

	PMD_DRV_LOG(INFO,
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
tf_attach_session(struct tf *tfp __rte_unused,
		  struct tf_attach_session_parms *parms __rte_unused)
{
#if (TF_SHARED == 1)
	int rc;

	if (tfp == NULL)
		return -EINVAL;

	/* - Open the shared memory for the attach_chan_name
	 * - Point to the shared session for this Device instance
	 * - Check that session is valid
	 * - Attach to the firmware so it can record there is more
	 *   than one client of the session.
	 */

	if (tfp->session) {
		if (tfp->session->session_id.id != TF_SESSION_ID_INVALID) {
			rc = tf_msg_session_attach(tfp,
						   parms->ctrl_chan_name,
						   parms->session_id);
		}
	}
#endif /* TF_SHARED */
	return -1;
}

int
tf_close_session(struct tf *tfp)
{
	int rc;
	int rc_close = 0;
	struct tf_session *tfs;
	union tf_session_id session_id;

	if (tfp == NULL || tfp->session == NULL)
		return -EINVAL;

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
			PMD_DRV_LOG(ERR,
				    "Message send failed, rc:%d\n",
				    rc);
		}

		/* Update the ref_count */
		tfs->ref_count--;
	}

	session_id = tfs->session_id;

	/* Final cleanup as we're last user of the session */
	if (tfs->ref_count == 0) {
		tfp_free(tfp->session->core_data);
		tfp_free(tfp->session);
		tfp->session = NULL;
	}

	PMD_DRV_LOG(INFO,
		    "Session closed, session_id:%d\n",
		    session_id.id);

	PMD_DRV_LOG(INFO,
		    "domain:%d, bus:%d, device:%d, fw_session_id:%d\n",
		    session_id.internal.domain,
		    session_id.internal.bus,
		    session_id.internal.device,
		    session_id.internal.fw_session_id);

	return rc_close;
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
	struct tf_tbl_scope_cb     *tbl_scope_cb;

	if (tfp == NULL || parms == NULL)
		return -EINVAL;

	tbl_scope_cb =
		tbl_scope_cb_find((struct tf_session *)tfp->session->core_data,
				  parms->tbl_scope_id);
	if (tbl_scope_cb == NULL)
		return -EINVAL;

	/* Process the EM entry per Table Scope type */
	return tf_insert_eem_entry((struct tf_session *)tfp->session->core_data,
				   tbl_scope_cb,
				   parms);
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
	struct tf_tbl_scope_cb     *tbl_scope_cb;

	if (tfp == NULL || parms == NULL)
		return -EINVAL;

	tbl_scope_cb =
		tbl_scope_cb_find((struct tf_session *)tfp->session->core_data,
				  parms->tbl_scope_id);
	if (tbl_scope_cb == NULL)
		return -EINVAL;

	return tf_delete_eem_entry(tfp, parms);
}

/** allocate identifier resource
 *
 * Returns success or failure code.
 */
int tf_alloc_identifier(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms)
{
	struct bitalloc *session_pool;
	struct tf_session *tfs;
	int id;
	int rc;

	if (parms == NULL || tfp == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR, "%s: session error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

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
		PMD_DRV_LOG(ERR, "%s: unsupported %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	default:
		PMD_DRV_LOG(ERR, "%s: %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EINVAL;
		break;
	}

	if (rc) {
		PMD_DRV_LOG(ERR, "%s: identifier pool %s failure\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		return rc;
	}

	id = ba_alloc(session_pool);

	if (id == BA_FAIL) {
		PMD_DRV_LOG(ERR, "%s: %s: No resource available\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		return -ENOMEM;
	}
	parms->id = id;
	return 0;
}

/** free identifier resource
 *
 * Returns success or failure code.
 */
int tf_free_identifier(struct tf *tfp,
		       struct tf_free_identifier_parms *parms)
{
	struct bitalloc *session_pool;
	int rc;
	int ba_rc;
	struct tf_session *tfs;

	if (parms == NULL || tfp == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR, "%s: Session error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

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
		PMD_DRV_LOG(ERR, "%s: unsupported %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EOPNOTSUPP;
		break;
	default:
		PMD_DRV_LOG(ERR, "%s: invalid %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		rc = -EINVAL;
		break;
	}
	if (rc) {
		PMD_DRV_LOG(ERR, "%s: %s Identifier pool access failed\n",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type));
		return rc;
	}

	ba_rc = ba_inuse(session_pool, (int)parms->id);

	if (ba_rc == BA_FAIL || ba_rc == BA_ENTRY_FREE) {
		PMD_DRV_LOG(ERR, "%s: %s: Entry %d already free",
			    tf_dir_2_str(parms->dir),
			    tf_ident_2_str(parms->ident_type),
			    parms->id);
		return -EINVAL;
	}

	ba_free(session_pool, (int)parms->id);

	return 0;
}

int
tf_alloc_tcam_entry(struct tf *tfp,
		    struct tf_alloc_tcam_entry_parms *parms)
{
	int rc;
	int index;
	struct tf_session *tfs;
	struct bitalloc *session_pool;

	if (parms == NULL || tfp == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR, "%s: session error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	rc = tf_rm_lookup_tcam_type_pool(tfs,
					 parms->dir,
					 parms->tcam_tbl_type,
					 &session_pool);
	/* Error logging handled by tf_rm_lookup_tcam_type_pool */
	if (rc)
		return rc;

	index = ba_alloc(session_pool);
	if (index == BA_FAIL) {
		PMD_DRV_LOG(ERR, "%s: %s: No resource available\n",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->tcam_tbl_type));
		return -ENOMEM;
	}

	parms->idx = index;
	return 0;
}

int
tf_set_tcam_entry(struct tf *tfp,
		  struct tf_set_tcam_entry_parms *parms)
{
	int rc;
	int id;
	struct tf_session *tfs;
	struct bitalloc *session_pool;

	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "%s, Session info invalid\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	/*
	 * Each tcam send msg function should check for key sizes range
	 */

	rc = tf_rm_lookup_tcam_type_pool(tfs,
					 parms->dir,
					 parms->tcam_tbl_type,
					 &session_pool);
	/* Error logging handled by tf_rm_lookup_tcam_type_pool */
	if (rc)
		return rc;


	/* Verify that the entry has been previously allocated */
	id = ba_inuse(session_pool, parms->idx);
	if (id != 1) {
		PMD_DRV_LOG(ERR,
		   "%s: %s: Invalid or not allocated index, idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   tf_tcam_tbl_2_str(parms->tcam_tbl_type),
		   parms->idx);
		return -EINVAL;
	}

	rc = tf_msg_tcam_entry_set(tfp, parms);

	return rc;
}

int
tf_get_tcam_entry(struct tf *tfp __rte_unused,
		  struct tf_get_tcam_entry_parms *parms __rte_unused)
{
	int rc = -EOPNOTSUPP;

	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "%s, Session info invalid\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	return rc;
}

int
tf_free_tcam_entry(struct tf *tfp,
		   struct tf_free_tcam_entry_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct bitalloc *session_pool;

	if (parms == NULL || tfp == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR, "%s: Session error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	rc = tf_rm_lookup_tcam_type_pool(tfs,
					 parms->dir,
					 parms->tcam_tbl_type,
					 &session_pool);
	/* Error logging handled by tf_rm_lookup_tcam_type_pool */
	if (rc)
		return rc;

	rc = ba_inuse(session_pool, (int)parms->idx);
	if (rc == BA_FAIL || rc == BA_ENTRY_FREE) {
		PMD_DRV_LOG(ERR, "%s: %s: Entry %d already free",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->tcam_tbl_type),
			    parms->idx);
		return -EINVAL;
	}

	ba_free(session_pool, (int)parms->idx);

	rc = tf_msg_tcam_entry_free(tfp, parms);
	if (rc) {
		/* Log error */
		PMD_DRV_LOG(ERR, "%s: %s: Entry %d free failed",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->tcam_tbl_type),
			    parms->idx);
	}

	return rc;
}
