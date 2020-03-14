/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_WQ_H_
#define _HINIC_PMD_WQ_H_

#define WQS_BLOCKS_PER_PAGE		4

#define WQ_SIZE(wq)		(u32)((u64)(wq)->q_depth * (wq)->wqebb_size)

#define	WQE_PAGE_NUM(wq, idx)	(((idx) >> ((wq)->wqebbs_per_page_shift)) & \
				((wq)->num_q_pages - 1))

#define	WQE_PAGE_OFF(wq, idx)	((u64)((wq)->wqebb_size) * \
				((idx) & ((wq)->num_wqebbs_per_page - 1)))

#define WQ_PAGE_ADDR_SIZE		sizeof(u64)
#define WQ_PAGE_ADDR_SIZE_SHIFT		3
#define WQ_PAGE_ADDR(wq, idx)		\
		(u8 *)(*(u64 *)((u64)((wq)->shadow_block_vaddr) + \
		(WQE_PAGE_NUM(wq, idx) << WQ_PAGE_ADDR_SIZE_SHIFT)))

#define WQ_BLOCK_SIZE		4096UL
#define WQS_PAGE_SIZE		(WQS_BLOCKS_PER_PAGE * WQ_BLOCK_SIZE)
#define WQ_MAX_PAGES		(WQ_BLOCK_SIZE >> WQ_PAGE_ADDR_SIZE_SHIFT)

#define CMDQ_BLOCKS_PER_PAGE		8
#define CMDQ_BLOCK_SIZE			512UL
#define CMDQ_PAGE_SIZE			ALIGN((CMDQ_BLOCKS_PER_PAGE * \
						CMDQ_BLOCK_SIZE), PAGE_SIZE)

#define ADDR_4K_ALIGNED(addr)		(0 == ((addr) & 0xfff))
#define ADDR_256K_ALIGNED(addr)		(0 == ((addr) & 0x3ffff))

#define WQ_BASE_VADDR(wqs, wq)		\
		(u64 *)(((u64)((wqs)->page_vaddr[(wq)->page_idx])) \
				+ (wq)->block_idx * WQ_BLOCK_SIZE)

#define WQ_BASE_PADDR(wqs, wq)	(((wqs)->page_paddr[(wq)->page_idx]) \
				+ (u64)(wq)->block_idx * WQ_BLOCK_SIZE)

#define WQ_BASE_ADDR(wqs, wq)		\
		(u64 *)(((u64)((wqs)->shadow_page_vaddr[(wq)->page_idx])) \
				+ (wq)->block_idx * WQ_BLOCK_SIZE)

#define CMDQ_BASE_VADDR(cmdq_pages, wq)	\
			(u64 *)(((u64)((cmdq_pages)->cmdq_page_vaddr)) \
				+ (wq)->block_idx * CMDQ_BLOCK_SIZE)

#define CMDQ_BASE_PADDR(cmdq_pages, wq)	\
			(((u64)((cmdq_pages)->cmdq_page_paddr)) \
				+ (u64)(wq)->block_idx * CMDQ_BLOCK_SIZE)

#define CMDQ_BASE_ADDR(cmdq_pages, wq)	\
			(u64 *)(((u64)((cmdq_pages)->cmdq_shadow_page_vaddr)) \
				+ (wq)->block_idx * CMDQ_BLOCK_SIZE)

#define MASKED_WQE_IDX(wq, idx)	((idx) & (wq)->mask)

#define WQE_SHADOW_PAGE(wq, wqe)	\
		(u16)(((unsigned long)(wqe) - (unsigned long)(wq)->shadow_wqe) \
		/ (wq)->max_wqe_size)

#define WQE_IN_RANGE(wqe, start, end)	\
		(((unsigned long)(wqe) >= (unsigned long)(start)) && \
		((unsigned long)(wqe) < (unsigned long)(end)))

#define WQ_NUM_PAGES(num_wqs)	\
	(ALIGN((u32)num_wqs, WQS_BLOCKS_PER_PAGE) / WQS_BLOCKS_PER_PAGE)

#define	WQ_WQE_ADDR(wq, idx) ((void *)((u64)((wq)->queue_buf_vaddr) + \
			      ((idx) << (wq)->wqebb_shift)))

#define	WQ_PAGE_PFN_SHIFT			12
#define	WQ_BLOCK_PFN_SHIFT			9

#define WQ_PAGE_PFN(page_addr)		((page_addr) >> WQ_PAGE_PFN_SHIFT)
#define WQ_BLOCK_PFN(page_addr)		((page_addr) >> WQ_BLOCK_PFN_SHIFT)


#define HINIC_SQ_WQEBB_SIZE	64
#define HINIC_RQ_WQE_SIZE	32
#define HINIC_SQ_WQEBB_SHIFT	6
#define HINIC_RQ_WQEBB_SHIFT	5

struct hinic_sge {
	u32		hi_addr;
	u32		lo_addr;
	u32		len;
};

/* Working Queue */
struct hinic_wq {
	/* The addresses are 64 bit in the HW */
	u64     queue_buf_vaddr;

	u16		q_depth;
	u16		mask;
	u32		delta;

	u32		cons_idx;
	u32		prod_idx;

	u64     queue_buf_paddr;

	u32		wqebb_size;
	u32		wqebb_shift;

	u32		wq_buf_size;

	u32		rsvd[5];
};

void hinic_wq_wqe_pg_clear(struct hinic_wq *wq);

int hinic_cmdq_alloc(struct hinic_wq *wq, struct hinic_hwdev *hwdev,
		     int cmdq_blocks, u32 wq_buf_size, u32 wqebb_shift,
		     u16 q_depth);

void hinic_cmdq_free(struct hinic_hwdev *hwdev, struct hinic_wq *wq,
		     int cmdq_blocks);

int hinic_wq_allocate(struct hinic_hwdev *hwdev, struct hinic_wq *wq,
		      u32 wqebb_shift, u16 q_depth, unsigned int socket_id);

void hinic_wq_free(struct hinic_hwdev *hwdev, struct hinic_wq *wq);

void *hinic_get_wqe(struct hinic_wq *wq, int num_wqebbs, u16 *prod_idx);

void hinic_put_wqe(struct hinic_wq *wq, int num_wqebbs);

void *hinic_read_wqe(struct hinic_wq *wq, int num_wqebbs, u16 *cons_idx);

void hinic_set_sge(struct hinic_sge *sge, dma_addr_t addr, u32 len);

#endif /* _HINIC_PMD_WQ_H_ */
