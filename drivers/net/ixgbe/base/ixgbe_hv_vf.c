/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "ixgbe_vf.h"
#include "ixgbe_hv_vf.h"

/**
 * Hyper-V variant - just a stub.
 * @hw: unused
 * @mc_addr_list: unused
 * @mc_addr_count: unused
 * @next: unused
 * @clear: unused
 */
static s32 ixgbevf_hv_update_mc_addr_list_vf(struct ixgbe_hw *hw, u8 *mc_addr_list,
				 u32 mc_addr_count, ixgbe_mc_addr_itr next,
				 bool clear)
{
	UNREFERENCED_5PARAMETER(hw, mc_addr_list, mc_addr_count, next, clear);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

/**
 * Hyper-V variant - just a stub.
 * @hw: unused
 * @xcast_mode: unused
 */
static s32 ixgbevf_hv_update_xcast_mode(struct ixgbe_hw *hw, int xcast_mode)
{
	UNREFERENCED_2PARAMETER(hw, xcast_mode);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

/**
 * Hyper-V variant - just a stub.
 * @hw: unused
 * @vlan: unused
 * @vind: unused
 * @vlan_on: unused
 * @vlvf_bypass: unused
 */
static s32 ixgbevf_hv_set_vfta_vf(struct ixgbe_hw *hw, u32 vlan, u32 vind,
				  bool vlan_on, bool vlvf_bypass)
{
	UNREFERENCED_5PARAMETER(hw, vlan, vind, vlan_on, vlvf_bypass);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

static s32 ixgbevf_hv_set_uc_addr_vf(struct ixgbe_hw *hw, u32 index, u8 *addr)
{
	UNREFERENCED_3PARAMETER(hw, index, addr);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

/**
 * Hyper-V variant - just a stub.
 */
static s32 ixgbevf_hv_reset_hw_vf(struct ixgbe_hw *hw)
{
	UNREFERENCED_PARAMETER(hw);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

/**
 * Hyper-V variant - just a stub.
 */
static s32 ixgbevf_hv_set_rar_vf(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vlan, u32 vind)
{
	UNREFERENCED_5PARAMETER(hw, index, addr, vlan, vind);

	return IXGBE_ERR_FEATURE_NOT_SUPPORTED;
}

/**
 * Hyper-V variant; there is no mailbox communication.
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: true is link is up, false otherwise
 * @autoneg_wait_to_complete: unused
 *
 */
static s32 ixgbevf_hv_check_mac_link_vf(struct ixgbe_hw *hw,
					ixgbe_link_speed *speed,
					bool *link_up,
					bool autoneg_wait_to_complete)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	struct ixgbe_mac_info *mac = &hw->mac;
	u32 links_reg;
	UNREFERENCED_1PARAMETER(autoneg_wait_to_complete);

	/* If we were hit with a reset drop the link */
	if (!mbx->ops.check_for_rst(hw, 0) || !mbx->timeout)
		mac->get_link_status = true;

	if (!mac->get_link_status)
		goto out;

	/* if link status is down no point in checking to see if pf is up */
	links_reg = IXGBE_READ_REG(hw, IXGBE_VFLINKS);
	if (!(links_reg & IXGBE_LINKS_UP))
		goto out;

	/* for SFP+ modules and DA cables on 82599 it can take up to 500usecs
	 * before the link status is correct
	 */
	if (mac->type == ixgbe_mac_82599_vf) {
		int i;

		for (i = 0; i < 5; i++) {
			DELAY(100);
			links_reg = IXGBE_READ_REG(hw, IXGBE_VFLINKS);

			if (!(links_reg & IXGBE_LINKS_UP))
				goto out;
		}
	}

	switch (links_reg & IXGBE_LINKS_SPEED_82599) {
	case IXGBE_LINKS_SPEED_10G_82599:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		if (hw->mac.type >= ixgbe_mac_X550) {
			if (links_reg & IXGBE_LINKS_SPEED_NON_STD)
				*speed = IXGBE_LINK_SPEED_2_5GB_FULL;
		}
		break;
	case IXGBE_LINKS_SPEED_1G_82599:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		break;
	case IXGBE_LINKS_SPEED_100_82599:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		if (hw->mac.type == ixgbe_mac_X550) {
			if (links_reg & IXGBE_LINKS_SPEED_NON_STD)
				*speed = IXGBE_LINK_SPEED_5GB_FULL;
		}
		break;
	case IXGBE_LINKS_SPEED_10_X550EM_A:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		/* Reserved for pre-x550 devices */
		if (hw->mac.type >= ixgbe_mac_X550)
			*speed = IXGBE_LINK_SPEED_10_FULL;
		break;
	default:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
	}

	/* if we passed all the tests above then the link is up and we no
	 * longer need to check for link
	 */
	mac->get_link_status = false;

out:
	*link_up = !mac->get_link_status;
	return IXGBE_SUCCESS;
}

/**
 * ixgbevf_hv_set_rlpml_vf - Set the maximum receive packet length
 * @hw: pointer to the HW structure
 * @max_size: value to assign to max frame size
 * Hyper-V variant.
 **/
static s32 ixgbevf_hv_set_rlpml_vf(struct ixgbe_hw *hw, u16 max_size)
{
	u32 reg;

	/* If we are on Hyper-V, we implement this functionality
	 * differently.
	 */
	reg =  IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(0));
	/* CRC == 4 */
	reg |= ((max_size + 4) | IXGBE_RXDCTL_RLPML_EN);
	IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(0), reg);

	return IXGBE_SUCCESS;
}

/**
 * ixgbevf_hv_negotiate_api_version_vf - Negotiate supported API version
 * @hw: pointer to the HW structure
 * @api: integer containing requested API version
 * Hyper-V version - only ixgbe_mbox_api_10 supported.
 **/
static int ixgbevf_hv_negotiate_api_version_vf(struct ixgbe_hw *hw, int api)
{
	UNREFERENCED_1PARAMETER(hw);

	/* Hyper-V only supports api version ixgbe_mbox_api_10 */
	if (api != ixgbe_mbox_api_10)
		return IXGBE_ERR_INVALID_ARGUMENT;

	return IXGBE_SUCCESS;
}

/**
 * ixgbevf_hv_init_ops_vf - Initialize the pointers for vf
 * @hw: pointer to hardware structure
 *
 * This will assign function pointers, adapter-specific functions can
 * override the assignment of generic function pointers by assigning
 * their own adapter-specific function pointers.
 * Does not touch the hardware.
 **/
s32 ixgbevf_hv_init_ops_vf(struct ixgbe_hw *hw)
{
	/* Set defaults for VF then override applicable Hyper-V
	 * specific functions
	 */
	ixgbe_init_ops_vf(hw);

	hw->mac.ops.reset_hw = ixgbevf_hv_reset_hw_vf;
	hw->mac.ops.check_link = ixgbevf_hv_check_mac_link_vf;
	hw->mac.ops.negotiate_api_version = ixgbevf_hv_negotiate_api_version_vf;
	hw->mac.ops.set_rar = ixgbevf_hv_set_rar_vf;
	hw->mac.ops.update_mc_addr_list = ixgbevf_hv_update_mc_addr_list_vf;
	hw->mac.ops.update_xcast_mode = ixgbevf_hv_update_xcast_mode;
	hw->mac.ops.set_uc_addr = ixgbevf_hv_set_uc_addr_vf;
	hw->mac.ops.set_vfta = ixgbevf_hv_set_vfta_vf;
	hw->mac.ops.set_rlpml = ixgbevf_hv_set_rlpml_vf;

	return IXGBE_SUCCESS;
}
