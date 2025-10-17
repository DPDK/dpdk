/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#include <stdbool.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>
#include "enetc_logs.h"
#include "enetc.h"

#define ENETC_CRC_TABLE_SIZE		256
#define ENETC_POLY			0x1021
#define ENETC_CRC_INIT			0xffff
#define ENETC_BYTE_SIZE			8
#define ENETC_MSB_BIT			0x8000

uint16_t enetc_crc_table[ENETC_CRC_TABLE_SIZE];
bool enetc_crc_gen;

/* Supported Rx offloads */
static uint64_t dev_vf_rx_offloads_sup =
	RTE_ETH_RX_OFFLOAD_VLAN_FILTER;

static void
enetc_gen_crc_table(void)
{
	uint16_t crc = 0;
	uint16_t c;

	for (int i = 0; i < ENETC_CRC_TABLE_SIZE; i++) {
		crc = 0;
		c = i << ENETC_BYTE_SIZE;
		for (int j = 0; j < ENETC_BYTE_SIZE; j++) {
			if ((crc ^ c) & ENETC_MSB_BIT)
				crc = (crc << 1) ^ ENETC_POLY;
			else
				crc = crc << 1;
			c = c << 1;
		}

		enetc_crc_table[i] = crc;
	}

	enetc_crc_gen = true;
}

static uint16_t
enetc_crc_calc(uint16_t crc, const uint8_t *buffer, size_t len)
{
	uint8_t data;

	while (len--) {
		data = *buffer;
		crc = (crc << 8) ^ enetc_crc_table[((crc >> 8) ^ data) & 0xff];
		buffer++;
	}
	return crc;
}

static int
enetc4_vf_dev_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info)
{
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	ret = enetc4_dev_infos_get(dev, dev_info);
	if (ret)
		return ret;

	dev_info->max_mtu = dev_info->max_rx_pktlen - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN);
	dev_info->max_mac_addrs = ENETC4_MAC_ENTRIES;
	dev_info->rx_offload_capa |= dev_vf_rx_offloads_sup;

	return 0;
}

int
enetc4_vf_dev_stop(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_dev_start(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_bdr *rx_ring;
	uint8_t i;

	PMD_INIT_FUNC_TRACE();
	stats->ipackets = enetc4_rd(enetc_hw, ENETC4_SIRFRM0);
	stats->opackets = enetc4_rd(enetc_hw, ENETC4_SITFRM0);
	stats->ibytes = enetc4_rd(enetc_hw, ENETC4_SIROCT0);
	stats->obytes = enetc4_rd(enetc_hw, ENETC4_SITOCT0);
	stats->oerrors = enetc4_rd(enetc_hw, ENETC4_SITDFCR);
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx_ring = dev->data->rx_queues[i];
		stats->ierrors += rx_ring->ierrors;
	}
	return 0;
}


static void
enetc_msg_vf_fill_common_hdr(struct enetc_msg_swbd *msg,
					uint8_t class_id, uint8_t cmd_id, uint8_t proto_ver,
					uint8_t len, uint8_t cookie)
{
	struct enetc_msg_cmd_header *hdr = msg->vaddr;

	hdr->class_id = class_id;
	hdr->cmd_id = cmd_id;
	hdr->proto_ver = proto_ver;
	hdr->len = len;
	hdr->cookie = cookie;
	/* Incrementing msg 2 bytes ahead as the first two bytes are for CRC */
	hdr->csum = rte_cpu_to_be_16(enetc_crc_calc(ENETC_CRC_INIT,
				(uint8_t *)msg->vaddr + sizeof(uint16_t),
				msg->size - sizeof(uint16_t)));

	dcbf(hdr);
}

/* Messaging */
static void
enetc4_msg_vsi_write_msg(struct enetc_hw *hw,
		struct enetc_msg_swbd *msg)
{
	uint32_t val;

	val = enetc_vsi_set_msize(msg->size) | lower_32_bits(msg->dma);
	enetc_wr(hw, ENETC4_VSIMSGSNDAR1, upper_32_bits(msg->dma));
	enetc_wr(hw, ENETC4_VSIMSGSNDAR0, val);
}

static void
enetc4_msg_vsi_reply_msg(struct enetc_hw *enetc_hw, struct enetc_psi_reply_msg *reply_msg)
{
	int vsimsgsr;
	int8_t class_id = 0;
	uint8_t status = 0;

	vsimsgsr = enetc_rd(enetc_hw, ENETC4_VSIMSGSR);

	/* Extracting 8 bits of message result in class_id */
	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 8) & 0xff);

	/* Extracting 4 bits of message result in status */
	status |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 4) & 0xf);

	reply_msg->class_id = class_id;
	reply_msg->status = status;
}

static void
enetc4_msg_get_psi_msg(struct enetc_hw *enetc_hw, struct enetc_psi_reply_msg *reply_msg)
{
	int vsimsgrr;
	int8_t class_id = 0;
	uint8_t status = 0;

	vsimsgrr = enetc_rd(enetc_hw, ENETC4_VSIMSGRR);

	/* Extracting 8 bits of message result in class_id */
	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgrr) >> 8) & 0xff);

	/* Extracting 4 bits of message result in status */
	status |= ((ENETC_SIMSGSR_GET_MC(vsimsgrr) >> 4) & 0xf);

	reply_msg->class_id = class_id;
	reply_msg->status = status;
}

static void
enetc4_process_psi_msg(struct rte_eth_dev *eth_dev, struct enetc_hw *enetc_hw)
{
	struct enetc_psi_reply_msg *msg;
	struct rte_eth_link link;
	int ret = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		return;
	}

	rte_eth_linkstatus_get(eth_dev, &link);
	enetc4_msg_get_psi_msg(enetc_hw, msg);

	if (msg->class_id == ENETC_CLASS_ID_LINK_STATUS) {
		switch (msg->status) {
		case ENETC_LINK_UP:
			ENETC_PMD_DEBUG("Link is up");
			link.link_status = RTE_ETH_LINK_UP;
			break;
		case ENETC_LINK_DOWN:
			ENETC_PMD_DEBUG("Link is down");
			link.link_status = RTE_ETH_LINK_DOWN;
			break;
		default:
			ENETC_PMD_ERR("Unknown link status 0x%x", msg->status);
			break;
		}
		ret = rte_eth_linkstatus_set(eth_dev, &link);
		if (!ret)
			ENETC_PMD_DEBUG("Link status has been changed");

		/* Process user registered callback */
		rte_eth_dev_callback_process(eth_dev,
			RTE_ETH_EVENT_INTR_LSC, NULL);
	} else {
		ENETC_PMD_ERR("Wrong message 0x%x", msg->class_id);
	}

	rte_free(msg);
}

static int
enetc4_msg_vsi_send(struct enetc_hw *enetc_hw, struct enetc_msg_swbd *msg)
{
	int timeout = ENETC4_DEF_VSI_WAIT_TIMEOUT_UPDATE;
	int delay_us = ENETC4_DEF_VSI_WAIT_DELAY_UPDATE;
	uint8_t class_id = 0;
	int err = 0;
	int vsimsgsr;

	enetc4_msg_vsi_write_msg(enetc_hw, msg);

	do {
		vsimsgsr = enetc_rd(enetc_hw, ENETC4_VSIMSGSR);
		if (!(vsimsgsr & ENETC4_VSIMSGSR_MB))
			break;
		rte_delay_us(delay_us);
	} while (--timeout);

	if (!timeout) {
		ENETC_PMD_ERR("Message not processed by PSI");
		return -ETIMEDOUT;
	}
	/* check for message delivery error */
	if (vsimsgsr & ENETC4_VSIMSGSR_MS) {
		ENETC_PMD_ERR("Transfer error when copying the data");
		return -EIO;
	}

	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 8) & 0xff);

	/* Check the user-defined completion status. */
	if (class_id != ENETC_MSG_CLASS_ID_CMD_SUCCESS) {
		switch (class_id) {
		case ENETC_MSG_CLASS_ID_PERMISSION_DENY:
			ENETC_PMD_ERR("Permission denied");
			err = -EACCES;
			break;
		case ENETC_MSG_CLASS_ID_CMD_NOT_SUPPORT:
			ENETC_PMD_ERR("Command not supported");
			err = -EOPNOTSUPP;
			break;
		case ENETC_MSG_CLASS_ID_PSI_BUSY:
			ENETC_PMD_ERR("PSI Busy");
			err = -EBUSY;
			break;
		case ENETC_MSG_CLASS_ID_CMD_TIMEOUT:
			ENETC_PMD_ERR("Command timeout");
			err = -ETIME;
			break;
		case ENETC_MSG_CLASS_ID_CRC_ERROR:
			ENETC_PMD_ERR("CRC error");
			err = -EIO;
			break;
		case ENETC_MSG_CLASS_ID_PROTO_NOT_SUPPORT:
			ENETC_PMD_ERR("Protocol Version not supported");
			err = -EOPNOTSUPP;
			break;
		case ENETC_MSG_CLASS_ID_INVALID_MSG_LEN:
			ENETC_PMD_ERR("Invalid message length");
			err = -EINVAL;
			break;
		case ENETC_CLASS_ID_MAC_FILTER:
		case ENETC_CLASS_ID_LINK_STATUS:
		case ENETC_CLASS_ID_LINK_SPEED:
			break;
		default:
			err = -EIO;
		}
	}

	return err;
}

static int
enetc4_vf_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_cmd_set_primary_mac *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;

	PMD_INIT_FUNC_TRACE();
	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_primary_mac),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_primary_mac *)msg->vaddr;

	cmd->count = 0;
	memcpy(&cmd->addr.addr_bytes, addr, sizeof(struct rte_ether_addr));

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
					ENETC_CMD_ID_SET_PRIMARY_MAC, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(enetc_hw, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_MAC_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_MAC_ADDR:
			ENETC_PMD_ERR("Invalid MAC address");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_MAC_ADDR:
			ENETC_PMD_ERR("Duplicate MAC address");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)&cmd->addr,
			&dev->data->mac_addrs[0]);

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_promisc_send_message(struct rte_eth_dev *dev, bool promisc_en)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_cmd_set_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_promisc), ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_promisc *)msg->vaddr;

	/* op_type is based on the result of message format
	 *    7  6      1       0
	      type   promisc  flush
	 */

	if (promisc_en)
		cmd->op_type = ENETC_PROMISC_ENABLE;
	else
		cmd->op_type = ENETC_PROMISC_DISABLE;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
				ENETC_CMD_ID_SET_MAC_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_allmulti_send_message(struct rte_eth_dev *dev, bool mc_promisc)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_cmd_set_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_promisc),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_promisc *)msg->vaddr;

	/* op_type is based on the result of message format
	 *    7  6      1       0
	      type   promisc  flush
	 */

	if (mc_promisc)
		cmd->op_type = ENETC_ALLMULTI_PROMISC_EN;
	else
		cmd->op_type = ENETC_ALLMULTI_PROMISC_DIS;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
				ENETC_CMD_ID_SET_MAC_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}


static int
enetc4_vf_multicast_enable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_allmulti_send_message(dev, true);
	if (err) {
		ENETC_PMD_ERR("Failed to enable multicast promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_multicast_disable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_allmulti_send_message(dev, false);
	if (err) {
		ENETC_PMD_ERR("Failed to disable multicast promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_promisc_enable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_promisc_send_message(dev, true);
	if (err) {
		ENETC_PMD_ERR("Failed to enable promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_promisc_disable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_promisc_send_message(dev, false);
	if (err) {
		ENETC_PMD_ERR("Failed to disable promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_get_link_status(struct rte_eth_dev *dev, struct enetc_psi_reply_msg *reply_msg)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_status),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_STATUS,
			ENETC_CMD_ID_GET_LINK_STATUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(enetc_hw, reply_msg);
end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_get_link_speed(struct rte_eth_dev *dev, struct enetc_psi_reply_msg *reply_msg)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_speed),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_SPEED,
			ENETC_CMD_ID_GET_LINK_SPEED, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(enetc_hw, reply_msg);
end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct enetc_psi_reply_msg *reply_msg;
	struct rte_eth_link link;
	int err;

	PMD_INIT_FUNC_TRACE();
	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	memset(&link, 0, sizeof(struct rte_eth_link));

	err = enetc4_vf_get_link_status(dev, reply_msg);
	if (err) {
		ENETC_PMD_ERR("Failed to get link status");
		rte_free(reply_msg);
		return err;
	}

	if (reply_msg->class_id == ENETC_CLASS_ID_LINK_STATUS) {
		switch (reply_msg->status) {
		case ENETC_LINK_UP:
			link.link_status = RTE_ETH_LINK_UP;
			break;
		case ENETC_LINK_DOWN:
			link.link_status = RTE_ETH_LINK_DOWN;
			break;
		default:
			ENETC_PMD_ERR("Unknown link status");
			break;
		}
	} else {
		ENETC_PMD_ERR("Wrong reply message");
		return -1;
	}

	err = enetc4_vf_get_link_speed(dev, reply_msg);
	if (err) {
		ENETC_PMD_ERR("Failed to get link speed");
		rte_free(reply_msg);
		return err;
	}

	if (reply_msg->class_id == ENETC_CLASS_ID_LINK_SPEED) {
		switch (reply_msg->status) {
		case ENETC_SPEED_UNKNOWN:
			ENETC_PMD_DEBUG("Speed unknown");
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			break;
		case ENETC_SPEED_10_HALF_DUPLEX:
			link.link_speed = RTE_ETH_SPEED_NUM_10M;
			link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
			break;
		case ENETC_SPEED_10_FULL_DUPLEX:
			link.link_speed = RTE_ETH_SPEED_NUM_10M;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_100_HALF_DUPLEX:
			link.link_speed = RTE_ETH_SPEED_NUM_100M;
			link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
			break;
		case ENETC_SPEED_100_FULL_DUPLEX:
			link.link_speed = RTE_ETH_SPEED_NUM_100M;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_1000:
			link.link_speed = RTE_ETH_SPEED_NUM_1G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_2500:
			link.link_speed = RTE_ETH_SPEED_NUM_2_5G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_5000:
			link.link_speed = RTE_ETH_SPEED_NUM_5G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_10G:
			link.link_speed = RTE_ETH_SPEED_NUM_10G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_25G:
			link.link_speed = RTE_ETH_SPEED_NUM_25G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_50G:
			link.link_speed = RTE_ETH_SPEED_NUM_50G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_100G:
			link.link_speed = RTE_ETH_SPEED_NUM_100G;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case ENETC_SPEED_NOT_SUPPORTED:
			ENETC_PMD_DEBUG("Speed not supported");
			link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
			break;
		default:
			ENETC_PMD_ERR("Unknown speed status");
			break;
		}
	} else {
		ENETC_PMD_ERR("Wrong reply message");
		return -1;
	}

	link.link_autoneg = 1;

	rte_eth_linkstatus_set(dev, &link);

	rte_free(reply_msg);
	return 0;
}

static int
enetc4_vf_vlan_promisc(struct rte_eth_dev *dev, bool promisc_en)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_cmd_set_vlan_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_vlan_promisc),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_vlan_promisc *)msg->vaddr;
	/* op is based on the result of message format
	 *	   1	  0
	 *	promisc	flush
	 */

	if (promisc_en)
		cmd->op = ENETC_PROMISC_VLAN_ENABLE;
	else
		cmd->op = ENETC_PROMISC_VLAN_DISABLE;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_CMD_ID_SET_VLAN_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *addr,
			uint32_t index __rte_unused, uint32_t pool __rte_unused)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_cmd_set_primary_mac *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	if (!rte_is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_primary_mac),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	cmd = (struct enetc_msg_cmd_set_primary_mac *)msg->vaddr;
	memcpy(&cmd->addr.addr_bytes, addr, sizeof(struct rte_ether_addr));
	cmd->count = 1;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
			ENETC_MSG_ADD_EXACT_MAC_ENTRIES, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(enetc_hw, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_MAC_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_MAC_ADDR:
			ENETC_PMD_ERR("Invalid MAC address");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_MAC_ADDR:
			ENETC_PMD_ERR("Duplicate MAC address");
			err = -EINVAL;
			break;
		case ENETC_MAC_FILTER_NO_RESOURCE:
			ENETC_PMD_ERR("Not enough exact-match entries available");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int enetc4_vf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_vlan_exact_filter *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_vlan_exact_filter),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	cmd = (struct enetc_msg_vlan_exact_filter *)msg->vaddr;
	cmd->vlan_count = 1;
	cmd->vlan_id = vlan_id;

	/* TPID 2-bit encoding value is taken from the H/W block guide:
	 *	00b Standard C-VLAN 0x8100
	 *	01b Standard S-VLAN 0x88A8
	 *	10b Custom VLAN as defined by CVLANR1[ETYPE]
	 *	11b Custom VLAN as defined by CVLANR2[ETYPE]
	 * Currently Standard C-VLAN is supported. To support others in future.
	 */
	cmd->tpid = 0;

	if (on) {
		enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_MSG_ADD_EXACT_VLAN_ENTRIES, 0, 0, 0);
	} else {
		enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_MSG_REMOVE_EXACT_VLAN_ENTRIES, 0, 0, 0);
	}

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(enetc_hw, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_VLAN_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_VLAN_ENTRY:
			ENETC_PMD_ERR("VLAN entry not valid");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_VLAN_ENTRY:
			ENETC_PMD_ERR("Duplicated VLAN entry");
			err = -EINVAL;
			break;
		case ENETC_VLAN_ENTRY_NOT_FOUND:
			ENETC_PMD_ERR("VLAN entry not found");
			err = -EINVAL;
			break;
		case ENETC_VLAN_NO_RESOURCE:
			ENETC_PMD_ERR("Not enough exact-match entries available");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int enetc4_vf_vlan_offload_set(struct rte_eth_dev *dev, int mask __rte_unused)
{
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.offloads) {
		ENETC_PMD_DEBUG("VLAN filter table entry inserted:"
					"Disabling VLAN promisc mode");
		err = enetc4_vf_vlan_promisc(dev, false);
		if (err) {
			ENETC_PMD_ERR("Added VLAN filter table entry:"
					"Failed to disable promiscuous mode");
			return err;
		}
	} else {
		ENETC_PMD_DEBUG("Enabling VLAN promisc mode");
		err = enetc4_vf_vlan_promisc(dev, true);
		if (err) {
			ENETC_PMD_ERR("Vlan filter table empty:"
					"Failed to enable promiscuous mode");
			return err;
		}
	}

	return 0;
}

static int
enetc4_vf_mtu_set(struct rte_eth_dev *dev __rte_unused, uint16_t mtu __rte_unused)
{
	return 0;
}

static int
enetc4_vf_link_register_notif(struct rte_eth_dev *dev, bool enable)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_msg_swbd *msg;
	struct rte_eth_link link;
	uint32_t msg_size;
	int err = 0;
	uint8_t cmd;

	PMD_INIT_FUNC_TRACE();
	memset(&link, 0, sizeof(struct rte_eth_link));
	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_status), ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	if (enable)
		cmd = ENETC_CMD_ID_REGISTER_LINK_NOTIF;
	else
		cmd = ENETC_CMD_ID_UNREGISTER_LINK_NOTIF;
	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_STATUS,
			cmd, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(enetc_hw, msg);
	if (err)
		ENETC_PMD_ERR("VSI msg error for link status notification");

	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);

	return err;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_vf_id_enetc4_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NXP, ENETC4_DEV_ID_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc4_vf_ops = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_vf_dev_start,
	.dev_stop             = enetc4_vf_dev_stop,
	.dev_close            = enetc4_dev_close,
	.stats_get            = enetc4_vf_stats_get,
	.dev_infos_get        = enetc4_vf_dev_infos_get,
	.mtu_set              = enetc4_vf_mtu_set,
	.mac_addr_set         = enetc4_vf_set_mac_addr,
	.mac_addr_add	      = enetc4_vf_mac_addr_add,
	.promiscuous_enable   = enetc4_vf_promisc_enable,
	.promiscuous_disable  = enetc4_vf_promisc_disable,
	.allmulticast_enable  = enetc4_vf_multicast_enable,
	.allmulticast_disable = enetc4_vf_multicast_disable,
	.link_update	      = enetc4_vf_link_update,
	.vlan_filter_set      = enetc4_vf_vlan_filter_set,
	.vlan_offload_set     = enetc4_vf_vlan_offload_set,
	.rx_queue_setup       = enetc4_rx_queue_setup,
	.rx_queue_start       = enetc4_rx_queue_start,
	.rx_queue_stop        = enetc4_rx_queue_stop,
	.rx_queue_release     = enetc4_rx_queue_release,
	.tx_queue_setup       = enetc4_tx_queue_setup,
	.tx_queue_start       = enetc4_tx_queue_start,
	.tx_queue_stop        = enetc4_tx_queue_stop,
	.tx_queue_release     = enetc4_tx_queue_release,
	.dev_supported_ptypes_get = enetc4_supported_ptypes_get,
};

static int
enetc4_vf_mac_init(struct enetc_eth_hw *hw, struct rte_eth_dev *eth_dev)
{
	uint32_t *mac = (uint32_t *)hw->mac.addr;
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;
	char vf_eth_name[ENETC_ETH_NAMESIZE];

	PMD_INIT_FUNC_TRACE();

	/* Enabling Station Interface */
	enetc4_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);
	*mac = (uint32_t)enetc_rd(enetc_hw, ENETC_SIPMAR0);
	high_mac = (uint32_t)*mac;
	mac++;
	*mac = (uint16_t)enetc_rd(enetc_hw, ENETC_SIPMAR1);
	low_mac = (uint16_t)*mac;

	if ((high_mac | low_mac) == 0) {
		char *first_byte;
		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				 "set random MAC");
		mac = (uint32_t *)hw->mac.addr;
		*mac = (uint32_t)rte_rand();
		first_byte = (char *)mac;
		*first_byte &= 0xfe;    /* clear multicast bit */
		*first_byte |= 0x02;    /* set local assignment bit (IEEE802) */
		enetc4_port_wr(enetc_hw, ENETC4_PMAR0, *mac);
		mac++;
		*mac = (uint16_t)rte_rand();
		enetc4_port_wr(enetc_hw, ENETC4_PMAR1, *mac);
		enetc_print_ethaddr("New address: ",
			(const struct rte_ether_addr *)hw->mac.addr);
	}

	/* Allocate memory for storing MAC addresses */
	snprintf(vf_eth_name, sizeof(vf_eth_name), "enetc4_vf_eth_%d", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = rte_zmalloc(vf_eth_name,
					RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      RTE_ETHER_ADDR_LEN * 1);
		return -ENOMEM;
	}

	if (!enetc_crc_gen)
		enetc_gen_crc_table();

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			     &eth_dev->data->mac_addrs[0]);

	return 0;
}

static void
enetc_vf_enable_mr_int(struct enetc_hw *hw, bool en)
{
	uint32_t val;

	val = enetc_rd(hw, ENETC4_VSIIER);
	val &= ~ENETC4_VSIIER_MRIE;
	val |= (en) ? ENETC4_VSIIER_MRIE : 0;
	enetc_wr(hw, ENETC4_VSIIER, val);
	ENETC_PMD_DEBUG("Interrupt enable status (VSIIER) = 0x%x", val);
}

static void
enetc4_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t status;

	/* Disable interrupts before process */
	enetc_vf_enable_mr_int(enetc_hw, false);

	status = enetc_rd(enetc_hw, ENETC4_VSIIDR);
	ENETC_PMD_DEBUG("Got INTR VSIIDR status = 0x%0x", status);
	/* Check for PSI to VSI message interrupt */
	if (!(status & ENETC4_VSIIER_MRIE)) {
		ENETC_PMD_ERR("Interrupt is not PSI to VSI");
		goto intr_clear;
	}

	enetc4_process_psi_msg(eth_dev, enetc_hw);
intr_clear:
	/* Clear Interrupts */
	enetc_wr(enetc_hw, ENETC4_VSIIDR, 0xffffffff);
	enetc_vf_enable_mr_int(enetc_hw, true);
}

static int
enetc4_vf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
			    ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int error = 0;
	uint32_t si_cap;
	struct enetc_hw *enetc_hw = &hw->hw;

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc4_vf_ops;
	enetc4_dev_hw_init(eth_dev);

	si_cap = enetc_rd(enetc_hw, ENETC_SICAPR0);
	hw->max_tx_queues = si_cap & ENETC_SICAPR0_BDR_MASK;
	hw->max_rx_queues = (si_cap >> 16) & ENETC_SICAPR0_BDR_MASK;

	ENETC_PMD_DEBUG("Max RX queues = %d Max TX queues = %d",
			hw->max_rx_queues, hw->max_tx_queues);
	error = enetc4_vf_mac_init(hw, eth_dev);
	if (error != 0) {
		ENETC_PMD_ERR("MAC initialization failed!!");
		return -1;
	}

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	/* update link */
	enetc4_vf_link_update(eth_dev, 0);

	return 0;
}

static int
enetc4_vf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		    struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc4_vf_dev_init);
}

int
enetc4_vf_dev_intr(struct rte_eth_dev *eth_dev, bool enable)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	if (!(intr_handle && rte_intr_fd_get(intr_handle))) {
		ENETC_PMD_ERR("No INTR handle");
		return -1;
	}
	if (enable) {
		/* if the interrupts were configured on this devices*/
		ret = rte_intr_callback_register(intr_handle,
				enetc4_dev_interrupt_handler, eth_dev);
		if (ret) {
			ENETC_PMD_ERR("Failed to register INTR callback %d", ret);
			return ret;
		}
		/* set one IRQ entry for PSI-to-VSI messaging */
		/* Vector index 0 */
		enetc_wr(enetc_hw, ENETC4_SIMSIVR, ENETC4_SI_INT_IDX);

		/* enable uio/vfio intr/eventfd mapping */
		ret = rte_intr_enable(intr_handle);
		if (ret) {
			ENETC_PMD_ERR("Failed to enable INTR %d", ret);
			goto intr_enable_fail;
		}

		/* Enable message received interrupt */
		enetc_vf_enable_mr_int(enetc_hw, true);
		ret = enetc4_vf_link_register_notif(eth_dev, true);
		if (ret) {
			ENETC_PMD_ERR("Failed to register link notifications %d", ret);
			goto disable;
		}

		return ret;
	}

	ret = enetc4_vf_link_register_notif(eth_dev, false);
	if (ret)
		ENETC_PMD_WARN("Failed to un-register link notification %d", ret);
disable:
	enetc_vf_enable_mr_int(enetc_hw, false);
	ret = rte_intr_disable(intr_handle);
	if (ret)
		ENETC_PMD_WARN("Failed to disable INTR %d", ret);
intr_enable_fail:
	rte_intr_callback_unregister(intr_handle,
			enetc4_dev_interrupt_handler, eth_dev);

	return ret;
}

static struct rte_pci_driver rte_enetc4_vf_pmd = {
	.id_table = pci_vf_id_enetc4_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = enetc4_vf_pci_probe,
	.remove = enetc4_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc4_vf, rte_enetc4_vf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc4_vf, pci_vf_id_enetc4_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc4_vf, "* igb_uio | uio_pci_generic");
RTE_LOG_REGISTER_DEFAULT(enetc4_vf_logtype_pmd, NOTICE);
