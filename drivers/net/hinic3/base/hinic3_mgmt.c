/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_hwdev.h"
#include "hinic3_mbox.h"
#include "hinic3_nic_event.h"

#define HINIC3_MSG_TO_MGMT_MAX_LEN 2016

#define MAX_PF_MGMT_BUF_SIZE 2048UL
#define SEGMENT_LEN	     48
#define MGMT_MSG_MAX_SEQ_ID \
	(RTE_ALIGN(HINIC3_MSG_TO_MGMT_MAX_LEN, SEGMENT_LEN) / SEGMENT_LEN)

#define BUF_OUT_DEFAULT_SIZE 1

#define MGMT_MSG_SIZE_MIN     20
#define MGMT_MSG_SIZE_STEP    16
#define MGMT_MSG_RSVD_FOR_DEV 8

static void
send_mgmt_ack(struct hinic3_msg_pf_to_mgmt *pf_to_mgmt, enum hinic3_mod_type mod,
	      struct hinic3_handler_info *handler_info, uint16_t msg_id)
{
	if (!handler_info->in_size)
		handler_info->in_size = BUF_OUT_DEFAULT_SIZE;

	hinic3_response_mbox_to_mgmt(pf_to_mgmt->hwdev, mod, handler_info, msg_id);
}

static bool
check_mgmt_seq_id_and_seg_len(struct hinic3_recv_msg *recv_msg, uint8_t seq_id,
			      uint8_t seg_len, uint16_t msg_id)
{
	if (seq_id > MGMT_MSG_MAX_SEQ_ID || seg_len > SEGMENT_LEN)
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

static void
hinic3_mgmt_recv_msg_handler(struct hinic3_msg_pf_to_mgmt *pf_to_mgmt,
			     struct hinic3_recv_msg *recv_msg,
			     __rte_unused void *param)
{
	void *buf_out = pf_to_mgmt->mgmt_ack_buf;
	bool ack_first = false;
	uint16_t out_size = 0;

	struct hinic3_handler_info handler_info = {
		.cmd = recv_msg->cmd,
		.buf_in = recv_msg->msg,
		.in_size = recv_msg->msg_len,
		.buf_out = buf_out,
		.out_size = &out_size,
		.dst_func = HINIC3_MGMT_SRC_ID,
		.direction = HINIC3_MSG_RESPONSE,
		.ack_type = HINIC3_MSG_NO_ACK,
	};

	memset(buf_out, 0, MAX_PF_MGMT_BUF_SIZE);

	/* Select the corresponding processing function according to the mod. */
	switch (recv_msg->mod) {
	case HINIC3_MOD_COMM:
		hinic3_pf_handle_mgmt_comm_event(pf_to_mgmt->hwdev,
						 pf_to_mgmt, &handler_info);
		break;
	case HINIC3_MOD_L2NIC:
		hinic3_pf_event_handler(pf_to_mgmt->hwdev,
					pf_to_mgmt, &handler_info);
		break;
	case HINIC3_MOD_HILINK:
		hinic3_pf_mag_event_handler(pf_to_mgmt->hwdev,
					    pf_to_mgmt, &handler_info);
		break;

	default:
		PMD_DRV_LOG(ERR,
			    "Not support mod, maybe need to response, mod: %d",
			    recv_msg->mod);
		break;
	}

	if (!ack_first && !recv_msg->async_mgmt_to_pf)
		/* Mgmt sends async msg, sends the response. */
		send_mgmt_ack(pf_to_mgmt, recv_msg->mod, &handler_info, recv_msg->msg_id);
}

/**
 * Handler a recv message from mgmt channel.
 *
 * @param[in] pf_to_mgmt
 * PF to mgmt channel.
 * @param[in] recv_msg
 * Received message details.
 * @param[in] param
 * Customized parameter.
 * @return
 * 0 : When aeqe is response message.
 * -1 : Default result, when wrong message or not last message.
 */
static int
recv_mgmt_msg_handler(struct hinic3_msg_pf_to_mgmt *pf_to_mgmt, uint8_t *header,
		      struct hinic3_recv_msg *recv_msg, void *param)
{
	uint64_t mbox_header = *((uint64_t *)header);
	void *msg_body = header + sizeof(mbox_header);
	uint8_t seq_id, seq_len;
	uint32_t offset;
	uint16_t msg_id;

	/* Don't need to get anything from hw when cmd is async. */
	if (HINIC3_MSG_HEADER_GET(mbox_header, DIRECTION) ==
	    HINIC3_MSG_RESPONSE)
		return 0;

	seq_len = HINIC3_MSG_HEADER_GET(mbox_header, SEG_LEN);
	seq_id = HINIC3_MSG_HEADER_GET(mbox_header, SEQID);
	msg_id = HINIC3_MSG_HEADER_GET(mbox_header, MSG_ID);

	/* Check the consistency between seq_id and seg_len. */
	if (!check_mgmt_seq_id_and_seg_len(recv_msg, seq_id, seq_len, msg_id)) {
		PMD_DRV_LOG(ERR, "Mgmt msg sequence id and segment length check failed");
		PMD_DRV_LOG(ERR,
			    "front seq_id: 0x%x, current seq_id: 0x%x, seq_len: 0x%x front msg_id: %d, cur msg_id: %d",
			    recv_msg->seq_id, seq_id, seq_len, recv_msg->msg_id, msg_id);
		/* Set seq_id to invalid seq_id. */
		recv_msg->seq_id = MGMT_MSG_MAX_SEQ_ID;
		return HINIC3_MSG_HANDLER_RES;
	}

	offset = seq_id * SEGMENT_LEN;
	memcpy((uint8_t *)recv_msg->msg + offset, msg_body, seq_len);

	if (!HINIC3_MSG_HEADER_GET(mbox_header, LAST))
		return HINIC3_MSG_HANDLER_RES;
	/* Setting the message receiving information. */
	recv_msg->cmd = HINIC3_MSG_HEADER_GET(mbox_header, CMD);
	recv_msg->mod = HINIC3_MSG_HEADER_GET(mbox_header, MODULE);
	recv_msg->async_mgmt_to_pf = HINIC3_MSG_HEADER_GET(mbox_header, NO_ACK);
	recv_msg->msg_len = HINIC3_MSG_HEADER_GET(mbox_header, MSG_LEN);
	recv_msg->msg_id = HINIC3_MSG_HEADER_GET(mbox_header, MSG_ID);
	recv_msg->seq_id = MGMT_MSG_MAX_SEQ_ID;

	hinic3_mgmt_recv_msg_handler(pf_to_mgmt, recv_msg, param);

	return HINIC3_MSG_HANDLER_RES;
}

/**
 * Handler for a aeqe from mgmt channel.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device.
 * @param[in] header
 * The header of the message.
 * @param[in] size
 * Indicate size.
 * @param[in] param
 * Customized parameter.
 * @return
 * zero: When aeqe is response message
 * negative: When wrong message or not last message.
 */
int
hinic3_mgmt_msg_aeqe_handler(struct hinic3_hwdev *hwdev, uint8_t *header,
			     uint8_t size, void *param)
{
	struct hinic3_msg_pf_to_mgmt *pf_to_mgmt = NULL;
	struct hinic3_recv_msg *recv_msg = NULL;
	bool is_send_dir = false;

	/* For mbox message, invoke the mailbox processing function. */
	if ((HINIC3_MSG_HEADER_GET(*(uint64_t *)header, SOURCE) ==
	     HINIC3_MSG_FROM_MBOX)) {
		return hinic3_mbox_func_aeqe_handler(hwdev, header, size, param);
	}

	pf_to_mgmt = hwdev->pf_to_mgmt;

	is_send_dir = (HINIC3_MSG_HEADER_GET(*(uint64_t *)header, DIRECTION) ==
		       HINIC3_MSG_DIRECT_SEND) ? true : false;

	/* Determine whether a message is received or responded. */
	recv_msg = is_send_dir ? &pf_to_mgmt->recv_msg_from_mgmt
			       : &pf_to_mgmt->recv_resp_msg_from_mgmt;

	return recv_mgmt_msg_handler(pf_to_mgmt, header, recv_msg, param);
}

/**
 * Allocate received message memory.
 *
 * @param[in] recv_msg
 * Pointer that will hold the allocated data.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
alloc_recv_msg(struct hinic3_recv_msg *recv_msg)
{
	recv_msg->seq_id = MGMT_MSG_MAX_SEQ_ID;

	recv_msg->msg = rte_zmalloc("recv_msg", MAX_PF_MGMT_BUF_SIZE,
				    HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!recv_msg->msg)
		return -ENOMEM;

	return 0;
}

static void
free_recv_msg(struct hinic3_recv_msg *recv_msg)
{
	rte_free(recv_msg->msg);
}

/**
 * Allocate all the message buffers of PF to mgmt channel.
 *
 * @param[in] pf_to_mgmt
 * PF to mgmt channel.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
alloc_msg_buf(struct hinic3_msg_pf_to_mgmt *pf_to_mgmt)
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

	pf_to_mgmt->mgmt_ack_buf = rte_zmalloc("mgmt_ack_buf",
					       MAX_PF_MGMT_BUF_SIZE,
					       HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!pf_to_mgmt->mgmt_ack_buf) {
		err = -ENOMEM;
		goto ack_msg_buf_err;
	}

	return 0;

ack_msg_buf_err:
	free_recv_msg(&pf_to_mgmt->recv_resp_msg_from_mgmt);

alloc_msg_for_resp_err:
	free_recv_msg(&pf_to_mgmt->recv_msg_from_mgmt);
	return err;
}

/**
 * Free all the message buffers of PF to mgmt channel.
 *
 * @param[in] pf_to_mgmt
 * PF to mgmt channel.
 */
static void
free_msg_buf(struct hinic3_msg_pf_to_mgmt *pf_to_mgmt)
{
	rte_free(pf_to_mgmt->mgmt_ack_buf);
	free_recv_msg(&pf_to_mgmt->recv_resp_msg_from_mgmt);
	free_recv_msg(&pf_to_mgmt->recv_msg_from_mgmt);
}

/**
 * Initialize PF to mgmt channel.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_pf_to_mgmt_init(struct hinic3_hwdev *hwdev)
{
	struct hinic3_msg_pf_to_mgmt *pf_to_mgmt;
	int err;

	pf_to_mgmt = rte_zmalloc("pf_to_mgmt", sizeof(*pf_to_mgmt),
				 HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!pf_to_mgmt)
		return -ENOMEM;

	hwdev->pf_to_mgmt = pf_to_mgmt;
	pf_to_mgmt->hwdev = hwdev;

	err = alloc_msg_buf(pf_to_mgmt);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate msg buffers failed");
		goto alloc_msg_buf_err;
	}

	return 0;

alloc_msg_buf_err:
	rte_free(pf_to_mgmt);

	return err;
}

/**
 * Free PF to mgmt channel.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device.
 */
void
hinic3_pf_to_mgmt_free(struct hinic3_hwdev *hwdev)
{
	struct hinic3_msg_pf_to_mgmt *pf_to_mgmt = hwdev->pf_to_mgmt;

	free_msg_buf(pf_to_mgmt);
	rte_free(pf_to_mgmt);
}
