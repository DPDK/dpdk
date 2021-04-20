/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef _RTE_REGEXDEV_DRIVER_H_
#define _RTE_REGEXDEV_DRIVER_H_

/**
 * @file
 *
 * RTE RegEx Device PMD API
 *
 * APIs that are used by the RegEx drivers, to communicate with the
 * RegEx lib.
 */

#include "rte_regexdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 * Register a RegEx device slot for a RegEx device and return the
 * pointer to that slot.
 *
 * @param name
 *   RegEx device name.
 *
 * @return
 *   A pointer to the RegEx device slot case of success,
 *   NULL otherwise.
 */
struct rte_regexdev *rte_regexdev_register(const char *name);

/**
 * @internal
 * Unregister the specified regexdev port.
 *
 * @param dev
 *   Device to be released.
 */
void rte_regexdev_unregister(struct rte_regexdev *dev);

/**
 * @internal
 * Return the RegEx device based on the device name.
 *
 * @param name
 *   The device name.
 */
struct rte_regexdev *rte_regexdev_get_device_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_REGEXDEV_DRIVER_H_ */
