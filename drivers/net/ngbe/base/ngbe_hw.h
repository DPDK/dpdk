/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_HW_H_
#define _NGBE_HW_H_

#include "ngbe_type.h"

void ngbe_set_lan_id_multi_port(struct ngbe_hw *hw);

s32 ngbe_acquire_swfw_sync(struct ngbe_hw *hw, u32 mask);
void ngbe_release_swfw_sync(struct ngbe_hw *hw, u32 mask);

s32 ngbe_init_shared_code(struct ngbe_hw *hw);
s32 ngbe_set_mac_type(struct ngbe_hw *hw);
s32 ngbe_init_ops_pf(struct ngbe_hw *hw);
void ngbe_map_device_id(struct ngbe_hw *hw);

#endif /* _NGBE_HW_H_ */
