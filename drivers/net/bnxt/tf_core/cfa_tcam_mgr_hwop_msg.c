/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 Broadcom
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
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_hwop_msg.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_util.h"

int
cfa_tcam_mgr_hwops_init(struct cfa_tcam_mgr_data *tcam_mgr_data,
			enum cfa_tcam_mgr_device_type type)
{
	struct cfa_tcam_mgr_hwops_funcs *hwop_funcs =
			&tcam_mgr_data->hwop_funcs;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
		return cfa_tcam_mgr_hwops_get_funcs_p4(hwop_funcs);
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		return cfa_tcam_mgr_hwops_get_funcs_p58(hwop_funcs);
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
cfa_tcam_mgr_entry_set_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
			   struct tf *tfp __rte_unused,
			   struct cfa_tcam_mgr_set_parms *parms,
			   int row, int slice,
			   int max_slices __rte_unused)
{
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);
	cfa_tcam_mgr_hwop_set_func_t set_func;
	struct tf_tcam_set_parms sparms;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	set_func = tcam_mgr_data->hwop_funcs.set;
	if (!set_func)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);


	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
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

#ifdef CFA_TCAM_MGR_TRACING
	CFA_TCAM_MGR_LOG_DIR_TYPE(INFO, parms->dir, parms->type,
				  "%s: %s row:%d slice:%d "
				  "set tcam physical idx 0x%x\n",
				  tf_dir_2_str(parms->dir),
				  cfa_tcam_mgr_tbl_2_str(parms->type),
				  row, slice, sparms.idx);
#endif

	rc = tf_msg_tcam_entry_set(tfp, dev, &sparms);
	if (rc) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "%s: %s entry:%d "
					  "set tcam failed, rc:%d\n",
					  tf_dir_2_str(parms->dir),
					  cfa_tcam_mgr_tbl_2_str(parms->type),
					  parms->id, -rc);
		return rc;
	}

	return set_func(tcam_mgr_data, parms, row, slice, max_slices);
}

int
cfa_tcam_mgr_entry_get_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
			   struct tf *tfp __rte_unused,
			   struct cfa_tcam_mgr_get_parms *parms,
			   int row, int slice,
			   int max_slices __rte_unused)
{
	cfa_tcam_mgr_hwop_get_func_t get_func;

	get_func = tcam_mgr_data->hwop_funcs.get;
	if (!get_func)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	return get_func(tcam_mgr_data, parms, row, slice, max_slices);
}

int
cfa_tcam_mgr_entry_free_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
			    struct tf *tfp __rte_unused,
			    struct cfa_tcam_mgr_free_parms *parms,
			    int row, int slice,
			    int key_size,
			    int result_size,
			    int max_slices)
{
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);
	uint8_t mask[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };
	uint8_t key[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };
	cfa_tcam_mgr_hwop_free_func_t free_func;
	struct tf_tcam_set_parms sparms;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	free_func = tcam_mgr_data->hwop_funcs.free;
	if (!free_func)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	/*
	 * The free hwop will free more than a single slice (an entire row),
	 * so cannot be used. Use set message to clear an individual entry
	 */

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (key_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "%s: %s entry:%d key size:%d > %d\n",
					  tf_dir_2_str(parms->dir),
					  cfa_tcam_mgr_tbl_2_str(parms->type),
					  parms->id, key_size,
					  CFA_TCAM_MGR_MAX_KEY_SIZE);
		return -EINVAL;
	}

	if (result_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "%s: %s entry:%d res size:%d > %d\n",
					  tf_dir_2_str(parms->dir),
					  cfa_tcam_mgr_tbl_2_str(parms->type),
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

#ifdef CFA_TCAM_MGR_TRACING
	CFA_TCAM_MGR_LOG_DIR_TYPE(INFO, parms->dir, parms->type,
				  "%s: %s row:%d slice:%d free idx:%d "
				  "key_sz:%d result_sz:%d\n",
				  tf_dir_2_str(parms->dir),
				  cfa_tcam_mgr_tbl_2_str(parms->type),
				  row, slice, sparms.idx, key_size,
				  result_size);
#endif

	rc = tf_msg_tcam_entry_set(tfp, dev, &sparms);
	if (rc) {
		/* Log error */
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					  "%s: %s row:%d slice:%d set failed, "
					  "rc:%d\n",
					  tf_dir_2_str(parms->dir),
					  cfa_tcam_mgr_tbl_2_str(parms->type),
					  row, slice, rc);
		return rc;
	}

	return free_func(tcam_mgr_data, parms, row, slice, max_slices);
}
