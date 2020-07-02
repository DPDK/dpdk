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

struct tf;

/**
 * TCAM DBs.
 */
static void *tcam_db[TF_DIR_MAX];

/**
 * TCAM Shadow DBs
 */
/* static void *shadow_tcam_db[TF_DIR_MAX]; */

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
/* static uint8_t shadow_init; */

int
tf_tcam_bind(struct tf *tfp,
	     struct tf_tcam_cfg_parms *parms)
{
	int rc;
	int i;
	struct tf_tcam_resources *tcam_cnt;
	struct tf_rm_create_db_parms db_cfg = { 0 };

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

	db_cfg.type = TF_DEVICE_MODULE_TYPE_TCAM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->tcam_cnt[i].cnt;
		db_cfg.rm_db = &tcam_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: TCAM DB creation failed\n",
				    tf_dir_2_str(i));
			return rc;
		}
	}

	init = 1;

	TFP_DRV_LOG(INFO,
		    "TCAM - initialized\n");

	return 0;
}

int
tf_tcam_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized */
	if (!init) {
		TFP_DRV_LOG(INFO,
			    "No TCAM DBs created\n");
		return 0;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		fparms.dir = i;
		fparms.rm_db = tcam_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		tcam_db[i] = NULL;
	}

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
	struct tf_rm_allocate_parms aparms = { 0 };
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
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
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
						  0,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* Check if element is in use */
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

	/* Free requested element */
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
						    "%s: Free failed, type:%d, index:%d\n",
						    tf_dir_2_str(parms->dir),
						    parms->type,
						    fparms.index);
					return rc;
				}
			}
		}
	}

	/* Convert TF type to HCAPI RM type */
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
tf_tcam_alloc_search(struct tf *tfp __rte_unused,
		     struct tf_tcam_alloc_search_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_set(struct tf *tfp __rte_unused,
	    struct tf_tcam_set_parms *parms __rte_unused)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
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

	return 0;
}

int
tf_tcam_get(struct tf *tfp __rte_unused,
	    struct tf_tcam_get_parms *parms __rte_unused)
{
	return 0;
}
