/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_eqs.h"
#include "hinic_pmd_wq.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_cmdq.h"
#include "hinic_pmd_niccfg.h"
#include "hinic_pmd_mbox.h"

#define l2nic_msg_to_mgmt_sync(hwdev, cmd, buf_in,		\
			       in_size, buf_out, out_size)	\
	hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_L2NIC, cmd,	\
			buf_in, in_size,			\
			buf_out, out_size, 0)

/**
 * hinic_init_function_table - Initialize function table.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param rx_buf_sz
 *   Receive buffer size.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_init_function_table(void *hwdev, u16 rx_buf_sz)
{
	struct hinic_function_table function_table;
	u16 out_size = sizeof(function_table);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&function_table, 0, sizeof(function_table));
	function_table.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	function_table.func_id = hinic_global_func_id(hwdev);
	function_table.mtu = 0x3FFF;	/* default, max mtu */
	function_table.rx_wqe_buf_size = rx_buf_sz;

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_L2NIC,
				     HINIC_PORT_CMD_INIT_FUNC,
				     &function_table, sizeof(function_table),
				     &function_table, &out_size, 0);
	if (err || function_table.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			"Failed to init func table, err: %d, status: 0x%x, out size: 0x%x",
			err, function_table.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_get_base_qpn - Get global queue number.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param global_qpn
 *   Global queue number.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_get_base_qpn(void *hwdev, u16 *global_qpn)
{
	struct hinic_cmd_qpn cmd_qpn;
	u16 out_size = sizeof(cmd_qpn);
	int err;

	if (!hwdev || !global_qpn) {
		PMD_DRV_LOG(ERR, "Hwdev or global_qpn is NULL");
		return -EINVAL;
	}

	memset(&cmd_qpn, 0, sizeof(cmd_qpn));
	cmd_qpn.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	cmd_qpn.func_id = hinic_global_func_id(hwdev);

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_L2NIC,
				     HINIC_PORT_CMD_GET_GLOBAL_QPN,
				     &cmd_qpn, sizeof(cmd_qpn), &cmd_qpn,
				     &out_size, 0);
	if (err || !out_size || cmd_qpn.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to get base qpn, err: %d, status: 0x%x, out size: 0x%x",
			err, cmd_qpn.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	*global_qpn = cmd_qpn.base_qpn;

	return 0;
}

/**
 * hinic_set_mac - Init mac_vlan table in NIC.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param mac_addr
 *   MAC address.
 * @param vlan_id
 *   Set 0 for mac_vlan table initialization.
 * @param func_id
 *   Global function id of NIC.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_mac(void *hwdev, u8 *mac_addr, u16 vlan_id, u16 func_id)
{
	struct hinic_port_mac_set mac_info;
	u16 out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr) {
		PMD_DRV_LOG(ERR, "Hwdev or mac_addr is NULL");
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memmove(mac_info.mac, mac_addr, ETH_ALEN);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_MAC, &mac_info,
				     sizeof(mac_info), &mac_info, &out_size);
	if (err || !out_size || (mac_info.mgmt_msg_head.status &&
	    mac_info.mgmt_msg_head.status != HINIC_PF_SET_VF_ALREADY)) {
		PMD_DRV_LOG(ERR, "Failed to set MAC, err: %d, status: 0x%x, out size: 0x%x",
			err, mac_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	if (mac_info.mgmt_msg_head.status == HINIC_PF_SET_VF_ALREADY) {
		PMD_DRV_LOG(WARNING, "PF has already set vf mac, Ignore set operation.");
		return HINIC_PF_SET_VF_ALREADY;
	}

	return 0;
}

/**
 * hinic_del_mac - Uninit mac_vlan table in NIC.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param mac_addr
 *   MAC address.
 * @param vlan_id
 *   Set 0 for mac_vlan table initialization.
 * @param func_id
 *   Global function id of NIC.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_del_mac(void *hwdev, u8 *mac_addr, u16 vlan_id, u16 func_id)
{
	struct hinic_port_mac_set mac_info;
	u16 out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr) {
		PMD_DRV_LOG(ERR, "Hwdev or mac_addr is NULL");
		return -EINVAL;
	}

	if (vlan_id >= VLAN_N_VID) {
		PMD_DRV_LOG(ERR, "Invalid VLAN number");
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memmove(mac_info.mac, mac_addr, ETH_ALEN);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_DEL_MAC, &mac_info,
				     sizeof(mac_info), &mac_info, &out_size);
	if (err || !out_size || (mac_info.mgmt_msg_head.status &&
		mac_info.mgmt_msg_head.status != HINIC_PF_SET_VF_ALREADY)) {
		PMD_DRV_LOG(ERR, "Failed to delete MAC, err: %d, status: 0x%x, out size: 0x%x",
			err, mac_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}
	if (mac_info.mgmt_msg_head.status == HINIC_PF_SET_VF_ALREADY) {
		PMD_DRV_LOG(WARNING, "PF has already set vf mac, Ignore delete operation.");
		return HINIC_PF_SET_VF_ALREADY;
	}

	return 0;
}

/**
 * hinic_get_default_mac - Get default mac address from hardware.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param mac_addr
 *   MAC address.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_get_default_mac(void *hwdev, u8 *mac_addr)
{
	struct hinic_port_mac_set mac_info;
	u16 out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !mac_addr) {
		PMD_DRV_LOG(ERR, "Hwdev or mac_addr is NULL");
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	mac_info.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_MAC,
				     &mac_info, sizeof(mac_info),
				     &mac_info, &out_size);
	if (err || !out_size || mac_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get mac, err: %d, status: 0x%x, out size: 0x%x",
			err, mac_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	memmove(mac_addr, mac_info.mac, ETH_ALEN);

	return 0;
}

/**
*  hinic_update_mac - Update mac address to hardware.
*
* @param hwdev
*   The hardware interface of a nic device.
* @param old_mac
*   Old mac address.
* @param new_mac
*   New mac address.
* @param vlan_id
*   Set 0 for mac_vlan table initialization.
* @param func_id
*   Global function id of NIC.
*
* @return
*   0 on success.
*   negative error value otherwise.
*/
int hinic_update_mac(void *hwdev, u8 *old_mac, u8 *new_mac, u16 vlan_id,
		     u16 func_id)
{
	struct hinic_port_mac_update mac_info;
	u16 out_size = sizeof(mac_info);
	int err;

	if (!hwdev || !old_mac || !new_mac) {
		PMD_DRV_LOG(ERR, "Hwdev, old_mac or new_mac is NULL");
		return -EINVAL;
	}

	memset(&mac_info, 0, sizeof(mac_info));
	mac_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	mac_info.func_id = func_id;
	mac_info.vlan_id = vlan_id;
	memcpy(mac_info.old_mac, old_mac, ETH_ALEN);
	memcpy(mac_info.new_mac, new_mac, ETH_ALEN);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_UPDATE_MAC,
				     &mac_info, sizeof(mac_info),
				     &mac_info, &out_size);
	if (err || !out_size ||
	    (mac_info.mgmt_msg_head.status &&
	     mac_info.mgmt_msg_head.status != HINIC_PF_SET_VF_ALREADY)) {
		PMD_DRV_LOG(ERR, "Failed to update MAC, err: %d, status: 0x%x, out size: 0x%x",
			    err, mac_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}
	if (mac_info.mgmt_msg_head.status == HINIC_PF_SET_VF_ALREADY) {
		PMD_DRV_LOG(WARNING, "PF has already set vf mac, Ignore update operation");
		return HINIC_PF_SET_VF_ALREADY;
	}

	return 0;
}

/**
 * hinic_set_port_mtu -  Set MTU to port.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param new_mtu
 *   MTU size.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_port_mtu(void *hwdev, u32 new_mtu)
{
	struct hinic_mtu mtu_info;
	u16 out_size = sizeof(mtu_info);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&mtu_info, 0, sizeof(mtu_info));
	mtu_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	mtu_info.func_id = hinic_global_func_id(hwdev);
	mtu_info.mtu = new_mtu;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_CHANGE_MTU,
				     &mtu_info, sizeof(mtu_info),
				     &mtu_info, &out_size);
	if (err || !out_size || mtu_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set mtu, err: %d, status: 0x%x, out size: 0x%x",
			err, mtu_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_add_remove_vlan - Add or remove vlan id to vlan elb table.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param vlan_id
 *   Vlan id.
 * @param func_id
 *   Global function id of NIC.
 * @param add
 *   Add or remove operation.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_add_remove_vlan(void *hwdev, u16 vlan_id, u16 func_id, bool add)
{
	struct hinic_vlan_config vlan_info;
	u16 out_size = sizeof(vlan_info);
	u8 cmd;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	cmd = add ? HINIC_PORT_CMD_ADD_VLAN : HINIC_PORT_CMD_DEL_VLAN;

	memset(&vlan_info, 0, sizeof(vlan_info));
	vlan_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	vlan_info.func_id = func_id;
	vlan_info.vlan_id = vlan_id;

	err = l2nic_msg_to_mgmt_sync(hwdev, cmd, &vlan_info, sizeof(vlan_info),
				     &vlan_info, &out_size);
	if (err || !out_size || vlan_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to %s vlan, err: %d, status: 0x%x, out size: 0x%x",
			add ? "add" : "remove", err,
			vlan_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_config_vlan_filter - Enable or Disable vlan filter.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param vlan_filter_ctrl
 *   Enable or Disable.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_config_vlan_filter(void *hwdev, u32 vlan_filter_ctrl)
{
	struct hinic_hwdev *nic_hwdev = (struct hinic_hwdev *)hwdev;
	struct hinic_vlan_filter vlan_filter;
	u16 out_size = sizeof(vlan_filter);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&vlan_filter, 0, sizeof(vlan_filter));
	vlan_filter.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	vlan_filter.func_id = hinic_global_func_id(nic_hwdev);
	vlan_filter.vlan_filter_ctrl = vlan_filter_ctrl;

	err = l2nic_msg_to_mgmt_sync(nic_hwdev, HINIC_PORT_CMD_SET_VLAN_FILTER,
				     &vlan_filter, sizeof(vlan_filter),
				     &vlan_filter, &out_size);
	if (vlan_filter.mgmt_msg_head.status == HINIC_MGMT_CMD_UNSUPPORTED) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
	} else if ((err == HINIC_MBOX_VF_CMD_ERROR) &&
		(HINIC_IS_VF(nic_hwdev))) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
	} else if (err || !out_size || vlan_filter.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to config vlan filter, vlan_filter_ctrl: 0x%x, err: %d, status: 0x%x, out size: 0x%x",
			vlan_filter_ctrl, err,
			vlan_filter.mgmt_msg_head.status, out_size);
		err = -EIO;
	}

	return err;
}

/**
 * hinic_set_rx_vlan_offload - Enable or Disable vlan offload.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param en
 *   Enable or Disable.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_rx_vlan_offload(void *hwdev, u8 en)
{
	struct hinic_vlan_offload vlan_cfg;
	u16 out_size = sizeof(vlan_cfg);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	vlan_cfg.func_id = hinic_global_func_id(hwdev);
	vlan_cfg.vlan_rx_offload = en;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_RX_VLAN_OFFLOAD,
				     &vlan_cfg, sizeof(vlan_cfg),
				     &vlan_cfg, &out_size);
	if (err || !out_size || vlan_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to set rx vlan offload, err: %d, status: 0x%x, out size: 0x%x",
			err, vlan_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_get_link_status - Get link status from hardware.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param link_state
 *   Link status.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_get_link_status(void *hwdev, u8 *link_state)
{
	struct hinic_get_link get_link;
	u16 out_size = sizeof(get_link);
	int err;

	if (!hwdev || !link_state) {
		PMD_DRV_LOG(ERR, "Hwdev or link_state is NULL");
		return -EINVAL;
	}

	memset(&get_link, 0, sizeof(get_link));
	get_link.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	get_link.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_LINK_STATE,
				     &get_link, sizeof(get_link),
				     &get_link, &out_size);
	if (err || !out_size || get_link.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get link state, err: %d, status: 0x%x, out size: 0x%x",
			err, get_link.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	*link_state = get_link.link_status;

	return 0;
}

/**
 * hinic_set_vport_enable - Notify firmware that driver is ready or not.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param enable
 *   1: driver is ready; 0: driver is not ok.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_vport_enable(void *hwdev, bool enable)
{
	struct hinic_vport_state en_state;
	u16 out_size = sizeof(en_state);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&en_state, 0, sizeof(en_state));
	en_state.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	en_state.func_id = hinic_global_func_id(hwdev);
	en_state.state = (enable ? 1 : 0);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_VPORT_ENABLE,
				     &en_state, sizeof(en_state),
				     &en_state, &out_size);
	if (err || !out_size || en_state.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set vport state, err: %d, status: 0x%x, out size: 0x%x",
			err, en_state.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_set_port_enable - Open MAG to receive packets.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param enable
 *   1: open MAG; 0: close MAG.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_port_enable(void *hwdev, bool enable)
{
	struct hinic_port_state en_state;
	u16 out_size = sizeof(en_state);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	if (HINIC_IS_VF((struct hinic_hwdev *)hwdev))
		return 0;

	memset(&en_state, 0, sizeof(en_state));
	en_state.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	en_state.state = (enable ? HINIC_PORT_ENABLE : HINIC_PORT_DISABLE);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_PORT_ENABLE,
				     &en_state, sizeof(en_state),
				     &en_state, &out_size);
	if (err || !out_size || en_state.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set phy port state, err: %d, status: 0x%x, out size: 0x%x",
			err, en_state.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_get_port_info(void *hwdev, struct nic_port_info *port_info)
{
	struct hinic_port_info port_msg;
	u16 out_size = sizeof(port_msg);
	int err;

	if (!hwdev || !port_info) {
		PMD_DRV_LOG(ERR, "Hwdev or port_info is NULL");
		return -EINVAL;
	}

	memset(&port_msg, 0, sizeof(port_msg));
	port_msg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_msg.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_PORT_INFO,
				     &port_msg, sizeof(port_msg),
				     &port_msg, &out_size);
	if (err || !out_size || port_msg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to get port info, err: %d, status: 0x%x, out size: 0x%x",
			err, port_msg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	port_info->autoneg_cap = port_msg.autoneg_cap;
	port_info->autoneg_state = port_msg.autoneg_state;
	port_info->duplex = port_msg.duplex;
	port_info->port_type = port_msg.port_type;
	port_info->speed = port_msg.speed;

	return 0;
}

int hinic_set_pause_config(void *hwdev, struct nic_pause_config nic_pause)
{
	struct hinic_pause_config pause_info;
	u16 out_size = sizeof(pause_info);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&pause_info, 0, sizeof(pause_info));
	pause_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	pause_info.func_id = hinic_global_func_id(hwdev);
	pause_info.auto_neg = nic_pause.auto_neg;
	pause_info.rx_pause = nic_pause.rx_pause;
	pause_info.tx_pause = nic_pause.tx_pause;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_PAUSE_INFO,
				     &pause_info, sizeof(pause_info),
				     &pause_info, &out_size);
	if (err || !out_size || pause_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set pause info, err: %d, status: 0x%x, out size: 0x%x",
			err, pause_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_get_pause_info(void *hwdev, struct nic_pause_config *nic_pause)
{
	struct hinic_pause_config pause_info;
	u16 out_size = sizeof(pause_info);
	int err;

	if (!hwdev || !nic_pause)
		return -EINVAL;

	memset(&pause_info, 0, sizeof(pause_info));
	pause_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	pause_info.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_PAUSE_INFO,
				     &pause_info, sizeof(pause_info),
				     &pause_info, &out_size);
	if (err || !out_size || pause_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get pause info, err: %d, status: 0x%x, out size: 0x%x\n",
			err, pause_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	nic_pause->auto_neg = pause_info.auto_neg;
	nic_pause->rx_pause = pause_info.rx_pause;
	nic_pause->tx_pause = pause_info.tx_pause;

	return 0;
}

int hinic_dcb_set_ets(void *hwdev, u8 *up_tc, u8 *pg_bw,
		      u8 *pgid, u8 *up_bw, u8 *prio)
{
	struct hinic_up_ets_cfg ets;
	u16 out_size = sizeof(ets);
	u16 up_bw_t = 0;
	u8 pg_bw_t = 0;
	int i, err;

	if (!hwdev || !up_tc || !pg_bw || !pgid || !up_bw || !prio) {
		PMD_DRV_LOG(ERR, "Hwdev, up_tc, pg_bw, pgid, up_bw or prio is NULL");
		return -EINVAL;
	}

	for (i = 0; i < HINIC_DCB_TC_MAX; i++) {
		up_bw_t += *(up_bw + i);
		pg_bw_t += *(pg_bw + i);

		if (*(up_tc + i) > HINIC_DCB_TC_MAX) {
			PMD_DRV_LOG(ERR, "Invalid up %d mapping tc: %d", i,
				*(up_tc + i));
			return -EINVAL;
		}
	}

	if (pg_bw_t != 100 || (up_bw_t % 100) != 0) {
		PMD_DRV_LOG(ERR,
			"Invalid pg_bw: %d or up_bw: %d", pg_bw_t, up_bw_t);
		return -EINVAL;
	}

	memset(&ets, 0, sizeof(ets));
	ets.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	ets.port_id = 0;	/* reserved */
	memcpy(ets.up_tc, up_tc, HINIC_DCB_TC_MAX);
	memcpy(ets.pg_bw, pg_bw, HINIC_DCB_UP_MAX);
	memcpy(ets.pgid, pgid, HINIC_DCB_UP_MAX);
	memcpy(ets.up_bw, up_bw, HINIC_DCB_UP_MAX);
	memcpy(ets.prio, prio, HINIC_DCB_UP_MAX);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_ETS,
				     &ets, sizeof(ets), &ets, &out_size);
	if (err || ets.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			"Failed to set ets, err: %d, status: 0x%x, out size: 0x%x",
			err, ets.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_get_vport_stats(void *hwdev, struct hinic_vport_stats *stats)
{
	struct hinic_port_stats_info vport_stats_cmd;
	struct hinic_cmd_vport_stats vport_stats_rsp;
	u16 out_size = sizeof(vport_stats_rsp);
	int err;

	if (!hwdev || !stats) {
		PMD_DRV_LOG(ERR, "Hwdev or stats is NULL");
		return -EINVAL;
	}

	memset(&vport_stats_rsp, 0, sizeof(vport_stats_rsp));
	memset(&vport_stats_cmd, 0, sizeof(vport_stats_cmd));
	vport_stats_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	vport_stats_cmd.stats_version = HINIC_PORT_STATS_VERSION;
	vport_stats_cmd.func_id = hinic_global_func_id(hwdev);
	vport_stats_cmd.stats_size = sizeof(vport_stats_rsp);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_VPORT_STAT,
				     &vport_stats_cmd, sizeof(vport_stats_cmd),
				     &vport_stats_rsp, &out_size);
	if (err || !out_size || vport_stats_rsp.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Get vport stats from fw failed, err: %d, status: 0x%x, out size: 0x%x",
			err, vport_stats_rsp.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	memcpy(stats, &vport_stats_rsp.stats, sizeof(*stats));

	return 0;
}

int hinic_get_phy_port_stats(void *hwdev, struct hinic_phy_port_stats *stats)
{
	struct hinic_port_stats_info port_stats_cmd;
	struct hinic_port_stats port_stats_rsp;
	u16 out_size = sizeof(port_stats_rsp);
	int err;

	if (!hwdev || !stats) {
		PMD_DRV_LOG(ERR, "Hwdev or stats is NULL");
		return -EINVAL;
	}

	memset(&port_stats_rsp, 0, sizeof(port_stats_rsp));
	memset(&port_stats_cmd, 0, sizeof(port_stats_cmd));
	port_stats_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_stats_cmd.stats_version = HINIC_PORT_STATS_VERSION;
	port_stats_cmd.stats_size = sizeof(port_stats_rsp);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_PORT_STATISTICS,
				     &port_stats_cmd, sizeof(port_stats_cmd),
				     &port_stats_rsp, &out_size);
	if (err || !out_size || port_stats_rsp.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to get port statistics, err: %d, status: 0x%x, out size: 0x%x",
			err, port_stats_rsp.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	memcpy(stats, &port_stats_rsp.stats, sizeof(*stats));

	return 0;
}

int hinic_set_rss_type(void *hwdev, u32 tmpl_idx, struct nic_rss_type rss_type)
{
	struct nic_rss_context_tbl *ctx_tbl;
	struct hinic_cmd_buf *cmd_buf;
	u32 ctx = 0;
	u64 out_param;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	cmd_buf = hinic_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	ctx |= HINIC_RSS_TYPE_SET(1, VALID) |
		HINIC_RSS_TYPE_SET(rss_type.ipv4, IPV4) |
		HINIC_RSS_TYPE_SET(rss_type.ipv6, IPV6) |
		HINIC_RSS_TYPE_SET(rss_type.ipv6_ext, IPV6_EXT) |
		HINIC_RSS_TYPE_SET(rss_type.tcp_ipv4, TCP_IPV4) |
		HINIC_RSS_TYPE_SET(rss_type.tcp_ipv6, TCP_IPV6) |
		HINIC_RSS_TYPE_SET(rss_type.tcp_ipv6_ext, TCP_IPV6_EXT) |
		HINIC_RSS_TYPE_SET(rss_type.udp_ipv4, UDP_IPV4) |
		HINIC_RSS_TYPE_SET(rss_type.udp_ipv6, UDP_IPV6);

	cmd_buf->size = sizeof(struct nic_rss_context_tbl);

	ctx_tbl = (struct nic_rss_context_tbl *)cmd_buf->buf;
	ctx_tbl->group_index = cpu_to_be32(tmpl_idx);
	ctx_tbl->offset = 0;
	ctx_tbl->size = sizeof(u32);
	ctx_tbl->size = cpu_to_be32(ctx_tbl->size);
	ctx_tbl->rsvd = 0;
	ctx_tbl->ctx = cpu_to_be32(ctx);

	/* cfg the rss context table by command queue */
	err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
				     HINIC_MOD_L2NIC,
				     HINIC_UCODE_CMD_SET_RSS_CONTEXT_TABLE,
				     cmd_buf, &out_param, 0);

	hinic_free_cmd_buf(hwdev, cmd_buf);

	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR, "Failed to set rss context table");
		return -EIO;
	}

	return 0;
}

int hinic_get_rss_type(void *hwdev, u32 tmpl_idx, struct nic_rss_type *rss_type)
{
	struct hinic_rss_context_table ctx_tbl;
	u16 out_size = sizeof(ctx_tbl);
	int err;

	if (!hwdev || !rss_type) {
		PMD_DRV_LOG(ERR, "Hwdev or rss_type is NULL");
		return -EINVAL;
	}

	ctx_tbl.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	ctx_tbl.func_id = hinic_global_func_id(hwdev);
	ctx_tbl.template_id = (u8)tmpl_idx;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_RSS_CTX_TBL,
				     &ctx_tbl, sizeof(ctx_tbl),
				     &ctx_tbl, &out_size);
	if (err || !out_size || ctx_tbl.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to get hash type, err: %d, status: 0x%x, out size: 0x%x",
			err, ctx_tbl.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	rss_type->ipv4 = HINIC_RSS_TYPE_GET(ctx_tbl.context, IPV4);
	rss_type->ipv6 = HINIC_RSS_TYPE_GET(ctx_tbl.context, IPV6);
	rss_type->ipv6_ext = HINIC_RSS_TYPE_GET(ctx_tbl.context, IPV6_EXT);
	rss_type->tcp_ipv4 = HINIC_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV4);
	rss_type->tcp_ipv6 = HINIC_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV6);
	rss_type->tcp_ipv6_ext =
			HINIC_RSS_TYPE_GET(ctx_tbl.context, TCP_IPV6_EXT);
	rss_type->udp_ipv4 = HINIC_RSS_TYPE_GET(ctx_tbl.context, UDP_IPV4);
	rss_type->udp_ipv6 = HINIC_RSS_TYPE_GET(ctx_tbl.context, UDP_IPV6);

	return 0;
}

int hinic_rss_set_template_tbl(void *hwdev, u32 tmpl_idx, u8 *temp)
{
	struct hinic_rss_template_key temp_key;
	u16 out_size = sizeof(temp_key);
	int err;

	if (!hwdev || !temp) {
		PMD_DRV_LOG(ERR, "Hwdev or temp is NULL");
		return -EINVAL;
	}

	memset(&temp_key, 0, sizeof(temp_key));
	temp_key.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	temp_key.func_id = hinic_global_func_id(hwdev);
	temp_key.template_id = (u8)tmpl_idx;
	memcpy(temp_key.key, temp, HINIC_RSS_KEY_SIZE);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_RSS_TEMPLATE_TBL,
				     &temp_key, sizeof(temp_key),
				     &temp_key, &out_size);
	if (err || !out_size || temp_key.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to set hash key, err: %d, status: 0x%x, out size: 0x%x",
			err, temp_key.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_rss_get_template_tbl(void *hwdev, u32 tmpl_idx, u8 *temp)
{
	struct hinic_rss_template_key temp_key;
	u16 out_size = sizeof(temp_key);
	int err;

	if (!hwdev || !temp) {
		PMD_DRV_LOG(ERR, "Hwdev or temp is NULL");
		return -EINVAL;
	}

	memset(&temp_key, 0, sizeof(temp_key));
	temp_key.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	temp_key.func_id = hinic_global_func_id(hwdev);
	temp_key.template_id = (u8)tmpl_idx;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_RSS_TEMPLATE_TBL,
				     &temp_key, sizeof(temp_key),
				     &temp_key, &out_size);
	if (err || !out_size || temp_key.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get hash key, err: %d, status: 0x%x, out size: 0x%x",
			err, temp_key.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	memcpy(temp, temp_key.key, HINIC_RSS_KEY_SIZE);

	return 0;
}

/**
 * hinic_rss_set_hash_engine - Init rss hash function.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param tmpl_idx
 *   Index of rss template from NIC.
 * @param type
 *   Hash function, such as Toeplitz or XOR.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_rss_set_hash_engine(void *hwdev, u8 tmpl_idx, u8 type)
{
	struct hinic_rss_engine_type hash_type;
	u16 out_size = sizeof(hash_type);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&hash_type, 0, sizeof(hash_type));
	hash_type.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	hash_type.func_id = hinic_global_func_id(hwdev);
	hash_type.hash_engine = type;
	hash_type.template_id = tmpl_idx;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_RSS_HASH_ENGINE,
				     &hash_type, sizeof(hash_type),
				     &hash_type, &out_size);
	if (err || !out_size || hash_type.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get hash engine, err: %d, status: 0x%x, out size: 0x%x",
			err, hash_type.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_rss_set_indir_tbl(void *hwdev, u32 tmpl_idx, u32 *indir_table)
{
	struct nic_rss_indirect_tbl *indir_tbl;
	struct hinic_cmd_buf *cmd_buf;
	int i;
	u32 *temp;
	u32 indir_size;
	u64 out_param;
	int err;

	if (!hwdev || !indir_table) {
		PMD_DRV_LOG(ERR, "Hwdev or indir_table is NULL");
		return -EINVAL;
	}

	cmd_buf = hinic_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	cmd_buf->size = sizeof(struct nic_rss_indirect_tbl);
	indir_tbl = cmd_buf->buf;
	indir_tbl->group_index = cpu_to_be32(tmpl_idx);

	for (i = 0; i < HINIC_RSS_INDIR_SIZE; i++) {
		indir_tbl->entry[i] = (u8)(*(indir_table + i));

		if (0x3 == (i & 0x3)) {
			temp = (u32 *)&indir_tbl->entry[i - 3];
			*temp = cpu_to_be32(*temp);
		}
	}

	/* configure the rss indirect table by command queue */
	indir_size = HINIC_RSS_INDIR_SIZE / 2;
	indir_tbl->offset = 0;
	indir_tbl->size = cpu_to_be32(indir_size);

	err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
				     HINIC_MOD_L2NIC,
				     HINIC_UCODE_CMD_SET_RSS_INDIR_TABLE,
				     cmd_buf, &out_param, 0);
	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR, "Failed to set rss indir table");
		err = -EIO;
		goto free_buf;
	}

	indir_tbl->offset = cpu_to_be32(indir_size);
	indir_tbl->size = cpu_to_be32(indir_size);
	memcpy(indir_tbl->entry, &indir_tbl->entry[indir_size], indir_size);

	err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
				     HINIC_MOD_L2NIC,
				     HINIC_UCODE_CMD_SET_RSS_INDIR_TABLE,
				     cmd_buf, &out_param, 0);
	if (err || out_param != 0) {
		PMD_DRV_LOG(ERR, "Failed to set rss indir table");
		err = -EIO;
	}

free_buf:
	hinic_free_cmd_buf(hwdev, cmd_buf);

	return err;
}

int hinic_rss_get_indir_tbl(void *hwdev, u32 tmpl_idx, u32 *indir_table)
{
	struct hinic_rss_indir_table rss_cfg;
	u16 out_size = sizeof(rss_cfg);
	int err = 0, i;

	if (!hwdev || !indir_table) {
		PMD_DRV_LOG(ERR, "Hwdev or indir_table is NULL");
		return -EINVAL;
	}

	memset(&rss_cfg, 0, sizeof(rss_cfg));
	rss_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	rss_cfg.func_id = hinic_global_func_id(hwdev);
	rss_cfg.template_id = (u8)tmpl_idx;

	err = l2nic_msg_to_mgmt_sync(hwdev,
				     HINIC_PORT_CMD_GET_RSS_TEMPLATE_INDIR_TBL,
				     &rss_cfg, sizeof(rss_cfg), &rss_cfg,
				     &out_size);
	if (err || !out_size || rss_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get indir table, err: %d, status: 0x%x, out size: 0x%x",
			err, rss_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	hinic_be32_to_cpu(rss_cfg.indir, HINIC_RSS_INDIR_SIZE);
	for (i = 0; i < HINIC_RSS_INDIR_SIZE; i++)
		indir_table[i] = rss_cfg.indir[i];

	return 0;
}

int hinic_rss_cfg(void *hwdev, u8 rss_en, u8 tmpl_idx, u8 tc_num, u8 *prio_tc)
{
	struct hinic_rss_config rss_cfg;
	u16 out_size = sizeof(rss_cfg);
	int err;

	/* micro code required: number of TC should be power of 2 */
	if (!hwdev || !prio_tc || (tc_num & (tc_num - 1))) {
		PMD_DRV_LOG(ERR, "Hwdev or prio_tc is NULL, or tc_num: %u Not power of 2",
			tc_num);
		return -EINVAL;
	}

	memset(&rss_cfg, 0, sizeof(rss_cfg));
	rss_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	rss_cfg.func_id = hinic_global_func_id(hwdev);
	rss_cfg.rss_en = rss_en;
	rss_cfg.template_id = tmpl_idx;
	rss_cfg.rq_priority_number = tc_num ? (u8)ilog2(tc_num) : 0;

	memcpy(rss_cfg.prio_tc, prio_tc, HINIC_DCB_UP_MAX);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_RSS_CFG,
				     &rss_cfg, sizeof(rss_cfg), &rss_cfg,
				     &out_size);
	if (err || !out_size || rss_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set rss cfg, err: %d, status: 0x%x, out size: 0x%x",
			err, rss_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_rss_template_alloc - Get rss template id from the chip,
 * all functions share 96 templates.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param tmpl_idx
 *   Index of rss template from chip.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_rss_template_alloc(void *hwdev, u8 *tmpl_idx)
{
	struct hinic_rss_template_mgmt template_mgmt;
	u16 out_size = sizeof(template_mgmt);
	int err;

	if (!hwdev || !tmpl_idx) {
		PMD_DRV_LOG(ERR, "Hwdev or tmpl_idx is NULL");
		return -EINVAL;
	}

	memset(&template_mgmt, 0, sizeof(template_mgmt));
	template_mgmt.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	template_mgmt.func_id = hinic_global_func_id(hwdev);
	template_mgmt.cmd = NIC_RSS_CMD_TEMP_ALLOC;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_RSS_TEMP_MGR,
				     &template_mgmt, sizeof(template_mgmt),
				     &template_mgmt, &out_size);
	if (err || !out_size || template_mgmt.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to alloc rss template, err: %d, status: 0x%x, out size: 0x%x",
			err, template_mgmt.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	*tmpl_idx = template_mgmt.template_id;

	return 0;
}

/**
 * hinic_rss_template_free - Free rss template id to the chip.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param tmpl_idx
 *   Index of rss template from chip.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_rss_template_free(void *hwdev, u8 tmpl_idx)
{
	struct hinic_rss_template_mgmt template_mgmt;
	u16 out_size = sizeof(template_mgmt);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&template_mgmt, 0, sizeof(template_mgmt));
	template_mgmt.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	template_mgmt.func_id = hinic_global_func_id(hwdev);
	template_mgmt.template_id = tmpl_idx;
	template_mgmt.cmd = NIC_RSS_CMD_TEMP_FREE;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_RSS_TEMP_MGR,
				     &template_mgmt, sizeof(template_mgmt),
				     &template_mgmt, &out_size);
	if (err || !out_size || template_mgmt.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to free rss template, err: %d, status: 0x%x, out size: 0x%x",
			err, template_mgmt.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_set_rx_vhd_mode - Change rx buffer size after initialization.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param vhd_mode
 *   Not needed.
 * @param rx_buf_sz
 *   receive buffer size.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_set_rx_vhd_mode(void *hwdev, u16 vhd_mode, u16 rx_buf_sz)
{
	struct hinic_set_vhd_mode vhd_mode_cfg;
	u16 out_size = sizeof(vhd_mode_cfg);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&vhd_mode_cfg, 0, sizeof(vhd_mode_cfg));

	vhd_mode_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	vhd_mode_cfg.func_id = hinic_global_func_id(hwdev);
	vhd_mode_cfg.vhd_type = vhd_mode;
	vhd_mode_cfg.rx_wqe_buffer_size = rx_buf_sz;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_VHD_CFG,
				     &vhd_mode_cfg, sizeof(vhd_mode_cfg),
				     &vhd_mode_cfg, &out_size);
	if (err || !out_size || vhd_mode_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to set vhd mode, err: %d, status: 0x%x, out size: 0x%x",
			err, vhd_mode_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_set_rx_mode(void *hwdev, u32 enable)
{
	struct hinic_rx_mode_config rx_mode_cfg;
	u16 out_size = sizeof(rx_mode_cfg);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&rx_mode_cfg, 0, sizeof(rx_mode_cfg));
	rx_mode_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	rx_mode_cfg.func_id = hinic_global_func_id(hwdev);
	rx_mode_cfg.rx_mode = enable;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_RX_MODE,
				     &rx_mode_cfg, sizeof(rx_mode_cfg),
				     &rx_mode_cfg, &out_size);
	if (err || !out_size || rx_mode_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set rx mode, err: %d, status: 0x%x, out size: 0x%x",
			err, rx_mode_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_get_mgmt_version - Get mgmt module version from chip.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param fw
 *   Firmware version.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_get_mgmt_version(void *hwdev, char *fw)
{
	struct hinic_version_info fw_ver;
	u16 out_size = sizeof(fw_ver);
	int err;

	if (!hwdev || !fw) {
		PMD_DRV_LOG(ERR, "Hwdev or fw is NULL");
		return -EINVAL;
	}

	memset(&fw_ver, 0, sizeof(fw_ver));
	fw_ver.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_MGMT_VERSION,
				     &fw_ver, sizeof(fw_ver), &fw_ver,
				     &out_size);
	if (err || !out_size || fw_ver.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to get mgmt version, err: %d, status: 0x%x, out size: 0x%x\n",
			err, fw_ver.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	snprintf(fw, HINIC_MGMT_VERSION_MAX_LEN, "%s", fw_ver.ver);

	return 0;
}

int hinic_set_rx_csum_offload(void *hwdev, u32 en)
{
	struct hinic_checksum_offload rx_csum_cfg;
	u16 out_size = sizeof(rx_csum_cfg);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&rx_csum_cfg, 0, sizeof(rx_csum_cfg));
	rx_csum_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	rx_csum_cfg.func_id = hinic_global_func_id(hwdev);
	rx_csum_cfg.rx_csum_offload = en;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_RX_CSUM,
				     &rx_csum_cfg, sizeof(rx_csum_cfg),
				     &rx_csum_cfg, &out_size);
	if (err || !out_size || rx_csum_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to set rx csum offload, err: %d, status: 0x%x, out size: 0x%x",
			err, rx_csum_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_set_rx_lro(void *hwdev, u8 ipv4_en, u8 ipv6_en, u8 max_wqe_num)
{
	struct hinic_lro_config lro_cfg;
	u16 out_size = sizeof(lro_cfg);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&lro_cfg, 0, sizeof(lro_cfg));
	lro_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	lro_cfg.func_id = hinic_global_func_id(hwdev);
	lro_cfg.lro_ipv4_en = ipv4_en;
	lro_cfg.lro_ipv6_en = ipv6_en;
	lro_cfg.lro_max_wqe_num = max_wqe_num;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_LRO,
				     &lro_cfg, sizeof(lro_cfg), &lro_cfg,
				     &out_size);
	if (err || !out_size || lro_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to set lro offload, err: %d, status: 0x%x, out size: 0x%x",
			err, lro_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_set_anti_attack(void *hwdev, bool enable)
{
	struct hinic_port_anti_attack_rate rate;
	u16 out_size = sizeof(rate);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&rate, 0, sizeof(rate));
	rate.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	rate.func_id = hinic_global_func_id(hwdev);
	rate.enable = enable;
	rate.cir = ANTI_ATTACK_DEFAULT_CIR;
	rate.xir = ANTI_ATTACK_DEFAULT_XIR;
	rate.cbs = ANTI_ATTACK_DEFAULT_CBS;
	rate.xbs = ANTI_ATTACK_DEFAULT_XBS;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_ANTI_ATTACK_RATE,
				     &rate, sizeof(rate), &rate, &out_size);
	if (err || !out_size || rate.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Can't %s port Anti-Attack rate limit, err: %d, status: 0x%x, out size: 0x%x",
			(enable ? "enable" : "disable"), err,
			rate.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/* Set autoneg status and restart port link status */
int hinic_reset_port_link_cfg(void *hwdev)
{
	struct hinic_reset_link_cfg reset_cfg;
	u16 out_size = sizeof(reset_cfg);
	int err;

	memset(&reset_cfg, 0, sizeof(reset_cfg));
	reset_cfg.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	reset_cfg.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_RESET_LINK_CFG,
				     &reset_cfg, sizeof(reset_cfg),
				     &reset_cfg, &out_size);
	if (err || !out_size || reset_cfg.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Reset port link configure failed, err: %d, status: 0x%x, out size: 0x%x",
			err, reset_cfg.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_vf_func_init - Register VF to PF.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_vf_func_init(struct hinic_hwdev *hwdev)
{
	int err, state = 0;

	if (!HINIC_IS_VF(hwdev))
		return 0;

	err = hinic_mbox_to_pf(hwdev, HINIC_MOD_L2NIC,
			HINIC_PORT_CMD_VF_REGISTER, &state, sizeof(state),
			NULL, NULL, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Fail to register vf");
		return err;
	}

	return 0;
}

/**
 * hinic_vf_func_free - Unregister VF from PF.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 */
void hinic_vf_func_free(struct hinic_hwdev *hwdev)
{
	int err;

	if (hinic_func_type(hwdev) != TYPE_VF)
		return;

	err = hinic_mbox_to_pf(hwdev, HINIC_MOD_L2NIC,
				HINIC_PORT_CMD_VF_UNREGISTER, &err, sizeof(err),
				NULL, NULL, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Fail to unregister VF, err: %d", err);
}

int hinic_set_fast_recycle_mode(void *hwdev, u8 mode)
{
	struct hinic_fast_recycled_mode fast_recycled_mode;
	u16 out_size = sizeof(fast_recycled_mode);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&fast_recycled_mode, 0, sizeof(fast_recycled_mode));
	fast_recycled_mode.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	fast_recycled_mode.func_id = hinic_global_func_id(hwdev);
	fast_recycled_mode.fast_recycled_mode = mode;

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_FAST_RECYCLE_MODE_SET,
				     &fast_recycled_mode,
				     sizeof(fast_recycled_mode),
				     &fast_recycled_mode, &out_size, 0);
	if (err || fast_recycled_mode.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR, "Failed to set recycle mode, err: %d, status: 0x%x, out size: 0x%x",
			err, fast_recycled_mode.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_clear_vport_stats(struct hinic_hwdev *hwdev)
{
	struct hinic_clear_vport_stats clear_vport_stats;
	u16 out_size = sizeof(clear_vport_stats);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&clear_vport_stats, 0, sizeof(clear_vport_stats));
	clear_vport_stats.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	clear_vport_stats.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_CLEAN_VPORT_STAT,
				     &clear_vport_stats,
				     sizeof(clear_vport_stats),
				     &clear_vport_stats, &out_size);
	if (err || !out_size || clear_vport_stats.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to clear vport statistics, err: %d, status: 0x%x, out size: 0x%x",
			err, clear_vport_stats.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_clear_phy_port_stats(struct hinic_hwdev *hwdev)
{
	struct hinic_clear_port_stats clear_phy_port_stats;
	u16 out_size = sizeof(clear_phy_port_stats);
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&clear_phy_port_stats, 0, sizeof(clear_phy_port_stats));
	clear_phy_port_stats.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	clear_phy_port_stats.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev,
				     HINIC_PORT_CMD_CLEAR_PORT_STATISTICS,
				     &clear_phy_port_stats,
				     sizeof(clear_phy_port_stats),
				     &clear_phy_port_stats, &out_size);
	if (err || !out_size || clear_phy_port_stats.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to clear phy port statistics, err: %d, status: 0x%x, out size: 0x%x",
			err, clear_phy_port_stats.mgmt_msg_head.status,
			out_size);
		return -EIO;
	}

	return 0;
}

int hinic_set_link_status_follow(void *hwdev,
				 enum hinic_link_follow_status status)
{
	struct hinic_set_link_follow follow;
	u16 out_size = sizeof(follow);
	int err;

	if (!hwdev)
		return -EINVAL;

	if (HINIC_IS_VF((struct hinic_hwdev *)hwdev))
		return 0;

	if (status >= HINIC_LINK_FOLLOW_STATUS_MAX) {
		PMD_DRV_LOG(ERR, "Invalid link follow status: %d", status);
		return -EINVAL;
	}

	memset(&follow, 0, sizeof(follow));
	follow.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	follow.func_id = hinic_global_func_id(hwdev);
	follow.follow_status = status;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_SET_LINK_FOLLOW,
				     &follow, sizeof(follow),
				     &follow, &out_size);
	if ((follow.mgmt_msg_head.status != HINIC_MGMT_CMD_UNSUPPORTED &&
	     follow.mgmt_msg_head.status) || err || !out_size) {
		PMD_DRV_LOG(ERR,
			"Failed to set link status follow phy port status, err: %d, status: 0x%x, out size: 0x%x",
			err, follow.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return follow.mgmt_msg_head.status;
}

int hinic_get_link_mode(void *hwdev, u32 *supported, u32 *advertised)
{
	struct hinic_link_mode_cmd link_mode;
	u16 out_size = sizeof(link_mode);
	int err;

	if (!hwdev || !supported || !advertised)
		return -EINVAL;

	memset(&link_mode, 0, sizeof(link_mode));
	link_mode.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	link_mode.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_GET_LINK_MODE,
				     &link_mode, sizeof(link_mode),
				     &link_mode, &out_size);
	if (err || !out_size || link_mode.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Failed to get link mode, err: %d, status: 0x%x, out size: 0x%x",
			err, link_mode.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	*supported = link_mode.supported;
	*advertised = link_mode.advertised;

	return 0;
}

/**
 * hinic_flush_qp_res - Flush tx && rx chip resources in case of set vport
 * fake failed when device start.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_flush_qp_res(void *hwdev)
{
	struct hinic_clear_qp_resource qp_res;
	u16 out_size = sizeof(qp_res);
	int err;

	memset(&qp_res, 0, sizeof(qp_res));
	qp_res.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	qp_res.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_CLEAR_QP_RES,
				     &qp_res, sizeof(qp_res), &qp_res,
				     &out_size);
	if (err || !out_size || qp_res.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Failed to clear sq resources, err: %d, status: 0x%x, out size: 0x%x",
			err, qp_res.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_vf_get_default_cos - Get default cos of VF.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param cos_id
 *   Cos value.
 *
 * @return
 *   0 on success.
 *   negative error value otherwise.
 */
int hinic_vf_get_default_cos(struct hinic_hwdev *hwdev, u8 *cos_id)
{
	struct hinic_vf_default_cos vf_cos;
	u16 out_size = sizeof(vf_cos);
	int err;

	memset(&vf_cos, 0, sizeof(vf_cos));
	vf_cos.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_L2NIC,
				     HINIC_PORT_CMD_GET_VF_COS, &vf_cos,
				     sizeof(vf_cos), &vf_cos, &out_size, 0);
	if (err || !out_size || vf_cos.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Get VF default cos failed, err: %d, status: 0x%x, out size: 0x%x",
			err, vf_cos.mgmt_msg_head.status, out_size);
		return -EIO;
	}
	*cos_id = vf_cos.state.default_cos;

	return 0;
}

/**
 * hinic_set_fdir_filter - Set fdir filter for control path
 * packet to notify firmware.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param filter_type
 *   Packet type to filter.
 * @param qid
 *   Rx qid to filter.
 * @param type_enable
 *   The status of pkt type filter.
 * @param enable
 *   Fdir function Enable or Disable.
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_set_fdir_filter(void *hwdev, u8 filter_type, u8 qid, u8 type_enable,
			  bool enable)
{
	struct hinic_port_qfilter_info port_filer_cmd;
	u16 out_size = sizeof(port_filer_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&port_filer_cmd, 0, sizeof(port_filer_cmd));
	port_filer_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_filer_cmd.func_id = hinic_global_func_id(hwdev);
	port_filer_cmd.filter_enable = (u8)enable;
	port_filer_cmd.filter_type = filter_type;
	port_filer_cmd.qid = qid;
	port_filer_cmd.filter_type_enable = type_enable;
	port_filer_cmd.fdir_flag = 0;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_Q_FILTER,
			&port_filer_cmd, sizeof(port_filer_cmd),
			&port_filer_cmd, &out_size);
	if (err || !out_size || port_filer_cmd.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Set port Q filter failed, err: %d, status: 0x%x, out size: 0x%x, type: 0x%x,"
			" enable: 0x%x, qid: 0x%x, filter_type_enable: 0x%x\n",
			err, port_filer_cmd.mgmt_msg_head.status, out_size,
			filter_type, enable, qid, type_enable);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_set_normal_filter - Set fdir filter for IO path packet.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param qid
 *   Rx qid to filter.
 * @param normal_type_enable
 *   IO path packet function Enable or Disable
 * @param key
 *   IO path packet filter key value, such as DIP from pkt.
 * @param enable
 *   Fdir function Enable or Disable.
 * @param flag
 *   Filter flag, such as dip or others.
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_set_normal_filter(void *hwdev, u8 qid, u8 normal_type_enable,
				u32 key, bool enable, u8 flag)
{
	struct hinic_port_qfilter_info port_filer_cmd;
	u16 out_size = sizeof(port_filer_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&port_filer_cmd, 0, sizeof(port_filer_cmd));
	port_filer_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_filer_cmd.func_id = hinic_global_func_id(hwdev);
	port_filer_cmd.filter_enable = (u8)enable;
	port_filer_cmd.qid = qid;
	port_filer_cmd.normal_type_enable = normal_type_enable;
	port_filer_cmd.fdir_flag = flag; /* fdir flag: support dip */
	port_filer_cmd.key = key;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_Q_FILTER,
			&port_filer_cmd, sizeof(port_filer_cmd),
			&port_filer_cmd, &out_size);
	if (err || !out_size || port_filer_cmd.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Set normal filter failed, err: %d, status: 0x%x, out size: 0x%x, fdir_flag: 0x%x,"
			" enable: 0x%x, qid: 0x%x, normal_type_enable: 0x%x, key:0x%x\n",
			err, port_filer_cmd.mgmt_msg_head.status, out_size,
			flag, enable, qid, normal_type_enable, key);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_set_fdir_tcam - Set fdir filter for control packet
 * by tcam table to notify hardware.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param type_mask
 *   Index of TCAM.
 * @param filter_rule
 *   TCAM rule for control packet, such as lacp or bgp.
 * @param filter_action
 *   TCAM action for control packet, such as accept or drop.
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_set_fdir_tcam(void *hwdev, u16 type_mask,
			struct tag_pa_rule *filter_rule,
			struct tag_pa_action *filter_action)
{
	struct hinic_fdir_tcam_info port_tcam_cmd;
	u16 out_size = sizeof(port_tcam_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&port_tcam_cmd, 0, sizeof(port_tcam_cmd));
	port_tcam_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_tcam_cmd.tcam_index = type_mask;
	port_tcam_cmd.flag = TCAM_SET;
	memcpy((void *)&port_tcam_cmd.filter_rule,
		(void *)filter_rule, sizeof(struct tag_pa_rule));
	memcpy((void *)&port_tcam_cmd.filter_action,
		(void *)filter_action, sizeof(struct tag_pa_action));

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_TCAM_FILTER,
			&port_tcam_cmd, sizeof(port_tcam_cmd),
			&port_tcam_cmd, &out_size);
	if (err || !out_size || port_tcam_cmd.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Set tcam table failed, err: %d, status: 0x%x, out size: 0x%x",
			err, port_tcam_cmd.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_clear_fdir_tcam - Clear fdir filter TCAM table for control packet.
 *
 * @param hwdev
 *   The hardware interface of a nic device.
 * @param type_mask
 *   Index of TCAM.
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_clear_fdir_tcam(void *hwdev, u16 type_mask)
{
	struct hinic_fdir_tcam_info port_tcam_cmd;
	u16 out_size = sizeof(port_tcam_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&port_tcam_cmd, 0, sizeof(port_tcam_cmd));
	port_tcam_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_tcam_cmd.tcam_index = type_mask;
	port_tcam_cmd.flag = TCAM_CLEAR;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_TCAM_FILTER,
			&port_tcam_cmd, sizeof(port_tcam_cmd),
			&port_tcam_cmd, &out_size);
	if (err || !out_size || port_tcam_cmd.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR, "Clear tcam table failed, err: %d, status: 0x%x, out size: 0x%x",
			err, port_tcam_cmd.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_add_tcam_rule(void *hwdev, struct tag_tcam_cfg_rule *tcam_rule)
{
	u16 out_size = sizeof(struct tag_fdir_add_rule_cmd);
	struct tag_fdir_add_rule_cmd tcam_cmd;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	if (tcam_rule->index >= HINIC_MAX_TCAM_RULES_NUM) {
		PMD_DRV_LOG(ERR, "Tcam rules num to add is invalid");
		return -EINVAL;
	}

	memset(&tcam_cmd, 0, sizeof(struct tag_fdir_add_rule_cmd));
	tcam_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	memcpy((void *)&tcam_cmd.rule, (void *)tcam_rule,
		sizeof(struct tag_tcam_cfg_rule));

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_UP_TC_ADD_FLOW,
				&tcam_cmd, sizeof(tcam_cmd),
				&tcam_cmd, &out_size);
	if (err || tcam_cmd.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			"Add tcam rule failed, err: %d, status: 0x%x, out size: 0x%x",
			err, tcam_cmd.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

int hinic_del_tcam_rule(void *hwdev, u32 index)
{
	u16 out_size = sizeof(struct tag_fdir_del_rule_cmd);
	struct tag_fdir_del_rule_cmd tcam_cmd;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	if (index >= HINIC_MAX_TCAM_RULES_NUM) {
		PMD_DRV_LOG(ERR, "Tcam rules num to del is invalid");
		return -EINVAL;
	}

	memset(&tcam_cmd, 0, sizeof(struct tag_fdir_del_rule_cmd));
	tcam_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	tcam_cmd.index_start = index;
	tcam_cmd.index_num = 1;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_UP_TC_DEL_FLOW,
				&tcam_cmd, sizeof(tcam_cmd),
				&tcam_cmd, &out_size);
	if (err || tcam_cmd.mgmt_msg_head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			"Del tcam rule failed, err: %d, status: 0x%x, out size: 0x%x",
			err, tcam_cmd.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

static int hinic_mgmt_tcam_block(void *hwdev, u8 alloc_en,
				u8 block_type, u16 *index)
{
	struct hinic_cmd_ctrl_tcam_block tcam_block_info;
	u16 out_size = sizeof(struct hinic_cmd_ctrl_tcam_block);
	struct hinic_hwdev *nic_hwdev = (struct hinic_hwdev *)hwdev;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&tcam_block_info, 0, sizeof(struct hinic_cmd_ctrl_tcam_block));
	tcam_block_info.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	tcam_block_info.func_id = hinic_global_func_id(hwdev);
	tcam_block_info.alloc_en = alloc_en;
	tcam_block_info.tcam_type = block_type;
	tcam_block_info.tcam_block_index = *index;

	err = l2nic_msg_to_mgmt_sync(hwdev,
				HINIC_PORT_CMD_UP_TC_CTRL_TCAM_BLOCK,
				&tcam_block_info, sizeof(tcam_block_info),
				&tcam_block_info, &out_size);
	if (tcam_block_info.mgmt_msg_head.status ==
		HINIC_MGMT_CMD_UNSUPPORTED) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(INFO, "Firmware/uP doesn't support alloc or del tcam block");
		return err;
	} else if ((err == HINIC_MBOX_VF_CMD_ERROR) &&
			(HINIC_IS_VF(nic_hwdev))) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(INFO, "VF doesn't support alloc and del tcam block.");
		return err;
	} else if (err || (!out_size) || tcam_block_info.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Set tcam block failed, err: %d, status: 0x%x, out size: 0x%x",
			err, tcam_block_info.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	if (alloc_en)
		*index = tcam_block_info.tcam_block_index;

	return 0;
}

int hinic_alloc_tcam_block(void *hwdev, u8 block_type, u16 *index)
{
	return hinic_mgmt_tcam_block(hwdev, HINIC_TCAM_BLOCK_ENABLE,
				block_type, index);
}

int hinic_free_tcam_block(void *hwdev, u8 block_type, u16 *index)
{
	return hinic_mgmt_tcam_block(hwdev, HINIC_TCAM_BLOCK_DISABLE,
				block_type, index);
}

int hinic_flush_tcam_rule(void *hwdev)
{
	struct hinic_cmd_flush_tcam_rules tcam_flush;
	u16 out_size = sizeof(struct hinic_cmd_flush_tcam_rules);
	struct hinic_hwdev *nic_hwdev = (struct hinic_hwdev *)hwdev;
	int err;

	if (!hwdev) {
		PMD_DRV_LOG(ERR, "Hwdev is NULL");
		return -EINVAL;
	}

	memset(&tcam_flush, 0, sizeof(struct hinic_cmd_flush_tcam_rules));
	tcam_flush.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	tcam_flush.func_id = hinic_global_func_id(hwdev);

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_UP_TC_FLUSH_TCAM,
			&tcam_flush, sizeof(struct hinic_cmd_flush_tcam_rules),
			&tcam_flush, &out_size);
	if (tcam_flush.mgmt_msg_head.status == HINIC_MGMT_CMD_UNSUPPORTED) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(INFO, "Firmware/uP doesn't support flush tcam fdir");
	} else if ((err == HINIC_MBOX_VF_CMD_ERROR) &&
			(HINIC_IS_VF(nic_hwdev))) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(INFO, "VF doesn't support flush tcam fdir");
	} else if (err || (!out_size) || tcam_flush.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Flush tcam fdir rules failed, err: %d, status: 0x%x, out size: 0x%x",
			err, tcam_flush.mgmt_msg_head.status, out_size);
		err = -EIO;
	}

	return err;
}

int hinic_set_fdir_tcam_rule_filter(void *hwdev, bool enable)
{
	struct hinic_port_tcam_info port_tcam_cmd;
	u16 out_size = sizeof(port_tcam_cmd);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&port_tcam_cmd, 0, sizeof(port_tcam_cmd));
	port_tcam_cmd.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	port_tcam_cmd.func_id = hinic_global_func_id(hwdev);
	port_tcam_cmd.tcam_enable = (u8)enable;

	err = l2nic_msg_to_mgmt_sync(hwdev, HINIC_PORT_CMD_UP_TC_ENABLE,
			&port_tcam_cmd, sizeof(port_tcam_cmd),
			&port_tcam_cmd, &out_size);
	if ((port_tcam_cmd.mgmt_msg_head.status != HINIC_MGMT_CMD_UNSUPPORTED &&
		port_tcam_cmd.mgmt_msg_head.status) || err || !out_size) {
		if (err == HINIC_MBOX_VF_CMD_ERROR &&
			HINIC_IS_VF((struct hinic_hwdev *)hwdev)) {
			err = HINIC_MGMT_CMD_UNSUPPORTED;
			PMD_DRV_LOG(WARNING, "VF doesn't support setting fdir tcam filter");
			return err;
		}
		PMD_DRV_LOG(ERR, "Set fdir tcam filter failed, err: %d, "
			"status: 0x%x, out size: 0x%x, enable: 0x%x",
			err, port_tcam_cmd.mgmt_msg_head.status, out_size,
			enable);
		return -EIO;
	}

	if (port_tcam_cmd.mgmt_msg_head.status == HINIC_MGMT_CMD_UNSUPPORTED) {
		err = HINIC_MGMT_CMD_UNSUPPORTED;
		PMD_DRV_LOG(WARNING, "Fw doesn't support setting fdir tcam filter");
	}

	return err;
}


