/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013 - 2015 Intel Corporation
 */

#ifndef _FM10K_COMMON_H_
#define _FM10K_COMMON_H_

#include "fm10k_type.h"

u16 fm10k_get_pcie_msix_count_generic(struct fm10k_hw *hw);
s32 fm10k_init_ops_generic(struct fm10k_hw *hw);
s32 fm10k_disable_queues_generic(struct fm10k_hw *hw, u16 q_cnt);
s32 fm10k_start_hw_generic(struct fm10k_hw *hw);
s32 fm10k_stop_hw_generic(struct fm10k_hw *hw);
u32 fm10k_read_hw_stats_32b(struct fm10k_hw *hw, u32 addr,
			    struct fm10k_hw_stat *stat);
#define fm10k_update_hw_base_32b(stat, delta) ((stat)->base_l += (delta))
void fm10k_update_hw_stats_q(struct fm10k_hw *hw, struct fm10k_hw_stats_q *q,
			     u32 idx, u32 count);
#define fm10k_unbind_hw_stats_32b(s) ((s)->base_h = 0)
void fm10k_unbind_hw_stats_q(struct fm10k_hw_stats_q *q, u32 idx, u32 count);
s32 fm10k_get_host_state_generic(struct fm10k_hw *hw, bool *host_ready);
#endif /* _FM10K_COMMON_H_ */
