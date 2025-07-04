/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_P4_H
#define CFA_TCAM_MGR_P4_H

#include "cfa_tcam_mgr_device.h"

int
cfa_tcam_mgr_init_p4(struct tf *tfp);

void
cfa_tcam_mgr_uninit_p4(struct tf *tfp);

int
cfa_tcam_mgr_hwops_get_funcs_p4(struct cfa_tcam_mgr_hwops_funcs *hwop_funcs);
#endif /* CFA_TCAM_MGR_P4_H */
