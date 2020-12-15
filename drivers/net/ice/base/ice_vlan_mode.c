/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "ice_vlan_mode.h"
#include "ice_common.h"

/**
 * ice_set_svm - set single VLAN mode
 * @hw: pointer to the HW structure
 */
static enum ice_status ice_set_svm_dflt(struct ice_hw *hw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	return ice_aq_set_port_params(hw->port_info, 0, false, false, false, NULL);
}

/**
 * ice_init_vlan_mode_ops - initialize VLAN mode configuration ops
 * @hw: pointer to the HW structure
 */
void ice_init_vlan_mode_ops(struct ice_hw *hw)
{
	hw->vlan_mode_ops.set_dvm = NULL;
	hw->vlan_mode_ops.set_svm = ice_set_svm_dflt;
}

/**
 * ice_set_vlan_mode
 * @hw: pointer to the HW structure
 */
enum ice_status ice_set_vlan_mode(struct ice_hw *hw)
{
	enum ice_status status = ICE_ERR_NOT_IMPL;

	if (hw->vlan_mode_ops.set_dvm)
		status = hw->vlan_mode_ops.set_dvm(hw);

	if (status)
		return hw->vlan_mode_ops.set_svm(hw);

	return ICE_SUCCESS;
}
