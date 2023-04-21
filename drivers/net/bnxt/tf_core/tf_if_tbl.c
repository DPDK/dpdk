/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_if_tbl.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tfp.h"

struct tf;

/**
 * IF Table database
 */
struct tf_if_tbl_db {
	struct tf_if_tbl_cfg *if_tbl_cfg_db[TF_DIR_MAX];
};

/**
 * Convert if_tbl_type to hwrm type.
 *
 * [in] if_tbl_type
 *   Interface table type
 *
 * [out] hwrm_type
 *   HWRM device data type
 *
 * Returns:
 *    0          - Success
 *   -EOPNOTSUPP - Type not supported
 */
static int
tf_if_tbl_get_hcapi_type(struct tf_if_tbl_get_hcapi_parms *parms)
{
	struct tf_if_tbl_cfg *tbl_cfg;
	enum tf_if_tbl_cfg_type cfg_type;

	tbl_cfg = (struct tf_if_tbl_cfg *)parms->tbl_db;
	cfg_type = tbl_cfg[parms->db_index].cfg_type;

	if (cfg_type != TF_IF_TBL_CFG)
		return -ENOTSUP;

	*parms->hcapi_type = tbl_cfg[parms->db_index].hcapi_type;

	return 0;
}

int
tf_if_tbl_bind(struct tf *tfp,
	       struct tf_if_tbl_cfg_parms *parms)
{
	struct tfp_calloc_parms cparms;
	struct tf_if_tbl_db *if_tbl_db;

	TF_CHECK_PARMS2(tfp, parms);

	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_if_tbl_db);
	cparms.alignment = 0;
	if (tfp_calloc(&cparms) != 0) {
		TFP_DRV_LOG(ERR, "if_tbl_rm_db alloc error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}

	if_tbl_db = cparms.mem_va;
	if_tbl_db->if_tbl_cfg_db[TF_DIR_RX] = parms->cfg;
	if_tbl_db->if_tbl_cfg_db[TF_DIR_TX] = parms->cfg;
	tf_session_set_if_tbl_db(tfp, (void *)if_tbl_db);

	TFP_DRV_LOG(INFO,
		    "Table Type - initialized\n");

	return 0;
}

int
tf_if_tbl_unbind(struct tf *tfp)
{
	int rc;
	struct tf_if_tbl_db *if_tbl_db_ptr = NULL;

	TF_CHECK_PARMS1(tfp);

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(INFO, "No IF Table DBs initialized\n");
		return 0;
	}
	/* Bail if nothing has been initialized */
	if (!if_tbl_db_ptr) {
		TFP_DRV_LOG(INFO,
			    "No Table DBs created\n");
		return 0;
	}

	tfp_free((void *)if_tbl_db_ptr);
	tf_session_set_if_tbl_db(tfp, NULL);

	return 0;
}

int
tf_if_tbl_set(struct tf *tfp,
	      struct tf_if_tbl_set_parms *parms)
{
	int rc;
	struct tf_if_tbl_db *if_tbl_db_ptr = NULL;
	struct tf_if_tbl_get_hcapi_parms hparms;

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(INFO, "No IF Table DBs initialized\n");
		return 0;
	}

	if (!if_tbl_db_ptr) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Convert TF type to HCAPI type */
	hparms.tbl_db = if_tbl_db_ptr->if_tbl_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;
	rc = tf_if_tbl_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_set_if_tbl_entry(tfp, parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, If Tbl set failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
	}

	return 0;
}

int
tf_if_tbl_get(struct tf *tfp,
	      struct tf_if_tbl_get_parms *parms)
{
	int rc = 0;
	struct tf_if_tbl_db *if_tbl_db_ptr = NULL;
	struct tf_if_tbl_get_hcapi_parms hparms;

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(INFO, "No IF Table DBs initialized\n");
		return 0;
	}

	if (!if_tbl_db_ptr) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Convert TF type to HCAPI type */
	hparms.tbl_db = if_tbl_db_ptr->if_tbl_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;
	rc = tf_if_tbl_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	/* Get the entry */
	rc = tf_msg_get_if_tbl_entry(tfp, parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, If Tbl get failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
	}

	return 0;
}
