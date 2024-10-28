/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _CNXK_RVU_LF_DRIVER_H_
#define _CNXK_RVU_LF_DRIVER_H_

/**
 * @file cnxk_rvu_lf_driver.h
 *
 * Marvell RVU LF raw PMD specific structures and interface
 *
 * This API allows out of tree driver to manage RVU LF device
 * It enables operations such as  sending/receiving mailbox,
 * register and notify the interrupts etc
 */

#include <stdint.h>

#include <rte_compat.h>
#include <rte_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Obtain NPA PF func
 *
 * @return
 *   Returns NPA pf_func on success, 0 in case of invalid pf_func.
 */
__rte_internal
uint16_t rte_pmd_rvu_lf_npa_pf_func_get(void);

/**
 * Obtain SSO PF func
 *
 * @return
 *   Returns SSO pf_func on success, 0 in case of invalid pf_func.
 */
__rte_internal
uint16_t rte_pmd_rvu_lf_sso_pf_func_get(void);

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_RVU_LF_DRIVER_H_ */
