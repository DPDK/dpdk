/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <errno.h>
#include <wchar.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_stdatomic.h>
#include <rte_thread.h>

#include "eal_private.h"
#include "eal_windows.h"

struct eal_tls_key {
	DWORD thread_index;
};

struct thread_routine_ctx {
	rte_thread_func thread_func;
	RTE_ATOMIC(bool) thread_init_failed;
	void *routine_args;
};

/* Translates the most common error codes related to threads */
static int
thread_translate_win32_error(DWORD error)
{
	switch (error) {
	case ERROR_SUCCESS:
		return 0;

	case ERROR_INVALID_PARAMETER:
		return EINVAL;

	case ERROR_INVALID_HANDLE:
		return EFAULT;

	case ERROR_NOT_ENOUGH_MEMORY:
		/* FALLTHROUGH */
	case ERROR_NO_SYSTEM_RESOURCES:
		return ENOMEM;

	case ERROR_PRIVILEGE_NOT_HELD:
		/* FALLTHROUGH */
	case ERROR_ACCESS_DENIED:
		return EACCES;

	case ERROR_ALREADY_EXISTS:
		return EEXIST;

	case ERROR_POSSIBLE_DEADLOCK:
		return EDEADLK;

	case ERROR_INVALID_FUNCTION:
		/* FALLTHROUGH */
	case ERROR_CALL_NOT_IMPLEMENTED:
		return ENOSYS;
	}

	return EINVAL;
}

static int
thread_log_last_error(const char *message)
{
	DWORD error = GetLastError();
	EAL_LOG(DEBUG, "GetLastError()=%lu: %s", error, message);

	return thread_translate_win32_error(error);
}

static int
thread_map_priority_to_os_value(enum rte_thread_priority eal_pri, int *os_pri,
	DWORD *pri_class)
{
	/* Clear the output parameters. */
	*os_pri = -1;
	*pri_class = -1;

	switch (eal_pri) {
	case RTE_THREAD_PRIORITY_NORMAL:
		*pri_class = NORMAL_PRIORITY_CLASS;
		*os_pri = THREAD_PRIORITY_NORMAL;
		break;
	case RTE_THREAD_PRIORITY_REALTIME_CRITICAL:
		*pri_class = REALTIME_PRIORITY_CLASS;
		*os_pri = THREAD_PRIORITY_TIME_CRITICAL;
		break;
	default:
		EAL_LOG(DEBUG, "The requested priority value is invalid.");
		return EINVAL;
	}

	return 0;
}

static int
thread_map_os_priority_to_eal_value(int os_pri, DWORD pri_class,
	enum rte_thread_priority *eal_pri)
{
	switch (pri_class) {
	case NORMAL_PRIORITY_CLASS:
		if (os_pri == THREAD_PRIORITY_NORMAL) {
			*eal_pri = RTE_THREAD_PRIORITY_NORMAL;
			return 0;
		}
		break;
	case HIGH_PRIORITY_CLASS:
		EAL_LOG(WARNING, "The OS priority class is high not real-time.");
		/* FALLTHROUGH */
	case REALTIME_PRIORITY_CLASS:
		if (os_pri == THREAD_PRIORITY_TIME_CRITICAL) {
			*eal_pri = RTE_THREAD_PRIORITY_REALTIME_CRITICAL;
			return 0;
		}
		break;
	default:
		EAL_LOG(DEBUG, "The OS priority value does not map to an EAL-defined priority.");
		return EINVAL;
	}

	return 0;
}

static int
convert_cpuset_to_affinity(const rte_cpuset_t *cpuset,
		PGROUP_AFFINITY affinity)
{
	int ret = 0;
	PGROUP_AFFINITY cpu_affinity = NULL;
	unsigned int cpu_idx;

	memset(affinity, 0, sizeof(GROUP_AFFINITY));
	affinity->Group = (USHORT)-1;

	/* Check that all cpus of the set belong to the same processor group and
	 * accumulate thread affinity to be applied.
	 */
	for (cpu_idx = 0; cpu_idx < CPU_SETSIZE; cpu_idx++) {
		if (!CPU_ISSET(cpu_idx, cpuset))
			continue;

		cpu_affinity = eal_get_cpu_affinity(cpu_idx);

		if (affinity->Group == (USHORT)-1) {
			affinity->Group = cpu_affinity->Group;
		} else if (affinity->Group != cpu_affinity->Group) {
			EAL_LOG(DEBUG, "All processors must belong to the same processor group");
			ret = ENOTSUP;
			goto cleanup;
		}

		affinity->Mask |= cpu_affinity->Mask;
	}

	if (affinity->Mask == 0) {
		ret = EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

static DWORD
thread_func_wrapper(void *arg)
{
	struct thread_routine_ctx ctx = *(struct thread_routine_ctx *)arg;
	const bool thread_exit = rte_atomic_load_explicit(
		&ctx.thread_init_failed, rte_memory_order_acquire);

	free(arg);

	if (thread_exit)
		return 0;

	return (DWORD)ctx.thread_func(ctx.routine_args);
}

int
rte_thread_create(rte_thread_t *thread_id,
		  const rte_thread_attr_t *thread_attr,
		  rte_thread_func thread_func, void *args)
{
	int ret = 0;
	DWORD tid;
	HANDLE thread_handle = NULL;
	GROUP_AFFINITY thread_affinity;
	struct thread_routine_ctx *ctx;
	bool thread_exit = false;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		EAL_LOG(DEBUG, "Insufficient memory for thread context allocations");
		ret = ENOMEM;
		goto cleanup;
	}
	ctx->routine_args = args;
	ctx->thread_func = thread_func;
	ctx->thread_init_failed = false;

	thread_handle = CreateThread(NULL, 0, thread_func_wrapper, ctx,
		CREATE_SUSPENDED, &tid);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("CreateThread()");
		goto cleanup;
	}
	thread_id->opaque_id = tid;

	if (thread_attr != NULL) {
		if (CPU_COUNT(&thread_attr->cpuset) > 0) {
			ret = convert_cpuset_to_affinity(
							&thread_attr->cpuset,
							&thread_affinity
							);
			if (ret != 0) {
				EAL_LOG(DEBUG, "Unable to convert cpuset to thread affinity");
				thread_exit = true;
				goto resume_thread;
			}

			if (!SetThreadGroupAffinity(thread_handle,
						    &thread_affinity, NULL)) {
				ret = thread_log_last_error("SetThreadGroupAffinity()");
				thread_exit = true;
				goto resume_thread;
			}
		}
		ret = rte_thread_set_priority(*thread_id,
				thread_attr->priority);
		if (ret != 0) {
			EAL_LOG(DEBUG, "Unable to set thread priority");
			thread_exit = true;
			goto resume_thread;
		}
	}

resume_thread:
	rte_atomic_store_explicit(&ctx->thread_init_failed, thread_exit, rte_memory_order_release);

	if (ResumeThread(thread_handle) == (DWORD)-1) {
		ret = thread_log_last_error("ResumeThread()");
		goto cleanup;
	}

	ctx = NULL;
cleanup:
	free(ctx);
	if (thread_handle != NULL) {
		CloseHandle(thread_handle);
		thread_handle = NULL;
	}

	return ret;
}

int
rte_thread_join(rte_thread_t thread_id, uint32_t *value_ptr)
{
	HANDLE thread_handle;
	DWORD result;
	DWORD exit_code = 0;
	BOOL err;
	int ret = 0;

	thread_handle = OpenThread(SYNCHRONIZE | THREAD_QUERY_INFORMATION,
				   FALSE, thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	result = WaitForSingleObject(thread_handle, INFINITE);
	if (result != WAIT_OBJECT_0) {
		ret = thread_log_last_error("WaitForSingleObject()");
		goto cleanup;
	}

	if (value_ptr != NULL) {
		err = GetExitCodeThread(thread_handle, &exit_code);
		if (err == 0) {
			ret = thread_log_last_error("GetExitCodeThread()");
			goto cleanup;
		}
		*value_ptr = exit_code;
	}

cleanup:
	if (thread_handle != NULL) {
		CloseHandle(thread_handle);
		thread_handle = NULL;
	}

	return ret;
}

int
rte_thread_detach(rte_thread_t thread_id)
{
	/* No resources that need to be released. */
	RTE_SET_USED(thread_id);

	return 0;
}

int
rte_thread_equal(rte_thread_t t1, rte_thread_t t2)
{
	return t1.opaque_id == t2.opaque_id;
}

rte_thread_t
rte_thread_self(void)
{
	rte_thread_t thread_id;

	thread_id.opaque_id = GetCurrentThreadId();

	return thread_id;
}

void
rte_thread_set_name(rte_thread_t thread_id, const char *thread_name)
{
	int ret = 0;
	wchar_t wname[RTE_THREAD_NAME_SIZE];
	mbstate_t state = {0};
	size_t rv;
	HANDLE thread_handle;

	thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE,
		thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	memset(wname, 0, sizeof(wname));
	rv = mbsrtowcs(wname, &thread_name, RTE_DIM(wname) - 1, &state);
	if (rv == (size_t)-1) {
		ret = EILSEQ;
		goto cleanup;
	}

#ifndef RTE_TOOLCHAIN_GCC
	if (FAILED(SetThreadDescription(thread_handle, wname))) {
		ret = EINVAL;
		goto cleanup;
	}
#else
	ret = ENOTSUP;
	goto cleanup;
#endif

cleanup:
	if (thread_handle != NULL)
		CloseHandle(thread_handle);

	if (ret != 0)
		EAL_LOG(DEBUG, "Failed to set thread name");
}

int
rte_thread_get_priority(rte_thread_t thread_id,
	enum rte_thread_priority *priority)
{
	HANDLE thread_handle = NULL;
	DWORD pri_class;
	int os_pri;
	int ret;

	pri_class = GetPriorityClass(GetCurrentProcess());
	if (pri_class == 0) {
		ret = thread_log_last_error("GetPriorityClass()");
		goto cleanup;
	}

	thread_handle = OpenThread(THREAD_SET_INFORMATION |
		THREAD_QUERY_INFORMATION, FALSE, thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	os_pri = GetThreadPriority(thread_handle);
	if (os_pri == THREAD_PRIORITY_ERROR_RETURN) {
		ret = thread_log_last_error("GetThreadPriority()");
		goto cleanup;
	}

	ret = thread_map_os_priority_to_eal_value(os_pri, pri_class, priority);
	if (ret != 0)
		goto cleanup;

cleanup:
	if (thread_handle != NULL)
		CloseHandle(thread_handle);

	return ret;
}

int
rte_thread_set_priority(rte_thread_t thread_id,
			enum rte_thread_priority priority)
{
	HANDLE thread_handle;
	DWORD priority_class;
	int os_priority;
	int ret = 0;

	thread_handle = OpenThread(THREAD_SET_INFORMATION |
		THREAD_QUERY_INFORMATION, FALSE, thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	ret = thread_map_priority_to_os_value(priority, &os_priority,
		&priority_class);
	if (ret != 0)
		goto cleanup;

	if (!SetPriorityClass(GetCurrentProcess(), priority_class)) {
		ret = thread_log_last_error("SetPriorityClass()");
		goto cleanup;
	}

	if (!SetThreadPriority(thread_handle, os_priority)) {
		ret = thread_log_last_error("SetThreadPriority()");
		goto cleanup;
	}

cleanup:
	if (thread_handle != NULL)
		CloseHandle(thread_handle);

	return ret;
}

int
rte_thread_key_create(rte_thread_key *key,
		__rte_unused void (*destructor)(void *))
{
	*key = malloc(sizeof(**key));
	if ((*key) == NULL) {
		EAL_LOG(DEBUG, "Cannot allocate TLS key.");
		rte_errno = ENOMEM;
		return -1;
	}
	(*key)->thread_index = TlsAlloc();
	if ((*key)->thread_index == TLS_OUT_OF_INDEXES) {
		RTE_LOG_WIN32_ERR("TlsAlloc()");
		free(*key);
		rte_errno = ENOEXEC;
		return -1;
	}
	return 0;
}

int
rte_thread_key_delete(rte_thread_key key)
{
	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return -1;
	}
	if (!TlsFree(key->thread_index)) {
		RTE_LOG_WIN32_ERR("TlsFree()");
		free(key);
		rte_errno = ENOEXEC;
		return -1;
	}
	free(key);
	return 0;
}

int
rte_thread_value_set(rte_thread_key key, const void *value)
{
	char *p;

	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return -1;
	}
	/* discard const qualifier */
	p = (char *) (uintptr_t) value;
	if (!TlsSetValue(key->thread_index, p)) {
		RTE_LOG_WIN32_ERR("TlsSetValue()");
		rte_errno = ENOEXEC;
		return -1;
	}
	return 0;
}

void *
rte_thread_value_get(rte_thread_key key)
{
	void *output;

	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return NULL;
	}
	output = TlsGetValue(key->thread_index);
	if (GetLastError() != ERROR_SUCCESS) {
		RTE_LOG_WIN32_ERR("TlsGetValue()");
		rte_errno = ENOEXEC;
		return NULL;
	}
	return output;
}

int
rte_thread_set_affinity_by_id(rte_thread_t thread_id,
		const rte_cpuset_t *cpuset)
{
	int ret = 0;
	GROUP_AFFINITY thread_affinity;
	HANDLE thread_handle = NULL;

	if (cpuset == NULL) {
		ret = EINVAL;
		goto cleanup;
	}

	ret = convert_cpuset_to_affinity(cpuset, &thread_affinity);
	if (ret != 0) {
		EAL_LOG(DEBUG, "Unable to convert cpuset to thread affinity");
		goto cleanup;
	}

	thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE,
		thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	if (!SetThreadGroupAffinity(thread_handle, &thread_affinity, NULL)) {
		ret = thread_log_last_error("SetThreadGroupAffinity()");
		goto cleanup;
	}

cleanup:
	if (thread_handle != NULL) {
		CloseHandle(thread_handle);
		thread_handle = NULL;
	}

	return ret;
}

int
rte_thread_get_affinity_by_id(rte_thread_t thread_id,
		rte_cpuset_t *cpuset)
{
	HANDLE thread_handle = NULL;
	PGROUP_AFFINITY cpu_affinity;
	GROUP_AFFINITY thread_affinity;
	unsigned int cpu_idx;
	int ret = 0;

	if (cpuset == NULL) {
		ret = EINVAL;
		goto cleanup;
	}

	thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE,
		thread_id.opaque_id);
	if (thread_handle == NULL) {
		ret = thread_log_last_error("OpenThread()");
		goto cleanup;
	}

	/* obtain previous thread affinity */
	if (!GetThreadGroupAffinity(thread_handle, &thread_affinity)) {
		ret = thread_log_last_error("GetThreadGroupAffinity()");
		goto cleanup;
	}

	CPU_ZERO(cpuset);

	/* Convert affinity to DPDK cpu set */
	for (cpu_idx = 0; cpu_idx < CPU_SETSIZE; cpu_idx++) {

		cpu_affinity = eal_get_cpu_affinity(cpu_idx);

		if ((cpu_affinity->Group == thread_affinity.Group) &&
		   ((cpu_affinity->Mask & thread_affinity.Mask) != 0)) {
			CPU_SET(cpu_idx, cpuset);
		}
	}

cleanup:
	if (thread_handle != NULL) {
		CloseHandle(thread_handle);
		thread_handle = NULL;
	}
	return ret;
}
