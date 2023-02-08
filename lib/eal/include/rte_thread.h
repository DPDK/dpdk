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
 * Thread function
 *
 * Function pointer to thread start routine.
 *
 * @param arg
 *   Argument passed to rte_thread_create().
 * @return
 *   Thread function exit value.
 */
typedef uint32_t (*rte_thread_func) (void *arg);

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
 * Representation for thread attributes.
 */
typedef struct {
	enum rte_thread_priority priority; /**< thread priority */
#ifdef RTE_HAS_CPUSET
	rte_cpuset_t cpuset; /**< thread affinity */
#endif
} rte_thread_attr_t;

/**
 * TLS key type, an opaque pointer.
 */
typedef struct eal_tls_key *rte_thread_key;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create a new thread that will invoke the 'thread_func' routine.
 *
 * @param thread_id
 *    A pointer that will store the id of the newly created thread.
 *
 * @param thread_attr
 *    Attributes that are used at the creation of the new thread.
 *
 * @param thread_func
 *    The routine that the new thread will invoke when starting execution.
 *
 * @param arg
 *    Argument to be passed to the 'thread_func' routine.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_create(rte_thread_t *thread_id,
		const rte_thread_attr_t *thread_attr,
		rte_thread_func thread_func, void *arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create a control thread.
 *
 * Creates a control thread with the given name and attributes. The
 * affinity of the new thread is based on the CPU affinity retrieved
 * at the time rte_eal_init() was called, the EAL threads are then
 * excluded. If setting the name of the thread fails, the error is
 * ignored and a debug message is logged.
 *
 * @param thread
 *   Filled with the thread id of the new created thread.
 * @param name
 *   The name of the control thread
 *   (max RTE_MAX_THREAD_NAME_LEN characters including '\0').
 * @param thread_attr
 *   Attributes for the new thread.
 * @param thread_func
 *   Function to be executed by the new thread.
 * @param arg
 *   Argument passed to start_routine.
 * @return
 *   On success, returns 0; on error, it returns a negative value
 *   corresponding to the error number.
 */
__rte_experimental
int
rte_thread_create_control(rte_thread_t *thread, const char *name,
	const rte_thread_attr_t *thread_attr, rte_thread_func thread_func,
	void *arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Waits for the thread identified by 'thread_id' to terminate
 *
 * @param thread_id
 *    The identifier of the thread.
 *
 * @param value_ptr
 *    Stores the exit status of the thread.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_join(rte_thread_t thread_id, uint32_t *value_ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Indicate that the return value of the thread is not needed and
 * all thread resources should be release when the thread terminates.
 *
 * @param thread_id
 *    The id of the thread to be detached.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_detach(rte_thread_t thread_id);

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

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the name of the thread.
 * This API is a noop if the underlying platform does not
 * support setting the thread name or the platform-specific
 * API used to set the thread name fails.
 *
 * @param thread_id
 *    The id of the thread to set name.
 *
 * @param thread_name
 *    The name to set. Truncated to RTE_MAX_THREAD_NAME_LEN,
 *    including terminating NUL if necessary.
 */
__rte_experimental
void
rte_thread_set_name(rte_thread_t thread_id, const char *thread_name);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Check if 2 thread ids are equal.
 *
 * @param t1
 *   First thread id.
 *
 * @param t2
 *   Second thread id.
 *
 * @return
 *   If the ids are equal, return nonzero.
 *   Otherwise, return 0.
 */
__rte_experimental
int rte_thread_equal(rte_thread_t t1, rte_thread_t t2);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize the attributes of a thread.
 * These attributes can be passed to the rte_thread_create() function
 * that will create a new thread and set its attributes according to attr.
 *
 * @param attr
 *   Thread attributes to initialize.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_attr_init(rte_thread_attr_t *attr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the thread priority value in the thread attributes pointed to
 * by 'thread_attr'.
 *
 * @param thread_attr
 *   Points to the thread attributes in which priority will be updated.
 *
 * @param priority
 *   Points to the value of the priority to be set.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_attr_set_priority(rte_thread_attr_t *thread_attr,
		enum rte_thread_priority priority);

#ifdef RTE_HAS_CPUSET

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the CPU affinity value in the thread attributes pointed to
 * by 'thread_attr'.
 *
 * @param thread_attr
 *   Points to the thread attributes in which affinity will be updated.
 *
 * @param cpuset
 *   Points to the value of the affinity to be set.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_attr_set_affinity(rte_thread_attr_t *thread_attr,
		rte_cpuset_t *cpuset);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the value of CPU affinity that is set in the thread attributes pointed
 * to by 'thread_attr'.
 *
 * @param thread_attr
 *   Points to the thread attributes from which affinity will be retrieved.
 *
 * @param cpuset
 *   Pointer to the memory that will store the affinity.
 *
 * @return
 *   On success, return 0.
 *   On failure, return a positive errno-style error number.
 */
__rte_experimental
int rte_thread_attr_get_affinity(rte_thread_attr_t *thread_attr,
		rte_cpuset_t *cpuset);

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
