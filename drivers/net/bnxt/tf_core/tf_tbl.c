/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/param.h>
#include <rte_common.h>
#include <rte_errno.h>
#include "hsi_struct_def_dpdk.h"

#include "tf_core.h"
#include "tf_util.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tfp.h"
#include "hwrm_tf.h"
#include "bnxt.h"
#include "tf_resources.h"
#include "tf_rm.h"
#include "stack.h"
#include "tf_common.h"

/**
 * Internal function to get a Table Entry. Supports all Table Types
 * except the TF_TBL_TYPE_EXT as that is handled as a table scope.
 *
 * [in] tfp
 *   Pointer to TruFlow handle
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
static int
tf_bulk_get_tbl_entry_internal(struct tf *tfp,
			  struct tf_bulk_get_tbl_entry_parms *parms)
{
	int rc;
	int id;
	uint32_t index;
	struct bitalloc *session_pool;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Lookup the pool using the table type of the element */
	rc = tf_rm_lookup_tbl_type_pool(tfs,
					parms->dir,
					parms->type,
					&session_pool);
	/* Error logging handled by tf_rm_lookup_tbl_type_pool */
	if (rc)
		return rc;

	index = parms->starting_idx;

	/*
	 * Adjust the returned index/offset as there is no guarantee
	 * that the start is 0 at time of RM allocation
	 */
	tf_rm_convert_index(tfs,
			    parms->dir,
			    parms->type,
			    TF_RM_CONVERT_RM_BASE,
			    parms->starting_idx,
			    &index);

	/* Verify that the entry has been previously allocated */
	id = ba_inuse(session_pool, index);
	if (id != 1) {
		TFP_DRV_LOG(ERR,
		   "%s, Invalid or not allocated index, type:%d, starting_idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type,
		   index);
		return -EINVAL;
	}

	/* Get the entry */
	rc = tf_msg_bulk_get_tbl_entry(tfp, parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Bulk get failed, type:%d, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    parms->type,
			    strerror(-rc));
	}

	return rc;
}

#if (TF_SHADOW == 1)
/**
 * Allocate Tbl entry from the Shadow DB. Shadow DB is searched for
 * the requested entry. If found the ref count is incremente and
 * returned.
 *
 * [in] tfs
 *   Pointer to session
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count incremented
 *  -ENOENT - Failure, entry not found
 */
static int
tf_alloc_tbl_entry_shadow(struct tf_session *tfs __rte_unused,
			  struct tf_alloc_tbl_entry_parms *parms __rte_unused)
{
	TFP_DRV_LOG(ERR,
		    "%s, Entry Alloc with search not supported\n",
		    tf_dir_2_str(parms->dir));

	return -EOPNOTSUPP;
}

/**
 * Free Tbl entry from the Shadow DB. Shadow DB is searched for
 * the requested entry. If found the ref count is decremente and
 * new ref_count returned.
 *
 * [in] tfs
 *   Pointer to session
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count decremented
 *  -ENOENT - Failure, entry not found
 */
static int
tf_free_tbl_entry_shadow(struct tf_session *tfs,
			 struct tf_free_tbl_entry_parms *parms)
{
	TFP_DRV_LOG(ERR,
		    "%s, Entry Free with search not supported\n",
		    tf_dir_2_str(parms->dir));

	return -EOPNOTSUPP;
}
#endif /* TF_SHADOW */


 /* API defined in tf_core.h */
int
tf_bulk_get_tbl_entry(struct tf *tfp,
		 struct tf_bulk_get_tbl_entry_parms *parms)
{
	int rc = 0;

	TF_CHECK_PARMS_SESSION(tfp, parms);

	if (parms->type == TF_TBL_TYPE_EXT) {
		/* Not supported, yet */
		TFP_DRV_LOG(ERR,
			    "%s, External table type not supported\n",
			    tf_dir_2_str(parms->dir));

		rc = -EOPNOTSUPP;
	} else {
		/* Internal table type processing */
		rc = tf_bulk_get_tbl_entry_internal(tfp, parms);
		if (rc)
			TFP_DRV_LOG(ERR,
				    "%s, Bulk get failed, type:%d, rc:%s\n",
				    tf_dir_2_str(parms->dir),
				    parms->type,
				    strerror(-rc));
	}

	return rc;
}
