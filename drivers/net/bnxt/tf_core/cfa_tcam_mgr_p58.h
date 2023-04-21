/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_P58_H
#define CFA_TCAM_MGR_P58_H

#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_sbmp.h"

int
cfa_tcam_mgr_init_p58(int sess_idx, struct cfa_tcam_mgr_entry_data **global_entry_data);

int
cfa_tcam_mgr_sess_table_get_p58(int sess_idx, struct sbmp **session_bmp);

int
cfa_tcam_mgr_hwops_get_funcs_p58(struct cfa_tcam_mgr_hwops_funcs *hwop_funcs);
#endif /* CFA_TCAM_MGR_P58_H */
