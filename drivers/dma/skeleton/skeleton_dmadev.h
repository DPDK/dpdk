/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#ifndef SKELETON_DMADEV_H
#define SKELETON_DMADEV_H

#include <pthread.h>

#include <rte_ring.h>

#define SKELDMA_ARG_LCORE	"lcore"

struct skeldma_desc {
	void *src;
	void *dst;
	uint32_t len;
	uint16_t ridx; /* ring idx */
};

struct skeldma_hw {
	int lcore_id; /* cpucopy task affinity core */
	int socket_id;
	pthread_t thread; /* cpucopy task thread */
	volatile int exit_flag; /* cpucopy task exit flag */

	struct skeldma_desc *desc_mem;

	/* Descriptor ring state machine:
	 *
	 *  -----------     enqueue without submit     -----------
	 *  |  empty  |------------------------------->| pending |
	 *  -----------\                               -----------
	 *       ^      \------------                       |
	 *       |                  |                       |submit doorbell
	 *       |                  |                       |
	 *       |                  |enqueue with submit    |
	 *       |get completed     |------------------|    |
	 *       |                                     |    |
	 *       |                                     v    v
	 *  -----------     cpucopy thread working     -----------
	 *  |completed|<-------------------------------| running |
	 *  -----------                                -----------
	 */
	struct rte_ring *desc_empty;
	struct rte_ring *desc_pending;
	struct rte_ring *desc_running;
	struct rte_ring *desc_completed;

	/* Cache delimiter for dataplane API's operation data */
	char cache1 __rte_cache_aligned;
	uint16_t ridx;  /* ring idx */
	uint64_t submitted_count;

	/* Cache delimiter for cpucopy thread's operation data */
	char cache2 __rte_cache_aligned;
	volatile uint32_t zero_req_count;
	uint64_t completed_count;
};

#endif /* SKELETON_DMADEV_H */
