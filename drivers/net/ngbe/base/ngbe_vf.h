/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_VF_H_
#define _NGBE_VF_H_

#include "ngbe_type.h"

#define NGBE_VF_MAX_TX_QUEUES	1
#define NGBE_VF_MAX_RX_QUEUES	1

s32 ngbe_init_ops_vf(struct ngbe_hw *hw);
s32 ngbe_init_hw_vf(struct ngbe_hw *hw);
s32 ngbe_start_hw_vf(struct ngbe_hw *hw);
s32 ngbe_reset_hw_vf(struct ngbe_hw *hw);
s32 ngbe_stop_hw_vf(struct ngbe_hw *hw);
s32 ngbe_get_mac_addr_vf(struct ngbe_hw *hw, u8 *mac_addr);
s32 ngbe_set_rar_vf(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
		     u32 enable_addr);
s32 ngbevf_set_uc_addr_vf(struct ngbe_hw *hw, u32 index, u8 *addr);
s32 ngbevf_update_xcast_mode(struct ngbe_hw *hw, int xcast_mode);
s32 ngbe_set_vfta_vf(struct ngbe_hw *hw, u32 vlan, u32 vind,
		      bool vlan_on, bool vlvf_bypass);
s32 ngbevf_rlpml_set_vf(struct ngbe_hw *hw, u16 max_size);
int ngbevf_negotiate_api_version(struct ngbe_hw *hw, int api);
int ngbevf_get_queues(struct ngbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc);

#endif /* __NGBE_VF_H__ */
