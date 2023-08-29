/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell Inc.
 */

/**
 * @file rte_pmd_cnxk_eventdev.h
 * Marvell CNXK eventdev PMD specific functions.
 *
 **/

#ifndef _PMD_CNXK_EVENTDEV_H_
#define _PMD_CNXK_EVENTDEV_H_

#include <rte_common.h>
#include <rte_compat.h>

/**
 * Wait for the currently active flow context on the event port to become HEAD
 * of the flow-chain.
 *
 * @param dev
 *  Event device identifier.
 *
 * @param port
 *   Event port identifier.
 */
__rte_experimental
void
rte_pmd_cnxk_eventdev_wait_head(uint8_t dev, uint8_t port);


/**
 * Check if the currently active flow context on the event port is the HEAD
 * of the flow-chain.
 *
 * @param dev
 *  Event device identifier.
 *
 * @param port
 *   Event port identifier.
 *
 * @return Status of the currently held flow context
 *   0 not the head of the flow-chain
 *   1 head of the flow-chain
 */
__rte_experimental
uint8_t
rte_pmd_cnxk_eventdev_is_head(uint8_t dev, uint8_t port);

#endif
