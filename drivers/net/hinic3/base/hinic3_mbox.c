/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_csr.h"
#include "hinic3_eqs.h"
#include "hinic3_hw_cfg.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mbox.h"
#include "hinic3_nic_event.h"

#define HINIC3_MBOX_INT_DST_FUNC_SHIFT	    0
#define HINIC3_MBOX_INT_DST_AEQN_SHIFT	    10
#define HINIC3_MBOX_INT_SRC_RESP_AEQN_SHIFT 12
#define HINIC3_MBOX_INT_STAT_DMA_SHIFT	    14
/* The size of data to be send (unit of 4 bytes). */
#define HINIC3_MBOX_INT_TX_SIZE_SHIFT 20
/* SO_RO(strong order, relax order). */
#define HINIC3_MBOX_INT_STAT_DMA_SO_RO_SHIFT 25
#define HINIC3_MBOX_INT_WB_EN_SHIFT	     28

#define HINIC3_MBOX_INT_DST_AEQN_MASK	    0x3
#define HINIC3_MBOX_INT_SRC_RESP_AEQN_MASK  0x3
#define HINIC3_MBOX_INT_STAT_DMA_MASK	    0x3F
#define HINIC3_MBOX_INT_TX_SIZE_MASK	    0x1F
#define HINIC3_MBOX_INT_STAT_DMA_SO_RO_MASK 0x3
#define HINIC3_MBOX_INT_WB_EN_MASK	    0x1

#define HINIC3_MBOX_INT_SET(val, field)           \
	(((val) & HINIC3_MBOX_INT_##field##_MASK) \
	 << HINIC3_MBOX_INT_##field##_SHIFT)

enum hinic3_mbox_tx_status {
	TX_NOT_DONE = 1,
};

#define HINIC3_MBOX_CTRL_TRIGGER_AEQE_SHIFT 0

/*
 * Specifies the issue request for the message data.
 * 0 - Tx request is done;
 * 1 - Tx request is in process.
 */
#define HINIC3_MBOX_CTRL_TX_STATUS_SHIFT 1
#define HINIC3_MBOX_CTRL_DST_FUNC_SHIFT	 16

#define HINIC3_MBOX_CTRL_TRIGGER_AEQE_MASK 0x1
#define HINIC3_MBOX_CTRL_TX_STATUS_MASK	   0x1
#define HINIC3_MBOX_CTRL_DST_FUNC_MASK	   0x1FFF

#define HINIC3_MBOX_CTRL_SET(val, field)           \
	(((val) & HINIC3_MBOX_CTRL_##field##_MASK) \
	 << HINIC3_MBOX_CTRL_##field##_SHIFT)

#define MBOX_SEGLEN_MASK \
	HINIC3_MSG_HEADER_SET(HINIC3_MSG_HEADER_SEG_LEN_MASK, SEG_LEN)

#define MBOX_MSG_POLLING_TIMEOUT 500000 /* Unit is 10us. */
#define HINIC3_MBOX_COMP_TIME	 40000U	 /* Unit is 1ms. */

#define MBOX_MAX_BUF_SZ	      2048UL
#define MBOX_HEADER_SZ	      8
#define HINIC3_MBOX_DATA_SIZE (MBOX_MAX_BUF_SZ - MBOX_HEADER_SZ)

#define MBOX_TLP_HEADER_SZ 16

/* Mbox size is 64B, 8B for mbox_header, 8B reserved. */
#define MBOX_SEG_LEN	   48
#define MBOX_SEG_LEN_ALIGN 4
#define MBOX_WB_STATUS_LEN 16UL

/* Mbox write back status is 16B, only first 4B is used. */
#define MBOX_WB_STATUS_ERRCODE_MASK	 0xFFFF
#define MBOX_WB_STATUS_MASK		 0xFF
#define MBOX_WB_ERROR_CODE_MASK		 0xFF00
#define MBOX_WB_STATUS_FINISHED_SUCCESS	 0xFF
#define MBOX_WB_STATUS_FINISHED_WITH_ERR 0xFE
#define MBOX_WB_STATUS_NOT_FINISHED	 0x00

/* Determine the write back status. */
#define MBOX_STATUS_FINISHED(wb) \
	(((wb) & MBOX_WB_STATUS_MASK) != MBOX_WB_STATUS_NOT_FINISHED)
#define MBOX_STATUS_SUCCESS(wb) \
	(((wb) & MBOX_WB_STATUS_MASK) == MBOX_WB_STATUS_FINISHED_SUCCESS)
#define MBOX_STATUS_ERRCODE(wb) ((wb) & MBOX_WB_ERROR_CODE_MASK)

/* Indicate the value related to the sequence ID. */
#define SEQ_ID_START_VAL 0
#define SEQ_ID_MAX_VAL	 42

#define DST_AEQ_IDX_DEFAULT_VAL 0
#define SRC_AEQ_IDX_DEFAULT_VAL 0
#define NO_DMA_ATTRIBUTE_VAL	0

#define MBOX_MSG_NO_DATA_LEN 1

/* Obtain the specified content of the mailbox. */
#define MBOX_BODY_FROM_HDR(header) ((uint8_t *)(header) + MBOX_HEADER_SZ)
#define MBOX_AREA(hwif) \
	((hwif)->cfg_regs_base + HINIC3_FUNC_CSR_MAILBOX_DATA_OFF)

#define IS_PF_OR_PPF_SRC(src_func_idx) ((src_func_idx) < HINIC3_MAX_PF_FUNCS)

#define MBOX_RESPONSE_ERROR	  0x1
#define MBOX_MSG_ID_MASK	  0xF
#define MBOX_MSG_ID(func_to_func) ((func_to_func)->send_msg_id)
#define MBOX_MSG_ID_INC(func_to_func)                             \
	({                                                        \
		typeof(func_to_func) __func = (func_to_func);     \
		MBOX_MSG_ID(__func) = (MBOX_MSG_ID(__func) + 1) & \
				      MBOX_MSG_ID_MASK;           \
	})

/* Max message counter waits to process for one function. */
#define HINIC3_MAX_MSG_CNT_TO_PROCESS 10

enum mbox_ordering_type { STRONG_ORDER = 0 };

enum mbox_write_back_type { WRITE_BACK = 1 };

enum mbox_aeq_trig_type { NOT_TRIGGER = 0, TRIGGER = 1 };

static int send_mbox_to_func(struct hinic3_mbox *func_to_func,
			     enum hinic3_mod_type mod,
			     struct hinic3_handler_info *handler_info,
			     struct mbox_msg_info *msg_info);
static int send_tlp_mbox_to_func(struct hinic3_mbox *func_to_func,
				 enum hinic3_mod_type mod,
				 struct hinic3_handler_info *handler_info,
				 struct mbox_msg_info *msg_info);

static int
recv_vf_mbox_handler(struct hinic3_mbox *func_to_func,
		     struct hinic3_recv_mbox *recv_mbox, void *buf_out,
		     uint16_t *out_size, __rte_unused void *param)
{
	int err = 0;
	struct hinic3_handler_info handler_info;
	handler_info.cmd =  recv_mbox->cmd;
	handler_info.buf_in =  recv_mbox->mbox;
	handler_info.in_size = recv_mbox->mbox_len;
	handler_info.buf_out = buf_out;
	handler_info.out_size = out_size;

	/*
	 * Invoke the corresponding processing function according to the type of
	 * the received mailbox.
	 */
	switch (recv_mbox->mod) {
	case HINIC3_MOD_COMM:
		err = hinic3_vf_handle_pf_comm_mbox(func_to_func->hwdev,
			func_to_func, &handler_info);
		break;
	case HINIC3_MOD_CFGM:
		err = hinic3_cfg_mbx_vf_proc_msg(func_to_func->hwdev,
			func_to_func->hwdev->cfg_mgmt, &handler_info);
		break;
	case HINIC3_MOD_L2NIC:
		err = hinic3_vf_event_handler(func_to_func->hwdev,
			func_to_func->hwdev->cfg_mgmt, &handler_info);
		break;
	case HINIC3_MOD_HILINK:
		err = hinic3_vf_mag_event_handler(func_to_func->hwdev,
			func_to_func->hwdev->cfg_mgmt, &handler_info);
		break;
	default:
		PMD_DRV_LOG(ERR, "No handler, mod: %d", recv_mbox->mod);
		err = HINIC3_MBOX_VF_CMD_ERROR;
		break;
	}

	return err;
}

/**
 * Respond to the accept packet, construct a response message, and send it.
 *
 * @param[in] func_to_func
 * Context for inter-function communication.
 * @param[in] recv_mbox
 * Pointer to the received inter-function mailbox structure.
 * @param[in] err
 * Error Code.
 * @param[in] out_size
 * Output Size.
 * @param[in] src_func_idx
 * Index of the source function.
 */
static void
response_for_recv_func_mbox(struct hinic3_mbox *func_to_func,
			    struct hinic3_recv_mbox *recv_mbox, int err,
			    uint16_t out_size, uint16_t src_func_idx)
{
	struct mbox_msg_info msg_info = {0};
	struct hinic3_handler_info handler_info = {
		.cmd = recv_mbox->cmd,
		.buf_in = recv_mbox->buf_out,
		.in_size = out_size,
		.dst_func = HINIC3_MGMT_SRC_ID,
		.direction = HINIC3_MSG_RESPONSE,
		.ack_type = HINIC3_MSG_NO_ACK,
	};
	if (recv_mbox->ack_type == HINIC3_MSG_ACK) {
		msg_info.msg_id = recv_mbox->msg_info.msg_id;
		if (err)
			msg_info.status = HINIC3_MBOX_PF_SEND_ERR;

		/* Select the sending function based on the packet type. */
		if (IS_TLP_MBX(src_func_idx))
			send_tlp_mbox_to_func(func_to_func, recv_mbox->mod,
					      &handler_info, &msg_info);
		else
			send_mbox_to_func(func_to_func, recv_mbox->mod,
					  &handler_info, &msg_info);
	}
}

static bool
check_func_mbox_ack_first(uint8_t mod)
{
	return mod == HINIC3_MOD_HILINK;
}

static void
recv_func_mbox_handler(struct hinic3_mbox *func_to_func, struct hinic3_recv_mbox *recv_mbox,
		       uint16_t src_func_idx, void *param)
{
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;
	void *buf_out = recv_mbox->buf_out;
	bool ack_first = false;
	uint16_t out_size = MBOX_MAX_BUF_SZ;
	int err = 0;
	/* Check whether the response is the first ACK message. */
	ack_first = check_func_mbox_ack_first(recv_mbox->mod);
	if (ack_first && recv_mbox->ack_type == HINIC3_MSG_ACK) {
		response_for_recv_func_mbox(func_to_func, recv_mbox, err,
					    out_size, src_func_idx);
	}

	/* Processe mailbox information in the VF. */
	if (HINIC3_IS_VF(hwdev)) {
		err = recv_vf_mbox_handler(func_to_func, recv_mbox, buf_out,
					   &out_size, param);
	} else {
		err = -EINVAL;
		PMD_DRV_LOG(ERR,
			"PMD doesn't support non-VF handle mailbox message");
	}

	if (!out_size || err)
		out_size = MBOX_MSG_NO_DATA_LEN;

	if (!ack_first && recv_mbox->ack_type == HINIC3_MSG_ACK) {
		response_for_recv_func_mbox(func_to_func, recv_mbox, err,
					    out_size, src_func_idx);
	}
}

/**
 * Processe mailbox responses from functions.
 *
 * @param[in] func_to_func
 * Mailbox for inter-function communication.
 * @param[in] recv_mbox
 * Received mailbox message.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
resp_mbox_handler(struct hinic3_mbox *func_to_func,
		  struct hinic3_recv_mbox *recv_mbox)
{
	int ret;
	rte_spinlock_lock(&func_to_func->mbox_lock);
	if (recv_mbox->msg_info.msg_id == func_to_func->send_msg_id &&
	    func_to_func->event_flag == EVENT_START) {
		func_to_func->event_flag = EVENT_SUCCESS;
		ret = 0;
	} else {
		PMD_DRV_LOG(ERR,
			    "Mbox response timeout, current send msg id(0x%x), recv msg id(0x%x), status(0x%x)",
			    func_to_func->send_msg_id,
			    recv_mbox->msg_info.msg_id,
			    recv_mbox->msg_info.status);
		ret = HINIC3_MSG_HANDLER_RES;
	}
	rte_spinlock_unlock(&func_to_func->mbox_lock);
	return ret;
}

/**
 * Check whether the received mailbox message segment is valid.
 *
 * @param[out] recv_mbox
 * Received mailbox message.
 * @param[in] mbox_header
 * Mailbox header.
 * @return
 * The value true indicates valid, and the value false indicates invalid.
 */
static bool
check_mbox_segment(struct hinic3_recv_mbox *recv_mbox, uint64_t mbox_header)
{
	uint8_t seq_id, seg_len, msg_id, mod;
	uint16_t src_func_idx, cmd;

	/* Get info from the mailbox header. */
	seq_id = HINIC3_MSG_HEADER_GET(mbox_header, SEQID);
	seg_len = HINIC3_MSG_HEADER_GET(mbox_header, SEG_LEN);
	src_func_idx = HINIC3_MSG_HEADER_GET(mbox_header, SRC_GLB_FUNC_IDX);
	msg_id = HINIC3_MSG_HEADER_GET(mbox_header, MSG_ID);
	mod = HINIC3_MSG_HEADER_GET(mbox_header, MODULE);
	cmd = HINIC3_MSG_HEADER_GET(mbox_header, CMD);

	if (seq_id > SEQ_ID_MAX_VAL || seg_len > MBOX_SEG_LEN)
		goto seg_err;

	/* New message segment, which saves its information to recv_mbox. */
	if (seq_id == 0) {
		recv_mbox->seq_id = seq_id;
		recv_mbox->msg_info.msg_id = msg_id;
		recv_mbox->mod = mod;
		recv_mbox->cmd = cmd;
	} else {
		if ((seq_id != recv_mbox->seq_id + 1) ||
		    msg_id != recv_mbox->msg_info.msg_id ||
		    mod != recv_mbox->mod || cmd != recv_mbox->cmd)
			goto seg_err;

		recv_mbox->seq_id = seq_id;
	}

	return true;

seg_err:
	PMD_DRV_LOG(ERR, "Mailbox segment check failed");
	PMD_DRV_LOG(ERR,
		    "src func id: 0x%x, front seg info: seq id: 0x%x, msg id: 0x%x, mod: 0x%x, cmd: 0x%x",
		    src_func_idx, recv_mbox->seq_id, recv_mbox->msg_info.msg_id,
		    recv_mbox->mod, recv_mbox->cmd);
	PMD_DRV_LOG(ERR,
		    "Current seg info: seg len: 0x%x, seq id: 0x%x, msg id: 0x%x, mod: 0x%x, cmd: 0x%x",
		    seg_len, seq_id, msg_id, mod, cmd);

	return false;
}

static int
recv_mbox_handler(struct hinic3_mbox *func_to_func, void *header,
		  struct hinic3_recv_mbox *recv_mbox, void *param)
{
	uint64_t mbox_header = *((uint64_t *)header);
	void *mbox_body = MBOX_BODY_FROM_HDR(header);
	uint16_t src_func_idx;
	int pos;
	uint8_t seq_id;
	/* Obtain information from the mailbox header. */
	seq_id = HINIC3_MSG_HEADER_GET(mbox_header, SEQID);
	src_func_idx = HINIC3_MSG_HEADER_GET(mbox_header, SRC_GLB_FUNC_IDX);

	if (!check_mbox_segment(recv_mbox, mbox_header)) {
		recv_mbox->seq_id = SEQ_ID_MAX_VAL;
		return HINIC3_MSG_HANDLER_RES;
	}

	pos = seq_id * MBOX_SEG_LEN;
	memcpy(((uint8_t *)recv_mbox->mbox + pos), mbox_body,
		HINIC3_MSG_HEADER_GET(mbox_header, SEG_LEN));

	if (!HINIC3_MSG_HEADER_GET(mbox_header, LAST))
		return HINIC3_MSG_HANDLER_RES;
	/* Setting the information about the recv mailbox. */
	recv_mbox->cmd = HINIC3_MSG_HEADER_GET(mbox_header, CMD);
	recv_mbox->mod = HINIC3_MSG_HEADER_GET(mbox_header, MODULE);
	recv_mbox->mbox_len = HINIC3_MSG_HEADER_GET(mbox_header, MSG_LEN);
	recv_mbox->ack_type = HINIC3_MSG_HEADER_GET(mbox_header, NO_ACK);
	recv_mbox->msg_info.msg_id = HINIC3_MSG_HEADER_GET(mbox_header, MSG_ID);
	recv_mbox->msg_info.status = HINIC3_MSG_HEADER_GET(mbox_header, STATUS);
	recv_mbox->seq_id = SEQ_ID_MAX_VAL;

	/*
	 * If the received message is a response message, call the mbox response
	 * processing function.
	 */
	if (HINIC3_MSG_HEADER_GET(mbox_header, DIRECTION) ==
	    HINIC3_MSG_RESPONSE) {
		return resp_mbox_handler(func_to_func, recv_mbox);
	}

	recv_func_mbox_handler(func_to_func, recv_mbox, src_func_idx, param);
	return HINIC3_MSG_HANDLER_RES;
}

static inline uint16_t
hinic3_mbox_get_index(uint64_t func)
{
	return (func == HINIC3_MGMT_SRC_ID) ? HINIC3_MBOX_MPU_INDEX
					    : HINIC3_MBOX_PF_INDEX;
}

int
hinic3_mbox_func_aeqe_handler(struct hinic3_hwdev *hwdev, uint8_t *header,
			      __rte_unused uint8_t size, void *param)
{
	struct hinic3_mbox *func_to_func = NULL;
	struct hinic3_recv_mbox *recv_mbox = NULL;
	uint64_t mbox_header = *((uint64_t *)header);
	uint64_t src, dir;
	/* Obtain the mailbox for communication between functions. */
	func_to_func = hwdev->func_to_func;

	dir = HINIC3_MSG_HEADER_GET(mbox_header, DIRECTION);
	src = HINIC3_MSG_HEADER_GET(mbox_header, SRC_GLB_FUNC_IDX);

	src = hinic3_mbox_get_index(src);
	recv_mbox = (dir == HINIC3_MSG_DIRECT_SEND)
			    ? &func_to_func->mbox_send[src]
			    : &func_to_func->mbox_resp[src];
	/* Processing Received Mailbox info. */
	return recv_mbox_handler(func_to_func, header, recv_mbox, param);
}

static void
clear_mbox_status(struct hinic3_send_mbox *mbox)
{
	*mbox->wb_status = 0;

	/* Clear mailbox write back status. */
	rte_atomic_thread_fence(rte_memory_order_release);
}

static void
mbox_copy_header(struct hinic3_send_mbox *mbox, uint64_t *header)
{
	uint32_t *data = (uint32_t *)header;
	uint32_t i, idx_max = MBOX_HEADER_SZ / sizeof(uint32_t);

	for (i = 0; i < idx_max; i++) {
		rte_write32(rte_cpu_to_be_32(*(data + i)),
			    mbox->data + i * sizeof(uint32_t));
	}
}

#define MBOX_DMA_MSG_INIT_XOR_VAL 0x5a5a5a5a
static uint32_t
mbox_dma_msg_xor(uint32_t *data, uint16_t msg_len)
{
	uint32_t xor = MBOX_DMA_MSG_INIT_XOR_VAL;
	uint16_t dw_len = msg_len / sizeof(uint32_t);
	uint16_t i;

	for (i = 0; i < dw_len; i++)
		xor ^= data[i];

	return xor;
}

static void
mbox_copy_send_data_addr(struct hinic3_send_mbox *mbox, uint16_t seg_len)
{
	uint32_t addr_h, addr_l, xor;

	xor = mbox_dma_msg_xor(mbox->sbuff_vaddr, seg_len);
	addr_h = upper_32_bits(mbox->sbuff_paddr);
	addr_l = lower_32_bits(mbox->sbuff_paddr);

	rte_write32(rte_cpu_to_be_32(xor), mbox->data + MBOX_HEADER_SZ);
	rte_write32(rte_cpu_to_be_32(addr_h),
		    mbox->data + MBOX_HEADER_SZ + sizeof(uint32_t));
	rte_write32(rte_cpu_to_be_32(addr_l),
		    mbox->data + MBOX_HEADER_SZ + 0x2 * sizeof(uint32_t));
	rte_write32(rte_cpu_to_be_32((uint32_t)seg_len),
		    mbox->data + MBOX_HEADER_SZ + 0x3 * sizeof(uint32_t));
	/* Reserved field. */
	rte_write32(0, mbox->data + MBOX_HEADER_SZ + 0x4 * sizeof(uint32_t));
	rte_write32(0, mbox->data + MBOX_HEADER_SZ + 0x5 * sizeof(uint32_t));
}

static void
mbox_copy_send_data(struct hinic3_send_mbox *mbox, void *seg, uint16_t seg_len)
{
	uint32_t *data = seg;
	uint32_t chk_sz = sizeof(uint32_t);
	uint32_t i, idx_max;
	uint8_t mbox_max_buf[MBOX_SEG_LEN] = {0};

	/* The mbox message should be aligned in 4 bytes. */
	if (seg_len % chk_sz) {
		if (seg_len <= sizeof(mbox_max_buf)) {
			memcpy(mbox_max_buf, seg, seg_len);
			data = (uint32_t *)mbox_max_buf;
		} else {
			PMD_DRV_LOG(ERR, "mbox_max_buf overflow: seg_len=%u.", seg_len);
			return;
		}
	}

	idx_max = RTE_ALIGN(seg_len, chk_sz) / chk_sz;

	for (i = 0; i < idx_max; i++) {
		rte_write32(rte_cpu_to_be_32(*(data + i)),
			    mbox->data + MBOX_HEADER_SZ + i * sizeof(uint32_t));
	}
}

static void
write_mbox_msg_attr(struct hinic3_mbox *func_to_func, uint16_t dst_func,
		    uint16_t dst_aeqn, uint16_t seg_len)
{
	uint32_t mbox_int, mbox_ctrl;

	/* If VF, function ids must self-learning by HW(PPF=1 PF=0). */
	if (HINIC3_IS_VF(func_to_func->hwdev) &&
	    dst_func != HINIC3_MGMT_SRC_ID) {
		if (dst_func == HINIC3_HWIF_PPF_IDX(func_to_func->hwdev->hwif))
			dst_func = 1;
		else
			dst_func = 0;
	}
	/* Set the interrupt attribute of the mailbox. */
	mbox_int = HINIC3_MBOX_INT_SET(dst_aeqn, DST_AEQN) |
		   HINIC3_MBOX_INT_SET(0, SRC_RESP_AEQN) |
		   HINIC3_MBOX_INT_SET(NO_DMA_ATTRIBUTE_VAL, STAT_DMA) |
		   HINIC3_MBOX_INT_SET(RTE_ALIGN(seg_len + MBOX_HEADER_SZ,
						 MBOX_SEG_LEN_ALIGN) >> 2, TX_SIZE) |
		   HINIC3_MBOX_INT_SET(STRONG_ORDER, STAT_DMA_SO_RO) |
		   HINIC3_MBOX_INT_SET(WRITE_BACK, WB_EN);

	/* The interrupt attribute is written to the interrupt register. */
	hinic3_hwif_write_reg(func_to_func->hwdev->hwif,
			      HINIC3_FUNC_CSR_MAILBOX_INT_OFFSET_OFF, mbox_int);

	rte_atomic_thread_fence(rte_memory_order_release); /**< Writing the mbox intr attributes */

	/* Set the control attributes of the mailbox and write to register. */
	mbox_ctrl = HINIC3_MBOX_CTRL_SET(TX_NOT_DONE, TX_STATUS);
	mbox_ctrl |= HINIC3_MBOX_CTRL_SET(NOT_TRIGGER, TRIGGER_AEQE);
	mbox_ctrl |= HINIC3_MBOX_CTRL_SET(dst_func, DST_FUNC);
	hinic3_hwif_write_reg(func_to_func->hwdev->hwif,
			      HINIC3_FUNC_CSR_MAILBOX_CONTROL_OFF, mbox_ctrl);
}

/**
 * Read the value of the mailbox register of the hardware device.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 */
static void
dump_mbox_reg(struct hinic3_hwdev *hwdev)
{
	uint32_t val;
	/* Read the value of the MBOX control register. */
	val = hinic3_hwif_read_reg(hwdev->hwif,
				   HINIC3_FUNC_CSR_MAILBOX_CONTROL_OFF);
	PMD_DRV_LOG(ERR, "Mailbox control reg: 0x%x", val);
	/* Read the value of the MBOX interrupt offset register. */
	val = hinic3_hwif_read_reg(hwdev->hwif,
				   HINIC3_FUNC_CSR_MAILBOX_INT_OFFSET_OFF);
	PMD_DRV_LOG(ERR, "Mailbox interrupt offset: 0x%x", val);
}

static uint16_t
get_mbox_status(struct hinic3_send_mbox *mbox)
{
	/* Write back is 16B, but only use first 4B. */
	uint64_t wb_val = rte_be_to_cpu_64(*mbox->wb_status);

	rte_atomic_thread_fence(rte_memory_order_acquire); /**< Verify reading before check. */

	return wb_val & MBOX_WB_STATUS_ERRCODE_MASK;
}

/**
 * Sending Mailbox Message Segment.
 *
 * @param[in] func_to_func
 * Mailbox for inter-function communication.
 * @param[in] header
 * Mailbox header.
 * @param[in] dst_func
 * Indicate destination func.
 * @param[in] seg
 * Segment data to be sent.
 * @param[in] seg_len
 * Length of the segment to be sent.
 * @param[in] msg_info
 * Indicate the message information.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
send_mbox_seg(struct hinic3_mbox *func_to_func, uint64_t header, uint16_t dst_func,
	      void *seg, uint16_t seg_len, __rte_unused void *msg_info)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;
	uint8_t num_aeqs = hwdev->hwif->attr.num_aeqs;
	uint16_t dst_aeqn, wb_status = 0, errcode;
	uint16_t seq_dir = HINIC3_MSG_HEADER_GET(header, DIRECTION);
	uint32_t cnt = 0;

	/* Mbox to mgmt cpu, hardware doesn't care dst aeq id. */
	if (num_aeqs >= 2)
		dst_aeqn = (seq_dir == HINIC3_MSG_DIRECT_SEND)
				   ? HINIC3_ASYNC_MSG_AEQ
				   : HINIC3_MBOX_RSP_MSG_AEQ;
	else
		dst_aeqn = 0;

	clear_mbox_status(send_mbox);
	mbox_copy_header(send_mbox, &header);
	mbox_copy_send_data(send_mbox, seg, seg_len);

	/* Set mailbox msg seg len. */
	write_mbox_msg_attr(func_to_func, dst_func, dst_aeqn, seg_len);
	rte_atomic_thread_fence(rte_memory_order_release); /**< Writing the mbox msg attributes. */

	/* Wait until the status of the mailbox changes to Complete. */
	while (cnt < MBOX_MSG_POLLING_TIMEOUT) {
		wb_status = get_mbox_status(send_mbox);
		if (MBOX_STATUS_FINISHED(wb_status))
			break;

		rte_delay_us(10);
		cnt++;
	}

	if (cnt == MBOX_MSG_POLLING_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			    "Send mailbox segment timeout, wb status: 0x%x",
			    wb_status);
		dump_mbox_reg(hwdev);
		return -ETIMEDOUT;
	}

	if (!MBOX_STATUS_SUCCESS(wb_status)) {
		PMD_DRV_LOG(ERR,
			    "Send mailbox segment to function %d error, wb status: 0x%x",
			    dst_func, wb_status);
		errcode = MBOX_STATUS_ERRCODE(wb_status);
		return errcode ? errcode : -EFAULT;
	}

	return 0;
}

static int
send_tlp_mbox_seg(struct hinic3_mbox *func_to_func, uint64_t header, uint16_t dst_func,
		  void *seg, uint16_t seg_len, __rte_unused void *msg_info)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;
	uint8_t num_aeqs = hwdev->hwif->attr.num_aeqs;
	uint16_t dst_aeqn, errcode, wb_status = 0;
	uint16_t seq_dir = HINIC3_MSG_HEADER_GET(header, DIRECTION);
	uint32_t cnt = 0;

	/* Mbox to mgmt cpu, hardware doesn't care dst aeq id. */
	if (num_aeqs >= 2)
		dst_aeqn = (seq_dir == HINIC3_MSG_DIRECT_SEND)
				   ? HINIC3_ASYNC_MSG_AEQ
				   : HINIC3_MBOX_RSP_MSG_AEQ;
	else
		dst_aeqn = 0;

	clear_mbox_status(send_mbox);
	mbox_copy_header(send_mbox, &header);

	/* Copy data to DMA buffer. */
	memcpy(send_mbox->sbuff_vaddr, seg, seg_len);

	/*
	 * Copy data address to mailbox ctrl CSR(Control and Status Register).
	 */
	mbox_copy_send_data_addr(send_mbox, seg_len);

	/* Set mailbox msg header size. */
	write_mbox_msg_attr(func_to_func, dst_func, dst_aeqn, MBOX_TLP_HEADER_SZ);

	rte_atomic_thread_fence(rte_memory_order_release); /**< Writing the mbox msg attributes. */

	/* Wait until the status of the mailbox changes to Complete. */
	while (cnt < MBOX_MSG_POLLING_TIMEOUT) {
		wb_status = get_mbox_status(send_mbox);
		if (MBOX_STATUS_FINISHED(wb_status))
			break;

		rte_delay_us(10);
		cnt++;
	}

	if (cnt == MBOX_MSG_POLLING_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			    "Send mailbox segment timeout, wb status: 0x%x",
			    wb_status);
		dump_mbox_reg(hwdev);
		return -ETIMEDOUT;
	}

	if (!MBOX_STATUS_SUCCESS(wb_status)) {
		PMD_DRV_LOG(ERR,
			    "Send mailbox segment to function %d error, wb status: 0x%x",
			    dst_func, wb_status);
		errcode = MBOX_STATUS_ERRCODE(wb_status);
		return errcode ? errcode : -EFAULT;
	}

	return 0;
}

static int
send_mbox_to_func(struct hinic3_mbox *func_to_func, enum hinic3_mod_type mod,
		struct hinic3_handler_info *handler_info, struct mbox_msg_info *msg_info)
{
	int err = 0;
	uint32_t seq_id = 0;
	uint16_t seg_len = MBOX_SEG_LEN;
	uint16_t rsp_aeq_id, left = handler_info->in_size;
	uint8_t *msg_seg = (uint8_t *)handler_info->buf_in;
	uint64_t header = 0;

	rsp_aeq_id = HINIC3_MBOX_RSP_MSG_AEQ;

	/* Set the header message. */
	header = HINIC3_MSG_HEADER_SET(handler_info->in_size, MSG_LEN) |
		 HINIC3_MSG_HEADER_SET(mod, MODULE) |
		 HINIC3_MSG_HEADER_SET(seg_len, SEG_LEN) |
		 HINIC3_MSG_HEADER_SET(handler_info->ack_type, NO_ACK) |
		 HINIC3_MSG_HEADER_SET(HINIC3_DATA_INLINE, DATA_TYPE) |
		 HINIC3_MSG_HEADER_SET(SEQ_ID_START_VAL, SEQID) |
		 HINIC3_MSG_HEADER_SET(NOT_LAST_SEGMENT, LAST) |
		 HINIC3_MSG_HEADER_SET(handler_info->direction, DIRECTION) |
		 HINIC3_MSG_HEADER_SET(handler_info->cmd, CMD) |
		 /* The VF's offset to it's associated PF. */
		 HINIC3_MSG_HEADER_SET(msg_info->msg_id, MSG_ID) |
		 HINIC3_MSG_HEADER_SET(rsp_aeq_id, AEQ_ID) |
		 HINIC3_MSG_HEADER_SET(HINIC3_MSG_FROM_MBOX, SOURCE) |
		 HINIC3_MSG_HEADER_SET(!!msg_info->status, STATUS);
	/* Loop until all messages are sent. */
	while (!(HINIC3_MSG_HEADER_GET(header, LAST))) {
		if (left <= MBOX_SEG_LEN) {
			header &= ~MBOX_SEGLEN_MASK;
			header |= HINIC3_MSG_HEADER_SET(left, SEG_LEN);
			header |= HINIC3_MSG_HEADER_SET(LAST_SEGMENT, LAST);

			seg_len = left;
		}

		err = send_mbox_seg(func_to_func, header, handler_info->dst_func,
				    msg_seg, seg_len, msg_info);
		if (err) {
			PMD_DRV_LOG(ERR,
				    "Send mbox seg failed, seq_id: 0x%" PRIx64,
				    HINIC3_MSG_HEADER_GET(header, SEQID));

			goto send_err;
		}

		left -= MBOX_SEG_LEN;
		msg_seg += MBOX_SEG_LEN;

		seq_id++;
		header &= ~(HINIC3_MSG_HEADER_SET(HINIC3_MSG_HEADER_SEQID_MASK, SEQID));
		header |= HINIC3_MSG_HEADER_SET(seq_id, SEQID);
	}
send_err:
	return err;
}

static int
send_tlp_mbox_to_func(struct hinic3_mbox *func_to_func, enum hinic3_mod_type mod,
		      struct hinic3_handler_info *handler_info, struct mbox_msg_info *msg_info)
{
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;
	uint8_t *msg_seg = (uint8_t *)handler_info->buf_in;
	int err = 0;
	uint16_t rsp_aeq_id;
	uint64_t header = 0;

	rsp_aeq_id = HINIC3_MBOX_RSP_MSG_AEQ;

	/* Set the header message. */
	header = HINIC3_MSG_HEADER_SET(MBOX_TLP_HEADER_SZ, MSG_LEN) |
		 HINIC3_MSG_HEADER_SET(MBOX_TLP_HEADER_SZ, SEG_LEN) |
		 HINIC3_MSG_HEADER_SET(mod, MODULE) |
		 HINIC3_MSG_HEADER_SET(LAST_SEGMENT, LAST) |
		 HINIC3_MSG_HEADER_SET(handler_info->ack_type, NO_ACK) |
		 HINIC3_MSG_HEADER_SET(HINIC3_DATA_DMA, DATA_TYPE) |
		 HINIC3_MSG_HEADER_SET(SEQ_ID_START_VAL, SEQID) |
		 HINIC3_MSG_HEADER_SET(handler_info->direction, DIRECTION) |
		 HINIC3_MSG_HEADER_SET(handler_info->cmd, CMD) |
		 /* The VF's offset to it's associated PF. */
		 HINIC3_MSG_HEADER_SET(msg_info->msg_id, MSG_ID) |
		 HINIC3_MSG_HEADER_SET(rsp_aeq_id, AEQ_ID) |
		 HINIC3_MSG_HEADER_SET(HINIC3_MSG_FROM_MBOX, SOURCE) |
		 HINIC3_MSG_HEADER_SET(!!msg_info->status, STATUS) |
		 HINIC3_MSG_HEADER_SET(hinic3_global_func_id(hwdev),
				       SRC_GLB_FUNC_IDX);

	/* Send a message. */
	err = send_tlp_mbox_seg(func_to_func, header, handler_info->dst_func,
				msg_seg, handler_info->in_size, msg_info);
	if (err) {
		PMD_DRV_LOG(ERR, "Send mbox seg failed, seq_id: 0x%" PRIx64,
			    HINIC3_MSG_HEADER_GET(header, SEQID));
	}

	return err;
}

/**
 * Set mailbox F2F(Function to Function) event status.
 *
 * @param[out] func_to_func
 * Context for inter-function communication.
 * @param[in] event_flag
 * Event status enumerated value.
 */
static void
set_mbox_to_func_event(struct hinic3_mbox *func_to_func,
		       enum mbox_event_state event_flag)
{
	rte_spinlock_lock(&func_to_func->mbox_lock);
	func_to_func->event_flag = event_flag;
	rte_spinlock_unlock(&func_to_func->mbox_lock);
}

/**
 * Send data from one function to another and receive responses.
 *
 * @param[in] func_to_func
 * Context for inter-function communication.
 * @param[in] mod
 * Command queue module type.
 * @param[in] handler_info
 * warpped information for function
 * @param[in] timeout
 * Timeout interval for waiting for a response.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_mbox_to_func(struct hinic3_mbox *func_to_func, enum hinic3_mod_type mod,
		    struct hinic3_handler_info *handler_info, uint32_t timeout)
{
	/* Use mbox_resp to hole data which responded from other function. */
	struct hinic3_recv_mbox *mbox_for_resp = NULL;
	struct mbox_msg_info msg_info = {0};
	struct hinic3_eq *aeq = NULL;
	uint16_t mbox_rsp_idx;
	uint32_t time;
	int err;

	mbox_rsp_idx = hinic3_mbox_get_index(handler_info->dst_func);
	mbox_for_resp = &func_to_func->mbox_resp[mbox_rsp_idx];

	/* Set message ID and start event. */
	msg_info.msg_id = MBOX_MSG_ID_INC(func_to_func);
	set_mbox_to_func_event(func_to_func, EVENT_START);

	/* Select a function to send messages based on the dst_func type. */
	if (IS_TLP_MBX(handler_info->dst_func))
		err = send_tlp_mbox_to_func(func_to_func, mod, handler_info, &msg_info);
	else
		err = send_mbox_to_func(func_to_func, mod, handler_info, &msg_info);

	if (err) {
		PMD_DRV_LOG(ERR, "Send mailbox failed, msg_id: %d", msg_info.msg_id);
		set_mbox_to_func_event(func_to_func, EVENT_FAIL);
		goto send_err;
	}

	/* Wait for the response message. */
	time = timeout ? timeout : HINIC3_MBOX_COMP_TIME;
	aeq = &func_to_func->hwdev->aeqs->aeq[HINIC3_MBOX_RSP_MSG_AEQ];
	err = hinic3_aeq_poll_msg(aeq, time, NULL);
	if (err) {
		set_mbox_to_func_event(func_to_func, EVENT_TIMEOUT);
		PMD_DRV_LOG(ERR, "Send mailbox message time out");
		err = -ETIMEDOUT;
		goto send_err;
	}

	/* Check whether mod and command of the rsp message match the sent message. */
	if (mod != mbox_for_resp->mod || handler_info->cmd != mbox_for_resp->cmd) {
		PMD_DRV_LOG(ERR,
			    "Invalid response mbox message, mod: 0x%x, cmd: 0x%x, expect mod: 0x%x, cmd: 0x%x",
			    mbox_for_resp->mod, mbox_for_resp->cmd, mod, handler_info->cmd);
		err = -EFAULT;
		goto send_err;
	}

	/* Check the response status. */
	if (mbox_for_resp->msg_info.status) {
		err = mbox_for_resp->msg_info.status;
		goto send_err;
	}

	/* Check whether the length of the response message is valid. */
	if (handler_info->buf_out && handler_info->out_size) {
		if (*handler_info->out_size < mbox_for_resp->mbox_len) {
			PMD_DRV_LOG(ERR,
				"Invalid response mbox message length: %d for mod: %d cmd: %d, should less than: %d",
				mbox_for_resp->mbox_len, mod,
				handler_info->cmd, *handler_info->out_size);
			err = -EFAULT;
			goto send_err;
		}

		if (mbox_for_resp->mbox_len)
			memcpy(handler_info->buf_out, mbox_for_resp->mbox,
			       mbox_for_resp->mbox_len);

		*handler_info->out_size = mbox_for_resp->mbox_len;
	}
send_err:
	return err;
}

static int
mbox_func_params_valid(__rte_unused struct hinic3_mbox *func_to_func,
		       void *buf_in, uint16_t in_size)
{
	if (!buf_in || !in_size)
		return -EINVAL;

	if (in_size > HINIC3_MBOX_DATA_SIZE) {
		PMD_DRV_LOG(ERR, "Mbox msg len(%d) exceed limit(%" PRIu64 ")",
			    in_size, HINIC3_MBOX_DATA_SIZE);
		return -EINVAL;
	}

	return 0;
}

int
hinic3_send_mbox_to_mgmt(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
			 struct hinic3_handler_info *handler_info, uint32_t timeout)
{
	struct hinic3_mbox *func_to_func = hwdev->func_to_func;
	int err;
	/* Verify the validity of the input parameters. */
	err = mbox_func_params_valid(func_to_func, handler_info->buf_in, handler_info->in_size);
	if (err)
		return err;

	return hinic3_mbox_to_func(func_to_func, mod, handler_info, timeout);
}

void
hinic3_response_mbox_to_mgmt(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
			     struct hinic3_handler_info *handler_info, uint16_t msg_id)
{
	struct mbox_msg_info msg_info;
	msg_info.msg_id = (uint8_t)msg_id;
	msg_info.status = 0;

	send_tlp_mbox_to_func(hwdev->func_to_func, mod, handler_info, &msg_info);
}

static int
init_mbox_info(struct hinic3_recv_mbox *mbox_info, int mbox_max_buf_sz)
{
	int err;

	mbox_info->seq_id = SEQ_ID_MAX_VAL;

	mbox_info->mbox =
		rte_zmalloc("mbox", (size_t)mbox_max_buf_sz, 1); /*lint !e571*/
	if (!mbox_info->mbox)
		return -ENOMEM;

	mbox_info->buf_out = rte_zmalloc("mbox_buf_out",
		(size_t)mbox_max_buf_sz, 1); /*lint !e571*/
	if (!mbox_info->buf_out) {
		err = -ENOMEM;
		goto alloc_buf_out_err;
	}

	return 0;

alloc_buf_out_err:
	rte_free(mbox_info->mbox);

	return err;
}

static void
clean_mbox_info(struct hinic3_recv_mbox *mbox_info)
{
	rte_free(mbox_info->buf_out);
	rte_free(mbox_info->mbox);
}

static int
alloc_mbox_info(struct hinic3_recv_mbox *mbox_info, int mbox_max_buf_sz)
{
	uint16_t func_idx, i;
	int err;

	for (func_idx = 0; func_idx < HINIC3_MAX_FUNCTIONS + 1; func_idx++) {
		err = init_mbox_info(&mbox_info[func_idx], mbox_max_buf_sz);
		if (err) {
			PMD_DRV_LOG(ERR, "Init mbox info failed");
			goto init_mbox_info_err;
		}
	}

	return 0;

init_mbox_info_err:
	for (i = 0; i < func_idx; i++)
		clean_mbox_info(&mbox_info[i]);

	return err;
}

static void
free_mbox_info(struct hinic3_recv_mbox *mbox_info)
{
	uint16_t func_idx;

	for (func_idx = 0; func_idx < HINIC3_MAX_FUNCTIONS + 1; func_idx++)
		clean_mbox_info(&mbox_info[func_idx]);
}

static void
prepare_send_mbox(struct hinic3_mbox *func_to_func)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;

	send_mbox->data = MBOX_AREA(func_to_func->hwdev->hwif);
}

/**
 * Allocate memory for the write-back state of the mailbox and write to
 * register.
 *
 * @param[in] func_to_func
 * Context for inter-function communication.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
alloc_mbox_wb_status(struct hinic3_mbox *func_to_func)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;
	uint32_t addr_h, addr_l;

	/* Reserved DMA area. */
	send_mbox->wb_mz = hinic3_dma_zone_reserve(hwdev->eth_dev,
		"wb_mz", 0, MBOX_WB_STATUS_LEN,
		RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!send_mbox->wb_mz)
		return -ENOMEM;

	send_mbox->wb_vaddr = send_mbox->wb_mz->addr;
	send_mbox->wb_paddr = send_mbox->wb_mz->iova;
	send_mbox->wb_status = send_mbox->wb_vaddr;

	addr_h = upper_32_bits(send_mbox->wb_paddr);
	addr_l = lower_32_bits(send_mbox->wb_paddr);

	/* Write info to the register. */
	hinic3_hwif_write_reg(hwdev->hwif, HINIC3_FUNC_CSR_MAILBOX_RESULT_H_OFF, addr_h);
	hinic3_hwif_write_reg(hwdev->hwif, HINIC3_FUNC_CSR_MAILBOX_RESULT_L_OFF, addr_l);

	return 0;
}

static void
free_mbox_wb_status(struct hinic3_mbox *func_to_func)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;

	hinic3_hwif_write_reg(hwdev->hwif, HINIC3_FUNC_CSR_MAILBOX_RESULT_H_OFF, 0);
	hinic3_hwif_write_reg(hwdev->hwif, HINIC3_FUNC_CSR_MAILBOX_RESULT_L_OFF, 0);

	hinic3_memzone_free(send_mbox->wb_mz);
}

static int
alloc_mbox_tlp_buffer(struct hinic3_mbox *func_to_func)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;
	struct hinic3_hwdev *hwdev = func_to_func->hwdev;

	send_mbox->sbuff_mz = hinic3_dma_zone_reserve(hwdev->eth_dev, "sbuff_mz",
			0, MBOX_MAX_BUF_SZ, MBOX_MAX_BUF_SZ, SOCKET_ID_ANY);
	if (!send_mbox->sbuff_mz)
		return -ENOMEM;

	send_mbox->sbuff_vaddr = send_mbox->sbuff_mz->addr;
	send_mbox->sbuff_paddr = send_mbox->sbuff_mz->iova;

	return 0;
}

static void
free_mbox_tlp_buffer(struct hinic3_mbox *func_to_func)
{
	struct hinic3_send_mbox *send_mbox = &func_to_func->send_mbox;

	hinic3_memzone_free(send_mbox->sbuff_mz);
}

/**
 * Initialize function to function communication.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_func_to_func_init(struct hinic3_hwdev *hwdev)
{
	struct hinic3_mbox *func_to_func;
	int err;

	func_to_func = rte_zmalloc("func_to_func", sizeof(*func_to_func), 1);
	if (!func_to_func)
		return -ENOMEM;

	hwdev->func_to_func = func_to_func;
	func_to_func->hwdev = hwdev;
	rte_spinlock_init(&func_to_func->mbox_lock);

	/* Alloc the memory required by the mailbox. */
	err = alloc_mbox_info(func_to_func->mbox_send, MBOX_MAX_BUF_SZ);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc mem for mbox_active failed");
		goto alloc_mbox_for_send_err;
	}

	err = alloc_mbox_info(func_to_func->mbox_resp, MBOX_MAX_BUF_SZ);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc mem for mbox_passive failed");
		goto alloc_mbox_for_resp_err;
	}

	err = alloc_mbox_tlp_buffer(func_to_func);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc mbox send buffer failed");
		goto alloc_tlp_buffer_err;
	}

	err = alloc_mbox_wb_status(func_to_func);
	if (err) {
		PMD_DRV_LOG(ERR, "Alloc mbox write back status failed");
		goto alloc_wb_status_err;
	}

	prepare_send_mbox(func_to_func);

	return 0;

alloc_wb_status_err:
	free_mbox_tlp_buffer(func_to_func);

alloc_tlp_buffer_err:
	free_mbox_info(func_to_func->mbox_resp);

alloc_mbox_for_resp_err:
	free_mbox_info(func_to_func->mbox_send);

alloc_mbox_for_send_err:
	rte_free(func_to_func);

	return err;
}

void
hinic3_func_to_func_free(struct hinic3_hwdev *hwdev)
{
	struct hinic3_mbox *func_to_func = hwdev->func_to_func;

	free_mbox_wb_status(func_to_func);
	free_mbox_tlp_buffer(func_to_func);
	free_mbox_info(func_to_func->mbox_resp);
	free_mbox_info(func_to_func->mbox_send);
	rte_free(func_to_func);
}
