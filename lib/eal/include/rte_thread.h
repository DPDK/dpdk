/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Mellanox Technologies, Ltd
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <stdint.h>

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
 * Thread id descriptor.
 */
typedef struct {
	uintptr_t opaque_id; /**< thread identifier */
} rte_thread_t;

/**
 * Thread priority values.
 */
enum rte_thread_priority {
	RTE_THREAD_PRIORITY_NORMAL            = 0,
	/**< normal thread priority, the default */
	RTE_THREAD_PRIORITY_REALTIME_CRITICAL = 1,
	/**< highest thread priority allowed */
};

/**
 * TLS key type, an opaque pointer.
 */
typedef struct eal_tls_key *rte_thread_key;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the id of the calling thread.
 *
 * @return
 *   Return the thread id of the calling thread.
 */
__rte_experimental
rte_thread_t rte_thread_self(void);

#ifdef RTE_HAS_CPUSET

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the affinity of thread 'thread_id' to the cpu set
 * specified by 'cpuset'.
 *
 * @param thread_id
 *    Id of the thread for which to set the affinity.
 *
 * @param cpuset
 *   Pointer to CPU affinity to set.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_set_affinity_by_id(rte_thread_t thread_id,
		const rte_cpuset_t *cpuset);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the affinity of thread 'thread_id' and store it
 * in 'cpuset'.
 *
 * @param thread_id
 *    Id of the thread for which to get the affinity.
 *
 * @param cpuset
 *   Pointer for storing the affinity value.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_get_affinity_by_id(rte_thread_t thread_id,
		rte_cpuset_t *cpuset);

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
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the priority of a thread.
 *
 * @param thread_id
 *   Id of the thread for which to get priority.
 *
 * @param priority
 *   Location to store the retrieved priority.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_get_priority(rte_thread_t thread_id,
		enum rte_thread_priority *priority);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the priority of a thread.
 *
 * @param thread_id
 *   Id of the thread for which to set priority.
 *
 * @param priority
 *   Priority value to be set.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_set_priority(rte_thread_t thread_id,
		enum rte_thread_priority priority);

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
