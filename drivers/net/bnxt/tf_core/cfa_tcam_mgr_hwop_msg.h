/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_HWOP_MSG_H
#define CFA_TCAM_MGR_HWOP_MSG_H

int
cfa_tcam_mgr_hwops_init(enum cfa_tcam_mgr_device_type type);

int
cfa_tcam_mgr_entry_set_msg(int sess_idx,
			   struct cfa_tcam_mgr_context *context,
			   struct cfa_tcam_mgr_set_parms *parms,
			   int row, int slice, int max_slices);
int
cfa_tcam_mgr_entry_get_msg(int sess_idx,
			   struct cfa_tcam_mgr_context *context,
			   struct cfa_tcam_mgr_get_parms *parms,
			   int row, int slice, int max_slices);
int
cfa_tcam_mgr_entry_free_msg(int sess_idx,
			    struct cfa_tcam_mgr_context *context,
			    struct cfa_tcam_mgr_free_parms *parms,
			    int row, int slice, int key_size,
			    int result_size, int max_slices);
#endif  /* CFA_TCAM_MGR_HWOP_MSG_H */
