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
#include <rte_config.h>
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCORE_ID_ANY     UINT32_MAX       /**< Any lcore. */

#if defined(__linux__)
	typedef	cpu_set_t rte_cpuset_t;
#elif defined(__FreeBSD__)
#include <pthread_np.h>
	typedef cpuset_t rte_cpuset_t;
#endif

/**
 * Structure storing internal configuration (per-lcore)
 */
struct lcore_config {
	unsigned detected;         /**< true if lcore was detected */
	pthread_t thread_id;       /**< pthread identifier */
	int pipe_master2slave[2];  /**< communication pipe with master */
	int pipe_slave2master[2];  /**< communication pipe with master */
	lcore_function_t * volatile f;         /**< function to call */
	void * volatile arg;       /**< argument of function */
	volatile int ret;          /**< return value of function */
	volatile enum rte_lcore_state_t state; /**< lcore state */
	unsigned socket_id;        /**< physical socket id for this lcore */
	unsigned core_id;          /**< core number on socket for this lcore */
	int core_index;            /**< relative index, starting from 0 */
	rte_cpuset_t cpuset;       /**< cpu set which the lcore affinity to */
	uint8_t core_role;         /**< role of core eg: OFF, RTE, SERVICE */
};

/**
 * Internal configuration (per-lcore)
 */
extern struct lcore_config lcore_config[RTE_MAX_LCORE];

RTE_DECLARE_PER_LCORE(unsigned, _lcore_id);  /**< Per thread "lcore id". */
RTE_DECLARE_PER_LCORE(rte_cpuset_t, _cpuset); /**< Per thread "cpuset". */

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
 *  Logical core ID (in EAL thread) or LCORE_ID_ANY (in non-EAL thread)
 */
static inline unsigned
rte_lcore_id(void)
{
	return RTE_PER_LCORE(_lcore_id);
}

/**
 * Get the id of the master lcore
 *
 * @return
 *   the id of the master lcore
 */
static inline unsigned
rte_get_master_lcore(void)
{
	return rte_eal_get_configuration()->master_lcore;
}

/**
 * Return the number of execution units (lcores) on the system.
 *
 * @return
 *   the number of execution units (lcores) on the system.
 */
static inline unsigned
rte_lcore_count(void)
{
	const struct rte_config *cfg = rte_eal_get_configuration();
	return cfg->lcore_count;
}

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
static inline int
rte_lcore_index(int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE)
		return -1;
	if (lcore_id < 0)
		lcore_id = (int)rte_lcore_id();
	return lcore_config[lcore_id].core_index;
}

/**
 * Return the ID of the physical socket of the logical core we are
 * running on.
 * @return
 *   the ID of current lcoreid's physical socket
 */
unsigned rte_socket_id(void);

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
unsigned int __rte_experimental
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
int __rte_experimental
rte_socket_id_by_idx(unsigned int idx);

/**
 * Get the ID of the physical socket of the specified lcore
 *
 * @param lcore_id
 *   the targeted lcore, which MUST be between 0 and RTE_MAX_LCORE-1.
 * @return
 *   the ID of lcoreid's physical socket
 */
static inline unsigned
rte_lcore_to_socket_id(unsigned lcore_id)
{
	return lcore_config[lcore_id].socket_id;
}

/**
 * Test if an lcore is enabled.
 *
 * @param lcore_id
 *   The identifier of the lcore, which MUST be between 0 and
 *   RTE_MAX_LCORE-1.
 * @return
 *   True if the given lcore is enabled; false otherwise.
 */
static inline int
rte_lcore_is_enabled(unsigned lcore_id)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	if (lcore_id >= RTE_MAX_LCORE)
		return 0;
	return cfg->lcore_role[lcore_id] == ROLE_RTE;
}

/**
 * Get the next enabled lcore ID.
 *
 * @param i
 *   The current lcore (reference).
 * @param skip_master
 *   If true, do not return the ID of the master lcore.
 * @param wrap
 *   If true, go back to 0 when RTE_MAX_LCORE is reached; otherwise,
 *   return RTE_MAX_LCORE.
 * @return
 *   The next lcore_id or RTE_MAX_LCORE if not found.
 */
static inline unsigned
rte_get_next_lcore(unsigned i, int skip_master, int wrap)
{
	i++;
	if (wrap)
		i %= RTE_MAX_LCORE;

	while (i < RTE_MAX_LCORE) {
		if (!rte_lcore_is_enabled(i) ||
		    (skip_master && (i == rte_get_master_lcore()))) {
			i++;
			if (wrap)
				i %= RTE_MAX_LCORE;
			continue;
		}
		break;
	}
	return i;
}
/**
 * Macro to browse all running lcores.
 */
#define RTE_LCORE_FOREACH(i)						\
	for (i = rte_get_next_lcore(-1, 0, 0);				\
	     i<RTE_MAX_LCORE;						\
	     i = rte_get_next_lcore(i, 0, 0))

/**
 * Macro to browse all running lcores except the master lcore.
 */
#define RTE_LCORE_FOREACH_SLAVE(i)					\
	for (i = rte_get_next_lcore(-1, 1, 0);				\
	     i<RTE_MAX_LCORE;						\
	     i = rte_get_next_lcore(i, 1, 0))

/**
 * Set core affinity of the current thread.
 * Support both EAL and non-EAL thread and update TLS.
 *
 * @param cpusetp
 *   Point to cpu_set_t for setting current thread affinity.
 * @return
 *   On success, return 0; otherwise return -1;
 */
int rte_thread_set_affinity(rte_cpuset_t *cpusetp);

/**
 * Get core affinity of the current thread.
 *
 * @param cpusetp
 *   Point to cpu_set_t for getting current thread cpu affinity.
 *   It presumes input is not NULL, otherwise it causes panic.
 *
 */
void rte_thread_get_affinity(rte_cpuset_t *cpusetp);

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
 * Create a control thread.
 *
 * Wrapper to pthread_create(), pthread_setname_np() and
 * pthread_setaffinity_np(). The dataplane and service lcores are
 * excluded from the affinity of the new thread.
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
__rte_experimental int
rte_ctrl_thread_create(pthread_t *thread, const char *name,
		const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg);

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

#ifdef __cplusplus
}
#endif


#endif /* _RTE_LCORE_H_ */
