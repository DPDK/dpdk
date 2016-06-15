/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#define RTE_KEEPALIVE_SHM_NAME "/dpdk_keepalive_shm_name"

#define RTE_KEEPALIVE_SHM_ALIVE 1
#define RTE_KEEPALIVE_SHM_DEAD 2

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <rte_keepalive.h>

/**
 * Keepalive SHM structure.
 *
 * The shared memory allocated by the primary is this size, and contains the
 * information as contained within this struct. A secondary may open the SHM,
 * and read the contents.
 */
struct rte_keepalive_shm {
	/** IPC semaphore. Posted when a core dies */
	sem_t core_died;

	/**
	 * Relayed status of each core.
	 */
	enum rte_keepalive_state core_state[RTE_KEEPALIVE_MAXCORES];

	/**
	 * Last-seen-alive timestamps for the cores
	 */
	uint64_t core_last_seen_times[RTE_KEEPALIVE_MAXCORES];
};

/**
 * Create shared host memory keepalive object.
 * @return
 *  Pointer to SHM keepalive structure, or NULL on failure.
 */
struct rte_keepalive_shm *rte_keepalive_shm_create(void);

/**
 * Relays state for given core
 * @param *shm
 *  Pointer to SHM keepalive structure.
 * @param id_core
 *  Id of core
 * @param core_state
 *  State of core
 * @param last_alive
 *  Last seen timestamp for core
 */
void rte_keepalive_relayed_state(struct rte_keepalive_shm *shm,
	const int id_core, const enum rte_keepalive_state core_state,
	uint64_t last_alive);
