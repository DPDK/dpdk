/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_AML40_H_
#define _TXGBE_AML40_H_

#include "txgbe_type.h"

void txgbe_init_ops_aml40(struct txgbe_hw *hw);
s32 txgbe_check_mac_link_aml40(struct txgbe_hw *hw,
			       u32 *speed, bool *link_up, bool link_up_wait_to_complete);
s32 txgbe_get_link_capabilities_aml40(struct txgbe_hw *hw,
				      u32 *speed, bool *autoneg);
u32 txgbe_get_media_type_aml40(struct txgbe_hw *hw);
s32 txgbe_setup_mac_link_aml40(struct txgbe_hw *hw, u32 speed,
			       bool autoneg_wait_to_complete);
void txgbe_init_mac_link_ops_aml40(struct txgbe_hw *hw);
#endif /* _TXGBE_AML40_H_ */
