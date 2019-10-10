/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_MBOX_H_
#define _HINIC_PMD_MBOX_H_

#define HINIC_MBOX_RECV_AEQN		0
#define HINIC_MBOX_RSP_AEQN		2

#define HINIC_MBOX_PF_SEND_ERR		0x1
#define HINIC_MBOX_PF_BUSY_ACTIVE_FW	0x2
#define HINIC_MBOX_VF_CMD_ERROR		0x3

/* PFs do not support enable SR-IOV cap when PFs use PMD, VFs just receive
 * mailbox message from PFs. The max number of PFs is 16, so the max number
 * of mailbox buffer for functions is also 16.
 */
#define HINIC_MAX_FUNCTIONS		16
#define HINIC_MAX_PF_FUNCS		16

#define HINIC_MGMT_CMD_UNSUPPORTED	0xFF

#define HINIC_SEQ_ID_MAX_VAL		42
#define HINIC_MSG_SEG_LEN		48

enum hinic_mbox_ack_type {
	MBOX_ACK,
	MBOX_NO_ACK,
};

struct mbox_msg_info {
	u8 msg_id;
	u8 status; /*can only use 6 bit*/
};

struct hinic_recv_mbox {
	void *mbox;
	u8 cmd;
	enum hinic_mod_type mod;
	u16 mbox_len;
	void *buf_out;
	enum hinic_mbox_ack_type ack_type;
	struct mbox_msg_info msg_info;
	u8 sed_id;
};

struct hinic_send_mbox {
	u8 *data;
	volatile u64 *wb_status;
	void *wb_vaddr;
	dma_addr_t wb_paddr;
};

enum mbox_event_state {
	EVENT_START = 0,
	EVENT_TIMEOUT,
	EVENT_END,
};

struct hinic_mbox_func_to_func {
	struct hinic_hwdev *hwdev;

	pthread_mutex_t     mbox_send_mutex;
	pthread_mutex_t     msg_send_mutex;

	struct hinic_send_mbox send_mbox;

	struct hinic_recv_mbox mbox_resp[HINIC_MAX_FUNCTIONS];
	struct hinic_recv_mbox mbox_send[HINIC_MAX_FUNCTIONS];

	struct hinic_eq *rsp_aeq;
	struct hinic_eq *recv_aeq;

	u8 send_msg_id;
	enum mbox_event_state event_flag;
	spinlock_t mbox_lock; /* lock for mbox event flag */
};

/*
 * mbox function prototypes
 */
int hinic_comm_func_to_func_init(struct hinic_hwdev *hwdev);
void hinic_comm_func_to_func_free(struct hinic_hwdev *hwdev);
int hinic_mbox_func_aeqe_handler(void *handle, u8 *header,
					u8 size, void *param);
int hinic_mbox_to_pf(struct hinic_hwdev *hwdev, enum hinic_mod_type mod, u8 cmd,
			void *buf_in, u16 in_size,
			void *buf_out, u16 *out_size, u32 timeout);
int hinic_mbox_to_pf_no_ack(struct hinic_hwdev *hwdev, enum hinic_mod_type mod,
				u8 cmd, void *buf_in, u16 in_size);

#endif /* _HINIC_PMD_MBOX_H_ */
