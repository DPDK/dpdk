/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
 */

#ifndef RTE_VDEV_H
#define RTE_VDEV_H

/**
 * @file
 * RTE virtual bus API
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rte_vdev_scan_callback)(void *user_arg);

/**
 * Add a callback to be called on vdev scan
 * before reading the devargs list.
 *
 * This function cannot be called in a scan callback
 * because of deadlock.
 *
 * @param callback
 *   The function to be called which can update the devargs list.
 * @param user_arg
 *   An opaque pointer passed to callback.
 * @return
 *   0 on success, negative on error
 */
int
rte_vdev_add_custom_scan(rte_vdev_scan_callback callback, void *user_arg);

/**
 * Remove a registered scan callback.
 *
 * This function cannot be called in a scan callback
 * because of deadlock.
 *
 * @param callback
 *   The registered function to be removed.
 * @param user_arg
 *   The associated opaque pointer or (void*)-1 for any.
 * @return
 *   0 on success
 */
int
rte_vdev_remove_custom_scan(rte_vdev_scan_callback callback, void *user_arg);

/**
 * Initialize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @param args
 *   The pointer to arguments used by driver initialization.
 * @return
 *  0 on success, negative on error
 */
int rte_vdev_init(const char *name, const char *args);

/**
 * Uninitialize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be uninitialized.
 * @return
 *  0 on success, negative on error
 */
int rte_vdev_uninit(const char *name);

#ifdef __cplusplus
}
#endif

#endif
