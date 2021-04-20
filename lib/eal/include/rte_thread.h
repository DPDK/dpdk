/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Mellanox Technologies, Ltd
 */

#include <rte_os.h>
#include <rte_compat.h>

#ifndef _RTE_THREAD_H_
#define _RTE_THREAD_H_

/**
 * @file
 *
 * Threading functions
 *
 * Simple threads functionality supplied by EAL.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TLS key type, an opaque pointer.
 */
typedef struct eal_tls_key *rte_thread_key;

#ifdef RTE_HAS_CPUSET

/**
 * Set core affinity of the current thread.
 * Support both EAL and non-EAL thread and update TLS.
 *
 * @param cpusetp
 *   Pointer to CPU affinity to set.
 * @return
 *   On success, return 0; otherwise return -1;
 */
int rte_thread_set_affinity(rte_cpuset_t *cpusetp);

/**
 * Get core affinity of the current thread.
 *
 * @param cpusetp
 *   Pointer to CPU affinity of current thread.
 *   It presumes input is not NULL, otherwise it causes panic.
 *
 */
void rte_thread_get_affinity(rte_cpuset_t *cpusetp);

#endif /* RTE_HAS_CPUSET */

/**
 * Create a TLS data key visible to all threads in the process.
 * the created key is later used to get/set a value.
 * and optional destructor can be set to be called when a thread exits.
 *
 * @param key
 *   Pointer to store the allocated key.
 * @param destructor
 *   The function to be called when the thread exits.
 *   Ignored on Windows OS.
 *
 * @return
 *   On success, zero.
 *   On failure, a negative number and an error number is set in rte_errno.
 *   rte_errno can be: ENOMEM  - Memory allocation error.
 *                     ENOEXEC - Specific OS error.
 */

__rte_experimental
int rte_thread_key_create(rte_thread_key *key,
			void (*destructor)(void *));

/**
 * Delete a TLS data key visible to all threads in the process.
 *
 * @param key
 *   The key allocated by rte_thread_key_create().
 *
 * @return
 *   On success, zero.
 *   On failure, a negative number and an error number is set in rte_errno.
 *   rte_errno can be: EINVAL  - Invalid parameter passed.
 *                     ENOEXEC - Specific OS error.
 */
__rte_experimental
int rte_thread_key_delete(rte_thread_key key);

/**
 * Set value bound to the TLS key on behalf of the calling thread.
 *
 * @param key
 *   The key allocated by rte_thread_key_create().
 * @param value
 *   The value bound to the rte_thread_key key for the calling thread.
 *
 * @return
 *   On success, zero.
 *   On failure, a negative number and an error number is set in rte_errno.
 *   rte_errno can be: EINVAL  - Invalid parameter passed.
 *                     ENOEXEC - Specific OS error.
 */
__rte_experimental
int rte_thread_value_set(rte_thread_key key, const void *value);

/**
 * Get value bound to the TLS key on behalf of the calling thread.
 *
 * @param key
 *   The key allocated by rte_thread_key_create().
 *
 * @return
 *   On success, value data pointer (can also be NULL).
 *   On failure, NULL and an error number is set in rte_errno.
 *   rte_errno can be: EINVAL  - Invalid parameter passed.
 *                     ENOEXEC - Specific OS error.
 */
__rte_experimental
void *rte_thread_value_get(rte_thread_key key);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THREAD_H_ */
