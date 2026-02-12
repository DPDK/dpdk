/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_FLG_RD_TABLE_SIZE 64

/**
 * ice_flg_rd_dump - dump a flag redirect item info
 * @hw: pointer to the hardware structure
 * @item: flag redirect item to dump
 */
void ice_flg_rd_dump(struct ice_hw *hw, struct ice_flg_rd_item *item)
{
	ice_info(hw, "index = %d\n", item->idx);
	ice_info(hw, "expose = %d\n", item->expose);
	ice_info(hw, "intr_flg_id = %d\n", item->intr_flg_id);
}

/** The function parses a 8 bits Flag Redirect Table entry with below format:
 *  BIT 0:	Expose (rdi->expose)
 *  BIT 1-6:	Internal Flag ID (rdi->intr_flg_id)
 *  BIT 7:	reserved
 */
static void _flg_rd_parse_item(struct ice_hw *hw, u16 idx, void *item,
			       void *data, int size)
{
	struct ice_flg_rd_item *rdi = (struct ice_flg_rd_item *)item;
	u8 d8 = *(u8 *)data;

	rdi->idx = idx;
	rdi->expose = (d8 & 0x1) != 0;
	rdi->intr_flg_id = (u8)((d8 >> 1) & 0x3f);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_flg_rd_dump(hw, rdi);
}

/**
 * ice_flg_rd_table_get - create a flag redirect table
 * @hw: pointer to the hardware structure
 */
struct ice_flg_rd_item *ice_flg_rd_table_get(struct ice_hw *hw)
{
	return (struct ice_flg_rd_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_FLAG_REDIR,
					sizeof(struct ice_flg_rd_item),
					ICE_FLG_RD_TABLE_SIZE,
					ice_parser_sect_item_get,
					_flg_rd_parse_item, false);
}

/**
 * ice_flg_redirect - redirect a parser flag to packet flag
 * @table: flag redirect table
 * @psr_flg: parser flag to redirect
 */
u64 ice_flg_redirect(struct ice_flg_rd_item *table, u64 psr_flg)
{
	u64 flg = 0;
	int i;

	for (i = 0; i < 64; i++) {
		struct ice_flg_rd_item *item = &table[i];

		if (!item->expose)
			continue;

		if (psr_flg & (1ul << item->intr_flg_id))
			flg |= (1ul << i);
	}

	return flg;
}
