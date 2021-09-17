/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_PTYPE_MK_TCAM_TABLE_SIZE 1024

/**
 * ice_ptype_mk_tcam_dump - dump an ptype marker tcam info_
 * @ice_hw: pointer to the hardware structure
 * @item: ptype marker tcam to dump
 */
void ice_ptype_mk_tcam_dump(struct ice_hw *hw,
			    struct ice_ptype_mk_tcam_item *item)
{
	int i;

	ice_info(hw, "address = %d\n", item->address);
	ice_info(hw, "ptype = %d\n", item->ptype);
	ice_info(hw, "key    :");
	for (i = 0; i < 10; i++)
		ice_info(hw, "%02x ", item->key[i]);
	ice_info(hw, "\n");
	ice_info(hw, "key_inv:");
	for (i = 0; i < 10; i++)
		ice_info(hw, "%02x ", item->key_inv[i]);
	ice_info(hw, "\n");
}

static void _parse_ptype_mk_tcam_item(struct ice_hw *hw, u16 idx, void *item,
				      void *data, int size)
{
	ice_parse_item_dflt(hw, idx, item, data, size);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_ptype_mk_tcam_dump(hw,
				       (struct ice_ptype_mk_tcam_item *)item);
}

/**
 * ice_ptype_mk_tcam_table_get - create a ptype marker tcam table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_ptype_mk_tcam_item *ice_ptype_mk_tcam_table_get(struct ice_hw *hw)
{
	return (struct ice_ptype_mk_tcam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_MARKER_PTYPE,
					sizeof(struct ice_ptype_mk_tcam_item),
					ICE_PTYPE_MK_TCAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_parse_ptype_mk_tcam_item, true);
}

/**
 * ice_ptype_mk_tcam_match - match a pattern on a ptype marker tcam table
 * @table: ptype marker tcam table to search
 * @pat: pattern to match
 * @len: length of the pattern
 */
struct ice_ptype_mk_tcam_item *
ice_ptype_mk_tcam_match(struct ice_ptype_mk_tcam_item *table,
			u8 *pat, int len)
{
	int i;

	for (i = 0; i < ICE_PTYPE_MK_TCAM_TABLE_SIZE; i++) {
		struct ice_ptype_mk_tcam_item *item = &table[i];

		if (ice_ternary_match(item->key, item->key_inv, pat, len))
			return item;
	}

	return NULL;
}
