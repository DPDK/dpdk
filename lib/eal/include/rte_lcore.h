/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_LCORE_H_
#define _RTE_LCORE_H_

/**
 * @file
 *
 * API for lcore and socket manipulation
 *
 */
#include <stdio.h>

#include <rte_compat.h>
#include <rte_config.h>
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_thread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCORE_ID_ANY     UINT32_MAX       /**< Any lcore. */

RTE_DECLARE_PER_LCORE(unsigned, _lcore_id);  /**< Per thread "lcore id". */

/**
 * The lcore role (used in RTE or not).
 */
enum rte_lcore_role_t {
	ROLE_RTE,
	ROLE_OFF,
	ROLE_SERVICE,
	ROLE_NON_EAL,
};

/**
 * Get a lcore's role.
 *
 * @param lcore_id
 *   The identifier of the lcore, which MUST be between 0 and RTE_MAX_LCORE-1.
 * @return
 *   The role of the lcore.
 */
enum rte_lcore_role_t rte_eal_lcore_role(unsigned int lcore_id);

/**
 * Test if the core supplied has a specific role
 *
 * @param lcore_id
 *   The identifier of the lcore, which MUST be between 0 and
 *   RTE_MAX_LCORE-1.
 * @param role
 *   The role to be checked against.
 * @return
 *   Boolean value: positive if test is true; otherwise returns 0.
 */
int
rte_lcore_has_role(unsigned int lcore_id, enum rte_lcore_role_t role);

/**
 * Return the Application thread ID of the execution unit.
 *
 * Note: in most cases the lcore id returned here will also correspond
 *   to the processor id of the CPU on which the thread is pinned, this
 *   will not be the case if the user has explicitly changed the thread to
 *   core affinities using --lcores EAL argument e.g. --lcores '(0-3)@10'
 *   to run threads with lcore IDs 0, 1, 2 and 3 on physical core 10..
 *
 * @return
 *  Logical core ID (in EAL thread or registered non-EAL thread) or
 *  LCORE_ID_ANY (in unregistered non-EAL thread)
 */
static inline unsigned
rte_lcore_id(void)
{
	return RTE_PER_LCORE(_lcore_id);
}

/**
 * Get the id of the main lcore
 *
 * @return
 *   the id of the main lcore
 */
unsigned int rte_get_main_lcore(void);

/**
 * Return the number of execution units (lcores) on the system.
 *
 * @return
 *   the number of execution units (lcores) on the system.
 */
unsigned int rte_lcore_count(void);

/**
 * Return the index of the lcore starting from zero.
 *
 * When option -c or -l is given, the index corresponds
 * to the order in the list.
 * For example:
 * -c 0x30, lcore 4 has index 0, and 5 has index 1.
 * -l 22,18 lcore 22 has index 0, and 18 has index 1.
 *
 * @param lcore_id
 *   The targeted lcore, or -1 for the current one.
 * @return
 *   The relative index, or -1 if not enabled.
 */
int rte_lcore_index(int lcore_id);

/**
 * Return the ID of the physical socket of the logical core we are
 * running on.
 * @return
 *   the ID of current lcoreid's physical socket
 */
unsigned int rte_socket_id(void);

/**
 * Return number of physical sockets detected on the system.
 *
 * Note that number of nodes may not be correspondent to their physical id's:
 * for example, a system may report two socket id's, but the actual socket id's
 * may be 0 and 8.
 *
 * @return
 *   the number of physical sockets as recognized by EAL
 */
unsigned int
rte_socket_count(void);

/**
 * Return socket id with a particular index.
 *
 * This will return socket id at a particular position in list of all detected
 * physical socket id's. For example, on a machine with sockets [0, 8], passing
 * 1 as a parameter will return 8.
 *
 * @param idx
 *   index of physical socket id to return
 *
 * @return
 *   - physical socket id as recognized by EAL
 *   - -1 on error, with errno set to EINVAL
 */
int
rte_socket_id_by_idx(unsigned int idx);

/**
 * Get the ID of the physical socket of the specified lcore
 *
 * @param lcore_id
 *   the targeted lcore, which MUST be between 0 and RTE_MAX_LCORE-1.
 * @return
 *   the ID of lcoreid's physical socket
 */
unsigned int
rte_lcore_to_socket_id(unsigned int lcore_id);

/**
 * Return the id of the lcore on a socket starting from zero.
 *
 * @param lcore_id
 *   The targeted lcore, or -1 for the current one.
 * @return
 *   The relative index, or -1 if not enabled.
 */
int rte_lcore_to_cpu_id(int lcore_id);

#ifdef RTE_HAS_CPUSET

/**
 * Return the cpuset for a given lcore.
 *
 * @param lcore_id
 *   the targeted lcore, which MUST be between 0 and RTE_MAX_LCORE-1.
 * @return
 *   The cpuset of that lcore
 */
rte_cpuset_t rte_lcore_cpuset(unsigned int lcore_id);

#endif /* RTE_HAS_CPUSET */

/**
 * Test if an lcore is enabled.
 *
 * @param lcore_id
 *   The identifier of the lcore, which MUST be between 0 and
 *   RTE_MAX_LCORE-1.
 * @return
 *   True if the given lcore is enabled; false otherwise.
 */
int rte_lcore_is_enabled(unsigned int lcore_id);

/**
 * Get the next enabled lcore ID.
 *
 * @param i
 *   The current lcore (reference).
 * @param skip_main
 *   If true, do not return the ID of the main lcore.
 * @param wrap
 *   If true, go back to 0 when RTE_MAX_LCORE is reached; otherwise,
 *   return RTE_MAX_LCORE.
 * @return
 *   The next lcore_id or RTE_MAX_LCORE if not found.
 */
unsigned int rte_get_next_lcore(unsigned int i, int skip_main, int wrap);

/**
 * Macro to browse all running lcores.
 */
#define RTE_LCORE_FOREACH(i)						\
	for (i = rte_get_next_lcore(-1, 0, 0);				\
	     i < RTE_MAX_LCORE;						\
	     i = rte_get_next_lcore(i, 0, 0))

/**
 * Macro to browse all running lcores except the main lcore.
 */
#define RTE_LCORE_FOREACH_WORKER(i)					\
	for (i = rte_get_next_lcore(-1, 1, 0);				\
	     i < RTE_MAX_LCORE;						\
	     i = rte_get_next_lcore(i, 1, 0))

/**
 * Callback prototype for initializing lcores.
 *
 * @param lcore_id
 *   The lcore to consider.
 * @param arg
 *   An opaque pointer passed at callback registration.
 * @return
 *   - -1 when refusing this operation,
 *   - 0 otherwise.
 */
typedef int (*rte_lcore_init_cb)(unsigned int lcore_id, void *arg);

/**
 * Callback prototype for uninitializing lcores.
 *
 * @param lcore_id
 *   The lcore to consider.
 * @param arg
 *   An opaque pointer passed at callback registration.
 */
typedef void (*rte_lcore_uninit_cb)(unsigned int lcore_id, void *arg);

/**
 * Register callbacks invoked when initializing and uninitializing a lcore.
 *
 * This function calls the init callback with all initialized lcores.
 * Any error reported by the init callback triggers a rollback calling the
 * uninit callback for each lcore.
 * If this step succeeds, the callbacks are put in the lcore callbacks list
 * that will get called for each lcore allocation/release.
 *
 * Note: callbacks execution is serialised under a write lock protecting the
 * lcores and callbacks list.
 *
 * @param name
 *   A name serving as a small description for this callback.
 * @param init
 *   The callback invoked when a lcore_id is initialized.
 *   init can be NULL.
 * @param uninit
 *   The callback invoked when a lcore_id is uninitialized.
 *   uninit can be NULL.
 * @param arg
 *   An optional argument that gets passed to the callback when it gets
 *   invoked.
 * @return
 *   On success, returns an opaque pointer for the registered object.
 *   On failure (either memory allocation issue in the function itself or an
 *   error is returned by the init callback itself), returns NULL.
 */
void *
rte_lcore_callback_register(const char *name, rte_lcore_init_cb init,
	rte_lcore_uninit_cb uninit, void *arg);

/**
 * Unregister callbacks previously registered with rte_lcore_callback_register.
 *
 * This function calls the uninit callback with all initialized lcores.
 * The callbacks are then removed from the lcore callbacks list.
 *
 * @param handle
 *   The handle pointer returned by a former successful call to
 *   rte_lcore_callback_register.
 */
void
rte_lcore_callback_unregister(void *handle);

/**
 * Callback prototype for iterating over lcores.
 *
 * @param lcore_id
 *   The lcore to consider.
 * @param arg
 *   An opaque pointer coming from the caller.
 * @return
 *   - 0 lets the iteration continue.
 *   - !0 makes the iteration stop.
 */
typedef int (*rte_lcore_iterate_cb)(unsigned int lcore_id, void *arg);

/**
 * Iterate on all active lcores (ROLE_RTE, ROLE_SERVICE and ROLE_NON_EAL).
 * No modification on the lcore states is allowed in the callback.
 *
 * Note: as opposed to init/uninit callbacks, iteration callbacks can be
 * invoked in parallel as they are run under a read lock protecting the lcores
 * and callbacks list.
 *
 * @param cb
 *   The callback that gets passed each lcore.
 * @param arg
 *   An opaque pointer passed to cb.
 * @return
 *   Same return code as the callback last invocation (see rte_lcore_iterate_cb
 *   description).
 */
int
rte_lcore_iterate(rte_lcore_iterate_cb cb, void *arg);

/**
 * lcore usage statistics.
 */
struct rte_lcore_usage {
	/**
	 * The total amount of time that the application has been running on
	 * this lcore, in TSC cycles.
	 */
	uint64_t total_cycles;
	/**
	 * The amount of time the application was busy, handling some
	 * workload on this lcore, in TSC cycles.
	 */
	uint64_t busy_cycles;
};

/**
 * Callback to allow applications to report lcore usage.
 *
 * @param [in] lcore_id
 *   The lcore to consider.
 * @param [out] usage
 *   Counters representing this lcore usage. This can never be NULL.
 * @return
 *   - 0 if fields in usage were updated successfully. The fields that the
 *     application does not support must not be modified.
 *   - a negative value if the information is not available or if any error
 *     occurred.
 */
typedef int (*rte_lcore_usage_cb)(unsigned int lcore_id, struct rte_lcore_usage *usage);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Register a callback from an application to be called in rte_lcore_dump() and
 * the /eal/lcore/info telemetry endpoint handler. Applications are expected to
 * report lcore usage statistics via this callback.
 *
 * If a callback was already registered, it can be replaced with another callback
 * or unregistered with NULL. The previously registered callback may remain in
 * use for an undetermined period of time.
 *
 * @param cb
 *   The callback function.
 */
__rte_experimental
void rte_lcore_register_usage_cb(rte_lcore_usage_cb cb);

/**
 * List all lcores.
 *
 * @param f
 *   The output stream where the dump should be sent.
 */
void
rte_lcore_dump(FILE *f);

/**
 * Set thread names.
 *
 * @note It fails with glibc < 2.12.
 *
 * @param id
 *   Thread id.
 * @param name
 *   Thread name to set.
 * @return
 *   On success, return 0; otherwise return a negative value.
 */
int rte_thread_setname(pthread_t id, const char *name);

/**
 * Register current non-EAL thread as a lcore.
 *
 * @note This API is not compatible with the multi-process feature:
 * - if a primary process registers a non-EAL thread, then no secondary process
 *   will initialise.
 * - if a secondary process initialises successfully, trying to register a
 *   non-EAL thread from either primary or secondary processes will always end
 *   up with the thread getting LCORE_ID_ANY as lcore.
 *
 * @return
 *   On success, return 0; otherwise return -1 with rte_errno set.
 */
int
rte_thread_register(void);

/**
 * Unregister current thread and release lcore if one was associated.
 */
void
rte_thread_unregister(void);

/**
 * Create a control thread.
 *
 * Creates a control thread with the given name and attributes. The
 * affinity of the new thread is based on the CPU affinity retrieved
 * at the time rte_eal_init() was called, the dataplane and service
 * lcores are then excluded. If setting the name of the thread fails,
 * the error is ignored and a debug message is logged.
 *
 * @param thread
 *   Filled with the thread id of the new created thread.
 * @param name
 *   The name of the control thread (max 16 characters including '\0').
 * @param attr
 *   Attributes for the new thread.
 * @param start_routine
 *   Function to be executed by the new thread.
 * @param arg
 *   Argument passed to start_routine.
 * @return
 *   On success, returns 0; on error, it returns a negative value
 *   corresponding to the error number.
 */
int
rte_ctrl_thread_create(pthread_t *thread, const char *name,
		const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_LCORE_H_ */
