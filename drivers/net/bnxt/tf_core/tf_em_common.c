/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_em.h"
#include "tf_em_common.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_device.h"
#include "tf_ext_flow_handle.h"
#include "cfa_resource_types.h"

#include "bnxt.h"


/**
 * EM DBs.
 */
void *eem_db[TF_DIR_MAX];

/**
 * Init flag, set on bind and cleared on unbind
 */
static uint8_t init;

/**
 * Host or system
 */
static enum tf_mem_type mem_type;

/** Table scope array */
struct tf_tbl_scope_cb tbl_scopes[TF_NUM_TBL_SCOPE];

/* API defined in tf_em.h */
struct tf_tbl_scope_cb *
tbl_scope_cb_find(uint32_t tbl_scope_id)
{
	int i;
	struct tf_rm_is_allocated_parms parms;
	int allocated;

	/* Check that id is valid */
	parms.rm_db = eem_db[TF_DIR_RX];
	parms.db_index = TF_EM_TBL_TYPE_TBL_SCOPE;
	parms.index = tbl_scope_id;
	parms.allocated = &allocated;

	i = tf_rm_is_allocated(&parms);

	if (i < 0 || allocated != TF_RM_ALLOCATED_ENTRY_IN_USE)
		return NULL;

	for (i = 0; i < TF_NUM_TBL_SCOPE; i++) {
		if (tbl_scopes[i].tbl_scope_id == tbl_scope_id)
			return &tbl_scopes[i];
	}

	return NULL;
}

int
tf_create_tbl_pool_external(enum tf_dir dir,
			    struct tf_tbl_scope_cb *tbl_scope_cb,
			    uint32_t num_entries,
			    uint32_t entry_sz_bytes)
{
	struct tfp_calloc_parms parms;
	uint32_t i;
	int32_t j;
	int rc = 0;
	struct stack *pool = &tbl_scope_cb->ext_act_pool[dir];

	parms.nitems = num_entries;
	parms.size = sizeof(uint32_t);
	parms.alignment = 0;

	if (tfp_calloc(&parms) != 0) {
		TFP_DRV_LOG(ERR, "%s: TBL: external pool failure %s\n",
			    tf_dir_2_str(dir), strerror(ENOMEM));
		return -ENOMEM;
	}

	/* Create empty stack
	 */
	rc = stack_init(num_entries, parms.mem_va, pool);

	if (rc != 0) {
		TFP_DRV_LOG(ERR, "%s: TBL: stack init failure %s\n",
			    tf_dir_2_str(dir), strerror(-rc));
		goto cleanup;
	}

	/* Save the  malloced memory address so that it can
	 * be freed when the table scope is freed.
	 */
	tbl_scope_cb->ext_act_pool_mem[dir] = (uint32_t *)parms.mem_va;

	/* Fill pool with indexes in reverse
	 */
	j = (num_entries - 1) * entry_sz_bytes;

	for (i = 0; i < num_entries; i++) {
		rc = stack_push(pool, j);
		if (rc != 0) {
			TFP_DRV_LOG(ERR, "%s TBL: stack failure %s\n",
				    tf_dir_2_str(dir), strerror(-rc));
			goto cleanup;
		}

		if (j < 0) {
			TFP_DRV_LOG(ERR, "%d TBL: invalid offset (%d)\n",
				    dir, j);
			goto cleanup;
		}
		j -= entry_sz_bytes;
	}

	if (!stack_is_full(pool)) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR, "%s TBL: stack failure %s\n",
			    tf_dir_2_str(dir), strerror(-rc));
		goto cleanup;
	}
	return 0;
cleanup:
	tfp_free((void *)parms.mem_va);
	return rc;
}

/**
 * Destroy External Tbl pool of memory indexes.
 *
 * [in] dir
 *   direction
 * [in] tbl_scope_cb
 *   pointer to the table scope
 */
void
tf_destroy_tbl_pool_external(enum tf_dir dir,
			     struct tf_tbl_scope_cb *tbl_scope_cb)
{
	uint32_t *ext_act_pool_mem =
		tbl_scope_cb->ext_act_pool_mem[dir];

	tfp_free(ext_act_pool_mem);
}

/**
 * Allocate External Tbl entry from the scope pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry allocated - no search support
 *  -ENOMEM -EINVAL -EOPNOTSUPP
 *          - Failure, entry not allocated, out of resources
 */
int
tf_tbl_ext_alloc(struct tf *tfp,
		 struct tf_tbl_alloc_parms *parms)
{
	int rc;
	uint32_t index;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	TF_CHECK_PARMS2(tfp, parms);

	/* Get the pool info from the table scope
	 */
	tbl_scope_cb = tbl_scope_cb_find(parms->tbl_scope_id);

	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, table scope not allocated\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}
	pool = &tbl_scope_cb->ext_act_pool[parms->dir];

	/* Allocate an element
	 */
	rc = stack_pop(pool, &index);

	if (rc != 0) {
		TFP_DRV_LOG(ERR,
		   "%s, Allocation failed, type:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type);
		return rc;
	}

	*parms->idx = index;
	return rc;
}

/**
 * Free External Tbl entry to the scope pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry freed
 *
 * - Failure, entry not successfully freed for these reasons
 *  -ENOMEM
 *  -EOPNOTSUPP
 *  -EINVAL
 */
int
tf_tbl_ext_free(struct tf *tfp,
		struct tf_tbl_free_parms *parms)
{
	int rc = 0;
	uint32_t index;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	TF_CHECK_PARMS2(tfp, parms);

	/* Get the pool info from the table scope
	 */
	tbl_scope_cb = tbl_scope_cb_find(parms->tbl_scope_id);

	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, table scope error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}
	pool = &tbl_scope_cb->ext_act_pool[parms->dir];

	index = parms->idx;

	rc = stack_push(pool, index);

	if (rc != 0) {
		TFP_DRV_LOG(ERR,
		   "%s, consistency error, stack full, type:%d, idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type,
		   index);
	}
	return rc;
}

uint32_t
tf_em_get_key_mask(int num_entries)
{
	uint32_t mask = num_entries - 1;

	if (num_entries & TF_EM_MAX_MASK)
		return 0;

	if (num_entries > TF_EM_MAX_ENTRY)
		return 0;

	return mask;
}

void
tf_em_create_key_entry(struct cfa_p4_eem_entry_hdr *result,
		       uint8_t *in_key,
		       struct cfa_p4_eem_64b_entry *key_entry)
{
	key_entry->hdr.word1 = result->word1;

	if (result->word1 & CFA_P4_EEM_ENTRY_ACT_REC_INT_MASK)
		key_entry->hdr.pointer = result->pointer;
	else
		key_entry->hdr.pointer = result->pointer;

	memcpy(key_entry->key, in_key, TF_HW_EM_KEY_MAX_SIZE + 4);

#ifdef TF_EEM_DEBUG
	dump_raw((uint8_t *)key_entry, TF_EM_KEY_RECORD_SIZE, "Create raw:");
#endif
}

int
tf_em_ext_common_bind(struct tf *tfp,
		      struct tf_em_cfg_parms *parms)
{
	int rc;
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	uint8_t db_exists = 0;

	TF_CHECK_PARMS2(tfp, parms);

	if (init) {
		TFP_DRV_LOG(ERR,
			    "EM Ext DB already initialized\n");
		return -EINVAL;
	}

	db_cfg.type = TF_DEVICE_MODULE_TYPE_EM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->em_cnt[i].cnt;

		/* Check if we got any request to support EEM, if so
		 * we build an EM Ext DB holding Table Scopes.
		 */
		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_TBL_SCOPE] == 0)
			continue;

		db_cfg.rm_db = &eem_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: EM Ext DB creation failed\n",
				    tf_dir_2_str(i));

			return rc;
		}
		db_exists = 1;
	}

	if (db_exists) {
		mem_type = parms->mem_type;
		init = 1;
	}

	return 0;
}

int
tf_em_ext_common_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };

	TF_CHECK_PARMS1(tfp);

	/* Bail if nothing has been initialized */
	if (!init) {
		TFP_DRV_LOG(INFO,
			    "No EM Ext DBs created\n");
		return 0;
	}

	for (i = 0; i < TF_DIR_MAX; i++) {
		fparms.dir = i;
		fparms.rm_db = eem_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		eem_db[i] = NULL;
	}

	init = 0;

	return 0;
}

int tf_tbl_ext_set(struct tf *tfp,
		   struct tf_tbl_set_parms *parms)
{
	if (mem_type == TF_EEM_MEM_TYPE_HOST)
		return tf_tbl_ext_host_set(tfp, parms);
	else
		return tf_tbl_ext_system_set(tfp, parms);
}

int
tf_em_ext_common_alloc(struct tf *tfp,
		       struct tf_alloc_tbl_scope_parms *parms)
{
	if (mem_type == TF_EEM_MEM_TYPE_HOST)
		return tf_em_ext_host_alloc(tfp, parms);
	else
		return tf_em_ext_system_alloc(tfp, parms);
}

int
tf_em_ext_common_free(struct tf *tfp,
		      struct tf_free_tbl_scope_parms *parms)
{
	if (mem_type == TF_EEM_MEM_TYPE_HOST)
		return tf_em_ext_host_free(tfp, parms);
	else
		return tf_em_ext_system_free(tfp, parms);
}
