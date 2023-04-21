/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_identifier.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tfp.h"
#include "tf_session.h"

struct tf;

int
tf_ident_bind(struct tf *tfp,
	      struct tf_ident_cfg_parms *parms)
{
	int rc;
	int db_rc[TF_DIR_MAX] = { 0 };
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct ident_rm_db *ident_db;
	struct tfp_calloc_parms cparms;
	struct tf_session *tfs;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	memset(&db_cfg, 0, sizeof(db_cfg));
	cparms.nitems = 1;
	cparms.size = sizeof(struct ident_rm_db);
	cparms.alignment = 0;
	if (tfp_calloc(&cparms) != 0) {
		TFP_DRV_LOG(ERR, "ident_rm_db alloc error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}

	ident_db = cparms.mem_va;
	for (i = 0; i < TF_DIR_MAX; i++)
		ident_db->ident_db[i] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_IDENTIFIER, ident_db);

	db_cfg.module = TF_MODULE_TYPE_IDENTIFIER;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.rm_db = (void *)&ident_db->ident_db[i];
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->ident_cnt[i].cnt;
		if (tf_session_is_shared_session(tfs) &&
			(!tf_session_is_shared_session_creator(tfs)))
			db_rc[i] = tf_rm_create_db_no_reservation(tfp, &db_cfg);
		else
			db_rc[i] = tf_rm_create_db(tfp, &db_cfg);
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		TFP_DRV_LOG(ERR, "No Identifier DB created\n");
		return db_rc[TF_DIR_RX];
	}

	TFP_DRV_LOG(INFO,
		    "Identifier - initialized\n");

	return 0;
}

int
tf_ident_unbind(struct tf *tfp)
{
	int rc = 0;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;

	TF_CHECK_PARMS1(tfp);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc)
		return 0;

	ident_db = (struct ident_rm_db *)ident_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (ident_db->ident_db[i] == NULL)
			continue;
		fparms.rm_db = ident_db->ident_db[i];
		fparms.dir = i;
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "rm free failed on unbind\n");
		}
		ident_db->ident_db[i] = NULL;
	}
	return 0;
}

int
tf_ident_alloc(struct tf *tfp __rte_unused,
	       struct tf_ident_alloc_parms *parms)
{
	int rc;
	uint32_t id;
	uint32_t base_id;
	struct tf_rm_allocate_parms aparms = { 0 };
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get ident_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	aparms.rm_db = ident_db->ident_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = &id;
	aparms.base_index = &base_id;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed allocate, type:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type);
		return rc;
	}

	*parms->id = id;
	return 0;
}

int
tf_ident_free(struct tf *tfp __rte_unused,
	      struct tf_ident_free_parms *parms)
{
	int rc;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	int allocated = 0;
	uint32_t base_id;
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get ident_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = ident_db->ident_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->id;
	aparms.base_index = &base_id;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry already free, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->id);
		return -EINVAL;
	}

	/* Free requested element */
	fparms.rm_db = ident_db->ident_db[parms->dir];
	fparms.subtype = parms->type;
	fparms.index = parms->id;
	rc = tf_rm_free(&fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Free failed, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->id);
		return rc;
	}

	return 0;
}

int
tf_ident_search(struct tf *tfp __rte_unused,
		struct tf_ident_search_parms *parms)
{
	int rc;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	int allocated = 0;
	uint32_t base_id;
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get ident_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = ident_db->ident_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->search_id;
	aparms.base_index = &base_id;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry not allocated, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->search_id);
		return -EINVAL;
	}
	return 0;
}

int
tf_ident_get_resc_info(struct tf *tfp,
		       struct tf_identifier_resource_info *ident)
{
	int rc;
	int d;
	struct tf_resource_info *dinfo;
	struct tf_rm_get_alloc_info_parms ainfo;
	void *ident_db_ptr = NULL;
	struct ident_rm_db *ident_db;

	TF_CHECK_PARMS2(tfp, ident);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc == -ENOMEM)
		return 0; /* db doesn't exist */
	else if (rc)
		return rc; /* error getting db */

	ident_db = (struct ident_rm_db *)ident_db_ptr;

	/* check if reserved resource for WC is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = ident_db->ident_db[d];

		if (!ainfo.rm_db)
			continue;

		dinfo = ident[d].info;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_IDENT_TYPE_MAX);
		if (rc)
			return rc;
	}

	return 0;
}
