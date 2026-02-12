/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_NIC_IO_H_
#define _HINIC3_NIC_IO_H_

#include "hinic3_ethdev.h"

#define HINIC3_SQ_WQEBB_SHIFT 4
#define HINIC3_RQ_WQEBB_SHIFT 3

#define HINIC3_SQ_WQEBB_SIZE  RTE_BIT32(HINIC3_SQ_WQEBB_SHIFT)
#define HINIC3_CQE_SIZE_SHIFT 4

/* Ci addr should RTE_CACHE_SIZE(64B) alignment for performance. */
#define HINIC3_CI_Q_ADDR_SIZE 64

#define CI_TABLE_SIZE(num_qps, pg_sz) \
	(RTE_ALIGN((num_qps) * HINIC3_CI_Q_ADDR_SIZE, pg_sz))

#define HINIC3_CI_VADDR(base_addr, q_id) \
	((volatile uint8_t *)(base_addr) + (q_id) * HINIC3_CI_Q_ADDR_SIZE)

#define HINIC3_CI_PADDR(base_paddr, q_id) \
	((base_paddr) + (q_id) * HINIC3_CI_Q_ADDR_SIZE)

enum hinic3_rq_wqe_type {
	HINIC3_COMPACT_RQ_WQE,
	HINIC3_NORMAL_RQ_WQE,
	HINIC3_EXTEND_RQ_WQE
};

enum hinic3_queue_type {
	HINIC3_SQ,
	HINIC3_RQ,
	HINIC3_MAX_QUEUE_TYPE,
};

/* Doorbell info. */
struct hinic3_db {
	uint32_t db_info;
	uint32_t pi_hi;
};

#define DB_INFO_QID_SHIFT	 0
#define DB_INFO_NON_FILTER_SHIFT 22
#define DB_INFO_CFLAG_SHIFT	 23
#define DB_INFO_COS_SHIFT	 24
#define DB_INFO_TYPE_SHIFT	 27

#define DB_INFO_QID_MASK	0x1FFFU
#define DB_INFO_NON_FILTER_MASK 0x1U
#define DB_INFO_CFLAG_MASK	0x1U
#define DB_INFO_COS_MASK	0x7U
#define DB_INFO_TYPE_MASK	0x1FU
#define DB_INFO_SET(val, member) \
	(((uint32_t)(val) & DB_INFO_##member##_MASK) << DB_INFO_##member##_SHIFT)

#define DB_PI_LOW_MASK	      0xFFU
#define DB_PI_HIGH_MASK	      0xFFU
#define DB_PI_LOW(pi)	      ((pi) & DB_PI_LOW_MASK)
#define DB_PI_HI_SHIFT	      8
#define DB_PI_HIGH(pi)	      (((pi) >> DB_PI_HI_SHIFT) & DB_PI_HIGH_MASK)
#define DB_INFO_UPPER_32(val) (((uint64_t)(val)) << 32)

#define DB_ADDR(db_addr, pi) ((uint64_t *)(db_addr) + DB_PI_LOW(pi))
#define SRC_TYPE	     1

/* Cflag data path. */
#define SQ_CFLAG_DP 0
#define RQ_CFLAG_DP 1

#define MASKED_QUEUE_IDX(queue, idx) ((idx) & (queue)->q_mask)

#define NIC_WQE_ADDR(queue, idx)                           \
	({                                                 \
		typeof(queue) __queue = (queue);           \
		(void *)((uint64_t)(__queue->queue_buf_vaddr) + \
			 ((idx) << __queue->wqebb_shift)); \
	})

/**
 * Write send queue doorbell.
 *
 * @param[in] db_addr
 * Doorbell address.
 * @param[in] q_id
 * Send queue id.
 * @param[in] cos
 * Send queue cos.
 * @param[in] cflag
 * Cflag data path.
 * @param[in] pi
 * Send queue pi.
 */
static inline void
hinic3_write_db(void *db_addr, uint16_t q_id, int cos, uint8_t cflag, uint16_t pi)
{
	uint64_t db = DB_PI_HIGH(pi);

	db = DB_INFO_UPPER_32(db) | DB_INFO_SET(SRC_TYPE, TYPE) |
	     DB_INFO_SET(cflag, CFLAG) | DB_INFO_SET(cos, COS) |
	     DB_INFO_SET(q_id, QID);

	rte_atomic_thread_fence(rte_memory_order_release); /**< Write all before the doorbell. */
	/* Hardware will do endianness converting. */
	rte_write64(db, DB_ADDR(db_addr, pi));
}

/**
 * Get minimum RX buffer size for device.
 *
 * @param[in] nic_dev
 * Pointer to ethernet device structure.
 */
void hinic3_get_func_rx_buf_size(struct hinic3_nic_dev *nic_dev);

/**
 * Initialize qps contexts, set SQ ci attributes, arm all SQ.
 *
 * Function will perform following steps:
 * - Initialize SQ contexts.
 * - Initialize RQ contexts.
 * - Clean QP offload contexts of SQ and RQ.
 * - Set root context for device.
 * - Configure CI tables for each SQ.
 *
 * @param[in] nic_dev
 * Pointer to ethernet device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_init_qp_ctxts(struct hinic3_nic_dev *nic_dev);

/**
 * Free queue pair context.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 */
void hinic3_free_qp_ctxts(struct hinic3_hwdev *hwdev);

/**
 * Update driver feature capabilities.
 *
 * @param[in] nic_dev
 * Pointer to ethernet device structure.
 * @param[out] s_feature
 * s_feature driver supported.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
void hinic3_update_driver_feature(struct hinic3_nic_dev *nic_dev, uint64_t s_feature);

/**
 * Get driver feature capabilities.
 *
 * @param[in] nic_dev
 * Pointer to ethernet device structure.
 *
 * @return
 * Feature capabilities of driver.
 */
uint64_t hinic3_get_driver_feature(struct hinic3_nic_dev *nic_dev);

#endif /* _HINIC3_NIC_IO_H_ */
