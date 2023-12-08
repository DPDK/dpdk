/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_thread.h>

#include "eal_private.h"

struct eal_tls_key {
	pthread_key_t thread_index;
};

struct thread_start_context {
	rte_thread_func thread_func;
	void *thread_args;
	const rte_thread_attr_t *thread_attr;
	pthread_mutex_t wrapper_mutex;
	pthread_cond_t wrapper_cond;
	int wrapper_ret;
	bool wrapper_done;
};

static int
thread_map_priority_to_os_value(enum rte_thread_priority eal_pri, int *os_pri,
	int *pol)
{
	/* Clear the output parameters. */
	*os_pri = sched_get_priority_min(SCHED_OTHER) - 1;
	*pol = -1;

	switch (eal_pri) {
	case RTE_THREAD_PRIORITY_NORMAL:
		*pol = SCHED_OTHER;

		/*
		 * Choose the middle of the range to represent the priority
		 * 'normal'.
		 * On Linux, this should be 0, since both
		 * sched_get_priority_min/_max return 0 for SCHED_OTHER.
		 */
		*os_pri = (sched_get_priority_min(SCHED_OTHER) +
			sched_get_priority_max(SCHED_OTHER)) / 2;
		break;
	case RTE_THREAD_PRIORITY_REALTIME_CRITICAL:
		*pol = SCHED_RR;
		*os_pri = sched_get_priority_max(SCHED_RR);
		break;
	default:
		EAL_LOG(DEBUG, "The requested priority value is invalid.");
		return EINVAL;
	}

	return 0;
}

static int
thread_map_os_priority_to_eal_priority(int policy, int os_pri,
	enum rte_thread_priority *eal_pri)
{
	switch (policy) {
	case SCHED_OTHER:
		if (os_pri == (sched_get_priority_min(SCHED_OTHER) +
				sched_get_priority_max(SCHED_OTHER)) / 2) {
			*eal_pri = RTE_THREAD_PRIORITY_NORMAL;
			return 0;
		}
		break;
	case SCHED_RR:
		if (os_pri == sched_get_priority_max(SCHED_RR)) {
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

static void *
thread_start_wrapper(void *arg)
{
	struct thread_start_context *ctx = (struct thread_start_context *)arg;
	rte_thread_func thread_func = ctx->thread_func;
	void *thread_args = ctx->thread_args;
	int ret = 0;

	if (ctx->thread_attr != NULL && CPU_COUNT(&ctx->thread_attr->cpuset) > 0) {
		ret = rte_thread_set_affinity_by_id(rte_thread_self(), &ctx->thread_attr->cpuset);
		if (ret != 0)
			EAL_LOG(DEBUG, "rte_thread_set_affinity_by_id failed");
	}

	pthread_mutex_lock(&ctx->wrapper_mutex);
	ctx->wrapper_ret = ret;
	ctx->wrapper_done = true;
	pthread_cond_signal(&ctx->wrapper_cond);
	pthread_mutex_unlock(&ctx->wrapper_mutex);

	if (ret != 0)
		return NULL;

	return (void *)(uintptr_t)thread_func(thread_args);
}

int
rte_thread_create(rte_thread_t *thread_id,
		const rte_thread_attr_t *thread_attr,
		rte_thread_func thread_func, void *args)
{
	int ret = 0;
	pthread_attr_t attr;
	pthread_attr_t *attrp = NULL;
	struct sched_param param = {
		.sched_priority = 0,
	};
	int policy = SCHED_OTHER;
	struct thread_start_context ctx = {
		.thread_func = thread_func,
		.thread_args = args,
		.thread_attr = thread_attr,
		.wrapper_done = false,
		.wrapper_mutex = PTHREAD_MUTEX_INITIALIZER,
		.wrapper_cond = PTHREAD_COND_INITIALIZER,
	};

	if (thread_attr != NULL) {
		ret = pthread_attr_init(&attr);
		if (ret != 0) {
			EAL_LOG(DEBUG, "pthread_attr_init failed");
			goto cleanup;
		}

		attrp = &attr;

		/*
		 * Set the inherit scheduler parameter to explicit,
		 * otherwise the priority attribute is ignored.
		 */
		ret = pthread_attr_setinheritsched(attrp,
				PTHREAD_EXPLICIT_SCHED);
		if (ret != 0) {
			EAL_LOG(DEBUG, "pthread_attr_setinheritsched failed");
			goto cleanup;
		}

		if (thread_attr->priority ==
				RTE_THREAD_PRIORITY_REALTIME_CRITICAL) {
			ret = ENOTSUP;
			goto cleanup;
		}
		ret = thread_map_priority_to_os_value(thread_attr->priority,
				&param.sched_priority, &policy);
		if (ret != 0)
			goto cleanup;

		ret = pthread_attr_setschedpolicy(attrp, policy);
		if (ret != 0) {
			EAL_LOG(DEBUG, "pthread_attr_setschedpolicy failed");
			goto cleanup;
		}

		ret = pthread_attr_setschedparam(attrp, &param);
		if (ret != 0) {
			EAL_LOG(DEBUG, "pthread_attr_setschedparam failed");
			goto cleanup;
		}
	}

	ret = pthread_create((pthread_t *)&thread_id->opaque_id, attrp,
		thread_start_wrapper, &ctx);
	if (ret != 0) {
		EAL_LOG(DEBUG, "pthread_create failed");
		goto cleanup;
	}

	pthread_mutex_lock(&ctx.wrapper_mutex);
	while (!ctx.wrapper_done)
		pthread_cond_wait(&ctx.wrapper_cond, &ctx.wrapper_mutex);
	ret = ctx.wrapper_ret;
	pthread_mutex_unlock(&ctx.wrapper_mutex);

	if (ret != 0)
		rte_thread_join(*thread_id, NULL);

cleanup:
	if (attrp != NULL)
		pthread_attr_destroy(&attr);

	return ret;
}

int
rte_thread_join(rte_thread_t thread_id, uint32_t *value_ptr)
{
	int ret = 0;
	void *res = (void *)(uintptr_t)0;
	void **pres = NULL;

	if (value_ptr != NULL)
		pres = &res;

	ret = pthread_join((pthread_t)thread_id.opaque_id, pres);
	if (ret != 0) {
		EAL_LOG(DEBUG, "pthread_join failed");
		return ret;
	}

	if (value_ptr != NULL)
		*value_ptr = (uint32_t)(uintptr_t)res;

	return 0;
}

int
rte_thread_detach(rte_thread_t thread_id)
{
	return pthread_detach((pthread_t)thread_id.opaque_id);
}

int
rte_thread_equal(rte_thread_t t1, rte_thread_t t2)
{
	return pthread_equal((pthread_t)t1.opaque_id, (pthread_t)t2.opaque_id);
}

rte_thread_t
rte_thread_self(void)
{
	RTE_BUILD_BUG_ON(sizeof(pthread_t) > sizeof(uintptr_t));

	rte_thread_t thread_id;

	thread_id.opaque_id = (uintptr_t)pthread_self();

	return thread_id;
}

int
rte_thread_get_priority(rte_thread_t thread_id,
	enum rte_thread_priority *priority)
{
	struct sched_param param;
	int policy;
	int ret;

	ret = pthread_getschedparam((pthread_t)thread_id.opaque_id, &policy,
		&param);
	if (ret != 0) {
		EAL_LOG(DEBUG, "pthread_getschedparam failed");
		goto cleanup;
	}

	return thread_map_os_priority_to_eal_priority(policy,
		param.sched_priority, priority);

cleanup:
	return ret;
}

int
rte_thread_set_priority(rte_thread_t thread_id,
	enum rte_thread_priority priority)
{
	struct sched_param param;
	int policy;
	int ret;

	/* Realtime priority can cause crashes on non-Windows platforms. */
	if (priority == RTE_THREAD_PRIORITY_REALTIME_CRITICAL)
		return ENOTSUP;

	ret = thread_map_priority_to_os_value(priority, &param.sched_priority,
		&policy);
	if (ret != 0)
		return ret;

	return pthread_setschedparam((pthread_t)thread_id.opaque_id, policy,
		&param);
}

int
rte_thread_key_create(rte_thread_key *key, void (*destructor)(void *))
{
	int err;

	*key = malloc(sizeof(**key));
	if ((*key) == NULL) {
		EAL_LOG(DEBUG, "Cannot allocate TLS key.");
		rte_errno = ENOMEM;
		return -1;
	}
	err = pthread_key_create(&((*key)->thread_index), destructor);
	if (err) {
		EAL_LOG(DEBUG, "pthread_key_create failed: %s",
			strerror(err));
		free(*key);
		rte_errno = ENOEXEC;
		return -1;
	}
	return 0;
}

int
rte_thread_key_delete(rte_thread_key key)
{
	int err;

	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return -1;
	}
	err = pthread_key_delete(key->thread_index);
	if (err) {
		EAL_LOG(DEBUG, "pthread_key_delete failed: %s",
			strerror(err));
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
	int err;

	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return -1;
	}
	err = pthread_setspecific(key->thread_index, value);
	if (err) {
		EAL_LOG(DEBUG, "pthread_setspecific failed: %s",
			strerror(err));
		rte_errno = ENOEXEC;
		return -1;
	}
	return 0;
}

void *
rte_thread_value_get(rte_thread_key key)
{
	if (!key) {
		EAL_LOG(DEBUG, "Invalid TLS key.");
		rte_errno = EINVAL;
		return NULL;
	}
	return pthread_getspecific(key->thread_index);
}

int
rte_thread_set_affinity_by_id(rte_thread_t thread_id,
		const rte_cpuset_t *cpuset)
{
	return pthread_setaffinity_np((pthread_t)thread_id.opaque_id,
		sizeof(*cpuset), cpuset);
}

int
rte_thread_get_affinity_by_id(rte_thread_t thread_id,
		rte_cpuset_t *cpuset)
{
	return pthread_getaffinity_np((pthread_t)thread_id.opaque_id,
		sizeof(*cpuset), cpuset);
}
