/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TUN_H_
#define _BNXT_TUN_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "rte_ethdev.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"

#define	BNXT_OUTER_TUN_SIGNATURE(l3_tun, params)		\
	((l3_tun) &&					\
	 ULP_BITMAP_ISSET((params)->act_bitmap.bits,	\
			  BNXT_ULP_ACTION_BIT_JUMP))
#define	BNXT_INNER_TUN_SIGNATURE(l3_tun, l3_tun_decap, params)		\
	((l3_tun) && (l3_tun_decap) &&					\
	 !ULP_BITMAP_ISSET((params)->hdr_bitmap.bits,			\
			   BNXT_ULP_HDR_BIT_O_ETH))

#define	BNXT_FIRST_INNER_TUN_FLOW(state, inner_tun_sig)	\
	((state) == BNXT_ULP_FLOW_STATE_NORMAL && (inner_tun_sig))
#define	BNXT_INNER_TUN_FLOW(state, inner_tun_sig)		\
	((state) == BNXT_ULP_FLOW_STATE_TUN_O_OFFLD && (inner_tun_sig))
#define	BNXT_OUTER_TUN_FLOW(outer_tun_sig)		((outer_tun_sig))

/* It is invalid to get another outer flow offload request
 * for the same tunnel, while the outer flow is already offloaded.
 */
#define	BNXT_REJECT_OUTER_TUN_FLOW(state, outer_tun_sig)	\
	((state) == BNXT_ULP_FLOW_STATE_TUN_O_OFFLD && (outer_tun_sig))
/* It is invalid to get another inner flow offload request
 * for the same tunnel, while the outer flow is not yet offloaded.
 */
#define	BNXT_REJECT_INNER_TUN_FLOW(state, inner_tun_sig)	\
	((state) == BNXT_ULP_FLOW_STATE_TUN_I_CACHED && (inner_tun_sig))

#define	ULP_TUN_O_DMAC_HDR_FIELD_INDEX	1
#define	ULP_TUN_O_IPV4_DIP_INDEX	19
#define	ULP_TUN_O_IPV6_DIP_INDEX	17

/* When a flow offload request comes the following state transitions
 * happen based on the order in which the outer & inner flow offload
 * requests arrive.
 *
 * If inner tunnel flow offload request arrives first then the flow
 * state will change from BNXT_ULP_FLOW_STATE_NORMAL to
 * BNXT_ULP_FLOW_STATE_TUN_I_CACHED and the following outer tunnel
 * flow offload request will change the state of the flow to
 * BNXT_ULP_FLOW_STATE_TUN_O_OFFLD from BNXT_ULP_FLOW_STATE_TUN_I_CACHED.
 *
 * If outer tunnel flow offload request arrives first then the flow state
 * will change from BNXT_ULP_FLOW_STATE_NORMAL to
 * BNXT_ULP_FLOW_STATE_TUN_O_OFFLD.
 *
 * Once the flow state is in BNXT_ULP_FLOW_STATE_TUN_O_OFFLD, any inner
 * tunnel flow offload requests after that point will be treated as a
 * normal flow and the tunnel flow state remains in
 * BNXT_ULP_FLOW_STATE_TUN_O_OFFLD
 */
enum bnxt_ulp_tun_flow_state {
	BNXT_ULP_FLOW_STATE_NORMAL = 0,
	BNXT_ULP_FLOW_STATE_TUN_O_OFFLD,
	BNXT_ULP_FLOW_STATE_TUN_I_CACHED
};

struct ulp_per_port_flow_info {
	enum bnxt_ulp_tun_flow_state	state;
	uint32_t			first_tun_i_fid;
	struct ulp_rte_parser_params	first_inner_tun_params;
};

struct bnxt_tun_cache_entry {
	bool				valid;
	bool				t_dst_ip_valid;
	uint8_t				t_dmac[RTE_ETHER_ADDR_LEN];
	union {
		rte_be32_t		t_dst_ip;
		uint8_t			t_dst_ip6[16];
	};
	uint32_t			outer_tun_flow_id;
	uint16_t			outer_tun_rej_cnt;
	uint16_t			inner_tun_rej_cnt;
	struct ulp_per_port_flow_info	tun_flow_info[RTE_MAX_ETHPORTS];
};

void
ulp_clear_tun_entry(struct bnxt_tun_cache_entry *tun_tbl, uint8_t tun_idx);

void
ulp_clear_tun_inner_entry(struct bnxt_tun_cache_entry *tun_tbl, uint32_t fid);

#endif
