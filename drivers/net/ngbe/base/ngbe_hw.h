/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_HW_H_
#define _NGBE_HW_H_

#include "ngbe_type.h"

#define NGBE_EM_MAX_TX_QUEUES 8
#define NGBE_EM_MAX_RX_QUEUES 8
#define NGBE_EM_RAR_ENTRIES   32
#define NGBE_EM_MC_TBL_SIZE   32
#define NGBE_EM_VFT_TBL_SIZE  128
#define NGBE_EM_RX_PB_SIZE    42 /*KB*/

s32 ngbe_init_hw(struct ngbe_hw *hw);
s32 ngbe_start_hw(struct ngbe_hw *hw);
s32 ngbe_reset_hw_em(struct ngbe_hw *hw);
s32 ngbe_stop_hw(struct ngbe_hw *hw);
s32 ngbe_clear_hw_cntrs(struct ngbe_hw *hw);
s32 ngbe_get_mac_addr(struct ngbe_hw *hw, u8 *mac_addr);

void ngbe_set_lan_id_multi_port(struct ngbe_hw *hw);

s32 ngbe_check_mac_link_em(struct ngbe_hw *hw, u32 *speed,
			bool *link_up, bool link_up_wait_to_complete);
s32 ngbe_get_link_capabilities_em(struct ngbe_hw *hw,
				      u32 *speed,
				      bool *autoneg);
s32 ngbe_setup_mac_link_em(struct ngbe_hw *hw,
			       u32 speed,
			       bool autoneg_wait_to_complete);

s32 ngbe_led_on(struct ngbe_hw *hw, u32 index);
s32 ngbe_led_off(struct ngbe_hw *hw, u32 index);

s32 ngbe_set_rar(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
s32 ngbe_clear_rar(struct ngbe_hw *hw, u32 index);
s32 ngbe_init_rx_addrs(struct ngbe_hw *hw);
s32 ngbe_update_mc_addr_list(struct ngbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count,
				      ngbe_mc_addr_itr func, bool clear);
s32 ngbe_disable_sec_rx_path(struct ngbe_hw *hw);
s32 ngbe_enable_sec_rx_path(struct ngbe_hw *hw);
void ngbe_set_pba(struct ngbe_hw *hw);

s32 ngbe_setup_fc_em(struct ngbe_hw *hw);
s32 ngbe_fc_enable(struct ngbe_hw *hw);
void ngbe_fc_autoneg(struct ngbe_hw *hw);

s32 ngbe_validate_mac_addr(u8 *mac_addr);
s32 ngbe_acquire_swfw_sync(struct ngbe_hw *hw, u32 mask);
void ngbe_release_swfw_sync(struct ngbe_hw *hw, u32 mask);
s32 ngbe_set_pcie_master(struct ngbe_hw *hw, bool enable);

s32 ngbe_set_vmdq(struct ngbe_hw *hw, u32 rar, u32 vmdq);
s32 ngbe_clear_vmdq(struct ngbe_hw *hw, u32 rar, u32 vmdq);
s32 ngbe_init_uta_tables(struct ngbe_hw *hw);
s32 ngbe_set_vfta(struct ngbe_hw *hw, u32 vlan,
			 u32 vind, bool vlan_on, bool vlvf_bypass);
s32 ngbe_set_vlvf(struct ngbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, u32 *vfta_delta, u32 vfta,
			   bool vlvf_bypass);
s32 ngbe_clear_vfta(struct ngbe_hw *hw);
s32 ngbe_find_vlvf_slot(struct ngbe_hw *hw, u32 vlan, bool vlvf_bypass);

void ngbe_set_mac_anti_spoofing(struct ngbe_hw *hw, bool enable, int vf);
void ngbe_set_vlan_anti_spoofing(struct ngbe_hw *hw, bool enable, int vf);
s32 ngbe_init_thermal_sensor_thresh(struct ngbe_hw *hw);
s32 ngbe_mac_check_overtemp(struct ngbe_hw *hw);
void ngbe_disable_rx(struct ngbe_hw *hw);
void ngbe_enable_rx(struct ngbe_hw *hw);
void ngbe_set_mta(struct ngbe_hw *hw, u8 *mc_addr);
s32 ngbe_negotiate_fc(struct ngbe_hw *hw, u32 adv_reg, u32 lp_reg,
			u32 adv_sym, u32 adv_asm, u32 lp_sym, u32 lp_asm);
s32 ngbe_init_shared_code(struct ngbe_hw *hw);
s32 ngbe_set_mac_type(struct ngbe_hw *hw);
s32 ngbe_init_ops_pf(struct ngbe_hw *hw);
s32 ngbe_init_phy(struct ngbe_hw *hw);
s32 ngbe_enable_rx_dma(struct ngbe_hw *hw, u32 regval);
void ngbe_map_device_id(struct ngbe_hw *hw);

#endif /* _NGBE_HW_H_ */
