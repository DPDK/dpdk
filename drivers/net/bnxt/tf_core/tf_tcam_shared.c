/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>

#include "tf_core.h"

#include "tf_tcam_shared.h"
#include "tf_tcam.h"
#include "tf_common.h"
#include "tf_util.h"
#include "tf_rm.h"
#include "tf_device.h"
#include "tfp.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "bitalloc.h"
#include "tf_tcam_mgr_msg.h"

/**
 * tf_tcam_shared_bind
 */
int
tf_tcam_shared_bind(struct tf *tfp,
		    struct tf_tcam_cfg_parms *parms)
{
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	/* Perform normal bind
	 */
	rc = tf_tcam_bind(tfp, parms);
	return rc;

}
/**
 * tf_tcam_shared_unbind
 */
int
tf_tcam_shared_unbind(struct tf *tfp)
{
	int rc;

	TF_CHECK_PARMS1(tfp);

	rc = tf_tcam_unbind(tfp);
	return rc;
}

/**
 * tf_tcam_shared_alloc
 */
int
tf_tcam_shared_alloc(struct tf *tfp,
		     struct tf_tcam_alloc_parms *parms)
{
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_tcam_alloc(tfp, parms);
	return rc;
}

int
tf_tcam_shared_free(struct tf *tfp,
		    struct tf_tcam_free_parms *parms)
{
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_tcam_free(tfp, parms);
	return rc;
}

int
tf_tcam_shared_set(struct tf *tfp __rte_unused,
		   struct tf_tcam_set_parms *parms __rte_unused)
{
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_tcam_set(tfp, parms);
	return rc;
}

int
tf_tcam_shared_get(struct tf *tfp __rte_unused,
		   struct tf_tcam_get_parms *parms)
{
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	rc = tf_tcam_get(tfp, parms);
	return rc;
}

/** Move all shared WC TCAM entries from the high pool into the low pool
 *  and clear out the high pool entries.
 */
static
int tf_tcam_shared_move(struct tf *tfp,
			struct tf_move_tcam_shared_entries_parms *parms)
{
	struct tf_session *tfs;
	int rc;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or one of our
	 * special types
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		TFP_DRV_LOG(ERR,
			    "%s: Session must be shared with HI/LO type\n",
			    tf_dir_2_str(parms->dir));
		return -EOPNOTSUPP;
	}

	rc = tf_tcam_mgr_shared_move_msg(tfp, parms);
	return rc;
}

int
tf_tcam_shared_move_p4(struct tf *tfp,
		       struct tf_move_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	rc = tf_tcam_shared_move(tfp,
				 parms);
	return rc;
}

int
tf_tcam_shared_move_p58(struct tf *tfp,
			struct tf_move_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	rc = tf_tcam_shared_move(tfp,
				 parms);
	return rc;
}

int
tf_tcam_shared_clear(struct tf *tfp,
		     struct tf_clear_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	if (!tf_session_is_shared_session(tfs) ||
	    (parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW))
		return -EOPNOTSUPP;

	rc = tf_tcam_mgr_shared_clear_msg(tfp, parms);
	return rc;
}
