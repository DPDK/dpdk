/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_METAINIT_TABLE_SIZE 16

/**
 * ice_metainit_dump - dump an metainit item info
 * @ice_hw: pointer to the hardware structure
 * @item: metainit item to dump
 */
void ice_metainit_dump(struct ice_hw *hw, struct ice_metainit_item *item)
{
	ice_info(hw, "index = %d\n", item->idx);
	ice_info(hw, "tsr = %d\n", item->tsr);
	ice_info(hw, "ho = %d\n", item->ho);
	ice_info(hw, "pc = %d\n", item->pc);
	ice_info(hw, "pg_rn = %d\n", item->pg_rn);
	ice_info(hw, "cd = %d\n", item->cd);
	ice_info(hw, "gpr_a_ctrl = %d\n", item->gpr_a_ctrl);
	ice_info(hw, "gpr_a_data_mdid = %d\n", item->gpr_a_data_mdid);
	ice_info(hw, "gpr_a_data_start = %d\n", item->gpr_a_data_start);
	ice_info(hw, "gpr_a_data_len = %d\n", item->gpr_a_data_len);
	ice_info(hw, "gpr_a_id = %d\n", item->gpr_a_id);
	ice_info(hw, "gpr_b_ctrl = %d\n", item->gpr_b_ctrl);
	ice_info(hw, "gpr_b_data_mdid = %d\n", item->gpr_b_data_mdid);
	ice_info(hw, "gpr_b_data_start = %d\n", item->gpr_b_data_start);
	ice_info(hw, "gpr_b_data_len = %d\n", item->gpr_b_data_len);
	ice_info(hw, "gpr_b_id = %d\n", item->gpr_b_id);
	ice_info(hw, "gpr_c_ctrl = %d\n", item->gpr_c_ctrl);
	ice_info(hw, "gpr_c_data_mdid = %d\n", item->gpr_c_data_mdid);
	ice_info(hw, "gpr_c_data_start = %d\n", item->gpr_c_data_start);
	ice_info(hw, "gpr_c_data_len = %d\n", item->gpr_c_data_len);
	ice_info(hw, "gpr_c_id = %d\n", item->gpr_c_id);
	ice_info(hw, "gpr_d_ctrl = %d\n", item->gpr_d_ctrl);
	ice_info(hw, "gpr_d_data_mdid = %d\n", item->gpr_d_data_mdid);
	ice_info(hw, "gpr_d_data_start = %d\n", item->gpr_d_data_start);
	ice_info(hw, "gpr_d_data_len = %d\n", item->gpr_d_data_len);
	ice_info(hw, "gpr_d_id = %d\n", item->gpr_d_id);
	ice_info(hw, "flags = 0x%016" PRIx64 "\n", item->flags);
}

/** The function parses a 192 bits Metadata Init entry with below format:
 *  BIT 0-7:	TCAM Search Key Register (mi->tsr)
 *  BIT 8-16:	Header Offset (mi->ho)
 *  BIT 17-24:	Program Counter (mi->pc)
 *  BIT 25-35:	Parse Graph Root Node (mi->pg_rn)
 *  BIT 36-38:	Control Domain (mi->cd)
 *  BIT 39:	GPR_A Data Control (mi->gpr_a_ctrl)
 *  BIT 40-44:	GPR_A MDID.ID (mi->gpr_a_data_mdid)
 *  BIT 45-48:	GPR_A MDID.START (mi->gpr_a_data_start)
 *  BIT 49-53:	GPR_A MDID.LEN (mi->gpr_a_data_len)
 *  BIT 54-55:	reserved
 *  BIT 56-59:	GPR_A ID (mi->gpr_a_id)
 *  BIT 60:	GPR_B Data Control (mi->gpr_b_ctrl)
 *  BIT 61-65:	GPR_B MDID.ID (mi->gpr_b_data_mdid)
 *  BIT 66-69:	GPR_B MDID.START (mi->gpr_b_data_start)
 *  BIT 70-74:	GPR_B MDID.LEN (mi->gpr_b_data_len)
 *  BIT 75-76:	reserved
 *  BIT 77-80:	GPR_B ID (mi->gpr_a_id)
 *  BIT 81:	GPR_C Data Control (mi->gpr_c_ctrl)
 *  BIT 82-86:	GPR_C MDID.ID (mi->gpr_c_data_mdid)
 *  BIT 87-90:	GPR_C MDID.START (mi->gpr_c_data_start)
 *  BIT 91-95:	GPR_C MDID.LEN (mi->gpr_c_data_len)
 *  BIT 96-97:	reserved
 *  BIT 98-101:	GPR_C ID (mi->gpr_c_id)
 *  BIT 102:	GPR_D Data Control (mi->gpr_d_ctrl)
 *  BIT 103-107:GPR_D MDID.ID (mi->gpr_d_data_mdid)
 *  BIT 108-111:GPR_D MDID.START (mi->gpr_d_data_start)
 *  BIT 112-116:GPR_D MDID.LEN (mi->gpr_d_data_len)
 *  BIT 117-118:reserved
 *  BIT 119-122:GPR_D ID (mi->gpr_d_id)
 *  BIT 123-186:Flags (mi->flags)
 *  BIT 187-191:rserved
 */
static void _metainit_parse_item(struct ice_hw *hw, u16 idx, void *item,
				 void *data, int size)
{
	struct ice_metainit_item *mi = (struct ice_metainit_item *)item;
	u8 *buf = (u8 *)data;
	u64 d64;

	mi->idx = idx;
	d64 = *(u64 *)buf;

	mi->tsr = (u8)(d64 & 0xff);
	mi->ho = (u16)((d64 >> 8) & 0x1ff);
	mi->pc = (u16)((d64 >> 17) & 0xff);
	mi->pg_rn = (u16)((d64 >> 25) & 0x3ff);
	mi->cd = (u16)((d64 >> 36) & 0x7);
	mi->gpr_a_ctrl = ((d64 >> 39) & 0x1) != 0;
	mi->gpr_a_data_mdid = (u8)((d64 >> 40) & 0x1f);
	mi->gpr_a_data_start = (u8)((d64 >> 45) & 0xf);
	mi->gpr_a_data_len = (u8)((d64 >> 49) & 0x1f);
	mi->gpr_a_id = (u8)((d64 >> 56) & 0xf);

	d64 = *(u64 *)&buf[7] >> 4;
	mi->gpr_b_ctrl = (d64 & 0x1) != 0;
	mi->gpr_b_data_mdid = (u8)((d64 >> 1) & 0x1f);
	mi->gpr_b_data_start = (u8)((d64 >> 6) & 0xf);
	mi->gpr_b_data_len = (u8)((d64 >> 10) & 0x1f);
	mi->gpr_b_id = (u8)((d64 >> 17) & 0xf);

	mi->gpr_c_ctrl = ((d64 >> 21) & 0x1) != 0;
	mi->gpr_c_data_mdid = (u8)((d64 >> 22) & 0x1f);
	mi->gpr_c_data_start = (u8)((d64 >> 27) & 0xf);
	mi->gpr_c_data_len = (u8)((d64 >> 31) & 0x1f);
	mi->gpr_c_id = (u8)((d64 >> 38) & 0xf);

	mi->gpr_d_ctrl = ((d64 >> 42) & 0x1) != 0;
	mi->gpr_d_data_mdid = (u8)((d64 >> 43) & 0x1f);
	mi->gpr_d_data_start = (u8)((d64 >> 48) & 0xf);
	mi->gpr_d_data_len = (u8)((d64 >> 52) & 0x1f);

	d64 = *(u64 *)&buf[14] >> 7;
	mi->gpr_d_id = (u8)(d64 & 0xf);

	d64 = *(u64 *)&buf[15] >> 3;
	mi->flags = d64;

	d64 = ((*(u64 *)&buf[16] >> 56) & 0x7);
	mi->flags |= (d64 << 61);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_metainit_dump(hw, mi);
}

/**
 * ice_metainit_table_get - create a metainit table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_metainit_item *ice_metainit_table_get(struct ice_hw *hw)
{
	return (struct ice_metainit_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_METADATA_INIT,
					sizeof(struct ice_metainit_item),
					ICE_METAINIT_TABLE_SIZE,
					ice_parser_sect_item_get,
					_metainit_parse_item, false);
}
