/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>

#include <rte_common.h>

/* global data structure that contains the CPU map */
static struct _wcpu_map {
	unsigned int total_procs;
	unsigned int proc_sockets;
	unsigned int proc_cores;
	unsigned int reserved;
	struct _win_lcore_map {
		uint8_t socket_id;
		uint8_t core_id;
	} wlcore_map[RTE_MAX_LCORE];
} wcpu_map = { 0 };

/*
 * Create a map of all processors and associated cores on the system
 */
void
eal_create_cpu_map()
{
	wcpu_map.total_procs =
		GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

	LOGICAL_PROCESSOR_RELATIONSHIP lprocRel;
	DWORD lprocInfoSize = 0;
	BOOL ht_enabled = FALSE;

	/* First get the processor package information */
	lprocRel = RelationProcessorPackage;
	/* Determine the size of buffer we need (pass NULL) */
	GetLogicalProcessorInformationEx(lprocRel, NULL, &lprocInfoSize);
	wcpu_map.proc_sockets = lprocInfoSize / 48;

	lprocInfoSize = 0;
	/* Next get the processor core information */
	lprocRel = RelationProcessorCore;
	GetLogicalProcessorInformationEx(lprocRel, NULL, &lprocInfoSize);
	wcpu_map.proc_cores = lprocInfoSize / 48;

	if (wcpu_map.total_procs > wcpu_map.proc_cores)
		ht_enabled = TRUE;

	/* Distribute the socket and core ids appropriately
	 * across the logical cores. For now, split the cores
	 * equally across the sockets.
	 */
	unsigned int lcore = 0;
	for (unsigned int socket = 0; socket <
			wcpu_map.proc_sockets; ++socket) {
		for (unsigned int core = 0;
			core < (wcpu_map.proc_cores / wcpu_map.proc_sockets);
			++core) {
			wcpu_map.wlcore_map[lcore]
					.socket_id = socket;
			wcpu_map.wlcore_map[lcore]
					.core_id = core;
			lcore++;
			if (ht_enabled) {
				wcpu_map.wlcore_map[lcore]
					.socket_id = socket;
				wcpu_map.wlcore_map[lcore]
					.core_id = core;
				lcore++;
			}
		}
	}
}

/*
 * Check if a cpu is present by the presence of the cpu information for it
 */
int
eal_cpu_detected(unsigned int lcore_id)
{
	return (lcore_id < wcpu_map.total_procs);
}

/*
 * Get CPU socket id for a logical core
 */
unsigned
eal_cpu_socket_id(unsigned int lcore_id)
{
	return wcpu_map.wlcore_map[lcore_id].socket_id;
}

/*
 * Get CPU socket id (NUMA node) for a logical core
 */
unsigned
eal_cpu_core_id(unsigned int lcore_id)
{
	return wcpu_map.wlcore_map[lcore_id].core_id;
}
