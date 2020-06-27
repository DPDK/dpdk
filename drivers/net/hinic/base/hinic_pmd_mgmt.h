/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_MGMT_H_
#define _HINIC_PMD_MGMT_H_

#include "hinic_pmd_api_cmd.h"
#include "hinic_pmd_eqs.h"

#define HINIC_MSG_HEADER_MSG_LEN_SHIFT				0
#define HINIC_MSG_HEADER_MODULE_SHIFT				11
#define HINIC_MSG_HEADER_SEG_LEN_SHIFT				16
#define HINIC_MSG_HEADER_NO_ACK_SHIFT				22
#define HINIC_MSG_HEADER_ASYNC_MGMT_TO_PF_SHIFT			23
#define HINIC_MSG_HEADER_SEQID_SHIFT				24
#define HINIC_MSG_HEADER_LAST_SHIFT				30
#define HINIC_MSG_HEADER_DIRECTION_SHIFT			31
#define HINIC_MSG_HEADER_CMD_SHIFT				32
#define HINIC_MSG_HEADER_PCI_INTF_IDX_SHIFT			48
#define HINIC_MSG_HEADER_P2P_IDX_SHIFT				50
#define HINIC_MSG_HEADER_MSG_ID_SHIFT				54

#define HINIC_MSG_HEADER_MSG_LEN_MASK				0x7FF
#define HINIC_MSG_HEADER_MODULE_MASK				0x1F
#define HINIC_MSG_HEADER_SEG_LEN_MASK				0x3F
#define HINIC_MSG_HEADER_NO_ACK_MASK				0x1
#define HINIC_MSG_HEADER_ASYNC_MGMT_TO_PF_MASK			0x1
#define HINIC_MSG_HEADER_SEQID_MASK				0x3F
#define HINIC_MSG_HEADER_LAST_MASK				0x1
#define HINIC_MSG_HEADER_DIRECTION_MASK				0x1
#define HINIC_MSG_HEADER_CMD_MASK				0xFF
#define HINIC_MSG_HEADER_PCI_INTF_IDX_MASK			0x3
#define HINIC_MSG_HEADER_P2P_IDX_MASK				0xF
#define HINIC_MSG_HEADER_MSG_ID_MASK				0x3FF

#define HINIC_DEV_BUSY_ACTIVE_FW				0xFE

#define HINIC_MSG_HEADER_GET(val, member)			\
		(((val) >> HINIC_MSG_HEADER_##member##_SHIFT) & \
		HINIC_MSG_HEADER_##member##_MASK)

#define HINIC_MSG_HEADER_SET(val, member)			\
		((u64)((val) & HINIC_MSG_HEADER_##member##_MASK) << \
		HINIC_MSG_HEADER_##member##_SHIFT)

#define HINIC_MGMT_RSP_AEQN		(1)

enum hinic_msg_direction_type {
	HINIC_MSG_DIRECT_SEND	= 0,
	HINIC_MSG_RESPONSE	= 1
};
enum hinic_msg_segment_type {
	NOT_LAST_SEGMENT = 0,
	LAST_SEGMENT	= 1,
};

enum hinic_msg_ack_type {
	HINIC_MSG_ACK = 0,
	HINIC_MSG_NO_ACK = 1,
};

struct hinic_recv_msg {
	void			*msg;
	void			*buf_out;

	u16			msg_len;
	enum hinic_mod_type	mod;
	u8			cmd;
	u16			msg_id;
	int			async_mgmt_to_pf;
	u8			sed_id;
};

#define HINIC_COMM_SELF_CMD_MAX 8

enum comm_pf_to_mgmt_event_state {
	SEND_EVENT_START = 0,
	SEND_EVENT_TIMEOUT,
	SEND_EVENT_END,
};

struct hinic_msg_pf_to_mgmt {
	struct hinic_hwdev		*hwdev;

	/* mutex for sync message */
	pthread_mutex_t			sync_msg_mutex;

	void				*async_msg_buf;
	void				*sync_msg_buf;

	struct hinic_recv_msg		recv_msg_from_mgmt;
	struct hinic_recv_msg		recv_resp_msg_from_mgmt;

	u16				async_msg_id;
	u16				sync_msg_id;

	struct hinic_api_cmd_chain	*cmd_chain[HINIC_API_CMD_MAX];

	struct hinic_eq *rx_aeq;
};

int hinic_msg_to_mgmt_no_ack(void *hwdev, enum hinic_mod_type mod, u8 cmd,
			     void *buf_in, u16 in_size);

int hinic_comm_pf_to_mgmt_init(struct hinic_hwdev *hwdev);

void hinic_comm_pf_to_mgmt_free(struct hinic_hwdev *hwdev);

int hinic_aeq_poll_msg(struct hinic_eq *eq, u32 timeout, void *param);

int hinic_msg_to_mgmt_sync(void *hwdev, enum hinic_mod_type mod, u8 cmd,
			   void *buf_in, u16 in_size,
			   void *buf_out, u16 *out_size, u32 timeout);

void hinic_dev_handle_aeq_event(struct hinic_hwdev *hwdev, void *param);

#endif /* _HINIC_PMD_MGMT_H_ */
