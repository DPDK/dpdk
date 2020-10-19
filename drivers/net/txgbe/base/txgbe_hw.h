/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_HW_H_
#define _TXGBE_HW_H_

#include "txgbe_type.h"

s32 txgbe_init_hw(struct txgbe_hw *hw);
s32 txgbe_start_hw(struct txgbe_hw *hw);
s32 txgbe_stop_hw(struct txgbe_hw *hw);
s32 txgbe_start_hw_gen2(struct txgbe_hw *hw);
s32 txgbe_clear_hw_cntrs(struct txgbe_hw *hw);
s32 txgbe_get_mac_addr(struct txgbe_hw *hw, u8 *mac_addr);

void txgbe_set_lan_id_multi_port(struct txgbe_hw *hw);

s32 txgbe_set_rar(struct txgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
s32 txgbe_clear_rar(struct txgbe_hw *hw, u32 index);
s32 txgbe_init_rx_addrs(struct txgbe_hw *hw);
s32 txgbe_update_mc_addr_list(struct txgbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count,
				      txgbe_mc_addr_itr func, bool clear);
s32 txgbe_disable_sec_rx_path(struct txgbe_hw *hw);
s32 txgbe_enable_sec_rx_path(struct txgbe_hw *hw);
s32 txgbe_disable_sec_tx_path(struct txgbe_hw *hw);
s32 txgbe_enable_sec_tx_path(struct txgbe_hw *hw);

s32 txgbe_validate_mac_addr(u8 *mac_addr);
s32 txgbe_acquire_swfw_sync(struct txgbe_hw *hw, u32 mask);
void txgbe_release_swfw_sync(struct txgbe_hw *hw, u32 mask);

s32 txgbe_get_san_mac_addr(struct txgbe_hw *hw, u8 *san_mac_addr);
s32 txgbe_set_san_mac_addr(struct txgbe_hw *hw, u8 *san_mac_addr);

s32 txgbe_init_uta_tables(struct txgbe_hw *hw);
s32 txgbe_clear_vfta(struct txgbe_hw *hw);
s32 txgbe_check_mac_link(struct txgbe_hw *hw,
			       u32 *speed,
			       bool *link_up, bool link_up_wait_to_complete);

s32 txgbe_get_wwn_prefix(struct txgbe_hw *hw, u16 *wwnn_prefix,
				 u16 *wwpn_prefix);

s32 txgbe_get_device_caps(struct txgbe_hw *hw, u16 *device_caps);
void txgbe_clear_tx_pending(struct txgbe_hw *hw);

s32 txgbe_reset_pipeline_raptor(struct txgbe_hw *hw);

s32 txgbe_get_thermal_sensor_data(struct txgbe_hw *hw);
s32 txgbe_init_thermal_sensor_thresh(struct txgbe_hw *hw);

void txgbe_disable_rx(struct txgbe_hw *hw);
void txgbe_enable_rx(struct txgbe_hw *hw);
s32 txgbe_setup_mac_link_multispeed_fiber(struct txgbe_hw *hw,
					  u32 speed,
					  bool autoneg_wait_to_complete);
void txgbe_set_mta(struct txgbe_hw *hw, u8 *mc_addr);
s32 txgbe_init_shared_code(struct txgbe_hw *hw);
s32 txgbe_set_mac_type(struct txgbe_hw *hw);
s32 txgbe_init_ops_pf(struct txgbe_hw *hw);
s32 txgbe_get_link_capabilities_raptor(struct txgbe_hw *hw,
				      u32 *speed, bool *autoneg);
u32 txgbe_get_media_type_raptor(struct txgbe_hw *hw);
void txgbe_disable_tx_laser_multispeed_fiber(struct txgbe_hw *hw);
void txgbe_enable_tx_laser_multispeed_fiber(struct txgbe_hw *hw);
void txgbe_flap_tx_laser_multispeed_fiber(struct txgbe_hw *hw);
void txgbe_set_hard_rate_select_speed(struct txgbe_hw *hw,
					u32 speed);
s32 txgbe_setup_mac_link_smartspeed(struct txgbe_hw *hw,
				    u32 speed,
				    bool autoneg_wait_to_complete);
s32 txgbe_start_mac_link_raptor(struct txgbe_hw *hw,
			       bool autoneg_wait_to_complete);
s32 txgbe_setup_mac_link(struct txgbe_hw *hw, u32 speed,
			       bool autoneg_wait_to_complete);
s32 txgbe_setup_sfp_modules(struct txgbe_hw *hw);
void txgbe_init_mac_link_ops(struct txgbe_hw *hw);
s32 txgbe_reset_hw(struct txgbe_hw *hw);
s32 txgbe_start_hw_raptor(struct txgbe_hw *hw);
s32 txgbe_init_phy_raptor(struct txgbe_hw *hw);
s32 txgbe_enable_rx_dma_raptor(struct txgbe_hw *hw, u32 regval);
bool txgbe_verify_lesm_fw_enabled_raptor(struct txgbe_hw *hw);
#endif /* _TXGBE_HW_H_ */
