/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_FLG_RD_H_
#define _ICE_FLG_RD_H_

struct ice_flg_rd_item {
	u16 idx;
	bool expose;
	u8 intr_flg_id;
};

void ice_flg_rd_dump(struct ice_hw *hw, struct ice_flg_rd_item *item);
struct ice_flg_rd_item *ice_flg_rd_table_get(struct ice_hw *hw);
u64 ice_flg_redirect(struct ice_flg_rd_item *table, u64 psr_flg);
#endif /* _ICE_FLG_RD_H_ */
