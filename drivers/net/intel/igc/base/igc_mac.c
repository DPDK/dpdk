/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "igc_api.h"

static s32 igc_validate_mdi_setting_generic(struct igc_hw *hw);
static void igc_set_lan_id_multi_port_pcie(struct igc_hw *hw);
static void igc_config_collision_dist_generic(struct igc_hw *hw);
static int igc_rar_set_generic(struct igc_hw *hw, u8 *addr, u32 index);

/**
 *  igc_init_mac_ops_generic - Initialize MAC function pointers
 *  @hw: pointer to the HW structure
 *
 *  Setups up the function pointers to no-op functions
 **/
void igc_init_mac_ops_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	DEBUGFUNC("igc_init_mac_ops_generic");

	/* General Setup */
	mac->ops.init_params = igc_null_ops_generic;
	mac->ops.init_hw = igc_null_ops_generic;
	mac->ops.reset_hw = igc_null_ops_generic;
	mac->ops.setup_physical_interface = igc_null_ops_generic;
	mac->ops.get_bus_info = igc_null_ops_generic;
	mac->ops.set_lan_id = igc_set_lan_id_multi_port_pcie;
	mac->ops.read_mac_addr = igc_read_mac_addr_generic;
	mac->ops.config_collision_dist = igc_config_collision_dist_generic;
	mac->ops.clear_hw_cntrs = igc_null_mac_generic;
	/* LED */
	mac->ops.cleanup_led = igc_null_ops_generic;
	mac->ops.setup_led = igc_null_ops_generic;
	mac->ops.blink_led = igc_null_ops_generic;
	mac->ops.led_on = igc_null_ops_generic;
	mac->ops.led_off = igc_null_ops_generic;
	/* LINK */
	mac->ops.setup_link = igc_null_ops_generic;
	mac->ops.get_link_up_info = igc_null_link_info;
	mac->ops.check_for_link = igc_null_ops_generic;
	/* Management */
	mac->ops.check_mng_mode = igc_null_mng_mode;
	/* VLAN, MC, etc. */
	mac->ops.update_mc_addr_list = igc_null_update_mc;
	mac->ops.clear_vfta = igc_null_mac_generic;
	mac->ops.write_vfta = igc_null_write_vfta;
	mac->ops.rar_set = igc_rar_set_generic;
	mac->ops.validate_mdi_setting = igc_validate_mdi_setting_generic;
}

/**
 *  igc_null_ops_generic - No-op function, returns 0
 *  @hw: pointer to the HW structure
 **/
s32 igc_null_ops_generic(struct igc_hw IGC_UNUSEDARG * hw)
{
	DEBUGFUNC("igc_null_ops_generic");
	UNREFERENCED_1PARAMETER(hw);
	return IGC_SUCCESS;
}

/**
 *  igc_null_mac_generic - No-op function, return void
 *  @hw: pointer to the HW structure
 **/
void igc_null_mac_generic(struct igc_hw IGC_UNUSEDARG * hw)
{
	DEBUGFUNC("igc_null_mac_generic");
	UNREFERENCED_1PARAMETER(hw);
}

/**
 *  igc_null_link_info - No-op function, return 0
 *  @hw: pointer to the HW structure
 *  @s: dummy variable
 *  @d: dummy variable
 **/
s32 igc_null_link_info(struct igc_hw IGC_UNUSEDARG * hw,
			 u16 IGC_UNUSEDARG * s, u16 IGC_UNUSEDARG * d)
{
	DEBUGFUNC("igc_null_link_info");
	UNREFERENCED_3PARAMETER(hw, s, d);
	return IGC_SUCCESS;
}

/**
 *  igc_null_mng_mode - No-op function, return false
 *  @hw: pointer to the HW structure
 **/
bool igc_null_mng_mode(struct igc_hw IGC_UNUSEDARG * hw)
{
	DEBUGFUNC("igc_null_mng_mode");
	UNREFERENCED_1PARAMETER(hw);
	return false;
}

/**
 *  igc_null_update_mc - No-op function, return void
 *  @hw: pointer to the HW structure
 *  @h: dummy variable
 *  @a: dummy variable
 **/
void igc_null_update_mc(struct igc_hw IGC_UNUSEDARG * hw,
			  u8 IGC_UNUSEDARG * h, u32 IGC_UNUSEDARG a)
{
	DEBUGFUNC("igc_null_update_mc");
	UNREFERENCED_3PARAMETER(hw, h, a);
}

/**
 *  igc_null_write_vfta - No-op function, return void
 *  @hw: pointer to the HW structure
 *  @a: dummy variable
 *  @b: dummy variable
 **/
void igc_null_write_vfta(struct igc_hw IGC_UNUSEDARG * hw,
			   u32 IGC_UNUSEDARG a, u32 IGC_UNUSEDARG b)
{
	DEBUGFUNC("igc_null_write_vfta");
	UNREFERENCED_3PARAMETER(hw, a, b);
}

/**
 *  igc_null_rar_set - No-op function, return 0
 *  @hw: pointer to the HW structure
 *  @h: dummy variable
 *  @a: dummy variable
 **/
int igc_null_rar_set(struct igc_hw IGC_UNUSEDARG * hw,
			u8 IGC_UNUSEDARG * h, u32 IGC_UNUSEDARG a)
{
	DEBUGFUNC("igc_null_rar_set");
	UNREFERENCED_3PARAMETER(hw, h, a);
	return IGC_SUCCESS;
}

/**
 *  igc_get_bus_info_pci_generic - Get PCI(x) bus information
 *  @hw: pointer to the HW structure
 *
 *  Determines and stores the system bus information for a particular
 *  network interface.  The following bus information is determined and stored:
 *  bus speed, bus width, type (PCI/PCIx), and PCI(-x) function.
 **/
s32 igc_get_bus_info_pci_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	struct igc_bus_info *bus = &hw->bus;
	u32 status = IGC_READ_REG(hw, IGC_STATUS);
	s32 ret_val = IGC_SUCCESS;

	DEBUGFUNC("igc_get_bus_info_pci_generic");

	/* PCI or PCI-X? */
	bus->type = (status & IGC_STATUS_PCIX_MODE)
			? igc_bus_type_pcix
			: igc_bus_type_pci;

	/* Bus speed */
	if (bus->type == igc_bus_type_pci) {
		bus->speed = (status & IGC_STATUS_PCI66)
			     ? igc_bus_speed_66
			     : igc_bus_speed_33;
	} else {
		switch (status & IGC_STATUS_PCIX_SPEED) {
		case IGC_STATUS_PCIX_SPEED_66:
			bus->speed = igc_bus_speed_66;
			break;
		case IGC_STATUS_PCIX_SPEED_100:
			bus->speed = igc_bus_speed_100;
			break;
		case IGC_STATUS_PCIX_SPEED_133:
			bus->speed = igc_bus_speed_133;
			break;
		default:
			bus->speed = igc_bus_speed_reserved;
			break;
		}
	}

	/* Bus width */
	bus->width = (status & IGC_STATUS_BUS64)
		     ? igc_bus_width_64
		     : igc_bus_width_32;

	/* Which PCI(-X) function? */
	mac->ops.set_lan_id(hw);

	return ret_val;
}

/**
 *  igc_get_bus_info_pcie_generic - Get PCIe bus information
 *  @hw: pointer to the HW structure
 *
 *  Determines and stores the system bus information for a particular
 *  network interface.  The following bus information is determined and stored:
 *  bus speed, bus width, type (PCIe), and PCIe function.
 **/
s32 igc_get_bus_info_pcie_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	struct igc_bus_info *bus = &hw->bus;
	s32 ret_val;
	u16 pcie_link_status;

	DEBUGFUNC("igc_get_bus_info_pcie_generic");

	bus->type = igc_bus_type_pci_express;

	ret_val = igc_read_pcie_cap_reg(hw, PCIE_LINK_STATUS,
					  &pcie_link_status);
	if (ret_val) {
		bus->width = igc_bus_width_unknown;
		bus->speed = igc_bus_speed_unknown;
	} else {
		switch (pcie_link_status & PCIE_LINK_SPEED_MASK) {
		case PCIE_LINK_SPEED_2500:
			bus->speed = igc_bus_speed_2500;
			break;
		case PCIE_LINK_SPEED_5000:
			bus->speed = igc_bus_speed_5000;
			break;
		default:
			bus->speed = igc_bus_speed_unknown;
			break;
		}

		bus->width = (enum igc_bus_width)((pcie_link_status &
			      PCIE_LINK_WIDTH_MASK) >> PCIE_LINK_WIDTH_SHIFT);
	}

	mac->ops.set_lan_id(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_set_lan_id_multi_port_pcie - Set LAN id for PCIe multiple port devices
 *
 *  @hw: pointer to the HW structure
 *
 *  Determines the LAN function id by reading memory-mapped registers
 *  and swaps the port value if requested.
 **/
static void igc_set_lan_id_multi_port_pcie(struct igc_hw *hw)
{
	struct igc_bus_info *bus = &hw->bus;
	u32 reg;

	/* The status register reports the correct function number
	 * for the device regardless of function swap state.
	 */
	reg = IGC_READ_REG(hw, IGC_STATUS);
	bus->func = (reg & IGC_STATUS_FUNC_MASK) >> IGC_STATUS_FUNC_SHIFT;
}

/**
 *  igc_set_lan_id_multi_port_pci - Set LAN id for PCI multiple port devices
 *  @hw: pointer to the HW structure
 *
 *  Determines the LAN function id by reading PCI config space.
 **/
void igc_set_lan_id_multi_port_pci(struct igc_hw *hw)
{
	struct igc_bus_info *bus = &hw->bus;
	u16 pci_header_type;
	u32 status;

	igc_read_pci_cfg(hw, PCI_HEADER_TYPE_REGISTER, &pci_header_type);
	if (pci_header_type & PCI_HEADER_TYPE_MULTIFUNC) {
		status = IGC_READ_REG(hw, IGC_STATUS);
		bus->func = (status & IGC_STATUS_FUNC_MASK)
			    >> IGC_STATUS_FUNC_SHIFT;
	} else {
		bus->func = 0;
	}
}

/**
 *  igc_set_lan_id_single_port - Set LAN id for a single port device
 *  @hw: pointer to the HW structure
 *
 *  Sets the LAN function id to zero for a single port device.
 **/
void igc_set_lan_id_single_port(struct igc_hw *hw)
{
	struct igc_bus_info *bus = &hw->bus;

	bus->func = 0;
}

/**
 *  igc_clear_vfta_generic - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  Clears the register array which contains the VLAN filter table by
 *  setting all the values to 0.
 **/
void igc_clear_vfta_generic(struct igc_hw *hw)
{
	u32 offset;

	DEBUGFUNC("igc_clear_vfta_generic");

	for (offset = 0; offset < IGC_VLAN_FILTER_TBL_SIZE; offset++) {
		IGC_WRITE_REG_ARRAY(hw, IGC_VFTA, offset, 0);
		IGC_WRITE_FLUSH(hw);
	}
}

/**
 *  igc_write_vfta_generic - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: register offset in VLAN filter table
 *  @value: register value written to VLAN filter table
 *
 *  Writes value at the given offset in the register array which stores
 *  the VLAN filter table.
 **/
void igc_write_vfta_generic(struct igc_hw *hw, u32 offset, u32 value)
{
	DEBUGFUNC("igc_write_vfta_generic");

	IGC_WRITE_REG_ARRAY(hw, IGC_VFTA, offset, value);
	IGC_WRITE_FLUSH(hw);
}

/**
 *  igc_init_rx_addrs_generic - Initialize receive address's
 *  @hw: pointer to the HW structure
 *  @rar_count: receive address registers
 *
 *  Setup the receive address registers by setting the base receive address
 *  register to the devices MAC address and clearing all the other receive
 *  address registers to 0.
 **/
void igc_init_rx_addrs_generic(struct igc_hw *hw, u16 rar_count)
{
	u32 i;
	u8 mac_addr[ETH_ADDR_LEN] = {0};

	DEBUGFUNC("igc_init_rx_addrs_generic");

	/* Setup the receive address */
	DEBUGOUT("Programming MAC Address into RAR[0]\n");

	hw->mac.ops.rar_set(hw, hw->mac.addr, 0);

	/* Zero out the other (rar_entry_count - 1) receive addresses */
	DEBUGOUT1("Clearing RAR[1-%u]\n", rar_count - 1);
	for (i = 1; i < rar_count; i++)
		hw->mac.ops.rar_set(hw, mac_addr, i);
}

/**
 *  igc_check_alt_mac_addr_generic - Check for alternate MAC addr
 *  @hw: pointer to the HW structure
 *
 *  Checks the nvm for an alternate MAC address.  An alternate MAC address
 *  can be setup by pre-boot software and must be treated like a permanent
 *  address and must override the actual permanent MAC address. If an
 *  alternate MAC address is found it is programmed into RAR0, replacing
 *  the permanent address that was installed into RAR0 by the Si on reset.
 *  This function will return SUCCESS unless it encounters an error while
 *  reading the EEPROM.
 **/
s32 igc_check_alt_mac_addr_generic(struct igc_hw *hw)
{
	u32 i;
	s32 ret_val;
	u16 offset, nvm_alt_mac_addr_offset, nvm_data;
	u8 alt_mac_addr[ETH_ADDR_LEN];

	DEBUGFUNC("igc_check_alt_mac_addr_generic");

	ret_val = hw->nvm.ops.read(hw, NVM_COMPAT, 1, &nvm_data);
	if (ret_val)
		return ret_val;

	/* not supported on older hardware or 82573 */
	if (hw->mac.type < igc_82571 || hw->mac.type == igc_82573)
		return IGC_SUCCESS;

	/* Alternate MAC address is handled by the option ROM for 82580
	 * and newer. SW support not required.
	 */
	if (hw->mac.type >= igc_82580)
		return IGC_SUCCESS;

	ret_val = hw->nvm.ops.read(hw, NVM_ALT_MAC_ADDR_PTR, 1,
				   &nvm_alt_mac_addr_offset);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (nvm_alt_mac_addr_offset == 0xFFFF ||
	    nvm_alt_mac_addr_offset == 0x0000)
		/* There is no Alternate MAC Address */
		return IGC_SUCCESS;

	if (hw->bus.func == IGC_FUNC_1)
		nvm_alt_mac_addr_offset += IGC_ALT_MAC_ADDRESS_OFFSET_LAN1;
	if (hw->bus.func == IGC_FUNC_2)
		nvm_alt_mac_addr_offset += IGC_ALT_MAC_ADDRESS_OFFSET_LAN2;

	if (hw->bus.func == IGC_FUNC_3)
		nvm_alt_mac_addr_offset += IGC_ALT_MAC_ADDRESS_OFFSET_LAN3;
	for (i = 0; i < ETH_ADDR_LEN; i += 2) {
		offset = nvm_alt_mac_addr_offset + (i >> 1);
		ret_val = hw->nvm.ops.read(hw, offset, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			return ret_val;
		}

		alt_mac_addr[i] = (u8)(nvm_data & 0xFF);
		alt_mac_addr[i + 1] = (u8)(nvm_data >> 8);
	}

	/* if multicast bit is set, the alternate address will not be used */
	if (alt_mac_addr[0] & 0x01) {
		DEBUGOUT("Ignoring Alternate Mac Address with MC bit set\n");
		return IGC_SUCCESS;
	}

	/* We have a valid alternate MAC address, and we want to treat it the
	 * same as the normal permanent MAC address stored by the HW into the
	 * RAR. Do this by mapping this address into RAR0.
	 */
	hw->mac.ops.rar_set(hw, alt_mac_addr, 0);

	return IGC_SUCCESS;
}

/**
 *  igc_rar_set_generic - Set receive address register
 *  @hw: pointer to the HW structure
 *  @addr: pointer to the receive address
 *  @index: receive address array register
 *
 *  Sets the receive address array register at index to the address passed
 *  in by addr.
 **/
static int igc_rar_set_generic(struct igc_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	DEBUGFUNC("igc_rar_set_generic");

	/* HW expects these in little endian so we reverse the byte order
	 * from network order (big endian) to little endian
	 */
	rar_low = ((u32)addr[0] | ((u32)addr[1] << 8) |
		   ((u32)addr[2] << 16) | ((u32)addr[3] << 24));

	rar_high = ((u32)addr[4] | ((u32)addr[5] << 8));

	/* If MAC address zero, no need to set the AV bit */
	if (rar_low || rar_high)
		rar_high |= IGC_RAH_AV;

	/* Some bridges will combine consecutive 32-bit writes into
	 * a single burst write, which will malfunction on some parts.
	 * The flushes avoid this.
	 */
	IGC_WRITE_REG(hw, IGC_RAL(index), rar_low);
	IGC_WRITE_FLUSH(hw);
	IGC_WRITE_REG(hw, IGC_RAH(index), rar_high);
	IGC_WRITE_FLUSH(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_hash_mc_addr_generic - Generate a multicast hash value
 *  @hw: pointer to the HW structure
 *  @mc_addr: pointer to a multicast address
 *
 *  Generates a multicast address hash value which is used to determine
 *  the multicast filter table array address and new table value.
 **/
u32 igc_hash_mc_addr_generic(struct igc_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 0;

	DEBUGFUNC("igc_hash_mc_addr_generic");

	/* Register count multiplied by bits per register */
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	/* For a mc_filter_type of 0, bit_shift is the number of left-shifts
	 * where 0xFF would still fall within the hash mask.
	 */
	while (hash_mask >> bit_shift != 0xFF)
		bit_shift++;

	/* The portion of the address that is used for the hash table
	 * is determined by the mc_filter_type setting.
	 * The algorithm is such that there is a total of 8 bits of shifting.
	 * The bit_shift for a mc_filter_type of 0 represents the number of
	 * left-shifts where the MSB of mc_addr[5] would still fall within
	 * the hash_mask.  Case 0 does this exactly.  Since there are a total
	 * of 8 bits of shifting, then mc_addr[4] will shift right the
	 * remaining number of bits. Thus 8 - bit_shift.  The rest of the
	 * cases are a variation of this algorithm...essentially raising the
	 * number of bits to shift mc_addr[5] left, while still keeping the
	 * 8-bit shifting total.
	 *
	 * For example, given the following Destination MAC Address and an
	 * mta register count of 128 (thus a 4096-bit vector and 0xFFF mask),
	 * we can see that the bit_shift for case 0 is 4.  These are the hash
	 * values resulting from each mc_filter_type...
	 * [0] [1] [2] [3] [4] [5]
	 * 01  AA  00  12  34  56
	 * LSB		 MSB
	 *
	 * case 0: hash_value = ((0x34 >> 4) | (0x56 << 4)) & 0xFFF = 0x563
	 * case 1: hash_value = ((0x34 >> 3) | (0x56 << 5)) & 0xFFF = 0xAC6
	 * case 2: hash_value = ((0x34 >> 2) | (0x56 << 6)) & 0xFFF = 0x163
	 * case 3: hash_value = ((0x34 >> 0) | (0x56 << 8)) & 0xFFF = 0x634
	 */
	switch (hw->mac.mc_filter_type) {
	default:
	case 0:
		break;
	case 1:
		bit_shift += 1;
		break;
	case 2:
		bit_shift += 2;
		break;
	case 3:
		bit_shift += 4;
		break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
				  (((u16)mc_addr[5]) << bit_shift)));

	return hash_value;
}

/**
 *  igc_update_mc_addr_list_generic - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *
 *  Updates entire Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 **/
void igc_update_mc_addr_list_generic(struct igc_hw *hw,
				       u8 *mc_addr_list, u32 mc_addr_count)
{
	u32 hash_value, hash_bit, hash_reg;
	int i;

	DEBUGFUNC("igc_update_mc_addr_list_generic");

	/* clear mta_shadow */
	memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));

	/* update mta_shadow from mc_addr_list */
	for (i = 0; (u32)i < mc_addr_count; i++) {
		hash_value = igc_hash_mc_addr_generic(hw, mc_addr_list);

		hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
		hash_bit = hash_value & 0x1F;

		hw->mac.mta_shadow[hash_reg] |= (1 << hash_bit);
		mc_addr_list += (ETH_ADDR_LEN);
	}

	/* replace the entire MTA table */
	for (i = hw->mac.mta_reg_count - 1; i >= 0; i--)
		IGC_WRITE_REG_ARRAY(hw, IGC_MTA, i, hw->mac.mta_shadow[i]);
	IGC_WRITE_FLUSH(hw);
}

/**
 *  igc_pcix_mmrbc_workaround_generic - Fix incorrect MMRBC value
 *  @hw: pointer to the HW structure
 *
 *  In certain situations, a system BIOS may report that the PCIx maximum
 *  memory read byte count (MMRBC) value is higher than than the actual
 *  value. We check the PCIx command register with the current PCIx status
 *  register.
 **/
void igc_pcix_mmrbc_workaround_generic(struct igc_hw *hw)
{
	u16 cmd_mmrbc;
	u16 pcix_cmd;
	u16 pcix_stat_hi_word;
	u16 stat_mmrbc;

	DEBUGFUNC("igc_pcix_mmrbc_workaround_generic");

	/* Workaround for PCI-X issue when BIOS sets MMRBC incorrectly */
	if (hw->bus.type != igc_bus_type_pcix)
		return;

	igc_read_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd);
	igc_read_pci_cfg(hw, PCIX_STATUS_REGISTER_HI, &pcix_stat_hi_word);
	cmd_mmrbc = (pcix_cmd & PCIX_COMMAND_MMRBC_MASK) >>
		     PCIX_COMMAND_MMRBC_SHIFT;
	stat_mmrbc = (pcix_stat_hi_word & PCIX_STATUS_HI_MMRBC_MASK) >>
		      PCIX_STATUS_HI_MMRBC_SHIFT;
	if (stat_mmrbc == PCIX_STATUS_HI_MMRBC_4K)
		stat_mmrbc = PCIX_STATUS_HI_MMRBC_2K;
	if (cmd_mmrbc > stat_mmrbc) {
		pcix_cmd &= ~PCIX_COMMAND_MMRBC_MASK;
		pcix_cmd |= stat_mmrbc << PCIX_COMMAND_MMRBC_SHIFT;
		igc_write_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd);
	}
}

/**
 *  igc_clear_hw_cntrs_base_generic - Clear base hardware counters
 *  @hw: pointer to the HW structure
 *
 *  Clears the base hardware counters by reading the counter registers.
 **/
void igc_clear_hw_cntrs_base_generic(struct igc_hw *hw)
{
	DEBUGFUNC("igc_clear_hw_cntrs_base_generic");

	IGC_READ_REG(hw, IGC_CRCERRS);
	IGC_READ_REG(hw, IGC_SYMERRS);
	IGC_READ_REG(hw, IGC_MPC);
	IGC_READ_REG(hw, IGC_SCC);
	IGC_READ_REG(hw, IGC_ECOL);
	IGC_READ_REG(hw, IGC_MCC);
	IGC_READ_REG(hw, IGC_LATECOL);
	IGC_READ_REG(hw, IGC_COLC);
	IGC_READ_REG(hw, IGC_DC);
	IGC_READ_REG(hw, IGC_SEC);
	IGC_READ_REG(hw, IGC_RLEC);
	IGC_READ_REG(hw, IGC_XONRXC);
	IGC_READ_REG(hw, IGC_XONTXC);
	IGC_READ_REG(hw, IGC_XOFFRXC);
	IGC_READ_REG(hw, IGC_XOFFTXC);
	IGC_READ_REG(hw, IGC_FCRUC);
	IGC_READ_REG(hw, IGC_GPRC);
	IGC_READ_REG(hw, IGC_BPRC);
	IGC_READ_REG(hw, IGC_MPRC);
	IGC_READ_REG(hw, IGC_GPTC);
	IGC_READ_REG(hw, IGC_GORCL);
	IGC_READ_REG(hw, IGC_GORCH);
	IGC_READ_REG(hw, IGC_GOTCL);
	IGC_READ_REG(hw, IGC_GOTCH);
	IGC_READ_REG(hw, IGC_RNBC);
	IGC_READ_REG(hw, IGC_RUC);
	IGC_READ_REG(hw, IGC_RFC);
	IGC_READ_REG(hw, IGC_ROC);
	IGC_READ_REG(hw, IGC_RJC);
	IGC_READ_REG(hw, IGC_TORL);
	IGC_READ_REG(hw, IGC_TORH);
	IGC_READ_REG(hw, IGC_TOTL);
	IGC_READ_REG(hw, IGC_TOTH);
	IGC_READ_REG(hw, IGC_TPR);
	IGC_READ_REG(hw, IGC_TPT);
	IGC_READ_REG(hw, IGC_MPTC);
	IGC_READ_REG(hw, IGC_BPTC);
}

/**
 *  igc_check_for_copper_link_generic - Check for link (Copper)
 *  @hw: pointer to the HW structure
 *
 *  Checks to see of the link status of the hardware has changed.  If a
 *  change in link status has been detected, then we read the PHY registers
 *  to get the current speed/duplex if link exists.
 **/
s32 igc_check_for_copper_link_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	DEBUGFUNC("igc_check_for_copper_link");

	/* We only want to go out to the PHY registers to see if Auto-Neg
	 * has completed and/or if our link status has changed.  The
	 * get_link_status flag is set upon receiving a Link Status
	 * Change or Rx Sequence Error interrupt.
	 */
	if (!mac->get_link_status)
		return IGC_SUCCESS;

	/* First we want to see if the MII Status Register reports
	 * link.  If so, then we want to get the current speed/duplex
	 * of the PHY.
	 */
	ret_val = igc_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link)
		return IGC_SUCCESS; /* No link detected */

	mac->get_link_status = false;

	/* Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	igc_check_downshift_generic(hw);

	/* If we are forcing speed/duplex, then we simply return since
	 * we have already determined whether we have link or not.
	 */
	if (!mac->autoneg)
		return -IGC_ERR_CONFIG;

	/* Auto-Neg is enabled.  Auto Speed Detection takes care
	 * of MAC speed/duplex configuration.  So we only need to
	 * configure Collision Distance in the MAC.
	 */
	mac->ops.config_collision_dist(hw);

	/* Configure Flow Control now that Auto-Neg has completed.
	 * First, we need to restore the desired flow control
	 * settings because we may have had to re-autoneg with a
	 * different link partner.
	 */
	ret_val = igc_config_fc_after_link_up_generic(hw);
	if (ret_val)
		DEBUGOUT("Error configuring flow control\n");

	return ret_val;
}

/**
 *  igc_check_for_fiber_link_generic - Check for link (Fiber)
 *  @hw: pointer to the HW structure
 *
 *  Checks for link up on the hardware.  If link is not up and we have
 *  a signal, then we need to force link up.
 **/
s32 igc_check_for_fiber_link_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	DEBUGFUNC("igc_check_for_fiber_link_generic");

	ctrl = IGC_READ_REG(hw, IGC_CTRL);
	status = IGC_READ_REG(hw, IGC_STATUS);
	rxcw = IGC_READ_REG(hw, IGC_RXCW);

	/* If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), the cable is plugged in (we have signal),
	 * and our link partner is not trying to auto-negotiate with us (we
	 * are receiving idles or data), we need to force link up. We also
	 * need to give auto-negotiation time to complete, in case the cable
	 * was just plugged in. The autoneg_failed flag does this.
	 */
	/* (ctrl & IGC_CTRL_SWDPIN1) == 1 == have signal */
	if ((ctrl & IGC_CTRL_SWDPIN1) && !(status & IGC_STATUS_LU) &&
	    !(rxcw & IGC_RXCW_C)) {
		if (!mac->autoneg_failed) {
			mac->autoneg_failed = true;
			return IGC_SUCCESS;
		}
		DEBUGOUT("NOT Rx'ing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		IGC_WRITE_REG(hw, IGC_TXCW, (mac->txcw & ~IGC_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = IGC_READ_REG(hw, IGC_CTRL);
		ctrl |= (IGC_CTRL_SLU | IGC_CTRL_FD);
		IGC_WRITE_REG(hw, IGC_CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = igc_config_fc_after_link_up_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & IGC_CTRL_SLU) && (rxcw & IGC_RXCW_C)) {
		/* If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		DEBUGOUT("Rx'ing /C/, enable AutoNeg and stop forcing link.\n");
		IGC_WRITE_REG(hw, IGC_TXCW, mac->txcw);
		IGC_WRITE_REG(hw, IGC_CTRL, (ctrl & ~IGC_CTRL_SLU));

		mac->serdes_has_link = true;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_check_for_serdes_link_generic - Check for link (Serdes)
 *  @hw: pointer to the HW structure
 *
 *  Checks for link up on the hardware.  If link is not up and we have
 *  a signal, then we need to force link up.
 **/
s32 igc_check_for_serdes_link_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	DEBUGFUNC("igc_check_for_serdes_link_generic");

	ctrl = IGC_READ_REG(hw, IGC_CTRL);
	status = IGC_READ_REG(hw, IGC_STATUS);
	rxcw = IGC_READ_REG(hw, IGC_RXCW);

	/* If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), and our link partner is not trying to
	 * auto-negotiate with us (we are receiving idles or data),
	 * we need to force link up. We also need to give auto-negotiation
	 * time to complete.
	 */
	/* (ctrl & IGC_CTRL_SWDPIN1) == 1 == have signal */
	if (!(status & IGC_STATUS_LU) && !(rxcw & IGC_RXCW_C)) {
		if (!mac->autoneg_failed) {
			mac->autoneg_failed = true;
			return IGC_SUCCESS;
		}
		DEBUGOUT("NOT Rx'ing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		IGC_WRITE_REG(hw, IGC_TXCW, (mac->txcw & ~IGC_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = IGC_READ_REG(hw, IGC_CTRL);
		ctrl |= (IGC_CTRL_SLU | IGC_CTRL_FD);
		IGC_WRITE_REG(hw, IGC_CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = igc_config_fc_after_link_up_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & IGC_CTRL_SLU) && (rxcw & IGC_RXCW_C)) {
		/* If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		DEBUGOUT("Rx'ing /C/, enable AutoNeg and stop forcing link.\n");
		IGC_WRITE_REG(hw, IGC_TXCW, mac->txcw);
		IGC_WRITE_REG(hw, IGC_CTRL, (ctrl & ~IGC_CTRL_SLU));

		mac->serdes_has_link = true;
	} else if (!(IGC_TXCW_ANE & IGC_READ_REG(hw, IGC_TXCW))) {
		/* If we force link for non-auto-negotiation switch, check
		 * link status based on MAC synchronization for internal
		 * serdes media type.
		 */
		/* SYNCH bit and IV bit are sticky. */
		usec_delay(10);
		rxcw = IGC_READ_REG(hw, IGC_RXCW);
		if (rxcw & IGC_RXCW_SYNCH) {
			if (!(rxcw & IGC_RXCW_IV)) {
				mac->serdes_has_link = true;
				DEBUGOUT("SERDES: Link up - forced.\n");
			}
		} else {
			mac->serdes_has_link = false;
			DEBUGOUT("SERDES: Link down - force failed.\n");
		}
	}

	if (IGC_TXCW_ANE & IGC_READ_REG(hw, IGC_TXCW)) {
		status = IGC_READ_REG(hw, IGC_STATUS);
		if (status & IGC_STATUS_LU) {
			/* SYNCH bit and IV bit are sticky, so reread rxcw. */
			usec_delay(10);
			rxcw = IGC_READ_REG(hw, IGC_RXCW);
			if (rxcw & IGC_RXCW_SYNCH) {
				if (!(rxcw & IGC_RXCW_IV)) {
					mac->serdes_has_link = true;
					DEBUGOUT("SERDES: Link up - autoneg completed successfully.\n");
				} else {
					mac->serdes_has_link = false;
					DEBUGOUT("SERDES: Link down - invalid codewords detected in autoneg.\n");
				}
			} else {
				mac->serdes_has_link = false;
				DEBUGOUT("SERDES: Link down - no sync.\n");
			}
		} else {
			mac->serdes_has_link = false;
			DEBUGOUT("SERDES: Link down - autoneg failed\n");
		}
	}

	return IGC_SUCCESS;
}

/**
 *  igc_set_default_fc_generic - Set flow control default values
 *  @hw: pointer to the HW structure
 *
 *  Read the EEPROM for the default values for flow control and store the
 *  values.
 **/
s32 igc_set_default_fc_generic(struct igc_hw *hw)
{
	s32 ret_val;
	u16 nvm_data;
	u16 nvm_offset = 0;

	DEBUGFUNC("igc_set_default_fc_generic");

	/* Read and store word 0x0F of the EEPROM. This word contains bits
	 * that determine the hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the HW defaults to enabling or
	 * disabling auto-negotiation, and the direction of the
	 * SW defined pins. If there is no SW over-ride of the flow
	 * control setting, then the variable hw->fc will
	 * be initialized based on a value in the EEPROM.
	 */
	if (hw->mac.type == igc_i350) {
		nvm_offset = NVM_82580_LAN_FUNC_OFFSET(hw->bus.func);
		ret_val = hw->nvm.ops.read(hw,
					   NVM_INIT_CONTROL2_REG +
					   nvm_offset,
					   1, &nvm_data);
	} else {
		ret_val = hw->nvm.ops.read(hw,
					   NVM_INIT_CONTROL2_REG,
					   1, &nvm_data);
	}

	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (!(nvm_data & NVM_WORD0F_PAUSE_MASK))
		hw->fc.requested_mode = igc_fc_none;
	else if ((nvm_data & NVM_WORD0F_PAUSE_MASK) ==
		 NVM_WORD0F_ASM_DIR)
		hw->fc.requested_mode = igc_fc_tx_pause;
	else
		hw->fc.requested_mode = igc_fc_full;

	return IGC_SUCCESS;
}

/**
 *  igc_setup_link_generic - Setup flow control and link settings
 *  @hw: pointer to the HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the appropriate media-specific link configuration
 *  function.  Assuming the adapter has a valid link partner, a valid link
 *  should be established.  Assumes the hardware has previously been reset
 *  and the transmitter and receiver are not enabled.
 **/
s32 igc_setup_link_generic(struct igc_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("igc_setup_link_generic");

	/* In the case of the phy reset being blocked, we already have a link.
	 * We do not need to set it up again.
	 */
	if (hw->phy.ops.check_reset_block && hw->phy.ops.check_reset_block(hw))
		return IGC_SUCCESS;

	/* If requested flow control is set to default, set flow control
	 * based on the EEPROM flow control settings.
	 */
	if (hw->fc.requested_mode == igc_fc_default)
		hw->fc.requested_mode = igc_fc_full;

	/* Save off the requested flow control mode for use later.  Depending
	 * on the link partner's capabilities, we may or may not use this mode.
	 */
	hw->fc.current_mode = hw->fc.requested_mode;

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n",
		hw->fc.current_mode);

	/* Call the necessary media_type subroutine to configure the link. */
	ret_val = hw->mac.ops.setup_physical_interface(hw);
	if (ret_val)
		return ret_val;

	/* Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	DEBUGOUT("Initializing the Flow Control address, type and timer regs\n");
	IGC_WRITE_REG(hw, IGC_FCT, FLOW_CONTROL_TYPE);
	IGC_WRITE_REG(hw, IGC_FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	IGC_WRITE_REG(hw, IGC_FCAL, FLOW_CONTROL_ADDRESS_LOW);

	IGC_WRITE_REG(hw, IGC_FCTTV, hw->fc.pause_time);

	return igc_set_fc_watermarks_generic(hw);
}

/**
 *  igc_commit_fc_settings_generic - Configure flow control
 *  @hw: pointer to the HW structure
 *
 *  Write the flow control settings to the Transmit Config Word Register (TXCW)
 *  base on the flow control settings in igc_mac_info.
 **/
s32 igc_commit_fc_settings_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	u32 txcw;

	DEBUGFUNC("igc_commit_fc_settings_generic");

	/* Check for a software override of the flow control settings, and
	 * setup the device accordingly.  If auto-negotiation is enabled, then
	 * software will have to set the "PAUSE" bits to the correct value in
	 * the Transmit Config Word Register (TXCW) and re-start auto-
	 * negotiation.  However, if auto-negotiation is disabled, then
	 * software will have to manually configure the two flow control enable
	 * bits in the CTRL register.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames,
	 *          but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames but we
	 *          do not support receiving pause frames).
	 *      3:  Both Rx and Tx flow control (symmetric) are enabled.
	 */
	switch (hw->fc.current_mode) {
	case igc_fc_none:
		/* Flow control completely disabled by a software over-ride. */
		txcw = (IGC_TXCW_ANE | IGC_TXCW_FD);
		break;
	case igc_fc_rx_pause:
		/* Rx Flow control is enabled and Tx Flow control is disabled
		 * by a software over-ride. Since there really isn't a way to
		 * advertise that we are capable of Rx Pause ONLY, we will
		 * advertise that we support both symmetric and asymmetric Rx
		 * PAUSE.  Later, we will disable the adapter's ability to send
		 * PAUSE frames.
		 */
		txcw = (IGC_TXCW_ANE | IGC_TXCW_FD | IGC_TXCW_PAUSE_MASK);
		break;
	case igc_fc_tx_pause:
		/* Tx Flow control is enabled, and Rx Flow control is disabled,
		 * by a software over-ride.
		 */
		txcw = (IGC_TXCW_ANE | IGC_TXCW_FD | IGC_TXCW_ASM_DIR);
		break;
	case igc_fc_full:
		/* Flow control (both Rx and Tx) is enabled by a software
		 * over-ride.
		 */
		txcw = (IGC_TXCW_ANE | IGC_TXCW_FD | IGC_TXCW_PAUSE_MASK);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -IGC_ERR_CONFIG;
	}

	IGC_WRITE_REG(hw, IGC_TXCW, txcw);
	mac->txcw = txcw;

	return IGC_SUCCESS;
}

/**
 *  igc_poll_fiber_serdes_link_generic - Poll for link up
 *  @hw: pointer to the HW structure
 *
 *  Polls for link up by reading the status register, if link fails to come
 *  up with auto-negotiation, then the link is forced if a signal is detected.
 **/
s32 igc_poll_fiber_serdes_link_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	u32 i, status;
	s32 ret_val;

	DEBUGFUNC("igc_poll_fiber_serdes_link_generic");

	/* If we have a signal (the cable is plugged in, or assumed true for
	 * serdes media) then poll for a "Link-Up" indication in the Device
	 * Status Register.  Time-out if a link isn't seen in 500 milliseconds
	 * seconds (Auto-negotiation should complete in less than 500
	 * milliseconds even if the other end is doing it in SW).
	 */
	for (i = 0; i < FIBER_LINK_UP_LIMIT; i++) {
		msec_delay(10);
		status = IGC_READ_REG(hw, IGC_STATUS);
		if (status & IGC_STATUS_LU)
			break;
	}
	if (i == FIBER_LINK_UP_LIMIT) {
		DEBUGOUT("Never got a valid link from auto-neg!!!\n");
		mac->autoneg_failed = true;
		/* AutoNeg failed to achieve a link, so we'll call
		 * mac->check_for_link. This routine will force the
		 * link up if we detect a signal. This will allow us to
		 * communicate with non-autonegotiating link partners.
		 */
		ret_val = mac->ops.check_for_link(hw);
		if (ret_val) {
			DEBUGOUT("Error while checking for link\n");
			return ret_val;
		}
		mac->autoneg_failed = false;
	} else {
		mac->autoneg_failed = false;
		DEBUGOUT("Valid Link Found\n");
	}

	return IGC_SUCCESS;
}

/**
 *  igc_setup_fiber_serdes_link_generic - Setup link for fiber/serdes
 *  @hw: pointer to the HW structure
 *
 *  Configures collision distance and flow control for fiber and serdes
 *  links.  Upon successful setup, poll for link.
 **/
s32 igc_setup_fiber_serdes_link_generic(struct igc_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	DEBUGFUNC("igc_setup_fiber_serdes_link_generic");

	ctrl = IGC_READ_REG(hw, IGC_CTRL);

	/* Take the link out of reset */
	ctrl &= ~IGC_CTRL_LRST;

	hw->mac.ops.config_collision_dist(hw);

	ret_val = igc_commit_fc_settings_generic(hw);
	if (ret_val)
		return ret_val;

	/* Since auto-negotiation is enabled, take the link out of reset (the
	 * link will be in reset, because we previously reset the chip). This
	 * will restart auto-negotiation.  If auto-negotiation is successful
	 * then the link-up status bit will be set and the flow control enable
	 * bits (RFCE and TFCE) will be set according to their negotiated value.
	 */
	DEBUGOUT("Auto-negotiation enabled\n");

	IGC_WRITE_REG(hw, IGC_CTRL, ctrl);
	IGC_WRITE_FLUSH(hw);
	msec_delay(1);

	/* For these adapters, the SW definable pin 1 is set when the optics
	 * detect a signal.  If we have a signal, then poll for a "Link-Up"
	 * indication.
	 */
	if (hw->phy.media_type == igc_media_type_internal_serdes ||
	    (IGC_READ_REG(hw, IGC_CTRL) & IGC_CTRL_SWDPIN1)) {
		ret_val = igc_poll_fiber_serdes_link_generic(hw);
	} else {
		DEBUGOUT("No signal detected\n");
	}

	return ret_val;
}

/**
 *  igc_config_collision_dist_generic - Configure collision distance
 *  @hw: pointer to the HW structure
 *
 *  Configures the collision distance to the default value and is used
 *  during link setup.
 **/
static void igc_config_collision_dist_generic(struct igc_hw *hw)
{
	u32 tctl;

	DEBUGFUNC("igc_config_collision_dist_generic");

	tctl = IGC_READ_REG(hw, IGC_TCTL);

	tctl &= ~IGC_TCTL_COLD;
	tctl |= IGC_COLLISION_DISTANCE << IGC_COLD_SHIFT;

	IGC_WRITE_REG(hw, IGC_TCTL, tctl);
	IGC_WRITE_FLUSH(hw);
}

/**
 *  igc_set_fc_watermarks_generic - Set flow control high/low watermarks
 *  @hw: pointer to the HW structure
 *
 *  Sets the flow control high/low threshold (watermark) registers.  If
 *  flow control XON frame transmission is enabled, then set XON frame
 *  transmission as well.
 **/
s32 igc_set_fc_watermarks_generic(struct igc_hw *hw)
{
	u32 fcrtl = 0, fcrth = 0;

	DEBUGFUNC("igc_set_fc_watermarks_generic");

	/* Set the flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the driver's runtime code.  However, if the
	 * ability to transmit pause frames is not enabled, then these
	 * registers will be set to 0.
	 */
	if (hw->fc.current_mode & igc_fc_tx_pause) {
		/* We need to set up the Receive Threshold high and low water
		 * marks as well as (optionally) enabling the transmission of
		 * XON frames.
		 */
		fcrtl = hw->fc.low_water;
		if (hw->fc.send_xon)
			fcrtl |= IGC_FCRTL_XONE;

		fcrth = hw->fc.high_water;
	}
	IGC_WRITE_REG(hw, IGC_FCRTL, fcrtl);
	IGC_WRITE_REG(hw, IGC_FCRTH, fcrth);

	return IGC_SUCCESS;
}

/**
 *  igc_force_mac_fc_generic - Force the MAC's flow control settings
 *  @hw: pointer to the HW structure
 *
 *  Force the MAC's flow control settings.  Sets the TFCE and RFCE bits in the
 *  device control register to reflect the adapter settings.  TFCE and RFCE
 *  need to be explicitly set by software when a copper PHY is used because
 *  autonegotiation is managed by the PHY rather than the MAC.  Software must
 *  also configure these bits when link is forced on a fiber connection.
 **/
s32 igc_force_mac_fc_generic(struct igc_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("igc_force_mac_fc_generic");

	ctrl = IGC_READ_REG(hw, IGC_CTRL);

	/* Because we didn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the "hw->fc.current_mode" parameter.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *          frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          frames but we do not receive pause frames).
	 *      3:  Both Rx and Tx flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */
	DEBUGOUT1("hw->fc.current_mode = %u\n", hw->fc.current_mode);

	switch (hw->fc.current_mode) {
	case igc_fc_none:
		ctrl &= (~(IGC_CTRL_TFCE | IGC_CTRL_RFCE));
		break;
	case igc_fc_rx_pause:
		ctrl &= (~IGC_CTRL_TFCE);
		ctrl |= IGC_CTRL_RFCE;
		break;
	case igc_fc_tx_pause:
		ctrl &= (~IGC_CTRL_RFCE);
		ctrl |= IGC_CTRL_TFCE;
		break;
	case igc_fc_full:
		ctrl |= (IGC_CTRL_TFCE | IGC_CTRL_RFCE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -IGC_ERR_CONFIG;
	}

	IGC_WRITE_REG(hw, IGC_CTRL, ctrl);

	return IGC_SUCCESS;
}

/**
 *  igc_config_fc_after_link_up_generic - Configures flow control after link
 *  @hw: pointer to the HW structure
 *
 *  Checks the status of auto-negotiation after link up to ensure that the
 *  speed and duplex were not forced.  If the link needed to be forced, then
 *  flow control needs to be forced also.  If auto-negotiation is enabled
 *  and did not fail, then we configure flow control based on our link
 *  partner.
 **/
s32 igc_config_fc_after_link_up_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	s32 ret_val = IGC_SUCCESS;
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	u16 speed, duplex;

	DEBUGFUNC("igc_config_fc_after_link_up_generic");

	/* Check for the case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the MAC to match the "fc" parameter.
	 */
	if (mac->autoneg_failed) {
		if (hw->phy.media_type == igc_media_type_copper)
			ret_val = igc_force_mac_fc_generic(hw);
	}

	if (ret_val) {
		DEBUGOUT("Error forcing flow control settings\n");
		return ret_val;
	}

	/* Check for the case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the PHY and link partner has
	 * flow control configured.
	 */
	if (hw->phy.media_type == igc_media_type_copper && mac->autoneg) {
		/* Read the MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			DEBUGOUT("Copper PHY and Auto Neg has not completed.\n");
			return ret_val;
		}

		/* The AutoNeg process has completed, so we now need to
		 * read both the Auto Negotiation Advertisement
		 * Register (Address 4) and the Auto_Negotiation Base
		 * Page Ability Register (Address 5) to determine how
		 * flow control was negotiated.
		 */
		ret_val = hw->phy.ops.read_reg(hw, PHY_AUTONEG_ADV,
					       &mii_nway_adv_reg);
		if (ret_val)
			return ret_val;
		ret_val = hw->phy.ops.read_reg(hw, PHY_LP_ABILITY,
					       &mii_nway_lp_ability_reg);
		if (ret_val)
			return ret_val;

		/* Two bits in the Auto Negotiation Advertisement Register
		 * (Address 4) and two bits in the Auto Negotiation Base
		 * Page Ability Register (Address 5) determine flow control
		 * for both the PHY and the link partner.  The following
		 * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
		 * 1999, describes these PAUSE resolution bits and how flow
		 * control is determined based upon these settings.
		 * NOTE:  DC = Don't Care
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
		 *-------|---------|-------|---------|--------------------
		 *   0   |    0    |  DC   |   DC    | igc_fc_none
		 *   0   |    1    |   0   |   DC    | igc_fc_none
		 *   0   |    1    |   1   |    0    | igc_fc_none
		 *   0   |    1    |   1   |    1    | igc_fc_tx_pause
		 *   1   |    0    |   0   |   DC    | igc_fc_none
		 *   1   |   DC    |   1   |   DC    | igc_fc_full
		 *   1   |    1    |   0   |    0    | igc_fc_none
		 *   1   |    1    |   0   |    1    | igc_fc_rx_pause
		 *
		 * Are both PAUSE bits set to 1?  If so, this implies
		 * Symmetric Flow Control is enabled at both ends.  The
		 * ASM_DIR bits are irrelevant per the spec.
		 *
		 * For Symmetric Flow Control:
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |   DC    |   1   |   DC    | IGC_fc_full
		 *
		 */
		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			/* Now we need to check if the user selected Rx ONLY
			 * of pause frames.  In this case, we had to advertise
			 * FULL flow control because we could not advertise Rx
			 * ONLY. Hence, we must now check to see if we need to
			 * turn OFF the TRANSMISSION of PAUSE frames.
			 */
			if (hw->fc.requested_mode == igc_fc_full) {
				hw->fc.current_mode = igc_fc_full;
				DEBUGOUT("Flow Control = FULL.\n");
			} else {
				hw->fc.current_mode = igc_fc_rx_pause;
				DEBUGOUT("Flow Control = Rx PAUSE frames only.\n");
			}
		}
		/* For receiving PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   0   |    1    |   1   |    1    | igc_fc_tx_pause
		 */
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			  (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = igc_fc_tx_pause;
			DEBUGOUT("Flow Control = Tx PAUSE frames only.\n");
		}
		/* For transmitting PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |    1    |   0   |    1    | igc_fc_rx_pause
		 */
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = igc_fc_rx_pause;
			DEBUGOUT("Flow Control = Rx PAUSE frames only.\n");
		} else {
			/* Per the IEEE spec, at this point flow control
			 * should be disabled.
			 */
			hw->fc.current_mode = igc_fc_none;
			DEBUGOUT("Flow Control = NONE.\n");
		}

		/* Now we need to do one last check...  If we auto-
		 * negotiated to HALF DUPLEX, flow control should not be
		 * enabled per IEEE 802.3 spec.
		 */
		ret_val = mac->ops.get_link_up_info(hw, &speed, &duplex);
		if (ret_val) {
			DEBUGOUT("Error getting link speed and duplex\n");
			return ret_val;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.current_mode = igc_fc_none;

		/* Now we call a subroutine to actually force the MAC
		 * controller to use the correct flow control settings.
		 */
		ret_val = igc_force_mac_fc_generic(hw);
		if (ret_val) {
			DEBUGOUT("Error forcing flow control settings\n");
			return ret_val;
		}
	}

	return IGC_SUCCESS;
}

/**
 *  igc_get_speed_and_duplex_copper_generic - Retrieve current speed/duplex
 *  @hw: pointer to the HW structure
 *  @speed: stores the current speed
 *  @duplex: stores the current duplex
 *
 *  Read the status register for the current speed/duplex and store the current
 *  speed and duplex for copper connections.
 **/
s32 igc_get_speed_and_duplex_copper_generic(struct igc_hw *hw, u16 *speed,
					      u16 *duplex)
{
	u32 status;

	DEBUGFUNC("igc_get_speed_and_duplex_copper_generic");

	status = IGC_READ_REG(hw, IGC_STATUS);
	if (status & IGC_STATUS_SPEED_1000) {
		/* For I225, STATUS will indicate 1G speed in both 1 Gbps
		 * and 2.5 Gbps link modes. An additional bit is used
		 * to differentiate between 1 Gbps and 2.5 Gbps.
		 */
		if (hw->mac.type == igc_i225 &&
		    (status & IGC_STATUS_SPEED_2500)) {
			*speed = SPEED_2500;
			DEBUGOUT("2500 Mbs, ");
		} else {
			*speed = SPEED_1000;
			DEBUGOUT("1000 Mbs, ");
		}
	} else if (status & IGC_STATUS_SPEED_100) {
		*speed = SPEED_100;
		DEBUGOUT("100 Mbs, ");
	} else {
		*speed = SPEED_10;
		DEBUGOUT("10 Mbs, ");
	}

	if (status & IGC_STATUS_FD) {
		*duplex = FULL_DUPLEX;
		DEBUGOUT("Full Duplex\n");
	} else {
		*duplex = HALF_DUPLEX;
		DEBUGOUT("Half Duplex\n");
	}

	return IGC_SUCCESS;
}

/**
 *  igc_get_speed_and_duplex_fiber_generic - Retrieve current speed/duplex
 *  @hw: pointer to the HW structure
 *  @speed: stores the current speed
 *  @duplex: stores the current duplex
 *
 *  Sets the speed and duplex to gigabit full duplex (the only possible option)
 *  for fiber/serdes links.
 **/
s32
igc_get_speed_and_duplex_fiber_serdes_generic(struct igc_hw *hw,
				u16 *speed, u16 *duplex)
{
	DEBUGFUNC("igc_get_speed_and_duplex_fiber_serdes_generic");
	UNREFERENCED_1PARAMETER(hw);

	*speed = SPEED_1000;
	*duplex = FULL_DUPLEX;

	return IGC_SUCCESS;
}

/**
 *  igc_get_hw_semaphore_generic - Acquire hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Acquire the HW semaphore to access the PHY or NVM
 **/
s32 igc_get_hw_semaphore_generic(struct igc_hw *hw)
{
	u32 swsm;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	DEBUGFUNC("igc_get_hw_semaphore_generic");

	/* Get the SW semaphore */
	while (i < timeout) {
		swsm = IGC_READ_REG(hw, IGC_SWSM);
		if (!(swsm & IGC_SWSM_SMBI))
			break;

		usec_delay(50);
		i++;
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access device - SMBI bit is set.\n");
		return -IGC_ERR_NVM;
	}

	/* Get the FW semaphore. */
	for (i = 0; i < timeout; i++) {
		swsm = IGC_READ_REG(hw, IGC_SWSM);
		IGC_WRITE_REG(hw, IGC_SWSM, swsm | IGC_SWSM_SWESMBI);

		/* Semaphore acquired if bit latched */
		if (IGC_READ_REG(hw, IGC_SWSM) & IGC_SWSM_SWESMBI)
			break;

		usec_delay(50);
	}

	if (i == timeout) {
		/* Release semaphores */
		igc_put_hw_semaphore_generic(hw);
		DEBUGOUT("Driver can't access the NVM\n");
		return -IGC_ERR_NVM;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_put_hw_semaphore_generic - Release hardware semaphore
 *  @hw: pointer to the HW structure
 *
 *  Release hardware semaphore used to access the PHY or NVM
 **/
void igc_put_hw_semaphore_generic(struct igc_hw *hw)
{
	u32 swsm;

	DEBUGFUNC("igc_put_hw_semaphore_generic");

	swsm = IGC_READ_REG(hw, IGC_SWSM);

	swsm &= ~(IGC_SWSM_SMBI | IGC_SWSM_SWESMBI);

	IGC_WRITE_REG(hw, IGC_SWSM, swsm);
}

/**
 *  igc_get_auto_rd_done_generic - Check for auto read completion
 *  @hw: pointer to the HW structure
 *
 *  Check EEPROM for Auto Read done bit.
 **/
s32 igc_get_auto_rd_done_generic(struct igc_hw *hw)
{
	s32 i = 0;

	DEBUGFUNC("igc_get_auto_rd_done_generic");

	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (IGC_READ_REG(hw, IGC_EECD) & IGC_EECD_AUTO_RD)
			break;
		msec_delay(1);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		DEBUGOUT("Auto read by HW from NVM has not completed.\n");
		return -IGC_ERR_RESET;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_valid_led_default_generic - Verify a valid default LED config
 *  @hw: pointer to the HW structure
 *  @data: pointer to the NVM (EEPROM)
 *
 *  Read the EEPROM for the current default LED configuration.  If the
 *  LED configuration is not valid, set to a valid LED configuration.
 **/
s32 igc_valid_led_default_generic(struct igc_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("igc_valid_led_default_generic");

	ret_val = hw->nvm.ops.read(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT;

	return IGC_SUCCESS;
}

/**
 *  igc_id_led_init_generic -
 *  @hw: pointer to the HW structure
 *
 **/
s32 igc_id_led_init_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = IGC_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = IGC_LEDCTL_MODE_LED_OFF;
	u16 data, i, temp;
	const u16 led_mask = 0x0F;

	DEBUGFUNC("igc_id_led_init_generic");

	ret_val = hw->nvm.ops.valid_led_default(hw, &data);
	if (ret_val)
		return ret_val;

	mac->ledctl_default = IGC_READ_REG(hw, IGC_LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
	}

	return IGC_SUCCESS;
}

/**
 *  igc_setup_led_generic - Configures SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This prepares the SW controllable LED for use and saves the current state
 *  of the LED so it can be later restored.
 **/
s32 igc_setup_led_generic(struct igc_hw *hw)
{
	u32 ledctl;

	DEBUGFUNC("igc_setup_led_generic");

	if (hw->mac.ops.setup_led != igc_setup_led_generic)
		return -IGC_ERR_CONFIG;

	if (hw->phy.media_type == igc_media_type_fiber) {
		ledctl = IGC_READ_REG(hw, IGC_LEDCTL);
		hw->mac.ledctl_default = ledctl;
		/* Turn off LED0 */
		ledctl &= ~(IGC_LEDCTL_LED0_IVRT | IGC_LEDCTL_LED0_BLINK |
			    IGC_LEDCTL_LED0_MODE_MASK);
		ledctl |= (IGC_LEDCTL_MODE_LED_OFF <<
			   IGC_LEDCTL_LED0_MODE_SHIFT);
		IGC_WRITE_REG(hw, IGC_LEDCTL, ledctl);
	} else if (hw->phy.media_type == igc_media_type_copper) {
		IGC_WRITE_REG(hw, IGC_LEDCTL, hw->mac.ledctl_mode1);
	}

	return IGC_SUCCESS;
}

/**
 *  igc_cleanup_led_generic - Set LED config to default operation
 *  @hw: pointer to the HW structure
 *
 *  Remove the current LED configuration and set the LED configuration
 *  to the default value, saved from the EEPROM.
 **/
s32 igc_cleanup_led_generic(struct igc_hw *hw)
{
	DEBUGFUNC("igc_cleanup_led_generic");

	IGC_WRITE_REG(hw, IGC_LEDCTL, hw->mac.ledctl_default);
	return IGC_SUCCESS;
}

/**
 *  igc_blink_led_generic - Blink LED
 *  @hw: pointer to the HW structure
 *
 *  Blink the LEDs which are set to be on.
 **/
s32 igc_blink_led_generic(struct igc_hw *hw)
{
	u32 ledctl_blink = 0;
	u32 i;

	DEBUGFUNC("igc_blink_led_generic");

	if (hw->phy.media_type == igc_media_type_fiber) {
		/* always blink LED0 for PCI-E fiber */
		ledctl_blink = IGC_LEDCTL_LED0_BLINK |
		     (IGC_LEDCTL_MODE_LED_ON << IGC_LEDCTL_LED0_MODE_SHIFT);
	} else {
		/* Set the blink bit for each LED that's "on" (0x0E)
		 * (or "off" if inverted) in ledctl_mode2.  The blink
		 * logic in hardware only works when mode is set to "on"
		 * so it must be changed accordingly when the mode is
		 * "off" and inverted.
		 */
		ledctl_blink = hw->mac.ledctl_mode2;
		for (i = 0; i < 32; i += 8) {
			u32 mode = (hw->mac.ledctl_mode2 >> i) &
			    IGC_LEDCTL_LED0_MODE_MASK;
			u32 led_default = hw->mac.ledctl_default >> i;

			if ((!(led_default & IGC_LEDCTL_LED0_IVRT) &&
			     mode == IGC_LEDCTL_MODE_LED_ON) ||
			    ((led_default & IGC_LEDCTL_LED0_IVRT) &&
			     mode == IGC_LEDCTL_MODE_LED_OFF)) {
				ledctl_blink &=
				    ~(IGC_LEDCTL_LED0_MODE_MASK << i);
				ledctl_blink |= (IGC_LEDCTL_LED0_BLINK |
						 IGC_LEDCTL_MODE_LED_ON) << i;
			}
		}
	}

	IGC_WRITE_REG(hw, IGC_LEDCTL, ledctl_blink);

	return IGC_SUCCESS;
}

/**
 *  igc_led_on_generic - Turn LED on
 *  @hw: pointer to the HW structure
 *
 *  Turn LED on.
 **/
s32 igc_led_on_generic(struct igc_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("igc_led_on_generic");

	switch (hw->phy.media_type) {
	case igc_media_type_fiber:
		ctrl = IGC_READ_REG(hw, IGC_CTRL);
		ctrl &= ~IGC_CTRL_SWDPIN0;
		ctrl |= IGC_CTRL_SWDPIO0;
		IGC_WRITE_REG(hw, IGC_CTRL, ctrl);
		break;
	case igc_media_type_copper:
		IGC_WRITE_REG(hw, IGC_LEDCTL, hw->mac.ledctl_mode2);
		break;
	default:
		break;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_led_off_generic - Turn LED off
 *  @hw: pointer to the HW structure
 *
 *  Turn LED off.
 **/
s32 igc_led_off_generic(struct igc_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("igc_led_off_generic");

	switch (hw->phy.media_type) {
	case igc_media_type_fiber:
		ctrl = IGC_READ_REG(hw, IGC_CTRL);
		ctrl |= IGC_CTRL_SWDPIN0;
		ctrl |= IGC_CTRL_SWDPIO0;
		IGC_WRITE_REG(hw, IGC_CTRL, ctrl);
		break;
	case igc_media_type_copper:
		IGC_WRITE_REG(hw, IGC_LEDCTL, hw->mac.ledctl_mode1);
		break;
	default:
		break;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_set_pcie_no_snoop_generic - Set PCI-express capabilities
 *  @hw: pointer to the HW structure
 *  @no_snoop: bitmap of snoop events
 *
 *  Set the PCI-express register to snoop for events enabled in 'no_snoop'.
 **/
void igc_set_pcie_no_snoop_generic(struct igc_hw *hw, u32 no_snoop)
{
	u32 gcr;

	DEBUGFUNC("igc_set_pcie_no_snoop_generic");

	if (hw->bus.type != igc_bus_type_pci_express)
		return;

	if (no_snoop) {
		gcr = IGC_READ_REG(hw, IGC_GCR);
		gcr &= ~(PCIE_NO_SNOOP_ALL);
		gcr |= no_snoop;
		IGC_WRITE_REG(hw, IGC_GCR, gcr);
	}
}

/**
 *  igc_disable_pcie_master_generic - Disables PCI-express master access
 *  @hw: pointer to the HW structure
 *
 *  Returns IGC_SUCCESS if successful, else returns -10
 *  (-IGC_ERR_MASTER_REQUESTS_PENDING) if master disable bit has not caused
 *  the master requests to be disabled.
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests.
 **/
s32 igc_disable_pcie_master_generic(struct igc_hw *hw)
{
	u32 ctrl;
	s32 timeout = MASTER_DISABLE_TIMEOUT;

	DEBUGFUNC("igc_disable_pcie_master_generic");

	ctrl = IGC_READ_REG(hw, IGC_CTRL);
	ctrl |= IGC_CTRL_GIO_MASTER_DISABLE;
	IGC_WRITE_REG(hw, IGC_CTRL, ctrl);

	while (timeout) {
		if (!(IGC_READ_REG(hw, IGC_STATUS) &
		      IGC_STATUS_GIO_MASTER_ENABLE) ||
				IGC_REMOVED(hw->hw_addr))
			break;
		usec_delay(100);
		timeout--;
	}

	if (!timeout) {
		DEBUGOUT("Master requests are pending.\n");
		return -IGC_ERR_MASTER_REQUESTS_PENDING;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_reset_adaptive_generic - Reset Adaptive Interframe Spacing
 *  @hw: pointer to the HW structure
 *
 *  Reset the Adaptive Interframe Spacing throttle to default values.
 **/
void igc_reset_adaptive_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;

	DEBUGFUNC("igc_reset_adaptive_generic");

	if (!mac->adaptive_ifs) {
		DEBUGOUT("Not in Adaptive IFS mode!\n");
		return;
	}

	mac->current_ifs_val = 0;
	mac->ifs_min_val = IFS_MIN;
	mac->ifs_max_val = IFS_MAX;
	mac->ifs_step_size = IFS_STEP;
	mac->ifs_ratio = IFS_RATIO;

	mac->in_ifs_mode = false;
	IGC_WRITE_REG(hw, IGC_AIT, 0);
}

/**
 *  igc_update_adaptive_generic - Update Adaptive Interframe Spacing
 *  @hw: pointer to the HW structure
 *
 *  Update the Adaptive Interframe Spacing Throttle value based on the
 *  time between transmitted packets and time between collisions.
 **/
void igc_update_adaptive_generic(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;

	DEBUGFUNC("igc_update_adaptive_generic");

	if (!mac->adaptive_ifs) {
		DEBUGOUT("Not in Adaptive IFS mode!\n");
		return;
	}

	if ((mac->collision_delta * mac->ifs_ratio) > mac->tx_packet_delta) {
		if (mac->tx_packet_delta > MIN_NUM_XMITS) {
			mac->in_ifs_mode = true;
			if (mac->current_ifs_val < mac->ifs_max_val) {
				if (!mac->current_ifs_val)
					mac->current_ifs_val = mac->ifs_min_val;
				else
					mac->current_ifs_val +=
						mac->ifs_step_size;
				IGC_WRITE_REG(hw, IGC_AIT,
						mac->current_ifs_val);
			}
		}
	} else {
		if (mac->in_ifs_mode &&
		    mac->tx_packet_delta <= MIN_NUM_XMITS) {
			mac->current_ifs_val = 0;
			mac->in_ifs_mode = false;
			IGC_WRITE_REG(hw, IGC_AIT, 0);
		}
	}
}

/**
 *  igc_validate_mdi_setting_generic - Verify MDI/MDIx settings
 *  @hw: pointer to the HW structure
 *
 *  Verify that when not using auto-negotiation that MDI/MDIx is correctly
 *  set, which is forced to MDI mode only.
 **/
static s32 igc_validate_mdi_setting_generic(struct igc_hw *hw)
{
	DEBUGFUNC("igc_validate_mdi_setting_generic");

	if (!hw->mac.autoneg && (hw->phy.mdix == 0 || hw->phy.mdix == 3)) {
		DEBUGOUT("Invalid MDI setting detected\n");
		hw->phy.mdix = 1;
		return -IGC_ERR_CONFIG;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_validate_mdi_setting_crossover_generic - Verify MDI/MDIx settings
 *  @hw: pointer to the HW structure
 *
 *  Validate the MDI/MDIx setting, allowing for auto-crossover during forced
 *  operation.
 **/
s32
igc_validate_mdi_setting_crossover_generic(struct igc_hw IGC_UNUSEDARG * hw)
{
	DEBUGFUNC("igc_validate_mdi_setting_crossover_generic");
	UNREFERENCED_1PARAMETER(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_write_8bit_ctrl_reg_generic - Write a 8bit CTRL register
 *  @hw: pointer to the HW structure
 *  @reg: 32bit register offset such as IGC_SCTL
 *  @offset: register offset to write to
 *  @data: data to write at register offset
 *
 *  Writes an address/data control type register.  There are several of these
 *  and they all have the format address << 8 | data and bit 31 is polled for
 *  completion.
 **/
s32 igc_write_8bit_ctrl_reg_generic(struct igc_hw *hw, u32 reg,
				      u32 offset, u8 data)
{
	u32 i, regvalue = 0;

	DEBUGFUNC("igc_write_8bit_ctrl_reg_generic");

	/* Set up the address and data */
	regvalue = ((u32)data) | (offset << IGC_GEN_CTL_ADDRESS_SHIFT);
	IGC_WRITE_REG(hw, reg, regvalue);

	/* Poll the ready bit to see if the MDI read completed */
	for (i = 0; i < IGC_GEN_POLL_TIMEOUT; i++) {
		usec_delay(5);
		regvalue = IGC_READ_REG(hw, reg);
		if (regvalue & IGC_GEN_CTL_READY)
			break;
	}
	if (!(regvalue & IGC_GEN_CTL_READY)) {
		DEBUGOUT1("Reg %08x did not indicate ready\n", reg);
		return -IGC_ERR_PHY;
	}

	return IGC_SUCCESS;
}
