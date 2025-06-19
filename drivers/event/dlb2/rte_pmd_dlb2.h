/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

/*!
 *  @file      rte_pmd_dlb2.h
 *
 *  @brief     DLB PMD-specific functions
 */

#ifndef _RTE_PMD_DLB2_H_
#define _RTE_PMD_DLB2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>

/**
 * Macro function to get QID depth of rte_event metadata.
 * Currently lower 2 bits of 'rsvd' field are used to store QID depth.
 */
 #define RTE_PMD_DLB2_GET_QID_DEPTH(x) ((x)->rsvd & 0x3)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Selects the token pop mode for a DLB2 port.
 */
enum rte_pmd_dlb2_token_pop_mode {
	/* Pop the CQ tokens immediately after dequeuing. */
	RTE_PMD_DLB2_AUTO_POP,
	/* Pop CQ tokens after (dequeue_depth - 1) events are released.
	 * Supported on load-balanced ports only.
	 */
	RTE_PMD_DLB2_DELAYED_POP,
	/* Pop the CQ tokens during next dequeue operation. */
	RTE_PMD_DLB2_DEFERRED_POP,

	/* NUM_TOKEN_POP_MODES must be last */
	RTE_PMD_DLB2_NUM_TOKEN_POP_MODES
};

/*!
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Configure the token pop mode for a DLB2 port. By default, all ports use
 * RTE_PMD_DLB2_AUTO_POP. This function must be called before calling
 * rte_event_port_setup() for the port, but after calling
 * rte_event_dev_configure().
 *
 * @param dev_id
 *    The identifier of the event device.
 * @param port_id
 *    The identifier of the event port.
 * @param mode
 *    The token pop mode.
 *
 * @return
 * - 0: Success
 * - EINVAL: Invalid parameter dev_id, port_id, or mode
 * - EINVAL: The DLB2 device is not configured or is already running,
 *           or the port is already setup
 */

__rte_experimental
int
rte_pmd_dlb2_set_token_pop_mode(uint8_t dev_id,
				uint8_t port_id,
				enum rte_pmd_dlb2_token_pop_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_DLB2_H_ */
