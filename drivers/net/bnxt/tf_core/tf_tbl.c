/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <rte_common.h>

#include "tf_tbl.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_shadow_tbl.h"
#include "tf_session.h"
#include "tf_device.h"


struct tf;

/**
 * Table DBs.
 */
static void *tbl_db[TF_DIR_MAX];

/**
 * Table Shadow DBs
 */
static void *shadow_tbl_db[TF_DIR_MAX];

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
static uint8_t shadow_init;

int
tf_tbl_bind(struct tf *tfp,
	    struct tf_tbl_cfg_parms *parms)
{
	int rc, d, i;
	struct tf_rm_alloc_info info;
	struct tf_rm_free_db_parms fparms;
	struct tf_shadow_tbl_free_db_parms fshadow;
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_shadow_tbl_cfg_parms shadow_cfg;
	struct tf_shadow_tbl_create_db_parms shadow_cdb;
	struct tf_rm_create_db_parms db_cfg = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (init) {
		TFP_DRV_LOG(ERR,
			    "Table DB already initialized\n");
		return -EINVAL;
	}

	db_cfg.num_elements = parms->num_elements;
	db_cfg.type = TF_DEVICE_MODULE_TYPE_TABLE;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (d = 0; d < TF_DIR_MAX; d++) {
		db_cfg.dir = d;
		db_cfg.alloc_cnt = parms->resources->tbl_cnt[d].cnt;
		db_cfg.rm_db = &tbl_db[d];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Table DB creation failed\n",
				    tf_dir_2_str(d));

			return rc;
		}
	}

	/* Initialize the Shadow Table. */
	if (parms->shadow_copy) {
		for (d = 0; d < TF_DIR_MAX; d++) {
			memset(&shadow_cfg, 0, sizeof(shadow_cfg));
			memset(&shadow_cdb, 0, sizeof(shadow_cdb));
			/* Get the base addresses of the tables */
			for (i = 0; i < TF_TBL_TYPE_MAX; i++) {
				memset(&info, 0, sizeof(info));

				if (!parms->resources->tbl_cnt[d].cnt[i])
					continue;
				ainfo.rm_db = tbl_db[d];
				ainfo.db_index = i;
				ainfo.info = &info;
				rc = tf_rm_get_info(&ainfo);
				if (rc)
					goto error;

				shadow_cfg.base_addr[i] = info.entry.start;
			}

			/* Create the shadow db */
			shadow_cfg.alloc_cnt =
				parms->resources->tbl_cnt[d].cnt;
			shadow_cfg.num_entries = parms->num_elements;

			shadow_cdb.shadow_db = &shadow_tbl_db[d];
			shadow_cdb.cfg = &shadow_cfg;
			rc = tf_shadow_tbl_create_db(&shadow_cdb);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "Shadow TBL DB creation failed "
					    "rc=%d\n", rc);
				goto error;
			}
		}
		shadow_init = 1;
	}

	init = 1;

	TFP_DRV_LOG(INFO,
		    "Table Type - initialized\n");

	return 0;
error:
	for (d = 0; d < TF_DIR_MAX; d++) {
		memset(&fparms, 0, sizeof(fparms));
		fparms.dir = d;
		fparms.rm_db = tbl_db[d];
		/* Ignoring return here since we are in the error case */
		(void)tf_rm_free_db(tfp, &fparms);

		if (parms->shadow_copy) {
			fshadow.shadow_db = shadow_tbl_db[d];
			tf_shadow_tbl_free_db(&fshadow);
			shadow_tbl_db[d] = NULL;
		}

		tbl_db[d] = NULL;
	}

	shadow_init = 0;
	init = 0;

	return rc;
}

int
tf_tbl_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };
	struct tf_shadow_tbl_free_db_parms fshadow;

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized */
	if (!init) {
		TFP_DRV_LOG(INFO,
			    "No Table DBs created\n");
		return 0;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		fparms.dir = i;
		fparms.rm_db = tbl_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		tbl_db[i] = NULL;

		if (shadow_init) {
			memset(&fshadow, 0, sizeof(fshadow));
			fshadow.shadow_db = shadow_tbl_db[i];
			tf_shadow_tbl_free_db(&fshadow);
			shadow_tbl_db[i] = NULL;
		}
	}

	init = 0;
	shadow_init = 0;

	return 0;
}

int
tf_tbl_alloc(struct tf *tfp __rte_unused,
	     struct tf_tbl_alloc_parms *parms)
{
	int rc;
	uint32_t idx;
	struct tf_rm_allocate_parms aparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Allocate requested element */
	aparms.rm_db = tbl_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = &idx;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed allocate, type:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type);
		return rc;
	}

	*parms->idx = idx;

	return 0;
}

int
tf_tbl_free(struct tf *tfp __rte_unused,
	    struct tf_tbl_free_parms *parms)
{
	int rc;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct tf_shadow_tbl_remove_parms shparms;
	int allocated = 0;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Check if element is in use */
	aparms.rm_db = tbl_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry already free, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->idx);
		return -EINVAL;
	}

	/*
	 * The Shadow mgmt, if enabled, determines if the entry needs
	 * to be deleted.
	 */
	if (shadow_init) {
		memset(&shparms, 0, sizeof(shparms));
		shparms.shadow_db = shadow_tbl_db[parms->dir];
		shparms.fparms = parms;
		rc = tf_shadow_tbl_remove(&shparms);
		if (rc) {
			/*
			 * Should not get here, log it and let the entry be
			 * deleted.
			 */
			TFP_DRV_LOG(ERR, "%s: Shadow free fail, "
				    "type:%d index:%d deleting the entry.\n",
				    tf_dir_2_str(parms->dir),
				    parms->type,
				    parms->idx);
		} else {
			/*
			 * If the entry still has references, just return the
			 * ref count to the caller.  No need to remove entry
			 * from rm.
			 */
			if (parms->ref_cnt >= 1)
				return rc;
		}
	}

	/* Free requested element */
	fparms.rm_db = tbl_db[parms->dir];
	fparms.db_index = parms->type;
	fparms.index = parms->idx;
	rc = tf_rm_free(&fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Free failed, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->idx);
		return rc;
	}

	return 0;
}

int
tf_tbl_alloc_search(struct tf *tfp,
		    struct tf_tbl_alloc_search_parms *parms)
{
	int rc, frc;
	uint32_t idx;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_tbl_alloc_parms aparms;
	struct tf_shadow_tbl_search_parms sparms;
	struct tf_shadow_tbl_bind_index_parms bparms;
	struct tf_tbl_free_parms fparms;

	TF_CHECK_PARMS2(tfp, parms);

	if (!shadow_init || !shadow_tbl_db[parms->dir]) {
		TFP_DRV_LOG(ERR, "%s: Shadow TBL not initialized.\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	memset(&sparms, 0, sizeof(sparms));
	sparms.sparms = parms;
	sparms.shadow_db = shadow_tbl_db[parms->dir];
	rc = tf_shadow_tbl_search(&sparms);
	if (rc)
		return rc;

	/*
	 * The app didn't request us to alloc the entry, so return now.
	 * The hit should have been updated in the original search parm.
	 */
	if (!parms->alloc || parms->search_status != MISS)
		return rc;

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

	/* Allocate the index */
	if (dev->ops->tf_dev_alloc_tbl == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return -EOPNOTSUPP;
	}

	memset(&aparms, 0, sizeof(aparms));
	aparms.dir = parms->dir;
	aparms.type = parms->type;
	aparms.tbl_scope_id = parms->tbl_scope_id;
	aparms.idx = &idx;
	rc = dev->ops->tf_dev_alloc_tbl(tfp, &aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Table allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Bind the allocated index to the data */
	memset(&bparms, 0, sizeof(bparms));
	bparms.shadow_db = shadow_tbl_db[parms->dir];
	bparms.dir = parms->dir;
	bparms.type = parms->type;
	bparms.idx = idx;
	bparms.data = parms->result;
	bparms.data_sz_in_bytes = parms->result_sz_in_bytes;
	bparms.hb_handle = sparms.hb_handle;
	rc = tf_shadow_tbl_bind_index(&bparms);
	if (rc) {
		/* Error binding entry, need to free the allocated idx */
		if (dev->ops->tf_dev_free_tbl == NULL) {
			rc = -EOPNOTSUPP;
			TFP_DRV_LOG(ERR,
				    "%s: Operation not supported, rc:%s\n",
				    tf_dir_2_str(parms->dir),
				    strerror(-rc));
			return rc;
		}

		memset(&fparms, 0, sizeof(fparms));
		fparms.dir = parms->dir;
		fparms.type = parms->type;
		fparms.idx = idx;
		frc = dev->ops->tf_dev_free_tbl(tfp, &fparms);
		if (frc) {
			TFP_DRV_LOG(ERR,
				    "%s: Failed free index allocated during "
				    "search. rc=%s\n",
				    tf_dir_2_str(parms->dir),
				    strerror(-frc));
			/* return the original failure. */
			return rc;
		}
	}

	parms->idx = idx;

	return rc;
}

int
tf_tbl_set(struct tf *tfp,
	   struct tf_tbl_set_parms *parms)
{
	int rc;
	int allocated = 0;
	uint16_t hcapi_type;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Verify that the entry has been previously allocated */
	aparms.rm_db = tbl_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
		   "%s, Invalid or not allocated index, type:%d, idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type,
		   parms->idx);
		return -EINVAL;
	}

	/* Set the entry */
	hparms.rm_db = tbl_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_set_tbl_entry(tfp,
				  parms->dir,
				  hcapi_type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Set failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_tbl_get(struct tf *tfp,
	   struct tf_tbl_get_parms *parms)
{
	int rc;
	uint16_t hcapi_type;
	int allocated = 0;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Verify that the entry has been previously allocated */
	aparms.rm_db = tbl_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
		   "%s, Invalid or not allocated index, type:%d, idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type,
		   parms->idx);
		return -EINVAL;
	}

	/* Set the entry */
	hparms.rm_db = tbl_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
		return rc;
	}

	/* Get the entry */
	rc = tf_msg_get_tbl_entry(tfp,
				  parms->dir,
				  hcapi_type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Get failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_tbl_bulk_get(struct tf *tfp,
		struct tf_tbl_get_bulk_parms *parms)
{
	int rc;
	uint16_t hcapi_type;
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tf_rm_check_indexes_in_range_parms cparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));

		return -EINVAL;
	}

	/* Verify that the entries are in the range of reserved resources. */
	cparms.rm_db = tbl_db[parms->dir];
	cparms.db_index = parms->type;
	cparms.starting_index = parms->starting_idx;
	cparms.num_entries = parms->num_entries;

	rc = tf_rm_check_indexes_in_range(&cparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Invalid or %d index starting from %d"
			    " not in range, type:%d",
			    tf_dir_2_str(parms->dir),
			    parms->starting_idx,
			    parms->num_entries,
			    parms->type);
		return rc;
	}

	hparms.rm_db = tbl_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
		return rc;
	}

	/* Get the entries */
	rc = tf_msg_bulk_get_tbl_entry(tfp,
				       parms->dir,
				       hcapi_type,
				       parms->starting_idx,
				       parms->num_entries,
				       parms->entry_sz_in_bytes,
				       parms->physical_mem_addr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Bulk get failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
	}

	return rc;
}
