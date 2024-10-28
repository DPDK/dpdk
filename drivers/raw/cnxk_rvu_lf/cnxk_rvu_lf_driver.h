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

/**
 * Signature of callback function called when an interrupt is received on RVU LF device.
 *
 * @param cb_arg
 *   pointer to the information received on an interrupt
 */
typedef void (*rte_pmd_rvu_lf_intr_callback_fn)(void *cb_arg);

/**
 * Register interrupt callback
 *
 * Registers an interrupt callback to be executed when interrupt is raised.
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param irq
 *   interrupt number for which interrupt will be raised
 * @param cb
 *   callback function to be executed
 * @param cb_arg
 *   argument to be passed to callback function
 *
 * @return 0 on success, negative value otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_irq_register(uint8_t dev_id, unsigned int irq,
				rte_pmd_rvu_lf_intr_callback_fn cb, void *cb_arg);

/**
 * Unregister interrupt callback
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param irq
 *   interrupt number
 * @param cb
 *   callback function registered
 * @param cb_arg
 *   argument to be passed to callback function
 *
 * @return 0 on success, negative value otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_irq_unregister(uint8_t dev_id, unsigned int irq,
				  rte_pmd_rvu_lf_intr_callback_fn cb, void *cb_arg);

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_RVU_LF_DRIVER_H_ */
