/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tfp.h"
#include "lookup3.h"
#include "tf_ext_flow_handle.h"

#include "bnxt.h"


static uint32_t tf_em_get_key_mask(int num_entries)
{
	uint32_t mask = num_entries - 1;

	if (num_entries & 0x7FFF)
		return 0;

	if (num_entries > (128 * 1024 * 1024))
		return 0;

	return mask;
}

static void tf_em_create_key_entry(struct cfa_p4_eem_entry_hdr *result,
				   uint8_t	       *in_key,
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

/** insert EEM entry API
 *
 * returns:
 *  0
 *  TF_ERR	    - unable to get lock
 *
 * insert callback returns:
 *   0
 *   TF_ERR_EM_DUP  - key is already in table
 */
static int tf_insert_eem_entry(struct tf_tbl_scope_cb	   *tbl_scope_cb,
			       struct tf_insert_em_entry_parms *parms)
{
	uint32_t	   mask;
	uint32_t	   key0_hash;
	uint32_t	   key1_hash;
	uint32_t	   key0_index;
	uint32_t	   key1_index;
	struct cfa_p4_eem_64b_entry key_entry;
	uint32_t	   index;
	enum hcapi_cfa_em_table_type table_type;
	uint32_t	   gfid;
	struct hcapi_cfa_hwop op;
	struct hcapi_cfa_key_tbl key_tbl;
	struct hcapi_cfa_key_data key_obj;
	struct hcapi_cfa_key_loc key_loc;
	uint64_t big_hash;
	int rc;

	/* Get mask to use on hash */
	mask = tf_em_get_key_mask(tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY0_TABLE].num_entries);

	if (!mask)
		return -EINVAL;

#ifdef TF_EEM_DEBUG
	dump_raw((uint8_t *)parms->key, TF_HW_EM_KEY_MAX_SIZE + 4, "In Key");
#endif

	big_hash = hcapi_cfa_key_hash((uint64_t *)parms->key,
				      (TF_HW_EM_KEY_MAX_SIZE + 4) * 8);
	key0_hash = (uint32_t)(big_hash >> 32);
	key1_hash = (uint32_t)(big_hash & 0xFFFFFFFF);

	key0_index = key0_hash & mask;
	key1_index = key1_hash & mask;

#ifdef TF_EEM_DEBUG
	TFP_DRV_LOG(DEBUG, "Key0 hash:0x%08x\n", key0_hash);
	TFP_DRV_LOG(DEBUG, "Key1 hash:0x%08x\n", key1_hash);
#endif
	/*
	 * Use the "result" arg to populate all of the key entry then
	 * store the byte swapped "raw" entry in a local copy ready
	 * for insertion in to the table.
	 */
	tf_em_create_key_entry((struct cfa_p4_eem_entry_hdr *)parms->em_record,
				((uint8_t *)parms->key),
				&key_entry);

	/*
	 * Try to add to Key0 table, if that does not work then
	 * try the key1 table.
	 */
	index = key0_index;
	op.opcode = HCAPI_CFA_HWOPS_ADD;
	key_tbl.base0 = (uint8_t *)
	&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY0_TABLE];
	key_obj.offset = (index * TF_EM_KEY_RECORD_SIZE) % TF_EM_PAGE_SIZE;
	key_obj.data = (uint8_t *)&key_entry;
	key_obj.size = TF_EM_KEY_RECORD_SIZE;

	rc = hcapi_cfa_key_hw_op(&op,
				 &key_tbl,
				 &key_obj,
				 &key_loc);

	if (rc == 0) {
		table_type = TF_KEY0_TABLE;
	} else {
		index = key1_index;

		key_tbl.base0 = (uint8_t *)
		&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY1_TABLE];
		key_obj.offset =
			(index * TF_EM_KEY_RECORD_SIZE) % TF_EM_PAGE_SIZE;

		rc = hcapi_cfa_key_hw_op(&op,
					 &key_tbl,
					 &key_obj,
					 &key_loc);
		if (rc != 0)
			return rc;

		table_type = TF_KEY1_TABLE;
	}

	TF_SET_GFID(gfid,
		    index,
		    table_type);
	TF_SET_FLOW_ID(parms->flow_id,
		       gfid,
		       TF_GFID_TABLE_EXTERNAL,
		       parms->dir);
	TF_SET_FIELDS_IN_FLOW_HANDLE(parms->flow_handle,
				     0,
				     0,
				     0,
				     index,
				     0,
				     table_type);

	return 0;
}

/**
 * Insert EM internal entry API
 *
 *  returns:
 *     0 - Success
 */
static int tf_insert_em_internal_entry(struct tf                       *tfp,
				       struct tf_insert_em_entry_parms *parms)
{
	int       rc;
	uint32_t  gfid;
	uint16_t  rptr_index = 0;
	uint8_t   rptr_entry = 0;
	uint8_t   num_of_entries = 0;
	struct tf_session *session =
		(struct tf_session *)(tfp->session->core_data);
	struct stack *pool = &session->em_pool[parms->dir];
	uint32_t index;

	rc = stack_pop(pool, &index);

	if (rc != 0) {
		TFP_DRV_LOG(ERR,
		   "dir:%d, EM entry index allocation failed\n",
		   parms->dir);
		return rc;
	}

	rptr_index = index * TF_SESSION_EM_ENTRY_SIZE;
	rc = tf_msg_insert_em_internal_entry(tfp,
					     parms,
					     &rptr_index,
					     &rptr_entry,
					     &num_of_entries);
	if (rc != 0)
		return -1;

	PMD_DRV_LOG(ERR,
		   "Internal entry @ Index:%d rptr_index:0x%x rptr_entry:0x%x num_of_entries:%d\n",
		   index * TF_SESSION_EM_ENTRY_SIZE,
		   rptr_index,
		   rptr_entry,
		   num_of_entries);

	TF_SET_GFID(gfid,
		    ((rptr_index << TF_EM_INTERNAL_INDEX_SHIFT) |
		     rptr_entry),
		    0); /* N/A for internal table */

	TF_SET_FLOW_ID(parms->flow_id,
		       gfid,
		       TF_GFID_TABLE_INTERNAL,
		       parms->dir);

	TF_SET_FIELDS_IN_FLOW_HANDLE(parms->flow_handle,
				     num_of_entries,
				     0,
				     0,
				     rptr_index,
				     rptr_entry,
				     0);
	return 0;
}

/** Delete EM internal entry API
 *
 * returns:
 * 0
 * -EINVAL
 */
static int tf_delete_em_internal_entry(struct tf                       *tfp,
				       struct tf_delete_em_entry_parms *parms)
{
	int rc;
	struct tf_session *session =
		(struct tf_session *)(tfp->session->core_data);
	struct stack *pool = &session->em_pool[parms->dir];

	rc = tf_msg_delete_em_entry(tfp, parms);

	/* Return resource to pool */
	if (rc == 0)
		stack_push(pool, parms->index / TF_SESSION_EM_ENTRY_SIZE);

	return rc;
}


/** delete EEM hash entry API
 *
 * returns:
 *   0
 *   -EINVAL	  - parameter error
 *   TF_NO_SESSION    - bad session ID
 *   TF_ERR_TBL_SCOPE - invalid table scope
 *   TF_ERR_TBL_IF    - invalid table interface
 *
 * insert callback returns
 *   0
 *   TF_NO_EM_MATCH - entry not found
 */
static int tf_delete_eem_entry(struct tf_tbl_scope_cb *tbl_scope_cb,
			       struct tf_delete_em_entry_parms *parms)
{
	enum hcapi_cfa_em_table_type hash_type;
	uint32_t index;
	struct hcapi_cfa_hwop op;
	struct hcapi_cfa_key_tbl key_tbl;
	struct hcapi_cfa_key_data key_obj;
	struct hcapi_cfa_key_loc key_loc;
	int rc;

	if (parms->flow_handle == 0)
		return -EINVAL;

	TF_GET_HASH_TYPE_FROM_FLOW_HANDLE(parms->flow_handle, hash_type);
	TF_GET_INDEX_FROM_FLOW_HANDLE(parms->flow_handle, index);

	op.opcode = HCAPI_CFA_HWOPS_DEL;
	key_tbl.base0 = (uint8_t *)
	&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[(hash_type == 0 ?
							  TF_KEY0_TABLE :
							  TF_KEY1_TABLE)];
	key_obj.offset = (index * TF_EM_KEY_RECORD_SIZE) % TF_EM_PAGE_SIZE;
	key_obj.data = NULL;
	key_obj.size = TF_EM_KEY_RECORD_SIZE;

	rc = hcapi_cfa_key_hw_op(&op,
				 &key_tbl,
				 &key_obj,
				 &key_loc);

	if (!rc)
		return rc;

	return 0;
}

/** insert EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_em_insert_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms)
{
	struct tf_tbl_scope_cb *tbl_scope_cb;

	tbl_scope_cb = tbl_scope_cb_find
		((struct tf_session *)(tfp->session->core_data),
		parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Invalid tbl_scope_cb\n");
		return -EINVAL;
	}

	/* Process the EM entry per Table Scope type */
	if (parms->mem == TF_MEM_EXTERNAL)
		/* External EEM */
		return tf_insert_eem_entry
			(tbl_scope_cb, parms);
	else if (parms->mem == TF_MEM_INTERNAL)
		/* Internal EM */
		return tf_insert_em_internal_entry(tfp,	parms);

	return -EINVAL;
}

/** Delete EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_em_delete_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms)
{
	struct tf_tbl_scope_cb *tbl_scope_cb;

	tbl_scope_cb = tbl_scope_cb_find
		((struct tf_session *)(tfp->session->core_data),
		parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Invalid tbl_scope_cb\n");
		return -EINVAL;
	}
	if (parms->mem == TF_MEM_EXTERNAL)
		return tf_delete_eem_entry(tbl_scope_cb, parms);
	else if (parms->mem == TF_MEM_INTERNAL)
		return tf_delete_em_internal_entry(tfp, parms);

	return -EINVAL;
}
