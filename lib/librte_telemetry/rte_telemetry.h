/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdint.h>

#ifndef _RTE_TELEMETRY_H_
#define _RTE_TELEMETRY_H_

/**
 * @file
 * RTE Telemetry
 *
 * The telemetry library provides a method to retrieve statistics from
 * DPDK by sending a JSON encoded message over a socket. DPDK will send
 * a JSON encoded response containing telemetry data.
 ***/

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Initialize Telemetry
 *
 * @return
 *  0 on successful initialisation.
 * @return
 *  -ENOMEM on memory allocation error
 * @return
 *  -EPERM on unknown error failure
 * @return
 *  -EALREADY if Telemetry is already initialised.
 */
int32_t __rte_experimental
rte_telemetry_init(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Clean up and free memory.
 *
 * @return
 *  0 on success
 * @return
 *  -EPERM on failure
 */
int32_t __rte_experimental
rte_telemetry_cleanup(void);

#endif
