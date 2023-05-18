/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_PARSER_RT_H_
#define _ICE_PARSER_RT_H_

struct ice_parser_ctx;

#define ICE_PARSER_MAX_PKT_LEN 504
#define ICE_PARSER_GPR_NUM 128
#define ICE_PARSER_HDR_BUF_LEN 32
#define ICE_PARSER_BST_KEY_LEN 20
#define ICE_PARSER_MARKER_NUM_IN_BYTES 9 /* 72 bits */
#define ICE_PARSER_PROTO_NUM 256

struct ice_gpr_pu {
	/* flag to indicate if GRP needs to be updated */
	bool gpr_val_upd[ICE_PARSER_GPR_NUM];
	u16 gpr_val[ICE_PARSER_GPR_NUM];
	u64 flg_msk;
	u64 flg_val;
	u16 err_msk;
	u16 err_val;
};

struct ice_parser_rt {
	struct ice_parser *psr;
	u16 gpr[ICE_PARSER_GPR_NUM];
	u8 pkt_buf[ICE_PARSER_MAX_PKT_LEN + ICE_PARSER_HDR_BUF_LEN];
	u16 pkt_len;
	u16 po;
	u8 bst_key[ICE_PARSER_BST_KEY_LEN];
	struct ice_pg_cam_key pg_key;
	struct ice_alu *alu0;
	struct ice_alu *alu1;
	struct ice_alu *alu2;
	struct ice_pg_cam_action *action;
	u8 pg;
	struct ice_gpr_pu pu;
	u8 markers[ICE_PARSER_MARKER_NUM_IN_BYTES];
	bool protocols[ICE_PARSER_PROTO_NUM];
	u16 offsets[ICE_PARSER_PROTO_NUM];
};

void ice_parser_rt_reset(struct ice_parser_rt *rt);
void ice_parser_rt_pktbuf_set(struct ice_parser_rt *rt, const u8 *pkt_buf,
			      int pkt_len);

struct ice_parser_result;
enum ice_status ice_parser_rt_execute(struct ice_parser_rt *rt,
				      struct ice_parser_result *rslt);
#endif /* _ICE_PARSER_RT_H_ */
