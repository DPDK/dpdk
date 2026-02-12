/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 Broadcom
 * All rights reserved.
 */

#include <errno.h>

#include "tfp.h"
#include "tf_tcam.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_device.h"
#include "tf_tcam_mgr_msg.h"

/*
 * Table to convert TCAM type to logical TCAM type for applications.
 * Index is tf_tcam_tbl_type.
 */
static enum cfa_tcam_mgr_tbl_type tcam_types[TF_TCAM_TBL_TYPE_MAX] = {
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH] =
		CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW]  =
		CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW,
	[TF_TCAM_TBL_TYPE_PROF_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM,
	[TF_TCAM_TBL_TYPE_WC_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM,
	[TF_TCAM_TBL_TYPE_SP_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_SP_TCAM,
	[TF_TCAM_TBL_TYPE_CT_RULE_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM,
	[TF_TCAM_TBL_TYPE_VEB_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM,
	[TF_TCAM_TBL_TYPE_WC_TCAM_HIGH]      =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH,
	[TF_TCAM_TBL_TYPE_WC_TCAM_LOW]       =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW,
};

static uint16_t hcapi_type[TF_TCAM_TBL_TYPE_MAX];

int
tf_tcam_mgr_bind_msg(struct tf *tfp,
		     struct tf_dev_info *dev __rte_unused,
		     struct tf_tcam_cfg_parms *parms,
		     struct tf_resource_info resv_res[][TF_TCAM_TBL_TYPE_MAX]
		     __rte_unused
	)
{
	struct tf_rm_resc_entry
		mgr_resv_res[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	struct cfa_tcam_mgr_cfg_parms mgr_parms;
	int dir, rc;
	int type;

	if (parms->num_elements != TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "Invalid number of elements in bind request.\n");
		TFP_DRV_LOG(ERR,
			    "Expected %d, received %d.\n",
			    TF_TCAM_TBL_TYPE_MAX,
			    parms->num_elements);
		return -EINVAL;
	}

	for (type = 0; type < TF_TCAM_TBL_TYPE_MAX; type++)
		hcapi_type[type] = parms->cfg[type].hcapi_type;

	memset(&mgr_parms, 0, sizeof(mgr_parms));

	mgr_parms.num_elements = CFA_TCAM_MGR_TBL_TYPE_MAX;

	/* Convert the data to logical tables */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		for (type = 0; type < TF_TCAM_TBL_TYPE_MAX; type++) {
			mgr_parms.tcam_cnt[dir][tcam_types[type]] =
				parms->resources->tcam_cnt[dir].cnt[type];
			mgr_resv_res[dir][tcam_types[type]].start =
				resv_res[dir][type].start;
			mgr_resv_res[dir][tcam_types[type]].stride =
				resv_res[dir][type].stride;
		}
	}
	mgr_parms.resv_res = mgr_resv_res;

	rc = cfa_tcam_mgr_bind(tfp, &mgr_parms);

	return rc;
}

int
tf_tcam_mgr_unbind_msg(struct tf *tfp,
		       struct tf_dev_info *dev __rte_unused)
{
	return cfa_tcam_mgr_unbind(tfp);
}

int
tf_tcam_mgr_alloc_msg(struct tf *tfp,
		      struct tf_dev_info *dev __rte_unused,
		      struct tf_tcam_alloc_parms *parms)
{
	struct cfa_tcam_mgr_alloc_parms mgr_parms;
	int rc;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	mgr_parms.dir	     = parms->dir;
	mgr_parms.type	     = tcam_types[parms->type];
	mgr_parms.hcapi_type = hcapi_type[parms->type];
	mgr_parms.key_size   = parms->key_size;
	if (parms->priority > TF_TCAM_PRIORITY_MAX)
		mgr_parms.priority = 0;
	else
		mgr_parms.priority = TF_TCAM_PRIORITY_MAX - parms->priority - 1;

	rc = cfa_tcam_mgr_alloc(tfp, &mgr_parms);
	if (rc)
		return rc;

	parms->idx = mgr_parms.id;
	return 0;
}

int
tf_tcam_mgr_free_msg(struct tf *tfp,
		     struct tf_dev_info *dev __rte_unused,
		     struct tf_tcam_free_parms *parms)
{
	struct cfa_tcam_mgr_free_parms mgr_parms;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	mgr_parms.dir	     = parms->dir;
	mgr_parms.type	     = tcam_types[parms->type];
	mgr_parms.hcapi_type = hcapi_type[parms->type];
	mgr_parms.id	     = parms->idx;

	return cfa_tcam_mgr_free(tfp, &mgr_parms);
}

int
tf_tcam_mgr_set_msg(struct tf *tfp,
		    struct tf_dev_info *dev __rte_unused,
		    struct tf_tcam_set_parms *parms)
{
	struct cfa_tcam_mgr_set_parms mgr_parms;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	mgr_parms.dir	      = parms->dir;
	mgr_parms.type	      = tcam_types[parms->type];
	mgr_parms.hcapi_type  = hcapi_type[parms->type];
	mgr_parms.id	      = parms->idx;
	mgr_parms.key	      = parms->key;
	mgr_parms.mask	      = parms->mask;
	mgr_parms.key_size    = parms->key_size;
	mgr_parms.result      = parms->result;
	mgr_parms.result_size = parms->result_size;

	return cfa_tcam_mgr_set(tfp, &mgr_parms);
}

int
tf_tcam_mgr_get_msg(struct tf *tfp,
		    struct tf_dev_info *dev __rte_unused,
		    struct tf_tcam_get_parms *parms)
{
	struct cfa_tcam_mgr_get_parms mgr_parms;
	int rc;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	mgr_parms.dir	      = parms->dir;
	mgr_parms.type	      = tcam_types[parms->type];
	mgr_parms.hcapi_type  = hcapi_type[parms->type];
	mgr_parms.id	      = parms->idx;
	mgr_parms.key	      = parms->key;
	mgr_parms.mask	      = parms->mask;
	mgr_parms.key_size    = parms->key_size;
	mgr_parms.result      = parms->result;
	mgr_parms.result_size = parms->result_size;

	rc = cfa_tcam_mgr_get(tfp, &mgr_parms);
	if (rc)
		return rc;

	parms->key_size	   = mgr_parms.key_size;
	parms->result_size = mgr_parms.result_size;

	return rc;
}

int
tf_tcam_mgr_shared_clear_msg(struct tf *tfp,
		     struct tf_clear_tcam_shared_entries_parms *parms)
{
	struct cfa_tcam_mgr_shared_clear_parms mgr_parms;

	mgr_parms.dir = parms->dir;
	mgr_parms.type = tcam_types[parms->tcam_tbl_type];

	return cfa_tcam_mgr_shared_clear(tfp, &mgr_parms);
}

int
tf_tcam_mgr_shared_move_msg(struct tf *tfp,
		     struct tf_move_tcam_shared_entries_parms *parms)
{
	struct cfa_tcam_mgr_shared_move_parms mgr_parms;

	mgr_parms.dir = parms->dir;
	mgr_parms.type = tcam_types[parms->tcam_tbl_type];

	return cfa_tcam_mgr_shared_move(tfp, &mgr_parms);
}
