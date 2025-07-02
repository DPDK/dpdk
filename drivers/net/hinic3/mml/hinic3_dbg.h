/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef _HINIC3_MML_DBG_H
#define _HINIC3_MML_DBG_H

/* nic_tool */
struct hinic3_tx_hw_page {
	u64 *phy_addr;
	u64 *map_addr;
};

/* nic_tool */
struct hinic3_dbg_sq_info {
	u16 q_id;
	u16 pi;
	u16 ci; /**< sw_ci */
	u16 fi; /**< hw_ci */

	u32 q_depth;
	u16 weqbb_size;

	volatile u16 *ci_addr;
	u64 cla_addr;

	struct hinic3_tx_hw_page db_addr;
	u32 pg_idx;
};

/* nic_tool */
struct hinic3_dbg_rq_info {
	u16 q_id;
	u16 hw_pi;
	u16 ci; /**< sw_ci */
	u16 sw_pi;
	u16 wqebb_size;
	u16 q_depth;
	u16 buf_len;

	void *ci_wqe_page_addr;
	void *ci_cla_tbl_addr;
	u16 msix_idx;
	u32 msix_vector;
};

void *hinic3_dbg_get_sq_wq_handle(void *hwdev, u16 q_id);

void *hinic3_dbg_get_rq_wq_handle(void *hwdev, u16 q_id);

void *hinic3_dbg_get_sq_ci_addr(void *hwdev, u16 q_id);

u16 hinic3_dbg_get_global_qpn(void *hwdev);

/**
 * Get details of specified RX queue and store in `rq_info`.
 *
 * @param[in] hwdev
 * Pointer to the hardware device.
 * @param[in] q_id
 * RX queue ID.
 * @param[out] rq_info
 * Structure to store RX queue information.
 * @param[out] msg_size
 * Size of the message.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_dbg_get_rq_info(void *hwdev, uint16_t q_id,
			   struct hinic3_dbg_rq_info *rq_info, u16 *msg_size);

/**
 * Get the RX CQE at the specified index from the given RX queue.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] q_id
 * RX queue ID.
 * @param[in] idx
 * Index of the CQE.
 * @param[out] buf_out
 * Buffer to store the CQE.
 * @param[out] out_size
 * Size of the CQE.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_dbg_get_rx_cqe_info(void *hwdev, uint16_t q_id, uint16_t idx,
			       void *buf_out, uint16_t *out_size);

/**
 * Get SQ information for debugging.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] q_id
 * ID of SQ to retrieve information for.
 * @param[out] sq_info
 * Pointer to the structure where the SQ information will be stored.
 * @param[out] msg_size
 * The size (in bytes) of the `sq_info` structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EINVAL if the queue ID is invalid.
 */
int hinic3_dbg_get_sq_info(void *hwdev, u16 q_id,
			   struct hinic3_dbg_sq_info *sq_info, u16 *msg_size);

/**
 * Get WQE information from a send queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] q_id
 * The ID of the send queue from which to retrieve WQE information.
 * @param[in] idx
 * The index of the first WQE to retrieve.
 * @param[in] wqebb_cnt
 * The number of WQEBBs to retrieve.
 * @param[out] wqe
 * Pointer to the buffer where the WQE data will be stored.
 * @param[out] wqe_size
 * The size (in bytes) of the retrieved WQE data.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EINVAL if queue ID invalid.
 * - -EFAULT if index invalid.
 */
int hinic3_dbg_get_sq_wqe_info(void *hwdev, u16 q_id, u16 idx, u16 wqebb_cnt,
			       u8 *wqe, u16 *wqe_size);

/**
 * Get WQE information from a receive queue.
 *
 * @param[in] dev
 * Pointer to the device structure.
 * @param[in] q_id
 * The ID of the receive queue from which to retrieve WQE information.
 * @param[in] idx
 * The index of the first WQE to retrieve.
 * @param[in] wqebb_cnt
 * The number of WQEBBs to retrieve.
 * @param[out] wqe
 * Pointer to the buffer where the WQE data will be stored.
 * @param[out] wqe_size
 * The size (in bytes) of the retrieved WQE data.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EINVAL if queue ID invalid.
 * - -EFAULT if index invalid.
 */
int hinic3_dbg_get_rq_wqe_info(void *hwdev, u16 q_id, u16 idx, u16 wqebb_cnt,
			       u8 *wqe, u16 *wqe_size);

#endif /* _HINIC3_MML_DBG_H */
