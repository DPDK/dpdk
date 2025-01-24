/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _IGC_API_H_
#define _IGC_API_H_

#include "igc_hw.h"

/* I2C SDA and SCL timing parameters for standard mode */
#define IGC_I2C_T_HD_STA	4
#define IGC_I2C_T_LOW		5
#define IGC_I2C_T_HIGH		4
#define IGC_I2C_T_SU_STA	5
#define IGC_I2C_T_HD_DATA	5
#define IGC_I2C_T_SU_DATA	1
#define IGC_I2C_T_RISE		1
#define IGC_I2C_T_FALL		1
#define IGC_I2C_T_SU_STO	4
#define IGC_I2C_T_BUF		5

s32 igc_set_i2c_bb(struct igc_hw *hw);
s32 igc_read_i2c_byte_generic(struct igc_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data);
s32 igc_write_i2c_byte_generic(struct igc_hw *hw, u8 byte_offset,
				 u8 dev_addr, u8 data);
void igc_i2c_bus_clear(struct igc_hw *hw);

void igc_init_function_pointers_82542(struct igc_hw *hw);
void igc_init_function_pointers_82543(struct igc_hw *hw);
void igc_init_function_pointers_82540(struct igc_hw *hw);
void igc_init_function_pointers_82571(struct igc_hw *hw);
void igc_init_function_pointers_82541(struct igc_hw *hw);
void igc_init_function_pointers_80003es2lan(struct igc_hw *hw);
void igc_init_function_pointers_ich8lan(struct igc_hw *hw);
void igc_init_function_pointers_82575(struct igc_hw *hw);
void igc_init_function_pointers_vf(struct igc_hw *hw);
void igc_power_up_fiber_serdes_link(struct igc_hw *hw);
void igc_shutdown_fiber_serdes_link(struct igc_hw *hw);
void igc_init_function_pointers_i210(struct igc_hw *hw);
void igc_init_function_pointers_i225(struct igc_hw *hw);

s32 igc_set_obff_timer(struct igc_hw *hw, u32 itr);
s32 igc_set_mac_type(struct igc_hw *hw);
s32 igc_setup_init_funcs(struct igc_hw *hw, bool init_device);
s32 igc_init_mac_params(struct igc_hw *hw);
s32 igc_init_nvm_params(struct igc_hw *hw);
s32 igc_init_phy_params(struct igc_hw *hw);
s32 igc_init_mbx_params(struct igc_hw *hw);
s32 igc_get_bus_info(struct igc_hw *hw);
void igc_clear_vfta(struct igc_hw *hw);
void igc_write_vfta(struct igc_hw *hw, u32 offset, u32 value);
s32 igc_force_mac_fc(struct igc_hw *hw);
s32 igc_check_for_link(struct igc_hw *hw);
s32 igc_reset_hw(struct igc_hw *hw);
s32 igc_init_hw(struct igc_hw *hw);
s32 igc_setup_link(struct igc_hw *hw);
s32 igc_get_speed_and_duplex(struct igc_hw *hw, u16 *speed, u16 *duplex);
s32 igc_disable_pcie_master(struct igc_hw *hw);
void igc_config_collision_dist(struct igc_hw *hw);
int igc_rar_set(struct igc_hw *hw, u8 *addr, u32 index);
u32 igc_hash_mc_addr(struct igc_hw *hw, u8 *mc_addr);
void igc_update_mc_addr_list(struct igc_hw *hw, u8 *mc_addr_list,
			       u32 mc_addr_count);
s32 igc_setup_led(struct igc_hw *hw);
s32 igc_cleanup_led(struct igc_hw *hw);
s32 igc_check_reset_block(struct igc_hw *hw);
s32 igc_blink_led(struct igc_hw *hw);
s32 igc_led_on(struct igc_hw *hw);
s32 igc_led_off(struct igc_hw *hw);
s32 igc_id_led_init(struct igc_hw *hw);
void igc_reset_adaptive(struct igc_hw *hw);
void igc_update_adaptive(struct igc_hw *hw);
s32 igc_get_cable_length(struct igc_hw *hw);
s32 igc_validate_mdi_setting(struct igc_hw *hw);
s32 igc_read_phy_reg(struct igc_hw *hw, u32 offset, u16 *data);
s32 igc_write_phy_reg(struct igc_hw *hw, u32 offset, u16 data);
s32 igc_write_8bit_ctrl_reg(struct igc_hw *hw, u32 reg, u32 offset,
			      u8 data);
s32 igc_get_phy_info(struct igc_hw *hw);
void igc_release_phy(struct igc_hw *hw);
s32 igc_acquire_phy(struct igc_hw *hw);
s32 igc_cfg_on_link_up(struct igc_hw *hw);
s32 igc_phy_hw_reset(struct igc_hw *hw);
s32 igc_phy_commit(struct igc_hw *hw);
void igc_power_up_phy(struct igc_hw *hw);
void igc_power_down_phy(struct igc_hw *hw);
s32 igc_read_mac_addr(struct igc_hw *hw);
s32 igc_read_pba_num(struct igc_hw *hw, u32 *part_num);
s32 igc_read_pba_string(struct igc_hw *hw, u8 *pba_num, u32 pba_num_size);
s32 igc_read_pba_length(struct igc_hw *hw, u32 *pba_num_size);
void igc_reload_nvm(struct igc_hw *hw);
s32 igc_update_nvm_checksum(struct igc_hw *hw);
s32 igc_validate_nvm_checksum(struct igc_hw *hw);
s32 igc_read_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
s32 igc_read_kmrn_reg(struct igc_hw *hw, u32 offset, u16 *data);
s32 igc_write_kmrn_reg(struct igc_hw *hw, u32 offset, u16 data);
s32 igc_write_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
s32 igc_set_d3_lplu_state(struct igc_hw *hw, bool active);
s32 igc_set_d0_lplu_state(struct igc_hw *hw, bool active);
bool igc_check_mng_mode(struct igc_hw *hw);
bool igc_enable_tx_pkt_filtering(struct igc_hw *hw);
s32 igc_mng_enable_host_if(struct igc_hw *hw);
s32 igc_mng_host_if_write(struct igc_hw *hw, u8 *buffer, u16 length,
			    u16 offset, u8 *sum);
s32 igc_mng_write_cmd_header(struct igc_hw *hw,
			       struct igc_host_mng_command_header *hdr);
s32 igc_mng_write_dhcp_info(struct igc_hw *hw, u8 *buffer, u16 length);
u32  igc_translate_register_82542(u32 reg);

#endif /* _IGC_API_H_ */
