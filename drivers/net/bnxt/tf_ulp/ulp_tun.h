/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TUN_H_
#define _BNXT_TUN_H_

#include <inttypes.h>
#include <stdbool.h>

#include "rte_version.h"
#include "rte_ethdev.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"

struct bnxt_tun_cache_entry {
	bool				t_dst_ip_valid;
	uint8_t				t_dmac[RTE_ETHER_ADDR_LEN];
	union {
		rte_be32_t		t_dst_ip;
		uint8_t			t_dst_ip6[16];
	};
	uint32_t			outer_tun_flow_id;
};

struct bnxt_flow_app_tun_ent {
	struct rte_flow_tunnel			app_tunnel;
	uint32_t				tun_id;
	uint32_t				ref_cnt;
	struct rte_flow_action			action;
	struct rte_flow_item			item;
};

int32_t
ulp_app_tun_search_entry(struct bnxt_ulp_context *ulp_ctx,
			 struct rte_flow_tunnel *app_tunnel,
			 struct bnxt_flow_app_tun_ent **tun_entry);

void
ulp_app_tun_entry_delete(struct bnxt_flow_app_tun_ent *tun_entry);

int32_t
ulp_app_tun_entry_set_decap_action(struct bnxt_flow_app_tun_ent *tun_entry);

int32_t
ulp_app_tun_entry_set_decap_item(struct bnxt_flow_app_tun_ent *tun_entry);

struct bnxt_flow_app_tun_ent *
ulp_app_tun_match_entry(struct bnxt_ulp_context *ulp_ctx, const void *ctx);

/* Tunnel API to delete the tunnel entry */
void
ulp_tunnel_offload_entry_clear(struct bnxt_tun_cache_entry *tun_tbl,
			       uint8_t tun_idx);

int32_t
ulp_tunnel_offload_process(struct ulp_rte_parser_params *params);
#endif
