/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_THREAD_H
#define EAL_THREAD_H

#include <rte_lcore.h>

/**
 * basic loop of thread, called for each thread by eal_init().
 *
 * @param arg
 *   opaque pointer
 */
__attribute__((noreturn)) void *eal_thread_loop(void *arg);

/**
 * Init per-lcore info for master thread
 *
 * @param lcore_id
 *   identifier of master lcore
 */
void eal_thread_init_master(unsigned lcore_id);

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
 * Dump the current pthread cpuset.
 * This function is private to EAL.
 *
 * Note:
 *   If the dump size is greater than the size of given buffer,
 *   the string will be truncated and with '\0' at the end.
 *
 * @param str
 *   The string buffer the cpuset will dump to.
 * @param size
 *   The string buffer size.
 * @return
 *   0 for success, -1 if truncation happens.
 */
int
eal_thread_dump_affinity(char *str, unsigned size);

#endif /* EAL_THREAD_H */
