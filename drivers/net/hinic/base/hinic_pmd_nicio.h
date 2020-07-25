/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_NICIO_H_
#define _HINIC_PMD_NICIO_H_

#define RX_BUF_LEN_16K			16384
#define RX_BUF_LEN_1_5K			1536

/* vhd type */
#define HINIC_VHD_TYPE_0B		2
#define HINIC_VHD_TYPE_10B		1
#define HINIC_VHD_TYPE_12B		0

#define HINIC_Q_CTXT_MAX		42

/* performance: ci addr RTE_CACHE_SIZE(64B) alignment */
#define HINIC_CI_Q_ADDR_SIZE		64

#define CI_TABLE_SIZE(num_qps, pg_sz)	\
	(ALIGN((num_qps) * HINIC_CI_Q_ADDR_SIZE, pg_sz))

#define HINIC_CI_VADDR(base_addr, q_id)		\
	((u8 *)(base_addr) + (q_id) * HINIC_CI_Q_ADDR_SIZE)

#define HINIC_CI_PADDR(base_paddr, q_id)	\
	((base_paddr) + (q_id) * HINIC_CI_Q_ADDR_SIZE)

#define Q_CTXT_SIZE				48
#define TSO_LRO_CTXT_SIZE			240

#define SQ_CTXT_OFFSET(max_sqs, max_rqs, q_id)	\
	(((max_rqs) + (max_sqs)) * TSO_LRO_CTXT_SIZE +	\
		(q_id) * Q_CTXT_SIZE)

#define RQ_CTXT_OFFSET(max_sqs, max_rqs, q_id)	\
	(((max_rqs) + (max_sqs)) * TSO_LRO_CTXT_SIZE +	\
		(max_sqs) * Q_CTXT_SIZE + (q_id) * Q_CTXT_SIZE)

#define SQ_CTXT_SIZE(num_sqs)		\
	((u16)(sizeof(struct hinic_qp_ctxt_header) +	\
		(num_sqs) * sizeof(struct hinic_sq_ctxt)))

#define RQ_CTXT_SIZE(num_rqs)		\
	((u16)(sizeof(struct hinic_qp_ctxt_header) +	\
		(num_rqs) * sizeof(struct hinic_rq_ctxt)))

#define SQ_CTXT_CEQ_ATTR_CEQ_ID_SHIFT			8
#define SQ_CTXT_CEQ_ATTR_GLOBAL_SQ_ID_SHIFT		13
#define SQ_CTXT_CEQ_ATTR_EN_SHIFT			23
#define SQ_CTXT_CEQ_ATTR_ARM_SHIFT			31

#define SQ_CTXT_CEQ_ATTR_CEQ_ID_MASK			0x1FU
#define SQ_CTXT_CEQ_ATTR_GLOBAL_SQ_ID_MASK		0x3FFU
#define SQ_CTXT_CEQ_ATTR_EN_MASK			0x1U
#define SQ_CTXT_CEQ_ATTR_ARM_MASK			0x1U

#define SQ_CTXT_CEQ_ATTR_SET(val, member)	\
	(((val) & SQ_CTXT_CEQ_ATTR_##member##_MASK) <<	\
		SQ_CTXT_CEQ_ATTR_##member##_SHIFT)

#define SQ_CTXT_CI_IDX_SHIFT				11
#define SQ_CTXT_CI_OWNER_SHIFT				23

#define SQ_CTXT_CI_IDX_MASK				0xFFFU
#define SQ_CTXT_CI_OWNER_MASK				0x1U

#define SQ_CTXT_CI_SET(val, member)		\
	(((val) & SQ_CTXT_CI_##member##_MASK) << SQ_CTXT_CI_##member##_SHIFT)

#define SQ_CTXT_WQ_PAGE_HI_PFN_SHIFT			0
#define SQ_CTXT_WQ_PAGE_PI_SHIFT			20

#define SQ_CTXT_WQ_PAGE_HI_PFN_MASK			0xFFFFFU
#define SQ_CTXT_WQ_PAGE_PI_MASK				0xFFFU

#define SQ_CTXT_WQ_PAGE_SET(val, member)	\
	(((val) & SQ_CTXT_WQ_PAGE_##member##_MASK) <<	\
		SQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define SQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT		0
#define SQ_CTXT_PREF_CACHE_MAX_SHIFT			14
#define SQ_CTXT_PREF_CACHE_MIN_SHIFT			25

#define SQ_CTXT_PREF_CACHE_THRESHOLD_MASK		0x3FFFU
#define SQ_CTXT_PREF_CACHE_MAX_MASK			0x7FFU
#define SQ_CTXT_PREF_CACHE_MIN_MASK			0x7FU

#define SQ_CTXT_PREF_WQ_PFN_HI_SHIFT			0
#define SQ_CTXT_PREF_CI_SHIFT				20

#define SQ_CTXT_PREF_WQ_PFN_HI_MASK			0xFFFFFU
#define SQ_CTXT_PREF_CI_MASK				0xFFFU

#define SQ_CTXT_PREF_SET(val, member)		\
	(((val) & SQ_CTXT_PREF_##member##_MASK) <<	\
		SQ_CTXT_PREF_##member##_SHIFT)

#define SQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT			0

#define SQ_CTXT_WQ_BLOCK_PFN_HI_MASK			0x7FFFFFU

#define SQ_CTXT_WQ_BLOCK_SET(val, member)	\
	(((val) & SQ_CTXT_WQ_BLOCK_##member##_MASK) <<	\
		SQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define RQ_CTXT_CEQ_ATTR_EN_SHIFT			0
#define RQ_CTXT_CEQ_ATTR_OWNER_SHIFT			1

#define RQ_CTXT_CEQ_ATTR_EN_MASK			0x1U
#define RQ_CTXT_CEQ_ATTR_OWNER_MASK			0x1U

#define RQ_CTXT_CEQ_ATTR_SET(val, member)	\
	(((val) & RQ_CTXT_CEQ_ATTR_##member##_MASK) <<	\
		RQ_CTXT_CEQ_ATTR_##member##_SHIFT)

#define RQ_CTXT_PI_IDX_SHIFT				0
#define RQ_CTXT_PI_INTR_SHIFT				22
#define RQ_CTXT_PI_CEQ_ARM_SHIFT			31

#define RQ_CTXT_PI_IDX_MASK				0xFFFU
#define RQ_CTXT_PI_INTR_MASK				0x3FFU
#define RQ_CTXT_PI_CEQ_ARM_MASK				0x1U

#define RQ_CTXT_PI_SET(val, member)		\
	(((val) & RQ_CTXT_PI_##member##_MASK) << RQ_CTXT_PI_##member##_SHIFT)

#define RQ_CTXT_WQ_PAGE_HI_PFN_SHIFT			0
#define RQ_CTXT_WQ_PAGE_CI_SHIFT			20

#define RQ_CTXT_WQ_PAGE_HI_PFN_MASK			0xFFFFFU
#define RQ_CTXT_WQ_PAGE_CI_MASK				0xFFFU

#define RQ_CTXT_WQ_PAGE_SET(val, member)	\
	(((val) & RQ_CTXT_WQ_PAGE_##member##_MASK) << \
		RQ_CTXT_WQ_PAGE_##member##_SHIFT)

#define RQ_CTXT_PREF_CACHE_THRESHOLD_SHIFT		0
#define RQ_CTXT_PREF_CACHE_MAX_SHIFT			14
#define RQ_CTXT_PREF_CACHE_MIN_SHIFT			25

#define RQ_CTXT_PREF_CACHE_THRESHOLD_MASK		0x3FFFU
#define RQ_CTXT_PREF_CACHE_MAX_MASK			0x7FFU
#define RQ_CTXT_PREF_CACHE_MIN_MASK			0x7FU

#define RQ_CTXT_PREF_WQ_PFN_HI_SHIFT			0
#define RQ_CTXT_PREF_CI_SHIFT				20

#define RQ_CTXT_PREF_WQ_PFN_HI_MASK			0xFFFFFU
#define RQ_CTXT_PREF_CI_MASK				0xFFFU

#define RQ_CTXT_PREF_SET(val, member)		\
	(((val) & RQ_CTXT_PREF_##member##_MASK) <<	\
		RQ_CTXT_PREF_##member##_SHIFT)

#define RQ_CTXT_WQ_BLOCK_PFN_HI_SHIFT			0

#define RQ_CTXT_WQ_BLOCK_PFN_HI_MASK			0x7FFFFFU

#define RQ_CTXT_WQ_BLOCK_SET(val, member)	\
	(((val) & RQ_CTXT_WQ_BLOCK_##member##_MASK) <<	\
		RQ_CTXT_WQ_BLOCK_##member##_SHIFT)

#define SIZE_16BYTES(size)		(ALIGN((size), 16) >> 4)

enum hinic_qp_ctxt_type {
	HINIC_QP_CTXT_TYPE_SQ,
	HINIC_QP_CTXT_TYPE_RQ,
};

struct hinic_sq {
	struct hinic_wq		*wq;
	volatile u16		*cons_idx_addr;
	void __iomem		*db_addr;

	u16	q_id;
	u16	owner;
	u16	sq_depth;
};

struct hinic_rq {
	struct hinic_wq		*wq;
	volatile u16		*pi_virt_addr;
	dma_addr_t		pi_dma_addr;

	u16			irq_id;
	u16			msix_entry_idx;
	u16			q_id;
	u16			rq_depth;
};

struct hinic_qp {
	struct hinic_sq		sq;
	struct hinic_rq		rq;
};

struct hinic_event {
	void (*tx_ack)(void *handle, u16 q_id);
	/* status: 0 - link down; 1 - link up */
	void (*link_change)(void *handle, int status);
};

struct hinic_nic_io {
	struct hinic_hwdev	*hwdev;

	u16			global_qpn;

	struct hinic_wq		*sq_wq;
	struct hinic_wq		*rq_wq;

	u16			max_qps;
	u16			num_qps;

	u16			num_sqs;
	u16			num_rqs;

	u16			sq_depth;
	u16			rq_depth;

	u16			rq_buf_size;
	u16			vhd_mode;

	struct hinic_qp		*qps;
	/* sq ci mem base addr of the function */
	void			*ci_vaddr_base;
	dma_addr_t		ci_dma_base;

	struct hinic_event	event;
	void			*event_handle;
};

struct hinic_sq_db {
	u32	db_info;
};

int hinic_init_qp_ctxts(struct hinic_hwdev *hwdev);

void hinic_free_qp_ctxts(struct hinic_hwdev *hwdev);

int hinic_rx_tx_flush(struct hinic_hwdev *hwdev);

int hinic_get_sq_free_wqebbs(struct hinic_hwdev *hwdev, u16 q_id);

u16 hinic_get_sq_local_ci(struct hinic_hwdev *hwdev, u16 q_id);

void hinic_update_sq_local_ci(struct hinic_hwdev *hwdev, u16 q_id,
			      int wqebb_cnt);

void hinic_return_sq_wqe(struct hinic_hwdev *hwdev, u16 q_id,
			 int num_wqebbs, u16 owner);

int hinic_get_rq_free_wqebbs(struct hinic_hwdev *hwdev, u16 q_id);

void *hinic_get_rq_wqe(struct hinic_hwdev *hwdev, u16 q_id, u16 *pi);

void hinic_return_rq_wqe(struct hinic_hwdev *hwdev, u16 q_id, int num_wqebbs);

u16 hinic_get_rq_local_ci(struct hinic_hwdev *hwdev, u16 q_id);

void hinic_update_rq_local_ci(struct hinic_hwdev *hwdev, u16 q_id, int wqe_cnt);

int hinic_init_nicio(struct hinic_hwdev *hwdev);

void hinic_deinit_nicio(struct hinic_hwdev *hwdev);

int hinic_convert_rx_buf_size(u32 rx_buf_sz, u32 *match_sz);

#endif /* _HINIC_PMD_NICIO_H_ */
