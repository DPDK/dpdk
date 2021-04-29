/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _TXGBE_TYPE_DUMMY_H_
#define _TXGBE_TYPE_DUMMY_H_

#ifdef TUP
#elif defined(__GNUC__)
#define TUP(x) x##_unused txgbe_unused
#elif defined(__LCLINT__)
#define TUP(x) x /*@unused@*/
#else
#define TUP(x) x
#endif /*TUP*/
#define TUP0 TUP(p0)
#define TUP1 TUP(p1)
#define TUP2 TUP(p2)
#define TUP3 TUP(p3)
#define TUP4 TUP(p4)
#define TUP5 TUP(p5)
#define TUP6 TUP(p6)
#define TUP7 TUP(p7)
#define TUP8 TUP(p8)
#define TUP9 TUP(p9)

/* struct txgbe_bus_operations */
static inline s32 txgbe_bus_get_bus_info_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_bus_set_lan_id_dummy(struct txgbe_hw *TUP0)
{
}
/* struct txgbe_rom_operations */
static inline s32 txgbe_rom_init_params_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_read16_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u16 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_readw_buffer_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_readw_sw_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u16 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_read32_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_read_buffer_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_write16_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u16 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_writew_buffer_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_writew_sw_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u16 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_write32_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_write_buffer_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_validate_checksum_dummy(struct txgbe_hw *TUP0,
					u16 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_update_checksum_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_rom_calc_checksum_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}

/* struct txgbe_mac_operations */
static inline s32 txgbe_mac_init_hw_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_reset_hw_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_start_hw_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_stop_hw_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_clear_hw_cntrs_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_mac_addr_dummy(struct txgbe_hw *TUP0, u8 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_san_mac_addr_dummy(struct txgbe_hw *TUP0,
					u8 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_san_mac_addr_dummy(struct txgbe_hw *TUP0,
					u8 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_device_caps_dummy(struct txgbe_hw *TUP0,
					u16 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_wwn_prefix_dummy(struct txgbe_hw *TUP0,
					u16 *TUP1, u16 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_setup_sfp_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_enable_rx_dma_dummy(struct txgbe_hw *TUP0, u32 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_disable_sec_rx_path_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_enable_sec_rx_path_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_disable_sec_tx_path_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_enable_sec_tx_path_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_acquire_swfw_sync_dummy(struct txgbe_hw *TUP0,
					u32 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_release_swfw_sync_dummy(struct txgbe_hw *TUP0,
					u32 TUP1)
{
}
static inline u64 txgbe_mac_autoc_read_dummy(struct txgbe_hw *TUP0)
{
	return 0;
}
static inline void txgbe_mac_autoc_write_dummy(struct txgbe_hw *TUP0, u64 TUP1)
{
}
static inline s32 txgbe_mac_prot_autoc_read_dummy(struct txgbe_hw *TUP0,
					bool *TUP1, u64 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_prot_autoc_write_dummy(struct txgbe_hw *TUP0,
					bool TUP1, u64 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_negotiate_api_version_dummy(struct txgbe_hw *TUP0,
					int TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_disable_tx_laser_dummy(struct txgbe_hw *TUP0)
{
}
static inline void txgbe_mac_enable_tx_laser_dummy(struct txgbe_hw *TUP0)
{
}
static inline void txgbe_mac_flap_tx_laser_dummy(struct txgbe_hw *TUP0)
{
}
static inline s32 txgbe_mac_setup_link_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					bool TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_setup_mac_link_dummy(struct txgbe_hw *TUP0,
					u32 TUP1, bool TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_check_link_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					bool *TUP3, bool TUP4)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_link_capabilities_dummy(struct txgbe_hw *TUP0,
					u32 *TUP1, bool *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_set_rate_select_speed_dummy(struct txgbe_hw *TUP0,
					u32 TUP1)
{
}
static inline void txgbe_mac_setup_pba_dummy(struct txgbe_hw *TUP0, int TUP1,
					u32 TUP2, int TUP3)
{
}
static inline s32 txgbe_mac_led_on_dummy(struct txgbe_hw *TUP0, u32 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_led_off_dummy(struct txgbe_hw *TUP0, u32 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_rar_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u8 *TUP2, u32 TUP3, u32 TUP4)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_uc_addr_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u8 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_clear_rar_dummy(struct txgbe_hw *TUP0, u32 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_vmdq_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_clear_vmdq_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_init_rx_addrs_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_update_mc_addr_list_dummy(struct txgbe_hw *TUP0,
			u8 *TUP1, u32 TUP2, txgbe_mc_addr_itr TUP3, bool TUP4)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_clear_vfta_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_vfta_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, bool TUP3, bool TUP4)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_vlvf_dummy(struct txgbe_hw *TUP0, u32 TUP1,
			u32 TUP2, bool TUP3, u32 *TUP4, u32 TUP5, bool TUP6)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_init_uta_tables_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_set_mac_anti_spoofing_dummy(struct txgbe_hw *TUP0,
					bool TUP1, int TUP2)
{
}
static inline void txgbe_mac_set_vlan_anti_spoofing_dummy(struct txgbe_hw *TUP0,
					bool TUP1, int TUP2)
{
}
static inline s32 txgbe_mac_update_xcast_mode_dummy(struct txgbe_hw *TUP0,
					int TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_set_rlpml_dummy(struct txgbe_hw *TUP0, u16 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_fc_enable_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_setup_fc_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_fc_autoneg_dummy(struct txgbe_hw *TUP0)
{
}
static inline s32 txgbe_mac_set_fw_drv_ver_dummy(struct txgbe_hw *TUP0, u8 TUP1,
			u8 TUP2, u8 TUP3, u8 TUP4, u16 TUP5, const char *TUP6)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_get_thermal_sensor_data_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_init_thermal_ssth_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline void txgbe_mac_get_rtrup2tc_dummy(struct txgbe_hw *TUP0, u8 *TUP1)
{
}
static inline void txgbe_mac_disable_rx_dummy(struct txgbe_hw *TUP0)
{
}
static inline void txgbe_mac_enable_rx_dummy(struct txgbe_hw *TUP0)
{
}
static inline void
txgbe_mac_set_ethertype_anti_spoofing_dummy(struct txgbe_hw *TUP0, bool TUP1,
					int TUP2)
{
}
static inline s32 txgbe_mac_dmac_update_tcs_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_dmac_config_tcs_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_dmac_config_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mac_setup_eee_dummy(struct txgbe_hw *TUP0, bool TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}

/* struct txgbe_phy_operations */
static inline u32 txgbe_phy_get_media_type_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_identify_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_identify_sfp_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_init_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_reset_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_reg_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_write_reg_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_reg_mdi_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_write_reg_mdi_dummy(struct txgbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_setup_link_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_setup_link_speed_dummy(struct txgbe_hw *TUP0,
					u32 TUP1, bool TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_check_link_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					bool *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_get_phy_fw_version_dummy(struct txgbe_hw *TUP0,
					u32 *TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_i2c_byte_dummy(struct txgbe_hw *TUP0, u8 TUP1,
					u8 TUP2, u8 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_write_i2c_byte_dummy(struct txgbe_hw *TUP0, u8 TUP1,
					u8 TUP2, u8 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_i2c_sff8472_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u8 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_i2c_eeprom_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u8 *TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_write_i2c_eeprom_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u8 TUP2)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_check_overtemp_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_set_phy_power_dummy(struct txgbe_hw *TUP0,
					bool TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_handle_lasi_dummy(struct txgbe_hw *TUP0)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_read_i2c_byte_unlocked_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u8 TUP2, u8 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_phy_write_i2c_byte_unlocked_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u8 TUP2, u8 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}

/* struct txgbe_link_operations */
static inline s32 txgbe_link_read_link_dummy(struct txgbe_hw *TUP0, u8 TUP1,
					u16 TUP2, u16 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_link_read_link_unlocked_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u16 TUP2, u16 *TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_link_write_link_dummy(struct txgbe_hw *TUP0, u8 TUP1,
					u16 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_link_write_link_unlocked_dummy(struct txgbe_hw *TUP0,
					u8 TUP1, u16 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}

/* struct txgbe_mbx_operations */
static inline void txgbe_mbx_init_params_dummy(struct txgbe_hw *TUP0)
{
}
static inline s32 txgbe_mbx_read_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_write_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_read_posted_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP3)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_write_posted_dummy(struct txgbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP4)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_check_for_msg_dummy(struct txgbe_hw *TUP0, u16 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_check_for_ack_dummy(struct txgbe_hw *TUP0, u16 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}
static inline s32 txgbe_mbx_check_for_rst_dummy(struct txgbe_hw *TUP0, u16 TUP1)
{
	return TXGBE_ERR_OPS_DUMMY;
}


static inline void txgbe_init_ops_dummy(struct txgbe_hw *hw)
{
	hw->bus.get_bus_info = txgbe_bus_get_bus_info_dummy;
	hw->bus.set_lan_id = txgbe_bus_set_lan_id_dummy;
	hw->rom.init_params = txgbe_rom_init_params_dummy;
	hw->rom.read16 = txgbe_rom_read16_dummy;
	hw->rom.readw_buffer = txgbe_rom_readw_buffer_dummy;
	hw->rom.readw_sw = txgbe_rom_readw_sw_dummy;
	hw->rom.read32 = txgbe_rom_read32_dummy;
	hw->rom.read_buffer = txgbe_rom_read_buffer_dummy;
	hw->rom.write16 = txgbe_rom_write16_dummy;
	hw->rom.writew_buffer = txgbe_rom_writew_buffer_dummy;
	hw->rom.writew_sw = txgbe_rom_writew_sw_dummy;
	hw->rom.write32 = txgbe_rom_write32_dummy;
	hw->rom.write_buffer = txgbe_rom_write_buffer_dummy;
	hw->rom.validate_checksum = txgbe_rom_validate_checksum_dummy;
	hw->rom.update_checksum = txgbe_rom_update_checksum_dummy;
	hw->rom.calc_checksum = txgbe_rom_calc_checksum_dummy;
	hw->mac.init_hw = txgbe_mac_init_hw_dummy;
	hw->mac.reset_hw = txgbe_mac_reset_hw_dummy;
	hw->mac.start_hw = txgbe_mac_start_hw_dummy;
	hw->mac.stop_hw = txgbe_mac_stop_hw_dummy;
	hw->mac.clear_hw_cntrs = txgbe_mac_clear_hw_cntrs_dummy;
	hw->mac.get_mac_addr = txgbe_mac_get_mac_addr_dummy;
	hw->mac.get_san_mac_addr = txgbe_mac_get_san_mac_addr_dummy;
	hw->mac.set_san_mac_addr = txgbe_mac_set_san_mac_addr_dummy;
	hw->mac.get_device_caps = txgbe_mac_get_device_caps_dummy;
	hw->mac.get_wwn_prefix = txgbe_mac_get_wwn_prefix_dummy;
	hw->mac.setup_sfp = txgbe_mac_setup_sfp_dummy;
	hw->mac.enable_rx_dma = txgbe_mac_enable_rx_dma_dummy;
	hw->mac.disable_sec_rx_path = txgbe_mac_disable_sec_rx_path_dummy;
	hw->mac.enable_sec_rx_path = txgbe_mac_enable_sec_rx_path_dummy;
	hw->mac.disable_sec_tx_path = txgbe_mac_disable_sec_tx_path_dummy;
	hw->mac.enable_sec_tx_path = txgbe_mac_enable_sec_tx_path_dummy;
	hw->mac.acquire_swfw_sync = txgbe_mac_acquire_swfw_sync_dummy;
	hw->mac.release_swfw_sync = txgbe_mac_release_swfw_sync_dummy;
	hw->mac.autoc_read = txgbe_mac_autoc_read_dummy;
	hw->mac.autoc_write = txgbe_mac_autoc_write_dummy;
	hw->mac.prot_autoc_read = txgbe_mac_prot_autoc_read_dummy;
	hw->mac.prot_autoc_write = txgbe_mac_prot_autoc_write_dummy;
	hw->mac.negotiate_api_version = txgbe_mac_negotiate_api_version_dummy;
	hw->mac.disable_tx_laser = txgbe_mac_disable_tx_laser_dummy;
	hw->mac.enable_tx_laser = txgbe_mac_enable_tx_laser_dummy;
	hw->mac.flap_tx_laser = txgbe_mac_flap_tx_laser_dummy;
	hw->mac.setup_link = txgbe_mac_setup_link_dummy;
	hw->mac.setup_mac_link = txgbe_mac_setup_mac_link_dummy;
	hw->mac.check_link = txgbe_mac_check_link_dummy;
	hw->mac.get_link_capabilities = txgbe_mac_get_link_capabilities_dummy;
	hw->mac.set_rate_select_speed = txgbe_mac_set_rate_select_speed_dummy;
	hw->mac.setup_pba = txgbe_mac_setup_pba_dummy;
	hw->mac.led_on = txgbe_mac_led_on_dummy;
	hw->mac.led_off = txgbe_mac_led_off_dummy;
	hw->mac.set_rar = txgbe_mac_set_rar_dummy;
	hw->mac.set_uc_addr = txgbe_mac_set_uc_addr_dummy;
	hw->mac.clear_rar = txgbe_mac_clear_rar_dummy;
	hw->mac.set_vmdq = txgbe_mac_set_vmdq_dummy;
	hw->mac.clear_vmdq = txgbe_mac_clear_vmdq_dummy;
	hw->mac.init_rx_addrs = txgbe_mac_init_rx_addrs_dummy;
	hw->mac.update_mc_addr_list = txgbe_mac_update_mc_addr_list_dummy;
	hw->mac.clear_vfta = txgbe_mac_clear_vfta_dummy;
	hw->mac.set_vfta = txgbe_mac_set_vfta_dummy;
	hw->mac.set_vlvf = txgbe_mac_set_vlvf_dummy;
	hw->mac.init_uta_tables = txgbe_mac_init_uta_tables_dummy;
	hw->mac.set_mac_anti_spoofing = txgbe_mac_set_mac_anti_spoofing_dummy;
	hw->mac.set_vlan_anti_spoofing = txgbe_mac_set_vlan_anti_spoofing_dummy;
	hw->mac.update_xcast_mode = txgbe_mac_update_xcast_mode_dummy;
	hw->mac.set_rlpml = txgbe_mac_set_rlpml_dummy;
	hw->mac.fc_enable = txgbe_mac_fc_enable_dummy;
	hw->mac.setup_fc = txgbe_mac_setup_fc_dummy;
	hw->mac.fc_autoneg = txgbe_mac_fc_autoneg_dummy;
	hw->mac.set_fw_drv_ver = txgbe_mac_set_fw_drv_ver_dummy;
	hw->mac.get_thermal_sensor_data =
			txgbe_mac_get_thermal_sensor_data_dummy;
	hw->mac.init_thermal_sensor_thresh = txgbe_mac_init_thermal_ssth_dummy;
	hw->mac.get_rtrup2tc = txgbe_mac_get_rtrup2tc_dummy;
	hw->mac.disable_rx = txgbe_mac_disable_rx_dummy;
	hw->mac.enable_rx = txgbe_mac_enable_rx_dummy;
	hw->mac.set_ethertype_anti_spoofing =
			txgbe_mac_set_ethertype_anti_spoofing_dummy;
	hw->mac.dmac_update_tcs = txgbe_mac_dmac_update_tcs_dummy;
	hw->mac.dmac_config_tcs = txgbe_mac_dmac_config_tcs_dummy;
	hw->mac.dmac_config = txgbe_mac_dmac_config_dummy;
	hw->mac.setup_eee = txgbe_mac_setup_eee_dummy;
	hw->phy.get_media_type = txgbe_phy_get_media_type_dummy;
	hw->phy.identify = txgbe_phy_identify_dummy;
	hw->phy.identify_sfp = txgbe_phy_identify_sfp_dummy;
	hw->phy.init = txgbe_phy_init_dummy;
	hw->phy.reset = txgbe_phy_reset_dummy;
	hw->phy.read_reg = txgbe_phy_read_reg_dummy;
	hw->phy.write_reg = txgbe_phy_write_reg_dummy;
	hw->phy.read_reg_mdi = txgbe_phy_read_reg_mdi_dummy;
	hw->phy.write_reg_mdi = txgbe_phy_write_reg_mdi_dummy;
	hw->phy.setup_link = txgbe_phy_setup_link_dummy;
	hw->phy.setup_link_speed = txgbe_phy_setup_link_speed_dummy;
	hw->phy.check_link = txgbe_phy_check_link_dummy;
	hw->phy.get_fw_version = txgbe_get_phy_fw_version_dummy;
	hw->phy.read_i2c_byte = txgbe_phy_read_i2c_byte_dummy;
	hw->phy.write_i2c_byte = txgbe_phy_write_i2c_byte_dummy;
	hw->phy.read_i2c_sff8472 = txgbe_phy_read_i2c_sff8472_dummy;
	hw->phy.read_i2c_eeprom = txgbe_phy_read_i2c_eeprom_dummy;
	hw->phy.write_i2c_eeprom = txgbe_phy_write_i2c_eeprom_dummy;
	hw->phy.check_overtemp = txgbe_phy_check_overtemp_dummy;
	hw->phy.set_phy_power = txgbe_phy_set_phy_power_dummy;
	hw->phy.handle_lasi = txgbe_phy_handle_lasi_dummy;
	hw->phy.read_i2c_byte_unlocked = txgbe_phy_read_i2c_byte_unlocked_dummy;
	hw->phy.write_i2c_byte_unlocked =
			txgbe_phy_write_i2c_byte_unlocked_dummy;
	hw->link.read_link = txgbe_link_read_link_dummy;
	hw->link.read_link_unlocked = txgbe_link_read_link_unlocked_dummy;
	hw->link.write_link = txgbe_link_write_link_dummy;
	hw->link.write_link_unlocked = txgbe_link_write_link_unlocked_dummy;
	hw->mbx.init_params = txgbe_mbx_init_params_dummy;
	hw->mbx.read = txgbe_mbx_read_dummy;
	hw->mbx.write = txgbe_mbx_write_dummy;
	hw->mbx.read_posted = txgbe_mbx_read_posted_dummy;
	hw->mbx.write_posted = txgbe_mbx_write_posted_dummy;
	hw->mbx.check_for_msg = txgbe_mbx_check_for_msg_dummy;
	hw->mbx.check_for_ack = txgbe_mbx_check_for_ack_dummy;
	hw->mbx.check_for_rst = txgbe_mbx_check_for_rst_dummy;
}

#endif /* _TXGBE_TYPE_DUMMY_H_ */

