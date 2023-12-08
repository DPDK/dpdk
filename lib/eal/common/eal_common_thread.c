/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <string.h>

#include <eal_trace_internal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_trace_point.h>

#include "eal_internal_cfg.h"
#include "eal_private.h"
#include "eal_thread.h"
#include "eal_trace.h"

RTE_DEFINE_PER_LCORE(unsigned int, _lcore_id) = LCORE_ID_ANY;
RTE_DEFINE_PER_LCORE(int, _thread_id) = -1;
static RTE_DEFINE_PER_LCORE(unsigned int, _socket_id) =
	(unsigned int)SOCKET_ID_ANY;
static RTE_DEFINE_PER_LCORE(rte_cpuset_t, _cpuset);

unsigned rte_socket_id(void)
{
	return RTE_PER_LCORE(_socket_id);
}

static int
eal_cpuset_socket_id(rte_cpuset_t *cpusetp)
{
	unsigned cpu = 0;
	int socket_id = SOCKET_ID_ANY;
	int sid;

	if (cpusetp == NULL)
		return SOCKET_ID_ANY;

	do {
		if (!CPU_ISSET(cpu, cpusetp))
			continue;

		if (socket_id == SOCKET_ID_ANY)
			socket_id = eal_cpu_socket_id(cpu);

		sid = eal_cpu_socket_id(cpu);
		if (socket_id != sid) {
			socket_id = SOCKET_ID_ANY;
			break;
		}

	} while (++cpu < CPU_SETSIZE);

	return socket_id;
}

static void
thread_update_affinity(rte_cpuset_t *cpusetp)
{
	unsigned int lcore_id = rte_lcore_id();

	/* store socket_id in TLS for quick access */
	RTE_PER_LCORE(_socket_id) =
		eal_cpuset_socket_id(cpusetp);

	/* store cpuset in TLS for quick access */
	memmove(&RTE_PER_LCORE(_cpuset), cpusetp,
		sizeof(rte_cpuset_t));

	if (lcore_id != (unsigned)LCORE_ID_ANY) {
		/* EAL thread will update lcore_config */
		lcore_config[lcore_id].socket_id = RTE_PER_LCORE(_socket_id);
		memmove(&lcore_config[lcore_id].cpuset, cpusetp,
			sizeof(rte_cpuset_t));
	}
}

int
rte_thread_set_affinity(rte_cpuset_t *cpusetp)
{
	if (rte_thread_set_affinity_by_id(rte_thread_self(), cpusetp) != 0) {
		EAL_LOG(ERR, "rte_thread_set_affinity_by_id failed");
		return -1;
	}

	thread_update_affinity(cpusetp);
	return 0;
}

void
rte_thread_get_affinity(rte_cpuset_t *cpusetp)
{
	assert(cpusetp);
	memmove(cpusetp, &RTE_PER_LCORE(_cpuset),
		sizeof(rte_cpuset_t));
}

int
eal_thread_dump_affinity(rte_cpuset_t *cpuset, char *str, unsigned int size)
{
	unsigned cpu;
	int ret;
	unsigned int out = 0;

	for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (!CPU_ISSET(cpu, cpuset))
			continue;

		ret = snprintf(str + out,
			       size - out, "%u,", cpu);
		if (ret < 0 || (unsigned)ret >= size - out) {
			/* string will be truncated */
			ret = -1;
			goto exit;
		}

		out += ret;
	}

	ret = 0;
exit:
	/* remove the last separator */
	if (out > 0)
		str[out - 1] = '\0';

	return ret;
}

int
eal_thread_dump_current_affinity(char *str, unsigned int size)
{
	rte_cpuset_t cpuset;

	rte_thread_get_affinity(&cpuset);
	return eal_thread_dump_affinity(&cpuset, str, size);
}

void
__rte_thread_init(unsigned int lcore_id, rte_cpuset_t *cpuset)
{
	/* set the lcore ID in per-lcore memory area */
	RTE_PER_LCORE(_lcore_id) = lcore_id;

	/* acquire system unique id */
	rte_gettid();

	thread_update_affinity(cpuset);

	__rte_trace_mem_per_thread_alloc();
}

void
__rte_thread_uninit(void)
{
	trace_mem_per_thread_free();

	RTE_PER_LCORE(_lcore_id) = LCORE_ID_ANY;
}

/* main loop of threads */
__rte_noreturn uint32_t
eal_thread_loop(void *arg)
{
	unsigned int lcore_id = (uintptr_t)arg;
	char cpuset[RTE_CPU_AFFINITY_STR_LEN];
	int ret;

	__rte_thread_init(lcore_id, &lcore_config[lcore_id].cpuset);

	ret = eal_thread_dump_current_affinity(cpuset, sizeof(cpuset));
	EAL_LOG(DEBUG, "lcore %u is ready (tid=%zx;cpuset=[%s%s])",
		lcore_id, rte_thread_self().opaque_id, cpuset,
		ret == 0 ? "" : "...");

	rte_eal_trace_thread_lcore_ready(lcore_id, cpuset);

	/* read on our pipe to get commands */
	while (1) {
		lcore_function_t *f;
		void *fct_arg;

		eal_thread_wait_command();

		/* Set the state to 'RUNNING'. Use release order
		 * since 'state' variable is used as the guard variable.
		 */
		rte_atomic_store_explicit(&lcore_config[lcore_id].state, RUNNING,
			rte_memory_order_release);

		eal_thread_ack_command();

		/* Load 'f' with acquire order to ensure that
		 * the memory operations from the main thread
		 * are accessed only after update to 'f' is visible.
		 * Wait till the update to 'f' is visible to the worker.
		 */
		while ((f = rte_atomic_load_explicit(&lcore_config[lcore_id].f,
				rte_memory_order_acquire)) == NULL)
			rte_pause();

		rte_eal_trace_thread_lcore_running(lcore_id, f);

		/* call the function and store the return value */
		fct_arg = lcore_config[lcore_id].arg;
		ret = f(fct_arg);
		lcore_config[lcore_id].ret = ret;
		lcore_config[lcore_id].f = NULL;
		lcore_config[lcore_id].arg = NULL;

		/* Store the state with release order to ensure that
		 * the memory operations from the worker thread
		 * are completed before the state is updated.
		 * Use 'state' as the guard variable.
		 */
		rte_atomic_store_explicit(&lcore_config[lcore_id].state, WAIT,
			rte_memory_order_release);

		rte_eal_trace_thread_lcore_stopped(lcore_id);
	}

	/* never reached */
	/* return 0; */
}

enum __rte_ctrl_thread_status {
	CTRL_THREAD_LAUNCHING, /* Yet to call pthread_create function */
	CTRL_THREAD_RUNNING, /* Control thread is running successfully */
	CTRL_THREAD_ERROR /* Control thread encountered an error */
};

struct control_thread_params {
	rte_thread_func start_routine;
	void *arg;
	int ret;
	/* Control thread status.
	 * If the status is CTRL_THREAD_ERROR, 'ret' has the error code.
	 */
	RTE_ATOMIC(enum __rte_ctrl_thread_status) status;
};

static int control_thread_init(void *arg)
{
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	rte_cpuset_t *cpuset = &internal_conf->ctrl_cpuset;
	struct control_thread_params *params = arg;

	__rte_thread_init(rte_lcore_id(), cpuset);
	/* Set control thread socket ID to SOCKET_ID_ANY
	 * as control threads may be scheduled on any NUMA node.
	 */
	RTE_PER_LCORE(_socket_id) = SOCKET_ID_ANY;
	params->ret = rte_thread_set_affinity_by_id(rte_thread_self(), cpuset);
	if (params->ret != 0) {
		rte_atomic_store_explicit(&params->status,
			CTRL_THREAD_ERROR, rte_memory_order_release);
		return 1;
	}

	rte_atomic_store_explicit(&params->status,
		CTRL_THREAD_RUNNING, rte_memory_order_release);

	return 0;
}

static uint32_t control_thread_start(void *arg)
{
	struct control_thread_params *params = arg;
	void *start_arg = params->arg;
	rte_thread_func start_routine = params->start_routine;

	if (control_thread_init(arg) != 0)
		return 0;

	return start_routine(start_arg);
}

int
rte_thread_create_control(rte_thread_t *thread, const char *name,
		rte_thread_func start_routine, void *arg)
{
	struct control_thread_params *params;
	enum __rte_ctrl_thread_status ctrl_thread_status;
	int ret;

	params = malloc(sizeof(*params));
	if (params == NULL)
		return -ENOMEM;

	params->start_routine = start_routine;
	params->arg = arg;
	params->ret = 0;
	params->status = CTRL_THREAD_LAUNCHING;

	ret = rte_thread_create(thread, NULL, control_thread_start, params);
	if (ret != 0) {
		free(params);
		return -ret;
	}

	if (name != NULL)
		rte_thread_set_name(*thread, name);

	/* Wait for the control thread to initialize successfully */
	while ((ctrl_thread_status =
			rte_atomic_load_explicit(&params->status,
			rte_memory_order_acquire)) == CTRL_THREAD_LAUNCHING) {
		rte_delay_us_sleep(1);
	}

	/* Check if the control thread encountered an error */
	if (ctrl_thread_status == CTRL_THREAD_ERROR) {
		/* ctrl thread is exiting */
		rte_thread_join(*thread, NULL);
	}

	ret = params->ret;
	free(params);

	return ret;
}

static void
add_internal_prefix(char *prefixed_name, const char *name, size_t size)
{
	size_t prefixlen;

	/* Check RTE_THREAD_INTERNAL_NAME_SIZE definition. */
	RTE_BUILD_BUG_ON(RTE_THREAD_INTERNAL_NAME_SIZE !=
		RTE_THREAD_NAME_SIZE - sizeof(RTE_THREAD_INTERNAL_PREFIX) + 1);

	prefixlen = strlen(RTE_THREAD_INTERNAL_PREFIX);
	strlcpy(prefixed_name, RTE_THREAD_INTERNAL_PREFIX, size);
	strlcpy(prefixed_name + prefixlen, name, size - prefixlen);
}

int
rte_thread_create_internal_control(rte_thread_t *id, const char *name,
		rte_thread_func func, void *arg)
{
	char prefixed_name[RTE_THREAD_NAME_SIZE];

	add_internal_prefix(prefixed_name, name, sizeof(prefixed_name));
	return rte_thread_create_control(id, prefixed_name, func, arg);
}

void
rte_thread_set_prefixed_name(rte_thread_t id, const char *name)
{
	char prefixed_name[RTE_THREAD_NAME_SIZE];

	add_internal_prefix(prefixed_name, name, sizeof(prefixed_name));
	rte_thread_set_name(id, prefixed_name);
}

int
rte_thread_register(void)
{
	unsigned int lcore_id;
	rte_cpuset_t cpuset;

	/* EAL init flushes all lcores, we can't register before. */
	if (eal_get_internal_configuration()->init_complete != 1) {
		EAL_LOG(DEBUG, "Called %s before EAL init.", __func__);
		rte_errno = EINVAL;
		return -1;
	}
	if (!rte_mp_disable()) {
		EAL_LOG(ERR, "Multiprocess in use, registering non-EAL threads is not supported.");
		rte_errno = EINVAL;
		return -1;
	}
	if (rte_thread_get_affinity_by_id(rte_thread_self(), &cpuset) != 0)
		CPU_ZERO(&cpuset);
	lcore_id = eal_lcore_non_eal_allocate();
	if (lcore_id >= RTE_MAX_LCORE)
		lcore_id = LCORE_ID_ANY;
	__rte_thread_init(lcore_id, &cpuset);
	if (lcore_id == LCORE_ID_ANY) {
		rte_errno = ENOMEM;
		return -1;
	}
	EAL_LOG(DEBUG, "Registered non-EAL thread as lcore %u.",
		lcore_id);
	return 0;
}

void
rte_thread_unregister(void)
{
	unsigned int lcore_id = rte_lcore_id();

	if (lcore_id != LCORE_ID_ANY)
		eal_lcore_non_eal_release(lcore_id);
	__rte_thread_uninit();
	if (lcore_id != LCORE_ID_ANY)
		EAL_LOG(DEBUG, "Unregistered non-EAL thread (was lcore %u).",
			lcore_id);
}

int
rte_thread_attr_init(rte_thread_attr_t *attr)
{
	if (attr == NULL)
		return EINVAL;

	CPU_ZERO(&attr->cpuset);
	attr->priority = RTE_THREAD_PRIORITY_NORMAL;

	return 0;
}

int
rte_thread_attr_set_priority(rte_thread_attr_t *thread_attr,
		enum rte_thread_priority priority)
{
	if (thread_attr == NULL)
		return EINVAL;

	thread_attr->priority = priority;

	return 0;
}

int
rte_thread_attr_set_affinity(rte_thread_attr_t *thread_attr,
		rte_cpuset_t *cpuset)
{
	if (thread_attr == NULL)
		return EINVAL;

	if (cpuset == NULL)
		return EINVAL;

	thread_attr->cpuset = *cpuset;

	return 0;
}

int
rte_thread_attr_get_affinity(rte_thread_attr_t *thread_attr,
		rte_cpuset_t *cpuset)
{
	if (thread_attr == NULL)
		return EINVAL;

	if (cpuset == NULL)
		return EINVAL;

	*cpuset = thread_attr->cpuset;

	return 0;
}
