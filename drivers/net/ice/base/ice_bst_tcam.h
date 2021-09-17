/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_BST_TCAM_H_
#define _ICE_BST_TCAM_H_

#include "ice_imem.h"

struct ice_bst_tcam_item {
	u16 address;
	u8 key[20];
	u8 key_inv[20];
	u8 hit_idx_grp;
	u8 pg_pri;
	struct ice_np_keybuilder np_kb;
	struct ice_pg_keybuilder pg_kb;
	struct ice_alu alu0;
	struct ice_alu alu1;
	struct ice_alu alu2;
};

void ice_bst_tcam_dump(struct ice_hw *hw, struct ice_bst_tcam_item *item);

struct ice_bst_tcam_item *ice_bst_tcam_table_get(struct ice_hw *hw);

struct ice_lbl_item *ice_bst_lbl_table_get(struct ice_hw *hw);
#endif /*_ICE_BST_TCAM_H_ */
