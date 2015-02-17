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
#include <limits.h>
#include <string.h>
#include <dirent.h>

#include <rte_log.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_filesystem.h"
#include "eal_thread.h"

#define SYS_CPU_DIR "/sys/devices/system/cpu/cpu%u"
#define CORE_ID_FILE "topology/core_id"
#define PHYS_PKG_FILE "topology/physical_package_id"

/* Check if a cpu is present by the presence of the cpu information for it */
static int
cpu_detected(unsigned lcore_id)
{
	char path[PATH_MAX];
	int len = snprintf(path, sizeof(path), SYS_CPU_DIR
		"/"CORE_ID_FILE, lcore_id);
	if (len <= 0 || (unsigned)len >= sizeof(path))
		return 0;
	if (access(path, F_OK) != 0)
		return 0;

	return 1;
}

/* Get CPU socket id (NUMA node) by reading directory
 * /sys/devices/system/cpu/cpuX looking for symlink "nodeY"
 * which gives the NUMA topology information.
 * Note: physical package id != NUMA node, but we use it as a
 * fallback for kernels which don't create a nodeY link
 */
unsigned
eal_cpu_socket_id(unsigned lcore_id)
{
	const char node_prefix[] = "node";
	const size_t prefix_len = sizeof(node_prefix) - 1;
	char path[PATH_MAX];
	DIR *d = NULL;
	unsigned long id = 0;
	struct dirent *e;
	char *endptr = NULL;

	int len = snprintf(path, sizeof(path),
			       SYS_CPU_DIR, lcore_id);
	if (len <= 0 || (unsigned)len >= sizeof(path))
		goto err;

	d = opendir(path);
	if (!d)
		goto err;

	while ((e = readdir(d)) != NULL) {
		if (strncmp(e->d_name, node_prefix, prefix_len) == 0) {
			id = strtoul(e->d_name+prefix_len, &endptr, 0);
			break;
		}
	}
	if (endptr == NULL || *endptr!='\0' || endptr == e->d_name+prefix_len) {
		RTE_LOG(WARNING, EAL, "Cannot read numa node link "
				"for lcore %u - using physical package id instead\n",
				lcore_id);

		len = snprintf(path, sizeof(path), SYS_CPU_DIR "/%s",
				lcore_id, PHYS_PKG_FILE);
		if (len <= 0 || (unsigned)len >= sizeof(path))
			goto err;
		if (eal_parse_sysfs_value(path, &id) != 0)
			goto err;
	}
	closedir(d);
	return (unsigned)id;

err:
	if (d)
		closedir(d);
	RTE_LOG(ERR, EAL, "Error getting NUMA socket information from %s "
			"for lcore %u - assuming NUMA socket 0\n", SYS_CPU_DIR, lcore_id);
	return 0;
}

/* Get the cpu core id value from the /sys/.../cpuX core_id value */
static unsigned
cpu_core_id(unsigned lcore_id)
{
	char path[PATH_MAX];
	unsigned long id;

	int len = snprintf(path, sizeof(path), SYS_CPU_DIR "/%s", lcore_id, CORE_ID_FILE);
	if (len <= 0 || (unsigned)len >= sizeof(path))
		goto err;
	if (eal_parse_sysfs_value(path, &id) != 0)
		goto err;
	return (unsigned)id;

err:
	RTE_LOG(ERR, EAL, "Error reading core id value from %s "
			"for lcore %u - assuming core 0\n", SYS_CPU_DIR, lcore_id);
	return 0;
}

/*
 * Parse /sys/devices/system/cpu to get the number of physical and logical
 * processors on the machine. The function will fill the cpu_info
 * structure.
 */
int
rte_eal_cpu_init(void)
{
	/* pointer to global configuration */
	struct rte_config *config = rte_eal_get_configuration();
	unsigned lcore_id;
	unsigned count = 0;

	/*
	 * Parse the maximum set of logical cores, detect the subset of running
	 * ones and enable them by default.
	 */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		/* init cpuset for per lcore config */
		CPU_ZERO(&lcore_config[lcore_id].cpuset);

		/* in 1:1 mapping, record related cpu detected state */
		lcore_config[lcore_id].detected = cpu_detected(lcore_id);
		if (lcore_config[lcore_id].detected == 0) {
			config->lcore_role[lcore_id] = ROLE_OFF;
			continue;
		}

		/* By default, lcore 1:1 map to cpu id */
		CPU_SET(lcore_id, &lcore_config[lcore_id].cpuset);

		/* By default, each detected core is enabled */
		config->lcore_role[lcore_id] = ROLE_RTE;
		lcore_config[lcore_id].core_id = cpu_core_id(lcore_id);
		lcore_config[lcore_id].socket_id = eal_cpu_socket_id(lcore_id);
		if (lcore_config[lcore_id].socket_id >= RTE_MAX_NUMA_NODES)
#ifdef RTE_EAL_ALLOW_INV_SOCKET_ID
			lcore_config[lcore_id].socket_id = 0;
#else
			rte_panic("Socket ID (%u) is greater than "
				"RTE_MAX_NUMA_NODES (%d)\n",
				lcore_config[lcore_id].socket_id, RTE_MAX_NUMA_NODES);
#endif
		RTE_LOG(DEBUG, EAL, "Detected lcore %u as core %u on socket %u\n",
				lcore_id,
				lcore_config[lcore_id].core_id,
				lcore_config[lcore_id].socket_id);
		count ++;
	}
	/* Set the count of enabled logical cores of the EAL configuration */
	config->lcore_count = count;
	RTE_LOG(DEBUG, EAL, "Support maximum %u logical core(s) by configuration.\n",
		RTE_MAX_LCORE);
	RTE_LOG(DEBUG, EAL, "Detected %u lcore(s)\n", config->lcore_count);

	return 0;
}
