/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_ext_flow_handle.h"

#include "bnxt.h"

/**
 * EM Pool
 */
extern struct stack em_pool[TF_DIR_MAX];

/**
 * Insert EM internal entry API
 *
 *  returns:
 *     0 - Success
 */
int
tf_em_hash_insert_int_entry(struct tf *tfp,
			    struct tf_insert_em_entry_parms *parms)
{
	int rc;
	uint32_t gfid;
	uint16_t rptr_index = 0;
	uint8_t rptr_entry = 0;
	uint8_t num_of_entries = 0;
	struct stack *pool = &em_pool[parms->dir];
	uint32_t index;
	uint32_t key0_hash;
	uint32_t key1_hash;
	uint64_t big_hash;

	rc = stack_pop(pool, &index);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "%s, EM entry index allocation failed\n",
			    tf_dir_2_str(parms->dir));
		return rc;
	}

	big_hash = hcapi_cfa_key_hash((uint64_t *)parms->key,
				      (TF_HW_EM_KEY_MAX_SIZE + 4) * 8);
	key0_hash = (uint32_t)(big_hash >> 32);
	key1_hash = (uint32_t)(big_hash & 0xFFFFFFFF);

	rptr_index = index;
	rc = tf_msg_hash_insert_em_internal_entry(tfp,
						  parms,
						  key0_hash,
						  key1_hash,
						  &rptr_index,
						  &rptr_entry,
						  &num_of_entries);
	if (rc) {
		/* Free the allocated index before returning */
		stack_push(pool, index);
		return -1;
	}

	PMD_DRV_LOG
		  (DEBUG,
		   "%s, Internal entry @ Index:%d rptr_index:0x%x rptr_entry:0x%x num_of_entries:%d\n",
		   tf_dir_2_str(parms->dir),
		   index,
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
				     (uint32_t)num_of_entries,
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
int
tf_em_hash_delete_int_entry(struct tf *tfp,
			    struct tf_delete_em_entry_parms *parms)
{
	int rc = 0;
	struct stack *pool = &em_pool[parms->dir];

	rc = tf_msg_delete_em_entry(tfp, parms);

	/* Return resource to pool */
	if (rc == 0)
		stack_push(pool, parms->index);

	return rc;
}
