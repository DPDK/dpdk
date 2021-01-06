/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Mellanox Technologies, Ltd
 */

#include <rte_os.h>

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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THREAD_H_ */
