/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_HW_COMM_H_
#define _HINIC3_HW_COMM_H_

#include "hinic3_hwdev.h"
#define HINIC3_MGMT_CMD_OP_GET 0
#define HINIC3_MGMT_CMD_OP_SET 1

#define HINIC3_MSIX_CNT_LLI_TIMER_SHIFT	    0
#define HINIC3_MSIX_CNT_LLI_CREDIT_SHIFT    8
#define HINIC3_MSIX_CNT_COALESCE_TIMER_SHIFT 8
#define HINIC3_MSIX_CNT_PENDING_SHIFT	    8
#define HINIC3_MSIX_CNT_RESEND_TIMER_SHIFT  29

#define HINIC3_MSIX_CNT_LLI_TIMER_MASK	   0xFFU
#define HINIC3_MSIX_CNT_LLI_CREDIT_MASK	   0xFFU
#define HINIC3_MSIX_CNT_COALESCE_TIMER_MASK 0xFFU
#define HINIC3_MSIX_CNT_PENDING_MASK	   0x1FU
#define HINIC3_MSIX_CNT_RESEND_TIMER_MASK  0x7U

#define HINIC3_MSIX_CNT_SET(val, member)           \
	(((val) & HINIC3_MSIX_CNT_##member##_MASK) \
	 << HINIC3_MSIX_CNT_##member##_SHIFT)

#define MSG_TO_MGMT_SYNC_RETURN_ERR(err, out_size, status) \
	((err) || (status) || !(out_size))

#define DEFAULT_RX_BUF_SIZE ((uint16_t)0xB)
#define RX_BUF_SIZE_1K_LEN  ((uint16_t)0xA)

enum hinic3_rx_buf_size {
	HINIC3_RX_BUF_SIZE_32B = 0x20,
	HINIC3_RX_BUF_SIZE_64B = 0x40,
	HINIC3_RX_BUF_SIZE_96B = 0x60,
	HINIC3_RX_BUF_SIZE_128B = 0x80,
	HINIC3_RX_BUF_SIZE_192B = 0xC0,
	HINIC3_RX_BUF_SIZE_256B = 0x100,
	HINIC3_RX_BUF_SIZE_384B = 0x180,
	HINIC3_RX_BUF_SIZE_512B = 0x200,
	HINIC3_RX_BUF_SIZE_768B = 0x300,
	HINIC3_RX_BUF_SIZE_1K = 0x400,
	HINIC3_RX_BUF_SIZE_1_5K = 0x600,
	HINIC3_RX_BUF_SIZE_2K = 0x800,
	HINIC3_RX_BUF_SIZE_3K = 0xC00,
	HINIC3_RX_BUF_SIZE_4K = 0x1000,
	HINIC3_RX_BUF_SIZE_8K = 0x2000,
	HINIC3_RX_BUF_SIZE_16K = 0x4000,
};

struct hinic3_cmd_msix_config {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_id;
	uint8_t opcode;
	uint8_t rsvd1;
	uint16_t msix_index;
	uint8_t pending_cnt;
	uint8_t coalesce_timer_cnt;
	uint8_t resend_timer_cnt;
	uint8_t lli_tmier_cnt;
	uint8_t lli_credit_cnt;
	uint8_t rsvd2[5];
};

struct hinic3_dma_attr_table {
	struct mgmt_msg_head head;

	uint16_t func_id;
	uint8_t entry_idx;
	uint8_t st;
	uint8_t at;
	uint8_t ph;
	uint8_t no_snooping;
	uint8_t tph_en;
	uint32_t rsvd1;
};

#define HINIC3_PAGE_SIZE_HW(pg_size) ((uint8_t)rte_log2_u32((pg_size) >> 12))

struct hinic3_cmd_wq_page_size {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_idx;
	uint8_t opcode;
	/**
	 * Real size is 4KB * 2^page_size, range(0~20) must be checked by
	 * driver.
	 */
	uint8_t page_size;

	uint32_t rsvd1;
};

struct hinic3_reset {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_id;
	uint16_t rsvd1[3];
	uint64_t reset_flag;
};

struct comm_cmd_func_svc_used_state {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_id;
	uint16_t svc_type;
	uint8_t used_state;
	uint8_t rsvd[35];
};

struct hinic3_cmd_root_ctxt {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_idx;
	uint8_t set_cmdq_depth;
	uint8_t cmdq_depth;
	uint16_t rx_buf_sz;
	uint8_t lro_en;
	uint8_t rsvd1;
	uint16_t sq_depth;
	uint16_t rq_depth;
	uint64_t rsvd2;
};

enum hinic3_fw_ver_type {
	HINIC3_FW_VER_TYPE_BOOT,
	HINIC3_FW_VER_TYPE_MPU,
	HINIC3_FW_VER_TYPE_NPU,
	HINIC3_FW_VER_TYPE_SMU,
	HINIC3_FW_VER_TYPE_CFG,
};

#define MGMT_MSG_CMD_OP_SET 1
#define MGMT_MSG_CMD_OP_GET 0

#define COMM_MAX_FEATURE_QWORD 4
struct comm_cmd_feature_nego {
	struct mgmt_msg_head head;

	uint16_t func_id;
	uint8_t opcode; /**< 1: set, 0: get. */
	uint8_t rsvd;
	uint64_t s_feature[COMM_MAX_FEATURE_QWORD];
};

#define HINIC3_FW_VERSION_LEN	    16
#define HINIC3_FW_COMPILE_TIME_LEN  20
#define HINIC3_MGMT_VERSION_MAX_LEN 32
struct hinic3_cmd_get_fw_version {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t fw_type;
	uint16_t rsvd1;
	uint8_t ver[HINIC3_FW_VERSION_LEN];
	uint8_t time[HINIC3_FW_COMPILE_TIME_LEN];
};

struct hinic3_cmd_clear_doorbell {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_idx;
	uint16_t rsvd1[3];
};

struct hinic3_cmd_clear_resource {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_idx;
	uint16_t rsvd1[3];
};

struct hinic3_cmd_board_info {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	struct hinic3_board_info info;

	uint32_t rsvd1[23];
};

struct interrupt_info {
	uint32_t lli_set;
	uint32_t interrupt_coalesce_set;
	uint16_t msix_index;
	uint8_t lli_credit_limit;
	uint8_t lli_timer_cfg;
	uint8_t pending_limt;
	uint8_t coalesce_timer_cfg;
	uint8_t resend_timer_cfg;
};

enum cfg_msix_operation {
	CFG_MSIX_OPERATION_FREE = 0,
	CFG_MSIX_OPERATION_ALLOC = 1,
};

struct comm_cmd_cfg_msix_num {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_id;
	uint8_t op_code; /**< 1: alloc, 0: free. */
	uint8_t rsvd1;

	uint16_t msix_num;
	uint16_t rsvd2;
};

int hinic3_get_interrupt_cfg(struct hinic3_hwdev *hwdev, struct interrupt_info *info);

/**
 * Set interrupt cfg.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] info
 * Interrupt info.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_interrupt_cfg(struct hinic3_hwdev *hwdev, struct interrupt_info info);

int hinic3_set_wq_page_size(struct hinic3_hwdev *hwdev, uint16_t func_idx, uint32_t page_size);

/**
 * Send a reset command to hardware.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] reset_flag
 * The flag that specifies the reset behavior.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_func_reset(struct hinic3_hwdev *hwdev, uint64_t reset_flag);

/**
 * Send a command to management module to set usage state of a specific service
 * for given function.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] svc_type
 * The service type to update.
 * @param[in] state
 * The state to set for the service.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_func_svc_used_state(struct hinic3_hwdev *hwdev, uint16_t svc_type, uint8_t state);

/**
 * Adjust the requested RX buffer size to the closest valid size supported by
 * the hardware.
 *
 * @param[in] rx_buf_sz
 * The requested RX buffer size.
 * @param[out] match_sz
 * The closest valid RX buffer size.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_convert_rx_buf_size(uint32_t rx_buf_sz, uint32_t *match_sz);

/**
 * Send a command to apply the settings.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] rq_depth
 * The depth of the receive queue.
 * @param[in] sq_depth
 * The depth of the send queue.
 * @param[in] rx_buf_sz
 * The RX buffer size to set.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_root_ctxt(struct hinic3_hwdev *hwdev, uint32_t rq_depth, uint32_t sq_depth,
			 uint16_t rx_buf_sz);

/**
 * Send a command to clear any previously set context.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_clean_root_ctxt(struct hinic3_hwdev *hwdev);

/**
 * Send a command to set command queue depth.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] cmdq_depth
 * The desired depth of the command queue, converted to logarithmic value
 * before being set.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_cmdq_depth(struct hinic3_hwdev *hwdev, uint16_t cmdq_depth);

/**
 * Send a command to get firmware version.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[out] mgmt_ver
 * The buffer to store the retrieved management firmware version.
 * @param[in] max_mgmt_len
 * The maximum length of the `mgmt_ver` buffer.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_mgmt_version(struct hinic3_hwdev *hwdev, char *mgmt_ver, int max_mgmt_len);

/**
 * Send a command to get board information.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[out] info
 * The structure to store the retrieved board information.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_board_info(struct hinic3_hwdev *hwdev, struct hinic3_board_info *info);

int hinic3_get_comm_features(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size);

int hinic3_set_comm_features(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size);

#endif /* _HINIC3_HW_COMM_H_ */
