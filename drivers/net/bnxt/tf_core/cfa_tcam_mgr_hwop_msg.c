/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

/*
 * This file will "do the right thing" for each of the primitives set, get and
 * free. The TCAM manager is running in the core, so the tables will be cached.
 * Set and free messages will also be sent to the firmware.  Instead of sending
 * get messages, the entry will be read from the cached copy thus saving a
 * firmware message.
 */

#include "tf_tcam.h"
#include "hcapi_cfa_defs.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_hwop_msg.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_util.h"

/*
 * The free hwop will free more than a single slice so cannot be used.
 */
struct cfa_tcam_mgr_hwops_funcs hwop_funcs;

int
cfa_tcam_mgr_hwops_init(enum cfa_tcam_mgr_device_type type)
{
	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		return cfa_tcam_mgr_hwops_get_funcs_p4(&hwop_funcs);
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		return cfa_tcam_mgr_hwops_get_funcs_p58(&hwop_funcs);
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device\n");
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}
}

/*
 * This is the glue between the TCAM manager and the firmware HW operations.  It
 * is intended to abstract out the location of the TCAM manager so that the TCAM
 * manager code will be the same whether or not it is actually using the
 * firmware.
 */

int
cfa_tcam_mgr_entry_set_msg(int sess_idx, struct cfa_tcam_mgr_context *context
			   __rte_unused,
			   struct cfa_tcam_mgr_set_parms *parms,
			   int row, int slice,
			   int max_slices __rte_unused)
{
	cfa_tcam_mgr_hwop_set_func_t set_func;

	set_func = hwop_funcs.set;
	if (set_func == NULL)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	struct tf_tcam_set_parms sparms;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int rc;
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(context->tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	memset(&sparms, 0, sizeof(sparms));
	sparms.dir	   = parms->dir;
	sparms.type	   = type;
	sparms.hcapi_type  = parms->hcapi_type;
	sparms.idx	   = (row * max_slices) + slice;
	sparms.key	   = parms->key;
	sparms.mask	   = parms->mask;
	sparms.key_size	   = parms->key_size;
	sparms.result	   = parms->result;
	sparms.result_size = parms->result_size;

	rc = tf_msg_tcam_entry_set(context->tfp, dev, &sparms);
	if (rc) {
		/* Log error */
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "Entry %d set failed, rc:%d\n",
					  parms->id, -rc);
		return rc;
	}

	return set_func(sess_idx, parms, row, slice, max_slices);
}

int
cfa_tcam_mgr_entry_get_msg(int sess_idx, struct cfa_tcam_mgr_context *context
			   __rte_unused,
			   struct cfa_tcam_mgr_get_parms *parms,
			   int row, int slice,
			   int max_slices __rte_unused)
{
	cfa_tcam_mgr_hwop_get_func_t get_func;

	get_func = hwop_funcs.get;
	if (get_func == NULL)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	return get_func(sess_idx, parms, row, slice, max_slices);
}

int
cfa_tcam_mgr_entry_free_msg(int sess_idx, struct cfa_tcam_mgr_context *context
			    __rte_unused,
			    struct cfa_tcam_mgr_free_parms *parms,
			    int row, int slice,
			    int key_size,
			    int result_size,
			    int max_slices)
{
	cfa_tcam_mgr_hwop_free_func_t free_func;

	free_func = hwop_funcs.free;
	if (free_func == NULL)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);

	/* Free will clear an entire row. */
	/* Use set message to clear an individual entry */
	struct tf_tcam_set_parms sparms;
	uint8_t key[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };
	uint8_t mask[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(context->tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (key_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "Entry %d key size is %d greater than:%d\n",
					  parms->id, key_size,
					  CFA_TCAM_MGR_MAX_KEY_SIZE);
		return -EINVAL;
	}

	if (result_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "Entry %d result size is %d greater than:%d\n",
					  parms->id, result_size,
					  CFA_TCAM_MGR_MAX_KEY_SIZE);
		return -EINVAL;
	}

	memset(&sparms, 0, sizeof(sparms));
	memset(&key, 0, sizeof(key));
	memset(&mask, 0xff, sizeof(mask));

	sparms.dir	   = parms->dir;
	sparms.type	   = type;
	sparms.hcapi_type  = parms->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = key;
	sparms.idx	   = (row * max_slices) + slice;
	sparms.key_size	   = key_size;
	sparms.result_size = result_size;

	rc = tf_msg_tcam_entry_set(context->tfp, dev, &sparms);
	if (rc) {
		/* Log error */
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "Row %d, slice %d set failed, "
					  "rc:%d.\n",
					  row,
					  slice,
					  rc);
		return rc;
	}
	return free_func(sess_idx, parms, row, slice, max_slices);
}
