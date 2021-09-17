/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_XLT_KB_H_
#define _ICE_XLT_KB_H_

#define ICE_XLT_KB_TBL_CNT 8
#define ICE_XLT_KB_FLAG0_14_CNT 15

struct ice_xlt_kb_entry {
	u8 xlt1_ad_sel;
	u8 xlt2_ad_sel;
	u16 flg0_14_sel[ICE_XLT_KB_FLAG0_14_CNT];
	u8 xlt1_md_sel;
	u8 xlt2_md_sel;
};

struct ice_xlt_kb {
	u8 xlt1_pm;
	u8 xlt2_pm;
	u8 prof_id_pm;
	u64 flag15;

	struct ice_xlt_kb_entry entries[ICE_XLT_KB_TBL_CNT];
};

void ice_xlt_kb_dump(struct ice_hw *hw, struct ice_xlt_kb *kb);
struct ice_xlt_kb *ice_xlt_kb_get_sw(struct ice_hw *hw);
struct ice_xlt_kb *ice_xlt_kb_get_acl(struct ice_hw *hw);
struct ice_xlt_kb *ice_xlt_kb_get_fd(struct ice_hw *hw);
struct ice_xlt_kb *ice_xlt_kb_get_rss(struct ice_hw *hw);
u16 ice_xlt_kb_flag_get(struct ice_xlt_kb *kb, u64 pkt_flag);
#endif /* _ICE_XLT_KB_H */
