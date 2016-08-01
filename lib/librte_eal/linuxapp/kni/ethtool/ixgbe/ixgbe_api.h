/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "LICENSE.GPL".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_API_H_
#define _IXGBE_API_H_

#include "ixgbe_type.h"

s32 ixgbe_init_shared_code(struct ixgbe_hw *hw);

extern s32 ixgbe_init_ops_82598(struct ixgbe_hw *hw);
extern s32 ixgbe_init_ops_82599(struct ixgbe_hw *hw);
extern s32 ixgbe_init_ops_X540(struct ixgbe_hw *hw);

s32 ixgbe_set_mac_type(struct ixgbe_hw *hw);
s32 ixgbe_init_hw(struct ixgbe_hw *hw);
s32 ixgbe_reset_hw(struct ixgbe_hw *hw);
s32 ixgbe_start_hw(struct ixgbe_hw *hw);
s32 ixgbe_clear_hw_cntrs(struct ixgbe_hw *hw);
enum ixgbe_media_type ixgbe_get_media_type(struct ixgbe_hw *hw);
s32 ixgbe_get_mac_addr(struct ixgbe_hw *hw, u8 *mac_addr);
s32 ixgbe_get_bus_info(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_tx_queues(struct ixgbe_hw *hw);
u32 ixgbe_get_num_of_rx_queues(struct ixgbe_hw *hw);
s32 ixgbe_stop_adapter(struct ixgbe_hw *hw);
s32 ixgbe_read_pba_string(struct ixgbe_hw *hw, u8 *pba_num, u32 pba_num_size);

s32 ixgbe_identify_phy(struct ixgbe_hw *hw);
s32 ixgbe_reset_phy(struct ixgbe_hw *hw);
s32 ixgbe_read_phy_reg(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
		       u16 *phy_data);
s32 ixgbe_write_phy_reg(struct ixgbe_hw *hw, u32 reg_addr, u32 device_type,
			u16 phy_data);

s32 ixgbe_setup_phy_link(struct ixgbe_hw *hw);
s32 ixgbe_check_phy_link(struct ixgbe_hw *hw,
			 ixgbe_link_speed *speed,
			 bool *link_up);
s32 ixgbe_setup_phy_link_speed(struct ixgbe_hw *hw,
			       ixgbe_link_speed speed,
			       bool autoneg,
			       bool autoneg_wait_to_complete);
void ixgbe_disable_tx_laser(struct ixgbe_hw *hw);
void ixgbe_enable_tx_laser(struct ixgbe_hw *hw);
void ixgbe_flap_tx_laser(struct ixgbe_hw *hw);
s32 ixgbe_setup_link(struct ixgbe_hw *hw, ixgbe_link_speed speed,
		     bool autoneg, bool autoneg_wait_to_complete);
s32 ixgbe_check_link(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
		     bool *link_up, bool link_up_wait_to_complete);
s32 ixgbe_get_link_capabilities(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				bool *autoneg);
s32 ixgbe_led_on(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_led_off(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_blink_led_start(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_blink_led_stop(struct ixgbe_hw *hw, u32 index);

s32 ixgbe_init_eeprom_params(struct ixgbe_hw *hw);
s32 ixgbe_write_eeprom(struct ixgbe_hw *hw, u16 offset, u16 data);
s32 ixgbe_write_eeprom_buffer(struct ixgbe_hw *hw, u16 offset,
			      u16 words, u16 *data);
s32 ixgbe_read_eeprom(struct ixgbe_hw *hw, u16 offset, u16 *data);
s32 ixgbe_read_eeprom_buffer(struct ixgbe_hw *hw, u16 offset,
			     u16 words, u16 *data);

s32 ixgbe_validate_eeprom_checksum(struct ixgbe_hw *hw, u16 *checksum_val);
s32 ixgbe_update_eeprom_checksum(struct ixgbe_hw *hw);

s32 ixgbe_insert_mac_addr(struct ixgbe_hw *hw, u8 *addr, u32 vmdq);
s32 ixgbe_set_rar(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		  u32 enable_addr);
s32 ixgbe_clear_rar(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_set_vmdq(struct ixgbe_hw *hw, u32 rar, u32 vmdq);
s32 ixgbe_set_vmdq_san_mac(struct ixgbe_hw *hw, u32 vmdq);
s32 ixgbe_clear_vmdq(struct ixgbe_hw *hw, u32 rar, u32 vmdq);
s32 ixgbe_init_rx_addrs(struct ixgbe_hw *hw);
u32 ixgbe_get_num_rx_addrs(struct ixgbe_hw *hw);
s32 ixgbe_update_uc_addr_list(struct ixgbe_hw *hw, u8 *addr_list,
			      u32 addr_count, ixgbe_mc_addr_itr func);
s32 ixgbe_update_mc_addr_list(struct ixgbe_hw *hw, u8 *mc_addr_list,
			      u32 mc_addr_count, ixgbe_mc_addr_itr func,
			      bool clear);
void ixgbe_add_uc_addr(struct ixgbe_hw *hw, u8 *addr_list, u32 vmdq);
s32 ixgbe_enable_mc(struct ixgbe_hw *hw);
s32 ixgbe_disable_mc(struct ixgbe_hw *hw);
s32 ixgbe_clear_vfta(struct ixgbe_hw *hw);
s32 ixgbe_set_vfta(struct ixgbe_hw *hw, u32 vlan,
		   u32 vind, bool vlan_on);
s32 ixgbe_set_vlvf(struct ixgbe_hw *hw, u32 vlan, u32 vind,
		   bool vlan_on, bool *vfta_changed);
s32 ixgbe_fc_enable(struct ixgbe_hw *hw);
s32 ixgbe_set_fw_drv_ver(struct ixgbe_hw *hw, u8 maj, u8 min, u8 build,
			 u8 ver);
s32 ixgbe_get_thermal_sensor_data(struct ixgbe_hw *hw);
s32 ixgbe_init_thermal_sensor_thresh(struct ixgbe_hw *hw);
void ixgbe_set_mta(struct ixgbe_hw *hw, u8 *mc_addr);
s32 ixgbe_get_phy_firmware_version(struct ixgbe_hw *hw,
				   u16 *firmware_version);
s32 ixgbe_read_analog_reg8(struct ixgbe_hw *hw, u32 reg, u8 *val);
s32 ixgbe_write_analog_reg8(struct ixgbe_hw *hw, u32 reg, u8 val);
s32 ixgbe_init_uta_tables(struct ixgbe_hw *hw);
s32 ixgbe_read_i2c_eeprom(struct ixgbe_hw *hw, u8 byte_offset, u8 *eeprom_data);
u32 ixgbe_get_supported_physical_layer(struct ixgbe_hw *hw);
s32 ixgbe_enable_rx_dma(struct ixgbe_hw *hw, u32 regval);
s32 ixgbe_disable_sec_rx_path(struct ixgbe_hw *hw);
s32 ixgbe_enable_sec_rx_path(struct ixgbe_hw *hw);
s32 ixgbe_reinit_fdir_tables_82599(struct ixgbe_hw *hw);
s32 ixgbe_init_fdir_signature_82599(struct ixgbe_hw *hw, u32 fdirctrl);
s32 ixgbe_init_fdir_perfect_82599(struct ixgbe_hw *hw, u32 fdirctrl);
s32 ixgbe_fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
					  union ixgbe_atr_hash_dword input,
					  union ixgbe_atr_hash_dword common,
					  u8 queue);
s32 ixgbe_fdir_set_input_mask_82599(struct ixgbe_hw *hw,
				    union ixgbe_atr_input *input_mask);
s32 ixgbe_fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
					  union ixgbe_atr_input *input,
					  u16 soft_id, u8 queue);
s32 ixgbe_fdir_erase_perfect_filter_82599(struct ixgbe_hw *hw,
					  union ixgbe_atr_input *input,
					  u16 soft_id);
s32 ixgbe_fdir_add_perfect_filter_82599(struct ixgbe_hw *hw,
					union ixgbe_atr_input *input,
					union ixgbe_atr_input *mask,
					u16 soft_id,
					u8 queue);
void ixgbe_atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
					  union ixgbe_atr_input *mask);
u32 ixgbe_atr_compute_sig_hash_82599(union ixgbe_atr_hash_dword input,
				     union ixgbe_atr_hash_dword common);
s32 ixgbe_read_i2c_byte(struct ixgbe_hw *hw, u8 byte_offset, u8 dev_addr,
			u8 *data);
s32 ixgbe_write_i2c_byte(struct ixgbe_hw *hw, u8 byte_offset, u8 dev_addr,
			 u8 data);
s32 ixgbe_write_i2c_eeprom(struct ixgbe_hw *hw, u8 byte_offset, u8 eeprom_data);
s32 ixgbe_get_san_mac_addr(struct ixgbe_hw *hw, u8 *san_mac_addr);
s32 ixgbe_set_san_mac_addr(struct ixgbe_hw *hw, u8 *san_mac_addr);
s32 ixgbe_get_device_caps(struct ixgbe_hw *hw, u16 *device_caps);
s32 ixgbe_acquire_swfw_semaphore(struct ixgbe_hw *hw, u16 mask);
void ixgbe_release_swfw_semaphore(struct ixgbe_hw *hw, u16 mask);
s32 ixgbe_get_wwn_prefix(struct ixgbe_hw *hw, u16 *wwnn_prefix,
			 u16 *wwpn_prefix);
s32 ixgbe_get_fcoe_boot_status(struct ixgbe_hw *hw, u16 *bs);

#endif /* _IXGBE_API_H_ */
