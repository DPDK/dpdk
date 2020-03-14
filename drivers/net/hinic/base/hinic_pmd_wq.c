/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_wq.h"

static void free_wq_pages(struct hinic_hwdev *hwdev, struct hinic_wq *wq)
{
	dma_free_coherent(hwdev, wq->wq_buf_size, (void *)wq->queue_buf_vaddr,
			(dma_addr_t)wq->queue_buf_paddr);

	wq->queue_buf_paddr = 0;
	wq->queue_buf_vaddr = 0;
}

static int alloc_wq_pages(struct hinic_hwdev *hwdev, struct hinic_wq *wq,
			unsigned int socket_id)
{
	dma_addr_t dma_addr = 0;

	wq->queue_buf_vaddr = (u64)(u64 *)
		dma_zalloc_coherent_aligned256k(hwdev, wq->wq_buf_size,
						&dma_addr, socket_id);
	if (!wq->queue_buf_vaddr) {
		PMD_DRV_LOG(ERR, "Failed to allocate wq page");
		return -ENOMEM;
	}

	if (!ADDR_256K_ALIGNED(dma_addr)) {
		PMD_DRV_LOG(ERR, "Wqe pages is not 256k aligned!");
		dma_free_coherent(hwdev, wq->wq_buf_size,
				  (void *)wq->queue_buf_vaddr,
				  dma_addr);
		return -ENOMEM;
	}
	wq->queue_buf_paddr = dma_addr;

	return 0;
}

int hinic_wq_allocate(struct hinic_hwdev *hwdev, struct hinic_wq *wq,
		      u32 wqebb_shift, u16 q_depth, unsigned int socket_id)
{
	int err;

	if (q_depth & (q_depth - 1)) {
		PMD_DRV_LOG(ERR, "WQ q_depth isn't power of 2");
		return -EINVAL;
	}

	wq->wqebb_size = 1 << wqebb_shift;
	wq->wqebb_shift = wqebb_shift;
	wq->wq_buf_size = ((u32)q_depth) << wqebb_shift;
	wq->q_depth = q_depth;

	if (wq->wq_buf_size > (HINIC_PAGE_SIZE << HINIC_PAGE_SIZE_DPDK)) {
		PMD_DRV_LOG(ERR, "Invalid q_depth %u which one page_size can not hold",
			q_depth);
		return -EINVAL;
	}

	err = alloc_wq_pages(hwdev, wq, socket_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate wq pages");
		return err;
	}

	wq->cons_idx = 0;
	wq->prod_idx = 0;
	wq->delta = q_depth;
	wq->mask = q_depth - 1;

	return 0;
}

void hinic_wq_free(struct hinic_hwdev *hwdev, struct hinic_wq *wq)
{
	free_wq_pages(hwdev, wq);
}

void hinic_put_wqe(struct hinic_wq *wq, int num_wqebbs)
{
	wq->cons_idx += num_wqebbs;
	wq->delta += num_wqebbs;
}

void *hinic_read_wqe(struct hinic_wq *wq, int num_wqebbs, u16 *cons_idx)
{
	u16 curr_cons_idx;

	if ((wq->delta + num_wqebbs) > wq->q_depth)
		return NULL;

	curr_cons_idx = (u16)(wq->cons_idx);

	curr_cons_idx = MASKED_WQE_IDX(wq, curr_cons_idx);

	*cons_idx = curr_cons_idx;

	return WQ_WQE_ADDR(wq, (u32)(*cons_idx));
}

int hinic_cmdq_alloc(struct hinic_wq *wq, struct hinic_hwdev *hwdev,
		     int cmdq_blocks, u32 wq_buf_size, u32 wqebb_shift,
		     u16 q_depth)
{
	int i, j, err = -ENOMEM;

	/* validate q_depth is power of 2 & wqebb_size is not 0 */
	for (i = 0; i < cmdq_blocks; i++) {
		wq[i].wqebb_size = 1 << wqebb_shift;
		wq[i].wqebb_shift = wqebb_shift;
		wq[i].wq_buf_size = wq_buf_size;
		wq[i].q_depth = q_depth;

		err = alloc_wq_pages(hwdev, &wq[i], SOCKET_ID_ANY);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to alloc CMDQ blocks");
			goto cmdq_block_err;
		}

		wq[i].cons_idx = 0;
		wq[i].prod_idx = 0;
		wq[i].delta = q_depth;

		wq[i].mask = q_depth - 1;
	}

	return 0;

cmdq_block_err:
	for (j = 0; j < i; j++)
		free_wq_pages(hwdev, &wq[j]);

	return err;
}

void hinic_cmdq_free(struct hinic_hwdev *hwdev, struct hinic_wq *wq,
		     int cmdq_blocks)
{
	int i;

	for (i = 0; i < cmdq_blocks; i++)
		free_wq_pages(hwdev, &wq[i]);
}

void hinic_wq_wqe_pg_clear(struct hinic_wq *wq)
{
	wq->cons_idx = 0;
	wq->prod_idx = 0;

	memset((void *)wq->queue_buf_vaddr, 0, wq->wq_buf_size);
}

void *hinic_get_wqe(struct hinic_wq *wq, int num_wqebbs, u16 *prod_idx)
{
	u16 curr_prod_idx;

	wq->delta -= num_wqebbs;
	curr_prod_idx = wq->prod_idx;
	wq->prod_idx += num_wqebbs;
	*prod_idx = MASKED_WQE_IDX(wq, curr_prod_idx);

	return WQ_WQE_ADDR(wq, (u32)(*prod_idx));
}

/**
 * hinic_set_sge - set dma area in scatter gather entry
 * @sge: scatter gather entry
 * @addr: dma address
 * @len: length of relevant data in the dma address
 **/
void hinic_set_sge(struct hinic_sge *sge, dma_addr_t addr, u32 len)
{
	sge->hi_addr = upper_32_bits(addr);
	sge->lo_addr = lower_32_bits(addr);
	sge->len  = len;
}
