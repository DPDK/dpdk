/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
