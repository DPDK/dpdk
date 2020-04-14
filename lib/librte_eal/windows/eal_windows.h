/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _EAL_WINDOWS_H_
#define _EAL_WINDOWS_H_

/**
 * @file Facilities private to Windows EAL
 */

#include <rte_windows.h>

/**
 * Create a map of processors and cores on the system.
 */
void eal_create_cpu_map(void);

/**
 * Create a thread.
 *
 * @param thread
 *   The location to store the thread id if successful.
 * @return
 *   0 for success, -1 if the thread is not created.
 */
int eal_thread_create(pthread_t *thread);

#endif /* _EAL_WINDOWS_H_ */
