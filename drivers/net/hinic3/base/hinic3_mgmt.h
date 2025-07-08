/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_MGMT_H_
#define _HINIC3_MGMT_H_

#define HINIC3_MSG_HANDLER_RES (-1)

struct mgmt_msg_head {
	u8 status;
	u8 version;
	u8 rsvd0[6];
};

/* Cmdq module type. */
enum hinic3_mod_type {
	HINIC3_MOD_COMM = 0,  /**< HW communication module. */
	HINIC3_MOD_L2NIC = 1, /**< L2NIC module. */
	HINIC3_MOD_ROCE = 2,
	HINIC3_MOD_PLOG = 3,
	HINIC3_MOD_TOE = 4,
	HINIC3_MOD_FLR = 5,
	HINIC3_MOD_FC = 6,
	HINIC3_MOD_CFGM = 7, /**< Configuration module. */
	HINIC3_MOD_CQM = 8,
	HINIC3_MOD_VSWITCH = 9,
	COMM_MOD_FC = 10,
	HINIC3_MOD_OVS = 11,
	HINIC3_MOD_DSW = 12,
	HINIC3_MOD_MIGRATE = 13,
	HINIC3_MOD_HILINK = 14,
	HINIC3_MOD_CRYPT = 15,	/**< Secure crypto module. */
	HINIC3_MOD_HW_MAX = 16, /**< Hardware max module id. */

	HINIC3_MOD_SW_FUNC = 17, /**< SW module for PF/VF and multi-host. */
	HINIC3_MOD_IOE = 18,
	HINIC3_MOD_MAX
};

typedef enum {
	RES_TYPE_FLUSH_BIT = 0,
	RES_TYPE_MQM,
	RES_TYPE_SMF,

	RES_TYPE_COMM = 10,
	/* Clear mbox and aeq, The RES_TYPE_COMM bit must be set. */
	RES_TYPE_COMM_MGMT_CH,
	/* Clear cmdq and ceq, The RES_TYPE_COMM bit must be set. */
	RES_TYPE_COMM_CMD_CH,
	RES_TYPE_NIC,
	RES_TYPE_OVS,
	RES_TYPE_VBS,
	RES_TYPE_ROCE,
	RES_TYPE_FC,
	RES_TYPE_TOE,
	RES_TYPE_IPSEC,
	RES_TYPE_MAX,
} func_reset_flag_e;

#define HINIC3_COMM_RES                                 \
	((1 << RES_TYPE_COMM) | (1 << RES_TYPE_FLUSH_BIT) | \
	 (1 << RES_TYPE_MQM) | (1 << RES_TYPE_SMF) |        \
	 (1 << RES_TYPE_COMM_CMD_CH))
#define HINIC3_NIC_RES	 (1 << RES_TYPE_NIC)
#define HINIC3_OVS_RES	 (1 << RES_TYPE_OVS)
#define HINIC3_VBS_RES	 (1 << RES_TYPE_VBS)
#define HINIC3_ROCE_RES	 (1 << RES_TYPE_ROCE)
#define HINIC3_FC_RES	 (1 << RES_TYPE_FC)
#define HINIC3_TOE_RES	 (1 << RES_TYPE_TOE)
#define HINIC3_IPSEC_RES (1 << RES_TYPE_IPSEC)

struct hinic3_recv_msg {
	void *msg;

	u16 msg_len;
	enum hinic3_mod_type mod;
	u16 cmd;
	u8 seq_id;
	u16 msg_id;
	int async_mgmt_to_pf;
};

/* Indicate the event status in pf-to-management communication. */
enum comm_pf_to_mgmt_event_state {
	SEND_EVENT_UNINIT = 0,
	SEND_EVENT_START,
	SEND_EVENT_SUCCESS,
	SEND_EVENT_FAIL,
	SEND_EVENT_TIMEOUT,
	SEND_EVENT_END
};

struct hinic3_msg_pf_to_mgmt {
	struct hinic3_hwdev *hwdev;

	/* Mutex for sync message. */
	pthread_mutex_t sync_msg_mutex;

	void *mgmt_ack_buf;

	struct hinic3_recv_msg recv_msg_from_mgmt;
	struct hinic3_recv_msg recv_resp_msg_from_mgmt;

	u16 sync_msg_id;
};

int hinic3_mgmt_msg_aeqe_handler(void *hwdev, u8 *header, u8 size, void *param);

int hinic3_pf_to_mgmt_init(struct hinic3_hwdev *hwdev);

void hinic3_pf_to_mgmt_free(struct hinic3_hwdev *hwdev);

int hinic3_msg_to_mgmt_sync(void *hwdev, enum hinic3_mod_type mod, u16 cmd,
			    void *buf_in, u16 in_size, void *buf_out,
			    u16 *out_size, u32 timeout);

int hinic3_msg_to_mgmt_no_ack(void *hwdev, enum hinic3_mod_type mod, u16 cmd,
			      void *buf_in, u16 in_size);

#endif /**< _HINIC3_MGMT_H_ */
