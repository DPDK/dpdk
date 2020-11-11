/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_mbox.h"

#define BUF_OUT_DEFAULT_SIZE		1

#define MAX_PF_MGMT_BUF_SIZE		2048UL

#define MGMT_MSG_SIZE_MIN		20
#define MGMT_MSG_SIZE_STEP		16
#define	MGMT_MSG_RSVD_FOR_DEV		8

#define MGMT_MSG_TIMEOUT		5000	/* millisecond */

#define SYNC_MSG_ID_MASK		0x1FF
#define ASYNC_MSG_ID_MASK		0x1FF
#define ASYNC_MSG_FLAG			0x200

#define MSG_NO_RESP			0xFFFF

#define MAX_MSG_SZ			2016

#define MSG_SZ_IS_VALID(in_size)	((in_size) <= MAX_MSG_SZ)

#define SYNC_MSG_ID(pf_to_mgmt)		((pf_to_mgmt)->sync_msg_id)

#define SYNC_MSG_ID_INC(pf_to_mgmt)	(SYNC_MSG_ID(pf_to_mgmt) = \
			(SYNC_MSG_ID(pf_to_mgmt) + 1) & SYNC_MSG_ID_MASK)

#define ASYNC_MSG_ID(pf_to_mgmt)	((pf_to_mgmt)->async_msg_id)

#define ASYNC_MSG_ID_INC(pf_to_mgmt)	(ASYNC_MSG_ID(pf_to_mgmt) = \
			((ASYNC_MSG_ID(pf_to_mgmt) + 1) & ASYNC_MSG_ID_MASK) \
			| ASYNC_MSG_FLAG)

#define HINIC_SEQ_ID_MAX_VAL		42
#define HINIC_MSG_SEG_LEN		48

#define GET_CURR_AEQ_ELEM(eq)		GET_AEQ_ELEM((eq), (eq)->cons_idx)

#define EQ_ELEM_DESC_TYPE_SHIFT			0
#define EQ_ELEM_DESC_SRC_SHIFT			7
#define EQ_ELEM_DESC_SIZE_SHIFT			8
#define EQ_ELEM_DESC_WRAPPED_SHIFT		31

#define EQ_ELEM_DESC_TYPE_MASK			0x7FU
#define EQ_ELEM_DESC_SRC_MASK			0x1U
#define EQ_ELEM_DESC_SIZE_MASK			0xFFU
#define EQ_ELEM_DESC_WRAPPED_MASK		0x1U

#define EQ_MSIX_RESEND_TIMER_CLEAR		1

#define EQ_ELEM_DESC_GET(val, member)		\
				(((val) >> EQ_ELEM_DESC_##member##_SHIFT) & \
				EQ_ELEM_DESC_##member##_MASK)

#define HINIC_MGMT_CHANNEL_STATUS_SHIFT		0x0
#define HINIC_MGMT_CHANNEL_STATUS_MASK		0x1

#define HINIC_GET_MGMT_CHANNEL_STATUS(val, member)	\
	(((val) >> HINIC_##member##_SHIFT) & HINIC_##member##_MASK)

#define HINIC_MSG_TO_MGMT_MAX_LEN		2016

/**
 * mgmt_msg_len - calculate the total message length
 * @msg_data_len: the length of the message data
 * Return: the total message length
 */
static u16 mgmt_msg_len(u16 msg_data_len)
{
	/* u64 - the size of the header */
	u16 msg_size = (u16)(MGMT_MSG_RSVD_FOR_DEV + sizeof(u64) +
			     msg_data_len);

	if (msg_size > MGMT_MSG_SIZE_MIN)
		msg_size = MGMT_MSG_SIZE_MIN +
			ALIGN((msg_size - MGMT_MSG_SIZE_MIN),
			      MGMT_MSG_SIZE_STEP);
	else
		msg_size = MGMT_MSG_SIZE_MIN;

	return msg_size;
}

/**
 * prepare_header - prepare the header of the message
 * @pf_to_mgmt: PF to MGMT channel
 * @header: pointer of the header to prepare
 * @msg_len: the length of the message
 * @mod: module in the chip that will get the message
 * @ack_type: the type to response
 * @direction: the direction of the original message
 * @cmd: the command to do
 * @msg_id: message id
 */
static void prepare_header(struct hinic_msg_pf_to_mgmt *pf_to_mgmt,
			   u64 *header, int msg_len, enum hinic_mod_type mod,
			   enum hinic_msg_ack_type ack_type,
			   enum hinic_msg_direction_type direction,
			   u8 cmd, u32 msg_id)
{
	struct hinic_hwif *hwif = pf_to_mgmt->hwdev->hwif;

	*header = HINIC_MSG_HEADER_SET(msg_len, MSG_LEN) |
		HINIC_MSG_HEADER_SET(mod, MODULE) |
		HINIC_MSG_HEADER_SET(msg_len, SEG_LEN) |
		HINIC_MSG_HEADER_SET(ack_type, NO_ACK) |
		HINIC_MSG_HEADER_SET(0, ASYNC_MGMT_TO_PF) |
		HINIC_MSG_HEADER_SET(0, SEQID) |
		HINIC_MSG_HEADER_SET(LAST_SEGMENT, LAST) |
		HINIC_MSG_HEADER_SET(direction, DIRECTION) |
		HINIC_MSG_HEADER_SET(cmd, CMD) |
		HINIC_MSG_HEADER_SET(HINIC_PCI_INTF_IDX(hwif), PCI_INTF_IDX) |
		HINIC_MSG_HEADER_SET(hwif->attr.port_to_port_idx, P2P_IDX) |
		HINIC_MSG_HEADER_SET(msg_id, MSG_ID);
}

/**
 * prepare_mgmt_cmd - prepare the mgmt command
 * @mgmt_cmd: pointer to the command to prepare
 * @header: pointer of the header to prepare
 * @msg: the data of the message
 * @msg_len: the length of the message
 */
static void prepare_mgmt_cmd(u8 *mgmt_cmd, u64 *header, void *msg,
			     int msg_len)
{
	u32 cmd_buf_max = MAX_PF_MGMT_BUF_SIZE;

	memset(mgmt_cmd, 0, MGMT_MSG_RSVD_FOR_DEV);

	mgmt_cmd += MGMT_MSG_RSVD_FOR_DEV;
	cmd_buf_max -= MGMT_MSG_RSVD_FOR_DEV;
	memcpy(mgmt_cmd, header, sizeof(*header));

	mgmt_cmd += sizeof(*header);
	cmd_buf_max -= sizeof(*header);
	memcpy(mgmt_cmd, msg, msg_len);
}

/**
 * alloc_recv_msg - allocate received message memory
 * @recv_msg: pointer that will hold the allocated data
 * Return: 0 - success, negative - failure
 */
static int alloc_recv_msg(struct hinic_recv_msg *recv_msg)
{
	int err;

	recv_msg->msg = kzalloc(MAX_PF_MGMT_BUF_SIZE, GFP_KERNEL);
	if (!recv_msg->msg) {
		PMD_DRV_LOG(ERR, "Allocate recv msg buf failed");
		return -ENOMEM;
	}

	recv_msg->buf_out = kzalloc(MAX_PF_MGMT_BUF_SIZE, GFP_KERNEL);
	if (!recv_msg->buf_out) {
		PMD_DRV_LOG(ERR, "Allocate recv msg output buf failed");
		err = -ENOMEM;
		goto alloc_buf_out_err;
	}

	return 0;

alloc_buf_out_err:
	kfree(recv_msg->msg);
	return err;
}

/**
 * free_recv_msg - free received message memory
 * @recv_msg: pointer that holds the allocated data
 */
static void free_recv_msg(struct hinic_recv_msg *recv_msg)
{
	kfree(recv_msg->buf_out);
	kfree(recv_msg->msg);
}

/**
 * alloc_msg_buf - allocate all the message buffers of PF to MGMT channel
 * @pf_to_mgmt: PF to MGMT channel
 * Return: 0 - success, negative - failure
 */
static int alloc_msg_buf(struct hinic_msg_pf_to_mgmt *pf_to_mgmt)
{
	int err;

	err = alloc_recv_msg(&pf_to_mgmt->recv_msg_from_mgmt);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate recv msg failed");
		return err;
	}

	err = alloc_recv_msg(&pf_to_mgmt->recv_resp_msg_from_mgmt);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate resp recv msg failed");
		goto alloc_msg_for_resp_err;
	}

	pf_to_mgmt->async_msg_buf = kzalloc(MAX_PF_MGMT_BUF_SIZE, GFP_KERNEL);
	if (!pf_to_mgmt->async_msg_buf)	{
		PMD_DRV_LOG(ERR, "Allocate async msg buf failed");
		err = -ENOMEM;
		goto async_msg_buf_err;
	}

	pf_to_mgmt->sync_msg_buf = kzalloc(MAX_PF_MGMT_BUF_SIZE, GFP_KERNEL);
	if (!pf_to_mgmt->sync_msg_buf)	{
		PMD_DRV_LOG(ERR, "Allocate sync msg buf failed");
		err = -ENOMEM;
		goto sync_msg_buf_err;
	}

	return 0;

sync_msg_buf_err:
	kfree(pf_to_mgmt->async_msg_buf);

async_msg_buf_err:
	free_recv_msg(&pf_to_mgmt->recv_resp_msg_from_mgmt);

alloc_msg_for_resp_err:
	free_recv_msg(&pf_to_mgmt->recv_msg_from_mgmt);

	return err;
}

/**
 * free_msg_buf - free all the message buffers of PF to MGMT channel
 * @pf_to_mgmt: PF to MGMT channel
 * Return: 0 - success, negative - failure
 */
static void free_msg_buf(struct hinic_msg_pf_to_mgmt *pf_to_mgmt)
{
	kfree(pf_to_mgmt->sync_msg_buf);
	kfree(pf_to_mgmt->async_msg_buf);

	free_recv_msg(&pf_to_mgmt->recv_resp_msg_from_mgmt);
	free_recv_msg(&pf_to_mgmt->recv_msg_from_mgmt);
}

static int hinic_get_mgmt_channel_status(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	u32 val;

	if (hinic_func_type((struct hinic_hwdev *)hwdev) == TYPE_VF)
		return false;

	val = hinic_hwif_read_reg(hwif, HINIC_ICPL_RESERVD_ADDR);

	return HINIC_GET_MGMT_CHANNEL_STATUS(val, MGMT_CHANNEL_STATUS);
}

/**
 * send_msg_to_mgmt_async - send async message
 * @pf_to_mgmt: PF to MGMT channel
 * @mod: module in the chip that will get the message
 * @cmd: command of the message
 * @msg: the data of the message
 * @msg_len: the length of the message
 * @direction: the direction of the original message
 * @resp_msg_id: message id of response
 * Return: 0 - success, negative - failure
 */
static int send_msg_to_mgmt_async(struct hinic_msg_pf_to_mgmt *pf_to_mgmt,
				  enum hinic_mod_type mod, u8 cmd,
				  void *msg, u16 msg_len,
				  enum hinic_msg_direction_type direction,
				  u16 resp_msg_id)
{
	void *mgmt_cmd = pf_to_mgmt->async_msg_buf;
	struct hinic_api_cmd_chain *chain;
	u64 header;
	u16 cmd_size = mgmt_msg_len(msg_len);

	if (direction == HINIC_MSG_RESPONSE)
		prepare_header(pf_to_mgmt, &header, msg_len, mod, HINIC_MSG_ACK,
			       direction, cmd, resp_msg_id);
	else
		prepare_header(pf_to_mgmt, &header, msg_len, mod, HINIC_MSG_ACK,
			       direction, cmd, ASYNC_MSG_ID(pf_to_mgmt));

	prepare_mgmt_cmd((u8 *)mgmt_cmd, &header, msg, msg_len);

	chain = pf_to_mgmt->cmd_chain[HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU];

	return hinic_api_cmd_write(chain, HINIC_NODE_ID_MGMT_HOST, mgmt_cmd,
				   cmd_size);
}

/**
 * send_msg_to_mgmt_sync - send async message
 * @pf_to_mgmt: PF to MGMT channel
 * @mod: module in the chip that will get the message
 * @cmd: command of the message
 * @msg: the msg data
 * @msg_len: the msg data length
 * @ack_type: indicate mgmt command whether need ack or not
 * @direction: the direction of the original message
 * @resp_msg_id: msg id to response for
 * Return: 0 - success, negative - failure
 */
static int send_msg_to_mgmt_sync(struct hinic_msg_pf_to_mgmt *pf_to_mgmt,
				 enum hinic_mod_type mod, u8 cmd,
				 void *msg, u16 msg_len,
				 enum hinic_msg_ack_type ack_type,
				 enum hinic_msg_direction_type direction,
				 __rte_unused u16 resp_msg_id)
{
	void *mgmt_cmd = pf_to_mgmt->sync_msg_buf;
	struct hinic_api_cmd_chain *chain;
	u64 header;
	u16 cmd_size = mgmt_msg_len(msg_len);

	/* If fw is hot active, return failed */
	if (hinic_get_mgmt_channel_status(pf_to_mgmt->hwdev)) {
		if (mod == HINIC_MOD_COMM || mod == HINIC_MOD_L2NIC)
			return HINIC_DEV_BUSY_ACTIVE_FW;
		else
			return -EBUSY;
	}

	if (direction == HINIC_MSG_RESPONSE)
		prepare_header(pf_to_mgmt, &header, msg_len, mod, ack_type,
			       direction, cmd, resp_msg_id);
	else
		prepare_header(pf_to_mgmt, &header, msg_len, mod, ack_type,
			       direction, cmd, SYNC_MSG_ID(pf_to_mgmt));

	prepare_mgmt_cmd((u8 *)mgmt_cmd, &header, msg, msg_len);

	chain = pf_to_mgmt->cmd_chain[HINIC_API_CMD_PMD_WRITE_TO_MGMT];

	return hinic_api_cmd_write(chain, HINIC_NODE_ID_MGMT_HOST,
				   mgmt_cmd, cmd_size);
}

/**
 * hinic_pf_to_mgmt_init - initialize PF to MGMT channel
 * @hwdev: the pointer to the private hardware device object
 * Return: 0 - success, negative - failure
 */
static int hinic_pf_to_mgmt_init(struct hinic_hwdev *hwdev)
{
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt;
	int err;

	pf_to_mgmt = kzalloc(sizeof(*pf_to_mgmt), GFP_KERNEL);
	if (!pf_to_mgmt) {
		PMD_DRV_LOG(ERR, "Allocate pf to mgmt mem failed");
		return -ENOMEM;
	}

	hwdev->pf_to_mgmt = pf_to_mgmt;
	pf_to_mgmt->hwdev = hwdev;

	err = hinic_mutex_init(&pf_to_mgmt->sync_msg_mutex, NULL);
	if (err)
		goto mutex_init_err;

	err = alloc_msg_buf(pf_to_mgmt);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate msg buffers failed");
		goto alloc_msg_buf_err;
	}

	err = hinic_api_cmd_init(hwdev, pf_to_mgmt->cmd_chain);
	if (err) {
		PMD_DRV_LOG(ERR, "Init the api cmd chains failed");
		goto api_cmd_init_err;
	}

	return 0;

api_cmd_init_err:
	free_msg_buf(pf_to_mgmt);

alloc_msg_buf_err:
	hinic_mutex_destroy(&pf_to_mgmt->sync_msg_mutex);

mutex_init_err:
	kfree(pf_to_mgmt);

	return err;
}

/**
 * hinic_pf_to_mgmt_free - free PF to MGMT channel
 * @hwdev: the pointer to the private hardware device object
 */
static void hinic_pf_to_mgmt_free(struct hinic_hwdev *hwdev)
{
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt = hwdev->pf_to_mgmt;

	hinic_api_cmd_free(pf_to_mgmt->cmd_chain);
	free_msg_buf(pf_to_mgmt);
	hinic_mutex_destroy(&pf_to_mgmt->sync_msg_mutex);
	kfree(pf_to_mgmt);
}

static int
hinic_pf_to_mgmt_sync(struct hinic_hwdev *hwdev,
		enum hinic_mod_type mod, u8 cmd, void *buf_in, u16 in_size,
		void *buf_out, u16 *out_size, u32 timeout)
{
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt = hwdev->pf_to_mgmt;
	struct hinic_recv_msg *recv_msg;
	u32 timeo;
	int err, i;

	err = hinic_mutex_lock(&pf_to_mgmt->sync_msg_mutex);
	if (err)
		return err;

	SYNC_MSG_ID_INC(pf_to_mgmt);
	recv_msg = &pf_to_mgmt->recv_resp_msg_from_mgmt;

	err = send_msg_to_mgmt_sync(pf_to_mgmt, mod, cmd, buf_in, in_size,
				    HINIC_MSG_ACK, HINIC_MSG_DIRECT_SEND,
				    MSG_NO_RESP);
	if (err) {
		PMD_DRV_LOG(ERR, "Send msg to mgmt failed");
		goto unlock_sync_msg;
	}

	timeo = msecs_to_jiffies(timeout ? timeout : MGMT_MSG_TIMEOUT);
	for (i = 0; i < pf_to_mgmt->rx_aeq->poll_retry_nr; i++) {
		err = hinic_aeq_poll_msg(pf_to_mgmt->rx_aeq, timeo, NULL);
		if (err) {
			PMD_DRV_LOG(ERR, "Poll mgmt rsp timeout, mod=%d cmd=%d msg_id=%u rc=%d",
				mod, cmd, pf_to_mgmt->sync_msg_id, err);
			err = -ETIMEDOUT;
			hinic_dump_aeq_info(hwdev);
			goto unlock_sync_msg;
		} else {
			if (mod == recv_msg->mod && cmd == recv_msg->cmd &&
			    recv_msg->msg_id == pf_to_mgmt->sync_msg_id) {
				/* the expected response polled */
				break;
			}
			PMD_DRV_LOG(ERR, "AEQ[%d] poll(mod=%d, cmd=%d, msg_id=%u) an "
				"unexpected(mod=%d, cmd=%d, msg_id=%u) response",
				pf_to_mgmt->rx_aeq->q_id, mod, cmd,
				pf_to_mgmt->sync_msg_id, recv_msg->mod,
				recv_msg->cmd, recv_msg->msg_id);
		}
	}

	if (i == pf_to_mgmt->rx_aeq->poll_retry_nr) {
		PMD_DRV_LOG(ERR, "Get %d unexpected mgmt rsp from AEQ[%d], poll mgmt rsp failed",
			i, pf_to_mgmt->rx_aeq->q_id);
		err = -EBADMSG;
		goto unlock_sync_msg;
	}

	rte_smp_rmb();
	if (recv_msg->msg_len && buf_out && out_size) {
		if (recv_msg->msg_len <= *out_size) {
			memcpy(buf_out, recv_msg->msg,
			       recv_msg->msg_len);
			*out_size = recv_msg->msg_len;
		} else {
			PMD_DRV_LOG(ERR, "Mgmt rsp's msg len: %u overflow.",
				recv_msg->msg_len);
			err = -ERANGE;
		}
	}

unlock_sync_msg:
	if (err && out_size)
		*out_size = 0;
	(void)hinic_mutex_unlock(&pf_to_mgmt->sync_msg_mutex);
	return err;
}

int hinic_msg_to_mgmt_sync(void *hwdev, enum hinic_mod_type mod, u8 cmd,
			   void *buf_in, u16 in_size,
			   void *buf_out, u16 *out_size, u32 timeout)
{
	int rc = HINIC_ERROR;

	if (!hwdev || in_size > HINIC_MSG_TO_MGMT_MAX_LEN)
		return -EINVAL;

	if (hinic_func_type(hwdev) == TYPE_VF) {
		rc = hinic_mbox_to_pf(hwdev, mod, cmd, buf_in, in_size,
					buf_out, out_size, timeout);
	} else {
		rc = hinic_pf_to_mgmt_sync(hwdev, mod, cmd, buf_in, in_size,
						buf_out, out_size, timeout);
	}

	return rc;
}

int hinic_msg_to_mgmt_no_ack(void *hwdev, enum hinic_mod_type mod, u8 cmd,
			     void *buf_in, u16 in_size)
{
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt =
				((struct hinic_hwdev *)hwdev)->pf_to_mgmt;
	int err = -EINVAL;

	if (!MSG_SZ_IS_VALID(in_size)) {
		PMD_DRV_LOG(ERR, "Mgmt msg buffer size is invalid");
		return err;
	}

	err = hinic_mutex_lock(&pf_to_mgmt->sync_msg_mutex);
	if (err)
		return err;

	err = send_msg_to_mgmt_sync(pf_to_mgmt, mod, cmd, buf_in, in_size,
				    HINIC_MSG_NO_ACK, HINIC_MSG_DIRECT_SEND,
				    MSG_NO_RESP);

	(void)hinic_mutex_unlock(&pf_to_mgmt->sync_msg_mutex);

	return err;
}

static bool check_mgmt_seq_id_and_seg_len(struct hinic_recv_msg *recv_msg,
					  u8 seq_id, u8 seg_len, u16 msg_id)
{
	if (seq_id > HINIC_SEQ_ID_MAX_VAL || seg_len > HINIC_MSG_SEG_LEN)
		return false;

	if (seq_id == 0) {
		recv_msg->seq_id = seq_id;
		recv_msg->msg_id = msg_id;
	} else {
		if ((seq_id != recv_msg->seq_id + 1) ||
			msg_id != recv_msg->msg_id) {
			recv_msg->seq_id = 0;
			return false;
		}
		recv_msg->seq_id = seq_id;
	}

	return true;
}

/**
 * hinic_mgmt_recv_msg_handler - handler for message from mgmt cpu
 * @pf_to_mgmt: PF to MGMT channel
 * @recv_msg: received message details
 * @param: customized parameter
 */
static void hinic_mgmt_recv_msg_handler(struct hinic_msg_pf_to_mgmt *pf_to_mgmt,
					struct hinic_recv_msg *recv_msg,
					void *param)
{
	void *buf_out = recv_msg->buf_out;
	u16 out_size = 0;

	switch (recv_msg->mod) {
	case HINIC_MOD_COMM:
		hinic_comm_async_event_handle(pf_to_mgmt->hwdev,
					      recv_msg->cmd, recv_msg->msg,
					      recv_msg->msg_len,
					      buf_out, &out_size);
		break;
	case HINIC_MOD_L2NIC:
		hinic_l2nic_async_event_handle(pf_to_mgmt->hwdev, param,
					       recv_msg->cmd, recv_msg->msg,
					       recv_msg->msg_len,
					       buf_out, &out_size);
		break;
	case HINIC_MOD_HILINK:
		hinic_hilink_async_event_handle(pf_to_mgmt->hwdev,
						recv_msg->cmd, recv_msg->msg,
						recv_msg->msg_len,
						buf_out, &out_size);
		break;
	default:
		PMD_DRV_LOG(ERR, "No handler, mod: %d", recv_msg->mod);
		break;
	}

	if (!recv_msg->async_mgmt_to_pf) {
		if (!out_size)
			out_size = BUF_OUT_DEFAULT_SIZE;

		/* MGMT sent sync msg, send the response */
		(void)send_msg_to_mgmt_async(pf_to_mgmt, recv_msg->mod,
					     recv_msg->cmd, buf_out, out_size,
					     HINIC_MSG_RESPONSE,
					     recv_msg->msg_id);
	}
}

/**
 * recv_mgmt_msg_handler - handler a message from mgmt cpu
 * @pf_to_mgmt: PF to MGMT channel
 * @header: the header of the message
 * @recv_msg: received message details
 * @param: customized parameter
 * Return: 0 when aeq is response message, -1 default result,
 * and when wrong message or not last message
 */
static int recv_mgmt_msg_handler(struct hinic_msg_pf_to_mgmt *pf_to_mgmt,
				 u8 *header, struct hinic_recv_msg *recv_msg,
				 void *param)
{
	u64 msg_header = *((u64 *)header);
	void *msg_body = header + sizeof(msg_header);
	u8 *dest_msg;
	u8 seq_id, seq_len;
	u32 msg_buf_max = MAX_PF_MGMT_BUF_SIZE;
	u8 front_id;
	u16 msg_id;

	seq_id = HINIC_MSG_HEADER_GET(msg_header, SEQID);
	seq_len = HINIC_MSG_HEADER_GET(msg_header, SEG_LEN);
	front_id = recv_msg->seq_id;
	msg_id = HINIC_MSG_HEADER_GET(msg_header, MSG_ID);

	if (!check_mgmt_seq_id_and_seg_len(recv_msg, seq_id, seq_len, msg_id)) {
		PMD_DRV_LOG(ERR,
			"Mgmt msg sequence and segment check failed, "
			"func id: 0x%x, front id: 0x%x, current id: 0x%x, seg len: 0x%x "
			"front msg_id: %d, cur msg_id: %d",
			hinic_global_func_id(pf_to_mgmt->hwdev),
			front_id, seq_id, seq_len, recv_msg->msg_id, msg_id);
		return HINIC_ERROR;
	}

	dest_msg = (u8 *)recv_msg->msg + seq_id * HINIC_MSG_SEG_LEN;
	msg_buf_max -= seq_id * HINIC_MSG_SEG_LEN;
	memcpy(dest_msg, msg_body, seq_len);

	if (!HINIC_MSG_HEADER_GET(msg_header, LAST))
		return HINIC_ERROR;

	recv_msg->cmd = HINIC_MSG_HEADER_GET(msg_header, CMD);
	recv_msg->mod = HINIC_MSG_HEADER_GET(msg_header, MODULE);
	recv_msg->async_mgmt_to_pf = HINIC_MSG_HEADER_GET(msg_header,
							  ASYNC_MGMT_TO_PF);
	recv_msg->msg_len = HINIC_MSG_HEADER_GET(msg_header, MSG_LEN);
	recv_msg->msg_id = HINIC_MSG_HEADER_GET(msg_header, MSG_ID);

	if (HINIC_MSG_HEADER_GET(msg_header, DIRECTION) == HINIC_MSG_RESPONSE)
		return HINIC_OK;

	hinic_mgmt_recv_msg_handler(pf_to_mgmt, recv_msg, param);

	return HINIC_ERROR;
}

/**
 * hinic_mgmt_msg_aeqe_handler - handler for a mgmt message event
 * @hwdev: the pointer to the private hardware device object
 * @header: the header of the message
 * @size: unused
 * @param: customized parameter
 * Return: 0 when aeq is response message,
 *	   -1 default result, and when wrong message or not last message
 */
static int hinic_mgmt_msg_aeqe_handler(void *hwdev, u8 *header,
			__rte_unused u8 size, void *param)
{
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt =
				((struct hinic_hwdev *)hwdev)->pf_to_mgmt;
	struct hinic_recv_msg *recv_msg;

	recv_msg = (HINIC_MSG_HEADER_GET(*(u64 *)header, DIRECTION) ==
		    HINIC_MSG_DIRECT_SEND) ?
		    &pf_to_mgmt->recv_msg_from_mgmt :
		    &pf_to_mgmt->recv_resp_msg_from_mgmt;

	return recv_mgmt_msg_handler(pf_to_mgmt, header, recv_msg, param);
}

static int hinic_handle_aeqe(void *handle, enum hinic_aeq_type event,
		      u8 *data, u8 size, void *param)
{
	int rc = 0;

	switch (event) {
	case HINIC_MSG_FROM_MGMT_CPU:
		rc = hinic_mgmt_msg_aeqe_handler(handle, data, size, param);
		break;
	case HINIC_MBX_FROM_FUNC:
		rc = hinic_mbox_func_aeqe_handler(handle, data, size, param);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown event type: 0x%x, size: %d",
			    event, size);
		rc = HINIC_ERROR;
		break;
	}

	return rc;
}

/**
 * hinic_aeq_poll_msg - poll one or continue aeqe, and call dedicated process
 * @eq: aeq of the chip
 * @timeout: 0   - poll all aeqe in eq, used in interrupt mode,
 *           > 0 - poll aeq until get aeqe with 'last' field set to 1,
 *           used in polling mode.
 * @param: customized parameter
 * Return: 0 - Success, EIO - poll timeout, ENODEV - swe not support
 */
int hinic_aeq_poll_msg(struct hinic_eq *eq, u32 timeout, void *param)
{
	struct hinic_aeq_elem *aeqe_pos;
	enum hinic_aeq_type event;
	u32 aeqe_desc = 0;
	u16 i;
	u8 size;
	int done = HINIC_ERROR;
	int err = -EFAULT;
	unsigned long end;

	for (i = 0; ((timeout == 0) && (i < eq->eq_len)) ||
	     ((timeout > 0) && (done != HINIC_OK) && (i < eq->eq_len)); i++) {
		err = -EIO;
		end = jiffies + msecs_to_jiffies(timeout);
		do {
			aeqe_pos = GET_CURR_AEQ_ELEM(eq);
			rte_rmb();

			/* Data in HW is in Big endian Format */
			aeqe_desc = be32_to_cpu(aeqe_pos->desc);

			/* HW updates wrapped bit,
			 * when it adds eq element event
			 */
			if (EQ_ELEM_DESC_GET(aeqe_desc, WRAPPED)
			    != eq->wrapped) {
				err = 0;
				break;
			}

			if (timeout != 0)
				usleep(1000);
		} while (time_before(jiffies, end));

		if (err != HINIC_OK) /*poll time out*/
			break;

		event = EQ_ELEM_DESC_GET(aeqe_desc, TYPE);
		if (EQ_ELEM_DESC_GET(aeqe_desc, SRC)) {
			PMD_DRV_LOG(ERR, "AEQ sw event not support %d", event);
			return -ENODEV;

		} else {
			size = EQ_ELEM_DESC_GET(aeqe_desc, SIZE);
			done = hinic_handle_aeqe(eq->hwdev, event,
						 aeqe_pos->aeqe_data,
						 size, param);
		}

		eq->cons_idx++;
		if (eq->cons_idx == eq->eq_len) {
			eq->cons_idx = 0;
			eq->wrapped = !eq->wrapped;
		}
	}

	eq_update_ci(eq);

	return err;
}

int hinic_comm_pf_to_mgmt_init(struct hinic_hwdev *hwdev)
{
	int rc;

	/* VF do not support send msg to mgmt directly */
	if (hinic_func_type(hwdev) == TYPE_VF)
		return 0;

	rc = hinic_pf_to_mgmt_init(hwdev);
	if (rc)
		return rc;

	hwdev->pf_to_mgmt->rx_aeq = &hwdev->aeqs->aeq[HINIC_MGMT_RSP_AEQN];

	return 0;
}

void hinic_comm_pf_to_mgmt_free(struct hinic_hwdev *hwdev)
{
	/* VF do not support send msg to mgmt directly */
	if (hinic_func_type(hwdev) == TYPE_VF)
		return;

	hinic_pf_to_mgmt_free(hwdev);
}

void hinic_dev_handle_aeq_event(struct hinic_hwdev *hwdev, void *param)
{
	struct hinic_eq *aeq = &hwdev->aeqs->aeq[0];

	/* clear resend timer cnt register */
	hinic_misx_intr_clear_resend_bit(hwdev, aeq->eq_irq.msix_entry_idx,
					 EQ_MSIX_RESEND_TIMER_CLEAR);
	(void)hinic_aeq_poll_msg(aeq, 0, param);
}
