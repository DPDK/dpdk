/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _EAL_WINDOWS_H_
#define _EAL_WINDOWS_H_

/**
 * @file Facilities private to Windows EAL
 */

#include <rte_errno.h>
#include <rte_windows.h>

/**
 * Create a map of processors and cores on the system.
 *
 * @return
 *  0 on success, (-1) on failure and rte_errno is set.
 */
int eal_create_cpu_map(void);

/**
 * Create a thread.
 *
 * @param thread
 *   The location to store the thread id if successful.
 * @return
 *   0 for success, -1 if the thread is not created.
 */
int eal_thread_create(pthread_t *thread);

/**
 * Get system NUMA node number for a socket ID.
 *
 * @param socket_id
 *  Valid EAL socket ID.
 * @return
 *  NUMA node number to use with Win32 API.
 */
unsigned int eal_socket_numa_node(unsigned int socket_id);

#endif /* _EAL_WINDOWS_H_ */
