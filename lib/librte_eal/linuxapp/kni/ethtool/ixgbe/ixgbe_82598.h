/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_82598_H_
#define _IXGBE_82598_H_

u32 ixgbe_get_pcie_msix_count_82598(struct ixgbe_hw *hw);
s32 ixgbe_fc_enable_82598(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_82598(struct ixgbe_hw *hw);
s32 ixgbe_set_vmdq_82598(struct ixgbe_hw *hw, u32 rar, u32 vmdq);
s32 ixgbe_set_vfta_82598(struct ixgbe_hw *hw, u32 vlan, u32 vind, bool vlan_on);
s32 ixgbe_read_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 *val);
s32 ixgbe_write_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 val);
s32 ixgbe_read_i2c_eeprom_82598(struct ixgbe_hw *hw, u8 byte_offset,
				u8 *eeprom_data);
u32 ixgbe_get_supported_physical_layer_82598(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_82598(struct ixgbe_hw *hw);
void ixgbe_set_lan_id_multi_port_pcie_82598(struct ixgbe_hw *hw);
void ixgbe_set_pcie_completion_timeout(struct ixgbe_hw *hw);
#endif /* _IXGBE_82598_H_ */
