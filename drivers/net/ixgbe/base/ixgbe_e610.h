/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _IXGBE_E610_H_
#define _IXGBE_E610_H_

#include "ixgbe_type.h"

void ixgbe_init_aci(struct ixgbe_hw *hw);
void ixgbe_shutdown_aci(struct ixgbe_hw *hw);

s32 ixgbe_aci_send_cmd(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
		       void *buf, u16 buf_size);
bool ixgbe_aci_check_event_pending(struct ixgbe_hw *hw);
s32 ixgbe_aci_get_event(struct ixgbe_hw *hw, struct ixgbe_aci_event *e,
			bool *pending);

void ixgbe_fill_dflt_direct_cmd_desc(struct ixgbe_aci_desc *desc, u16 opcode);

s32 ixgbe_acquire_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      enum ixgbe_aci_res_access_type access, u32 timeout);
void ixgbe_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res);
s32 ixgbe_aci_get_internal_data(struct ixgbe_hw *hw, u16 cluster_id,
				u16 table_id, u32 start, void *buf,
				u16 buf_size, u16 *ret_buf_size,
				u16 *ret_next_cluster, u16 *ret_next_table,
				u32 *ret_next_index);

#endif /* _IXGBE_E610_H_ */
