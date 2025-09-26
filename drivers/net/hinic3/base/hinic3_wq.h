/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_WQ_H_
#define _HINIC3_WQ_H_

/* Use 0-level CLA, page size must be: SQ 16B(wqe) * 64k(max_q_depth). */
#define HINIC3_DEFAULT_WQ_PAGE_SIZE 0x100000
#define HINIC3_HW_WQ_PAGE_SIZE	    0x1000

#define MASKED_WQE_IDX(wq, idx) ((idx) & (wq)->mask)

#define WQ_WQE_ADDR(wq, idx)                                                           \
	({                                                                             \
		typeof(wq) __wq = (wq);                                                \
		(void *)((uint64_t)(__wq->queue_buf_vaddr) + ((idx) << __wq->wqebb_shift)); \
	})

struct hinic3_sge {
	uint32_t hi_addr;
	uint32_t lo_addr;
	uint32_t len;
};

struct hinic3_wq {
	/* The addresses are 64 bit in the HW. */
	uint64_t queue_buf_vaddr;

	uint16_t q_depth;
	uint16_t mask;
	RTE_ATOMIC(int32_t)delta;

	uint32_t cons_idx;
	uint32_t prod_idx;

	uint64_t queue_buf_paddr;

	uint32_t wqebb_size;
	uint32_t wqebb_shift;

	uint32_t wq_buf_size;

	const struct rte_memzone *wq_mz;

	uint32_t rsvd[5];
};

void hinic3_put_wqe(struct hinic3_wq *wq, int num_wqebbs);

/**
 * Read a WQE and update CI.
 *
 * @param[in] wq
 * The work queue structure.
 * @param[in] num_wqebbs
 * The number of work queue elements to read.
 * @param[out] cons_idx
 * The updated consumer index.
 *
 * @return
 * The address of WQE, or NULL if not enough elements are available.
 */
void *hinic3_read_wqe(struct hinic3_wq *wq, int num_wqebbs, uint16_t *cons_idx);

/**
 * Allocate command queue blocks and initialize related parameters.
 *
 * @param[in] wq
 * The cmdq->wq structure.
 * @param[in] dev
 * The device context for the hardware.
 * @param[in] cmdq_blocks
 * The number of command queue blocks to allocate.
 * @param[in] wq_buf_size
 * The size of each work queue buffer.
 * @param[in] wqebb_shift
 * The shift value for determining the work queue element size.
 * @param[in] q_depth
 * The depth of each command queue.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_cmdq_alloc(struct hinic3_wq *wq, void *dev, int cmdq_blocks,
		      uint32_t wq_buf_size, uint32_t wqebb_shift, uint16_t q_depth);

void hinic3_cmdq_free(struct hinic3_wq *wq, int cmdq_blocks);

void hinic3_wq_wqe_pg_clear(struct hinic3_wq *wq);

/**
 * Get WQE and update PI.
 *
 * @param[in] wq
 * The cmdq->wq structure.
 * @param[in] num_wqebbs
 * The number of work queue elements to allocate.
 * @param[out] prod_idx
 * The updated producer index, masked according to the queue size.
 *
 * @return
 * The address of the work queue element.
 */
void *hinic3_get_wqe(struct hinic3_wq *wq, int num_wqebbs, uint16_t *prod_idx);

void hinic3_set_sge(struct hinic3_sge *sge, uint64_t addr, uint32_t len);

#endif /* _HINIC3_WQ_H_ */
