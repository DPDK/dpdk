/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _PMD_DPAA_H_
#define _PMD_DPAA_H_

/**
 * @file rte_pmd_dpaa.h
 *
 * NXP dpaa PMD specific functions.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

/**
 * Enable/Disable TX loopback
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param on
 *    1 - Enable TX loopback.
 *    0 - Disable TX loopback.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int
rte_pmd_dpaa_set_tx_loopback(uint16_t port, uint8_t on);

#endif /* _PMD_DPAA_H_ */
