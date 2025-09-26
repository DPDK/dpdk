/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_hwdev.h"
#include "hinic3_wq.h"

static void
free_wq_pages(struct hinic3_wq *wq)
{
	hinic3_memzone_free(wq->wq_mz);

	wq->queue_buf_paddr = 0;
	wq->queue_buf_vaddr = 0;
}

static int
alloc_wq_pages(struct hinic3_hwdev *hwdev, struct hinic3_wq *wq, int qid)
{
	const struct rte_memzone *wq_mz;

	wq_mz = hinic3_dma_zone_reserve(hwdev->eth_dev, "hinic3_wq_mz",
					qid, wq->wq_buf_size, RTE_PGSIZE_256K, SOCKET_ID_ANY);
	if (!wq_mz) {
		PMD_DRV_LOG(ERR, "Allocate wq[%d] rq_mz failed", qid);
		return -ENOMEM;
	}

	memset(wq_mz->addr, 0, wq->wq_buf_size);
	wq->wq_mz = wq_mz;
	wq->queue_buf_paddr = wq_mz->iova;
	wq->queue_buf_vaddr = (uint64_t)wq_mz->addr;

	return 0;
}

void
hinic3_put_wqe(struct hinic3_wq *wq, int num_wqebbs)
{
	wq->cons_idx += num_wqebbs;
	rte_atomic_fetch_add_explicit(&wq->delta, num_wqebbs,
				      rte_memory_order_seq_cst);
}

void *
hinic3_read_wqe(struct hinic3_wq *wq, int num_wqebbs, uint16_t *cons_idx)
{
	uint16_t curr_cons_idx;

	if ((rte_atomic_load_explicit(&wq->delta, rte_memory_order_seq_cst) +
	     num_wqebbs) > wq->q_depth)
		return NULL;

	curr_cons_idx = (uint16_t)(wq->cons_idx);

	curr_cons_idx = MASKED_WQE_IDX(wq, curr_cons_idx);

	*cons_idx = curr_cons_idx;

	return WQ_WQE_ADDR(wq, (uint32_t)(*cons_idx));
}

int
hinic3_cmdq_alloc(struct hinic3_wq *wq, void *dev, int cmdq_blocks,
		  uint32_t wq_buf_size, uint32_t wqebb_shift, uint16_t q_depth)
{
	struct hinic3_hwdev *hwdev = (struct hinic3_hwdev *)dev;
	int i, j;
	int err;

	for (i = 0; i < cmdq_blocks; i++) {
		wq[i].wqebb_size = 1U << wqebb_shift;
		wq[i].wqebb_shift = wqebb_shift;
		wq[i].wq_buf_size = wq_buf_size;
		wq[i].q_depth = q_depth;

		err = alloc_wq_pages(hwdev, &wq[i], i);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to alloc CMDQ blocks");
			goto cmdq_block_err;
		}

		wq[i].cons_idx = 0;
		wq[i].prod_idx = 0;
		rte_atomic_store_explicit(&wq[i].delta, q_depth,
					  rte_memory_order_seq_cst);

		wq[i].mask = q_depth - 1;
	}

	return 0;

cmdq_block_err:
	for (j = 0; j < i; j++)
		free_wq_pages(&wq[j]);

	return err;
}

void
hinic3_cmdq_free(struct hinic3_wq *wq, int cmdq_blocks)
{
	int i;

	for (i = 0; i < cmdq_blocks; i++)
		free_wq_pages(&wq[i]);
}

void
hinic3_wq_wqe_pg_clear(struct hinic3_wq *wq)
{
	wq->cons_idx = 0;
	wq->prod_idx = 0;

	memset((void *)wq->queue_buf_vaddr, 0, wq->wq_buf_size);
}

void *
hinic3_get_wqe(struct hinic3_wq *wq, int num_wqebbs, uint16_t *prod_idx)
{
	uint16_t curr_prod_idx;

	rte_atomic_fetch_sub_explicit(&wq->delta, num_wqebbs,
				      rte_memory_order_seq_cst);
	curr_prod_idx = (uint16_t)(wq->prod_idx);
	wq->prod_idx += num_wqebbs;
	*prod_idx = MASKED_WQE_IDX(wq, curr_prod_idx);

	return WQ_WQE_ADDR(wq, (uint32_t)(*prod_idx));
}

void
hinic3_set_sge(struct hinic3_sge *sge, uint64_t addr, uint32_t len)
{
	sge->hi_addr = upper_32_bits(addr);
	sge->lo_addr = lower_32_bits(addr);
	sge->len = len;
}
