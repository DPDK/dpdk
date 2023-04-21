/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
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
#include "tf_tcam_mgr_msg.h"

struct tf;

int
tf_tcam_bind(struct tf *tfp,
	     struct tf_tcam_cfg_parms *parms)
{
	int rc;
	int db_rc[TF_DIR_MAX] = { 0 };
	int d, t;
	struct tf_rm_alloc_info info;
	struct tf_rm_free_db_parms fparms;
	struct tf_rm_create_db_parms db_cfg;
	struct tf_tcam_resources local_tcam_cnt[TF_DIR_MAX];
	struct tf_tcam_resources *tcam_cnt;
	struct tf_rm_get_alloc_info_parms ainfo;
	uint16_t num_slices = 1;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tcam_rm_db *tcam_db;
	struct tfp_calloc_parms cparms;
	struct tf_resource_info resv_res[TF_DIR_MAX][TF_TCAM_TBL_TYPE_MAX];
	uint32_t rx_supported;
	uint32_t tx_supported;
	bool no_req = true;

	TF_CHECK_PARMS2(tfp, parms);

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
			    "Operation not supported, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	tcam_cnt = parms->resources->tcam_cnt;

	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			rc = dev->ops->tf_dev_get_tcam_slice_info(tfp, t, 0,
								  &num_slices);
	if (rc)
		return rc;

			if (num_slices == 1)
				continue;

			if (tcam_cnt[d].cnt[t] % num_slices) {
				TFP_DRV_LOG(ERR,
					    "%s: Requested num of %s entries "
					    "has to be multiple of %d\n",
					    tf_dir_2_str(d),
					    tf_tcam_tbl_2_str(t),
					    num_slices);
				return -EINVAL;
			}
		}
	}

	memset(&db_cfg, 0, sizeof(db_cfg));
	cparms.nitems = 1;
	cparms.size = sizeof(struct tcam_rm_db);
	cparms.alignment = 0;
	if (tfp_calloc(&cparms) != 0) {
		TFP_DRV_LOG(ERR, "tcam_rm_db alloc error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}

	tcam_db = cparms.mem_va;
	for (d = 0; d < TF_DIR_MAX; d++)
		tcam_db->tcam_db[d] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, tcam_db);

	db_cfg.module = TF_MODULE_TYPE_TCAM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (d = 0; d < TF_DIR_MAX; d++) {
		db_cfg.dir = d;
		db_cfg.alloc_cnt = tcam_cnt[d].cnt;
		db_cfg.rm_db = (void *)&tcam_db->tcam_db[d];
		if (tf_session_is_shared_session(tfs) &&
			(!tf_session_is_shared_session_creator(tfs)))
			db_rc[d] = tf_rm_create_db_no_reservation(tfp, &db_cfg);
		else
			db_rc[d] = tf_rm_create_db(tfp, &db_cfg);
	}
	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		TFP_DRV_LOG(ERR, "No TCAM DB created\n");
		return db_rc[TF_DIR_RX];
	}

	/* Collect info on which entries were reserved. */
	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			memset(&info, 0, sizeof(info));
			if (tcam_cnt[d].cnt[t] == 0) {
				resv_res[d][t].start  = 0;
				resv_res[d][t].stride = 0;
				continue;
			}
			ainfo.rm_db = tcam_db->tcam_db[d];
			ainfo.subtype = t;
			ainfo.info = &info;
			rc = tf_rm_get_info(&ainfo);
			if (rc)
				goto error;

			rc = dev->ops->tf_dev_get_tcam_slice_info(tfp, t, 0,
								  &num_slices);
			if (rc)
				return rc;

			if (num_slices > 1) {
				/* check if reserved resource for is multiple of
				 * num_slices
				 */
				if (info.entry.start % num_slices != 0 ||
				    info.entry.stride % num_slices != 0) {
					TFP_DRV_LOG(ERR,
						    "%s: %s reserved resource"
						    " is not multiple of %d\n",
						    tf_dir_2_str(d),
						    tf_tcam_tbl_2_str(t),
						    num_slices);
					rc = -EINVAL;
					goto error;
				}
			}

			resv_res[d][t].start  = info.entry.start;
			resv_res[d][t].stride = info.entry.stride;
		}
	}

	rc = tf_tcam_mgr_bind_msg(tfp, dev, parms, resv_res);
	if (rc)
		return rc;

	rc = tf_tcam_mgr_qcaps_msg(tfp, dev,
				   &rx_supported, &tx_supported);
	if (rc)
		return rc;

	for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
		if (rx_supported & 1 << t)
			tfs->tcam_mgr_control[TF_DIR_RX][t] = 1;
		if (tx_supported & 1 << t)
			tfs->tcam_mgr_control[TF_DIR_TX][t] = 1;
	}

	/*
	 * Make a local copy of tcam_cnt with only resources not managed by TCAM
	 * Manager requested.
	 */
	memcpy(&local_tcam_cnt, tcam_cnt, sizeof(local_tcam_cnt));
	tcam_cnt = local_tcam_cnt;
	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			/* If controlled by TCAM Manager */
			if (tfs->tcam_mgr_control[d][t])
				tcam_cnt[d].cnt[t] = 0;
			else if (tcam_cnt[d].cnt[t] > 0)
				no_req = false;
		}
	}

	/* If no resources left to request */
	if (no_req)
		goto finished;

finished:
	TFP_DRV_LOG(INFO,
		    "TCAM - initialized\n");

	return 0;
error:
	for (d = 0; d < TF_DIR_MAX; d++) {
		if (tcam_db->tcam_db[d] != NULL) {
			memset(&fparms, 0, sizeof(fparms));
			fparms.dir = d;
			fparms.rm_db = tcam_db->tcam_db[d];
			/*
			 * Ignoring return here since we are in the error case
			 */
			(void)tf_rm_free_db(tfp, &fparms);

			tcam_db->tcam_db[d] = NULL;
		}
		tcam_db->tcam_db[d] = NULL;
		tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, NULL);
	}
	return rc;
}

int
tf_tcam_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	TF_CHECK_PARMS1(tfp);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;
	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc)
		return 0;

	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (tcam_db->tcam_db[i] != NULL) {
			memset(&fparms, 0, sizeof(fparms));
			fparms.dir = i;
			fparms.rm_db = tcam_db->tcam_db[i];
			rc = tf_rm_free_db(tfp, &fparms);
			if (rc)
				return rc;

			tcam_db->tcam_db[i] = NULL;
		}

	}

	rc = tf_tcam_mgr_unbind_msg(tfp, dev);
	if (rc)
		return rc;

	return 0;
}

int
tf_tcam_alloc(struct tf *tfp,
	      struct tf_tcam_alloc_parms *parms)
{
	int rc, i;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_allocate_parms aparms;
	uint16_t num_slices = 1;
	uint32_t index;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

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

	/* Need to retrieve number of slices based on the key_size */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slices);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_alloc_msg(tfp, dev, parms);
	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/*
	 * For WC TCAM, number of slices could be 4, 2, 1 based on
	 * the key_size. For other TCAM, it is always 1
	 */
	for (i = 0; i < num_slices; i++) {
		memset(&aparms, 0, sizeof(aparms));
		aparms.rm_db = tcam_db->tcam_db[parms->dir];
		aparms.subtype = parms->type;
		aparms.priority = parms->priority;
		aparms.index = &index;
		rc = tf_rm_allocate(&aparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Failed tcam, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type);
			return rc;
		}

		/* return the start index of each row */
			if (i == 0)
				parms->idx = index;
	}

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
	uint16_t num_slices = 1;
	int allocated = 0;
	int i;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

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
						  &num_slices);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		/*
		 * If a session can have multiple references to an entry, check
		 * the reference count here before actually freeing the entry.
		 */
		return tf_tcam_mgr_free_msg(tfp, dev, parms);

	if (parms->idx % num_slices) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM reserved resource is not multiple of %d\n",
			    tf_dir_2_str(parms->dir),
			    num_slices);
		return -EINVAL;
	}

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	memset(&aparms, 0, sizeof(aparms));
	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
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

	for (i = 0; i < num_slices; i++) {
		/* Free requested element */
		memset(&fparms, 0, sizeof(fparms));
		fparms.rm_db = tcam_db->tcam_db[parms->dir];
		fparms.subtype = parms->type;
		fparms.index = parms->idx + i;
		rc = tf_rm_free(&fparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Free failed, type:%d, index:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type,
				    parms->idx);
			return rc;
		}
	}

	/* Convert TF type to HCAPI RM type */
	memset(&hparms, 0, sizeof(hparms));

	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_free(tfp, dev, parms);
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
tf_tcam_set(struct tf *tfp __rte_unused,
	    struct tf_tcam_set_parms *parms __rte_unused)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_is_allocated_parms aparms;
	struct tf_rm_get_hcapi_parms hparms;
	uint16_t num_slice_per_row = 1;
	int allocated = 0;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

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

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_set_msg(tfp, dev, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	memset(&aparms, 0, sizeof(aparms));

	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
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

	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_set(tfp, dev, parms);
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
	    struct tf_tcam_get_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_is_allocated_parms aparms;
	struct tf_rm_get_hcapi_parms hparms;
	int allocated = 0;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_get_msg(tfp, dev, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	memset(&aparms, 0, sizeof(aparms));

	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
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

	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_get(tfp, dev, parms);
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
tf_tcam_get_resc_info(struct tf *tfp,
		      struct tf_tcam_resource_info *tcam)
{
	int rc;
	int d;
	struct tf_resource_info *dinfo;
	struct tf_rm_get_alloc_info_parms ainfo;
	void *tcam_db_ptr = NULL;
	struct tcam_rm_db *tcam_db;

	TF_CHECK_PARMS2(tfp, tcam);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc == -ENOMEM)
		return 0;  /* db doesn't exist */
	else if (rc)
		return rc; /* error getting db */

	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* check if reserved resource for WC is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = tcam_db->tcam_db[d];

		if (!ainfo.rm_db)
			continue;

		dinfo = tcam[d].info;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_TCAM_TBL_TYPE_MAX);
		if (rc && rc != -ENOTSUP)
			return rc;
	}

	return 0;
}
