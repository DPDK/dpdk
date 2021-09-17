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

static void _rt_flag_set(struct ice_parser_rt *rt, int idx)
{
	int y = idx / 16;
	int x = idx % 16;

	rt->gpr[GPR_FLG_IDX + y] |= (u16)(1 << x);
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

	for (i = 0; i < 64; i++) {
		if ((mi->flags & (1ul << i)) != 0ul)
			_rt_flag_set(rt, i);
	}

	rt->psr = psr;
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

	ice_memcpy(&rt->gpr[GPR_HB_IDX], &rt->pkt_buf[ho], 32,
		   ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_parser_rt_execute - parser execution routine
 * @rt: pointer to the parser runtime
 * @rslt: input/output parameter to save parser result
 */
enum ice_status ice_parser_rt_execute(struct ice_parser_rt *rt,
				      struct ice_parser_result *rslt)
{
	return ICE_ERR_NOT_IMPL;
}
