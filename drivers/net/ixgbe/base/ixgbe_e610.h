/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _IXGBE_E610_H_
#define _IXGBE_E610_H_

#include "ixgbe_type.h"

void ixgbe_init_aci(struct ixgbe_hw *hw);
void ixgbe_shutdown_aci(struct ixgbe_hw *hw);

s32 ixgbe_aci_send_cmd(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
		       void *buf, u16 buf_size);
bool ixgbe_aci_check_event_pending(struct ixgbe_hw *hw);
s32 ixgbe_aci_get_event(struct ixgbe_hw *hw, struct ixgbe_aci_event *e,
			bool *pending);

void ixgbe_fill_dflt_direct_cmd_desc(struct ixgbe_aci_desc *desc, u16 opcode);

s32 ixgbe_acquire_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      enum ixgbe_aci_res_access_type access, u32 timeout);
void ixgbe_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res);
s32 ixgbe_aci_list_caps(struct ixgbe_hw *hw, void *buf, u16 buf_size,
			u32 *cap_count, enum ixgbe_aci_opc opc);
s32 ixgbe_discover_dev_caps(struct ixgbe_hw *hw,
			    struct ixgbe_hw_dev_caps *dev_caps);
s32 ixgbe_discover_func_caps(struct ixgbe_hw* hw,
			     struct ixgbe_hw_func_caps* func_caps);
s32 ixgbe_get_caps(struct ixgbe_hw *hw);
s32 ixgbe_aci_disable_rxen(struct ixgbe_hw *hw);
s32 ixgbe_aci_get_phy_caps(struct ixgbe_hw *hw, bool qual_mods, u8 report_mode,
			   struct ixgbe_aci_cmd_get_phy_caps_data *pcaps);
bool ixgbe_phy_caps_equals_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
			       struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
void ixgbe_copy_phy_caps_to_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
				struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
s32 ixgbe_aci_set_phy_cfg(struct ixgbe_hw *hw,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *cfg);
s32 ixgbe_aci_set_link_restart_an(struct ixgbe_hw *hw, bool ena_link);
s32 ixgbe_update_link_info(struct ixgbe_hw *hw);
s32 ixgbe_get_link_status(struct ixgbe_hw *hw, bool *link_up);
s32 ixgbe_aci_get_link_info(struct ixgbe_hw *hw, bool ena_lse,
			    struct ixgbe_link_status *link);
s32 ixgbe_aci_set_event_mask(struct ixgbe_hw *hw, u8 port_num, u16 mask);
s32 ixgbe_configure_lse(struct ixgbe_hw *hw, bool activate, u16 mask);

s32 ixgbe_aci_get_netlist_node(struct ixgbe_hw *hw,
			       struct ixgbe_aci_cmd_get_link_topo *cmd,
			       u8 *node_part_number, u16 *node_handle);
s32 ixgbe_aci_get_netlist_node_pin(struct ixgbe_hw *hw,
				   struct ixgbe_aci_cmd_get_link_topo_pin *cmd,
				   u16 *node_handle);
s32 ixgbe_find_netlist_node(struct ixgbe_hw *hw, u8 node_type_ctx,
			    u8 node_part_number, u16 *node_handle);
s32 ixgbe_aci_sff_eeprom(struct ixgbe_hw *hw, u16 lport, u8 bus_addr,
			 u16 mem_addr, u8 page, u8 page_bank_ctrl, u8 *data,
			 u8 length, bool write);
s32 ixgbe_aci_get_internal_data(struct ixgbe_hw *hw, u16 cluster_id,
				u16 table_id, u32 start, void *buf,
				u16 buf_size, u16 *ret_buf_size,
				u16 *ret_next_cluster, u16 *ret_next_table,
				u32 *ret_next_index);
enum ixgbe_media_type ixgbe_get_media_type_E610(struct ixgbe_hw *hw);
s32 ixgbe_setup_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			  bool autoneg_wait);
s32 ixgbe_check_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			  bool *link_up, bool link_up_wait_to_complete);
s32 ixgbe_get_link_capabilities_E610(struct ixgbe_hw *hw,
				     ixgbe_link_speed *speed,
				     bool *autoneg);
s32 ixgbe_cfg_phy_fc(struct ixgbe_hw *hw,
		     struct ixgbe_aci_cmd_set_phy_cfg_data *cfg,
		     enum ixgbe_fc_mode req_mode);
s32 ixgbe_setup_fc_E610(struct ixgbe_hw *hw);
void ixgbe_fc_autoneg_E610(struct ixgbe_hw *hw);
void ixgbe_disable_rx_E610(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_E610(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_E610(struct ixgbe_hw *hw);
s32 ixgbe_identify_module_E610(struct ixgbe_hw *hw);
s32 ixgbe_setup_phy_link_E610(struct ixgbe_hw *hw);
s32 ixgbe_get_phy_firmware_version_E610(struct ixgbe_hw *hw,
					u16 *firmware_version);
s32 ixgbe_read_i2c_sff8472_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 *sff8472_data);
s32 ixgbe_read_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
			       u8 *eeprom_data);
s32 ixgbe_write_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 eeprom_data);
s32 ixgbe_check_overtemp_E610(struct ixgbe_hw *hw);
s32 ixgbe_set_phy_power_E610(struct ixgbe_hw *hw, bool on);
s32 ixgbe_enter_lplu_E610(struct ixgbe_hw *hw);

#endif /* _IXGBE_E610_H_ */
