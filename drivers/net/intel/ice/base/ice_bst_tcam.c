/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_BST_TCAM_TABLE_SIZE 256

static void _bst_np_kb_dump(struct ice_hw *hw, struct ice_np_keybuilder *kb)
{
	ice_info(hw, "next proto key builder:\n");
	ice_info(hw, "\tops = %d\n", kb->ops);
	ice_info(hw, "\tstart_or_reg0 = %d\n", kb->start_or_reg0);
	ice_info(hw, "\tlen_or_reg1 = %d\n", kb->len_or_reg1);
}

static void _bst_pg_kb_dump(struct ice_hw *hw, struct ice_pg_keybuilder *kb)
{
	ice_info(hw, "parse graph key builder:\n");
	ice_info(hw, "\tflag0_ena = %d\n", kb->flag0_ena);
	ice_info(hw, "\tflag1_ena = %d\n", kb->flag1_ena);
	ice_info(hw, "\tflag2_ena = %d\n", kb->flag2_ena);
	ice_info(hw, "\tflag3_ena = %d\n", kb->flag3_ena);
	ice_info(hw, "\tflag0_idx = %d\n", kb->flag0_idx);
	ice_info(hw, "\tflag1_idx = %d\n", kb->flag1_idx);
	ice_info(hw, "\tflag2_idx = %d\n", kb->flag2_idx);
	ice_info(hw, "\tflag3_idx = %d\n", kb->flag3_idx);
	ice_info(hw, "\talu_reg_idx = %d\n", kb->alu_reg_idx);
}

static void _bst_alu_dump(struct ice_hw *hw, struct ice_alu *alu, int index)
{
	ice_info(hw, "alu%d:\n", index);
	ice_info(hw, "\topc = %d\n", alu->opc);
	ice_info(hw, "\tsrc_start = %d\n", alu->src_start);
	ice_info(hw, "\tsrc_len = %d\n", alu->src_len);
	ice_info(hw, "\tshift_xlate_select = %d\n", alu->shift_xlate_select);
	ice_info(hw, "\tshift_xlate_key = %d\n", alu->shift_xlate_key);
	ice_info(hw, "\tsrc_reg_id = %d\n", alu->src_reg_id);
	ice_info(hw, "\tdst_reg_id = %d\n", alu->dst_reg_id);
	ice_info(hw, "\tinc0 = %d\n", alu->inc0);
	ice_info(hw, "\tinc1 = %d\n", alu->inc1);
	ice_info(hw, "\tproto_offset_opc = %d\n", alu->proto_offset_opc);
	ice_info(hw, "\tproto_offset = %d\n", alu->proto_offset);
	ice_info(hw, "\tbranch_addr = %d\n", alu->branch_addr);
	ice_info(hw, "\timm = %d\n", alu->imm);
	ice_info(hw, "\tdst_start = %d\n", alu->dst_start);
	ice_info(hw, "\tdst_len = %d\n", alu->dst_len);
	ice_info(hw, "\tflags_extr_imm = %d\n", alu->flags_extr_imm);
	ice_info(hw, "\tflags_start_imm= %d\n", alu->flags_start_imm);
}

/**
 * ice_bst_tcam_dump - dump a boost tcam info
 * @hw: pointer to the hardware structure
 * @item: boost tcam to dump
 */
void ice_bst_tcam_dump(struct ice_hw *hw, struct ice_bst_tcam_item *item)
{
	int i;

	ice_info(hw, "address = %d\n", item->address);
	ice_info(hw, "key    :");
	for (i = 0; i < 20; i++)
		ice_info(hw, "%02x ", item->key[i]);
	ice_info(hw, "\n");
	ice_info(hw, "key_inv:");
	for (i = 0; i < 20; i++)
		ice_info(hw, "%02x ", item->key_inv[i]);
	ice_info(hw, "\n");
	ice_info(hw, "hit_idx_grp = %d\n", item->hit_idx_grp);
	ice_info(hw, "pg_pri = %d\n", item->pg_pri);
	_bst_np_kb_dump(hw, &item->np_kb);
	_bst_pg_kb_dump(hw, &item->pg_kb);
	_bst_alu_dump(hw, &item->alu0, 0);
	_bst_alu_dump(hw, &item->alu1, 1);
	_bst_alu_dump(hw, &item->alu2, 2);
}

/** The function parses a 96 bits ALU entry with below format:
 *  BIT 0-5:	Opcode (alu->opc)
 *  BIT 6-13:	Source Start (alu->src_start)
 *  BIT 14-18:	Source Length (alu->src_len)
 *  BIT 19:	Shift/Xlate Select (alu->shift_xlate_select)
 *  BIT 20-23:	Shift/Xlate Key (alu->shift_xlate_key)
 *  BIT 24-30:	Source Register ID (alu->src_reg_id)
 *  BIT 31-37:	Dest. Register ID (alu->dst_reg_id)
 *  BIT 38:	Inc0 (alu->inc0)
 *  BIT 39:	Inc1:(alu->inc1)
 *  BIT 40:41	Protocol Offset Opcode (alu->proto_offset_opc)
 *  BIT 42:49	Protocol Offset (alu->proto_offset)
 *  BIT 50:57	Branch Address (alu->branch_addr)
 *  BIT 58:73	Immediate (alu->imm)
 *  BIT 74	Dedicated Flags Enable (alu->dedicate_flags_ena)
 *  BIT 75:80	Dest. Start (alu->dst_start)
 *  BIT 81:86	Dest. Length (alu->dst_len)
 *  BIT 87	Flags Extract Imm. (alu->flags_extr_imm)
 *  BIT 88:95	Flags Start/Immediate (alu->flags_start_imm)
 *
 *  NOTE: the first 7 bits are skipped as the start bit is not
 *  byte aligned.
 */
static void _bst_alu_init(struct ice_alu *alu, u8 *data)
{
	u64 d64 = *(u64 *)data >> 7;

	alu->opc = (enum ice_alu_opcode)(d64 & 0x3f);
	alu->src_start = (u8)((d64 >> 6) & 0xff);
	alu->src_len = (u8)((d64 >> 14) & 0x1f);
	alu->shift_xlate_select = ((d64 >> 19) & 0x1) != 0;
	alu->shift_xlate_key = (u8)((d64 >> 20) & 0xf);
	alu->src_reg_id = (u8)((d64 >> 24) & 0x7f);
	alu->dst_reg_id = (u8)((d64 >> 31) & 0x7f);
	alu->inc0 = ((d64 >> 38) & 0x1) != 0;
	alu->inc1 = ((d64 >> 39) & 0x1) != 0;
	alu->proto_offset_opc = (u8)((d64 >> 40) & 0x3);
	alu->proto_offset = (u8)((d64 >> 42) & 0xff);

	d64 = *(u64 *)(&data[6]) >> 9;

	alu->branch_addr = (u8)(d64 & 0xff);
	alu->imm = (u16)((d64 >> 8) & 0xffff);
	alu->dedicate_flags_ena = ((d64 >> 24) & 0x1) != 0;
	alu->dst_start = (u8)((d64 >> 25) & 0x3f);
	alu->dst_len = (u8)((d64 >> 31) & 0x3f);
	alu->flags_extr_imm = ((d64 >> 37) & 0x1) != 0;
	alu->flags_start_imm = (u8)((d64 >> 38) & 0xff);
}

/** The function parses a 35 bits Parse Graph Key Build with below format:
 *  BIT 0:	Flag 0 Enable (kb->flag0_ena)
 *  BIT 1-6:	Flag 0 Index (kb->flag0_idx)
 *  BIT 7:	Flag 1 Enable (kb->flag1_ena)
 *  BIT 8-13:	Flag 1 Index (kb->flag1_idx)
 *  BIT 14:	Flag 2 Enable (kb->flag2_ena)
 *  BIT 15-20:	Flag 2 Index (kb->flag2_idx)
 *  BIT 21:	Flag 3 Enable (kb->flag3_ena)
 *  BIT 22-27:	Flag 3 Index (kb->flag3_idx)
 *  BIT 28-34:	ALU Register Index (kb->alu_reg_idx)
 */
static void _bst_pgkb_init(struct ice_pg_keybuilder *kb, u64 data)
{
	kb->flag0_ena = (data & 0x1) != 0;
	kb->flag0_idx = (u8)((data >> 1) & 0x3f);
	kb->flag1_ena = ((data >> 7) & 0x1) != 0;
	kb->flag1_idx = (u8)((data >> 8) & 0x3f);
	kb->flag2_ena = ((data >> 14) & 0x1) != 0;
	kb->flag2_idx = (u8)((data >> 15) & 0x3f);
	kb->flag3_ena = ((data >> 21) & 0x1) != 0;
	kb->flag3_idx = (u8)((data >> 22) & 0x3f);
	kb->alu_reg_idx = (u8)((data >> 28) & 0x7f);
}

/** The function parses a 18 bits Next Protocol Key Build with below format:
 *  BIT 0-1:	Opcode kb->ops
 *  BIT 2-9:	Start / Reg 0 (kb->start_or_reg0)
 *  BIT 10-17:	Length / Reg 1 (kb->len_or_reg1)
 */
static void _bst_npkb_init(struct ice_np_keybuilder *kb, u32 data)
{
	kb->ops = (u8)(data & 0x3);
	kb->start_or_reg0 = (u8)((data >> 2) & 0xff);
	kb->len_or_reg1 = (u8)((data >> 10) & 0xff);
}

/** The function parses a 704 bits Boost TCAM entry with below format:
 *  BIT 0-15:	Address (ti->address)
 *  BIT 16-31:	reserved
 *  BIT 32-191: Key (ti->key)
 *  BIT 192-351:Key Invert (ti->key_inv)
 *  BIT 352-359:Boost Hit Index Group (ti->hit_idx_grp)
 *  BIT 360-361:PG Priority (ti->pg_pri)
 *  BIT 362-379:Next Proto Key Build (ti->np_kb)
 *  BIT 380-414:PG Key Build (ti->pg_kb)
 *  BIT 415-510:ALU 0 (ti->alu0)
 *  BIT 511-606:ALU 1 (ti->alu1)
 *  BIT 607-702:ALU 2 (ti->alu2)
 *  BIT 703:	reserved
 */
static void _bst_parse_item(struct ice_hw *hw, u16 idx, void *item,
			    void *data, int size)
{
	struct ice_bst_tcam_item *ti = (struct ice_bst_tcam_item *)item;
	u8 *buf = (u8 *)data;
	int i;

	ti->address = *(u16 *)buf;

	for (i = 0; i < 20; i++)
		ti->key[i] = buf[4 + i];
	for (i = 0; i < 20; i++)
		ti->key_inv[i] = buf[24 + i];
	ti->hit_idx_grp = buf[44];
	ti->pg_pri = buf[45] & 0x3;
	_bst_npkb_init(&ti->np_kb, *(u32 *)&buf[45] >> 2);
	_bst_pgkb_init(&ti->pg_kb, *(u64 *)&buf[47] >> 4);
	_bst_alu_init(&ti->alu0, &buf[51]);
	_bst_alu_init(&ti->alu1, &buf[63]);
	_bst_alu_init(&ti->alu2, &buf[75]);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_bst_tcam_dump(hw, ti);
}

/**
 * ice_bst_tcam_table_get - create a boost tcam table
 * @hw: pointer to the hardware structure
 */
struct ice_bst_tcam_item *ice_bst_tcam_table_get(struct ice_hw *hw)
{
	return (struct ice_bst_tcam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_BOOST_TCAM,
					sizeof(struct ice_bst_tcam_item),
					ICE_BST_TCAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_bst_parse_item, true);
}

static void _parse_lbl_item(struct ice_hw *hw, u16 idx, void *item,
			    void *data, int size)
{
	ice_parse_item_dflt(hw, idx, item, data, size);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_lbl_dump(hw, (struct ice_lbl_item *)item);
}

/**
 * ice_bst_lbl_table_get - create a boost label table
 * @hw: pointer to the hardware structure
 */
struct ice_lbl_item *ice_bst_lbl_table_get(struct ice_hw *hw)
{
	return (struct ice_lbl_item *)
		ice_parser_create_table(hw, ICE_SID_LBL_RXPARSER_TMEM,
					sizeof(struct ice_lbl_item),
					ICE_BST_TCAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_parse_lbl_item, true);
}

/**
 * ice_bst_tcam_match - match a pattern on the boost tcam table
 * @tcam_table: boost tcam table to search
 * @pat: pattern to match
 */
struct ice_bst_tcam_item *
ice_bst_tcam_match(struct ice_bst_tcam_item *tcam_table, u8 *pat)
{
	int i;

	for (i = 0; i < ICE_BST_TCAM_TABLE_SIZE; i++) {
		struct ice_bst_tcam_item *item = &tcam_table[i];

		if (item->hit_idx_grp == 0)
			continue;
		if (ice_ternary_match(item->key, item->key_inv, pat, 20))
			return item;
	}

	return NULL;
}

static bool _start_with(const char *prefix, const char *string)
{
	int len1 = strlen(prefix);
	int len2 = strlen(string);

	if (len2 < len1)
		return false;

	return !memcmp(prefix, string, len1);
}

struct ice_bst_tcam_item *
ice_bst_tcam_search(struct ice_bst_tcam_item *tcam_table,
		    struct ice_lbl_item *lbl_table,
		    const char *prefix, u16 *start)
{
	u16 i = *start;

	for (; i < ICE_BST_TCAM_TABLE_SIZE; i++) {
		if (_start_with(prefix, lbl_table[i].label)) {
			*start = i;
			return &tcam_table[lbl_table[i].idx];
		}
	}

	return NULL;
}
