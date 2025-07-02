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
		(void *)((u64)(__wq->queue_buf_vaddr) + ((idx) << __wq->wqebb_shift)); \
	})

struct hinic3_sge {
	u32 hi_addr;
	u32 lo_addr;
	u32 len;
};

struct hinic3_wq {
	/* The addresses are 64 bit in the HW. */
	u64 queue_buf_vaddr;

	u16 q_depth;
	u16 mask;
	RTE_ATOMIC(int32_t)delta;

	u32 cons_idx;
	u32 prod_idx;

	u64 queue_buf_paddr;

	u32 wqebb_size;
	u32 wqebb_shift;

	u32 wq_buf_size;

	const struct rte_memzone *wq_mz;

	u32 rsvd[5];
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
void *hinic3_read_wqe(struct hinic3_wq *wq, int num_wqebbs, u16 *cons_idx);

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
		      u32 wq_buf_size, u32 wqebb_shift, u16 q_depth);

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
void *hinic3_get_wqe(struct hinic3_wq *wq, int num_wqebbs, u16 *prod_idx);

void hinic3_set_sge(struct hinic3_sge *sge, uint64_t addr, u32 len);

#endif /* _HINIC3_WQ_H_ */
