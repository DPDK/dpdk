/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <stdint.h>

#include <rte_common.h>
#include <rte_spinlock.h>
#include <rte_lcore.h>
#include <rte_service.h>
#include <rte_memory.h>

#include "sfc_log.h"
#include "sfc_service.h"
#include "sfc_debug.h"

static uint32_t sfc_service_lcore[RTE_MAX_NUMA_NODES];
static rte_spinlock_t sfc_service_lcore_lock = RTE_SPINLOCK_INITIALIZER;

RTE_INIT(sfc_service_lcore_init)
{
	size_t i;

	for (i = 0; i < RTE_DIM(sfc_service_lcore); ++i)
		sfc_service_lcore[i] = RTE_MAX_LCORE;
}

static uint32_t
sfc_find_service_lcore(int *socket_id)
{
	uint32_t service_core_list[RTE_MAX_LCORE];
	uint32_t lcore_id;
	int num;
	int i;

	SFC_ASSERT(rte_spinlock_is_locked(&sfc_service_lcore_lock));

	num = rte_service_lcore_list(service_core_list,
				    RTE_DIM(service_core_list));
	if (num == 0) {
		SFC_GENERIC_LOG(WARNING, "No service cores available");
		return RTE_MAX_LCORE;
	}
	if (num < 0) {
		SFC_GENERIC_LOG(ERR, "Failed to get service core list");
		return RTE_MAX_LCORE;
	}

	for (i = 0; i < num; ++i) {
		lcore_id = service_core_list[i];

		if (*socket_id == SOCKET_ID_ANY) {
			*socket_id = rte_lcore_to_socket_id(lcore_id);
			break;
		} else if (rte_lcore_to_socket_id(lcore_id) ==
			   (unsigned int)*socket_id) {
			break;
		}
	}

	if (i == num) {
		SFC_GENERIC_LOG(WARNING,
			"No service cores reserved at socket %d", *socket_id);
		return RTE_MAX_LCORE;
	}

	return lcore_id;
}

uint32_t
sfc_get_service_lcore(int socket_id)
{
	uint32_t lcore_id = RTE_MAX_LCORE;

	rte_spinlock_lock(&sfc_service_lcore_lock);

	if (socket_id != SOCKET_ID_ANY) {
		lcore_id = sfc_service_lcore[socket_id];
	} else {
		size_t i;

		for (i = 0; i < RTE_DIM(sfc_service_lcore); ++i) {
			if (sfc_service_lcore[i] != RTE_MAX_LCORE) {
				lcore_id = sfc_service_lcore[i];
				break;
			}
		}
	}

	if (lcore_id == RTE_MAX_LCORE) {
		lcore_id = sfc_find_service_lcore(&socket_id);
		if (lcore_id != RTE_MAX_LCORE)
			sfc_service_lcore[socket_id] = lcore_id;
	}

	rte_spinlock_unlock(&sfc_service_lcore_lock);
	return lcore_id;
}
