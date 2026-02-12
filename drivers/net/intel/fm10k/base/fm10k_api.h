/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013 - 2015 Intel Corporation
 */

#ifndef _FM10K_API_H_
#define _FM10K_API_H_

#include "fm10k_pf.h"
#include "fm10k_vf.h"

s32 fm10k_set_mac_type(struct fm10k_hw *hw);
s32 fm10k_reset_hw(struct fm10k_hw *hw);
s32 fm10k_init_hw(struct fm10k_hw *hw);
s32 fm10k_stop_hw(struct fm10k_hw *hw);
s32 fm10k_start_hw(struct fm10k_hw *hw);
s32 fm10k_init_shared_code(struct fm10k_hw *hw);
s32 fm10k_get_bus_info(struct fm10k_hw *hw);
#ifndef NO_IS_SLOT_APPROPRIATE_CHECK
bool fm10k_is_slot_appropriate(struct fm10k_hw *hw);
#endif
s32 fm10k_update_vlan(struct fm10k_hw *hw, u32 vid, u8 idx, bool set);
s32 fm10k_read_mac_addr(struct fm10k_hw *hw);
void fm10k_update_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats);
void fm10k_rebind_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats);
s32 fm10k_configure_dglort_map(struct fm10k_hw *hw,
			       struct fm10k_dglort_cfg *dglort);
void fm10k_set_dma_mask(struct fm10k_hw *hw, u64 dma_mask);
s32 fm10k_get_fault(struct fm10k_hw *hw, int type, struct fm10k_fault *fault);
s32 fm10k_update_uc_addr(struct fm10k_hw *hw, u16 lport,
			  const u8 *mac, u16 vid, bool add, u8 flags);
s32 fm10k_update_mc_addr(struct fm10k_hw *hw, u16 lport,
			 const u8 *mac, u16 vid, bool add);
s32 fm10k_adjust_systime(struct fm10k_hw *hw, s32 ppb);
s32 fm10k_notify_offset(struct fm10k_hw *hw, u64 offset);
#endif /* _FM10K_API_H_ */
