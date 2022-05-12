/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_lcore.h>

#include "eal_private.h"
#include "eal_thread.h"
#include "eal_windows.h"

/** Number of logical processors (cores) in a processor group (32 or 64). */
#define EAL_PROCESSOR_GROUP_SIZE (sizeof(KAFFINITY) * CHAR_BIT)

struct lcore_map {
	uint8_t socket_id;
	uint8_t core_id;
};

struct socket_map {
	uint16_t node_id;
};

struct cpu_map {
	unsigned int lcore_count;
	unsigned int socket_count;
	unsigned int cpu_count;
	struct lcore_map lcores[RTE_MAX_LCORE];
	struct socket_map sockets[RTE_MAX_NUMA_NODES];
	GROUP_AFFINITY cpus[CPU_SETSIZE];
};

static struct cpu_map cpu_map;

/* eal_create_cpu_map() is called before logging is initialized */
static void
__rte_format_printf(1, 2)
log_early(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
}

static int
eal_query_group_affinity(void)
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *infos = NULL;
	unsigned int *cpu_count = &cpu_map.cpu_count;
	DWORD infos_size = 0;
	int ret = 0;
	USHORT group_count;
	KAFFINITY affinity;
	USHORT group_no;
	unsigned int i;

	if (!GetLogicalProcessorInformationEx(RelationGroup, NULL,
			&infos_size)) {
		DWORD error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER) {
			RTE_LOG(ERR, EAL, "Cannot get group information size, error %lu\n", error);
			rte_errno = EINVAL;
			ret = -1;
			goto cleanup;
		}
	}

	infos = malloc(infos_size);
	if (infos == NULL) {
		RTE_LOG(ERR, EAL, "Cannot allocate memory for NUMA node information\n");
		rte_errno = ENOMEM;
		ret = -1;
		goto cleanup;
	}

	if (!GetLogicalProcessorInformationEx(RelationGroup, infos,
			&infos_size)) {
		RTE_LOG(ERR, EAL, "Cannot get group information, error %lu\n",
			GetLastError());
		rte_errno = EINVAL;
		ret = -1;
		goto cleanup;
	}

	*cpu_count = 0;
	group_count = infos->Group.ActiveGroupCount;
	for (group_no = 0; group_no < group_count; group_no++) {
		affinity = infos->Group.GroupInfo[group_no].ActiveProcessorMask;
		for (i = 0; i < EAL_PROCESSOR_GROUP_SIZE; i++) {
			if ((affinity & ((KAFFINITY)1 << i)) == 0)
				continue;
			cpu_map.cpus[*cpu_count].Group = group_no;
			cpu_map.cpus[*cpu_count].Mask = (KAFFINITY)1 << i;
			(*cpu_count)++;
		}
	}

cleanup:
	free(infos);
	return ret;
}

static bool
eal_create_lcore_map(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *info)
{
	const unsigned int node_id = info->NumaNode.NodeNumber;
	const GROUP_AFFINITY *cores = &info->NumaNode.GroupMask;
	struct lcore_map *lcore;
	unsigned int socket_id;
	unsigned int i;

	/*
	 * NUMA node may be reported multiple times if it includes
	 * cores from different processor groups, e. g. 80 cores
	 * of a physical processor comprise one NUMA node, but two
	 * processor groups, because group size is limited by 32/64.
	 */
	for (socket_id = 0; socket_id < cpu_map.socket_count; socket_id++)
		if (cpu_map.sockets[socket_id].node_id == node_id)
			break;

	if (socket_id == cpu_map.socket_count) {
		if (socket_id == RTE_DIM(cpu_map.sockets))
			return true;

		cpu_map.sockets[socket_id].node_id = node_id;
		cpu_map.socket_count++;
	}

	for (i = 0; i < EAL_PROCESSOR_GROUP_SIZE; i++) {
		if ((cores->Mask & ((KAFFINITY)1 << i)) == 0)
			continue;

		if (cpu_map.lcore_count == RTE_DIM(cpu_map.lcores))
			return true;

		lcore = &cpu_map.lcores[cpu_map.lcore_count];
		lcore->socket_id = socket_id;
		lcore->core_id = cores->Group * EAL_PROCESSOR_GROUP_SIZE + i;
		cpu_map.lcore_count++;
	}
	return false;
}

int
eal_create_cpu_map(void)
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *infos, *info;
	DWORD infos_size;
	bool full = false;
	int ret = 0;

	infos = NULL;
	infos_size = 0;
	if (!GetLogicalProcessorInformationEx(
			RelationNumaNode, NULL, &infos_size)) {
		DWORD error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER) {
			log_early("Cannot get NUMA node info size, error %lu\n",
				GetLastError());
			rte_errno = ENOMEM;
			ret = -1;
			goto exit;
		}
	}

	infos = malloc(infos_size);
	if (infos == NULL) {
		log_early("Cannot allocate memory for NUMA node information\n");
		rte_errno = ENOMEM;
		ret = -1;
		goto exit;
	}

	if (!GetLogicalProcessorInformationEx(
			RelationNumaNode, infos, &infos_size)) {
		log_early("Cannot get NUMA node information, error %lu\n",
			GetLastError());
		rte_errno = EINVAL;
		ret = -1;
		goto exit;
	}

	info = infos;
	while ((uint8_t *)info - (uint8_t *)infos < infos_size) {
		if (eal_create_lcore_map(info)) {
			full = true;
			break;
		}

		info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)(
			(uint8_t *)info + info->Size);
	}

	if (eal_query_group_affinity()) {
		/*
		 * No need to set rte_errno here.
		 * It is set by eal_query_group_affinity().
		 */
		ret = -1;
		goto exit;
	}

exit:
	if (full) {
		/* Not a fatal error, but important for troubleshooting. */
		log_early("Enumerated maximum of %u NUMA nodes and %u cores\n",
			cpu_map.socket_count, cpu_map.lcore_count);
	}

	free(infos);

	return ret;
}

int
eal_cpu_detected(unsigned int lcore_id)
{
	return lcore_id < cpu_map.lcore_count;
}

unsigned
eal_cpu_socket_id(unsigned int lcore_id)
{
	return cpu_map.lcores[lcore_id].socket_id;
}

unsigned
eal_cpu_core_id(unsigned int lcore_id)
{
	return cpu_map.lcores[lcore_id].core_id;
}

unsigned int
eal_socket_numa_node(unsigned int socket_id)
{
	return cpu_map.sockets[socket_id].node_id;
}

PGROUP_AFFINITY
eal_get_cpu_affinity(size_t cpu_index)
{
	RTE_VERIFY(cpu_index < CPU_SETSIZE);

	return &cpu_map.cpus[cpu_index];
}
