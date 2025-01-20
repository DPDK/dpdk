/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_vf.h"

/**
 *  ngbe_init_ops_vf - Initialize the pointers for vf
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers, adapter-specific functions can
 *  override the assignment of generic function pointers by assigning
 *  their own adapter-specific function pointers.
 *  Does not touch the hardware.
 **/
s32 ngbe_init_ops_vf(struct ngbe_hw *hw)
{
	struct ngbe_mac_info *mac = &hw->mac;

	mac->max_tx_queues = 1;
	mac->max_rx_queues = 1;

	return 0;
}
