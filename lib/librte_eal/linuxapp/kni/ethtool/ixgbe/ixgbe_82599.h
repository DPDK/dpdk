/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "LICENSE.GPL".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_82599_H_
#define _IXGBE_82599_H_

s32 ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw,
				      ixgbe_link_speed *speed, bool *autoneg);
enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw);
void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
s32 ixgbe_setup_mac_link_multispeed_fiber(struct ixgbe_hw *hw,
					  ixgbe_link_speed speed, bool autoneg,
					  bool autoneg_wait_to_complete);
s32 ixgbe_setup_mac_link_smartspeed(struct ixgbe_hw *hw,
				    ixgbe_link_speed speed, bool autoneg,
				    bool autoneg_wait_to_complete);
s32 ixgbe_start_mac_link_82599(struct ixgbe_hw *hw,
			       bool autoneg_wait_to_complete);
s32 ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			       bool autoneg, bool autoneg_wait_to_complete);
s32 ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw);
void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw);
s32 ixgbe_reset_hw_82599(struct ixgbe_hw *hw);
s32 ixgbe_read_analog_reg8_82599(struct ixgbe_hw *hw, u32 reg, u8 *val);
s32 ixgbe_write_analog_reg8_82599(struct ixgbe_hw *hw, u32 reg, u8 val);
s32 ixgbe_start_hw_82599(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_82599(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw);
u32 ixgbe_get_supported_physical_layer_82599(struct ixgbe_hw *hw);
s32 ixgbe_enable_rx_dma_82599(struct ixgbe_hw *hw, u32 regval);
bool ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw);
#endif /* _IXGBE_82599_H_ */
