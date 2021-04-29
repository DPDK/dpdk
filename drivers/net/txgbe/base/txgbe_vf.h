/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_VF_H_
#define _TXGBE_VF_H_

#include "txgbe_type.h"

#define TXGBE_VF_MAX_TX_QUEUES	8
#define TXGBE_VF_MAX_RX_QUEUES	8

struct txgbevf_hw_stats {
	u64 base_vfgprc;
	u64 base_vfgptc;
	u64 base_vfgorc;
	u64 base_vfgotc;
	u64 base_vfmprc;

	struct{
		u64 last_vfgprc;
		u64 last_vfgptc;
		u64 last_vfgorc;
		u64 last_vfgotc;
		u64 last_vfmprc;
		u64 vfgprc;
		u64 vfgptc;
		u64 vfgorc;
		u64 vfgotc;
		u64 vfmprc;
	} qp[8];

	u64 saved_reset_vfgprc;
	u64 saved_reset_vfgptc;
	u64 saved_reset_vfgorc;
	u64 saved_reset_vfgotc;
	u64 saved_reset_vfmprc;
};

s32 txgbe_init_ops_vf(struct txgbe_hw *hw);
s32 txgbe_start_hw_vf(struct txgbe_hw *hw);
s32 txgbe_reset_hw_vf(struct txgbe_hw *hw);
s32 txgbe_stop_hw_vf(struct txgbe_hw *hw);
s32 txgbe_get_mac_addr_vf(struct txgbe_hw *hw, u8 *mac_addr);
s32 txgbe_check_mac_link_vf(struct txgbe_hw *hw, u32 *speed,
			    bool *link_up, bool autoneg_wait_to_complete);
s32 txgbe_set_rar_vf(struct txgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		     u32 enable_addr);
s32 txgbevf_set_uc_addr_vf(struct txgbe_hw *hw, u32 index, u8 *addr);
s32 txgbe_update_mc_addr_list_vf(struct txgbe_hw *hw, u8 *mc_addr_list,
				 u32 mc_addr_count, txgbe_mc_addr_itr next,
				 bool clear);
s32 txgbevf_update_xcast_mode(struct txgbe_hw *hw, int xcast_mode);
s32 txgbe_set_vfta_vf(struct txgbe_hw *hw, u32 vlan, u32 vind,
		      bool vlan_on, bool vlvf_bypass);
s32 txgbevf_rlpml_set_vf(struct txgbe_hw *hw, u16 max_size);
int txgbevf_negotiate_api_version(struct txgbe_hw *hw, int api);
int txgbevf_get_queues(struct txgbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc);

#endif /* __TXGBE_VF_H__ */
