/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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

/** Shutdown cleanup of shared host memory keepalive object.
 * @param *shm
 *  Pointer to SHM keepalive structure. May be NULL.
 *
 *  If *shm is NULL, this function will only attempt to remove the
 *  shared host memory handle and not unmap the underlying memory.
 */
void rte_keepalive_shm_cleanup(struct rte_keepalive_shm *ka_shm);
