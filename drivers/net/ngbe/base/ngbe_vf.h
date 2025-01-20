/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_VF_H_
#define _NGBE_VF_H_

#include "ngbe_type.h"

s32 ngbe_init_ops_vf(struct ngbe_hw *hw);
int ngbevf_negotiate_api_version(struct ngbe_hw *hw, int api);

#endif /* __NGBE_VF_H__ */
