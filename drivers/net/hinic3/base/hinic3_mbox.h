/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_MBOX_H_
#define _HINIC3_MBOX_H_

#include "hinic3_mgmt.h"
#include "hinic3_hwdev.h"
#define HINIC3_MBOX_PF_SEND_ERR	      0x1
#define HINIC3_MBOX_PF_BUSY_ACTIVE_FW 0x2
#define HINIC3_MBOX_VF_CMD_ERROR      0x3

#define HINIC3_MGMT_SRC_ID 0x1FFF

#define HINIC3_MAX_PF_FUNCS 32

/* Message header define. */
#define HINIC3_MSG_HEADER_SRC_GLB_FUNC_IDX_SHIFT 0
#define HINIC3_MSG_HEADER_STATUS_SHIFT		 13
#define HINIC3_MSG_HEADER_SOURCE_SHIFT		 15
#define HINIC3_MSG_HEADER_AEQ_ID_SHIFT		 16
#define HINIC3_MSG_HEADER_MSG_ID_SHIFT		 18
#define HINIC3_MSG_HEADER_CMD_SHIFT		 22

#define HINIC3_MSG_HEADER_MSG_LEN_SHIFT	  32
#define HINIC3_MSG_HEADER_MODULE_SHIFT	  43
#define HINIC3_MSG_HEADER_SEG_LEN_SHIFT	  48
#define HINIC3_MSG_HEADER_NO_ACK_SHIFT	  54
#define HINIC3_MSG_HEADER_DATA_TYPE_SHIFT 55
#define HINIC3_MSG_HEADER_SEQID_SHIFT	  56
#define HINIC3_MSG_HEADER_LAST_SHIFT	  62
#define HINIC3_MSG_HEADER_DIRECTION_SHIFT 63

#define HINIC3_MSG_HEADER_CMD_MASK		0x3FF
#define HINIC3_MSG_HEADER_MSG_ID_MASK		0xF
#define HINIC3_MSG_HEADER_AEQ_ID_MASK		0x3
#define HINIC3_MSG_HEADER_SOURCE_MASK		0x1
#define HINIC3_MSG_HEADER_STATUS_MASK		0x1
#define HINIC3_MSG_HEADER_SRC_GLB_FUNC_IDX_MASK 0x1FFF

#define HINIC3_MSG_HEADER_MSG_LEN_MASK	 0x7FF
#define HINIC3_MSG_HEADER_MODULE_MASK	 0x1F
#define HINIC3_MSG_HEADER_SEG_LEN_MASK	 0x3F
#define HINIC3_MSG_HEADER_NO_ACK_MASK	 0x1
#define HINIC3_MSG_HEADER_DATA_TYPE_MASK 0x1
#define HINIC3_MSG_HEADER_SEQID_MASK	 0x3F
#define HINIC3_MSG_HEADER_LAST_MASK	 0x1
#define HINIC3_MSG_HEADER_DIRECTION_MASK 0x1

#define HINIC3_MSG_HEADER_GET(val, field)               \
	(((val) >> HINIC3_MSG_HEADER_##field##_SHIFT) & \
	 HINIC3_MSG_HEADER_##field##_MASK)
#define HINIC3_MSG_HEADER_SET(val, field)                       \
	((((uint64_t)(val)) & HINIC3_MSG_HEADER_##field##_MASK) \
	 << HINIC3_MSG_HEADER_##field##_SHIFT)

#define IS_TLP_MBX(dst_func) ((dst_func) == HINIC3_MGMT_SRC_ID)

enum hinic3_msg_direction_type {
	HINIC3_MSG_DIRECT_SEND = 0,
	HINIC3_MSG_RESPONSE = 1
};

enum hinic3_msg_segment_type { NOT_LAST_SEGMENT = 0, LAST_SEGMENT = 1 };

enum hinic3_msg_ack_type { HINIC3_MSG_ACK = 0, HINIC3_MSG_NO_ACK = 1 };

enum hinic3_data_type { HINIC3_DATA_INLINE = 0, HINIC3_DATA_DMA = 1 };

enum hinic3_msg_src_type { HINIC3_MSG_FROM_MGMT = 0, HINIC3_MSG_FROM_MBOX = 1 };

enum hinic3_msg_aeq_type {
	HINIC3_ASYNC_MSG_AEQ = 0,
	/* Indicate dst_func or mgmt cpu which aeq to response mbox message. */
	HINIC3_MBOX_RSP_MSG_AEQ = 1,
	/* Indicate mgmt cpu which aeq to response api cmd message. */
	HINIC3_MGMT_RSP_MSG_AEQ = 2
};

enum hinic3_mbox_seg_errcode {
	MBOX_ERRCODE_NO_ERRORS = 0,
	/* VF sends the mailbox data to the wrong destination functions. */
	MBOX_ERRCODE_VF_TO_WRONG_FUNC = 0x100,
	/* PPF sends the mailbox data to the wrong destination functions. */
	MBOX_ERRCODE_PPF_TO_WRONG_FUNC = 0x200,
	/* PF sends the mailbox data to the wrong destination functions. */
	MBOX_ERRCODE_PF_TO_WRONG_FUNC = 0x300,
	/* The mailbox data size is set to all zero. */
	MBOX_ERRCODE_ZERO_DATA_SIZE = 0x400,
	/* The sender func attribute has not been learned by CPI hardware. */
	MBOX_ERRCODE_UNKNOWN_SRC_FUNC = 0x500,
	/* The receiver func attr has not been learned by CPI hardware. */
	MBOX_ERRCODE_UNKNOWN_DES_FUNC = 0x600
};

enum hinic3_mbox_func_index {
	HINIC3_MBOX_MPU_INDEX = 0,
	HINIC3_MBOX_PF_INDEX = 1,
	HINIC3_MAX_FUNCTIONS = 2,
};

struct mbox_msg_info {
	uint8_t msg_id;
	uint8_t status; /**< Can only use 3 bit. */
};

struct hinic3_recv_mbox {
	void *mbox;
	uint16_t cmd;
	enum hinic3_mod_type mod;
	uint16_t mbox_len;
	void *buf_out;
	enum hinic3_msg_ack_type ack_type;
	struct mbox_msg_info msg_info;
	uint8_t seq_id;
	RTE_ATOMIC(int32_t)msg_cnt;
};

struct hinic3_send_mbox {
	uint8_t *data;
	uint64_t *wb_status; /**< Write back status. */

	const struct rte_memzone *wb_mz;
	void *wb_vaddr;	     /**< Write back virtual address. */
	rte_iova_t wb_paddr; /**< Write back physical address. */

	const struct rte_memzone *sbuff_mz;
	void *sbuff_vaddr;
	rte_iova_t sbuff_paddr;
};

enum mbox_event_state {
	EVENT_START = 0,
	EVENT_FAIL,
	EVENT_SUCCESS,
	EVENT_TIMEOUT,
	EVENT_END
};

/* Execution status of the callback function. */
enum hinic3_mbox_cb_state {
	HINIC3_VF_MBOX_CB_REG = 0,
	HINIC3_VF_MBOX_CB_RUNNING,
	HINIC3_PF_MBOX_CB_REG,
	HINIC3_PF_MBOX_CB_RUNNING,
	HINIC3_PPF_MBOX_CB_REG,
	HINIC3_PPF_MBOX_CB_RUNNING,
	HINIC3_PPF_TO_PF_MBOX_CB_REG,
	HINIC3_PPF_TO_PF_MBOX_CB_RUNNING
};

struct hinic3_mbox {
	struct hinic3_hwdev *hwdev;

	struct hinic3_send_mbox send_mbox;

	/* Last element for mgmt. */
	struct hinic3_recv_mbox mbox_resp[HINIC3_MAX_FUNCTIONS + 1];
	struct hinic3_recv_mbox mbox_send[HINIC3_MAX_FUNCTIONS + 1];

	uint8_t send_msg_id;
	enum mbox_event_state event_flag;
	/* Lock for mbox event flag. */
	rte_spinlock_t mbox_lock;
};

int hinic3_mbox_func_aeqe_handler(struct hinic3_hwdev *hwdev, uint8_t *header,
				  __rte_unused uint8_t size, void *param);

int hinic3_func_to_func_init(struct hinic3_hwdev *hwdev);

void hinic3_func_to_func_free(struct hinic3_hwdev *hwdev);

int hinic3_send_mbox_to_mgmt(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
			     struct hinic3_handler_info *handler_info, uint32_t timeout);

void hinic3_response_mbox_to_mgmt(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
				  struct hinic3_handler_info *handler_info, uint16_t msg_id);

#endif /**< _HINIC3_MBOX_H_ */
