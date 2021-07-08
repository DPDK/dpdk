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
static inline void ngbe_init_ops_dummy(struct ngbe_hw *hw)
{
	hw->bus.set_lan_id = ngbe_bus_set_lan_id_dummy;
	hw->rom.init_params = ngbe_rom_init_params_dummy;
	hw->rom.validate_checksum = ngbe_rom_validate_checksum_dummy;
	hw->mac.init_hw = ngbe_mac_init_hw_dummy;
	hw->mac.reset_hw = ngbe_mac_reset_hw_dummy;
	hw->mac.start_hw = ngbe_mac_start_hw_dummy;
	hw->mac.stop_hw = ngbe_mac_stop_hw_dummy;
	hw->mac.get_mac_addr = ngbe_mac_get_mac_addr_dummy;
	hw->mac.enable_rx_dma = ngbe_mac_enable_rx_dma_dummy;
	hw->mac.disable_sec_rx_path = ngbe_mac_disable_sec_rx_path_dummy;
	hw->mac.enable_sec_rx_path = ngbe_mac_enable_sec_rx_path_dummy;
	hw->mac.acquire_swfw_sync = ngbe_mac_acquire_swfw_sync_dummy;
	hw->mac.release_swfw_sync = ngbe_mac_release_swfw_sync_dummy;
	hw->mac.setup_link = ngbe_mac_setup_link_dummy;
	hw->mac.check_link = ngbe_mac_check_link_dummy;
	hw->mac.get_link_capabilities = ngbe_mac_get_link_capabilities_dummy;
	hw->mac.set_rar = ngbe_mac_set_rar_dummy;
	hw->mac.clear_rar = ngbe_mac_clear_rar_dummy;
	hw->mac.set_vmdq = ngbe_mac_set_vmdq_dummy;
	hw->mac.clear_vmdq = ngbe_mac_clear_vmdq_dummy;
	hw->mac.init_rx_addrs = ngbe_mac_init_rx_addrs_dummy;
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
}

#endif /* _NGBE_TYPE_DUMMY_H_ */

