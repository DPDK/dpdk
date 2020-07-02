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


struct tf;

/**
 * Table DBs.
 */
static void *tbl_db[TF_DIR_MAX];

/**
 * Table Shadow DBs
 */
/* static void *shadow_tbl_db[TF_DIR_MAX]; */

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
/* static uint8_t shadow_init; */

int
tf_tbl_bind(struct tf *tfp,
	    struct tf_tbl_cfg_parms *parms)
{
	int rc;
	int i;
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

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->tbl_cnt[i].cnt;
		db_cfg.rm_db = &tbl_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Table DB creation failed\n",
				    tf_dir_2_str(i));

			return rc;
		}
	}

	init = 1;

	TFP_DRV_LOG(INFO,
		    "Table Type - initialized\n");

	return 0;
}

int
tf_tbl_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };

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
	}

	init = 0;

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
tf_tbl_alloc_search(struct tf *tfp __rte_unused,
		    struct tf_tbl_alloc_search_parms *parms __rte_unused)
{
	return 0;
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
	int i;
	uint16_t hcapi_type;
	uint32_t idx;
	int allocated = 0;
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Table DBs created\n",
			    tf_dir_2_str(parms->dir));

		return -EINVAL;
	}
	/* Verify that the entries has been previously allocated */
	aparms.rm_db = tbl_db[parms->dir];
	aparms.db_index = parms->type;
	aparms.allocated = &allocated;
	idx = parms->starting_idx;
	for (i = 0; i < parms->num_entries; i++) {
		aparms.index = idx;
		rc = tf_rm_is_allocated(&aparms);
		if (rc)
			return rc;

		if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
			TFP_DRV_LOG(ERR,
				    "%s, Invalid or not allocated index, type:%d, idx:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type,
				    idx);
			return -EINVAL;
		}
		idx++;
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
