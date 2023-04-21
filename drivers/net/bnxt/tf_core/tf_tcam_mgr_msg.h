/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TCAM_MGR_MSG_H_
#define _TF_TCAM_MGR_MSG_H_

#include "tf_tcam.h"
#include "tf_rm.h"

int
tf_tcam_mgr_qcaps_msg(struct tf *tfp,
		      struct tf_dev_info *dev __rte_unused,
		      uint32_t *rx_tcam_supported,
		      uint32_t *tx_tcam_supported);

int
tf_tcam_mgr_bind_msg(struct tf *tfp,
		     struct tf_dev_info *dev,
		     struct tf_tcam_cfg_parms *parms,
		     struct tf_resource_info resv_res[][TF_TCAM_TBL_TYPE_MAX]);
int
tf_tcam_mgr_unbind_msg(struct tf *tfp,
		       struct tf_dev_info *dev);
int
tf_tcam_mgr_alloc_msg(struct tf *tfp,
		      struct tf_dev_info *dev,
		      struct tf_tcam_alloc_parms *parms);
int
tf_tcam_mgr_free_msg(struct tf *tfp,
		     struct tf_dev_info *dev,
		     struct tf_tcam_free_parms *parms);
int
tf_tcam_mgr_set_msg(struct tf *tfp,
		    struct tf_dev_info *dev,
		    struct tf_tcam_set_parms *parms);
int
tf_tcam_mgr_get_msg(struct tf *tfp,
		    struct tf_dev_info *dev,
		    struct tf_tcam_get_parms *parms);
int
tf_tcam_mgr_shared_clear_msg(struct tf *tfp,
		     struct tf_clear_tcam_shared_entries_parms *parms);

int
tf_tcam_mgr_shared_move_msg(struct tf *tfp,
		     struct tf_move_tcam_shared_entries_parms *parms);
#endif /* _TF_TCAM_MGR_MSG_H_ */
