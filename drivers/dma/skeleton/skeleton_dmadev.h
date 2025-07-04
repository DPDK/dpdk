/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 HiSilicon Limited
 */

#ifndef SKELETON_DMADEV_H
#define SKELETON_DMADEV_H

#include <rte_dmadev.h>
#include <rte_ring.h>
#include <rte_thread.h>

#define SKELDMA_ARG_LCORE	"lcore"

#define SKELDMA_MAX_SGES	4

enum skeldma_op {
	SKELDMA_OP_COPY,
	SKELDMA_OP_COPY_SG,
	SKELDMA_OP_FILL,
};

struct skeldma_desc {
	enum skeldma_op op;
	uint16_t ridx; /* ring idx */

	union {
		struct {
			void *src;
			void *dst;
			uint32_t len;
		} copy;
		struct {
			struct rte_dma_sge src[SKELDMA_MAX_SGES];
			struct rte_dma_sge dst[SKELDMA_MAX_SGES];
			uint16_t nb_src;
			uint16_t nb_dst;
		} copy_sg;
		struct {
			void *dst;
			uint32_t len;
			uint64_t pattern;
		} fill;
	};
};

struct skeldma_hw {
	int lcore_id; /* cpuwork task affinity core */
	int socket_id;
	rte_thread_t thread; /* cpuwork task thread */
	volatile int exit_flag; /* cpuwork task exit flag */

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
	 *  -----------     cpuwork thread working     -----------
	 *  |completed|<-------------------------------| running |
	 *  -----------                                -----------
	 */
	struct rte_ring *desc_empty;
	struct rte_ring *desc_pending;
	struct rte_ring *desc_running;
	struct rte_ring *desc_completed;

	/* Cache delimiter for dataplane API's operation data */
	alignas(RTE_CACHE_LINE_SIZE) char cache1;
	uint16_t ridx;  /* ring idx */
	uint16_t last_ridx;
	uint64_t submitted_count;

	/* Cache delimiter for cpuwork thread's operation data */
	alignas(RTE_CACHE_LINE_SIZE) char cache2;
	volatile uint32_t zero_req_count;
	RTE_ATOMIC(uint64_t) completed_count;
};

#endif /* SKELETON_DMADEV_H */
