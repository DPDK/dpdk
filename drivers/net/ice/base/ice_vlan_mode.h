/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _ICE_VLAN_MODE_H_
#define _ICE_VLAN_MODE_H_

struct ice_hw;

enum ice_status ice_set_vlan_mode(struct ice_hw *hw);
void ice_init_vlan_mode_ops(struct ice_hw *hw);

/* This structure defines the VLAN mode configuration interface. It is used to set the VLAN mode.
 *
 * Note: These operations will be called while the global configuration lock is held.
 *
 * enum ice_status (*set_svm)(struct ice_hw *hw);
 *	This function is called when the DDP and/or Firmware don't support double VLAN mode (DVM) or
 *	if the set_dvm op is not implemented and/or returns failure. It will set the device in
 *	single VLAN mode (SVM).
 *
 * enum ice_status (*set_dvm)(struct ice_hw *hw);
 *	This function is called when the DDP and Firmware support double VLAN mode (DVM). It should
 *	be implemented to set double VLAN mode. If it fails or remains unimplemented, set_svm will
 *	be called as a fallback plan.
 */
struct ice_vlan_mode_ops {
	enum ice_status (*set_svm)(struct ice_hw *hw);
	enum ice_status (*set_dvm)(struct ice_hw *hw);
};

#endif /* _ICE_VLAN_MODE_H */
