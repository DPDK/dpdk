/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_VF_H_
#define _TXGBE_VF_H_

#include "txgbe_type.h"

#define TXGBE_VF_MAX_TX_QUEUES	8
#define TXGBE_VF_MAX_RX_QUEUES	8

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
int txgbevf_negotiate_api_version(struct txgbe_hw *hw, int api);
int txgbevf_get_queues(struct txgbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc);

#endif /* __TXGBE_VF_H__ */
