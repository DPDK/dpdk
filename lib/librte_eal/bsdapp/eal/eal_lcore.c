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

#include <unistd.h>
#include <sys/sysctl.h>

#include <rte_log.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_thread.h"

/* No topology information available on FreeBSD including NUMA info */
#define cpu_core_id(X) 0
#define cpu_socket_id(X) 0

static int
get_ncpus(void)
{
	int mib[2] = {CTL_HW, HW_NCPU};
	int ncpu;
	size_t len = sizeof(ncpu);

	sysctl(mib, 2, &ncpu, &len, NULL, 0);
	RTE_LOG(INFO, EAL, "Sysctl reports %d cpus\n", ncpu);
	return ncpu;
}

/*
 * fill the cpu_info structure with as much info as we can get.
 * code is similar to linux version, but sadly available info is less.
 */
int
rte_eal_cpu_init(void)
{
	/* pointer to global configuration */
	struct rte_config *config = rte_eal_get_configuration();
	unsigned lcore_id;
	unsigned count = 0;

	const unsigned ncpus = get_ncpus();
	/*
	 * Parse the maximum set of logical cores, detect the subset of running
	 * ones and enable them by default.
	 */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		/* init cpuset for per lcore config */
		CPU_ZERO(&lcore_config[lcore_id].cpuset);

		lcore_config[lcore_id].detected = (lcore_id < ncpus);
		if (lcore_config[lcore_id].detected == 0) {
			config->lcore_role[lcore_id] = ROLE_OFF;
			continue;
		}

		/* By default, lcore 1:1 map to cpu id */
		CPU_SET(lcore_id, &lcore_config[lcore_id].cpuset);

		/* By default, each detected core is enabled */
		config->lcore_role[lcore_id] = ROLE_RTE;
		lcore_config[lcore_id].core_id = cpu_core_id(lcore_id);
		lcore_config[lcore_id].socket_id = cpu_socket_id(lcore_id);
		if (lcore_config[lcore_id].socket_id >= RTE_MAX_NUMA_NODES)
#ifdef RTE_EAL_ALLOW_INV_SOCKET_ID
			lcore_config[lcore_id].socket_id = 0;
#else
			rte_panic("Socket ID (%u) is greater than "
				"RTE_MAX_NUMA_NODES (%d)\n",
				lcore_config[lcore_id].socket_id, RTE_MAX_NUMA_NODES);
#endif
		RTE_LOG(DEBUG, EAL, "Detected lcore %u\n",
				lcore_id);
		count++;
	}
	/* Set the count of enabled logical cores of the EAL configuration */
	config->lcore_count = count;
	RTE_LOG(DEBUG, EAL, "Support maximum %u logical core(s) by configuration.\n",
		RTE_MAX_LCORE);
	RTE_LOG(DEBUG, EAL, "Detected %u lcore(s)\n", config->lcore_count);

	return 0;
}

unsigned
eal_cpu_socket_id(__rte_unused unsigned cpu_id)
{
	return cpu_socket_id(cpu_id);
}
