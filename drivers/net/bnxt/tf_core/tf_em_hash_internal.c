/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
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
#include "tf_device.h"

#include "bnxt.h"

/**
 * EM Pool
 */
#include "dpool.h"

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
	struct dpool *pool;
	uint32_t index;
	uint32_t key0_hash;
	uint32_t key1_hash;
	uint64_t big_hash;
	struct tf_dev_info *dev;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;
	pool = (struct dpool *)tfs->em_pool[parms->dir];
	index = dpool_alloc(pool,
			    parms->em_record_sz_in_bits / 128,
			    DP_DEFRAG_TO_FIT);

	if (index == DP_INVALID_INDEX) {
		PMD_DRV_LOG(ERR,
			    "%s, EM entry index allocation failed\n",
			    tf_dir_2_str(parms->dir));
		return -1;
	}

	if (dev->ops->tf_dev_cfa_key_hash == NULL)
		return -EINVAL;

	big_hash = dev->ops->tf_dev_cfa_key_hash((uint64_t *)parms->key,
					TF_P58_HW_EM_KEY_MAX_SIZE * 8);
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
		dpool_free(pool, index);
		return -1;
	}

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
				     TF_FLAGS_FLOW_HANDLE_INTERNAL,
				     rptr_index,
				     rptr_entry,
				     0);
	dpool_set_entry_data(pool, index, parms->flow_handle);
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
	struct tf_session *tfs;
	struct dpool *pool;
	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_delete_em_entry(tfp, parms);

	/* Return resource to pool */
	if (rc == 0) {
		pool = (struct dpool *)tfs->em_pool[parms->dir];
		dpool_free(pool, parms->index);
	}

	return rc;
}

/** Move EM internal entry API
 *
 * returns:
 * 0
 * -EINVAL
 */
int
tf_em_move_int_entry(struct tf *tfp,
		     struct tf_move_em_entry_parms *parms)
{
	int rc = 0;
	struct dpool *pool;
	struct tf_session *tfs;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to lookup session, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_move_em_entry(tfp, parms);

	/* Return resource to pool */
	if (rc == 0) {
		pool = (struct dpool *)tfs->em_pool[parms->dir];
		dpool_free(pool, parms->index);
	}

	return rc;
}
