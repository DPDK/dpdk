/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

static void _pg_cam_key_dump(struct ice_hw *hw, struct ice_pg_cam_key *key)
{
	ice_info(hw, "key:\n");
	ice_info(hw, "\tvalid = %d\n", key->valid);
	ice_info(hw, "\tnode_id = %d\n", key->node_id);
	ice_info(hw, "\tflag0 = %d\n", key->flag0);
	ice_info(hw, "\tflag1 = %d\n", key->flag1);
	ice_info(hw, "\tflag2 = %d\n", key->flag2);
	ice_info(hw, "\tflag3 = %d\n", key->flag3);
	ice_info(hw, "\tboost_idx = %d\n", key->boost_idx);
	ice_info(hw, "\talu_reg = 0x%04x\n", key->alu_reg);
	ice_info(hw, "\tnext_proto = 0x%08x\n", key->next_proto);
}

static void _pg_nm_cam_key_dump(struct ice_hw *hw,
				struct ice_pg_nm_cam_key *key)
{
	ice_info(hw, "key:\n");
	ice_info(hw, "\tvalid = %d\n", key->valid);
	ice_info(hw, "\tnode_id = %d\n", key->node_id);
	ice_info(hw, "\tflag0 = %d\n", key->flag0);
	ice_info(hw, "\tflag1 = %d\n", key->flag1);
	ice_info(hw, "\tflag2 = %d\n", key->flag2);
	ice_info(hw, "\tflag3 = %d\n", key->flag3);
	ice_info(hw, "\tboost_idx = %d\n", key->boost_idx);
	ice_info(hw, "\talu_reg = 0x%04x\n", key->alu_reg);
}

static void _pg_cam_action_dump(struct ice_hw *hw,
				struct ice_pg_cam_action *action)
{
	ice_info(hw, "action:\n");
	ice_info(hw, "\tnext_node = %d\n", action->next_node);
	ice_info(hw, "\tnext_pc = %d\n", action->next_pc);
	ice_info(hw, "\tis_pg = %d\n", action->is_pg);
	ice_info(hw, "\tproto_id = %d\n", action->proto_id);
	ice_info(hw, "\tis_mg = %d\n", action->is_mg);
	ice_info(hw, "\tmarker_id = %d\n", action->marker_id);
	ice_info(hw, "\tis_last_round = %d\n", action->is_last_round);
	ice_info(hw, "\tho_polarity = %d\n", action->ho_polarity);
	ice_info(hw, "\tho_inc = %d\n", action->ho_inc);
}

/**
 * ice_pg_cam_dump - dump an parse graph cam info
 * @ice_hw: pointer to the hardware structure
 * @item: parse graph cam to dump
 */
void ice_pg_cam_dump(struct ice_hw *hw, struct ice_pg_cam_item *item)
{
	ice_info(hw, "index = %d\n", item->idx);
	_pg_cam_key_dump(hw, &item->key);
	_pg_cam_action_dump(hw, &item->action);
}

/**
 * ice_pg_nm_cam_dump - dump an parse graph no match cam info
 * @ice_hw: pointer to the hardware structure
 * @item: parse graph no match cam to dump
 */
void ice_pg_nm_cam_dump(struct ice_hw *hw, struct ice_pg_nm_cam_item *item)
{
	ice_info(hw, "index = %d\n", item->idx);
	_pg_nm_cam_key_dump(hw, &item->key);
	_pg_cam_action_dump(hw, &item->action);
}

/** The function parses a 55 bits Parse Graph CAM Action with below format:
 *  BIT 0-11:	Next Node ID (action->next_node)
 *  BIT 12-19:	Next PC (action->next_pc)
 *  BIT 20:	Is Protocol Group (action->is_pg)
 *  BIT 21-23:	reserved
 *  BIT 24-31:	Protocol ID (action->proto_id)
 *  BIT 32:	Is Marker Group (action->is_mg)
 *  BIT 33-40:	Marker ID (action->marker_id)
 *  BIT 41:	Is Last Round (action->is_last_round)
 *  BIT 42:	Header Offset Polarity (action->ho_poloarity)
 *  BIT 43-51:	Header Offset Inc (action->ho_inc)
 *  BIT 52-54:	reserved
 */
static void _pg_cam_action_init(struct ice_pg_cam_action *action, u64 data)
{
	action->next_node = (u16)(data & 0x7ff);
	action->next_pc = (u8)((data >> 11) & 0xff);
	action->is_pg = ((data >> 19) & 0x1) != 0;
	action->proto_id = ((data >> 23) & 0xff);
	action->is_mg = ((data >> 31) & 0x1) != 0;
	action->marker_id = ((data >> 32) & 0xff);
	action->is_last_round = ((data >> 40) & 0x1) != 0;
	action->ho_polarity = ((data >> 41) & 0x1) != 0;
	action->ho_inc = ((data >> 42) & 0x1ff);
}

/** The function parses a 41 bits Parse Graph NoMatch CAM Key with below format:
 *  BIT 0:	Valid (key->valid)
 *  BIT 1-11:	Node ID (key->node_id)
 *  BIT 12:	Flag 0 (key->flag0)
 *  BIT 13:	Flag 1 (key->flag1)
 *  BIT 14:	Flag 2 (key->flag2)
 *  BIT 15:	Flag 3 (key->flag3)
 *  BIT 16:	Boost Hit (key->boost_idx to 0 if it is 0)
 *  BIT 17-24:	Boost Index (key->boost_idx only if Boost Hit is not 0)
 *  BIT 25-40:	ALU Reg (key->alu_reg)
 */
static void _pg_nm_cam_key_init(struct ice_pg_nm_cam_key *key, u64 data)
{
	key->valid = (data & 0x1) != 0;
	key->node_id = (u16)((data >> 1) & 0x7ff);
	key->flag0 = ((data >> 12) & 0x1) != 0;
	key->flag1 = ((data >> 13) & 0x1) != 0;
	key->flag2 = ((data >> 14) & 0x1) != 0;
	key->flag3 = ((data >> 15) & 0x1) != 0;
	if ((data >> 16) & 0x1)
		key->boost_idx = (u8)((data >> 17) & 0xff);
	else
		key->boost_idx = 0;
	key->alu_reg = (u16)((data >> 25) & 0xffff);
}

/** The function parses a 73 bits Parse Graph CAM Key with below format:
 *  BIT 0:	Valid (key->valid)
 *  BIT 1-11:	Node ID (key->node_id)
 *  BIT 12:	Flag 0 (key->flag0)
 *  BIT 13:	Flag 1 (key->flag1)
 *  BIT 14:	Flag 2 (key->flag2)
 *  BIT 15:	Flag 3 (key->flag3)
 *  BIT 16:	Boost Hit (key->boost_idx to 0 if it is 0)
 *  BIT 17-24:	Boost Index (key->boost_idx only if Boost Hit is not 0)
 *  BIT 25-40:	ALU Reg (key->alu_reg)
 *  BIT 41-72:	Next Proto Key (key->next_proto)
 */
static void _pg_cam_key_init(struct ice_pg_cam_key *key, u8 *data)
{
	u64 d64 = *(u64 *)data;

	key->valid = (d64 & 0x1) != 0;
	key->node_id = (u16)((d64 >> 1) & 0x7ff);
	key->flag0 = ((d64 >> 12) & 0x1) != 0;
	key->flag1 = ((d64 >> 13) & 0x1) != 0;
	key->flag2 = ((d64 >> 14) & 0x1) != 0;
	key->flag3 = ((d64 >> 15) & 0x1) != 0;
	if ((d64 >> 16) & 0x1)
		key->boost_idx = (u8)((d64 >> 17) & 0xff);
	else
		key->boost_idx = 0;
	key->alu_reg = (u16)((d64 >> 25) & 0xffff);

	key->next_proto = (*(u32 *)&data[5] >> 1);
	key->next_proto |= ((u32)(data[9] & 0x1) << 31);
}

/** The function parses a 128 bits Parse Graph CAM Entry with below format:
 *  BIT 0-72:	Key (ci->key)
 *  BIT 73-127:	Action (ci->action)
 */
static void _pg_cam_parse_item(struct ice_hw *hw, u16 idx, void *item,
			       void *data, int size)
{
	struct ice_pg_cam_item *ci = (struct ice_pg_cam_item *)item;
	u8 *buf = (u8 *)data;
	u64 d64;

	ci->idx = idx;
	d64 = (*(u64 *)&buf[9] >> 1);
	_pg_cam_key_init(&ci->key, buf);
	_pg_cam_action_init(&ci->action, d64);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_pg_cam_dump(hw, ci);
}

/** The function parses a 136 bits Parse Graph Spill CAM Entry with below
 *  format:
 *  BIT 0-55:	Action (ci->key)
 *  BIT 56-135:	Key (ci->action)
 */
static void _pg_sp_cam_parse_item(struct ice_hw *hw, u16 idx, void *item,
				  void *data, int size)
{
	struct ice_pg_cam_item *ci = (struct ice_pg_cam_item *)item;
	u8 *buf = (u8 *)data;
	u64 d64;

	ci->idx = idx;
	d64 = *(u64 *)buf;
	_pg_cam_action_init(&ci->action, d64);
	_pg_cam_key_init(&ci->key, &buf[7]);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_pg_cam_dump(hw, ci);
}

/** The function parses a 96 bits Parse Graph NoMatch CAM Entry with below
 *  format:
 *  BIT 0-40:	Key (ci->key)
 *  BIT 41-95:	Action (ci->action)
 */
static void _pg_nm_cam_parse_item(struct ice_hw *hw, u16 idx, void *item,
				  void *data, int size)
{
	struct ice_pg_nm_cam_item *ci = (struct ice_pg_nm_cam_item *)item;
	u8 *buf = (u8 *)data;
	u64 d64;

	ci->idx = idx;
	d64 = *(u64 *)buf;
	_pg_nm_cam_key_init(&ci->key, d64);
	d64 = (*(u64 *)&buf[5] >> 1);
	_pg_cam_action_init(&ci->action, d64);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_pg_nm_cam_dump(hw, ci);
}

/** The function parses a 104 bits Parse Graph NoMatch Spill CAM Entry with
 *  below format:
 *  BIT 0-55:	Key (ci->key)
 *  BIT 56-103:	Action (ci->action)
 */
static void _pg_nm_sp_cam_parse_item(struct ice_hw *hw, u16 idx, void *item,
				     void *data, int size)
{
	struct ice_pg_nm_cam_item *ci = (struct ice_pg_nm_cam_item *)item;
	u8 *buf = (u8 *)data;
	u64 d64;

	ci->idx = idx;
	d64 = *(u64 *)buf;
	_pg_cam_action_init(&ci->action, d64);
	d64 = *(u64 *)&buf[7];
	_pg_nm_cam_key_init(&ci->key, d64);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_pg_nm_cam_dump(hw, ci);
}

/**
 * ice_pg_cam_table_get - create a parse graph cam table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_pg_cam_item *ice_pg_cam_table_get(struct ice_hw *hw)
{
	return (struct ice_pg_cam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_CAM,
					sizeof(struct ice_pg_cam_item),
					ICE_PG_CAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_pg_cam_parse_item, false);
}

/**
 * ice_pg_sp_cam_table_get - create a parse graph spill cam table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_pg_cam_item *ice_pg_sp_cam_table_get(struct ice_hw *hw)
{
	return (struct ice_pg_cam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_PG_SPILL,
					sizeof(struct ice_pg_cam_item),
					ICE_PG_SP_CAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_pg_sp_cam_parse_item, false);
}

/**
 * ice_pg_nm_cam_table_get - create a parse graph no match cam table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_pg_nm_cam_item *ice_pg_nm_cam_table_get(struct ice_hw *hw)
{
	return (struct ice_pg_nm_cam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_NOMATCH_CAM,
					sizeof(struct ice_pg_nm_cam_item),
					ICE_PG_NM_CAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_pg_nm_cam_parse_item, false);
}

/**
 * ice_pg_nm_sp_cam_table_get - create a parse graph no match spill cam table
 * @ice_hw: pointer to the hardware structure
 */
struct ice_pg_nm_cam_item *ice_pg_nm_sp_cam_table_get(struct ice_hw *hw)
{
	return (struct ice_pg_nm_cam_item *)
		ice_parser_create_table(hw, ICE_SID_RXPARSER_NOMATCH_SPILL,
					sizeof(struct ice_pg_nm_cam_item),
					ICE_PG_NM_SP_CAM_TABLE_SIZE,
					ice_parser_sect_item_get,
					_pg_nm_sp_cam_parse_item, false);
}

static bool _pg_cam_match(struct ice_pg_cam_item *item,
			  struct ice_pg_cam_key *key)
{
	if (!item->key.valid ||
	    item->key.node_id != key->node_id ||
	    item->key.flag0 != key->flag0 ||
	    item->key.flag1 != key->flag1 ||
	    item->key.flag2 != key->flag2 ||
	    item->key.flag3 != key->flag3 ||
	    item->key.boost_idx != key->boost_idx ||
	    item->key.alu_reg != key->alu_reg ||
	    item->key.next_proto != key->next_proto)
		return false;

	return true;
}

static bool _pg_nm_cam_match(struct ice_pg_nm_cam_item *item,
			     struct ice_pg_cam_key *key)
{
	if (!item->key.valid ||
	    item->key.node_id != key->node_id ||
	    item->key.flag0 != key->flag0 ||
	    item->key.flag1 != key->flag1 ||
	    item->key.flag2 != key->flag2 ||
	    item->key.flag3 != key->flag3 ||
	    item->key.boost_idx != key->boost_idx ||
	    item->key.alu_reg != key->alu_reg)
		return false;

	return true;
}

/**
 * ice_pg_cam_match - search parse graph cam table by key
 * @table: parse graph cam table to search
 * @size: cam table size
 * @key: search key
 */
struct ice_pg_cam_item *ice_pg_cam_match(struct ice_pg_cam_item *table,
					 int size, struct ice_pg_cam_key *key)
{
	int i;

	for (i = 0; i < size; i++) {
		struct ice_pg_cam_item *item = &table[i];

		if (_pg_cam_match(item, key))
			return item;
	}

	return NULL;
}

/**
 * ice_pg_nm_cam_match - search parse graph no match cam table by key
 * @table: parse graph no match cam table to search
 * @size: cam table size
 * @key: search key
 */
struct ice_pg_nm_cam_item *
ice_pg_nm_cam_match(struct ice_pg_nm_cam_item *table, int size,
		    struct ice_pg_cam_key *key)
{
	int i;

	for (i = 0; i < size; i++) {
		struct ice_pg_nm_cam_item *item = &table[i];

		if (_pg_nm_cam_match(item, key))
			return item;
	}

	return NULL;
}
