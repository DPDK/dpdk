/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_X540_H_
#define _IXGBE_X540_H_

#include "ixgbe_type.h"

s32 ixgbe_get_link_capabilities_X540(struct ixgbe_hw *hw,
				     ixgbe_link_speed *speed, bool *autoneg);
enum ixgbe_media_type ixgbe_get_media_type_X540(struct ixgbe_hw *hw);
s32 ixgbe_setup_mac_link_X540(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			      bool autoneg, bool link_up_wait_to_complete);
s32 ixgbe_reset_hw_X540(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_X540(struct ixgbe_hw *hw);
u32 ixgbe_get_supported_physical_layer_X540(struct ixgbe_hw *hw);

s32 ixgbe_init_eeprom_params_X540(struct ixgbe_hw *hw);
s32 ixgbe_read_eerd_X540(struct ixgbe_hw *hw, u16 offset, u16 *data);
s32 ixgbe_read_eerd_buffer_X540(struct ixgbe_hw *hw, u16 offset, u16 words,
				u16 *data);
s32 ixgbe_write_eewr_X540(struct ixgbe_hw *hw, u16 offset, u16 data);
s32 ixgbe_write_eewr_buffer_X540(struct ixgbe_hw *hw, u16 offset, u16 words,
				 u16 *data);
s32 ixgbe_update_eeprom_checksum_X540(struct ixgbe_hw *hw);
s32 ixgbe_validate_eeprom_checksum_X540(struct ixgbe_hw *hw, u16 *checksum_val);
u16 ixgbe_calc_eeprom_checksum_X540(struct ixgbe_hw *hw);

s32 ixgbe_acquire_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask);
void ixgbe_release_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask);

s32 ixgbe_blink_led_start_X540(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_blink_led_stop_X540(struct ixgbe_hw *hw, u32 index);
#endif /* _IXGBE_X540_H_ */
