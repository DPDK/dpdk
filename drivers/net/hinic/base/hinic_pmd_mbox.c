/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_eqs.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_mbox.h"

#define HINIC_MBOX_INT_DST_FUNC_SHIFT				0
#define HINIC_MBOX_INT_DST_AEQN_SHIFT				10
#define HINIC_MBOX_INT_SRC_RESP_AEQN_SHIFT			12
#define HINIC_MBOX_INT_STAT_DMA_SHIFT				14
/* The size of data to be send (unit of 4 bytes) */
#define HINIC_MBOX_INT_TX_SIZE_SHIFT				20
/* SO_RO(strong order, relax order)  */
#define HINIC_MBOX_INT_STAT_DMA_SO_RO_SHIFT			25
#define HINIC_MBOX_INT_WB_EN_SHIFT				28


#define HINIC_MBOX_INT_DST_FUNC_MASK				0x3FF
#define HINIC_MBOX_INT_DST_AEQN_MASK				0x3
#define HINIC_MBOX_INT_SRC_RESP_AEQN_MASK			0x3
#define HINIC_MBOX_INT_STAT_DMA_MASK				0x3F
#define HINIC_MBOX_INT_TX_SIZE_MASK				0x1F
#define HINIC_MBOX_INT_STAT_DMA_SO_RO_MASK			0x3
#define HINIC_MBOX_INT_WB_EN_MASK				0x1

#define HINIC_MBOX_INT_SET(val, field)	\
			(((val) & HINIC_MBOX_INT_##field##_MASK) << \
			HINIC_MBOX_INT_##field##_SHIFT)

enum hinic_mbox_tx_status {
	TX_DONE = 0,
	TX_IN_PROGRESS,
};

#define HINIC_MBOX_CTRL_TRIGGER_AEQE_SHIFT			0
/* specifies the issue request for the message data.
 * 0 - Tx request is done;
 * 1 - Tx request is in process.
 */
#define HINIC_MBOX_CTRL_TX_STATUS_SHIFT				1

#define HINIC_MBOX_CTRL_TRIGGER_AEQE_MASK			0x1
#define HINIC_MBOX_CTRL_TX_STATUS_MASK				0x1

#define HINIC_MBOX_CTRL_SET(val, field)	\
			(((val) & HINIC_MBOX_CTRL_##field##_MASK) << \
			HINIC_MBOX_CTRL_##field##_SHIFT)

#define HINIC_MBOX_HEADER_MSG_LEN_SHIFT				0
#define HINIC_MBOX_HEADER_MODULE_SHIFT				11
#define HINIC_MBOX_HEADER_SEG_LEN_SHIFT				16
#define HINIC_MBOX_HEADER_NO_ACK_SHIFT				22
#define HINIC_MBOX_HEADER_SEQID_SHIFT				24
#define HINIC_MBOX_HEADER_LAST_SHIFT				30

#define HINIC_MBOX_HEADER_DIRECTION_SHIFT			31
#define HINIC_MBOX_HEADER_CMD_SHIFT				32
#define HINIC_MBOX_HEADER_MSG_ID_SHIFT				40
#define HINIC_MBOX_HEADER_STATUS_SHIFT				48
#define HINIC_MBOX_HEADER_SRC_GLB_FUNC_IDX_SHIFT		54

#define HINIC_MBOX_HEADER_MSG_LEN_MASK				0x7FF
#define HINIC_MBOX_HEADER_MODULE_MASK				0x1F
#define HINIC_MBOX_HEADER_SEG_LEN_MASK				0x3F
#define HINIC_MBOX_HEADER_NO_ACK_MASK				0x1
#define HINIC_MBOX_HEADER_SEQID_MASK				0x3F
#define HINIC_MBOX_HEADER_LAST_MASK				0x1
#define HINIC_MBOX_HEADER_DIRECTION_MASK			0x1
#define HINIC_MBOX_HEADER_CMD_MASK				0xFF
#define HINIC_MBOX_HEADER_MSG_ID_MASK				0xFF
#define HINIC_MBOX_HEADER_STATUS_MASK				0x3F
#define HINIC_MBOX_HEADER_SRC_GLB_FUNC_IDX_MASK			0x3FF

#define HINIC_MBOX_HEADER_GET(val, field)	\
			(((val) >> HINIC_MBOX_HEADER_##field##_SHIFT) & \
			HINIC_MBOX_HEADER_##field##_MASK)
#define HINIC_MBOX_HEADER_SET(val, field)	\
			((u64)((val) & HINIC_MBOX_HEADER_##field##_MASK) << \
			HINIC_MBOX_HEADER_##field##_SHIFT)

#define HINIC_MBOX_COMP_TIME_MS			8000U
#define MBOX_MSG_POLLING_TIMEOUT_MS		5000

/* The size unit is Bytes */
#define HINIC_MBOX_DATA_SIZE			2040
#define MBOX_MAX_BUF_SZ				2048UL
#define MBOX_HEADER_SZ				8

/* MBOX size is 64B, 8B for mbox_header, 4B reserved */
#define MBOX_SEG_LEN				48
#define MBOX_SEG_LEN_ALIGN			4
#define MBOX_WB_STATUS_LEN			16UL
#define MBOX_SIZE				64

/* mbox write back status is 16B, only first 4B is used */
#define MBOX_WB_STATUS_ERRCODE_MASK		0xFFFF
#define MBOX_WB_STATUS_MASK			0xFF
#define MBOX_WB_ERROR_CODE_MASK			0xFF00
#define MBOX_WB_STATUS_FINISHED_SUCCESS		0xFF
#define MBOX_WB_STATUS_FINISHED_WITH_ERR	0xFE
#define MBOX_WB_STATUS_NOT_FINISHED		0x00

#define MBOX_STATUS_FINISHED(wb)	\
	(((wb) & MBOX_WB_STATUS_MASK) != MBOX_WB_STATUS_NOT_FINISHED)
#define MBOX_STATUS_SUCCESS(wb)		\
	(((wb) & MBOX_WB_STATUS_MASK) == MBOX_WB_STATUS_FINISHED_SUCCESS)
#define MBOX_STATUS_ERRCODE(wb)		\
	((wb) & MBOX_WB_ERROR_CODE_MASK)

#define SEQ_ID_START_VAL			0

#define DST_AEQ_IDX_DEFAULT_VAL			0
#define SRC_AEQ_IDX_DEFAULT_VAL			0
#define NO_DMA_ATTRIBUTE_VAL			0

#define MBOX_MSG_NO_DATA_LEN			1

#define FUNC_ID_OFF_SET_8B		8
#define FUNC_ID_OFF_SET_10B		10

#define MBOX_BODY_FROM_HDR(header)	((u8 *)(header) + MBOX_HEADER_SZ)
#define MBOX_AREA(hwif)			\
		((hwif)->cfg_regs_base + HINIC_FUNC_CSR_MAILBOX_DATA_OFF)

#define MBOX_RESPONSE_ERROR		0x1
#define MBOX_MSG_ID_MASK		0xFF
#define MBOX_MSG_ID(func_to_func)	((func_to_func)->send_msg_id)

enum hinic_hwif_direction_type {
	/* driver send msg to up or up send msg to driver*/
	HINIC_HWIF_DIRECT_SEND = 0,
	/* after driver/up send msg to each other, then up/driver ack the msg */
	HINIC_HWIF_RESPONSE,
};

enum mbox_send_mod {
	MBOX_SEND_MSG_POLL = 1
};

enum mbox_seg_type {
	NOT_LAST_SEG,
	LAST_SEG,
};

enum mbox_ordering_type {
	STRONG_ORDER,
	RELAX_ORDER,
};

enum mbox_write_back_type {
	NOT_WRITE_BACK = 0,
	WRITE_BACK,
};

enum mbox_aeq_trig_type {
	NOT_TRIGGER,
	TRIGGER,
};

static int send_mbox_to_func(struct hinic_mbox_func_to_func *func_to_func,
				enum hinic_mod_type mod, u16 cmd, void *msg,
				u16 msg_len, u16 dst_func,
				enum hinic_hwif_direction_type direction,
				enum hinic_mbox_ack_type ack_type,
				struct mbox_msg_info *msg_info);

static int recv_vf_mbox_handler(struct hinic_mbox_func_to_func *func_to_func,
				struct hinic_recv_mbox *recv_mbox,
				void *buf_out, u16 *out_size, void *param)
{
	int rc = 0;
	*out_size = 0;

	switch (recv_mbox->mod) {
	case HINIC_MOD_COMM:
		hinic_comm_async_event_handle(func_to_func->hwdev,
						recv_mbox->cmd, recv_mbox->mbox,
						recv_mbox->mbox_len,
						buf_out, out_size);
		break;
	case HINIC_MOD_L2NIC:
		hinic_l2nic_async_event_handle(func_to_func->hwdev, param,
						recv_mbox->cmd, recv_mbox->mbox,
						recv_mbox->mbox_len,
						buf_out, out_size);
		break;
	default:
		PMD_DRV_LOG(ERR, "No handler, mod: %d", recv_mbox->mod);
		rc = HINIC_MBOX_VF_CMD_ERROR;
		break;
	}

	return rc;
}

static void set_mbx_msg_status(struct mbox_msg_info *msg_info, int status)
{
	if (status == HINIC_DEV_BUSY_ACTIVE_FW)
		msg_info->status = HINIC_MBOX_PF_BUSY_ACTIVE_FW;
	else if (status == HINIC_MBOX_VF_CMD_ERROR)
		msg_info->status = HINIC_MBOX_VF_CMD_ERROR;
	else if (status)
		msg_info->status = HINIC_MBOX_PF_SEND_ERR;
}

static void recv_func_mbox_handler(struct hinic_mbox_func_to_func *func_to_func,
				struct hinic_recv_mbox *recv_mbox,
				u16 src_func_idx, void *param)
{
	struct hinic_hwdev *dev = func_to_func->hwdev;
	struct mbox_msg_info msg_info = { 0 };
	u16 out_size = MBOX_MAX_BUF_SZ;
	void *buf_out = recv_mbox->buf_out;
	int err = 0;

	if (HINIC_IS_VF(dev)) {
		err = recv_vf_mbox_handler(func_to_func, recv_mbox, buf_out,
						&out_size, param);
	} else {
		err = -EINVAL;
		PMD_DRV_LOG(ERR, "PMD doesn't support non-VF handle mailbox message");
	}

	if (!out_size || err)
		out_size = MBOX_MSG_NO_DATA_LEN;

	if (recv_mbox->ack_type == MBOX_ACK) {
		msg_info.msg_id = recv_mbox->msg_info.msg_id;
		set_mbx_msg_status(&msg_info, err);
		send_mbox_to_func(func_to_func, recv_mbox->mod, recv_mbox->cmd,
				buf_out, out_size, src_func_idx,
				HINIC_HWIF_RESPONSE, MBOX_ACK, &msg_info);
	}
}

static bool check_mbox_seq_id_and_seg_len(struct hinic_recv_mbox *recv_mbox,
					  u8 seq_id, u8 seg_len, u8 msg_id)
{
	if (seq_id > HINIC_SEQ_ID_MAX_VAL || seg_len > HINIC_MSG_SEG_LEN)
		return false;

	if (seq_id == 0) {
		recv_mbox->seq_id = seq_id;
		recv_mbox->msg_info.msg_id = msg_id;
	} else {
		if ((seq_id != recv_mbox->seq_id + 1) ||
			msg_id != recv_mbox->msg_info.msg_id) {
			recv_mbox->seq_id = 0;
			return false;
		}

		recv_mbox->seq_id = seq_id;
	}

	return true;
}

static void clear_mbox_status(struct hinic_send_mbox *mbox)
{
	/* clear mailbox write back status */
	*mbox->wb_status = 0;
	rte_wmb();
}

static void mbox_copy_header(struct hinic_send_mbox *mbox, u64 *header)
{
	u32 *data = (u32 *)header;
	u32 i, idx_max = MBOX_HEADER_SZ / sizeof(u32);

	for (i = 0; i < idx_max; i++)
		__raw_writel(*(data + i), mbox->data + i * sizeof(u32));
}

static void
mbox_copy_send_data(struct hinic_send_mbox *mbox, void *seg, u16 seg_len)
{
	u32 *data = (u32 *)seg;
	u32 data_len, chk_sz = sizeof(u32);
	u32 i, idx_max;
	u8 mbox_max_buf[MBOX_SEG_LEN] = {0};

	/* The mbox message should be aligned in 4 bytes. */
	if (seg_len % chk_sz) {
		memcpy(mbox_max_buf, seg, seg_len);
		data = (u32 *)mbox_max_buf;
	}

	data_len = seg_len;
	idx_max = ALIGN(data_len, chk_sz) / chk_sz;

	for (i = 0; i < idx_max; i++)
		__raw_writel(*(data + i),
				mbox->data + MBOX_HEADER_SZ + i * sizeof(u32));
}

static int mbox_msg_ack_aeqn(struct hinic_hwdev *hwdev)
{
	u16 aeq_num = HINIC_HWIF_NUM_AEQS(hwdev->hwif);
	int msg_ack_aeqn;

	if (aeq_num >= HINIC_MAX_AEQS - 1) {
		msg_ack_aeqn = HINIC_AEQN_2;
	} else if (aeq_num == HINIC_MIN_AEQS) {
		/* This is used for ovs */
		msg_ack_aeqn = HINIC_AEQN_1;
	} else {
		PMD_DRV_LOG(ERR, "Warning: Invalid aeq num: %d\n", aeq_num);
		msg_ack_aeqn = -1;
	}

	return msg_ack_aeqn;
}

static u16 mbox_msg_dst_aeqn(struct hinic_hwdev *hwdev,
			enum hinic_hwif_direction_type seq_dir)
{
	u16 dst_aeqn;

	if (seq_dir == HINIC_HWIF_DIRECT_SEND)
		dst_aeqn = HINIC_AEQN_0;
	else
		dst_aeqn = mbox_msg_ack_aeqn(hwdev);

	return dst_aeqn;
}

static int mbox_seg_ack_aeqn(struct hinic_hwdev *hwdev)
{
	return mbox_msg_ack_aeqn(hwdev);
}

static void write_mbox_msg_attr(struct hinic_mbox_func_to_func *func_to_func,
			u16 dst_func, u16 dst_aeqn, u16 seg_ack_aeqn,
			__rte_unused u16 seg_len, int poll)
{
	u32 mbox_int, mbox_ctrl;

	mbox_int = HINIC_MBOX_INT_SET(dst_func, DST_FUNC) |
		HINIC_MBOX_INT_SET(dst_aeqn, DST_AEQN) |
		/* N/A in polling mode */
		HINIC_MBOX_INT_SET(seg_ack_aeqn, SRC_RESP_AEQN) |
		HINIC_MBOX_INT_SET(NO_DMA_ATTRIBUTE_VAL, STAT_DMA) |
		HINIC_MBOX_INT_SET(ALIGN(MBOX_SIZE, MBOX_SEG_LEN_ALIGN) >> 2,
					TX_SIZE) |
		HINIC_MBOX_INT_SET(STRONG_ORDER, STAT_DMA_SO_RO) |
		HINIC_MBOX_INT_SET(WRITE_BACK, WB_EN);

	hinic_hwif_write_reg(func_to_func->hwdev->hwif,
			HINIC_FUNC_CSR_MAILBOX_INT_OFFSET_OFF, mbox_int);

	rte_wmb();
	mbox_ctrl = HINIC_MBOX_CTRL_SET(TX_IN_PROGRESS, TX_STATUS);

	if (poll)
		mbox_ctrl |= HINIC_MBOX_CTRL_SET(NOT_TRIGGER, TRIGGER_AEQE);
	else
		mbox_ctrl |= HINIC_MBOX_CTRL_SET(TRIGGER, TRIGGER_AEQE);

	hinic_hwif_write_reg(func_to_func->hwdev->hwif,
				HINIC_FUNC_CSR_MAILBOX_CONTROL_OFF, mbox_ctrl);
}

static int init_mbox_info(struct hinic_recv_mbox *mbox_info)
{
	int err;

	mbox_info->mbox = kzalloc(MBOX_MAX_BUF_SZ, GFP_KERNEL);
	if (!mbox_info->mbox) {
		PMD_DRV_LOG(ERR, "Alloc mbox buf_in mem failed\n");
		return -ENOMEM;
	}

	mbox_info->buf_out = kzalloc(MBOX_MAX_BUF_SZ, GFP_KERNEL);
	if (!mbox_info->buf_out) {
		PMD_DRV_LOG(ERR, "Alloc mbox buf_out mem failed\n");
		err = -ENOMEM;
		goto alloc_buf_out_err;
	}

	return 0;

alloc_buf_out_err:
	kfree(mbox_info->mbox);

	return err;
}

static void clean_mbox_info(struct hinic_recv_mbox *mbox_info)
{
	kfree(mbox_info->buf_out);
	kfree(mbox_info->mbox);
}

static int alloc_mbox_info(struct hinic_recv_mbox *mbox_info)
{
	u16 func_idx, i;
	int err;

	for (func_idx = 0; func_idx < HINIC_MAX_FUNCTIONS; func_idx++) {
		err = init_mbox_info(&mbox_info[func_idx]);
		if (err) {
			PMD_DRV_LOG(ERR, "Initialize function[%d] mailbox information failed, err: %d",
				    func_idx, err);
			goto init_mbox_info_err;
		}
	}

	return 0;

init_mbox_info_err:
	for (i = 0; i < func_idx; i++)
		clean_mbox_info(&mbox_info[i]);

	return err;
}

static void free_mbox_info(struct hinic_recv_mbox *mbox_info)
{
	u16 func_idx;

	for (func_idx = 0; func_idx < HINIC_MAX_FUNCTIONS; func_idx++)
		clean_mbox_info(&mbox_info[func_idx]);
}

static void prepare_send_mbox(struct hinic_mbox_func_to_func *func_to_func)
{
	struct hinic_send_mbox *send_mbox = &func_to_func->send_mbox;

	send_mbox->data = MBOX_AREA(func_to_func->hwdev->hwif);
}

static int alloc_mbox_wb_status(struct hinic_mbox_func_to_func *func_to_func)
{
	struct hinic_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic_hwdev *hwdev = func_to_func->hwdev;
	struct hinic_hwif *hwif = hwdev->hwif;
	u32 addr_h, addr_l;

	send_mbox->wb_vaddr = dma_zalloc_coherent(hwdev, MBOX_WB_STATUS_LEN,
					&send_mbox->wb_paddr, SOCKET_ID_ANY);
	if (!send_mbox->wb_vaddr) {
		PMD_DRV_LOG(ERR, "Allocating memory for mailbox wb status failed");
		return -ENOMEM;
	}
	send_mbox->wb_status = (volatile u64 *)send_mbox->wb_vaddr;

	addr_h = upper_32_bits(send_mbox->wb_paddr);
	addr_l = lower_32_bits(send_mbox->wb_paddr);
	hinic_hwif_write_reg(hwif, HINIC_FUNC_CSR_MAILBOX_RESULT_H_OFF, addr_h);
	hinic_hwif_write_reg(hwif, HINIC_FUNC_CSR_MAILBOX_RESULT_L_OFF, addr_l);

	return 0;
}

static void free_mbox_wb_status(struct hinic_mbox_func_to_func *func_to_func)
{
	struct hinic_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic_hwdev *hwdev = func_to_func->hwdev;
	struct hinic_hwif *hwif = hwdev->hwif;

	hinic_hwif_write_reg(hwif, HINIC_FUNC_CSR_MAILBOX_RESULT_H_OFF, 0);
	hinic_hwif_write_reg(hwif, HINIC_FUNC_CSR_MAILBOX_RESULT_L_OFF, 0);

	dma_free_coherent(hwdev, MBOX_WB_STATUS_LEN,
				send_mbox->wb_vaddr, send_mbox->wb_paddr);
}

static int recv_mbox_handler(struct hinic_mbox_func_to_func *func_to_func,
		void *header, struct hinic_recv_mbox *recv_mbox, void *param)
{
	u64 mbox_header = *((u64 *)header);
	void *mbox_body = MBOX_BODY_FROM_HDR(header);
	u16 src_func_idx;
	enum hinic_hwif_direction_type direction;
	u8 seq_id, seg_len;
	u8 msg_id;
	u8 front_id;

	seq_id = HINIC_MBOX_HEADER_GET(mbox_header, SEQID);
	seg_len = HINIC_MBOX_HEADER_GET(mbox_header, SEG_LEN);
	direction = HINIC_MBOX_HEADER_GET(mbox_header, DIRECTION);
	src_func_idx = HINIC_MBOX_HEADER_GET(mbox_header, SRC_GLB_FUNC_IDX);
	msg_id = HINIC_MBOX_HEADER_GET(mbox_header, MSG_ID);
	front_id = recv_mbox->seq_id;

	if (!check_mbox_seq_id_and_seg_len(recv_mbox, seq_id, seg_len,
		msg_id)) {
		PMD_DRV_LOG(ERR,
			"Mailbox sequence and segment check failed, src func id: 0x%x, "
			"front id: 0x%x, current id: 0x%x, seg len: 0x%x "
			"front msg_id: %d, cur msg_id: %d",
			src_func_idx, front_id, seq_id, seg_len,
			recv_mbox->msg_info.msg_id, msg_id);
		return HINIC_ERROR;
	}

	memcpy((u8 *)recv_mbox->mbox + seq_id * HINIC_MSG_SEG_LEN,
		mbox_body, seg_len);

	if (!HINIC_MBOX_HEADER_GET(mbox_header, LAST))
		return HINIC_ERROR;

	recv_mbox->seq_id = 0;
	recv_mbox->cmd = HINIC_MBOX_HEADER_GET(mbox_header, CMD);
	recv_mbox->mod = HINIC_MBOX_HEADER_GET(mbox_header, MODULE);
	recv_mbox->mbox_len = HINIC_MBOX_HEADER_GET(mbox_header, MSG_LEN);
	recv_mbox->ack_type = HINIC_MBOX_HEADER_GET(mbox_header, NO_ACK);
	recv_mbox->msg_info.msg_id = HINIC_MBOX_HEADER_GET(mbox_header, MSG_ID);
	recv_mbox->msg_info.status = HINIC_MBOX_HEADER_GET(mbox_header, STATUS);

	if (direction == HINIC_HWIF_RESPONSE) {
		if (recv_mbox->msg_info.msg_id == func_to_func->send_msg_id &&
			func_to_func->event_flag == EVENT_START) {
			return HINIC_OK;
		}

		PMD_DRV_LOG(ERR, "Mbox response timeout, current send msg id(0x%x), recv msg id(0x%x), status(0x%x)",
			func_to_func->send_msg_id, recv_mbox->msg_info.msg_id,
			recv_mbox->msg_info.status);
		return HINIC_ERROR;
	}

	recv_func_mbox_handler(func_to_func, recv_mbox, src_func_idx, param);

	return HINIC_ERROR;
}

/**
 * hinic_mbox_func_aeqe_handler - Process mbox info from func which is
 * sent by aeqe.
 *
 * @param handle
 *   Pointer to hradware nic device.
 * @param header
 *   Mbox header info.
 * @param size
 *   The size of aeqe descriptor.
 * @param param
 *   customized parameter.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
int hinic_mbox_func_aeqe_handler(void *handle, u8 *header,
					__rte_unused u8 size, void *param)
{
	struct hinic_mbox_func_to_func *func_to_func =
				((struct hinic_hwdev *)handle)->func_to_func;
	struct hinic_recv_mbox *recv_mbox;
	u64 mbox_header = *((u64 *)header);
	u16 src = HINIC_MBOX_HEADER_GET(mbox_header, SRC_GLB_FUNC_IDX);

	if (src >= HINIC_MAX_FUNCTIONS) {
		PMD_DRV_LOG(ERR, "Mailbox source function id: %d is invalid",
				src);
		return HINIC_ERROR;
	}

	recv_mbox = (HINIC_MBOX_HEADER_GET(mbox_header, DIRECTION) ==
			HINIC_HWIF_DIRECT_SEND) ?
			&func_to_func->mbox_send[src] :
			&func_to_func->mbox_resp[src];

	return recv_mbox_handler(func_to_func, (u64 *)header, recv_mbox, param);
}

static u16 get_mbox_status(struct hinic_send_mbox *mbox)
{
	/* write back is 16B, but only use first 4B */
	u64 wb_val = be64_to_cpu(*mbox->wb_status);

	rte_rmb(); /* verify reading before check */

	return (u16)(wb_val & MBOX_WB_STATUS_ERRCODE_MASK);
}

static void dump_mox_reg(struct hinic_hwdev *hwdev)
{
	u32 val;

	val = hinic_hwif_read_reg(hwdev->hwif,
					HINIC_FUNC_CSR_MAILBOX_CONTROL_OFF);
	PMD_DRV_LOG(WARNING, "Mailbox control reg: 0x%x", val);
	val = hinic_hwif_read_reg(hwdev->hwif,
					HINIC_FUNC_CSR_MAILBOX_INT_OFFSET_OFF);
	PMD_DRV_LOG(WARNING, "Mailbox interrupt offset: 0x%x", val);
}

static int send_mbox_seg(struct hinic_mbox_func_to_func *func_to_func,
			 u64 header, u16 dst_func, void *seg, u16 seg_len)
{
	struct hinic_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic_hwdev *hwdev = func_to_func->hwdev;
	u16 seq_dir = HINIC_MBOX_HEADER_GET(header, DIRECTION);
	u16 dst_aeqn, seg_ack_aeqn;
	u16 err_code, wb_status = 0;
	u32 cnt = 0;

	dst_aeqn = mbox_msg_dst_aeqn(hwdev, seq_dir);
	seg_ack_aeqn = mbox_seg_ack_aeqn(hwdev);

	clear_mbox_status(send_mbox);

	mbox_copy_header(send_mbox, &header);

	mbox_copy_send_data(send_mbox, seg, seg_len);

	write_mbox_msg_attr(func_to_func, dst_func, dst_aeqn, seg_ack_aeqn,
				seg_len, MBOX_SEND_MSG_POLL);

	rte_wmb();

	while (cnt < MBOX_MSG_POLLING_TIMEOUT_MS) {
		wb_status = get_mbox_status(send_mbox);
		if (MBOX_STATUS_FINISHED(wb_status))
			break;

		rte_delay_ms(1); /* loop every ms */
		cnt++;
	}

	if (cnt == MBOX_MSG_POLLING_TIMEOUT_MS) {
		PMD_DRV_LOG(ERR, "Send mailbox segment timeout, wb status: 0x%x",
				wb_status);
		dump_mox_reg(hwdev);
		return -ETIMEDOUT;
	}

	if (!MBOX_STATUS_SUCCESS(wb_status)) {
		PMD_DRV_LOG(ERR, "Send mailbox segment to function %d error, wb status: 0x%x",
				dst_func, wb_status);
		/*
		 * err_code: 0 responses no errors, other values can
		 * refer to FS doc.
		 */
		err_code = MBOX_STATUS_ERRCODE(wb_status);
		return err_code ? err_code : -EFAULT;
	}

	return 0;
}

static void set_mbox_to_func_event(struct hinic_mbox_func_to_func *func_to_func,
				   enum mbox_event_state event_flag)
{
	spin_lock(&func_to_func->mbox_lock);
	func_to_func->event_flag = event_flag;
	spin_unlock(&func_to_func->mbox_lock);
}

static int send_mbox_to_func(struct hinic_mbox_func_to_func *func_to_func,
				enum hinic_mod_type mod, u16 cmd, void *msg,
				u16 msg_len, u16 dst_func,
				enum hinic_hwif_direction_type direction,
				enum hinic_mbox_ack_type ack_type,
				struct mbox_msg_info *msg_info)
{
	struct hinic_hwdev *hwdev = func_to_func->hwdev;
	int err = 0;
	u32 seq_id = 0;
	u16 seg_len = HINIC_MSG_SEG_LEN;
	u16 left = msg_len;
	u8 *msg_seg = (u8 *)msg;
	u64 header = 0;

	err = hinic_mutex_lock(&func_to_func->msg_send_mutex);
	if (err)
		return err;

	header = HINIC_MBOX_HEADER_SET(msg_len, MSG_LEN) |
		HINIC_MBOX_HEADER_SET(mod, MODULE) |
		HINIC_MBOX_HEADER_SET(seg_len, SEG_LEN) |
		HINIC_MBOX_HEADER_SET(ack_type, NO_ACK) |
		HINIC_MBOX_HEADER_SET(SEQ_ID_START_VAL, SEQID) |
		HINIC_MBOX_HEADER_SET(NOT_LAST_SEG, LAST) |
		HINIC_MBOX_HEADER_SET(direction, DIRECTION) |
		HINIC_MBOX_HEADER_SET(cmd, CMD) |
		HINIC_MBOX_HEADER_SET(msg_info->msg_id, MSG_ID) |
		HINIC_MBOX_HEADER_SET(msg_info->status, STATUS) |
		HINIC_MBOX_HEADER_SET(hinic_global_func_id(hwdev),
					SRC_GLB_FUNC_IDX);

	while (!(HINIC_MBOX_HEADER_GET(header, LAST))) {
		if (left <= HINIC_MSG_SEG_LEN) {
			header &=
			~(HINIC_MBOX_HEADER_SET(HINIC_MBOX_HEADER_SEG_LEN_MASK,
						SEG_LEN));
			header |= HINIC_MBOX_HEADER_SET(left, SEG_LEN);
			header |= HINIC_MBOX_HEADER_SET(LAST_SEG, LAST);

			seg_len = left;
		}

		err = send_mbox_seg(func_to_func, header, dst_func, msg_seg,
				    seg_len);
		if (err) {
			PMD_DRV_LOG(ERR, "Fail to send mbox seg, err: %d", err);
			goto send_err;
		}

		left -= HINIC_MSG_SEG_LEN;
		msg_seg += HINIC_MSG_SEG_LEN;

		seq_id++;
		header &= ~(HINIC_MBOX_HEADER_SET(HINIC_MBOX_HEADER_SEQID_MASK,
							SEQID));
		header |= HINIC_MBOX_HEADER_SET(seq_id, SEQID);
	}

send_err:
	(void)hinic_mutex_unlock(&func_to_func->msg_send_mutex);

	return err;
}

static int hinic_mbox_to_func(struct hinic_mbox_func_to_func *func_to_func,
			enum hinic_mod_type mod, u16 cmd, u16 dst_func,
			void *buf_in, u16 in_size, void *buf_out, u16 *out_size,
			u32 timeout)
{
	struct hinic_recv_mbox *mbox_for_resp =
					&func_to_func->mbox_resp[dst_func];
	struct mbox_msg_info msg_info = {0};
	u32 time;
	int err;

	err = hinic_mutex_lock(&func_to_func->mbox_send_mutex);
	if (err)
		return err;

	msg_info.msg_id = (MBOX_MSG_ID(func_to_func) + 1) & MBOX_MSG_ID_MASK;
	MBOX_MSG_ID(func_to_func) = msg_info.msg_id;

	set_mbox_to_func_event(func_to_func, EVENT_START);

	err = send_mbox_to_func(func_to_func, mod, cmd, buf_in, in_size,
				dst_func, HINIC_HWIF_DIRECT_SEND,
				MBOX_ACK, &msg_info);
	if (err)
		goto send_err;

	time = msecs_to_jiffies(timeout ? timeout : HINIC_MBOX_COMP_TIME_MS);
	err = hinic_aeq_poll_msg(func_to_func->ack_aeq, time, NULL);
	if (err) {
		set_mbox_to_func_event(func_to_func, EVENT_TIMEOUT);
		PMD_DRV_LOG(ERR, "Send mailbox message time out");
		err = -ETIMEDOUT;
		goto send_err;
	}

	set_mbox_to_func_event(func_to_func, EVENT_END);

	if (mbox_for_resp->msg_info.status) {
		err = mbox_for_resp->msg_info.status;
		if (err != HINIC_MBOX_PF_BUSY_ACTIVE_FW)
			PMD_DRV_LOG(ERR, "Mailbox response error: 0x%x",
					mbox_for_resp->msg_info.status);
		else
			PMD_DRV_LOG(ERR, "Chip is in active, PF can't process VF message");
		goto send_err;
	}

	rte_rmb();

	if (mbox_for_resp->mbox_len && buf_out && out_size) {
		if (mbox_for_resp->mbox_len <= *out_size) {
			memcpy(buf_out, mbox_for_resp->mbox,
				mbox_for_resp->mbox_len);
			*out_size = mbox_for_resp->mbox_len;
		} else {
			PMD_DRV_LOG(ERR, "Mailbox response message len[%u] overflow",
					mbox_for_resp->mbox_len);
			err = -ERANGE;
		}
	}

send_err:
	if (err && out_size)
		*out_size = 0;
	(void)hinic_mutex_unlock(&func_to_func->mbox_send_mutex);

	return err;
}

static int
mbox_func_params_valid(__rte_unused struct hinic_mbox_func_to_func *mbox_obj,
			void *buf_in, u16 in_size)
{
	if (!buf_in || !in_size)
		return -EINVAL;

	if (in_size > HINIC_MBOX_DATA_SIZE) {
		PMD_DRV_LOG(ERR, "Mailbox message len(%d) exceed limit(%d)",
				in_size, HINIC_MBOX_DATA_SIZE);
		return -EINVAL;
	}

	return 0;
}

static u8 hinic_pf_id_of_vf(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	return hwif->attr.port_to_port_idx;
}

/**
 * hinic_mbox_to_pf - Send mbox info to pf and need pf to response.
 *
 * @param hwdev
 *   Pointer to hardware nic device.
 * @param mod
 *   Mode type of hardware.
 * @param cmd
 *   The command sent to pf.
 * @param buf_in
 *   Input parameter.
 * @param in_size
 *   Input parameter size.
 * @param buf_out
 *   Output parameter.
 * @param out_size
 *   Output parameter size.
 * @param timeout
 *   Timeout.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
int hinic_mbox_to_pf(struct hinic_hwdev *hwdev,
		      enum hinic_mod_type mod, u8 cmd, void *buf_in,
		      u16 in_size, void *buf_out, u16 *out_size, u32 timeout)
{
	struct hinic_mbox_func_to_func *func_to_func = hwdev->func_to_func;
	int err;

	err = mbox_func_params_valid(func_to_func, buf_in, in_size);
	if (err) {
		PMD_DRV_LOG(ERR, "Mailbox parameters check failed: %d", err);
		return err;
	}

	if (!HINIC_IS_VF(hwdev)) {
		PMD_DRV_LOG(ERR, "Input function type error, func_type: %d",
				hinic_func_type(hwdev));
		return -EINVAL;
	}

	return hinic_mbox_to_func(func_to_func, mod, cmd,
				   hinic_pf_id_of_vf(hwdev), buf_in, in_size,
				   buf_out, out_size, timeout);
}

/**
 * hinic_mbox_to_pf_no_ack - Send mbox info to pf and do not need pf to response
 *
 * @param hwdev
 *   Pointer to hardware nic device.
 * @param mod
 *   Mode type of hardware.
 * @param cmd
 *   The command sent to pf.
 * @param buf_in
 *   Input parameter.
 * @param in_size
 *   Input parameter size.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
int hinic_mbox_to_pf_no_ack(struct hinic_hwdev *hwdev, enum hinic_mod_type mod,
			u8 cmd, void *buf_in, u16 in_size)
{
	int err;
	struct mbox_msg_info msg_info = {0};

	err = hinic_mutex_lock(&hwdev->func_to_func->mbox_send_mutex);
	if (err)
		return err;

	err = send_mbox_to_func(hwdev->func_to_func, mod, cmd, buf_in, in_size,
			hinic_pf_id_of_vf(hwdev), HINIC_HWIF_DIRECT_SEND,
			MBOX_NO_ACK, &msg_info);
	if (err)
		PMD_DRV_LOG(ERR, "Send mailbox no ack failed, err: %d", err);

	(void)hinic_mutex_unlock(&hwdev->func_to_func->mbox_send_mutex);

	return err;
}

static int hinic_func_to_func_init(struct hinic_hwdev *hwdev)
{
	struct hinic_mbox_func_to_func *func_to_func;
	int err;

	func_to_func = kzalloc(sizeof(*func_to_func), GFP_KERNEL);
	if (!func_to_func) {
		PMD_DRV_LOG(ERR, "Allocating memory for func_to_func object failed");
		return -ENOMEM;
	}
	hwdev->func_to_func = func_to_func;
	func_to_func->hwdev = hwdev;
	(void)hinic_mutex_init(&func_to_func->mbox_send_mutex, NULL);
	(void)hinic_mutex_init(&func_to_func->msg_send_mutex, NULL);

	err = alloc_mbox_info(func_to_func->mbox_send);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocating memory for mailbox sending failed");
		goto alloc_mbox_for_send_err;
	}

	err = alloc_mbox_info(func_to_func->mbox_resp);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocating memory for mailbox responding failed");
		goto alloc_mbox_for_resp_err;
	}

	err = alloc_mbox_wb_status(func_to_func);
	if (err)
		goto alloc_wb_status_err;

	prepare_send_mbox(func_to_func);

	return 0;

alloc_wb_status_err:
	free_mbox_info(func_to_func->mbox_resp);

alloc_mbox_for_resp_err:
	free_mbox_info(func_to_func->mbox_send);

alloc_mbox_for_send_err:
	kfree(func_to_func);

	return err;
}

/**
 * hinic_comm_func_to_func_free - Uninitialize func to func resource.
 *
 * @param hwdev
 *   Pointer to hardware nic device.
 */
void hinic_comm_func_to_func_free(struct hinic_hwdev *hwdev)
{
	struct hinic_mbox_func_to_func *func_to_func = hwdev->func_to_func;

	free_mbox_wb_status(func_to_func);
	free_mbox_info(func_to_func->mbox_resp);
	free_mbox_info(func_to_func->mbox_send);
	(void)hinic_mutex_destroy(&func_to_func->mbox_send_mutex);
	(void)hinic_mutex_destroy(&func_to_func->msg_send_mutex);
	kfree(func_to_func);
}

/**
 * hinic_comm_func_to_func_init - Initialize func to func resource.
 *
 * @param hwdev
 *   Pointer to hardware nic device.
 */
int hinic_comm_func_to_func_init(struct hinic_hwdev *hwdev)
{
	int rc;
	u16 msg_ack_aeqn;

	rc = hinic_func_to_func_init(hwdev);
	if (rc)
		return rc;

	msg_ack_aeqn = mbox_msg_ack_aeqn(hwdev);

	hwdev->func_to_func->ack_aeq = &hwdev->aeqs->aeq[msg_ack_aeqn];
	hwdev->func_to_func->recv_aeq = &hwdev->aeqs->aeq[HINIC_AEQN_0];

	return 0;
}

