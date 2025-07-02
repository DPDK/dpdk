/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include "hinic3_compat.h"
#include "hinic3_hwif.h"
#include "hinic3_hwdev.h"
#include "hinic3_wq.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"
#include "hinic3_rx.h"
#include "hinic3_tx.h"
#include "hinic3_nic_io.h"
#include "hinic3_dbg.h"

#define DB_IDX(db, db_base) \
	((u32)(((ulong)(db) - (ulong)(db_base)) / HINIC3_DB_PAGE_SIZE))

int
hinic3_dbg_get_rq_info(void *hwdev, uint16_t q_id,
		       struct hinic3_dbg_rq_info *rq_info, u16 *msg_size)
{
	struct hinic3_hwdev *dev = (struct hinic3_hwdev *)hwdev;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)dev->dev_handle;
	struct hinic3_rxq *rxq = NULL;

	if (q_id >= nic_dev->num_rqs) {
		PMD_DRV_LOG(ERR, "Invalid rx queue id, q_id: %d, num_rqs: %d",
			    q_id, nic_dev->num_rqs);
		return -EINVAL;
	}

	rq_info->q_id = q_id;
	rxq = nic_dev->rxqs[q_id];

	rq_info->hw_pi = (u16)cpu_to_be16(*rxq->pi_virt_addr);
	rq_info->ci = rxq->cons_idx & rxq->q_mask;
	rq_info->sw_pi = rxq->prod_idx & rxq->q_mask;
	rq_info->wqebb_size = HINIC3_SQ_WQEBB_SIZE;
	rq_info->q_depth = rxq->q_depth;
	rq_info->buf_len = rxq->buf_len;
	rq_info->ci_wqe_page_addr = rxq->queue_buf_vaddr;
	rq_info->ci_cla_tbl_addr = NULL;
	rq_info->msix_idx = 0;
	rq_info->msix_vector = 0;

	*msg_size = sizeof(*rq_info);

	return 0;
}

int
hinic3_dbg_get_rx_cqe_info(void *hwdev, uint16_t q_id, uint16_t idx,
			   void *buf_out, uint16_t *out_size)
{
	struct hinic3_hwdev *dev = (struct hinic3_hwdev *)hwdev;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)dev->dev_handle;

	if (q_id >= nic_dev->num_rqs || idx >= nic_dev->rxqs[q_id]->q_depth)
		return -EFAULT;

	memcpy(buf_out, (void *)&nic_dev->rxqs[q_id]->rx_cqe[idx],
		     sizeof(struct hinic3_rq_cqe));
	*out_size = sizeof(struct hinic3_rq_cqe);

	return 0;
}

int
hinic3_dbg_get_sq_info(void *dev, u16 q_id, struct hinic3_dbg_sq_info *sq_info,
		       u16 *msg_size)
{
	struct hinic3_hwdev *hwdev = (struct hinic3_hwdev *)dev;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)hwdev->dev_handle;
	struct hinic3_txq *txq = NULL;

	if (q_id >= nic_dev->num_sqs) {
		PMD_DRV_LOG(ERR,
			    "Inputting tx queue id is larger than actual tx "
			    "queue number, qid: %d, num_sqs: %d",
			    q_id, nic_dev->num_sqs);
		return -EINVAL;
	}

	sq_info->q_id = q_id;
	txq = nic_dev->txqs[q_id];

	sq_info->pi = txq->prod_idx & txq->q_mask;
	sq_info->ci = txq->cons_idx & txq->q_mask;
	sq_info->fi = (*(volatile u16 *)txq->ci_vaddr_base) & txq->q_mask;
	sq_info->q_depth = txq->q_depth;
	sq_info->weqbb_size = HINIC3_SQ_WQEBB_SIZE;
	sq_info->ci_addr =
		(volatile u16 *)HINIC3_CI_VADDR(txq->ci_vaddr_base, q_id);
	sq_info->cla_addr = txq->queue_buf_paddr;
	sq_info->db_addr.phy_addr = (u64 *)txq->db_addr;
	sq_info->pg_idx = DB_IDX(txq->db_addr, hwdev->hwif->db_base);

	*msg_size = sizeof(*sq_info);

	return 0;
}

int
hinic3_dbg_get_sq_wqe_info(void *dev, u16 q_id, u16 idx, u16 wqebb_cnt, u8 *wqe,
			   u16 *wqe_size)
{
	struct hinic3_hwdev *hwdev = (struct hinic3_hwdev *)dev;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)hwdev->dev_handle;
	struct hinic3_txq *txq = NULL;
	void *src_wqe = NULL;
	u32 offset;

	if (q_id >= nic_dev->num_sqs) {
		PMD_DRV_LOG(ERR,
			    "Inputting tx queue id is larger than actual tx "
			    "queue number, qid: %d, num_sqs: %d",
			    q_id, nic_dev->num_sqs);
		return -EINVAL;
	}

	txq = nic_dev->txqs[q_id];
	if (idx + wqebb_cnt > txq->q_depth)
		return -EFAULT;

	src_wqe = (void *)txq->queue_buf_vaddr;
	offset = (u32)idx << txq->wqebb_shift;

	memcpy((void *)wqe, (void *)((u8 *)src_wqe + offset),
		     (size_t)((u32)wqebb_cnt << txq->wqebb_shift));

	*wqe_size = (u16)((u32)wqebb_cnt << txq->wqebb_shift);
	return 0;
}

int
hinic3_dbg_get_rq_wqe_info(void *dev, u16 q_id, u16 idx, u16 wqebb_cnt, u8 *wqe,
			   u16 *wqe_size)
{
	struct hinic3_hwdev *hwdev = (struct hinic3_hwdev *)dev;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)hwdev->dev_handle;
	struct hinic3_rxq *rxq = NULL;
	void *src_wqe = NULL;
	u32 offset;

	if (q_id >= nic_dev->num_rqs) {
		PMD_DRV_LOG(ERR,
			    "Inputting rx queue id is larger than actual rx "
			    "queue number, qid: %d, num_rqs: %d",
			    q_id, nic_dev->num_rqs);
		return -EINVAL;
	}

	rxq = nic_dev->rxqs[q_id];
	if (idx + wqebb_cnt > rxq->q_depth)
		return -EFAULT;

	src_wqe = (void *)rxq->queue_buf_vaddr;
	offset = (u32)idx << rxq->wqebb_shift;

	memcpy((void *)wqe, (void *)((u8 *)src_wqe + offset),
		     (size_t)((u32)wqebb_cnt << rxq->wqebb_shift));

	*wqe_size = (u16)((u32)wqebb_cnt << rxq->wqebb_shift);
	return 0;
}
