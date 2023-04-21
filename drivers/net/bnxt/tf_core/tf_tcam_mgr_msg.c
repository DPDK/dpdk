/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#include <errno.h>

#include "tfp.h"
#include "tf_tcam.h"
#include "cfa_tcam_mgr.h"
#include "tf_tcam_mgr_msg.h"

/*
 * Table to convert TCAM type to logical TCAM type for applications.
 * Index is tf_tcam_tbl_type.
 */
static enum cfa_tcam_mgr_tbl_type tcam_types[TF_TCAM_TBL_TYPE_MAX] = {
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH] =
		CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS,
	[TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW]  =
		CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS,
	[TF_TCAM_TBL_TYPE_PROF_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS,
	[TF_TCAM_TBL_TYPE_WC_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS,
	[TF_TCAM_TBL_TYPE_SP_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS,
	[TF_TCAM_TBL_TYPE_CT_RULE_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS,
	[TF_TCAM_TBL_TYPE_VEB_TCAM]	     =
		CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS,
	[TF_TCAM_TBL_TYPE_WC_TCAM_HIGH]      =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS,
	[TF_TCAM_TBL_TYPE_WC_TCAM_LOW]       =
		CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS,
};

static uint16_t hcapi_type[TF_TCAM_TBL_TYPE_MAX];

/*
 * This is the glue between the core tf_tcam and the TCAM manager.  It is
 * intended to abstract out the location of the TCAM manager so that the core
 * code will be the same if the TCAM manager is in the core or in firmware.
 *
 * If the TCAM manager is in the core, then this file will just translate to
 * TCAM manager APIs.  If TCAM manager is in firmware, then this file will cause
 * messages to be sent (except for bind and unbind).
 */

int
tf_tcam_mgr_qcaps_msg(struct tf *tfp,
		      struct tf_dev_info *dev __rte_unused,
		      uint32_t *rx_tcam_supported,
		      uint32_t *tx_tcam_supported)
{
	struct cfa_tcam_mgr_context   context;
	struct cfa_tcam_mgr_qcaps_parms mgr_parms;
	int rc;

	context.tfp = tfp;
	memset(&mgr_parms, 0, sizeof(mgr_parms));
	rc = cfa_tcam_mgr_qcaps(&context, &mgr_parms);
	if (rc >= 0) {
		*rx_tcam_supported = mgr_parms.rx_tcam_supported;
		*tx_tcam_supported = mgr_parms.tx_tcam_supported;
	}
	return rc;
}

int
tf_tcam_mgr_bind_msg(struct tf *tfp,
		     struct tf_dev_info *dev __rte_unused,
		     struct tf_tcam_cfg_parms *parms,
		     struct tf_resource_info resv_res[][TF_TCAM_TBL_TYPE_MAX]
		     __rte_unused
	)
{
	/* Common Code */
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

	struct cfa_tcam_mgr_context   context;
	struct cfa_tcam_mgr_cfg_parms mgr_parms;
	struct tf_rm_resc_entry
		mgr_resv_res[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	int dir, rc;

	context.tfp = tfp;

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

	rc = cfa_tcam_mgr_bind(&context, &mgr_parms);

	return rc;
}

int
tf_tcam_mgr_unbind_msg(struct tf *tfp,
		       struct tf_dev_info *dev __rte_unused)
{
	struct cfa_tcam_mgr_context context;

	context.tfp = tfp;

	return cfa_tcam_mgr_unbind(&context);
}

int
tf_tcam_mgr_alloc_msg(struct tf *tfp,
		      struct tf_dev_info *dev __rte_unused,
		      struct tf_tcam_alloc_parms *parms)
{
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_alloc_parms mgr_parms;
	int rc;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	context.tfp = tfp;

	mgr_parms.dir	     = parms->dir;
	mgr_parms.type	     = tcam_types[parms->type];
	mgr_parms.hcapi_type = hcapi_type[parms->type];
	mgr_parms.key_size   = parms->key_size;
	if (parms->priority > TF_TCAM_PRIORITY_MAX)
		mgr_parms.priority = 0;
	else
		mgr_parms.priority = TF_TCAM_PRIORITY_MAX - parms->priority - 1;

	rc = cfa_tcam_mgr_alloc(&context, &mgr_parms);
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
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_free_parms mgr_parms;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	context.tfp = tfp;
	mgr_parms.dir	     = parms->dir;
	mgr_parms.type	     = tcam_types[parms->type];
	mgr_parms.hcapi_type = hcapi_type[parms->type];
	mgr_parms.id	     = parms->idx;

	return cfa_tcam_mgr_free(&context, &mgr_parms);
}

int
tf_tcam_mgr_set_msg(struct tf *tfp,
		    struct tf_dev_info *dev __rte_unused,
		    struct tf_tcam_set_parms *parms)
{
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_set_parms mgr_parms;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	context.tfp = tfp;
	mgr_parms.dir	      = parms->dir;
	mgr_parms.type	      = tcam_types[parms->type];
	mgr_parms.hcapi_type  = hcapi_type[parms->type];
	mgr_parms.id	      = parms->idx;
	mgr_parms.key	      = parms->key;
	mgr_parms.mask	      = parms->mask;
	mgr_parms.key_size    = parms->key_size;
	mgr_parms.result      = parms->result;
	mgr_parms.result_size = parms->result_size;

	return cfa_tcam_mgr_set(&context, &mgr_parms);
}

int
tf_tcam_mgr_get_msg(struct tf *tfp,
		    struct tf_dev_info *dev __rte_unused,
		    struct tf_tcam_get_parms *parms)
{
	int rc;
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_get_parms mgr_parms;

	if (parms->type >= TF_TCAM_TBL_TYPE_MAX) {
		TFP_DRV_LOG(ERR,
			    "No such TCAM table %d.\n",
			    parms->type);
		return -EINVAL;
	}

	context.tfp = tfp;
	mgr_parms.dir	      = parms->dir;
	mgr_parms.type	      = tcam_types[parms->type];
	mgr_parms.hcapi_type  = hcapi_type[parms->type];
	mgr_parms.id	      = parms->idx;
	mgr_parms.key	      = parms->key;
	mgr_parms.mask	      = parms->mask;
	mgr_parms.key_size    = parms->key_size;
	mgr_parms.result      = parms->result;
	mgr_parms.result_size = parms->result_size;

	rc = cfa_tcam_mgr_get(&context, &mgr_parms);
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
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_shared_clear_parms mgr_parms;

	context.tfp = tfp;
	mgr_parms.dir = parms->dir;
	mgr_parms.type = tcam_types[parms->tcam_tbl_type];

	return cfa_tcam_mgr_shared_clear(&context, &mgr_parms);
}

int
tf_tcam_mgr_shared_move_msg(struct tf *tfp,
		     struct tf_move_tcam_shared_entries_parms *parms)
{
	struct cfa_tcam_mgr_context context;
	struct cfa_tcam_mgr_shared_move_parms mgr_parms;

	context.tfp = tfp;
	mgr_parms.dir = parms->dir;
	mgr_parms.type = tcam_types[parms->tcam_tbl_type];

	return cfa_tcam_mgr_shared_move(&context, &mgr_parms);
}
