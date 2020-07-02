/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_identifier.h"
#include "tf_common.h"
#include "tf_rm_new.h"
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

int
tf_ident_bind(struct tf *tfp,
	      struct tf_ident_cfg_parms *parms)
{
	int rc;
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	if (init) {
		TFP_DRV_LOG(ERR,
			    "Identifier already initialized\n");
		return -EINVAL;
	}

	db_cfg.num_elements = parms->num_elements;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.num_elements = parms->num_elements;
		db_cfg.cfg = parms->cfg;
		db_cfg.alloc_cnt = parms->resources->ident_cnt[i].cnt;
		db_cfg.rm_db = &ident_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Identifier DB creation failed\n",
				    tf_dir_2_str(i));

			return rc;
		}
	}

	init = 1;

	printf("Identifier - initialized\n");

	return 0;
}

int
tf_ident_unbind(struct tf *tfp __rte_unused)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized done silent as to
	 * allow for creation cleanup.
	 */
	if (!init) {
		TFP_DRV_LOG(ERR,
			    "No Identifier DBs created\n");
		return -EINVAL;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		fparms.dir = i;
		fparms.rm_db = ident_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		ident_db[i] = NULL;
	}

	init = 0;

	return 0;
}

int
tf_ident_alloc(struct tf *tfp __rte_unused,
	       struct tf_ident_alloc_parms *parms)
{
	int rc;
	uint32_t id;
	struct tf_rm_allocate_parms aparms = { 0 };

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
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (!allocated) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry already free, type:%d, index:%d\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    parms->id);
		return rc;
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
