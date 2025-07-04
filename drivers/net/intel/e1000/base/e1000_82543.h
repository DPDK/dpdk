/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _E1000_82543_H_
#define _E1000_82543_H_

#define PHY_PREAMBLE		0xFFFFFFFF
#define PHY_PREAMBLE_SIZE	32
#define PHY_SOF			0x1
#define PHY_OP_READ		0x2
#define PHY_OP_WRITE		0x1
#define PHY_TURNAROUND		0x2

#define TBI_COMPAT_ENABLED	0x1 /* Global "knob" for the workaround */
/* If TBI_COMPAT_ENABLED, then this is the current state (on/off) */
#define TBI_SBP_ENABLED		0x2

void e1000_tbi_adjust_stats_82543(struct e1000_hw *hw,
				  struct e1000_hw_stats *stats,
				  u32 frame_len, u8 *mac_addr,
				  u32 max_frame_size);
void e1000_set_tbi_compatibility_82543(struct e1000_hw *hw,
				       bool state);
bool e1000_tbi_sbp_enabled_82543(struct e1000_hw *hw);

#endif
