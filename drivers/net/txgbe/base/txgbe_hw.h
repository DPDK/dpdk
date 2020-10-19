/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_HW_H_
#define _TXGBE_HW_H_

#include "txgbe_type.h"

s32 txgbe_init_hw(struct txgbe_hw *hw);

void txgbe_set_lan_id_multi_port(struct txgbe_hw *hw);

s32 txgbe_validate_mac_addr(u8 *mac_addr);
void txgbe_clear_tx_pending(struct txgbe_hw *hw);
s32 txgbe_init_shared_code(struct txgbe_hw *hw);
s32 txgbe_set_mac_type(struct txgbe_hw *hw);
s32 txgbe_init_ops_pf(struct txgbe_hw *hw);
s32 txgbe_reset_hw(struct txgbe_hw *hw);
#endif /* _TXGBE_HW_H_ */
