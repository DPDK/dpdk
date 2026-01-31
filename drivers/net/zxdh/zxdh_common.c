/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <string.h>

#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_msg.h"
#include "zxdh_common.h"
#include "zxdh_pci.h"

#define ZXDH_MSG_RSP_SIZE_MAX         512

#define ZXDH_COMMON_TABLE_READ        0
#define ZXDH_COMMON_TABLE_WRITE       1

#define ZXDH_COMMON_FIELD_PHYPORT     6
#define ZXDH_COMMON_FIELD_DATACH      3

#define ZXDH_RSC_TBL_CONTENT_LEN_MAX  (257 * 2)

#define ZXDH_REPS_HEADER_OFFSET       4
#define ZXDH_TBL_MSG_PRO_SUCCESS      0xaa

struct __rte_packed_begin zxdh_common_msg {
	uint8_t  type;    /* 0:read table 1:write table */
	uint8_t  field;
	uint16_t pcie_id;
	uint16_t slen;    /* Data length for write table */
	uint16_t reserved;
} __rte_packed_end;

struct __rte_packed_begin zxdh_common_rsp_hdr {
	uint8_t  rsp_status;
	uint16_t rsp_len;
	uint8_t  reserved;
	uint8_t  payload_status;
	uint8_t  rsv;
	uint16_t payload_len;
} __rte_packed_end;

struct zxdh_tbl_msg_header {
	uint8_t  type;
	uint8_t  field;
	uint16_t pcieid;
	uint16_t slen;
	uint16_t rsv;
};

struct zxdh_tbl_msg_reps_header {
	uint8_t  check;
	uint8_t  rsv;
	uint16_t len;
};

static int32_t
zxdh_fill_common_msg(struct zxdh_hw *hw, struct zxdh_pci_bar_msg *desc,
		uint8_t type, uint8_t field,
		void *buff, uint16_t buff_size)
{
	uint64_t msg_len = sizeof(struct zxdh_common_msg) + buff_size;

	desc->payload_addr = rte_zmalloc(NULL, msg_len, 0);
	if (unlikely(desc->payload_addr == NULL)) {
		PMD_DRV_LOG(ERR, "Failed to allocate msg_data");
		return -ENOMEM;
	}
	desc->payload_len = msg_len;
	struct zxdh_common_msg *msg_data = desc->payload_addr;

	msg_data->type = type;
	msg_data->field = field;
	msg_data->pcie_id = hw->pcie_id;
	msg_data->slen = buff_size;
	if (buff_size != 0)
		rte_memcpy(msg_data + 1, buff, buff_size);

	return 0;
}

static int32_t
zxdh_send_command(struct zxdh_hw *hw, struct zxdh_pci_bar_msg *desc,
		enum ZXDH_BAR_MODULE_ID module_id,
		struct zxdh_msg_recviver_mem *msg_rsp)
{
	desc->virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_CTRLCH_OFFSET);
	desc->src = hw->is_pf ? ZXDH_MSG_CHAN_END_PF : ZXDH_MSG_CHAN_END_VF;
	desc->dst = ZXDH_MSG_CHAN_END_RISC;
	desc->module_id = module_id;
	desc->src_pcieid = hw->pcie_id;

	msg_rsp->buffer_len  = ZXDH_MSG_RSP_SIZE_MAX;
	msg_rsp->recv_buffer = rte_zmalloc(NULL, msg_rsp->buffer_len, 0);
	if (unlikely(msg_rsp->recv_buffer == NULL)) {
		PMD_DRV_LOG(ERR, "Failed to allocate messages response");
		return -ENOMEM;
	}

	if (zxdh_bar_chan_sync_msg_send(desc, msg_rsp) != ZXDH_BAR_MSG_OK) {
		PMD_DRV_LOG(ERR, "Failed to send sync messages or receive response");
		rte_free(msg_rsp->recv_buffer);
		return -1;
	}

	return 0;
}

static int32_t
zxdh_common_rsp_check(struct zxdh_msg_recviver_mem *msg_rsp,
		void *buff, uint16_t len)
{
	struct zxdh_common_rsp_hdr *rsp_hdr = (struct zxdh_common_rsp_hdr *)msg_rsp->recv_buffer;

	if (rsp_hdr->payload_status != 0xaa || rsp_hdr->payload_len != len) {
		PMD_DRV_LOG(ERR, "Common response is invalid, status:0x%x rsp_len:%d",
					rsp_hdr->payload_status, rsp_hdr->payload_len);
		return -1;
	}
	if (len != 0)
		rte_memcpy(buff, rsp_hdr + 1, len);

	return 0;
}

static int32_t
zxdh_common_table_read(struct zxdh_hw *hw, uint8_t field,
		void *buff, uint16_t buff_size)
{
	struct zxdh_msg_recviver_mem msg_rsp;
	struct zxdh_pci_bar_msg desc;
	int32_t ret = 0;

	if (!hw->msg_chan_init) {
		PMD_DRV_LOG(ERR, "Bar messages channel not initialized");
		return -1;
	}

	ret = zxdh_fill_common_msg(hw, &desc, ZXDH_COMMON_TABLE_READ, field, NULL, 0);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to fill common msg");
		return ret;
	}

	ret = zxdh_send_command(hw, &desc, ZXDH_BAR_MODULE_TBL, &msg_rsp);
	if (ret != 0)
		goto free_msg_data;

	ret = zxdh_common_rsp_check(&msg_rsp, buff, buff_size);
	if (ret != 0)
		goto free_rsp_data;

free_rsp_data:
	rte_free(msg_rsp.recv_buffer);
free_msg_data:
	rte_free(desc.payload_addr);
	return ret;
}

int32_t
zxdh_phyport_get(struct rte_eth_dev *dev, uint8_t *phyport)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	int32_t ret = zxdh_common_table_read(hw, ZXDH_COMMON_FIELD_PHYPORT,
					(void *)phyport, sizeof(*phyport));
	return ret;
}

static inline void
zxdh_fill_res_para(struct rte_eth_dev *dev, struct zxdh_res_para *param)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	param->pcie_id   = hw->pcie_id;
	param->virt_addr = hw->bar_addr[0] + ZXDH_CTRLCH_OFFSET;
	param->src_type  = hw->is_pf ? ZXDH_MSG_CHAN_END_PF : ZXDH_MSG_CHAN_END_VF;
}

static int
zxdh_get_res_info(struct zxdh_res_para *dev, uint8_t field, uint8_t *res, uint16_t *len)
{
	struct zxdh_pci_bar_msg in = {0};
	uint8_t recv_buf[ZXDH_RSC_TBL_CONTENT_LEN_MAX + 8] = {0};
	int ret = 0;

	if (!res || !dev)
		return ZXDH_BAR_MSG_ERR_NULL;

	struct zxdh_tbl_msg_header tbl_msg = {
		.type = ZXDH_TBL_TYPE_READ,
		.field = field,
		.pcieid = dev->pcie_id,
		.slen = 0,
		.rsv = 0,
	};

	in.virt_addr = dev->virt_addr;
	in.payload_addr = &tbl_msg;
	in.payload_len = sizeof(tbl_msg);
	in.src = dev->src_type;
	in.dst = ZXDH_MSG_CHAN_END_RISC;
	in.module_id = ZXDH_BAR_MODULE_TBL;
	in.src_pcieid = dev->pcie_id;

	struct zxdh_msg_recviver_mem result = {
		.recv_buffer = recv_buf,
		.buffer_len = sizeof(recv_buf),
	};
	ret = zxdh_bar_chan_sync_msg_send(&in, &result);

	if (ret != ZXDH_BAR_MSG_OK) {
		PMD_DRV_LOG(ERR,
			"send sync_msg failed. pcieid: 0x%x, ret: %d", dev->pcie_id, ret);
		return ret;
	}
	struct zxdh_tbl_msg_reps_header *tbl_reps =
		(struct zxdh_tbl_msg_reps_header *)(recv_buf + ZXDH_REPS_HEADER_OFFSET);

	if (tbl_reps->check != ZXDH_TBL_MSG_PRO_SUCCESS) {
		PMD_DRV_LOG(ERR,
			"get resource_field failed. pcieid: 0x%x, ret: %d", dev->pcie_id, ret);
		return ret;
	}
	*len = tbl_reps->len;
	rte_memcpy(res, (recv_buf + ZXDH_REPS_HEADER_OFFSET +
		sizeof(struct zxdh_tbl_msg_reps_header)), *len);
	return ret;
}

static int
zxdh_get_res_panel_id(struct zxdh_res_para *in, uint8_t *panel_id)
{
	uint8_t reps = 0;
	uint16_t reps_len = 0;

	if (zxdh_get_res_info(in, ZXDH_TBL_FIELD_PNLID, &reps, &reps_len) != ZXDH_BAR_MSG_OK)
		return -1;

	*panel_id = reps;
	return ZXDH_BAR_MSG_OK;
}

int32_t
zxdh_panelid_get(struct rte_eth_dev *dev, uint8_t *panelid)
{
	struct zxdh_res_para param;

	zxdh_fill_res_para(dev, &param);
	int32_t ret = zxdh_get_res_panel_id(&param, panelid);
	return ret;
}

static int
zxdh_get_res_hash_id(struct zxdh_res_para *in, uint8_t *hash_id)
{
	uint8_t reps = 0;
	uint16_t reps_len = 0;

	if (zxdh_get_res_info(in, ZXDH_TBL_FIELD_HASHID, &reps, &reps_len) != ZXDH_BAR_MSG_OK)
		return -1;

	*hash_id = reps;
	return ZXDH_BAR_MSG_OK;
}

int32_t
zxdh_hashidx_get(struct rte_eth_dev *dev, uint8_t *hash_idx)
{
	struct zxdh_res_para param;

	zxdh_fill_res_para(dev, &param);
	int32_t ret = zxdh_get_res_hash_id(&param, hash_idx);

	return ret;
}

uint32_t
zxdh_read_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t baseaddr = (uint64_t)(hw->bar_addr[bar]);
	uint32_t val      = *((volatile uint32_t *)(baseaddr + reg));
	return val;
}

void
zxdh_write_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg, uint32_t val)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t baseaddr = (uint64_t)(hw->bar_addr[bar]);
	*((volatile uint32_t *)(baseaddr + reg)) = val;
}

static bool
zxdh_try_lock(struct zxdh_hw *hw)
{
	uint32_t var = zxdh_read_comm_reg((uint64_t)hw->common_cfg, ZXDH_VF_LOCK_REG);

	/* check whether lock is used */
	if (!(var & ZXDH_VF_LOCK_ENABLE_MASK))
		return false;

	return true;
}

int32_t
zxdh_timedlock(struct zxdh_hw *hw, uint32_t us)
{
	uint16_t timeout = 0;

	while ((timeout++) < ZXDH_ACQUIRE_CHANNEL_NUM_MAX) {
		rte_delay_us_block(us);
		/* acquire hw lock */
		if (!zxdh_try_lock(hw)) {
			PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout: %d", timeout);
			continue;
		}
		break;
	}
	if (timeout >= ZXDH_ACQUIRE_CHANNEL_NUM_MAX) {
		PMD_DRV_LOG(ERR, "Failed to acquire channel");
		return -1;
	}
	return 0;
}

void
zxdh_release_lock(struct zxdh_hw *hw)
{
	uint32_t var = zxdh_read_comm_reg((uint64_t)hw->common_cfg, ZXDH_VF_LOCK_REG);

	if (var & ZXDH_VF_LOCK_ENABLE_MASK) {
		var &= ~ZXDH_VF_LOCK_ENABLE_MASK;
		zxdh_write_comm_reg((uint64_t)hw->common_cfg, ZXDH_VF_LOCK_REG, var);
	}
}

uint32_t
zxdh_read_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg)
{
	uint32_t val = *((volatile uint32_t *)(pci_comm_cfg_baseaddr + reg));
	return val;
}

void
zxdh_write_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg, uint32_t val)
{
	*((volatile uint32_t *)(pci_comm_cfg_baseaddr + reg)) = val;
}

int32_t
zxdh_datach_set(struct rte_eth_dev *dev, uint16_t ph_chno)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t addr, pcieid_addr;

	if (ph_chno >= ZXDH_QUEUES_PCIEID_SIZE) {
		PMD_DRV_LOG(ERR, "ph_chno is greater than %08x", ph_chno);
		return -ENOMEM;
	}

	pcieid_addr =
		*((volatile uint64_t *)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_QUEUES_PCIEID_ADDR));
	addr = hw->bar_addr[ZXDH_BAR0_INDEX] + pcieid_addr + (ph_chno << 1);
	*((volatile uint16_t *)(addr)) = hw->pcie_id;
	PMD_DRV_LOG(DEBUG, "addr %"PRIx64" pcie_id %04x, pcieid_addr %"PRIx64" lch %d",
		addr, hw->pcie_id, pcieid_addr, ph_chno);
	return 0;
}

bool
zxdh_rx_offload_enabled(struct zxdh_hw *hw)
{
	return zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_CSUM) ||
		   zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_TSO4) ||
		   zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_TSO6) ||
		   (hw->vlan_offload_cfg.vlan_strip == 1);
}

bool
zxdh_tx_offload_enabled(struct zxdh_hw *hw)
{
	return zxdh_pci_with_feature(hw, ZXDH_NET_F_CSUM) ||
		   zxdh_pci_with_feature(hw, ZXDH_NET_F_HOST_TSO4) ||
		   zxdh_pci_with_feature(hw, ZXDH_NET_F_HOST_TSO6) ||
		   zxdh_pci_with_feature(hw, ZXDH_NET_F_HOST_UFO);
}
