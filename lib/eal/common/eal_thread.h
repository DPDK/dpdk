/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_THREAD_H
#define EAL_THREAD_H

#include <rte_common.h>
#include <rte_lcore.h>

/**
 * Basic loop of EAL thread, called for each worker thread by rte_eal_init().
 *
 * @param arg
 *   The lcore_id (passed as an integer) of this worker thread.
 */
__rte_noreturn uint32_t eal_thread_loop(void *arg);

/**
 * Get the NUMA socket id from cpu id.
 * This function is private to EAL.
 *
 * @param cpu_id
 *   The logical process id.
 * @return
 *   socket_id or SOCKET_ID_ANY
 */
unsigned eal_cpu_socket_id(unsigned cpu_id);

/**
 * Default buffer size to use with eal_thread_dump_affinity()
 */
#define RTE_CPU_AFFINITY_STR_LEN            256

/**
 * Dump the cpuset as a human readable string.
 * This function is private to EAL.
 *
 * Note:
 *   If the dump size is greater than the size of given buffer,
 *   the string will be truncated and with '\0' at the end.
 *
 * @param cpuset
 *   The CPU affinity object to dump.
 * @param str
 *   The string buffer the cpuset will dump to.
 * @param size
 *   The string buffer size.
 * @return
 *   0 for success, -1 if truncation happens.
 */
int
eal_thread_dump_affinity(rte_cpuset_t *cpuset, char *str, unsigned int size);

/**
 * Dump the current thread cpuset.
 * This is a wrapper on eal_thread_dump_affinity().
 */
int
eal_thread_dump_current_affinity(char *str, unsigned int size);

/**
 * Called by the main thread to wake up a worker in 'WAIT' state.
 * This function blocks until the worker acknowledge it started processing a
 * new command.
 * This function is private to EAL.
 *
 * @param worker_id
 *   The lcore_id of a worker thread.
 * @return
 *   0 on success, negative errno on error
 */
int
eal_thread_wake_worker(unsigned int worker_id);

/**
 * Called by a worker thread to sleep after entering 'WAIT' state.
 * This function is private to EAL.
 */
void
eal_thread_wait_command(void);

/**
 * Called by a worker thread to acknowledge new command after leaving 'WAIT'
 * state.
 * This function is private to EAL.
 */
void
eal_thread_ack_command(void);

#endif /* EAL_THREAD_H */
