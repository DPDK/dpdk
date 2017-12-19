/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_CPU_CORE_MAP_H__
#define __INCLUDE_CPU_CORE_MAP_H__

#include <stdio.h>

#include <rte_lcore.h>

struct cpu_core_map;

struct cpu_core_map *
cpu_core_map_init(uint32_t n_max_sockets,
	uint32_t n_max_cores_per_socket,
	uint32_t n_max_ht_per_core,
	uint32_t eal_initialized);

uint32_t
cpu_core_map_get_n_sockets(struct cpu_core_map *map);

uint32_t
cpu_core_map_get_n_cores_per_socket(struct cpu_core_map *map);

uint32_t
cpu_core_map_get_n_ht_per_core(struct cpu_core_map *map);

int
cpu_core_map_get_lcore_id(struct cpu_core_map *map,
	uint32_t socket_id,
	uint32_t core_id,
	uint32_t ht_id);

void cpu_core_map_print(struct cpu_core_map *map);

void
cpu_core_map_free(struct cpu_core_map *map);

#endif
