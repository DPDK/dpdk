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
 * Log current function as not implemented and set rte_errno.
 */
#define EAL_LOG_NOT_IMPLEMENTED() \
	do { \
		RTE_LOG(DEBUG, EAL, "%s() is not implemented\n", __func__); \
		rte_errno = ENOTSUP; \
	} while (0)

/**
 * Log current function as a stub.
 */
#define EAL_LOG_STUB() \
	RTE_LOG(DEBUG, EAL, "Windows: %s() is a stub\n", __func__)

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

/**
 * Open virt2phys driver interface device.
 *
 * @return 0 on success, (-1) on failure.
 */
int eal_mem_virt2iova_init(void);

/**
 * Locate Win32 memory management routines in system libraries.
 *
 * @return 0 on success, (-1) on failure.
 */
int eal_mem_win32api_init(void);

/**
 * Allocate new memory in hugepages on the specified NUMA node.
 *
 * @param size
 *  Number of bytes to allocate. Must be a multiple of huge page size.
 * @param socket_id
 *  Socket ID.
 * @return
 *  Address of the memory allocated on success or NULL on failure.
 */
void *eal_mem_alloc_socket(size_t size, int socket_id);

/**
 * Commit memory previously reserved with eal_mem_reserve()
 * or decommitted from hugepages by eal_mem_decommit().
 *
 * @param requested_addr
 *  Address within a reserved region. Must not be NULL.
 * @param size
 *  Number of bytes to commit. Must be a multiple of page size.
 * @param socket_id
 *  Socket ID to allocate on. Can be SOCKET_ID_ANY.
 * @return
 *  On success, address of the committed memory, that is, requested_addr.
 *  On failure, NULL and rte_errno is set.
 */
void *eal_mem_commit(void *requested_addr, size_t size, int socket_id);

/**
 * Put allocated or committed memory back into reserved state.
 *
 * @param addr
 *  Address of the region to decommit.
 * @param size
 *  Number of bytes to decommit, must be the size of a page
 *  (hugepage or regular one).
 *
 * The *addr* and *size* must match location and size
 * of a previously allocated or committed region.
 *
 * @return
 *  0 on success, (-1) on failure.
 */
int eal_mem_decommit(void *addr, size_t size);

#endif /* _EAL_WINDOWS_H_ */
