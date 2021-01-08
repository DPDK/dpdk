/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_vlan_mode.h"
#include "ice_common.h"

/**
 * ice_pkg_supports_dvm - determine if DDP supports Double VLAN mode (DVM)
 * @hw: pointer to the HW struct
 * @dvm: output variable to determine if DDP supports DVM(true) or SVM(false)
 */
static enum ice_status
ice_pkg_get_supported_vlan_mode(struct ice_hw *hw, bool *dvm)
{
	u16 meta_init_size = sizeof(struct ice_meta_init_section);
	struct ice_meta_init_section *sect;
	struct ice_buf_build *bld;
	enum ice_status status;

	/* if anything fails, we assume there is no DVM support */
	*dvm = false;

	bld = ice_pkg_buf_alloc_single_section(hw,
					       ICE_SID_RXPARSER_METADATA_INIT,
					       meta_init_size, (void **)&sect);
	if (!bld)
		return ICE_ERR_NO_MEMORY;

	/* only need to read a single section */
	sect->count = CPU_TO_LE16(1);
	sect->offset = CPU_TO_LE16(ICE_META_VLAN_MODE_ENTRY);

	status = ice_aq_upload_section(hw,
				       (struct ice_buf_hdr *)ice_pkg_buf(bld),
				       ICE_PKG_BUF_SIZE, NULL);
	if (!status) {
		ice_declare_bitmap(entry, ICE_META_INIT_BITS);
		u32 arr[ICE_META_INIT_DW_CNT];
		u16 i;

		/* convert to host bitmap format */
		for (i = 0; i < ICE_META_INIT_DW_CNT; i++)
			arr[i] = LE32_TO_CPU(sect->entry[0].bm[i]);

		ice_bitmap_from_array32(entry, arr, (u16)ICE_META_INIT_BITS);

		/* check if DVM is supported */
		*dvm = ice_is_bit_set(entry, ICE_META_VLAN_MODE_BIT);
	}

	ice_pkg_buf_free(hw, bld);

	return status;
}

/**
 * ice_is_dvm_supported - check if double VLAN mode is supported based on DDP
 * @hw: pointer to the hardware structure
 *
 * Returns true if DVM is supported and false if only SVM is supported. This
 * function should only be called while the global config lock is held and after
 * the package has been successfully downloaded.
 */
static bool ice_is_dvm_supported(struct ice_hw *hw)
{
	enum ice_status status;
	bool pkg_supports_dvm;

	status = ice_pkg_get_supported_vlan_mode(hw, &pkg_supports_dvm);
	if (status) {
		ice_debug(hw, ICE_DBG_PKG, "Failed to get supported VLAN mode, err %d\n",
			  status);
		return false;
	}

	if (!pkg_supports_dvm)
		return false;

	return true;
}

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

	if (!ice_is_dvm_supported(hw))
		return ICE_SUCCESS;

	if (hw->vlan_mode_ops.set_dvm)
		status = hw->vlan_mode_ops.set_dvm(hw);

	if (status)
		return hw->vlan_mode_ops.set_svm(hw);

	return ICE_SUCCESS;
}
