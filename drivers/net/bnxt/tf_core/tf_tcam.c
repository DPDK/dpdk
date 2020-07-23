/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>

#include "tf_tcam.h"
#include "tf_common.h"
#include "tf_util.h"
#include "tf_rm.h"
#include "tf_device.h"
#include "tfp.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tf_shadow_tcam.h"

struct tf;

/**
 * TCAM DBs.
 */
static void *tcam_db[TF_DIR_MAX];

/**
 * TCAM Shadow DBs
 */
static void *shadow_tcam_db[TF_DIR_MAX];

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
static uint8_t shadow_init;

int
tf_tcam_bind(struct tf *tfp,
	     struct tf_tcam_cfg_parms *parms)
{
	int rc;
	int i, d;
	struct tf_rm_alloc_info info;
	struct tf_rm_free_db_parms fparms;
	struct tf_rm_create_db_parms db_cfg;
	struct tf_tcam_resources *tcam_cnt;
	struct tf_shadow_tcam_free_db_parms fshadow;
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_shadow_tcam_cfg_parms shadow_cfg;
	struct tf_shadow_tcam_create_db_parms shadow_cdb;

	TF_CHECK_PARMS2(tfp, parms);

	if (init) {
		TFP_DRV_LOG(ERR,
			    "TCAM DB already initialized\n");
		return -EINVAL;
	}

	tcam_cnt = parms->resources->tcam_cnt;
	if ((tcam_cnt[TF_DIR_RX].cnt[TF_TCAM_TBL_TYPE_WC_TCAM] % 2) ||
	    (tcam_cnt[TF_DIR_TX].cnt[TF_TCAM_TBL_TYPE_WC_TCAM] % 2)) {
		TFP_DRV_LOG(ERR,
			    "Number of WC TCAM entries cannot be odd num\n");
		return -EINVAL;
	}

	memset(&db_cfg, 0, sizeof(db_cfg));

	db_cfg.type = TF_DEVICE_MODULE_TYPE_TCAM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (d = 0; d < TF_DIR_MAX; d++) {
		db_cfg.dir = d;
		db_cfg.alloc_cnt = parms->resources->tcam_cnt[d].cnt;
		db_cfg.rm_db = &tcam_db[d];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: TCAM DB creation failed\n",
				    tf_dir_2_str(d));
			return rc;
		}
	}

	/* Initialize the TCAM manager. */
	if (parms->shadow_copy) {
		for (d = 0; d < TF_DIR_MAX; d++) {
			memset(&shadow_cfg, 0, sizeof(shadow_cfg));
			memset(&shadow_cdb, 0, sizeof(shadow_cdb));
			/* Get the base addresses of the tcams for tcam mgr */
			for (i = 0; i < TF_TCAM_TBL_TYPE_MAX; i++) {
				memset(&info, 0, sizeof(info));

				if (!parms->resources->tcam_cnt[d].cnt[i])
					continue;
				ainfo.rm_db = tcam_db[d];
				ainfo.db_index = i;
				ainfo.info = &info;
				rc = tf_rm_get_info(&ainfo);
				if (rc)
					goto error;

				shadow_cfg.base_addr[i] = info.entry.start;
			}

			/* Create the shadow db */
			shadow_cfg.alloc_cnt =
				parms->resources->tcam_cnt[d].cnt;
			shadow_cfg.num_entries = parms->num_elements;

			shadow_cdb.shadow_db = &shadow_tcam_db[d];
			shadow_cdb.cfg = &shadow_cfg;
			rc = tf_shadow_tcam_create_db(&shadow_cdb);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "TCAM MGR DB creation failed "
					    "rc=%d\n", rc);
				goto error;
			}
		}
		shadow_init = 1;
	}

	init = 1;

	TFP_DRV_LOG(INFO,
		    "TCAM - initialized\n");

	return 0;
error:
	for (i = 0; i < TF_DIR_MAX; i++) {
		memset(&fparms, 0, sizeof(fparms));
		fparms.dir = i;
		fparms.rm_db = tcam_db[i];
		/* Ignoring return here since we are in the error case */
		(void)tf_rm_free_db(tfp, &fparms);

		if (parms->shadow_copy) {
			fshadow.shadow_db = shadow_tcam_db[i];
			tf_shadow_tcam_free_db(&fshadow);
			shadow_tcam_db[i] = NULL;
		}

		tcam_db[i] = NULL;
	}

	shadow_init = 0;
	init = 0;

	return rc;
}

int
tf_tcam_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms;
	struct tf_shadow_tcam_free_db_parms fshadow;

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized */
	if (!init) {
		TFP_DRV_LOG(INFO,
			    "No TCAM DBs created\n");
		return 0;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		memset(&fparms, 0, sizeof(fparms));
		fparms.dir = i;
		fparms.rm_db = tcam_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		tcam_db[i] = NULL;

		if (shadow_init) {
			memset(&fshadow, 0, sizeof(fshadow));

			fshadow.shadow_db = shadow_tcam_db[i];
			tf_shadow_tcam_free_db(&fshadow);
			shadow_tcam_db[i] = NULL;
		}
	}

	shadow_init = 0;
	init = 0;

	return 0;
}

int
tf_tcam_alloc(struct tf *tfp,
	      struct tf_tcam_alloc_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_allocate_parms aparms;
	uint16_t num_slice_per_row = 1;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (dev->ops->tf_dev_get_tcam_slice_info == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* Allocate requested element */
	memset(&aparms, 0, sizeof(aparms));

	aparms.rm_db = tcam_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.priority = parms->priority;
	aparms.index = (uint32_t *)&parms->idx;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed tcam, type:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type);
		return rc;
	}

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM &&
	    (parms->idx % 2) != 0) {
		rc = tf_rm_allocate(&aparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Failed tcam, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type);
			return rc;
		}
	}

	parms->idx *= num_slice_per_row;

	return 0;
}

int
tf_tcam_free(struct tf *tfp,
	     struct tf_tcam_free_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_is_allocated_parms aparms;
	struct tf_rm_free_parms fparms;
	struct tf_rm_get_hcapi_parms hparms;
	uint16_t num_slice_per_row = 1;
	int allocated = 0;
	struct tf_shadow_tcam_remove_parms shparms;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (dev->ops->tf_dev_get_tcam_slice_info == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  0,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* Check if element is in use */
	memset(&aparms, 0, sizeof(aparms));

	aparms.rm_db = tcam_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = parms->idx / num_slice_per_row;
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
		shparms.shadow_db = shadow_tcam_db[parms->dir];
		shparms.fparms = parms;
		rc = tf_shadow_tcam_remove(&shparms);
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
			 * from rm or hw
			 */
			if (parms->ref_cnt >= 1)
				return rc;
		}
	}

	/* Free requested element */
	memset(&fparms, 0, sizeof(fparms));
	fparms.rm_db = tcam_db[parms->dir];
	fparms.db_index = parms->type;
	fparms.index = parms->idx / num_slice_per_row;
	rc = tf_rm_free(&fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Free failed, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->idx);
		return rc;
	}

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		int i;

		for (i = -1; i < 3; i += 3) {
			aparms.index += i;
			rc = tf_rm_is_allocated(&aparms);
			if (rc)
				return rc;

			if (allocated == TF_RM_ALLOCATED_ENTRY_IN_USE) {
				/* Free requested element */
				fparms.index = aparms.index;
				rc = tf_rm_free(&fparms);
				if (rc) {
					TFP_DRV_LOG(ERR,
						    "%s: Free failed, type:%d, "
						    "index:%d\n",
						    tf_dir_2_str(parms->dir),
						    parms->type,
						    fparms.index);
					return rc;
				}
			}
		}
	}

	/* Convert TF type to HCAPI RM type */
	memset(&hparms, 0, sizeof(hparms));

	hparms.rm_db = tcam_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_free(tfp, parms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: Entry %d free failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type),
			    parms->idx,
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_tcam_alloc_search(struct tf *tfp,
		     struct tf_tcam_alloc_search_parms *parms)
{
	struct tf_shadow_tcam_search_parms sparms;
	struct tf_shadow_tcam_bind_index_parms bparms;
	struct tf_tcam_alloc_parms aparms;
	struct tf_tcam_free_parms fparms;
	uint16_t num_slice_per_row = 1;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	if (!shadow_init || !shadow_tcam_db[parms->dir]) {
		TFP_DRV_LOG(ERR, "%s: TCAM Shadow not initialized for %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type));
		return -EINVAL;
	}

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (dev->ops->tf_dev_get_tcam_slice_info == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/*
	 * Prep the shadow search, reusing the parms from original search
	 * instead of copying them.  Shadow will update output in there.
	 */
	memset(&sparms, 0, sizeof(sparms));
	sparms.sparms = parms;
	sparms.shadow_db = shadow_tcam_db[parms->dir];

	rc = tf_shadow_tcam_search(&sparms);
	if (rc)
		return rc;

	/*
	 * The app didn't request us to alloc the entry, so return now.
	 * The hit should have been updated in the original search parm.
	 */
	if (!parms->alloc || parms->search_status != MISS)
		return rc;

	/* Caller desires an allocate on miss */
	if (dev->ops->tf_dev_alloc_tcam == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}
	memset(&aparms, 0, sizeof(aparms));
	aparms.dir = parms->dir;
	aparms.type = parms->type;
	aparms.key_size = parms->key_size;
	aparms.priority = parms->priority;
	rc = dev->ops->tf_dev_alloc_tcam(tfp, &aparms);
	if (rc)
		return rc;

	/* Successful allocation, attempt to add it to the shadow */
	memset(&bparms, 0, sizeof(bparms));
	bparms.dir = parms->dir;
	bparms.shadow_db = shadow_tcam_db[parms->dir];
	bparms.type = parms->type;
	bparms.key = parms->key;
	bparms.mask = parms->mask;
	bparms.key_size = parms->key_size;
	bparms.idx = aparms.idx;
	bparms.hb_handle = sparms.hb_handle;
	rc = tf_shadow_tcam_bind_index(&bparms);
	if (rc) {
		/* Error binding entry, need to free the allocated idx */
		if (dev->ops->tf_dev_free_tcam == NULL) {
			rc = -EOPNOTSUPP;
			TFP_DRV_LOG(ERR,
				    "%s: Operation not supported, rc:%s\n",
				    tf_dir_2_str(parms->dir),
				    strerror(-rc));
			return rc;
		}

		fparms.dir = parms->dir;
		fparms.type = parms->type;
		fparms.idx = aparms.idx;
		rc = dev->ops->tf_dev_free_tcam(tfp, &fparms);
		if (rc)
			return rc;
	}

	/* Add the allocated index to output and done */
	parms->idx = aparms.idx;

	return 0;
}

int
tf_tcam_set(struct tf *tfp __rte_unused,
	    struct tf_tcam_set_parms *parms __rte_unused)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_is_allocated_parms aparms;
	struct tf_rm_get_hcapi_parms hparms;
	struct tf_shadow_tcam_insert_parms iparms;
	uint16_t num_slice_per_row = 1;
	int allocated = 0;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No TCAM DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (dev->ops->tf_dev_get_tcam_slice_info == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "%s: Operation not supported, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* Check if element is in use */
	memset(&aparms, 0, sizeof(aparms));

	aparms.rm_db = tcam_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.index = parms->idx / num_slice_per_row;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry is not allocated, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->idx);
		return -EINVAL;
	}

	/* Convert TF type to HCAPI RM type */
	memset(&hparms, 0, sizeof(hparms));

	hparms.rm_db = tcam_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_set(tfp, parms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: Entry %d set failed, rc:%s",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type),
			    parms->idx,
			    strerror(-rc));
		return rc;
	}

	/* Successfully added to hw, now for shadow if enabled. */
	if (!shadow_init || !shadow_tcam_db[parms->dir])
		return 0;

	iparms.shadow_db = shadow_tcam_db[parms->dir];
	iparms.sparms = parms;
	rc = tf_shadow_tcam_insert(&iparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: %s: Entry %d set failed, rc:%s",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type),
			    parms->idx,
			    strerror(-rc));
		return rc;
	}

	return 0;
}

int
tf_tcam_get(struct tf *tfp __rte_unused,
	    struct tf_tcam_get_parms *parms __rte_unused)
{
	return 0;
}
