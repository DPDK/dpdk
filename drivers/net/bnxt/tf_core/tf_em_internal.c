/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_ext_flow_handle.h"
#include "bnxt.h"

#define TF_EM_DB_EM_REC 0

/**
 * EM Pool
 */
#include "dpool.h"

/**
 * Insert EM internal entry API
 *
 *  returns:
 *     0 - Success
 */
int
tf_em_insert_int_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms)
{
	int rc;
	uint32_t gfid;
	uint16_t rptr_index = 0;
	uint8_t rptr_entry = 0;
	uint8_t num_of_entries = 0;
	struct tf_session *tfs;
	struct dpool *pool;
	uint32_t index;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	pool = (struct dpool *)tfs->em_pool[parms->dir];
	index = dpool_alloc(pool, TF_SESSION_EM_ENTRY_SIZE, 0);
	if (index == DP_INVALID_INDEX) {
		PMD_DRV_LOG(ERR,
			    "%s, EM entry index allocation failed\n",
			    tf_dir_2_str(parms->dir));
		return -1;
	}


	rptr_index = index;
	rc = tf_msg_insert_em_internal_entry(tfp,
					     parms,
					     &rptr_index,
					     &rptr_entry,
					     &num_of_entries);
	if (rc) {
		/* Free the allocated index before returning */
		dpool_free(pool, index);
		return -1;
	}
	TF_SET_GFID(gfid,
		    ((rptr_index << TF_EM_INTERNAL_INDEX_SHIFT) |
		     rptr_entry),
		    0); /* N/A for internal table */

	TF_SET_FLOW_ID(parms->flow_id,
		       gfid,
		       TF_GFID_TABLE_INTERNAL,
		       parms->dir);

	TF_SET_FIELDS_IN_FLOW_HANDLE(parms->flow_handle,
				     (uint32_t)num_of_entries,
				     0,
				     TF_FLAGS_FLOW_HANDLE_INTERNAL,
				     rptr_index,
				     rptr_entry,
				     0);
	return 0;
}


/** Delete EM internal entry API
 *
 * returns:
 * 0
 * -EINVAL
 */
int
tf_em_delete_int_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs;
	struct dpool *pool;
	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_delete_em_entry(tfp, parms);

	/* Return resource to pool */
	if (rc == 0) {
		pool = (struct dpool *)tfs->em_pool[parms->dir];
		dpool_free(pool, parms->index);
	}

	return rc;
}

static int
tf_em_move_callback(void *user_data,
		    uint64_t entry_data,
		    uint32_t new_index)
{
	int rc;
	struct tf *tfp = (struct tf *)user_data;
	struct tf_move_em_entry_parms parms;
	struct tf_dev_info     *dev;
	struct tf_session      *tfs;

	memset(&parms, 0, sizeof(parms));

	parms.tbl_scope_id = 0;
	parms.flow_handle  = entry_data;
	parms.new_index    = new_index;
	TF_GET_DIR_FROM_FLOW_ID(entry_data, parms.dir);
	parms.mem          = TF_MEM_INTERNAL;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms.dir),
			    strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup device, rc:%s\n",
			    tf_dir_2_str(parms.dir),
			    strerror(-rc));
		return rc;
	}

	if (dev->ops->tf_dev_move_int_em_entry != NULL)
		rc = dev->ops->tf_dev_move_int_em_entry(tfp, &parms);
	else
		rc = -EOPNOTSUPP;

	return rc;
}

int
tf_em_int_bind(struct tf *tfp,
	       struct tf_em_cfg_parms *parms)
{
	int rc;
	int db_rc[TF_DIR_MAX] = { 0 };
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct tf_rm_get_alloc_info_parms iparms;
	struct tf_rm_alloc_info info;
	struct em_rm_db *em_db;
	struct tfp_calloc_parms cparms;
	struct tf_session *tfs;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	memset(&db_cfg, 0, sizeof(db_cfg));
	cparms.nitems = 1;
	cparms.size = sizeof(struct em_rm_db);
	cparms.alignment = 0;
	if (tfp_calloc(&cparms) != 0) {
		TFP_DRV_LOG(ERR, "em_rm_db alloc error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}

	em_db = cparms.mem_va;
	for (i = 0; i < TF_DIR_MAX; i++)
		em_db->em_db[i] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_EM, em_db);

	db_cfg.module = TF_MODULE_TYPE_EM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->em_cnt[i].cnt;

		/* Check if we got any request to support EEM, if so
		 * we build an EM Int DB holding Table Scopes.
		 */
		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_EM_RECORD] == 0)
			continue;

		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_EM_RECORD] %
		    TF_SESSION_EM_ENTRY_SIZE != 0) {
			rc = -ENOMEM;
			TFP_DRV_LOG(ERR,
				    "%s, EM Allocation must be in blocks of %d, failure %s\n",
				    tf_dir_2_str(i),
				    TF_SESSION_EM_ENTRY_SIZE,
				    strerror(-rc));

			return rc;
		}

		db_cfg.rm_db = (void *)&em_db->em_db[i];
		if (tf_session_is_shared_session(tfs) &&
			(!tf_session_is_shared_session_creator(tfs)))
			db_rc[i] = tf_rm_create_db_no_reservation(tfp, &db_cfg);
		else
			db_rc[i] = tf_rm_create_db(tfp, &db_cfg);
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		TFP_DRV_LOG(ERR, "EM Int DB creation failed\n");
		return db_rc[TF_DIR_RX];
	}


	if (!tf_session_is_shared_session(tfs)) {
		for (i = 0; i < TF_DIR_MAX; i++) {
			iparms.rm_db = em_db->em_db[i];
			iparms.subtype = TF_EM_DB_EM_REC;
			iparms.info = &info;

			rc = tf_rm_get_info(&iparms);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "%s: EM DB get info failed\n",
					    tf_dir_2_str(i));
				return rc;
			}

			/*
			 * Allocate stack pool
			 */
			cparms.nitems = 1;
			cparms.size = sizeof(struct dpool);
			cparms.alignment = 0;

			rc = tfp_calloc(&cparms);

			if (rc) {
				TFP_DRV_LOG(ERR,
					 "%s, EM stack allocation failure %s\n",
					 tf_dir_2_str(i),
					 strerror(-rc));
				return rc;
			}

			tfs->em_pool[i] = (struct dpool *)cparms.mem_va;

			rc = dpool_init(tfs->em_pool[i],
					iparms.info->entry.start,
					iparms.info->entry.stride,
					7,
					(void *)tfp,
					tf_em_move_callback);
			/* Logging handled in tf_create_em_pool */
			if (rc)
				return rc;
		}

		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: EM pool init failed\n",
				    tf_dir_2_str(i));
			return rc;
		}
	}

	return 0;
}

int
tf_em_int_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };
	struct em_rm_db *em_db;
	void *em_db_ptr = NULL;
	struct tf_session *tfs;

	TF_CHECK_PARMS1(tfp);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	if (!tf_session_is_shared_session(tfs)) {
		for (i = 0; i < TF_DIR_MAX; i++) {
			if (tfs->em_pool[i] == NULL)
				continue;
			dpool_free_all(tfs->em_pool[i]);
		}
	}

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_EM, &em_db_ptr);
	if (rc) {
		return 0;
	}
	em_db = (struct em_rm_db *)em_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (em_db->em_db[i] == NULL)
			continue;
		fparms.dir = i;
		fparms.rm_db = em_db->em_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		em_db->em_db[i] = NULL;
	}

	return 0;
}

int
tf_em_get_resc_info(struct tf *tfp,
		    struct tf_em_resource_info *em)
{
	int rc;
	int d;
	struct tf_resource_info *dinfo;
	struct tf_rm_get_alloc_info_parms ainfo;
	void *em_db_ptr = NULL;
	struct em_rm_db *em_db;

	TF_CHECK_PARMS2(tfp, em);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_EM, &em_db_ptr);
	if (rc == -ENOMEM)
		return 0;  /* db does not exist */
	else if (rc)
		return rc; /* db error */

	em_db = (struct em_rm_db *)em_db_ptr;

	/* check if reserved resource for EM is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = em_db->em_db[d];
		dinfo = em[d].info;

		if (!ainfo.rm_db)
			continue;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_EM_TBL_TYPE_MAX);
		if (rc && rc != -ENOTSUP)
			return rc;
	}

	return 0;
}
