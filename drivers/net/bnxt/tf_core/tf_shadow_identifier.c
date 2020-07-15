/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_shadow_identifier.h"
#include "tf_common.h"
#include "tf_util.h"
#include "tfp.h"

/**
 * Shadow identifier DB element
 */
struct tf_shadow_ident_element {
	/**
	 * Identifier
	 */
	uint32_t *id;

	/**
	 * Reference count, array of number of identifier type entries
	 */
	uint32_t *ref_count;
};

/**
 * Shadow identifier DB definition
 */
struct tf_shadow_ident_db {
	/**
	 * Number of elements in the DB
	 */
	uint16_t num_entries;

	/**
	 * The DB consists of an array of elements
	 */
	struct tf_shadow_ident_element *db;
};

int
tf_shadow_ident_create_db(struct tf_shadow_ident_create_db_parms *parms)
{
	int rc;
	int i;
	struct tfp_calloc_parms cparms;
	struct tf_shadow_ident_db *shadow_db;
	struct tf_shadow_ident_element *db;

	TF_CHECK_PARMS1(parms);

	/* Build the shadow DB per the request */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_shadow_ident_db);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	shadow_db = (void *)cparms.mem_va;

	/* Build the DB within shadow DB */
	cparms.nitems = parms->num_elements;
	cparms.size = sizeof(struct tf_shadow_ident_element);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	shadow_db->db = (struct tf_shadow_ident_element *)cparms.mem_va;
	shadow_db->num_entries = parms->num_elements;

	db = shadow_db->db;
	for (i = 0; i < parms->num_elements; i++) {
		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (parms->cfg->alloc_cnt[i] == 0)
			continue;

		/* Create array */
		cparms.nitems = parms->cfg->alloc_cnt[i];
		cparms.size = sizeof(uint32_t);
		rc = tfp_calloc(&cparms);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Array alloc failed, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    i);
			goto fail;
		}
		db[i].ref_count = (uint32_t *)cparms.mem_va;
	}

	*parms->tf_shadow_ident_db = (void *)shadow_db;

	return 0;
fail:
	tfp_free((void *)db->ref_count);
	tfp_free((void *)db);
	tfp_free((void *)shadow_db);
	parms->tf_shadow_ident_db = NULL;

	return -EINVAL;
}

int
tf_shadow_ident_free_db(struct tf_shadow_ident_free_db_parms *parms)
{
	int i;
	struct tf_shadow_ident_db *shadow_db;

	TF_CHECK_PARMS1(parms);

	shadow_db = (struct tf_shadow_ident_db *)parms->tf_shadow_ident_db;
	for (i = 0; i < shadow_db->num_entries; i++)
		tfp_free((void *)shadow_db->db[i].ref_count);

	tfp_free((void *)shadow_db->db);
	tfp_free((void *)parms->tf_shadow_ident_db);

	return 0;
}

int
tf_shadow_ident_search(struct tf_shadow_ident_search_parms *parms)
{
	struct tf_shadow_ident_db *shadow_db;
	uint32_t ref_cnt = 0;

	TF_CHECK_PARMS1(parms);

	shadow_db = (struct tf_shadow_ident_db *)parms->tf_shadow_ident_db;
	ref_cnt = shadow_db->db[parms->type].ref_count[parms->search_id];
	if (ref_cnt > 0) {
		*parms->hit = 1;
		*parms->ref_cnt = ++ref_cnt;
		shadow_db->db[parms->type].ref_count[parms->search_id] =
								ref_cnt;
	} else {
		*parms->hit = 0;
		*parms->ref_cnt = 0;
	}


	return 0;
}

#define ID_REF_CNT_MAX 0xffffffff
int
tf_shadow_ident_insert(struct tf_shadow_ident_insert_parms *parms)
{
	struct tf_shadow_ident_db *shadow_db;

	TF_CHECK_PARMS1(parms);

	shadow_db = (struct tf_shadow_ident_db *)parms->tf_shadow_ident_db;

	/* In case of overflow, ref count keeps the max value */
	if (shadow_db->db[parms->type].ref_count[parms->id] < ID_REF_CNT_MAX)
		shadow_db->db[parms->type].ref_count[parms->id]++;
	else
		TFP_DRV_LOG(ERR,
			    "Identifier %d in type %d reaches the max ref_cnt\n",
			    parms->type,
			    parms->id);

	parms->ref_cnt = shadow_db->db[parms->type].ref_count[parms->id];

	return 0;
}

int
tf_shadow_ident_remove(struct tf_shadow_ident_remove_parms *parms)
{
	struct tf_shadow_ident_db *shadow_db;
	uint32_t ref_cnt = 0;

	TF_CHECK_PARMS1(parms);

	shadow_db = (struct tf_shadow_ident_db *)parms->tf_shadow_ident_db;
	ref_cnt = shadow_db->db[parms->type].ref_count[parms->id];
	if (ref_cnt > 0)
		shadow_db->db[parms->type].ref_count[parms->id]--;
	else
		return -EINVAL;

	*parms->ref_cnt = shadow_db->db[parms->type].ref_count[parms->id];

	return 0;
}
