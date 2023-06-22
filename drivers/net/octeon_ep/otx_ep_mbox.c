/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <ethdev_pci.h>
#include <rte_ether.h>
#include <rte_kvargs.h>

#include "otx_ep_common.h"
#include "otx_ep_vf.h"
#include "otx2_ep_vf.h"
#include "cnxk_ep_vf.h"
#include "otx_ep_mbox.h"

/*
 * When a new command is implemented, the below table should be updated
 * with new command and it's version info.
 */
static uint32_t otx_ep_cmd_versions[OTX_EP_MBOX_CMD_MAX] = {
	[0 ... OTX_EP_MBOX_CMD_DEV_REMOVE] = OTX_EP_MBOX_VERSION_V1
};

static int
__otx_ep_send_mbox_cmd(struct otx_ep_device *otx_ep,
		       union otx_ep_mbox_word cmd,
		       union otx_ep_mbox_word *rsp)
{
	volatile uint64_t reg_val = 0ull;
	int count = 0;

	cmd.s.type = OTX_EP_MBOX_TYPE_CMD;
	otx2_write64(cmd.u64, otx_ep->hw_addr + CNXK_EP_R_MBOX_VF_PF_DATA(0));

	/* No response for notification messages */
	if (!rsp)
		return 0;

	for (count = 0; count < OTX_EP_MBOX_TIMEOUT_MS; count++) {
		rte_delay_ms(1);
		reg_val = otx2_read64(otx_ep->hw_addr + CNXK_EP_R_MBOX_VF_PF_DATA(0));
		if (reg_val != cmd.u64) {
			rsp->u64 = reg_val;
			break;
		}
	}
	if (count == OTX_EP_MBOX_TIMEOUT_MS) {
		otx_ep_err("mbox send Timeout count:%d\n", count);
		return OTX_EP_MBOX_TIMEOUT_MS;
	}
	if (rsp->s.type != OTX_EP_MBOX_TYPE_RSP_ACK) {
		otx_ep_err("mbox received  NACK from PF\n");
		return OTX_EP_MBOX_CMD_STATUS_NACK;
	}

	rsp->u64 = reg_val;
	return 0;
}

static int
otx_ep_send_mbox_cmd(struct otx_ep_device *otx_ep,
		     union otx_ep_mbox_word cmd,
		     union otx_ep_mbox_word *rsp)
{
	int ret;

	rte_spinlock_lock(&otx_ep->mbox_lock);
	if (otx_ep_cmd_versions[cmd.s.opcode] > otx_ep->mbox_neg_ver) {
		otx_ep_dbg("CMD:%d not supported in Version:%d\n", cmd.s.opcode,
			    otx_ep->mbox_neg_ver);
		rte_spinlock_unlock(&otx_ep->mbox_lock);
		return -EOPNOTSUPP;
	}
	ret = __otx_ep_send_mbox_cmd(otx_ep, cmd, rsp);
	rte_spinlock_unlock(&otx_ep->mbox_lock);
	return ret;
}

static int
otx_ep_mbox_bulk_read(struct otx_ep_device *otx_ep,
		      enum otx_ep_mbox_opcode opcode,
		      uint8_t *data, int32_t *size)
{
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int read_cnt, i = 0, ret;
	int data_len = 0, tmp_len = 0;

	rte_spinlock_lock(&otx_ep->mbox_lock);
	cmd.u64 = 0;
	cmd.s_data.opcode = opcode;
	cmd.s_data.frag = 0;
	/* Send cmd to read data from PF */
	ret = __otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("mbox bulk read data request failed\n");
		rte_spinlock_unlock(&otx_ep->mbox_lock);
		return ret;
	}
	/*  PF sends the data length of requested CMD
	 *  in  ACK
	 */
	memcpy(&data_len, rsp.s_data.data, sizeof(data_len));
	tmp_len = data_len;
	cmd.u64 = 0;
	rsp.u64 = 0;
	cmd.s_data.opcode = opcode;
	cmd.s_data.frag = 1;
	while (data_len) {
		ret = __otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
		if (ret) {
			otx_ep_err("mbox bulk read data request failed\n");
			otx_ep->mbox_data_index = 0;
			memset(otx_ep->mbox_data_buf, 0, OTX_EP_MBOX_MAX_DATA_BUF_SIZE);
			rte_spinlock_unlock(&otx_ep->mbox_lock);
			return ret;
		}
		if (data_len > OTX_EP_MBOX_MAX_DATA_SIZE) {
			data_len -= OTX_EP_MBOX_MAX_DATA_SIZE;
			read_cnt = OTX_EP_MBOX_MAX_DATA_SIZE;
		} else {
			read_cnt = data_len;
			data_len = 0;
		}
		for (i = 0; i < read_cnt; i++) {
			otx_ep->mbox_data_buf[otx_ep->mbox_data_index] =
				rsp.s_data.data[i];
			otx_ep->mbox_data_index++;
		}
		cmd.u64 = 0;
		rsp.u64 = 0;
		cmd.s_data.opcode = opcode;
		cmd.s_data.frag = 1;
	}
	memcpy(data, otx_ep->mbox_data_buf, tmp_len);
	*size = tmp_len;
	otx_ep->mbox_data_index = 0;
	memset(otx_ep->mbox_data_buf, 0, OTX_EP_MBOX_MAX_DATA_BUF_SIZE);
	rte_spinlock_unlock(&otx_ep->mbox_lock);
	return 0;
}

int
otx_ep_mbox_set_mtu(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int ret = 0;

	cmd.u64 = 0;
	cmd.s_set_mtu.opcode = OTX_EP_MBOX_CMD_SET_MTU;
	cmd.s_set_mtu.mtu = mtu;

	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("set MTU failed\n");
		return -EINVAL;
	}
	otx_ep_dbg("mtu set  success mtu %u\n", mtu);

	return 0;
}

int
otx_ep_mbox_set_mac_addr(struct rte_eth_dev *eth_dev,
			 struct rte_ether_addr *mac_addr)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int i, ret;

	cmd.u64 = 0;
	cmd.s_set_mac.opcode = OTX_EP_MBOX_CMD_SET_MAC_ADDR;
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		cmd.s_set_mac.mac_addr[i] = mac_addr->addr_bytes[i];
	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("set MAC address failed\n");
		return -EINVAL;
	}
	otx_ep_dbg("%s VF MAC " RTE_ETHER_ADDR_PRT_FMT "\n",
		    __func__, RTE_ETHER_ADDR_BYTES(mac_addr));
	rte_ether_addr_copy(mac_addr, eth_dev->data->mac_addrs);
	return 0;
}

int
otx_ep_mbox_get_mac_addr(struct rte_eth_dev *eth_dev,
			 struct rte_ether_addr *mac_addr)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int i, ret;

	cmd.u64 = 0;
	cmd.s_set_mac.opcode = OTX_EP_MBOX_CMD_GET_MAC_ADDR;
	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("get MAC address failed\n");
		return -EINVAL;
	}
	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		mac_addr->addr_bytes[i] = rsp.s_set_mac.mac_addr[i];
	otx_ep_dbg("%s VF MAC " RTE_ETHER_ADDR_PRT_FMT "\n",
		    __func__, RTE_ETHER_ADDR_BYTES(mac_addr));
	return 0;
}

int otx_ep_mbox_get_link_status(struct rte_eth_dev *eth_dev,
				uint8_t *oper_up)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int ret;

	cmd.u64 = 0;
	cmd.s_link_status.opcode = OTX_EP_MBOX_CMD_GET_LINK_STATUS;
	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("Get link status failed\n");
		return -EINVAL;
	}
	*oper_up = rsp.s_link_status.status;
	return 0;
}

int otx_ep_mbox_get_link_info(struct rte_eth_dev *eth_dev,
			      struct rte_eth_link *link)
{
	int32_t ret, size;
	struct otx_ep_iface_link_info link_info;
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	memset(&link_info, 0, sizeof(struct otx_ep_iface_link_info));
	ret = otx_ep_mbox_bulk_read(otx_ep, OTX_EP_MBOX_CMD_GET_LINK_INFO,
				      (uint8_t *)&link_info, (int32_t *)&size);
	if (ret) {
		otx_ep_err("Get link info failed\n");
		return ret;
	}
	link->link_status = RTE_ETH_LINK_UP;
	link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link->link_autoneg = (link_info.autoneg ==
			      OTX_EP_LINK_AUTONEG) ? RTE_ETH_LINK_AUTONEG : RTE_ETH_LINK_FIXED;

	link->link_autoneg = link_info.autoneg;
	link->link_speed = link_info.speed;
	return 0;
}

void
otx_ep_mbox_enable_interrupt(struct otx_ep_device *otx_ep)
{
	rte_write64(0x2, (uint8_t *)otx_ep->hw_addr +
		   CNXK_EP_R_MBOX_PF_VF_INT(0));
}

void
otx_ep_mbox_disable_interrupt(struct otx_ep_device *otx_ep)
{
	rte_write64(0x00, (uint8_t *)otx_ep->hw_addr +
		   CNXK_EP_R_MBOX_PF_VF_INT(0));
}

int
otx_ep_mbox_get_max_pkt_len(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int ret;

	rsp.u64 = 0;
	cmd.u64 = 0;
	cmd.s_get_mtu.opcode = OTX_EP_MBOX_CMD_GET_MTU;

	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);
	if (ret)
		return ret;
	return rsp.s_get_mtu.mtu;
}

int otx_ep_mbox_version_check(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	union otx_ep_mbox_word rsp;
	int ret;

	cmd.u64 = 0;
	cmd.s_version.opcode = OTX_EP_MBOX_CMD_VERSION;
	cmd.s_version.version = OTX_EP_MBOX_VERSION_CURRENT;
	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, &rsp);

	/*
	 * VF receives NACK or version info as zero
	 * only if PF driver running old version of Mailbox
	 * In this case VF mailbox version fallbacks to base
	 * mailbox vesrion OTX_EP_MBOX_VERSION_V1.
	 * Default VF mbox_neg_ver is set to OTX_EP_MBOX_VERSION_V1
	 * during initialization of PMD driver.
	 */
	if (ret == OTX_EP_MBOX_CMD_STATUS_NACK || rsp.s_version.version == 0) {
		otx_ep_dbg("VF Mbox version fallback to base version from:%u\n",
			(uint32_t)cmd.s_version.version);
		return 0;
	}
	otx_ep->mbox_neg_ver = (uint32_t)rsp.s_version.version;
	otx_ep_dbg("VF Mbox version:%u Negotiated VF version with PF:%u\n",
		    (uint32_t)cmd.s_version.version,
		    (uint32_t)rsp.s_version.version);
	return 0;
}

int otx_ep_mbox_send_dev_exit(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_ep =
		(struct otx_ep_device *)(eth_dev)->data->dev_private;
	union otx_ep_mbox_word cmd;
	int ret;

	cmd.u64 = 0;
	cmd.s_version.opcode = OTX_EP_MBOX_CMD_DEV_REMOVE;
	ret = otx_ep_send_mbox_cmd(otx_ep, cmd, NULL);
	return ret;
}
