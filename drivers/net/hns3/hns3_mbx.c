/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_io.h>
#include <rte_spinlock.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>

#include "hns3_ethdev.h"
#include "hns3_regs.h"
#include "hns3_logs.h"
#include "hns3_intr.h"
#include "hns3_rxtx.h"

#define HNS3_CMD_CODE_OFFSET		2

static const struct errno_respcode_map err_code_map[] = {
	{0, 0},
	{1, -EPERM},
	{2, -ENOENT},
	{5, -EIO},
	{11, -EAGAIN},
	{12, -ENOMEM},
	{16, -EBUSY},
	{22, -EINVAL},
	{28, -ENOSPC},
	{95, -EOPNOTSUPP},
};

static int
hns3_resp_to_errno(uint16_t resp_code)
{
	uint32_t i, num;

	num = sizeof(err_code_map) / sizeof(struct errno_respcode_map);
	for (i = 0; i < num; i++) {
		if (err_code_map[i].resp_code == resp_code)
			return err_code_map[i].err_no;
	}

	return -EIO;
}

static void
hns3_poll_all_sync_msg(void)
{
	struct rte_eth_dev *eth_dev;
	struct hns3_adapter *adapter;
	const char *name;
	uint16_t port_id;

	RTE_ETH_FOREACH_DEV(port_id) {
		eth_dev = &rte_eth_devices[port_id];
		name = eth_dev->device->driver->name;
		if (strcmp(name, "net_hns3") && strcmp(name, "net_hns3_vf"))
			continue;
		adapter = eth_dev->data->dev_private;
		if (!adapter || adapter->hw.adapter_state == HNS3_NIC_CLOSED)
			continue;
		/* Synchronous msg, the mbx_resp.req_msg_data is non-zero */
		if (adapter->hw.mbx_resp.req_msg_data)
			hns3_dev_handle_mbx_msg(&adapter->hw);
	}
}

static int
hns3_get_mbx_resp(struct hns3_hw *hw, uint16_t code0, uint16_t code1,
		  uint8_t *resp_data, uint16_t resp_len)
{
#define HNS3_MAX_RETRY_MS	500
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_mbx_resp_status *mbx_resp;
	bool in_irq = false;
	uint64_t now;
	uint64_t end;

	if (resp_len > HNS3_MBX_MAX_RESP_DATA_SIZE) {
		hns3_err(hw, "VF mbx response len(=%d) exceeds maximum(=%d)",
			 resp_len, HNS3_MBX_MAX_RESP_DATA_SIZE);
		return -EINVAL;
	}

	now = get_timeofday_ms();
	end = now + HNS3_MAX_RETRY_MS;
	while ((hw->mbx_resp.head != hw->mbx_resp.tail + hw->mbx_resp.lost) &&
	       (now < end)) {
		if (rte_atomic16_read(&hw->reset.disable_cmd)) {
			hns3_err(hw, "Don't wait for mbx respone because of "
				 "disable_cmd");
			return -EBUSY;
		}

		if (is_reset_pending(hns)) {
			hw->mbx_resp.req_msg_data = 0;
			hns3_err(hw, "Don't wait for mbx respone because of "
				 "reset pending");
			return -EIO;
		}

		/*
		 * The mbox response is running on the interrupt thread.
		 * Sending mbox in the interrupt thread cannot wait for the
		 * response, so polling the mbox response on the irq thread.
		 */
		if (pthread_equal(hw->irq_thread_id, pthread_self())) {
			in_irq = true;
			hns3_poll_all_sync_msg();
		} else {
			rte_delay_ms(HNS3_POLL_RESPONE_MS);
		}
		now = get_timeofday_ms();
	}
	hw->mbx_resp.req_msg_data = 0;
	if (now >= end) {
		hw->mbx_resp.lost++;
		hns3_err(hw,
			 "VF could not get mbx(%d,%d) head(%d) tail(%d) lost(%d) from PF in_irq:%d",
			 code0, code1, hw->mbx_resp.head, hw->mbx_resp.tail,
			 hw->mbx_resp.lost, in_irq);
		return -ETIME;
	}
	rte_io_rmb();
	mbx_resp = &hw->mbx_resp;

	if (mbx_resp->resp_status)
		return mbx_resp->resp_status;

	if (resp_data)
		memcpy(resp_data, &mbx_resp->additional_info[0], resp_len);

	return 0;
}

int
hns3_send_mbx_msg(struct hns3_hw *hw, uint16_t code, uint16_t subcode,
		  const uint8_t *msg_data, uint8_t msg_len, bool need_resp,
		  uint8_t *resp_data, uint16_t resp_len)
{
	struct hns3_mbx_vf_to_pf_cmd *req;
	struct hns3_cmd_desc desc;
	bool is_ring_vector_msg;
	int offset;
	int ret;

	req = (struct hns3_mbx_vf_to_pf_cmd *)desc.data;

	/* first two bytes are reserved for code & subcode */
	if (msg_len > (HNS3_MBX_MAX_MSG_SIZE - HNS3_CMD_CODE_OFFSET)) {
		hns3_err(hw,
			 "VF send mbx msg fail, msg len %d exceeds max payload len %d",
			 msg_len, HNS3_MBX_MAX_MSG_SIZE - HNS3_CMD_CODE_OFFSET);
		return -EINVAL;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MBX_VF_TO_PF, false);
	req->msg[0] = code;
	is_ring_vector_msg = (code == HNS3_MBX_MAP_RING_TO_VECTOR) ||
			     (code == HNS3_MBX_UNMAP_RING_TO_VECTOR) ||
			     (code == HNS3_MBX_GET_RING_VECTOR_MAP);
	if (!is_ring_vector_msg)
		req->msg[1] = subcode;
	if (msg_data) {
		offset = is_ring_vector_msg ? 1 : HNS3_CMD_CODE_OFFSET;
		memcpy(&req->msg[offset], msg_data, msg_len);
	}

	/* synchronous send */
	if (need_resp) {
		req->mbx_need_resp |= HNS3_MBX_NEED_RESP_BIT;
		rte_spinlock_lock(&hw->mbx_resp.lock);
		hw->mbx_resp.req_msg_data = (uint32_t)code << 16 | subcode;
		hw->mbx_resp.head++;
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			rte_spinlock_unlock(&hw->mbx_resp.lock);
			hns3_err(hw, "VF failed(=%d) to send mbx message to PF",
				 ret);
			return ret;
		}

		ret = hns3_get_mbx_resp(hw, code, subcode, resp_data, resp_len);
		rte_spinlock_unlock(&hw->mbx_resp.lock);
	} else {
		/* asynchronous send */
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "VF failed(=%d) to send mbx message to PF",
				 ret);
			return ret;
		}
	}

	return ret;
}

static bool
hns3_cmd_crq_empty(struct hns3_hw *hw)
{
	uint32_t tail = hns3_read_dev(hw, HNS3_CMDQ_RX_TAIL_REG);

	return tail == hw->cmq.crq.next_to_use;
}

static void
hns3_mbx_handler(struct hns3_hw *hw)
{
	struct hns3_mac *mac = &hw->mac;
	enum hns3_reset_level reset_level;
	uint16_t *msg_q;
	uint8_t opcode;
	uint32_t tail;

	tail = hw->arq.tail;

	/* process all the async queue messages */
	while (tail != hw->arq.head) {
		msg_q = hw->arq.msg_q[hw->arq.head];

		opcode = msg_q[0] & 0xff;
		switch (opcode) {
		case HNS3_MBX_LINK_STAT_CHANGE:
			memcpy(&mac->link_speed, &msg_q[2],
				   sizeof(mac->link_speed));
			mac->link_status = rte_le_to_cpu_16(msg_q[1]);
			mac->link_duplex = (uint8_t)rte_le_to_cpu_16(msg_q[4]);
			break;
		case HNS3_MBX_ASSERTING_RESET:
			/* PF has asserted reset hence VF should go in pending
			 * state and poll for the hardware reset status till it
			 * has been completely reset. After this stack should
			 * eventually be re-initialized.
			 */
			reset_level = rte_le_to_cpu_16(msg_q[1]);
			hns3_atomic_set_bit(reset_level, &hw->reset.pending);

			hns3_warn(hw, "PF inform reset level %d", reset_level);
			hw->reset.stats.request_cnt++;
			hns3_schedule_reset(HNS3_DEV_HW_TO_ADAPTER(hw));
			break;
		default:
			hns3_err(hw, "Fetched unsupported(%d) message from arq",
				 opcode);
			break;
		}

		hns3_mbx_head_ptr_move_arq(hw->arq);
		msg_q = hw->arq.msg_q[hw->arq.head];
	}
}

/*
 * Case1: receive response after timeout, req_msg_data
 *        is 0, not equal resp_msg, do lost--
 * Case2: receive last response during new send_mbx_msg,
 *	  req_msg_data is different with resp_msg, let
 *	  lost--, continue to wait for response.
 */
static void
hns3_update_resp_position(struct hns3_hw *hw, uint32_t resp_msg)
{
	struct hns3_mbx_resp_status *resp = &hw->mbx_resp;
	uint32_t tail = resp->tail + 1;

	if (tail > resp->head)
		tail = resp->head;
	if (resp->req_msg_data != resp_msg) {
		if (resp->lost)
			resp->lost--;
		hns3_warn(hw, "Received a mismatched response req_msg(%x) "
			  "resp_msg(%x) head(%d) tail(%d) lost(%d)",
			  resp->req_msg_data, resp_msg, resp->head, tail,
			  resp->lost);
	} else if (tail + resp->lost > resp->head) {
		resp->lost--;
		hns3_warn(hw, "Received a new response again resp_msg(%x) "
			  "head(%d) tail(%d) lost(%d)", resp_msg,
			  resp->head, tail, resp->lost);
	}
	rte_io_wmb();
	resp->tail = tail;
}

static void
hns3_link_fail_parse(struct hns3_hw *hw, uint8_t link_fail_code)
{
	switch (link_fail_code) {
	case HNS3_MBX_LF_NORMAL:
		break;
	case HNS3_MBX_LF_REF_CLOCK_LOST:
		hns3_warn(hw, "Reference clock lost!");
		break;
	case HNS3_MBX_LF_XSFP_TX_DISABLE:
		hns3_warn(hw, "SFP tx is disabled!");
		break;
	case HNS3_MBX_LF_XSFP_ABSENT:
		hns3_warn(hw, "SFP is absent!");
		break;
	default:
		hns3_warn(hw, "Unknown fail code:%u!", link_fail_code);
		break;
	}
}

static void
hns3_handle_link_change_event(struct hns3_hw *hw,
			      struct hns3_mbx_pf_to_vf_cmd *req)
{
#define LINK_STATUS_OFFSET     1
#define LINK_FAIL_CODE_OFFSET  2

	if (!req->msg[LINK_STATUS_OFFSET])
		hns3_link_fail_parse(hw, req->msg[LINK_FAIL_CODE_OFFSET]);

	hns3_update_link_status(hw);
}

static void
hns3_update_port_base_vlan_info(struct hns3_hw *hw,
				struct hns3_mbx_pf_to_vf_cmd *req)
{
#define PVID_STATE_OFFSET	1
	uint16_t new_pvid_state = req->msg[PVID_STATE_OFFSET] ?
		HNS3_PORT_BASE_VLAN_ENABLE : HNS3_PORT_BASE_VLAN_DISABLE;
	/*
	 * Currently, hardware doesn't support more than two layers VLAN offload
	 * based on hns3 network engine, which would cause packets loss or wrong
	 * packets for these types of packets. If the hns3 PF kernel ethdev
	 * driver sets the PVID for VF device after initialization of the
	 * related VF device, the PF driver will notify VF driver to update the
	 * PVID configuration state. The VF driver will update the PVID
	 * configuration state immediately to ensure that the VLAN process in Tx
	 * and Rx is correct. But in the window period of this state transition,
	 * packets loss or packets with wrong VLAN may occur.
	 */
	if (hw->port_base_vlan_cfg.state != new_pvid_state) {
		hw->port_base_vlan_cfg.state = new_pvid_state;
		hns3_update_all_queues_pvid_state(hw);
	}
}

static void
hns3_handle_promisc_info(struct hns3_hw *hw, uint16_t promisc_en)
{
	if (!promisc_en) {
		/*
		 * When promisc/allmulti mode is closed by the hns3 PF kernel
		 * ethdev driver for untrusted, modify VF's related status.
		 */
		hns3_warn(hw, "Promisc mode will be closed by host for being "
			      "untrusted.");
		hw->data->promiscuous = 0;
		hw->data->all_multicast = 0;
	}
}

void
hns3_dev_handle_mbx_msg(struct hns3_hw *hw)
{
	struct hns3_mbx_resp_status *resp = &hw->mbx_resp;
	struct hns3_cmq_ring *crq = &hw->cmq.crq;
	struct hns3_mbx_pf_to_vf_cmd *req;
	struct hns3_cmd_desc *desc;
	uint32_t msg_data;
	uint16_t *msg_q;
	uint8_t opcode;
	uint16_t flag;
	uint8_t *temp;
	int i;

	while (!hns3_cmd_crq_empty(hw)) {
		if (rte_atomic16_read(&hw->reset.disable_cmd))
			return;

		desc = &crq->desc[crq->next_to_use];
		req = (struct hns3_mbx_pf_to_vf_cmd *)desc->data;
		opcode = req->msg[0] & 0xff;

		flag = rte_le_to_cpu_16(crq->desc[crq->next_to_use].flag);
		if (unlikely(!hns3_get_bit(flag, HNS3_CMDQ_RX_OUTVLD_B))) {
			hns3_warn(hw,
				  "dropped invalid mailbox message, code = %d",
				  opcode);

			/* dropping/not processing this invalid message */
			crq->desc[crq->next_to_use].flag = 0;
			hns3_mbx_ring_ptr_move_crq(crq);
			continue;
		}

		switch (opcode) {
		case HNS3_MBX_PF_VF_RESP:
			resp->resp_status = hns3_resp_to_errno(req->msg[3]);

			temp = (uint8_t *)&req->msg[4];
			for (i = 0; i < HNS3_MBX_MAX_RESP_DATA_SIZE; i++) {
				resp->additional_info[i] = *temp;
				temp++;
			}
			msg_data = (uint32_t)req->msg[1] << 16 | req->msg[2];
			hns3_update_resp_position(hw, msg_data);
			break;
		case HNS3_MBX_LINK_STAT_CHANGE:
		case HNS3_MBX_ASSERTING_RESET:
			msg_q = hw->arq.msg_q[hw->arq.tail];
			memcpy(&msg_q[0], req->msg,
			       HNS3_MBX_MAX_ARQ_MSG_SIZE * sizeof(uint16_t));
			hns3_mbx_tail_ptr_move_arq(hw->arq);

			hns3_mbx_handler(hw);
			break;
		case HNS3_MBX_PUSH_LINK_STATUS:
			hns3_handle_link_change_event(hw, req);
			break;
		case HNS3_MBX_PUSH_VLAN_INFO:
			/*
			 * When the PVID configuration status of VF device is
			 * changed by the hns3 PF kernel driver, VF driver will
			 * receive this mailbox message from PF driver.
			 */
			hns3_update_port_base_vlan_info(hw, req);
			break;
		case HNS3_MBX_PUSH_PROMISC_INFO:
			/*
			 * When the trust status of VF device changed by the
			 * hns3 PF kernel driver, VF driver will receive this
			 * mailbox message from PF driver.
			 */
			hns3_handle_promisc_info(hw, req->msg[1]);
			break;
		default:
			hns3_err(hw,
				 "VF received unsupported(%d) mbx msg from PF",
				 req->msg[0]);
			break;
		}

		crq->desc[crq->next_to_use].flag = 0;
		hns3_mbx_ring_ptr_move_crq(crq);
	}

	/* Write back CMDQ_RQ header pointer, IMP need this pointer */
	hns3_write_dev(hw, HNS3_CMDQ_RX_HEAD_REG, crq->next_to_use);
}
