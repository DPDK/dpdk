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

/* pthread function overrides */
#define pthread_self() \
	((pthread_t)GetCurrentThreadId())


static inline int
pthread_equal(pthread_t t1, pthread_t t2)
{
	return t1 == t2;
}

static inline int
pthread_setaffinity_np(pthread_t threadid, size_t cpuset_size,
			rte_cpuset_t *cpuset)
{
	DWORD_PTR ret = 0;
	HANDLE thread_handle;

	if (cpuset == NULL || cpuset_size == 0)
		return -1;

	thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, threadid);
	if (thread_handle == NULL) {
		RTE_LOG_WIN32_ERR("OpenThread()");
		return -1;
	}

	ret = SetThreadAffinityMask(thread_handle, *cpuset->_bits);
	if (ret == 0) {
		RTE_LOG_WIN32_ERR("SetThreadAffinityMask()");
		goto close_handle;
	}

close_handle:
	if (CloseHandle(thread_handle) == 0) {
		RTE_LOG_WIN32_ERR("CloseHandle()");
		return -1;
	}
	return (ret == 0) ? -1 : 0;
}

static inline int
pthread_getaffinity_np(pthread_t threadid, size_t cpuset_size,
			rte_cpuset_t *cpuset)
{
	/* Workaround for the lack of a GetThreadAffinityMask()
	 *API in Windows
	 */
	DWORD_PTR prev_affinity_mask;
	HANDLE thread_handle;
	DWORD_PTR ret = 0;

	if (cpuset == NULL || cpuset_size == 0)
		return -1;

	thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, threadid);
	if (thread_handle == NULL) {
		RTE_LOG_WIN32_ERR("OpenThread()");
		return -1;
	}

	/* obtain previous mask by setting dummy mask */
	prev_affinity_mask = SetThreadAffinityMask(thread_handle, 0x1);
	if (prev_affinity_mask == 0) {
		RTE_LOG_WIN32_ERR("SetThreadAffinityMask()");
		goto close_handle;
	}

	/* set it back! */
	ret = SetThreadAffinityMask(thread_handle, prev_affinity_mask);
	if (ret == 0) {
		RTE_LOG_WIN32_ERR("SetThreadAffinityMask()");
		goto close_handle;
	}

	memset(cpuset, 0, cpuset_size);
	*cpuset->_bits = prev_affinity_mask;

close_handle:
	if (CloseHandle(thread_handle) == 0) {
		RTE_LOG_WIN32_ERR("SetThreadAffinityMask()");
		return -1;
	}
	return (ret == 0) ? -1 : 0;
}

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
pthread_detach(__rte_unused pthread_t thread)
{
	return 0;
}

static inline int
pthread_join(__rte_unused pthread_t thread,
	__rte_unused void **value_ptr)
{
	return 0;
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
