/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_IMEM_H_
#define _ICE_IMEM_H_

struct ice_bst_main {
	bool al0;
	bool al1;
	bool al2;
	bool pg;
};

struct ice_bst_keybuilder {
	u8 priority;
	bool tsr_ctrl;
};

struct ice_np_keybuilder {
	u8 ops;
	u8 start_or_reg0;
	u8 len_or_reg1;
};

struct ice_pg_keybuilder {
	bool flag0_ena;
	bool flag1_ena;
	bool flag2_ena;
	bool flag3_ena;
	u8 flag0_idx;
	u8 flag1_idx;
	u8 flag2_idx;
	u8 flag3_idx;
	u8 alu_reg_idx;
};

enum ice_alu_opcode {
	ICE_ALU_PARK = 0,
	ICE_ALU_MOV_ADD = 1,
	ICE_ALU_ADD = 2,
	ICE_ALU_MOV_AND = 4,
	ICE_ALU_AND = 5,
	ICE_ALU_AND_IMM = 6,
	ICE_ALU_MOV_OR = 7,
	ICE_ALU_OR = 8,
	ICE_ALU_MOV_XOR = 9,
	ICE_ALU_XOR = 10,
	ICE_ALU_NOP = 11,
	ICE_ALU_BR = 12,
	ICE_ALU_BREQ = 13,
	ICE_ALU_BRNEQ = 14,
	ICE_ALU_BRGT = 15,
	ICE_ALU_BRLT = 16,
	ICE_ALU_BRGEQ = 17,
	ICE_ALU_BRLEG = 18,
	ICE_ALU_SETEQ = 19,
	ICE_ALU_ANDEQ = 20,
	ICE_ALU_OREQ = 21,
	ICE_ALU_SETNEQ = 22,
	ICE_ALU_ANDNEQ = 23,
	ICE_ALU_ORNEQ = 24,
	ICE_ALU_SETGT = 25,
	ICE_ALU_ANDGT = 26,
	ICE_ALU_ORGT = 27,
	ICE_ALU_SETLT = 28,
	ICE_ALU_ANDLT = 29,
	ICE_ALU_ORLT = 30,
	ICE_ALU_MOV_SUB = 31,
	ICE_ALU_SUB = 32,
	ICE_ALU_INVALID = 64,
};

struct ice_alu {
	enum ice_alu_opcode opc;
	u8 src_start;
	u8 src_len;
	bool shift_xlate_select;
	u8 shift_xlate_key;
	u8 src_reg_id;
	u8 dst_reg_id;
	bool inc0;
	bool inc1;
	u8 proto_offset_opc;
	u8 proto_offset;
	u8 branch_addr;
	u16 imm;
	bool dedicate_flags_ena;
	u8 dst_start;
	u8 dst_len;
	bool flags_extr_imm;
	u8 flags_start_imm;
};

struct ice_imem_item {
	u16 idx;
	struct ice_bst_main b_m;
	struct ice_bst_keybuilder b_kb;
	u8 pg;
	struct ice_np_keybuilder np_kb;
	struct ice_pg_keybuilder pg_kb;
	struct ice_alu alu0;
	struct ice_alu alu1;
	struct ice_alu alu2;
};

void ice_imem_dump(struct ice_hw *hw, struct ice_imem_item *item);
struct ice_imem_item *ice_imem_table_get(struct ice_hw *hw);
#endif /* _ICE_IMEM_H_ */
