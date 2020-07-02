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
#include "tf_em_common.h"
#include "tf_msg.h"
#include "tfp.h"
#include "lookup3.h"
#include "tf_ext_flow_handle.h"

#include "bnxt.h"


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
static int
tf_insert_eem_entry(struct tf_tbl_scope_cb *tbl_scope_cb __rte_unused,
		    struct tf_insert_em_entry_parms *parms __rte_unused)
{
	return 0;
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
static int
tf_delete_eem_entry(struct tf_tbl_scope_cb *tbl_scope_cb __rte_unused,
		    struct tf_delete_em_entry_parms *parms __rte_unused)
{
	return 0;
}

/** insert EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int
tf_em_insert_ext_sys_entry(struct tf *tfp,
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

	return tf_insert_eem_entry
		(tbl_scope_cb, parms);
}

/** Delete EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int
tf_em_delete_ext_sys_entry(struct tf *tfp,
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

	return tf_delete_eem_entry(tbl_scope_cb, parms);
}

int
tf_em_ext_system_alloc(struct tf *tfp __rte_unused,
		       struct tf_alloc_tbl_scope_parms *parms __rte_unused)
{
	return 0;
}

int
tf_em_ext_system_free(struct tf *tfp __rte_unused,
		      struct tf_free_tbl_scope_parms *parms __rte_unused)
{
	return 0;
}
