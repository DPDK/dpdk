/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_identifier.h"
#include "tf_shadow_identifier.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tfp.h"

struct tf;

/**
 * Identifier DBs.
 */
static void *ident_db[TF_DIR_MAX];

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Identifier shadow DBs.
 */
static void *ident_shadow_db[TF_DIR_MAX];

/**
 * Shadow DB Init flag, set on bind and cleared on unbind
 */
static uint8_t shadow_init;

int
tf_ident_bind(struct tf *tfp,
	      struct tf_ident_cfg_parms *parms)
{
	int rc;
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct tf_shadow_ident_cfg_parms shadow_cfg = { 0 };
	struct tf_shadow_ident_create_db_parms shadow_cdb = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (init) {
		TFP_DRV_LOG(ERR,
			    "Identifier DB already initialized\n");
		return -EINVAL;
	}

	db_cfg.type = TF_DEVICE_MODULE_TYPE_IDENTIFIER;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->ident_cnt[i].cnt;
		db_cfg.rm_db = &ident_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Identifier DB creation failed\n",
				    tf_dir_2_str(i));

			return rc;
		}

		if (parms->shadow_copy) {
			shadow_cfg.alloc_cnt =
				parms->resources->ident_cnt[i].cnt;
			shadow_cdb.num_elements = parms->num_elements;
			shadow_cdb.tf_shadow_ident_db = &ident_shadow_db[i];
			shadow_cdb.cfg = &shadow_cfg;
			rc = tf_shadow_ident_create_db(&shadow_cdb);
			if (rc) {
				TFP_DRV_LOG(ERR,
				    "%s: Ident shadow DB creation failed\n",
				    tf_dir_2_str(i));

				return rc;
			}
			shadow_init = 1;
		}
	}

	init = 1;

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
	struct tf_shadow_ident_free_db_parms sparms = { 0 };

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized */
	if (!init) {
		TFP_DRV_LOG(INFO,
			    "No Identifier DBs created\n");
		return 0;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		fparms.dir = i;
		fparms.rm_db = ident_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "rm free failed on unbind\n");
		}
		if (shadow_init) {
			sparms.tf_shadow_ident_db = ident_shadow_db[i];
			rc = tf_shadow_ident_free_db(&sparms);
			if (rc) {
				/* TODO: If there are failures on unbind we
				 * really just have to try until all DBs are
				 * attempted to be cleared.
				 */
			}
			ident_shadow_db[i] = NULL;
		}
		ident_db[i] = NULL;
	}

	init = 0;
	shadow_init = 0;

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
	struct tf_shadow_ident_insert_parms iparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Identifier DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Allocate requested element */
	aparms.rm_db = ident_db[parms->dir];
	aparms.db_index = parms->type;
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

	if (shadow_init) {
		iparms.tf_shadow_ident_db = ident_shadow_db[parms->dir];
		iparms.type = parms->type;
		iparms.id = base_id;

		rc = tf_shadow_ident_insert(&iparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Failed insert shadow DB, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type);
			return rc;
		}
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
	struct tf_shadow_ident_remove_parms rparms = { 0 };
	int allocated = 0;
	uint32_t base_id;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Identifier DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Check if element is in use */
	aparms.rm_db = ident_db[parms->dir];
	aparms.db_index = parms->type;
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

	if (shadow_init) {
		rparms.tf_shadow_ident_db = ident_shadow_db[parms->dir];
		rparms.type = parms->type;
		rparms.id = base_id;
		rparms.ref_cnt = parms->ref_cnt;

		rc = tf_shadow_ident_remove(&rparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: ref_cnt was 0 in shadow DB,"
				    " type:%d, index:%d\n",
				    tf_dir_2_str(parms->dir),
				    parms->type,
				    parms->id);
			return rc;
		}

		if (*rparms.ref_cnt > 0)
			return 0;
	}

	/* Free requested element */
	fparms.rm_db = ident_db[parms->dir];
	fparms.db_index = parms->type;
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
	struct tf_shadow_ident_search_parms sparms = { 0 };
	int allocated = 0;
	uint32_t base_id;

	TF_CHECK_PARMS2(tfp, parms);

	if (!init) {
		TFP_DRV_LOG(ERR,
			    "%s: No Identifier DBs created\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	if (!shadow_init) {
		TFP_DRV_LOG(ERR,
			    "%s: Identifier Shadow copy is not enabled\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	/* Check if element is in use */
	aparms.rm_db = ident_db[parms->dir];
	aparms.db_index = parms->type;
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

	sparms.tf_shadow_ident_db = ident_shadow_db[parms->dir];
	sparms.type = parms->type;
	sparms.search_id = base_id;
	sparms.hit = parms->hit;
	sparms.ref_cnt = parms->ref_cnt;

	rc = tf_shadow_ident_search(&sparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed search shadow DB, type:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type);
		return rc;
	}

	return 0;
}
