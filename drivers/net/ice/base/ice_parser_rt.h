/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_PARSER_RT_H_
#define _ICE_PARSER_RT_H_

struct ice_parser_ctx;

#define ICE_PARSER_MAX_PKT_LEN 504
#define ICE_PARSER_GPR_NUM 128

struct ice_parser_rt {
	struct ice_parser *psr;
	u16 gpr[ICE_PARSER_GPR_NUM];
	u8 pkt_buf[ICE_PARSER_MAX_PKT_LEN + 32];
	u16 pkt_len;
	u16 po;
};

void ice_parser_rt_reset(struct ice_parser_rt *rt);
void ice_parser_rt_pktbuf_set(struct ice_parser_rt *rt, const u8 *pkt_buf,
			      int pkt_len);

struct ice_parser_result;
enum ice_status ice_parser_rt_execute(struct ice_parser_rt *rt,
				      struct ice_parser_result *rslt);
#endif /* _ICE_PARSER_RT_H_ */
