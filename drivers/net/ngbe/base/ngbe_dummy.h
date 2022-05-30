/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _NGBE_TYPE_DUMMY_H_
#define _NGBE_TYPE_DUMMY_H_

#ifdef TUP
#elif defined(__GNUC__)
#define TUP(x) x##_unused ngbe_unused
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

/* struct ngbe_bus_operations */
static inline void ngbe_bus_set_lan_id_dummy(struct ngbe_hw *TUP0)
{
}
/* struct ngbe_rom_operations */
static inline s32 ngbe_rom_init_params_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_rom_readw_buffer_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_rom_read32_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 *TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_rom_writew_buffer_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2, void *TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_rom_validate_checksum_dummy(struct ngbe_hw *TUP0,
					u16 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
/* struct ngbe_mac_operations */
static inline s32 ngbe_mac_init_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_reset_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_start_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_stop_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_clear_hw_cntrs_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_get_mac_addr_dummy(struct ngbe_hw *TUP0, u8 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_enable_rx_dma_dummy(struct ngbe_hw *TUP0, u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_disable_sec_rx_path_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_enable_sec_rx_path_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_acquire_swfw_sync_dummy(struct ngbe_hw *TUP0,
					u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline void ngbe_mac_release_swfw_sync_dummy(struct ngbe_hw *TUP0,
					u32 TUP1)
{
}
static inline s32 ngbe_mac_setup_link_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					bool TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_check_link_dummy(struct ngbe_hw *TUP0, u32 *TUP1,
					bool *TUP3, bool TUP4)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_get_link_capabilities_dummy(struct ngbe_hw *TUP0,
					u32 *TUP1, bool *TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline void ngbe_setup_pba_dummy(struct ngbe_hw *TUP0)
{
}
static inline s32 ngbe_mac_led_on_dummy(struct ngbe_hw *TUP0, u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_led_off_dummy(struct ngbe_hw *TUP0, u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_set_rar_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u8 *TUP2, u32 TUP3, u32 TUP4)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_clear_rar_dummy(struct ngbe_hw *TUP0, u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_set_vmdq_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_clear_vmdq_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_init_rx_addrs_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_update_mc_addr_list_dummy(struct ngbe_hw *TUP0,
			u8 *TUP1, u32 TUP2, ngbe_mc_addr_itr TUP3, bool TUP4)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_clear_vfta_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_set_vfta_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2, bool TUP3, bool TUP4)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_set_vlvf_dummy(struct ngbe_hw *TUP0, u32 TUP1,
			u32 TUP2, bool TUP3, u32 *TUP4, u32 TUP5, bool TUP6)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline void ngbe_mac_set_mac_anti_spoofing_dummy(struct ngbe_hw *TUP0,
					bool TUP1, int TUP2)
{
}
static inline void ngbe_mac_set_vlan_anti_spoofing_dummy(struct ngbe_hw *TUP0,
					bool TUP1, int TUP2)
{
}
static inline s32 ngbe_mac_fc_enable_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_setup_fc_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline void ngbe_mac_fc_autoneg_dummy(struct ngbe_hw *TUP0)
{
}
static inline s32 ngbe_mac_init_thermal_ssth_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_check_overtemp_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
/* struct ngbe_phy_operations */
static inline s32 ngbe_phy_identify_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_init_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_reset_hw_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_read_reg_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 *TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_write_reg_dummy(struct ngbe_hw *TUP0, u32 TUP1,
					u32 TUP2, u16 TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_read_reg_unlocked_dummy(struct ngbe_hw *TUP0,
					u32 TUP1, u32 TUP2, u16 *TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_write_reg_unlocked_dummy(struct ngbe_hw *TUP0,
					u32 TUP1, u32 TUP2, u16 TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_setup_link_dummy(struct ngbe_hw *TUP0,
					u32 TUP1, bool TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_check_link_dummy(struct ngbe_hw *TUP0, u32 *TUP1,
					bool *TUP2)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_set_phy_power_dummy(struct ngbe_hw *TUP0, bool TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_get_phy_advertised_pause_dummy(struct ngbe_hw *TUP0,
					u8 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_get_phy_lp_advertised_pause_dummy(struct ngbe_hw *TUP0,
					u8 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_set_phy_pause_adv_dummy(struct ngbe_hw *TUP0, u16 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_phy_led_oem_chk_dummy(struct ngbe_hw *TUP0, u32 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}

/* struct ngbe_mbx_operations */
static inline void ngbe_mbx_init_params_dummy(struct ngbe_hw *TUP0)
{
}
static inline s32 ngbe_mbx_read_dummy(struct ngbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mbx_write_dummy(struct ngbe_hw *TUP0, u32 *TUP1,
					u16 TUP2, u16 TUP3)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mbx_check_for_msg_dummy(struct ngbe_hw *TUP0, u16 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mbx_check_for_ack_dummy(struct ngbe_hw *TUP0, u16 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mbx_check_for_rst_dummy(struct ngbe_hw *TUP0, u16 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}

static inline void ngbe_init_ops_dummy(struct ngbe_hw *hw)
{
	hw->bus.set_lan_id = ngbe_bus_set_lan_id_dummy;
	hw->rom.init_params = ngbe_rom_init_params_dummy;
	hw->rom.readw_buffer = ngbe_rom_readw_buffer_dummy;
	hw->rom.read32 = ngbe_rom_read32_dummy;
	hw->rom.writew_buffer = ngbe_rom_writew_buffer_dummy;
	hw->rom.validate_checksum = ngbe_rom_validate_checksum_dummy;
	hw->mac.init_hw = ngbe_mac_init_hw_dummy;
	hw->mac.reset_hw = ngbe_mac_reset_hw_dummy;
	hw->mac.start_hw = ngbe_mac_start_hw_dummy;
	hw->mac.stop_hw = ngbe_mac_stop_hw_dummy;
	hw->mac.clear_hw_cntrs = ngbe_mac_clear_hw_cntrs_dummy;
	hw->mac.get_mac_addr = ngbe_mac_get_mac_addr_dummy;
	hw->mac.enable_rx_dma = ngbe_mac_enable_rx_dma_dummy;
	hw->mac.disable_sec_rx_path = ngbe_mac_disable_sec_rx_path_dummy;
	hw->mac.enable_sec_rx_path = ngbe_mac_enable_sec_rx_path_dummy;
	hw->mac.acquire_swfw_sync = ngbe_mac_acquire_swfw_sync_dummy;
	hw->mac.release_swfw_sync = ngbe_mac_release_swfw_sync_dummy;
	hw->mac.setup_link = ngbe_mac_setup_link_dummy;
	hw->mac.check_link = ngbe_mac_check_link_dummy;
	hw->mac.get_link_capabilities = ngbe_mac_get_link_capabilities_dummy;
	hw->mac.setup_pba = ngbe_setup_pba_dummy;
	hw->mac.led_on = ngbe_mac_led_on_dummy;
	hw->mac.led_off = ngbe_mac_led_off_dummy;
	hw->mac.set_rar = ngbe_mac_set_rar_dummy;
	hw->mac.clear_rar = ngbe_mac_clear_rar_dummy;
	hw->mac.set_vmdq = ngbe_mac_set_vmdq_dummy;
	hw->mac.clear_vmdq = ngbe_mac_clear_vmdq_dummy;
	hw->mac.init_rx_addrs = ngbe_mac_init_rx_addrs_dummy;
	hw->mac.update_mc_addr_list = ngbe_mac_update_mc_addr_list_dummy;
	hw->mac.clear_vfta = ngbe_mac_clear_vfta_dummy;
	hw->mac.set_vfta = ngbe_mac_set_vfta_dummy;
	hw->mac.set_vlvf = ngbe_mac_set_vlvf_dummy;
	hw->mac.set_mac_anti_spoofing = ngbe_mac_set_mac_anti_spoofing_dummy;
	hw->mac.set_vlan_anti_spoofing = ngbe_mac_set_vlan_anti_spoofing_dummy;
	hw->mac.fc_enable = ngbe_mac_fc_enable_dummy;
	hw->mac.setup_fc = ngbe_mac_setup_fc_dummy;
	hw->mac.fc_autoneg = ngbe_mac_fc_autoneg_dummy;
	hw->mac.init_thermal_sensor_thresh = ngbe_mac_init_thermal_ssth_dummy;
	hw->mac.check_overtemp = ngbe_mac_check_overtemp_dummy;
	hw->phy.identify = ngbe_phy_identify_dummy;
	hw->phy.init_hw = ngbe_phy_init_hw_dummy;
	hw->phy.reset_hw = ngbe_phy_reset_hw_dummy;
	hw->phy.read_reg = ngbe_phy_read_reg_dummy;
	hw->phy.write_reg = ngbe_phy_write_reg_dummy;
	hw->phy.read_reg_unlocked = ngbe_phy_read_reg_unlocked_dummy;
	hw->phy.write_reg_unlocked = ngbe_phy_write_reg_unlocked_dummy;
	hw->phy.setup_link = ngbe_phy_setup_link_dummy;
	hw->phy.check_link = ngbe_phy_check_link_dummy;
	hw->phy.get_adv_pause = ngbe_get_phy_advertised_pause_dummy;
	hw->phy.get_lp_adv_pause = ngbe_get_phy_lp_advertised_pause_dummy;
	hw->phy.set_pause_adv = ngbe_set_phy_pause_adv_dummy;
	hw->phy.led_oem_chk = ngbe_phy_led_oem_chk_dummy;
	hw->phy.set_phy_power = ngbe_phy_set_phy_power_dummy;
	hw->mbx.init_params = ngbe_mbx_init_params_dummy;
	hw->mbx.read = ngbe_mbx_read_dummy;
	hw->mbx.write = ngbe_mbx_write_dummy;
	hw->mbx.check_for_msg = ngbe_mbx_check_for_msg_dummy;
	hw->mbx.check_for_ack = ngbe_mbx_check_for_ack_dummy;
	hw->mbx.check_for_rst = ngbe_mbx_check_for_rst_dummy;
}

#endif /* _NGBE_TYPE_DUMMY_H_ */

