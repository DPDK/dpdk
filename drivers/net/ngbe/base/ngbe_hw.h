/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_HW_H_
#define _NGBE_HW_H_

#include "ngbe_type.h"

#define NGBE_EM_MAX_TX_QUEUES 8
#define NGBE_EM_MAX_RX_QUEUES 8

s32 ngbe_init_hw(struct ngbe_hw *hw);
s32 ngbe_reset_hw_em(struct ngbe_hw *hw);
s32 ngbe_stop_hw(struct ngbe_hw *hw);

void ngbe_set_lan_id_multi_port(struct ngbe_hw *hw);

s32 ngbe_acquire_swfw_sync(struct ngbe_hw *hw, u32 mask);
void ngbe_release_swfw_sync(struct ngbe_hw *hw, u32 mask);

s32 ngbe_init_thermal_sensor_thresh(struct ngbe_hw *hw);
s32 ngbe_mac_check_overtemp(struct ngbe_hw *hw);
void ngbe_disable_rx(struct ngbe_hw *hw);
s32 ngbe_init_shared_code(struct ngbe_hw *hw);
s32 ngbe_set_mac_type(struct ngbe_hw *hw);
s32 ngbe_init_ops_pf(struct ngbe_hw *hw);
s32 ngbe_init_phy(struct ngbe_hw *hw);
void ngbe_map_device_id(struct ngbe_hw *hw);

#endif /* _NGBE_HW_H_ */
