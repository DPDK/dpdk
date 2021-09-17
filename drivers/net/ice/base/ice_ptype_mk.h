/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_PTYPE_MK_H_
#define _ICE_PTYPE_MK_H_

struct ice_ptype_mk_tcam_item {
	u16 address;
	u16 ptype;
	u8 key[10];
	u8 key_inv[10];
};

void ice_ptype_mk_tcam_dump(struct ice_hw *hw,
			    struct ice_ptype_mk_tcam_item *item);
struct ice_ptype_mk_tcam_item *ice_ptype_mk_tcam_table_get(struct ice_hw *hw);
struct ice_ptype_mk_tcam_item *
ice_ptype_mk_tcam_match(struct ice_ptype_mk_tcam_item *table,
			u8 *pat, int len);
#endif /* _ICE_PTYPE_MK_H_ */
