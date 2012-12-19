/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include "eal_private.h"

#define PROC_CPUINFO "/proc/cpuinfo"
#define PROC_PROCESSOR_FMT ""

/* parse one line and try to match "processor : %d". */
static int
parse_processor_id(const char *buf, unsigned *lcore_id)
{
	static const char _processor[] = "processor";
	const char *s;

	if (strncmp(buf, _processor, sizeof(_processor) - 1) != 0)
		return -1;

	s = strchr(buf, ':');
	if (s == NULL)
		return -1;

	errno = 0;
	*lcore_id = strtoul(s+1, NULL, 10);
	if (errno != 0) {
		*lcore_id = -1;
		return -1;
	}

	return 0;
}

/* parse one line and try to match "physical id : %d". */
static int
parse_socket_id(const char *buf, unsigned *socket_id)
{
	static const char _physical_id[] = "physical id";
	const char *s;

	if (strncmp(buf, _physical_id, sizeof(_physical_id) - 1) != 0)
		return -1;

	s = strchr(buf, ':');
	if (s == NULL)
		return -1;

	errno = 0;
	*socket_id = strtoul(s+1, NULL, 10);
	if (errno != 0)
		return -1;

	return 0;
}

/*
 * Parse /proc/cpuinfo to get the number of physical and logical
 * processors on the machine. The function will fill the cpu_info
 * structure.
 */
int
rte_eal_cpu_init(void)
{
	struct rte_config *config;
	FILE *f;
	char buf[BUFSIZ];
	unsigned lcore_id = 0;
	unsigned socket_id = 0;
	unsigned count = 0;

	/* get pointer to global configuration */
	config = rte_eal_get_configuration();

	/* open /proc/cpuinfo */
	f = fopen(PROC_CPUINFO, "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "%s(): Cannot find "PROC_CPUINFO"\n", __func__);
		return -1;
	}

	/*
	 * browse lines of /proc/cpuinfo and fill memseg entries in
	 * global configuration
	 */
	while (fgets(buf, sizeof(buf), f) != NULL) {

		if (parse_processor_id(buf, &lcore_id) == 0)
			continue;

		if (parse_socket_id(buf, &socket_id) == 0)
			continue;

		if (buf[0] == '\n') {
			RTE_LOG(DEBUG, EAL, "Detected lcore %u on socket %u\n",
				lcore_id, socket_id);
			if (lcore_id >= RTE_MAX_LCORE) {
				RTE_LOG(DEBUG, EAL,
					"Skip lcore %u >= RTE_MAX_LCORE\n",
					  lcore_id);
				continue;
			}

			/*
			 * In a virtualization environment, the socket ID
			 * reported by the system may not be linked to a real
			 * physical socket ID, and may be incoherent. So in this
			 * case, a default socket ID of 0 is assigned.
			 */
			if (socket_id >= RTE_MAX_NUMA_NODES) {
#ifdef CONFIG_RTE_EAL_ALLOW_INV_SOCKET_ID
				socket_id = 0;
#else
				rte_panic("Socket ID (%u) is greater than "
				    "RTE_MAX_NUMA_NODES (%d)\n",
				    socket_id, RTE_MAX_NUMA_NODES);
#endif
			}

			lcore_config[lcore_id].detected = 1;
			lcore_config[lcore_id].socket_id = socket_id;

		}
	}

	fclose(f);

	/* disable lcores that were not detected */
	RTE_LCORE_FOREACH(lcore_id) {

		if (lcore_config[lcore_id].detected == 0) {
			RTE_LOG(DEBUG, EAL, "Skip lcore %u (not detected)\n",
				lcore_id);
			config->lcore_role[lcore_id] = ROLE_OFF;
		}
		else
			count ++;
	}

	config->lcore_count = count;

	return 0;
}
