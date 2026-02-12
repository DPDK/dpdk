/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_METAINIT_H_
#define _ICE_METAINIT_H_

struct ice_metainit_item {
	u16 idx;

	u8 tsr;
	u16 ho;
	u16 pc;
	u16 pg_rn;
	u8 cd;

	bool gpr_a_ctrl;
	u8 gpr_a_data_mdid;
	u8 gpr_a_data_start;
	u8 gpr_a_data_len;
	u8 gpr_a_id;

	bool gpr_b_ctrl;
	u8 gpr_b_data_mdid;
	u8 gpr_b_data_start;
	u8 gpr_b_data_len;
	u8 gpr_b_id;

	bool gpr_c_ctrl;
	u8 gpr_c_data_mdid;
	u8 gpr_c_data_start;
	u8 gpr_c_data_len;
	u8 gpr_c_id;

	bool gpr_d_ctrl;
	u8 gpr_d_data_mdid;
	u8 gpr_d_data_start;
	u8 gpr_d_data_len;
	u8 gpr_d_id;

	u64 flags;
};

void ice_metainit_dump(struct ice_hw *hw, struct ice_metainit_item *item);
struct ice_metainit_item *ice_metainit_table_get(struct ice_hw *hw);
#endif /*_ICE_METAINIT_H_ */
