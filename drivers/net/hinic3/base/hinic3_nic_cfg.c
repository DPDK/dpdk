/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_cmdq.h"
#include "hinic3_hw_cfg.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mbox.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_wq.h"

struct vf_msg_handler {
	uint16_t cmd;
};

static const struct vf_msg_handler vf_cmd_handler[] = {
	{
		.cmd = HINIC3_NIC_CMD_VF_REGISTER,
	},

	{
		.cmd = HINIC3_NIC_CMD_GET_MAC,
	},

	{
		.cmd = HINIC3_NIC_CMD_SET_MAC,
	},

	{
		.cmd = HINIC3_NIC_CMD_DEL_MAC,
	},

	{
		.cmd = HINIC3_NIC_CMD_UPDATE_MAC,
	},

	{
		.cmd = HINIC3_NIC_CMD_VF_COS,
	},
};

static const struct vf_msg_handler vf_mag_cmd_handler[] = {
	{
		.cmd = MAG_CMD_GET_LINK_STATUS,
	},
};

/**
 * Set CI table for a SQ.
 *
 * Configure the CI table with attributes like CI address, pending limit,
 * coalescing time, and optional interrupt settings for specified SQ.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] attr
 * Attributes to configure for CI table.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_set_ci_table(struct hinic3_hwdev *hwdev, struct hinic3_sq_attr *attr)
{
	struct hinic3_cmd_cons_idx_attr cons_idx_attr;
	uint16_t out_size = sizeof(cons_idx_attr);
	int err;

	if (!hwdev || !attr)
		return -EINVAL;

	memset(&cons_idx_attr, 0, sizeof(cons_idx_attr));
	cons_idx_attr.func_idx = hinic3_global_func_id(hwdev);
	cons_idx_attr.dma_attr_off = attr->dma_attr_off;
	cons_idx_attr.pending_limit = attr->pending_limit;
	cons_idx_attr.coalescing_time = attr->coalescing_time;

	if (attr->intr_en) {
		cons_idx_attr.intr_en = attr->intr_en;
		cons_idx_attr.intr_idx = attr->intr_idx;
	}

	cons_idx_attr.l2nic_sqn = attr->l2nic_sqn;
	cons_idx_attr.ci_addr = attr->ci_dma_base;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SQ_CI_ATTR_SET,
				     &cons_idx_attr, sizeof(cons_idx_attr),
				     &cons_idx_attr, &out_size);
	if (err || !out_size || cons_idx_attr.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set ci attribute table failed, err: %d, status: 0x%x, out_size: 0x%x",
			    err, cons_idx_attr.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

#define PF_HAS_SET_VF_MAC(hwdev, status)           \
	(hinic3_func_type(hwdev) == TYPE_VF && \
	 (status) == HINIC3_PF_SET_VF_ALREADY)

static int
hinic3_check_mac_info(struct hinic3_hwdev *hwdev, uint8_t status, uint16_t vlan_id)
{
	if ((status && status != HINIC3_MGMT_STATUS_EXIST) ||
	    ((vlan_id & CHECK_IPSU_15BIT) &&
	     status == HINIC3_MGMT_STATUS_EXIST)) {
		if (PF_HAS_SET_VF_MAC(hwdev, status))
			return 0;

		return -EINVAL;
	}

	return 0;
}

#define VLAN_N_VID 4096

int
hinic3_set_mac(struct hinic3_hwdev *hwdev,
	       const uint8_t *mac_addr, uint16_t vlan_id, uint16_t func_id)
{
	struct hinic3_port_mac_set mac_info;
	uint16_t out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr)
		return -EINVAL;

	memset(&mac_info, 0, sizeof(mac_info));

	if (vlan_id >= VLAN_N_VID) {
		PMD_DRV_LOG(ERR, "Invalid VLAN number: %d", vlan_id);
		return -EINVAL;
	}

	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memcpy(mac_info.mac, mac_addr, ETH_ALEN);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_MAC,
				      &mac_info, sizeof(mac_info),
				      &mac_info, &out_size);
	if (err || !out_size ||
	    hinic3_check_mac_info(hwdev, mac_info.msg_head.status,
				  mac_info.vlan_id)) {
		PMD_DRV_LOG(ERR,
			    "Update MAC failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, mac_info.msg_head.status, out_size);
		return -EIO;
	}

	if (PF_HAS_SET_VF_MAC(hwdev, mac_info.msg_head.status)) {
		PMD_DRV_LOG(WARNING,
			    "PF has already set VF mac, Ignore set operation");
		return HINIC3_PF_SET_VF_ALREADY;
	}

	if (mac_info.msg_head.status == HINIC3_MGMT_STATUS_EXIST) {
		PMD_DRV_LOG(WARNING,
			    "MAC is repeated. Ignore update operation");
		return 0;
	}

	return 0;
}

int
hinic3_del_mac(struct hinic3_hwdev *hwdev, const uint8_t *mac_addr,
	       uint16_t vlan_id, uint16_t func_id)
{
	struct hinic3_port_mac_set mac_info;
	uint16_t out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr)
		return -EINVAL;

	if (vlan_id >= VLAN_N_VID) {
		PMD_DRV_LOG(ERR, "Invalid VLAN number: %d", vlan_id);
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memcpy(mac_info.mac, mac_addr, ETH_ALEN);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_DEL_MAC,
				      &mac_info, sizeof(mac_info),
				      &mac_info, &out_size);
	if (err || !out_size ||
	    (mac_info.msg_head.status &&
	     !PF_HAS_SET_VF_MAC(hwdev, mac_info.msg_head.status))) {
		PMD_DRV_LOG(ERR,
			    "Delete MAC failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, mac_info.msg_head.status, out_size);
		return -EIO;
	}

	if (PF_HAS_SET_VF_MAC(hwdev, mac_info.msg_head.status)) {
		PMD_DRV_LOG(WARNING,
			"PF has already set VF mac, Ignore delete operation");
		return HINIC3_PF_SET_VF_ALREADY;
	}

	return 0;
}

int
hinic3_update_mac(struct hinic3_hwdev *hwdev, uint8_t *old_mac,
		  uint8_t *new_mac, uint16_t vlan_id, uint16_t func_id)
{
	struct hinic3_port_mac_update mac_info;
	uint16_t out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !old_mac || !new_mac)
		return -EINVAL;

	if (vlan_id >= VLAN_N_VID) {
		PMD_DRV_LOG(ERR, "Invalid VLAN number: %d", vlan_id);
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memcpy(mac_info.old_mac, old_mac, ETH_ALEN);
	memcpy(mac_info.new_mac, new_mac, ETH_ALEN);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_UPDATE_MAC,
				     &mac_info, sizeof(mac_info),
				     &mac_info, &out_size);
	if (err || !out_size || hinic3_check_mac_info(hwdev,
			mac_info.msg_head.status, mac_info.vlan_id)) {
		PMD_DRV_LOG(ERR,
			    "Update MAC failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, mac_info.msg_head.status, out_size);
		return -EIO;
	}

	if (PF_HAS_SET_VF_MAC(hwdev, mac_info.msg_head.status)) {
		PMD_DRV_LOG(WARNING,
			"PF has already set VF MAC. Ignore update operation");
		return HINIC3_PF_SET_VF_ALREADY;
	}

	if (mac_info.msg_head.status == HINIC3_MGMT_STATUS_EXIST) {
		PMD_DRV_LOG(INFO, "MAC is repeated. Ignore update operation");
		return 0;
	}

	return 0;
}

int
hinic3_get_default_mac(struct hinic3_hwdev *hwdev, uint8_t *mac_addr, int ether_len)
{
	struct hinic3_port_mac_set mac_info;
	uint16_t out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr)
		return -EINVAL;

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_GET_MAC,
				      &mac_info, sizeof(mac_info),
				      &mac_info, &out_size);
	if (err || !out_size || mac_info.msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Get MAC failed, err: %d, status: 0x%x, out size: 0x%x",
			err, mac_info.msg_head.status, out_size);
		return -EINVAL;
	}

	memcpy(mac_addr, mac_info.mac, ether_len);

	return 0;
}

static int
hinic3_config_vlan(struct hinic3_hwdev *hwdev, uint8_t opcode, uint16_t vlan_id, uint16_t func_id)
{
	struct hinic3_cmd_vlan_config vlan_info;
	uint16_t out_size = sizeof(vlan_info);
	int err;

	memset(&vlan_info, 0, sizeof(vlan_info));
	vlan_info.opcode = opcode;
	vlan_info.func_id = func_id;
	vlan_info.vlan_id = vlan_id;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_FUNC_VLAN,
				      &vlan_info, sizeof(vlan_info),
				      &vlan_info, &out_size);
	if (err || !out_size || vlan_info.msg_head.status) {
		PMD_DRV_LOG(ERR,
			"%s vlan failed, err: %d, status: 0x%x, out size: 0x%x",
			opcode == HINIC3_CMD_OP_ADD ? "Add" : "Delete", err,
			vlan_info.msg_head.status, out_size);
		return -EINVAL;
	}

	return 0;
}

int
hinic3_add_vlan(struct hinic3_hwdev *hwdev, uint16_t vlan_id, uint16_t func_id)
{
	if (!hwdev)
		return -EINVAL;

	return hinic3_config_vlan(hwdev, HINIC3_CMD_OP_ADD, vlan_id, func_id);
}

int
hinic3_del_vlan(struct hinic3_hwdev *hwdev, uint16_t vlan_id, uint16_t func_id)
{
	if (!hwdev)
		return -EINVAL;

	return hinic3_config_vlan(hwdev, HINIC3_CMD_OP_DEL, vlan_id, func_id);
}

int
hinic3_get_port_info(struct hinic3_hwdev *hwdev, struct nic_port_info *port_info)
{
	struct hinic3_cmd_port_info port_msg;
	uint16_t out_size = sizeof(port_msg);
	int err;

	if (!hwdev || !port_info)
		return -EINVAL;

	memset(&port_msg, 0, sizeof(port_msg));
	port_msg.port_id = hinic3_physical_port_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_GET_PORT_INFO,
				      &port_msg, sizeof(port_msg),
				      &port_msg, &out_size);
	if (err || !out_size || port_msg.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Get port info failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, port_msg.msg_head.status, out_size);
		return -EINVAL;
	}

	port_info->autoneg_cap = port_msg.autoneg_cap;
	port_info->autoneg_state = port_msg.autoneg_state;
	port_info->duplex = port_msg.duplex;
	port_info->port_type = port_msg.port_type;
	port_info->speed = port_msg.speed;
	port_info->fec = port_msg.fec;

	return 0;
}

int
hinic3_get_link_state(struct hinic3_hwdev *hwdev, uint8_t *link_state)
{
	struct hinic3_cmd_link_state get_link;
	uint16_t out_size = sizeof(get_link);
	int err;

	if (!hwdev || !link_state)
		return -EINVAL;

	memset(&get_link, 0, sizeof(get_link));
	get_link.port_id = hinic3_physical_port_id(hwdev);
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_GET_LINK_STATUS,
				      &get_link, sizeof(get_link),
				      &get_link, &out_size);
	if (err || !out_size || get_link.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Get link state failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, get_link.msg_head.status, out_size);
		return -EIO;
	}

	*link_state = get_link.state;

	return 0;
}

int
hinic3_set_vport_enable(struct hinic3_hwdev *hwdev, bool enable)
{
	struct hinic3_vport_state en_state;
	uint16_t out_size = sizeof(en_state);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&en_state, 0, sizeof(en_state));
	en_state.func_id = hinic3_global_func_id(hwdev);
	en_state.state = enable ? 1 : 0;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_VPORT_ENABLE,
				      &en_state, sizeof(en_state),
				      &en_state, &out_size);
	if (err || !out_size || en_state.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set vport state failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, en_state.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_set_port_enable(struct hinic3_hwdev *hwdev, bool enable)
{
	struct mag_cmd_set_port_enable en_state;
	uint16_t out_size = sizeof(en_state);
	int err;

	if (!hwdev)
		return -EINVAL;

	if (hinic3_func_type(hwdev) == TYPE_VF)
		return 0;

	memset(&en_state, 0, sizeof(en_state));
	en_state.function_id = hinic3_global_func_id(hwdev);
	en_state.state = enable ? MAG_CMD_TX_ENABLE | MAG_CMD_RX_ENABLE
				: MAG_CMD_PORT_DISABLE;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_SET_PORT_ENABLE,
				      &en_state, sizeof(en_state),
				      &en_state, &out_size);
	if (err || !out_size || en_state.head.status) {
		PMD_DRV_LOG(ERR,
			    "Set port state failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, en_state.head.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_flush_qps_res(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmd_clear_qp_resource sq_res;
	uint16_t out_size = sizeof(sq_res);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&sq_res, 0, sizeof(sq_res));
	sq_res.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CLEAR_QP_RESOURCE,
				      &sq_res, sizeof(sq_res),
				      &sq_res, &out_size);
	if (err || !out_size || sq_res.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Clear sq resources failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, sq_res.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * Get or set the flow control (pause frame) settings for NIC.
 *
 * @param[in] hwdev
 * Pointer to the hardware device.
 * @param[in] opcode
 * The operation to perform. Use `HINIC3_CMD_OP_SET` to set the pause settings
 * and `HINIC3_CMD_OP_GET` to get them.
 * @param[out] nic_pause
 * Pointer to the `nic_pause_config` structure. This structure contains the flow
 * control settings (auto-negotiation, Rx pause, and Tx pause). It is updated
 * when getting settings. When setting, the values in this structure are used.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EIO if the operation failed.
 */
static int
hinic3_cfg_hw_pause(struct hinic3_hwdev *hwdev, uint8_t opcode, struct nic_pause_config *nic_pause)
{
	struct hinic3_cmd_pause_config pause_info = {0};
	uint16_t out_size = sizeof(pause_info);
	int err = 0;

	pause_info.port_id = hinic3_physical_port_id(hwdev);
	pause_info.opcode = opcode;
	if (opcode == HINIC3_CMD_OP_SET) {
		pause_info.auto_neg = nic_pause->auto_neg;
		pause_info.rx_pause = nic_pause->rx_pause;
		pause_info.tx_pause = nic_pause->tx_pause;
	}

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_PAUSE_INFO,
				      &pause_info, sizeof(pause_info),
				      &pause_info, &out_size);
	if (err || !out_size || pause_info.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "%s pause info failed, err: %d, status: 0x%x, out size: 0x%x",
			    opcode == HINIC3_CMD_OP_SET ? "Set" : "Get", err,
			    pause_info.msg_head.status, out_size);
		return -EIO;
	}

	if (opcode == HINIC3_CMD_OP_GET) {
		nic_pause->auto_neg = pause_info.auto_neg;
		nic_pause->rx_pause = pause_info.rx_pause;
		nic_pause->tx_pause = pause_info.tx_pause;
	}

	return 0;
}

int
hinic3_set_pause_info(struct hinic3_hwdev *hwdev, struct nic_pause_config nic_pause)
{
	if (!hwdev)
		return -EINVAL;

	return hinic3_cfg_hw_pause(hwdev, HINIC3_CMD_OP_SET, &nic_pause);
}

int
hinic3_get_pause_info(struct hinic3_hwdev *hwdev, struct nic_pause_config *nic_pause)
{
	if (!hwdev || !nic_pause)
		return -EINVAL;

	return hinic3_cfg_hw_pause(hwdev, HINIC3_CMD_OP_GET, nic_pause);
}

int
hinic3_get_vport_stats(struct hinic3_hwdev *hwdev, struct hinic3_vport_stats *stats)
{
	struct hinic3_port_stats_info stats_info;
	struct hinic3_cmd_vport_stats vport_stats;
	uint16_t out_size = sizeof(vport_stats);
	int err;

	if (!hwdev || !stats)
		return -EINVAL;

	memset(&stats_info, 0, sizeof(stats_info));
	memset(&vport_stats, 0, sizeof(vport_stats));

	stats_info.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_GET_VPORT_STAT,
				      &stats_info, sizeof(stats_info),
				      &vport_stats, &out_size);
	if (err || !out_size || vport_stats.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Get function stats failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, vport_stats.msg_head.status, out_size);
		return -EIO;
	}

	*stats = vport_stats.stats;

	return 0;
}

int
hinic3_get_phy_port_stats(struct hinic3_hwdev *hwdev, struct mag_phy_port_stats *stats)
{
	struct mag_cmd_get_port_stat *port_stats = NULL;
	struct mag_cmd_port_stats_info stats_info;
	uint16_t out_size = sizeof(*port_stats);
	int err;

	port_stats = rte_zmalloc("port_stats", sizeof(*port_stats), 0);
	if (!port_stats)
		return -ENOMEM;

	memset(&stats_info, 0, sizeof(stats_info));
	stats_info.port_id = hinic3_physical_port_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_GET_PORT_STAT,
				      &stats_info, sizeof(stats_info),
				      port_stats, &out_size);
	if (err || !out_size || port_stats->head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to get port statistics, err: %d, status: 0x%x, out size: 0x%x",
			    err, port_stats->head.status, out_size);
		err = -EIO;
		goto out;
	}

	*stats = port_stats->counter;

out:
	rte_free(port_stats);

	return err;
}

int
hinic3_clear_vport_stats(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmd_clear_vport_stats clear_vport_stats;
	uint16_t out_size = sizeof(clear_vport_stats);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&clear_vport_stats, 0, sizeof(clear_vport_stats));
	clear_vport_stats.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CLEAN_VPORT_STAT,
				      &clear_vport_stats, sizeof(clear_vport_stats),
				      &clear_vport_stats, &out_size);
	if (err || !out_size || clear_vport_stats.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Clear vport stats failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, clear_vport_stats.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_clear_phy_port_stats(struct hinic3_hwdev *hwdev)
{
	struct mag_cmd_port_stats_info port_stats;
	uint16_t out_size = sizeof(port_stats);
	int err;

	port_stats.port_id = hinic3_physical_port_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_CLR_PORT_STAT,
				      &port_stats, sizeof(port_stats),
				      &port_stats, &out_size);
	if (err || !out_size || port_stats.head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to get port statistics, err: %d, status: 0x%x, out size: 0x%x",
			    err, port_stats.head.status, out_size);
		err = -EIO;
	}

	return err;
}

static int
hinic3_set_function_table(struct hinic3_hwdev *hwdev, uint32_t cfg_bitmap,
			  struct hinic3_func_tbl_cfg *cfg)
{
	struct hinic3_cmd_set_func_tbl cmd_func_tbl;
	uint16_t out_size = sizeof(cmd_func_tbl);
	int err;

	memset(&cmd_func_tbl, 0, sizeof(cmd_func_tbl));
	cmd_func_tbl.func_id = hinic3_global_func_id(hwdev);
	cmd_func_tbl.cfg_bitmap = cfg_bitmap;
	cmd_func_tbl.tbl_cfg = *cfg;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_FUNC_TBL,
				      &cmd_func_tbl, sizeof(cmd_func_tbl),
				      &cmd_func_tbl, &out_size);
	if (err || cmd_func_tbl.msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Set func table failed, bitmap: 0x%x, err: %d, status: 0x%x, out size: 0x%x",
			    cfg_bitmap, err, cmd_func_tbl.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_init_function_table(struct hinic3_hwdev *hwdev, uint16_t rx_buff_len)
{
	struct hinic3_func_tbl_cfg func_tbl_cfg;
	uint32_t cfg_bitmap = RTE_BIT32(FUNC_CFG_INIT) | RTE_BIT32(FUNC_CFG_MTU) |
			 RTE_BIT32(FUNC_CFG_RX_BUF_SIZE);

	memset(&func_tbl_cfg, 0, sizeof(func_tbl_cfg));
	func_tbl_cfg.mtu = 0x3FFF; /**< Default, max mtu */
	func_tbl_cfg.rx_wqe_buf_size = rx_buff_len;

	return hinic3_set_function_table(hwdev, cfg_bitmap, &func_tbl_cfg);
}

int
hinic3_set_port_mtu(struct hinic3_hwdev *hwdev, uint16_t new_mtu)
{
	struct hinic3_func_tbl_cfg func_tbl_cfg;

	if (!hwdev)
		return -EINVAL;

	memset(&func_tbl_cfg, 0, sizeof(func_tbl_cfg));
	func_tbl_cfg.mtu = new_mtu;
	return hinic3_set_function_table(hwdev, RTE_BIT32(FUNC_CFG_MTU), &func_tbl_cfg);
}

static int
nic_feature_nego(struct hinic3_hwdev *hwdev, uint8_t opcode, uint64_t *s_feature, uint16_t size)
{
	struct hinic3_cmd_feature_nego feature_nego;
	uint16_t out_size = sizeof(feature_nego);
	int err;

	if (!hwdev || !s_feature || size > MAX_FEATURE_QWORD)
		return -EINVAL;

	memset(&feature_nego, 0, sizeof(feature_nego));
	feature_nego.func_id = hinic3_global_func_id(hwdev);
	feature_nego.opcode = opcode;
	if (opcode == HINIC3_CMD_OP_SET)
		memcpy(feature_nego.s_feature, s_feature, size * sizeof(uint64_t));

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_FEATURE_NEGO,
				      &feature_nego, sizeof(feature_nego),
				      &feature_nego, &out_size);
	if (err || !out_size || feature_nego.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to negotiate nic feature, err:%d, status: 0x%x, out_size: 0x%x",
			    err, feature_nego.msg_head.status, out_size);
		return -EFAULT;
	}

	if (opcode == HINIC3_CMD_OP_GET)
		memcpy(s_feature, feature_nego.s_feature, size * sizeof(uint64_t));

	return 0;
}

int
hinic3_get_feature_from_hw(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size)
{
	return nic_feature_nego(hwdev, HINIC3_CMD_OP_GET, s_feature, size);
}

int
hinic3_set_feature_to_hw(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size)
{
	return nic_feature_nego(hwdev, HINIC3_CMD_OP_SET, s_feature, size);
}

static int
hinic3_vf_func_init(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmd_register_vf register_info;
	uint16_t out_size = sizeof(register_info);
	int err;

	if (hinic3_func_type(hwdev) != TYPE_VF)
		return 0;

	memset(&register_info, 0, sizeof(register_info));
	register_info.op_register = 1;
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_VF_REGISTER,
				      &register_info, sizeof(register_info),
				      &register_info, &out_size);
	if (err || register_info.msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Register VF failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, register_info.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

static int
hinic3_vf_func_free(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmd_register_vf unregister;
	uint16_t out_size = sizeof(unregister);
	int err;

	if (hinic3_func_type(hwdev) != TYPE_VF)
		return 0;

	memset(&unregister, 0, sizeof(unregister));
	unregister.op_register = 0;
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_VF_REGISTER,
				      &unregister, sizeof(unregister),
				      &unregister, &out_size);
	if (err || unregister.msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Unregister VF failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, unregister.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_init_nic_hwdev(struct hinic3_hwdev *hwdev)
{
	return hinic3_vf_func_init(hwdev);
}

void
hinic3_free_nic_hwdev(struct hinic3_hwdev *hwdev)
{
	if (!hwdev)
		return;

	if (hinic3_func_type(hwdev) != TYPE_VF)
		hinic3_set_link_status_follow(hwdev, HINIC3_LINK_FOLLOW_DEFAULT);

	hinic3_vf_func_free(hwdev);
}

int
hinic3_set_rx_mode(struct hinic3_hwdev *hwdev, uint32_t enable)
{
	struct hinic3_rx_mode_config rx_mode_cfg;
	uint16_t out_size = sizeof(rx_mode_cfg);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&rx_mode_cfg, 0, sizeof(rx_mode_cfg));
	rx_mode_cfg.func_id = hinic3_global_func_id(hwdev);
	rx_mode_cfg.rx_mode = enable;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_RX_MODE,
				      &rx_mode_cfg, sizeof(rx_mode_cfg),
				      &rx_mode_cfg, &out_size);
	if (err || !out_size || rx_mode_cfg.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set rx mode failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, rx_mode_cfg.msg_head.status, out_size);
		return -EIO;
	}
	return 0;
}

int
hinic3_set_rx_vlan_offload(struct hinic3_hwdev *hwdev, uint8_t en)
{
	struct hinic3_cmd_vlan_offload vlan_cfg;
	uint16_t out_size = sizeof(vlan_cfg);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.func_id = hinic3_global_func_id(hwdev);
	vlan_cfg.vlan_offload = en;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_RX_VLAN_OFFLOAD,
				      &vlan_cfg, sizeof(vlan_cfg),
				      &vlan_cfg, &out_size);
	if (err || !out_size || vlan_cfg.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set rx vlan offload failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, vlan_cfg.msg_head.status, out_size);
		return -EIO;
	}
	return 0;
}

int
hinic3_set_vlan_filter(struct hinic3_hwdev *hwdev, uint32_t vlan_filter_ctrl)
{
	struct hinic3_cmd_set_vlan_filter vlan_filter;
	uint16_t out_size = sizeof(vlan_filter);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&vlan_filter, 0, sizeof(vlan_filter));
	vlan_filter.func_id = hinic3_global_func_id(hwdev);
	vlan_filter.vlan_filter_ctrl = vlan_filter_ctrl;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_VLAN_FILTER_EN,
				      &vlan_filter, sizeof(vlan_filter),
				      &vlan_filter, &out_size);
	if (err || !out_size || vlan_filter.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to set vlan filter, err: %d, status: 0x%x, out size: 0x%x",
			    err, vlan_filter.msg_head.status, out_size);
		return -EIO;
	}
	return 0;
}

static int
hinic3_set_rx_lro(struct hinic3_hwdev *hwdev, uint8_t ipv4_en,
				uint8_t ipv6_en, uint8_t lro_max_pkt_len)
{
	struct hinic3_cmd_lro_config lro_cfg = {0};
	uint16_t out_size = sizeof(lro_cfg);
	int err;

	if (!hwdev)
		return -EINVAL;

	lro_cfg.func_id = hinic3_global_func_id(hwdev);
	lro_cfg.opcode = HINIC3_CMD_OP_SET;
	lro_cfg.lro_ipv4_en = ipv4_en;
	lro_cfg.lro_ipv6_en = ipv6_en;
	lro_cfg.lro_max_pkt_len = lro_max_pkt_len;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_RX_LRO,
				      &lro_cfg, sizeof(lro_cfg),
				      &lro_cfg, &out_size);
	if (err || !out_size || lro_cfg.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set lro offload failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, lro_cfg.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

static int
hinic3_set_rx_lro_timer(struct hinic3_hwdev *hwdev, uint32_t timer_value)
{
	struct hinic3_cmd_lro_timer lro_timer;
	uint16_t out_size = sizeof(lro_timer);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&lro_timer, 0, sizeof(lro_timer));
	lro_timer.opcode = HINIC3_CMD_OP_SET;
	lro_timer.timer = timer_value;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_LRO_TIMER,
				      &lro_timer, sizeof(lro_timer),
				      &lro_timer, &out_size);
	if (err || !out_size || lro_timer.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set lro timer failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, lro_timer.msg_head.status, out_size);

		return -EIO;
	}

	return 0;
}

int
hinic3_set_rx_lro_state(struct hinic3_hwdev *hwdev, uint8_t lro_en, uint32_t lro_timer,
			uint32_t lro_max_pkt_len)
{
	uint8_t ipv4_en = 0, ipv6_en = 0;
	int err;

	if (!hwdev)
		return -EINVAL;

	ipv4_en = lro_en ? 1 : 0;
	ipv6_en = lro_en ? 1 : 0;

	PMD_DRV_LOG(DEBUG, "Set LRO max coalesce packet size to %uK", lro_max_pkt_len);

	err = hinic3_set_rx_lro(hwdev, ipv4_en, ipv6_en, lro_max_pkt_len);
	if (err)
		return err;

	/* We don't set LRO timer for VF */
	if (hinic3_func_type(hwdev) == TYPE_VF)
		return 0;

	PMD_DRV_LOG(DEBUG, "Set LRO timer to %u", lro_timer);

	return hinic3_set_rx_lro_timer(hwdev, lro_timer);
}

/* RSS config */
int
hinic3_rss_template_alloc(struct hinic3_hwdev *hwdev)
{
	struct hinic3_rss_template_mgmt template_mgmt;
	uint16_t out_size = sizeof(template_mgmt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&template_mgmt, 0, sizeof(struct hinic3_rss_template_mgmt));
	template_mgmt.func_id = hinic3_global_func_id(hwdev);
	template_mgmt.cmd = NIC_RSS_CMD_TEMP_ALLOC;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_RSS_TEMP_MGR,
				      &template_mgmt, sizeof(template_mgmt),
				      &template_mgmt, &out_size);
	if (err || !out_size || template_mgmt.msg_head.status) {
		if (template_mgmt.msg_head.status ==
		    HINIC3_MGMT_STATUS_TABLE_FULL) {
			PMD_DRV_LOG(ERR, "There is no more template available");
			return -ENOSPC;
		}
		PMD_DRV_LOG(ERR,
			    "Alloc rss template failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, template_mgmt.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_rss_template_free(struct hinic3_hwdev *hwdev)
{
	struct hinic3_rss_template_mgmt template_mgmt;
	uint16_t out_size = sizeof(template_mgmt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&template_mgmt, 0, sizeof(struct hinic3_rss_template_mgmt));
	template_mgmt.func_id = hinic3_global_func_id(hwdev);
	template_mgmt.cmd = NIC_RSS_CMD_TEMP_FREE;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_RSS_TEMP_MGR,
				      &template_mgmt, sizeof(template_mgmt),
				      &template_mgmt, &out_size);
	if (err || !out_size || template_mgmt.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Free rss template failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, template_mgmt.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

static int
hinic3_rss_cfg_hash_key(struct hinic3_hwdev *hwdev, uint8_t opcode, uint8_t *key, uint16_t key_size)
{
	struct hinic3_cmd_rss_hash_key hash_key;
	uint16_t out_size = sizeof(hash_key);
	int err;

	if (!hwdev || !key)
		return -EINVAL;

	memset(&hash_key, 0, sizeof(struct hinic3_cmd_rss_hash_key));
	hash_key.func_id = hinic3_global_func_id(hwdev);
	hash_key.opcode = opcode;
	if (opcode == HINIC3_CMD_OP_SET)
		memcpy(hash_key.key, key, key_size);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_RSS_HASH_KEY,
				      &hash_key, sizeof(hash_key),
				      &hash_key, &out_size);
	if (err || !out_size || hash_key.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "%s hash key failed, err: %d, status: 0x%x, out size: 0x%x",
			    opcode == HINIC3_CMD_OP_SET ? "Set" : "Get", err,
			    hash_key.msg_head.status, out_size);
		return -EFAULT;
	}

	if (opcode == HINIC3_CMD_OP_GET)
		memcpy(key, hash_key.key, key_size);

	return 0;
}

int
hinic3_rss_set_hash_key(struct hinic3_hwdev *hwdev, uint8_t *key, uint16_t key_size)
{
	if (!hwdev || !key)
		return -EINVAL;

	return hinic3_rss_cfg_hash_key(hwdev, HINIC3_CMD_OP_SET, key, key_size);
}

int
hinic3_rss_get_indir_tbl(struct hinic3_hwdev *hwdev,
			 uint32_t *indir_table, uint32_t indir_table_size)
{
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint16_t *indir_tbl = NULL;
	int err;
	uint32_t i;

	if (!hwdev || !indir_table)
		return -EINVAL;

	cmd_buf = hinic3_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf failed");
		return -ENOMEM;
	}

	cmd_buf->size = sizeof(struct nic_rss_indirect_tbl);
	err = hinic3_cmdq_detail_resp(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_UCODE_CMD_GET_RSS_INDIR_TABLE,
				      cmd_buf, cmd_buf, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Get rss indir table failed");
		hinic3_free_cmd_buf(cmd_buf);
		return err;
	}

	indir_tbl = (uint16_t *)cmd_buf->buf;
	for (i = 0; i < indir_table_size; i++)
		indir_table[i] = *(indir_tbl + i);

	hinic3_free_cmd_buf(cmd_buf);
	return 0;
}

int
hinic3_rss_set_indir_tbl(struct hinic3_hwdev *hwdev, const uint32_t *indir_table,
			 uint32_t indir_table_size)
{
	struct nic_rss_indirect_tbl *indir_tbl = NULL;
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint32_t i, size;
	uint32_t *temp = NULL;
	uint64_t out_param = 0;
	int err;

	if (!hwdev || !indir_table)
		return -EINVAL;

	cmd_buf = hinic3_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf failed");
		return -ENOMEM;
	}

	cmd_buf->size = sizeof(struct nic_rss_indirect_tbl);
	indir_tbl = (struct nic_rss_indirect_tbl *)cmd_buf->buf;
	memset(indir_tbl, 0, sizeof(*indir_tbl));

	for (i = 0; i < indir_table_size; i++)
		indir_tbl->entry[i] = (uint16_t)(*(indir_table + i));

	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	size = sizeof(indir_tbl->entry) / sizeof(uint16_t);
	temp = (uint32_t *)indir_tbl->entry;
	for (i = 0; i < size; i++)
		temp[i] = rte_cpu_to_be_32(temp[i]);

	err = hinic3_cmdq_direct_resp(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_UCODE_CMD_SET_RSS_INDIR_TABLE,
				      cmd_buf, &out_param, 0);
	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR, "Set rss indir table failed");
		err = -EFAULT;
	}

	hinic3_free_cmd_buf(cmd_buf);
	return err;
}

static int
hinic3_cmdq_set_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type rss_type)
{
	struct nic_rss_context_tbl *ctx_tbl = NULL;
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint32_t ctx = 0;
	uint64_t out_param = 0;
	int err;

	if (!hwdev)
		return -EINVAL;

	cmd_buf = hinic3_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buf failed");
		return -ENOMEM;
	}

	ctx |= HINIC3_RSS_TYPE_SET(1, VALID) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv4, IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv6, IPV6) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv6_ext, IPV6_EXT) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv4, TCP_IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv6, TCP_IPV6) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv6_ext, TCP_IPV6_EXT) |
	       HINIC3_RSS_TYPE_SET(rss_type.udp_ipv4, UDP_IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.udp_ipv6, UDP_IPV6);

	cmd_buf->size = sizeof(struct nic_rss_context_tbl);
	ctx_tbl = (struct nic_rss_context_tbl *)cmd_buf->buf;
	memset(ctx_tbl, 0, sizeof(*ctx_tbl));
	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	ctx_tbl->ctx = rte_cpu_to_be_32(ctx);

	/* Cfg the RSS context table by command queue. */
	err = hinic3_cmdq_direct_resp(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_UCODE_CMD_SET_RSS_CONTEXT_TABLE,
				      cmd_buf, &out_param, 0);

	hinic3_free_cmd_buf(cmd_buf);

	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR, "Cmdq set rss context table failed, err: %d",
			    err);
		return -EFAULT;
	}

	return 0;
}

static int
hinic3_mgmt_set_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type rss_type)
{
	struct hinic3_rss_context_table ctx_tbl;
	uint32_t ctx = 0;
	uint16_t out_size = sizeof(ctx_tbl);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&ctx_tbl, 0, sizeof(ctx_tbl));
	ctx_tbl.func_id = hinic3_global_func_id(hwdev);
	ctx |= HINIC3_RSS_TYPE_SET(1, VALID) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv4, IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv6, IPV6) |
	       HINIC3_RSS_TYPE_SET(rss_type.ipv6_ext, IPV6_EXT) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv4, TCP_IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv6, TCP_IPV6) |
	       HINIC3_RSS_TYPE_SET(rss_type.tcp_ipv6_ext, TCP_IPV6_EXT) |
	       HINIC3_RSS_TYPE_SET(rss_type.udp_ipv4, UDP_IPV4) |
	       HINIC3_RSS_TYPE_SET(rss_type.udp_ipv6, UDP_IPV6);
	ctx_tbl.context = ctx;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
		HINIC3_NIC_CMD_SET_RSS_CTX_TBL_INTO_FUNC,
		&ctx_tbl, sizeof(ctx_tbl), &ctx_tbl, &out_size);
	if (ctx_tbl.msg_head.status == HINIC3_MGMT_CMD_UNSUPPORTED) {
		return HINIC3_MGMT_CMD_UNSUPPORTED;
	} else if (err || !out_size || ctx_tbl.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Mgmt set rss context table failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, ctx_tbl.msg_head.status, out_size);
		return -EINVAL;
	}
	return 0;
}

int
hinic3_set_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type rss_type)
{
	int err;
	err = hinic3_mgmt_set_rss_type(hwdev, rss_type);
	if (err == HINIC3_MGMT_CMD_UNSUPPORTED)
		err = hinic3_cmdq_set_rss_type(hwdev, rss_type);
	return err;
}

int
hinic3_get_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type *rss_type)
{
	struct hinic3_rss_context_table ctx_tbl;
	uint16_t out_size = sizeof(ctx_tbl);
	int err;

	if (!hwdev || !rss_type)
		return -EINVAL;

	memset(&ctx_tbl, 0, sizeof(struct hinic3_rss_context_table));
	ctx_tbl.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_GET_RSS_CTX_TBL,
				      &ctx_tbl, sizeof(ctx_tbl),
				      &ctx_tbl, &out_size);
	if (err || !out_size || ctx_tbl.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Get hash type failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, ctx_tbl.msg_head.status, out_size);
		return -EFAULT;
	}

	rss_type->ipv4 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, IPV4);
	rss_type->ipv6 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, IPV6);
	rss_type->ipv6_ext = HINIC3_RSS_TYPE_GET(ctx_tbl.context, IPV6_EXT);
	rss_type->tcp_ipv4 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV4);
	rss_type->tcp_ipv6 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV6);
	rss_type->tcp_ipv6_ext =
		HINIC3_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV6_EXT);
	rss_type->udp_ipv4 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, UDP_IPV4);
	rss_type->udp_ipv6 = HINIC3_RSS_TYPE_GET(ctx_tbl.context, UDP_IPV6);

	return 0;
}

static int
hinic3_rss_cfg_hash_engine(struct hinic3_hwdev *hwdev, uint8_t opcode, uint8_t *type)
{
	struct hinic3_cmd_rss_engine_type hash_type;
	uint16_t out_size = sizeof(hash_type);
	int err;

	if (!hwdev || !type)
		return -EINVAL;

	memset(&hash_type, 0, sizeof(struct hinic3_cmd_rss_engine_type));
	hash_type.func_id = hinic3_global_func_id(hwdev);
	hash_type.opcode = opcode;
	if (opcode == HINIC3_CMD_OP_SET)
		hash_type.hash_engine = *type;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_RSS_HASH_ENGINE,
				      &hash_type, sizeof(hash_type),
				      &hash_type, &out_size);
	if (err || !out_size || hash_type.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "%s hash engine failed, err: %d, status: 0x%x, out size: 0x%x",
			    opcode == HINIC3_CMD_OP_SET ? "Set" : "Get", err,
			    hash_type.msg_head.status, out_size);
		return -EFAULT;
	}

	if (opcode == HINIC3_CMD_OP_GET)
		*type = hash_type.hash_engine;

	return 0;
}

int
hinic3_rss_get_hash_engine(struct hinic3_hwdev *hwdev, uint8_t *type)
{
	if (!hwdev || !type)
		return -EINVAL;

	return hinic3_rss_cfg_hash_engine(hwdev, HINIC3_CMD_OP_GET, type);
}

int
hinic3_rss_set_hash_engine(struct hinic3_hwdev *hwdev, uint8_t type)
{
	if (!hwdev)
		return -EINVAL;

	return hinic3_rss_cfg_hash_engine(hwdev, HINIC3_CMD_OP_SET, &type);
}

int
hinic3_rss_cfg(struct hinic3_hwdev *hwdev, uint8_t rss_en, uint8_t tc_num, uint8_t *prio_tc)
{
	struct hinic3_cmd_rss_config rss_cfg;
	uint16_t out_size = sizeof(rss_cfg);
	int err;

	/* Ucode requires number of TC should be power of 2. */
	if (!hwdev || !prio_tc || (tc_num & (tc_num - 1)))
		return -EINVAL;

	memset(&rss_cfg, 0, sizeof(struct hinic3_cmd_rss_config));
	rss_cfg.func_id = hinic3_global_func_id(hwdev);
	rss_cfg.rss_en = rss_en;
	rss_cfg.rq_priority_number = tc_num ? (uint8_t)rte_log2_u32(tc_num) : 0;

	memcpy(rss_cfg.prio_tc, prio_tc, HINIC3_DCB_UP_MAX);
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_RSS_CFG,
				      &rss_cfg, sizeof(rss_cfg),
				      &rss_cfg, &out_size);
	if (err || !out_size || rss_cfg.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set rss cfg failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, rss_cfg.msg_head.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_vf_get_default_cos(struct hinic3_hwdev *hwdev, uint8_t *cos_id)
{
	struct hinic3_cmd_vf_dcb_state vf_dcb;
	uint16_t out_size = sizeof(vf_dcb);
	int err;

	memset(&vf_dcb, 0, sizeof(vf_dcb));

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_VF_COS,
				      &vf_dcb, sizeof(vf_dcb),
				      &vf_dcb, &out_size);
	if (err || !out_size || vf_dcb.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Get VF default cos failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, vf_dcb.msg_head.status, out_size);
		return -EIO;
	}

	*cos_id = vf_dcb.state.default_cos;

	return 0;
}

/**
 * Set the Ethernet type filtering rule for the FDIR of a NIC.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 * @param[in] pkt_type
 * Indicate the packet type.
 * @param[in] queue_id
 * Indicate the queue id.
 * @param[in] en
 * Indicate whether to add or delete an operation. 1 - add; 0 - delete.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_set_fdir_ethertype_filter(struct hinic3_hwdev *hwdev,
			 uint8_t pkt_type, uint16_t queue_id, uint8_t en)
{
	struct hinic3_set_fdir_ethertype_rule ethertype_cmd;
	uint16_t out_size = sizeof(ethertype_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&ethertype_cmd, 0,
	       sizeof(struct hinic3_set_fdir_ethertype_rule));
	ethertype_cmd.func_id = hinic3_global_func_id(hwdev);
	ethertype_cmd.pkt_type = pkt_type;
	ethertype_cmd.pkt_type_en = en;
	ethertype_cmd.qid = (uint8_t)queue_id;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_SET_FDIR_STATUS,
				      &ethertype_cmd, sizeof(ethertype_cmd),
				      &ethertype_cmd, &out_size);
	if (err || ethertype_cmd.head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "set fdir ethertype rule failed, err: %d, status: 0x%x, out size: 0x%x, func_id %d",
			    err, ethertype_cmd.head.status, out_size,
			    ethertype_cmd.func_id);
		return -EIO;
	}

	return 0;
}

int
hinic3_add_tcam_rule(struct hinic3_hwdev *hwdev, struct hinic3_tcam_cfg_rule *tcam_rule,
		     uint8_t tcam_rule_type)
{
	struct hinic3_fdir_add_rule tcam_cmd;
	uint16_t out_size = sizeof(tcam_cmd);
	int err;

	if (!hwdev || !tcam_rule)
		return -EINVAL;
	/* Check whether the index is out of range. */
	if (tcam_rule->index >= HINIC3_MAX_TCAM_RULES_NUM) {
		PMD_DRV_LOG(ERR, "Tcam rules num to add is invalid");
		return -EINVAL;
	}

	memset(&tcam_cmd, 0, sizeof(struct hinic3_fdir_add_rule));
	tcam_cmd.func_id = hinic3_global_func_id(hwdev);
	tcam_cmd.rule = *tcam_rule;
	tcam_cmd.type = tcam_rule_type;

	/* Synchronize the information to the management module. */
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_ADD_TC_FLOW,
				      &tcam_cmd, sizeof(tcam_cmd),
				      &tcam_cmd, &out_size);
	if (err || tcam_cmd.msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Add tcam rule failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, tcam_cmd.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_del_tcam_rule(struct hinic3_hwdev *hwdev, uint32_t index, uint8_t tcam_rule_type)
{
	struct hinic3_fdir_del_rule tcam_cmd;
	uint16_t out_size = sizeof(tcam_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;
	/* Check whether the index is out of range. */
	if (index >= HINIC3_MAX_TCAM_RULES_NUM) {
		PMD_DRV_LOG(ERR, "Tcam rules num to del is invalid");
		return -EINVAL;
	}

	memset(&tcam_cmd, 0, sizeof(struct hinic3_fdir_del_rule));
	tcam_cmd.func_id = hinic3_global_func_id(hwdev);
	tcam_cmd.index_start = index;
	tcam_cmd.index_num = 1;
	tcam_cmd.type = tcam_rule_type;

	/* Synchronize the information to the management module. */
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_DEL_TC_FLOW,
				      &tcam_cmd, sizeof(tcam_cmd),
				      &tcam_cmd, &out_size);
	if (err || tcam_cmd.msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Del tcam rule failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, tcam_cmd.msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

static int
hinic3_cfg_tcam_block(struct hinic3_hwdev *hwdev, uint8_t alloc_en, uint16_t *index)
{
	struct hinic3_tcam_block tcam_block_info;
	uint16_t out_size = sizeof(tcam_block_info);
	int err;

	if (!hwdev)
		return -EINVAL;
	/* Initialization TCAM block structure. */
	memset(&tcam_block_info, 0, sizeof(struct hinic3_tcam_block));
	tcam_block_info.func_id = hinic3_global_func_id(hwdev);
	tcam_block_info.alloc_en = alloc_en;
	tcam_block_info.tcam_type = HINIC3_TCAM_BLOCK_NORMAL_TYPE;
	tcam_block_info.tcam_block_index = *index;

	/* Synchronize the information to the management module. */
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_CFG_TCAM_BLOCK,
				      &tcam_block_info, sizeof(tcam_block_info),
				      &tcam_block_info, &out_size);
	if (err || !out_size || tcam_block_info.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Set tcam block failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, tcam_block_info.msg_head.status, out_size);
		return -EIO;
	}

	/* Update TCAM block of index. */
	if (alloc_en)
		*index = tcam_block_info.tcam_block_index;

	return 0;
}

int
hinic3_alloc_tcam_block(struct hinic3_hwdev *hwdev, uint16_t *index)
{
	return hinic3_cfg_tcam_block(hwdev, HINIC3_TCAM_BLOCK_ENABLE, index);
}

int
hinic3_free_tcam_block(struct hinic3_hwdev *hwdev, uint16_t *index)
{
	return hinic3_cfg_tcam_block(hwdev, HINIC3_TCAM_BLOCK_DISABLE, index);
}

int
hinic3_flush_tcam_rule(struct hinic3_hwdev *hwdev)
{
	struct hinic3_flush_tcam_rules tcam_flush;
	uint16_t out_size = sizeof(tcam_flush);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&tcam_flush, 0, sizeof(struct hinic3_flush_tcam_rules));
	tcam_flush.func_id = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_FLUSH_TCAM,
				      &tcam_flush, sizeof(struct hinic3_flush_tcam_rules),
				      &tcam_flush, &out_size);
	if (tcam_flush.msg_head.status == HINIC3_MGMT_CMD_UNSUPPORTED) {
		err = HINIC3_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(INFO, "Firmware/uP doesn't support flush tcam fdir");
	} else if (err || (!out_size) || tcam_flush.msg_head.status) {
		PMD_DRV_LOG(ERR,
			    "Flush tcam fdir rules failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, tcam_flush.msg_head.status, out_size);
		err = -EIO;
	}

	return err;
}

int
hinic3_set_fdir_tcam_rule_filter(struct hinic3_hwdev *hwdev, bool enable)
{
	struct hinic3_port_tcam_info port_tcam_cmd;
	uint16_t out_size = sizeof(port_tcam_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;
	/* Initialization information. */
	memset(&port_tcam_cmd, 0, sizeof(port_tcam_cmd));
	port_tcam_cmd.func_id = hinic3_global_func_id(hwdev);
	port_tcam_cmd.tcam_enable = (uint8_t)enable;

	/* Synchronize the information to the management module. */
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_NIC_CMD_ENABLE_TCAM,
				      &port_tcam_cmd, sizeof(port_tcam_cmd),
				      &port_tcam_cmd, &out_size);
	if ((port_tcam_cmd.msg_head.status != HINIC3_MGMT_CMD_UNSUPPORTED &&
	     port_tcam_cmd.msg_head.status) || err || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Set fdir tcam filter failed, err: %d, status: 0x%x, out size: 0x%x, enable: 0x%x",
			    err, port_tcam_cmd.msg_head.status, out_size,
			    enable);
		return -EIO;
	}

	if (port_tcam_cmd.msg_head.status == HINIC3_MGMT_CMD_UNSUPPORTED) {
		err = HINIC3_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(WARNING,
			    "Fw doesn't support setting fdir tcam filter");
	}

	return err;
}

int
hinic3_set_rq_flush(struct hinic3_hwdev *hwdev, uint16_t q_id)
{
	struct hinic3_cmd_set_rq_flush *rq_flush_msg = NULL;
	struct hinic3_cmd_buf *cmd_buf = NULL;
	uint64_t out_param = EIO;
	int err;

	cmd_buf = hinic3_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	cmd_buf->size = sizeof(*rq_flush_msg);

	rq_flush_msg = cmd_buf->buf;
	rq_flush_msg->local_rq_id = q_id;
	rte_atomic_thread_fence(rte_memory_order_seq_cst);
	rq_flush_msg->value = rte_cpu_to_be_32(rq_flush_msg->value);

	err = hinic3_cmdq_direct_resp(hwdev, HINIC3_MOD_L2NIC,
				      HINIC3_UCODE_CMD_SET_RQ_FLUSH, cmd_buf,
				      &out_param, 0);
	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR,
			"Failed to set rq flush, err:%d, out_param:0x%" PRIx64,
			err, out_param);
		err = -EFAULT;
	}

	hinic3_free_cmd_buf(cmd_buf);

	return err;
}

int
hinic3_msg_to_mgmt_sync(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
			uint16_t cmd, void *buf_in, uint16_t in_size,
			void *buf_out, uint16_t *out_size)
{
	uint32_t i;
	bool cmd_to_pf = false;
	struct hinic3_handler_info handler_info = {
		.cmd = cmd,
		.buf_in = buf_in,
		.in_size = in_size,
		.buf_out = buf_out,
		.out_size = out_size,
		.dst_func = HINIC3_MGMT_SRC_ID,
		.direction = HINIC3_MSG_DIRECT_SEND,
		.ack_type = HINIC3_MSG_ACK,
	};

	if (hinic3_func_type(hwdev) == TYPE_VF) {
		if (mod == HINIC3_MOD_HILINK) {
			for (i = 0; i < RTE_DIM(vf_mag_cmd_handler); i++) {
				if (cmd == vf_mag_cmd_handler[i].cmd)
					cmd_to_pf = true;
			}
		} else if (mod == HINIC3_MOD_L2NIC) {
			for (i = 0; i < RTE_DIM(vf_cmd_handler); i++) {
				if (cmd == vf_cmd_handler[i].cmd)
					cmd_to_pf = true;
			}
		}
	}
	if (cmd_to_pf)
		handler_info.dst_func = hinic3_pf_id_of_vf(hwdev);

	return hinic3_send_mbox_to_mgmt(hwdev, mod, &handler_info, 0);
}

int
hinic3_set_link_status_follow(struct hinic3_hwdev *hwdev,
			      enum hinic3_link_follow_status status)
{
	struct mag_cmd_set_link_follow follow;
	uint16_t out_size = sizeof(follow);
	int err;

	if (!hwdev)
		return -EINVAL;

	if (status >= HINIC3_LINK_FOLLOW_STATUS_MAX) {
		PMD_DRV_LOG(ERR, "Invalid link follow status: %d", status);
		return -EINVAL;
	}

	memset(&follow, 0, sizeof(follow));
	follow.function_id = hinic3_global_func_id(hwdev);
	follow.follow = status;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_HILINK,
				      MAG_CMD_SET_LINK_FOLLOW,
				      &follow, sizeof(follow),
				      &follow, &out_size);
	if ((follow.head.status != HINIC3_MGMT_CMD_UNSUPPORTED && follow.head.status) ||
	     err || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Failed to set link status follow port status, err: %d, status: 0x%x, out size: 0x%x",
			    err, follow.head.status, out_size);
		return -EFAULT;
	}

	return follow.head.status;
}
