/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"

#define GPR_HB_IDX	64
#define GPR_ERR_IDX	84
#define GPR_FLG_IDX	104
#define GPR_TSR_IDX	108
#define GPR_NN_IDX	109
#define GPR_HO_IDX	110
#define GPR_NP_IDX	111

static void _rt_tsr_set(struct ice_parser_rt *rt, u16 tsr)
{
	rt->gpr[GPR_TSR_IDX] = tsr;
}

static void _rt_ho_set(struct ice_parser_rt *rt, u16 ho)
{
	rt->gpr[GPR_HO_IDX] = ho;
	ice_memcpy(&rt->gpr[GPR_HB_IDX], &rt->pkt_buf[ho], 32,
		   ICE_NONDMA_TO_NONDMA);
}

static void _rt_np_set(struct ice_parser_rt *rt, u16 pc)
{
	rt->gpr[GPR_NP_IDX] = pc;
}

static void _rt_nn_set(struct ice_parser_rt *rt, u16 node)
{
	rt->gpr[GPR_NN_IDX] = node;
}

static void _rt_flag_set(struct ice_parser_rt *rt, int idx, bool val)
{
	int y = idx / 16;
	int x = idx % 16;

	if (val)
		rt->gpr[GPR_FLG_IDX + y] |= (u16)(1 << x);
	else
		rt->gpr[GPR_FLG_IDX + y] &= ~(u16)(1 << x);

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set parser flag %d value %d\n",
		  idx, val);
}

static void _rt_gpr_set(struct ice_parser_rt *rt, int idx, u16 val)
{
	if (idx == GPR_HO_IDX)
		_rt_ho_set(rt, val);
	else
		rt->gpr[idx] = val;

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set GPR %d value %d\n",
		  idx, val);
}

static void _rt_err_set(struct ice_parser_rt *rt, int idx, bool val)
{
	if (val)
		rt->gpr[GPR_ERR_IDX] |= (u16)(1 << idx);
	else
		rt->gpr[GPR_ERR_IDX] &= ~(u16)(1 << idx);

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set parser error %d value %d\n",
		  idx, val);
}

/**
 * ice_parser_rt_reset - reset the parser runtime
 * @rt: pointer to the parser runtime
 */
void ice_parser_rt_reset(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	struct ice_metainit_item *mi = &psr->mi_table[0];
	int i;

	ice_memset(rt, 0, sizeof(*rt), ICE_NONDMA_MEM);

	_rt_tsr_set(rt, mi->tsr);
	_rt_ho_set(rt, mi->ho);
	_rt_np_set(rt, mi->pc);
	_rt_nn_set(rt, mi->pg_rn);

	rt->psr = psr;

	for (i = 0; i < 64; i++) {
		if ((mi->flags & (1ul << i)) != 0ul)
			_rt_flag_set(rt, i, true);
	}
}

/**
 * ice_parser_rt_pktbuf_set - set a packet into parser runtime
 * @rt: pointer to the parser runtime
 * @pkt_buf: buffer with packet data
 * @pkt_len: packet buffer length
 */
void ice_parser_rt_pktbuf_set(struct ice_parser_rt *rt, const u8 *pkt_buf,
			      int pkt_len)
{
	int len = min(ICE_PARSER_MAX_PKT_LEN, pkt_len);
	u16 ho = rt->gpr[GPR_HO_IDX];

	ice_memcpy(rt->pkt_buf, pkt_buf, len, ICE_NONDMA_TO_NONDMA);
	rt->pkt_len = pkt_len;

	ice_memcpy(&rt->gpr[GPR_HB_IDX], &rt->pkt_buf[ho],
		   ICE_PARSER_HDR_BUF_LEN, ICE_NONDMA_TO_NONDMA);
}

static void _bst_key_init(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	int second_last_key_idx = ICE_PARSER_BST_KEY_LEN - 2;
	int last_key_idx = ICE_PARSER_BST_KEY_LEN - 1;
	u8 tsr = (u8)rt->gpr[GPR_TSR_IDX];
	u16 ho = rt->gpr[GPR_HO_IDX];
	u8 *key = rt->bst_key;

	int i, j;

	if (imem->b_kb.tsr_ctrl)
		key[last_key_idx] = (u8)tsr;
	else
		key[last_key_idx] = imem->b_kb.priority;

	for (i = second_last_key_idx; i >= 0; i--) {
		j = ho + second_last_key_idx - i;
		if (j < ICE_PARSER_MAX_PKT_LEN)
			key[i] = rt->pkt_buf[ho + second_last_key_idx - i];
		else
			key[i] = 0;
	}

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Generated Boost TCAM Key:\n");
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		  key[0], key[1], key[2], key[3], key[4],
		  key[5], key[6], key[7], key[8], key[9],
		  key[10], key[11], key[12], key[13], key[14],
		  key[15], key[16], key[17], key[18], key[19]);
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "\n");
}

static u8 _bit_rev_u8(u8 v)
{
	u8 r = 0;
	int i;

	for (i = 0; i < 8; i++) {
		r |= (u8)((v & 0x1) << (7 - i));
		v >>= 1;
	}

	return r;
}

static u8 _bit_rev_u16(u16 v, int len)
{
	u16 r = 0;
	int i;

	for (i = 0; i < len; i++) {
		r |= (u16)((v & 0x1) << (len - 1 - i));
		v >>= 1;
	}

	return r;
}

static u32 _bit_rev_u32(u32 v, int len)
{
	u32 r = 0;
	int i;

	for (i = 0; i < len; i++) {
		r |= (u32)((v & 0x1) << (len - 1 - i));
		v >>= 1;
	}

	return r;
}

static u32 _hv_bit_sel(struct ice_parser_rt *rt, int start, int len)
{
	u64 msk;
	union {
		u64 d64;
		u8 b[8];
	} bit_sel;
	int i;

	int offset = GPR_HB_IDX + start / 16;

	ice_memcpy(bit_sel.b, &rt->gpr[offset], 8, ICE_NONDMA_TO_NONDMA);

	for (i = 0; i < 8; i++)
		bit_sel.b[i] = _bit_rev_u8(bit_sel.b[i]);

	msk = (1ul << len) - 1;

	return _bit_rev_u32((u32)((bit_sel.d64 >> (start % 16)) & msk), len);
}

static u32 _pk_build(struct ice_parser_rt *rt, struct ice_np_keybuilder *kb)
{
	if (kb->ops == 0)
		return _hv_bit_sel(rt, kb->start_or_reg0, kb->len_or_reg1);
	else if (kb->ops == 1)
		return rt->gpr[kb->start_or_reg0] |
		       ((u32)rt->gpr[kb->len_or_reg1] << 16);
	else if (kb->ops == 2)
		return 0;

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Unsupported ops %d\n", kb->ops);
	return 0xffffffff;
}

static bool _flag_get(struct ice_parser_rt *rt, int index)
{
	int y = index / 16;
	int x = index % 16;

	return (rt->gpr[GPR_FLG_IDX + y] & (u16)(1 << x)) != 0;
}

static void _imem_pgk_init(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	ice_memset(&rt->pg_key, 0, sizeof(rt->pg_key), ICE_NONDMA_MEM);
	rt->pg_key.next_proto = _pk_build(rt, &imem->np_kb);

	if (imem->pg_kb.flag0_ena)
		rt->pg_key.flag0 = _flag_get(rt, imem->pg_kb.flag0_idx);
	if (imem->pg_kb.flag1_ena)
		rt->pg_key.flag1 = _flag_get(rt, imem->pg_kb.flag1_idx);
	if (imem->pg_kb.flag2_ena)
		rt->pg_key.flag2 = _flag_get(rt, imem->pg_kb.flag2_idx);
	if (imem->pg_kb.flag3_ena)
		rt->pg_key.flag3 = _flag_get(rt, imem->pg_kb.flag3_idx);

	rt->pg_key.alu_reg = rt->gpr[imem->pg_kb.alu_reg_idx];
	rt->pg_key.node_id = rt->gpr[GPR_NN_IDX];

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Generate Parse Graph Key: node_id(%d),flag0(%d), flag1(%d), flag2(%d), flag3(%d), boost_idx(%d), alu_reg(0x%04x), next_proto(0x%08x)\n",
		  rt->pg_key.node_id,
		  rt->pg_key.flag0,
		  rt->pg_key.flag1,
		  rt->pg_key.flag2,
		  rt->pg_key.flag3,
		  rt->pg_key.boost_idx,
		  rt->pg_key.alu_reg,
		  rt->pg_key.next_proto);
}

static void _imem_alu0_set(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	rt->alu0 = &imem->alu0;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU0 from imem pc %d\n",
		  imem->idx);
}

static void _imem_alu1_set(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	rt->alu1 = &imem->alu1;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU1 from imem pc %d\n",
		  imem->idx);
}

static void _imem_alu2_set(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	rt->alu2 = &imem->alu2;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU2 from imem pc %d\n",
		  imem->idx);
}

static void _imem_pgp_set(struct ice_parser_rt *rt, struct ice_imem_item *imem)
{
	rt->pg = imem->pg;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load PG priority %d from imem pc %d\n",
		  rt->pg, imem->idx);
}

static void
_bst_pgk_init(struct ice_parser_rt *rt, struct ice_bst_tcam_item *bst)
{
	ice_memset(&rt->pg_key, 0, sizeof(rt->pg_key), ICE_NONDMA_MEM);
	rt->pg_key.boost_idx = bst->hit_idx_grp;
	rt->pg_key.next_proto = _pk_build(rt, &bst->np_kb);

	if (bst->pg_kb.flag0_ena)
		rt->pg_key.flag0 = _flag_get(rt, bst->pg_kb.flag0_idx);
	if (bst->pg_kb.flag1_ena)
		rt->pg_key.flag1 = _flag_get(rt, bst->pg_kb.flag1_idx);
	if (bst->pg_kb.flag2_ena)
		rt->pg_key.flag2 = _flag_get(rt, bst->pg_kb.flag2_idx);
	if (bst->pg_kb.flag3_ena)
		rt->pg_key.flag3 = _flag_get(rt, bst->pg_kb.flag3_idx);

	rt->pg_key.alu_reg = rt->gpr[bst->pg_kb.alu_reg_idx];
	rt->pg_key.node_id = rt->gpr[GPR_NN_IDX];

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Generate Parse Graph Key: node_id(%d),flag0(%d), flag1(%d), flag2(%d), flag3(%d), boost_idx(%d), alu_reg(0x%04x), next_proto(0x%08x)\n",
		  rt->pg_key.node_id,
		  rt->pg_key.flag0,
		  rt->pg_key.flag1,
		  rt->pg_key.flag2,
		  rt->pg_key.flag3,
		  rt->pg_key.boost_idx,
		  rt->pg_key.alu_reg,
		  rt->pg_key.next_proto);
}

static void _bst_alu0_set(struct ice_parser_rt *rt,
			  struct ice_bst_tcam_item *bst)
{
	rt->alu0 = &bst->alu0;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU0 from boost address %d\n",
		  bst->address);
}

static void _bst_alu1_set(struct ice_parser_rt *rt,
			  struct ice_bst_tcam_item *bst)
{
	rt->alu1 = &bst->alu1;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU1 from boost address %d\n",
		  bst->address);
}

static void _bst_alu2_set(struct ice_parser_rt *rt,
			  struct ice_bst_tcam_item *bst)
{
	rt->alu2 = &bst->alu2;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load ALU2 from boost address %d\n",
		  bst->address);
}

static void _bst_pgp_set(struct ice_parser_rt *rt,
			 struct ice_bst_tcam_item *bst)
{
	rt->pg = bst->pg_pri;
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load PG priority %d from boost address %d\n",
		  rt->pg, bst->address);
}

static struct ice_pg_cam_item *_pg_cam_match(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	struct ice_pg_cam_item *item;

	item = ice_pg_cam_match(psr->pg_cam_table, ICE_PG_CAM_TABLE_SIZE,
				&rt->pg_key);
	if (item)
		return item;

	item = ice_pg_cam_match(psr->pg_sp_cam_table, ICE_PG_SP_CAM_TABLE_SIZE,
				&rt->pg_key);
	return item;
}

static struct ice_pg_nm_cam_item *_pg_nm_cam_match(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	struct ice_pg_nm_cam_item *item;

	item = ice_pg_nm_cam_match(psr->pg_nm_cam_table,
				   ICE_PG_NM_CAM_TABLE_SIZE, &rt->pg_key);

	if (item)
		return item;

	item = ice_pg_nm_cam_match(psr->pg_nm_sp_cam_table,
				   ICE_PG_NM_SP_CAM_TABLE_SIZE,
				   &rt->pg_key);
	return item;
}

static void _gpr_add(struct ice_parser_rt *rt, int idx, u16 val)
{
	rt->pu.gpr_val_upd[idx] = true;
	rt->pu.gpr_val[idx] = val;

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Pending update for register %d value %d\n",
		  idx, val);
}

static void _pg_exe(struct ice_parser_rt *rt)
{
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ParseGraph action ...\n");

	_gpr_add(rt, GPR_NP_IDX, rt->action->next_pc);
	_gpr_add(rt, GPR_NN_IDX, rt->action->next_node);

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ParseGraph action done.\n");
}

static void _flg_add(struct ice_parser_rt *rt, int idx, bool val)
{
	rt->pu.flg_msk |= (1ul << idx);
	if (val)
		rt->pu.flg_val |= (1ul << idx);
	else
		rt->pu.flg_val &= ~(1ul << idx);

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Pending update for flag %d value %d\n",
		  idx, val);
}

static void _flg_update(struct ice_parser_rt *rt, struct ice_alu *alu)
{
	int i;

	if (alu->dedicate_flags_ena) {
		if (alu->flags_extr_imm) {
			for (i = 0; i < alu->dst_len; i++)
				_flg_add(rt, alu->dst_start + i,
					 (alu->flags_start_imm &
					  (1u << i)) != 0);
		} else {
			for (i = 0; i < alu->dst_len; i++) {
				_flg_add(rt, alu->dst_start + i,
					 _hv_bit_sel(rt,
						     alu->flags_start_imm + i,
						     1) != 0);
			}
		}
	}
}

static void _po_update(struct ice_parser_rt *rt, struct ice_alu *alu)
{
	if (alu->proto_offset_opc == 1)
		rt->po = (u16)(rt->gpr[GPR_HO_IDX] + alu->proto_offset);
	else if (alu->proto_offset_opc == 2)
		rt->po = (u16)(rt->gpr[GPR_HO_IDX] - alu->proto_offset);
	else if (alu->proto_offset_opc == 0)
		rt->po = rt->gpr[GPR_HO_IDX];

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Update Protocol Offset = %d\n",
		  rt->po);
}

static u16 _reg_bit_sel(struct ice_parser_rt *rt, int reg_idx,
			int start, int len)
{
	u32 msk;
	union {
		u32 d32;
		u8 b[4];
	} bit_sel;

	ice_memcpy(bit_sel.b, &rt->gpr[reg_idx + start / 16], 4,
		   ICE_NONDMA_TO_NONDMA);

	bit_sel.b[0] = _bit_rev_u8(bit_sel.b[0]);
	bit_sel.b[1] = _bit_rev_u8(bit_sel.b[1]);
	bit_sel.b[2] = _bit_rev_u8(bit_sel.b[2]);
	bit_sel.b[3] = _bit_rev_u8(bit_sel.b[3]);

	msk = (1u << len) - 1;

	return _bit_rev_u16((u16)((bit_sel.d32 >> (start % 16)) & msk), len);
}

static void _err_add(struct ice_parser_rt *rt, int idx, bool val)
{
	rt->pu.err_msk |= (u16)(1 << idx);
	if (val)
		rt->pu.flg_val |= (u16)(1 << idx);
	else
		rt->pu.flg_val &= ~(u16)(1 << idx);

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Pending update for error %d value %d\n",
		  idx, val);
}

static void _dst_reg_bit_set(struct ice_parser_rt *rt, struct ice_alu *alu,
			     bool val)
{
	u16 flg_idx;

	if (alu->dedicate_flags_ena) {
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "DedicatedFlagsEnable should not be enabled in opcode %d\n",
			  alu->opc);
		return;
	}

	if (alu->dst_reg_id == GPR_ERR_IDX) {
		if (alu->dst_start >= 16) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Invalid error %d\n",
				  alu->dst_start);
			return;
		}
		_err_add(rt, alu->dst_start, val);
	} else if (alu->dst_reg_id >= GPR_FLG_IDX) {
		flg_idx = (u16)(((alu->dst_reg_id - GPR_FLG_IDX) << 4) +
				alu->dst_start);

		if (flg_idx >= 64) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Invalid flag %d\n",
				  flg_idx);
			return;
		}
		_flg_add(rt, flg_idx, val);
	} else {
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Unexpected Dest Register Bit set, RegisterID %d Start %d\n",
			  alu->dst_reg_id, alu->dst_start);
	}
}

static void _alu_exe(struct ice_parser_rt *rt, struct ice_alu *alu)
{
	u16 dst, src, shift, imm;

	if (alu->shift_xlate_select) {
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "shift_xlate_select != 0 is not expected\n");
		return;
	}

	_po_update(rt, alu);
	_flg_update(rt, alu);

	dst = rt->gpr[alu->dst_reg_id];
	src = _reg_bit_sel(rt, alu->src_reg_id, alu->src_start, alu->src_len);
	shift = alu->shift_xlate_key;
	imm = alu->imm;

	switch (alu->opc) {
	case ICE_ALU_PARK:
		break;
	case ICE_ALU_MOV_ADD:
		dst = (u16)((src << shift) + imm);
		_gpr_add(rt, alu->dst_reg_id, dst);
		break;
	case ICE_ALU_ADD:
		dst += (u16)((src << shift) + imm);
		_gpr_add(rt, alu->dst_reg_id, dst);
		break;
	case ICE_ALU_ORLT:
		if (src < imm)
			_dst_reg_bit_set(rt, alu, true);
		_gpr_add(rt, GPR_NP_IDX, alu->branch_addr);
		break;
	case ICE_ALU_OREQ:
		if (src == imm)
			_dst_reg_bit_set(rt, alu, true);
		_gpr_add(rt, GPR_NP_IDX, alu->branch_addr);
		break;
	case ICE_ALU_SETEQ:
		if (src == imm)
			_dst_reg_bit_set(rt, alu, true);
		else
			_dst_reg_bit_set(rt, alu, false);
		_gpr_add(rt, GPR_NP_IDX, alu->branch_addr);
		break;
	case ICE_ALU_MOV_XOR:
		dst = (u16)((u16)(src << shift) ^ (u16)imm);
		_gpr_add(rt, alu->dst_reg_id, dst);
		break;
	default:
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Unsupported ALU instruction %d\n",
			  alu->opc);
		break;
	}
}

static void _alu0_exe(struct ice_parser_rt *rt)
{
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU0 ...\n");
	_alu_exe(rt, rt->alu0);
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU0 done.\n");
}

static void _alu1_exe(struct ice_parser_rt *rt)
{
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU1 ...\n");
	_alu_exe(rt, rt->alu1);
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU1 done.\n");
}

static void _alu2_exe(struct ice_parser_rt *rt)
{
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU2 ...\n");
	_alu_exe(rt, rt->alu2);
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Executing ALU2 done.\n");
}

static void _pu_exe(struct ice_parser_rt *rt)
{
	struct ice_gpr_pu *pu = &rt->pu;
	int i;

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Updating Registers ...\n");

	for (i = 0; i < ICE_PARSER_GPR_NUM; i++) {
		if (pu->gpr_val_upd[i])
			_rt_gpr_set(rt, i, pu->gpr_val[i]);
	}

	for (i = 0; i < 64; i++) {
		if (pu->flg_msk & (1ul << i))
			_rt_flag_set(rt, i, pu->flg_val & (1ul << i));
	}

	for (i = 0; i < 16; i++) {
		if (pu->err_msk & (1u << 1))
			_rt_err_set(rt, i, pu->err_val & (1u << i));
	}

	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Updating Registers done.\n");
}

static void _alu_pg_exe(struct ice_parser_rt *rt)
{
	ice_memset(&rt->pu, 0, sizeof(rt->pu), ICE_NONDMA_MEM);

	if (rt->pg == 0) {
		_pg_exe(rt);
		_alu0_exe(rt);
		_alu1_exe(rt);
		_alu2_exe(rt);
	} else if (rt->pg == 1) {
		_alu0_exe(rt);
		_pg_exe(rt);
		_alu1_exe(rt);
		_alu2_exe(rt);
	} else if (rt->pg == 2) {
		_alu0_exe(rt);
		_alu1_exe(rt);
		_pg_exe(rt);
		_alu2_exe(rt);
	} else if (rt->pg == 3) {
		_alu0_exe(rt);
		_alu1_exe(rt);
		_alu2_exe(rt);
		_pg_exe(rt);
	}

	_pu_exe(rt);

	if (rt->action->ho_inc == 0)
		return;

	if (rt->action->ho_polarity)
		_rt_ho_set(rt, rt->gpr[GPR_HO_IDX] + rt->action->ho_inc);
	else
		_rt_ho_set(rt, rt->gpr[GPR_HO_IDX] - rt->action->ho_inc);
}

static void _proto_off_update(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	int i;

	if (rt->action->is_pg) {
		struct ice_proto_grp_item *proto_grp =
			&psr->proto_grp_table[rt->action->proto_id];
		u16 po;

		for (i = 0; i < 8; i++) {
			struct ice_proto_off *entry = &proto_grp->po[i];

			if (entry->proto_id == 0xff)
				break;

			if (!entry->polarity)
				po = (u16)(rt->po + entry->offset);
			else
				po = (u16)(rt->po - entry->offset);

			rt->protocols[entry->proto_id] = true;
			rt->offsets[entry->proto_id] = po;

			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set Protocol %d at offset %d\n",
				  entry->proto_id, po);
		}
	} else {
		rt->protocols[rt->action->proto_id] = true;
		rt->offsets[rt->action->proto_id] = rt->po;
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set Protocol %d at offset %d\n",
			  rt->action->proto_id, rt->po);
	}
}

static void _marker_set(struct ice_parser_rt *rt, int idx)
{
	int x = idx / 8;
	int y = idx % 8;

	rt->markers[x] |= (u8)(1u << y);
}

static void _marker_update(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	int i;

	if (rt->action->is_mg) {
		struct ice_mk_grp_item *mk_grp =
			&psr->mk_grp_table[rt->action->marker_id];

		for (i = 0; i < 8; i++) {
			u8 marker = mk_grp->markers[i];

			if (marker == 71)
				break;

			_marker_set(rt, marker);
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set Marker %d\n",
				  marker);
		}
	} else {
		if (rt->action->marker_id != 71)
			_marker_set(rt, rt->action->marker_id);
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Set Marker %d\n",
			  rt->action->marker_id);
	}
}

static u16 _ptype_resolve(struct ice_parser_rt *rt)
{
	struct ice_parser *psr = rt->psr;
	struct ice_ptype_mk_tcam_item *item;

	item = ice_ptype_mk_tcam_match(psr->ptype_mk_tcam_table,
				       rt->markers, 9);
	if (item)
		return item->ptype;
	return 0xffff;
}

static void _proto_off_resolve(struct ice_parser_rt *rt,
			       struct ice_parser_result *rslt)
{
	int i;

	for (i = 0; i < 255; i++) {
		if (rt->protocols[i]) {
			rslt->po[rslt->po_num].proto_id = (u8)i;
			rslt->po[rslt->po_num].offset = rt->offsets[i];
			rslt->po_num++;
		}
	}
}

static void _result_resolve(struct ice_parser_rt *rt,
			    struct ice_parser_result *rslt)
{
	struct ice_parser *psr = rt->psr;

	ice_memset(rslt, 0, sizeof(*rslt), ICE_NONDMA_MEM);

	rslt->ptype = _ptype_resolve(rt);

	ice_memcpy(&rslt->flags_psr, &rt->gpr[GPR_FLG_IDX], 8,
		   ICE_NONDMA_TO_NONDMA);
	rslt->flags_pkt = ice_flg_redirect(psr->flg_rd_table, rslt->flags_psr);
	rslt->flags_sw = ice_xlt_kb_flag_get(psr->xlt_kb_sw, rslt->flags_pkt);
	rslt->flags_fd = ice_xlt_kb_flag_get(psr->xlt_kb_fd, rslt->flags_pkt);
	rslt->flags_rss = ice_xlt_kb_flag_get(psr->xlt_kb_rss, rslt->flags_pkt);

	_proto_off_resolve(rt, rslt);
}

/**
 * ice_parser_rt_execute - parser execution routine
 * @rt: pointer to the parser runtime
 * @rslt: input/output parameter to save parser result
 */
enum ice_status ice_parser_rt_execute(struct ice_parser_rt *rt,
				      struct ice_parser_result *rslt)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_pg_nm_cam_item *pg_nm_cam;
	struct ice_parser *psr = rt->psr;
	struct ice_pg_cam_item *pg_cam;
	struct ice_bst_tcam_item *bst;
	struct ice_imem_item *imem;
	u16 node;
	u16 pc;

	node = rt->gpr[GPR_NN_IDX];
	ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Start with Node: %d\n", node);

	while (true) {
		pc = rt->gpr[GPR_NP_IDX];
		imem = &psr->imem_table[pc];
		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Load imem at pc: %d\n",
			  pc);

		_bst_key_init(rt, imem);
		bst = ice_bst_tcam_match(psr->bst_tcam_table, rt->bst_key);

		if (!bst) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "No Boost TCAM Match\n");
			_imem_pgk_init(rt, imem);
			_imem_alu0_set(rt, imem);
			_imem_alu1_set(rt, imem);
			_imem_alu2_set(rt, imem);
			_imem_pgp_set(rt, imem);
		} else {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Boost TCAM Match address: %d\n",
				  bst->address);
			if (imem->b_m.pg) {
				_bst_pgk_init(rt, bst);
				_bst_pgp_set(rt, bst);
			} else {
				_imem_pgk_init(rt, imem);
				_imem_pgp_set(rt, imem);
			}

			if (imem->b_m.al0)
				_bst_alu0_set(rt, bst);
			else
				_imem_alu0_set(rt, imem);

			if (imem->b_m.al1)
				_bst_alu1_set(rt, bst);
			else
				_imem_alu1_set(rt, imem);

			if (imem->b_m.al2)
				_bst_alu2_set(rt, bst);
			else
				_imem_alu2_set(rt, imem);
		}

		rt->action = NULL;
		pg_cam = _pg_cam_match(rt);
		if (!pg_cam) {
			pg_nm_cam = _pg_nm_cam_match(rt);
			if (pg_nm_cam) {
				ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Match ParseGraph Nomatch CAM Address %d\n",
					  pg_nm_cam->idx);
				rt->action = &pg_nm_cam->action;
			}
		} else {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Match ParseGraph CAM Address %d\n",
				  pg_cam->idx);
			rt->action = &pg_cam->action;
		}

		if (!rt->action) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Failed to match ParseGraph CAM, stop parsing.\n");
			status = ICE_ERR_PARAM;
			break;
		}

		_alu_pg_exe(rt);
		_marker_update(rt);
		_proto_off_update(rt);

		ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Go to node %d\n",
			  rt->action->next_node);

		if (rt->action->is_last_round) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Last Round in ParseGraph Action, stop parsing.\n");
			break;
		}

		if (rt->gpr[GPR_HO_IDX] >= rt->pkt_len) {
			ice_debug(rt->psr->hw, ICE_DBG_PARSER, "Header Offset %d is larger than packet len %d, stop parsing\n",
				  rt->gpr[GPR_HO_IDX], rt->pkt_len);
			break;
		}
	}

	_result_resolve(rt, rslt);

	return status;
}
