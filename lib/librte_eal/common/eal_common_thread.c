/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <string.h>

#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_log.h>

#include "eal_thread.h"

RTE_DECLARE_PER_LCORE(unsigned , _socket_id);

unsigned rte_socket_id(void)
{
	return RTE_PER_LCORE(_socket_id);
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
