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
 * Obtain RVU LF device PF func
 *
 * @param dev_id
 *   device id of RVU LF device
 *
 * @return
 *   Returns RVU LF pf_func on success, 0 in case of invalid pf_func.
 */
__rte_internal
uint16_t rte_pmd_rvu_lf_pf_func_get(uint8_t dev_id);

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

/**
 * Signature of callback function called when a message process handler is called
 * on RVU LF device.
 *
 * @param vf
 *   VF number(0 to N) from which message is received (ignored in case of PF)
 * @param msg_id
 *   message id
 * @param req
 *   pointer to message request
 * @param req_len
 *   pointer to message request
 * @param[out] rsp
 *   pointer to message response
 * @param[out] rsp_len
 *   length of message response
 *
 * @return 0 when response is set, negative value otherwise
 */
typedef int (*rte_pmd_rvu_lf_msg_handler_cb_fn)(uint16_t vf, uint16_t msg_id,
					    void *req, uint16_t req_len,
					    void **rsp, uint16_t *rsp_len);

/**
 * Register message handler callback
 *
 * Registers message handler callback to be executed when the message is received from peer.
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param cb
 *   callback function to be executed
 *
 * @return 0 on success, negative value otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_msg_handler_register(uint8_t dev_id, rte_pmd_rvu_lf_msg_handler_cb_fn cb);

/**
 * Unregister message handler callback
 *
 * @param dev_id
 *   device id of RVU LF device
 *
 * @return 0 on success, negative value otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_msg_handler_unregister(uint8_t dev_id);

/**
 * Set RVU mailbox message id range.
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param from
 *   starting message id for RVU mailbox (> 0x1FF)
 * @param to
 *   last message id for RVU mailbox (< 0xFFFF)
 *
 * @return 0 on success, -EINVAL for invalid range
 */
__rte_internal
int rte_pmd_rvu_lf_msg_id_range_set(uint8_t dev_id, uint16_t from, uint16_t to);

/**
 * Process a RVU mailbox message.
 *
 * Message request and response to be sent/received,
 * need to be allocated/deallocated by application
 * before/after processing the message.
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param vf
 *   VF number(0 to N) in case of PF->VF message. 0 is valid as VF0.
 *   (For VF->PF message, this field is ignored)
 * @param msg_id
 *   message id
 * @param req
 *   pointer to message request data to be sent
 * @param req_len
 *   length of request data
 * @param rsp
 *   pointer to message response expected to be received, NULL if no response
 * @param rsp_len
 *   length of message response expected, 0 if no response
 *
 * @return 0 on success, negative value otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_msg_process(uint8_t dev_id, uint16_t vf, uint16_t msg_id,
			       void *req, uint16_t req_len, void *rsp, uint16_t rsp_len);

/**
 * Get BAR addresses for the RVU LF device.
 *
 * @param dev_id
 *   device id of RVU LF device
 * @param bar_num
 *   BAR number for which address is required
 * @param[out] va
 *    Virtual address of the BAR. 0 if not mapped
 * @param[out] mask
 *    BAR address mask, 0 if not mapped
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
__rte_internal
int rte_pmd_rvu_lf_bar_get(uint8_t dev_id, uint8_t bar_num, size_t *va, size_t *mask);

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_RVU_LF_DRIVER_H_ */
