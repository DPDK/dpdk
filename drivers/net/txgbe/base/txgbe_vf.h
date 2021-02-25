/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_VF_H_
#define _TXGBE_VF_H_

#include "txgbe_type.h"

#define TXGBE_VF_MAX_TX_QUEUES	8
#define TXGBE_VF_MAX_RX_QUEUES	8

s32 txgbe_init_ops_vf(struct txgbe_hw *hw);
s32 txgbe_reset_hw_vf(struct txgbe_hw *hw);
s32 txgbe_stop_hw_vf(struct txgbe_hw *hw);
int txgbevf_negotiate_api_version(struct txgbe_hw *hw, int api);
int txgbevf_get_queues(struct txgbe_hw *hw, unsigned int *num_tcs,
		       unsigned int *default_tc);

#endif /* __TXGBE_VF_H__ */
