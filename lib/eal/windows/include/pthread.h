/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <stdint.h>
#include <sched.h>

/**
 * This file is required to support the common code in eal_common_proc.c,
 * eal_common_thread.c and common\include\rte_per_lcore.h as Microsoft libc
 * does not contain pthread.h. This may be removed in future releases.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_windows.h>

#define PTHREAD_BARRIER_SERIAL_THREAD TRUE

/* defining pthread_t type on Windows since there is no in Microsoft libc*/
typedef uintptr_t pthread_t;

/* defining pthread_attr_t type on Windows since there is no in Microsoft libc*/
typedef void *pthread_attr_t;

typedef void *pthread_mutexattr_t;

typedef CRITICAL_SECTION pthread_mutex_t;

typedef SYNCHRONIZATION_BARRIER pthread_barrier_t;

#define pthread_barrier_init(barrier, attr, count) \
	!InitializeSynchronizationBarrier(barrier, count, -1)
#define pthread_barrier_wait(barrier) EnterSynchronizationBarrier(barrier, \
	SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY)
#define pthread_barrier_destroy(barrier) \
	!DeleteSynchronizationBarrier(barrier)
#define pthread_cancel(thread) !TerminateThread((HANDLE) thread, 0)

static inline int
pthread_create(void *threadid, const void *threadattr, void *threadfunc,
		void *args)
{
	RTE_SET_USED(threadattr);
	HANDLE hThread;
	hThread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)(uintptr_t)threadfunc,
		args, 0, (LPDWORD)threadid);
	if (hThread) {
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
	}
	return ((hThread != NULL) ? 0 : E_FAIL);
}

static inline int
pthread_mutex_init(pthread_mutex_t *mutex,
		   __rte_unused pthread_mutexattr_t *attr)
{
	InitializeCriticalSection(mutex);
	return 0;
}

static inline int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

static inline int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}

static inline int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _PTHREAD_H_ */
