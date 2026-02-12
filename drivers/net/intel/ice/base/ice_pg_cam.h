/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_PG_CAM_H_
#define _ICE_PG_CAM_H_

#define ICE_PG_CAM_TABLE_SIZE		2048
#define ICE_PG_SP_CAM_TABLE_SIZE	128
#define ICE_PG_NM_CAM_TABLE_SIZE	1024
#define ICE_PG_NM_SP_CAM_TABLE_SIZE	64

struct ice_pg_cam_key {
	bool valid;
	u16 node_id;
	bool flag0;
	bool flag1;
	bool flag2;
	bool flag3;
	u8 boost_idx;
	u16 alu_reg;
	u32 next_proto;
};

struct ice_pg_nm_cam_key {
	bool valid;
	u16 node_id;
	bool flag0;
	bool flag1;
	bool flag2;
	bool flag3;
	u8 boost_idx;
	u16 alu_reg;
};

struct ice_pg_cam_action {
	u16 next_node;
	u8 next_pc;
	bool is_pg;
	u8 proto_id;
	bool is_mg;
	u8 marker_id;
	bool is_last_round;
	bool ho_polarity;
	u16 ho_inc;
};

struct ice_pg_cam_item {
	u16 idx;
	struct ice_pg_cam_key key;
	struct ice_pg_cam_action action;
};

struct ice_pg_nm_cam_item {
	u16 idx;
	struct ice_pg_nm_cam_key key;
	struct ice_pg_cam_action action;
};

void ice_pg_cam_dump(struct ice_hw *hw, struct ice_pg_cam_item *item);
void ice_pg_nm_cam_dump(struct ice_hw *hw, struct ice_pg_nm_cam_item *item);

struct ice_pg_cam_item *ice_pg_cam_table_get(struct ice_hw *hw);
struct ice_pg_cam_item *ice_pg_sp_cam_table_get(struct ice_hw *hw);

struct ice_pg_nm_cam_item *ice_pg_nm_cam_table_get(struct ice_hw *hw);
struct ice_pg_nm_cam_item *ice_pg_nm_sp_cam_table_get(struct ice_hw *hw);

struct ice_pg_cam_item *ice_pg_cam_match(struct ice_pg_cam_item *table,
					 int size, struct ice_pg_cam_key *key);
struct ice_pg_nm_cam_item *
ice_pg_nm_cam_match(struct ice_pg_nm_cam_item *table, int size,
		    struct ice_pg_cam_key *key);
#endif /* _ICE_PG_CAM_H_ */
