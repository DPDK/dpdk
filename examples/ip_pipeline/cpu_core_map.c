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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_lcore.h>

#include "cpu_core_map.h"

struct cpu_core_map {
	uint32_t n_max_sockets;
	uint32_t n_max_cores_per_socket;
	uint32_t n_max_ht_per_core;
	uint32_t n_sockets;
	uint32_t n_cores_per_socket;
	uint32_t n_ht_per_core;
	int map[0];
};

static inline uint32_t
cpu_core_map_pos(struct cpu_core_map *map,
	uint32_t socket_id,
	uint32_t core_id,
	uint32_t ht_id)
{
	return (socket_id * map->n_max_cores_per_socket + core_id) *
		map->n_max_ht_per_core + ht_id;
}

static int
cpu_core_map_compute_eal(struct cpu_core_map *map);

static int
cpu_core_map_compute_linux(struct cpu_core_map *map);

static int
cpu_core_map_compute_and_check(struct cpu_core_map *map);

struct cpu_core_map *
cpu_core_map_init(uint32_t n_max_sockets,
	uint32_t n_max_cores_per_socket,
	uint32_t n_max_ht_per_core,
	uint32_t eal_initialized)
{
	uint32_t map_size, map_mem_size, i;
	struct cpu_core_map *map;
	int status;

	/* Check input arguments */
	if ((n_max_sockets == 0) ||
		(n_max_cores_per_socket == 0) ||
		(n_max_ht_per_core == 0))
		return NULL;

	/* Memory allocation */
	map_size = n_max_sockets * n_max_cores_per_socket * n_max_ht_per_core;
	map_mem_size = sizeof(struct cpu_core_map) + map_size * sizeof(int);
	map = (struct cpu_core_map *) malloc(map_mem_size);
	if (map == NULL)
		return NULL;

	/* Initialization */
	map->n_max_sockets = n_max_sockets;
	map->n_max_cores_per_socket = n_max_cores_per_socket;
	map->n_max_ht_per_core = n_max_ht_per_core;
	map->n_sockets = 0;
	map->n_cores_per_socket = 0;
	map->n_ht_per_core = 0;

	for (i = 0; i < map_size; i++)
		map->map[i] = -1;

	status = (eal_initialized) ?
		cpu_core_map_compute_eal(map) :
		cpu_core_map_compute_linux(map);

	if (status) {
		free(map);
		return NULL;
	}

	status = cpu_core_map_compute_and_check(map);
	if (status) {
		free(map);
		return NULL;
	}

	return map;
}

int
cpu_core_map_compute_eal(struct cpu_core_map *map)
{
	uint32_t socket_id, core_id, ht_id;

	/* Compute map */
	for (socket_id = 0; socket_id < map->n_max_sockets; socket_id++) {
		uint32_t n_detected, core_id_contig;
		int lcore_id;

		n_detected = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			struct lcore_config *p = &lcore_config[lcore_id];

			if ((p->detected) && (p->socket_id == socket_id))
				n_detected++;
		}

		core_id_contig = 0;

		for (core_id = 0; n_detected ; core_id++) {
			ht_id = 0;

			for (lcore_id = 0;
				lcore_id < RTE_MAX_LCORE;
				lcore_id++) {
				struct lcore_config *p =
					&lcore_config[lcore_id];

				if ((p->detected) &&
					(p->socket_id == socket_id) &&
					(p->core_id == core_id)) {
					uint32_t pos = cpu_core_map_pos(map,
						socket_id,
						core_id_contig,
						ht_id);

					map->map[pos] = lcore_id;
					ht_id++;
					n_detected--;
				}
			}

			if (ht_id) {
				core_id_contig++;
				if (core_id_contig ==
					map->n_max_cores_per_socket)
					return -1;
			}
		}
	}

	return 0;
}

int
cpu_core_map_compute_and_check(struct cpu_core_map *map)
{
	uint32_t socket_id, core_id, ht_id;

	/* Compute n_ht_per_core, n_cores_per_socket, n_sockets */
	for (ht_id = 0; ht_id < map->n_max_ht_per_core; ht_id++) {
		if (map->map[ht_id] == -1)
			break;

		map->n_ht_per_core++;
	}

	if (map->n_ht_per_core == 0)
		return -1;

	for (core_id = 0; core_id < map->n_max_cores_per_socket; core_id++) {
		uint32_t pos = core_id * map->n_max_ht_per_core;

		if (map->map[pos] == -1)
			break;

		map->n_cores_per_socket++;
	}

	if (map->n_cores_per_socket == 0)
		return -1;

	for (socket_id = 0; socket_id < map->n_max_sockets; socket_id++) {
		uint32_t pos = socket_id * map->n_max_cores_per_socket *
			map->n_max_ht_per_core;

		if (map->map[pos] == -1)
			break;

		map->n_sockets++;
	}

	if (map->n_sockets == 0)
		return -1;

	/* Check that each socket has exactly the same number of cores
	and that each core has exactly the same number of hyper-threads */
	for (socket_id = 0; socket_id < map->n_sockets; socket_id++) {
		for (core_id = 0; core_id < map->n_cores_per_socket; core_id++)
			for (ht_id = 0;
				ht_id < map->n_max_ht_per_core;
				ht_id++) {
				uint32_t pos = (socket_id *
					map->n_max_cores_per_socket + core_id) *
					map->n_max_ht_per_core + ht_id;

				if (((ht_id < map->n_ht_per_core) &&
					(map->map[pos] == -1)) ||
					((ht_id >= map->n_ht_per_core) &&
					(map->map[pos] != -1)))
					return -1;
			}

		for ( ; core_id < map->n_max_cores_per_socket; core_id++)
			for (ht_id = 0;
				ht_id < map->n_max_ht_per_core;
				ht_id++) {
				uint32_t pos = cpu_core_map_pos(map,
					socket_id,
					core_id,
					ht_id);

				if (map->map[pos] != -1)
					return -1;
			}
	}

	return 0;
}

#define FILE_LINUX_CPU_N_LCORES \
	"/sys/devices/system/cpu/present"

static int
cpu_core_map_get_n_lcores_linux(void)
{
	char buffer[64], *string;
	FILE *fd;

	fd = fopen(FILE_LINUX_CPU_N_LCORES, "r");
	if (fd == NULL)
		return -1;

	if (fgets(buffer, sizeof(buffer), fd) == NULL) {
		fclose(fd);
		return -1;
	}

	fclose(fd);

	string = index(buffer, '-');
	if (string == NULL)
		return -1;

	return atoi(++string) + 1;
}

#define FILE_LINUX_CPU_CORE_ID \
	"/sys/devices/system/cpu/cpu%" PRIu32 "/topology/core_id"

static int
cpu_core_map_get_core_id_linux(int lcore_id)
{
	char buffer[64];
	FILE *fd;
	int core_id;

	snprintf(buffer, sizeof(buffer), FILE_LINUX_CPU_CORE_ID, lcore_id);
	fd = fopen(buffer, "r");
	if (fd == NULL)
		return -1;

	if (fgets(buffer, sizeof(buffer), fd) == NULL) {
		fclose(fd);
		return -1;
	}

	fclose(fd);

	core_id = atoi(buffer);
	return core_id;
}

#define FILE_LINUX_CPU_SOCKET_ID \
	"/sys/devices/system/cpu/cpu%" PRIu32 "/topology/physical_package_id"

static int
cpu_core_map_get_socket_id_linux(int lcore_id)
{
	char buffer[64];
	FILE *fd;
	int socket_id;

	snprintf(buffer, sizeof(buffer), FILE_LINUX_CPU_SOCKET_ID, lcore_id);
	fd = fopen(buffer, "r");
	if (fd == NULL)
		return -1;

	if (fgets(buffer, sizeof(buffer), fd) == NULL) {
		fclose(fd);
		return -1;
	}

	fclose(fd);

	socket_id = atoi(buffer);
	return socket_id;
}

int
cpu_core_map_compute_linux(struct cpu_core_map *map)
{
	uint32_t socket_id, core_id, ht_id;
	int n_lcores;

	n_lcores = cpu_core_map_get_n_lcores_linux();
	if (n_lcores <= 0)
		return -1;

	/* Compute map */
	for (socket_id = 0; socket_id < map->n_max_sockets; socket_id++) {
		uint32_t n_detected, core_id_contig;
		int lcore_id;

		n_detected = 0;
		for (lcore_id = 0; lcore_id < n_lcores; lcore_id++) {
			int lcore_socket_id =
				cpu_core_map_get_socket_id_linux(lcore_id);

#if !defined(RTE_ARCH_PPC_64)
			if (lcore_socket_id < 0)
				return -1;
#endif

			if (((uint32_t) lcore_socket_id) == socket_id)
				n_detected++;
		}

		core_id_contig = 0;

		for (core_id = 0; n_detected ; core_id++) {
			ht_id = 0;

			for (lcore_id = 0; lcore_id < n_lcores; lcore_id++) {
				int lcore_socket_id =
					cpu_core_map_get_socket_id_linux(
					lcore_id);

#if !defined(RTE_ARCH_PPC_64)
				if (lcore_socket_id < 0)
					return -1;

				int lcore_core_id =
					cpu_core_map_get_core_id_linux(
						lcore_id);

				if (lcore_core_id < 0)
					return -1;
#endif

#if !defined(RTE_ARCH_PPC_64)
				if (((uint32_t) lcore_socket_id == socket_id) &&
					((uint32_t) lcore_core_id == core_id)) {
#else
				if (((uint32_t) lcore_socket_id == socket_id)) {
#endif
					uint32_t pos = cpu_core_map_pos(map,
						socket_id,
						core_id_contig,
						ht_id);

					map->map[pos] = lcore_id;
					ht_id++;
					n_detected--;
				}
			}

			if (ht_id) {
				core_id_contig++;
				if (core_id_contig ==
					map->n_max_cores_per_socket)
					return -1;
			}
		}
	}

	return 0;
}

void
cpu_core_map_print(struct cpu_core_map *map)
{
	uint32_t socket_id, core_id, ht_id;

	if (map == NULL)
		return;

	for (socket_id = 0; socket_id < map->n_sockets; socket_id++) {
		printf("Socket %" PRIu32 ":\n", socket_id);

		for (core_id = 0;
			core_id < map->n_cores_per_socket;
			core_id++) {
			printf("[%" PRIu32 "] = [", core_id);

			for (ht_id = 0; ht_id < map->n_ht_per_core; ht_id++) {
				int lcore_id = cpu_core_map_get_lcore_id(map,
					socket_id,
					core_id,
					ht_id);

				uint32_t core_id_noncontig =
					cpu_core_map_get_core_id_linux(
						lcore_id);

				printf(" %" PRId32 " (%" PRIu32 ") ",
					lcore_id,
					core_id_noncontig);
			}

			printf("]\n");
		}
	}
}

uint32_t
cpu_core_map_get_n_sockets(struct cpu_core_map *map)
{
	if (map == NULL)
		return 0;

	return map->n_sockets;
}

uint32_t
cpu_core_map_get_n_cores_per_socket(struct cpu_core_map *map)
{
	if (map == NULL)
		return 0;

	return map->n_cores_per_socket;
}

uint32_t
cpu_core_map_get_n_ht_per_core(struct cpu_core_map *map)
{
	if (map == NULL)
		return 0;

	return map->n_ht_per_core;
}

int
cpu_core_map_get_lcore_id(struct cpu_core_map *map,
	uint32_t socket_id,
	uint32_t core_id,
	uint32_t ht_id)
{
	uint32_t pos;

	if ((map == NULL) ||
		(socket_id >= map->n_sockets) ||
		(core_id >= map->n_cores_per_socket) ||
		(ht_id >= map->n_ht_per_core))
		return -1;

	pos = cpu_core_map_pos(map, socket_id, core_id, ht_id);

	return map->map[pos];
}

void
cpu_core_map_free(struct cpu_core_map *map)
{
	free(map);
}
