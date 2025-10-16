/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 */

#ifndef HISI_ACC_DMADEV_H
#define HISI_ACC_DMADEV_H

#include <bus_uacce_driver.h>
#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_dmadev_pmd.h>

#define HACC_DMA_DEVARG_QUEUES		"queues"
#define HACC_DMA_DEFAULT_QUEUES		1

#define HACC_DMA_CQ_DOORBELL_PACE	64
#define HACC_DMA_SQ_GAP_NUM		HACC_DMA_CQ_DOORBELL_PACE

struct hacc_dma_config {
	uint16_t queues;

	/* The following fields are config contexts. */
	struct rte_uacce_device *dev;
	uint16_t avail_queues;
};

#define HACC_DMA_DOORBELL_OFFSET		0x1000u

struct hacc_dma_sqe {};
struct hacc_dma_cqe {};

struct hacc_dma_dev {
	struct hacc_dma_sqe *sqe;
	struct hacc_dma_cqe *cqe;
	uint16_t *status;             /* the completion status array of SQEs. */

	volatile void *doorbell_reg;  /**< register address for doorbell. */
	volatile uint32_t *sq_status; /**< SQ status pointer. */
	volatile uint32_t *cq_status; /**< CQ status pointer. */

	uint16_t sqn;           /**< SQ global number, inited when created. */
	uint16_t sq_depth_mask; /**< SQ depth - 1, the SQ depth is power of 2. */

	uint16_t ridx;  /**< ring index which will assign to the next request. */
	uint16_t cridx; /**< ring index which returned by completed APIs. */

	/**
	 * SQE array management fields:
	 *
	 *  -----------------------------------------------------
	 *  | SQE0 | SQE1 | SQE2 |   ...  | SQEx | ... | SQEn-1 |
	 *  -----------------------------------------------------
	 *     ^             ^               ^
	 *     |             |               |
	 *   sq_head     cq_sq_head       sq_tail
	 *
	 *  sq_head: next index to the oldest completed request, this filed was
	 *           updated by completed* APIs.
	 *  sq_tail: index of the next new request, this field was updated by
	 *           copy or fill API.
	 *  cq_sq_head: next index of index that has been completed by hardware,
	 *              this filed was updated by completed* APIs.
	 *
	 *  [sq_head, cq_sq_head): the SQEs that hardware already completed.
	 *  [cq_sq_head, sq_tail): the SQEs that hardware processing.
	 */
	uint16_t sq_head;
	uint16_t sq_tail;
	uint16_t cq_sq_head;
	uint16_t avail_sqes;

	uint16_t cq_depth;      /**< CQ depth, inited when created. */
	uint16_t cq_head;       /**< CQ index for next scans. */
	uint16_t cqs_completed; /**< accumulated number of completed CQs. */
	uint8_t  cqe_vld;       /**< valid bit for CQE, will change for every round. */

	uint64_t submitted;
	uint64_t completed;
	uint64_t errors;
	uint64_t invalid_lens;
	uint64_t io_errors;
	uint64_t qfulls;

	/**
	 * The following fields are not accessed in the I/O path, so they are
	 * placed at the end.
	 */
	struct rte_dma_dev_data *data;
	struct rte_uacce_qcontex qctx;
	void *io_base;
	void *dus_base;
	uint32_t sqe_size;
	uint16_t sq_depth;
	bool started;
};

#endif /* HISI_ACC_DMADEV_H */
