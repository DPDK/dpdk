/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <assert.h>
#include <string.h>

#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_log.h>

#include "eal_private.h"
#include "eal_thread.h"

RTE_DECLARE_PER_LCORE(unsigned , _socket_id);

unsigned rte_socket_id(void)
{
	return RTE_PER_LCORE(_socket_id);
}

int
rte_lcore_has_role(unsigned int lcore_id, enum rte_lcore_role_t role)
{
	struct rte_config *cfg = rte_eal_get_configuration();

	if (lcore_id >= RTE_MAX_LCORE)
		return -EINVAL;

	return cfg->lcore_role[lcore_id] == role;
}

int eal_cpuset_socket_id(rte_cpuset_t *cpusetp)
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

	} while (++cpu < RTE_MAX_LCORE);

	return socket_id;
}

int
rte_thread_set_affinity(rte_cpuset_t *cpusetp)
{
	int s;
	unsigned lcore_id;
	pthread_t tid;

	tid = pthread_self();

	s = pthread_setaffinity_np(tid, sizeof(rte_cpuset_t), cpusetp);
	if (s != 0) {
		RTE_LOG(ERR, EAL, "pthread_setaffinity_np failed\n");
		return -1;
	}

	/* store socket_id in TLS for quick access */
	RTE_PER_LCORE(_socket_id) =
		eal_cpuset_socket_id(cpusetp);

	/* store cpuset in TLS for quick access */
	memmove(&RTE_PER_LCORE(_cpuset), cpusetp,
		sizeof(rte_cpuset_t));

	lcore_id = rte_lcore_id();
	if (lcore_id != (unsigned)LCORE_ID_ANY) {
		/* EAL thread will update lcore_config */
		lcore_config[lcore_id].socket_id = RTE_PER_LCORE(_socket_id);
		memmove(&lcore_config[lcore_id].cpuset, cpusetp,
			sizeof(rte_cpuset_t));
	}

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
eal_thread_dump_affinity(char *str, unsigned size)
{
	rte_cpuset_t cpuset;
	unsigned cpu;
	int ret;
	unsigned int out = 0;

	rte_thread_get_affinity(&cpuset);

	for (cpu = 0; cpu < RTE_MAX_LCORE; cpu++) {
		if (!CPU_ISSET(cpu, &cpuset))
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


struct rte_thread_ctrl_params {
	void *(*start_routine)(void *);
	void *arg;
	pthread_barrier_t configured;
};

static void *rte_thread_init(void *arg)
{
	int ret;
	struct rte_thread_ctrl_params *params = arg;
	void *(*start_routine)(void *) = params->start_routine;
	void *routine_arg = params->arg;

	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	return start_routine(routine_arg);
}

__rte_experimental int
rte_ctrl_thread_create(pthread_t *thread, const char *name,
		const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg)
{
	struct rte_thread_ctrl_params *params;
	unsigned int lcore_id;
	rte_cpuset_t cpuset;
	int cpu_found, ret;

	params = malloc(sizeof(*params));
	if (!params)
		return -1;

	params->start_routine = start_routine;
	params->arg = arg;

	pthread_barrier_init(&params->configured, NULL, 2);

	ret = pthread_create(thread, attr, rte_thread_init, (void *)params);
	if (ret != 0) {
		free(params);
		return ret;
	}

	if (name != NULL) {
		ret = rte_thread_setname(*thread, name);
		if (ret < 0)
			goto fail;
	}

	cpu_found = 0;
	CPU_ZERO(&cpuset);
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (eal_cpu_detected(lcore_id) &&
				rte_lcore_has_role(lcore_id, ROLE_OFF)) {
			CPU_SET(lcore_id, &cpuset);
			cpu_found = 1;
		}
	}
	/* if no detected cpu is off, use master core */
	if (!cpu_found)
		CPU_SET(rte_get_master_lcore(), &cpuset);

	ret = pthread_setaffinity_np(*thread, sizeof(cpuset), &cpuset);
	if (ret < 0)
		goto fail;

	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	return 0;

fail:
	if (PTHREAD_BARRIER_SERIAL_THREAD ==
	    pthread_barrier_wait(&params->configured)) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}
	pthread_cancel(*thread);
	pthread_join(*thread, NULL);
	return ret;
}
