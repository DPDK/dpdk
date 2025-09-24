/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __CREATE_ELEMENTS_H__
#define __CREATE_ELEMENTS_H__

#include "stdint.h"

#include "stream_binary_flow_api.h"
#include <rte_flow.h>

#define MAX_ELEMENTS 64
#define MAX_ACTIONS 32

struct cnv_match_s {
	struct rte_flow_item rte_flow_item[MAX_ELEMENTS];
};

struct cnv_attr_s {
	struct cnv_match_s match;
	struct rte_flow_attr attr;
	uint16_t forced_vlan_vid;
	uint16_t caller_id;
};

struct cnv_action_s {
	struct rte_flow_action flow_actions[MAX_ACTIONS];
	struct rte_flow_action_rss flow_rss;
	struct flow_action_raw_encap encap;
	struct flow_action_raw_decap decap;
	struct rte_flow_action_queue queue;
};

/*
 * Only needed because it eases the use of statistics through NTAPI
 * for faster integration into NTAPI version of driver
 * Therefore, this is only a good idea when running on a temporary NTAPI
 * The query() functionality must go to flow engine, when moved to Open Source driver
 */

struct rte_flow {
	void *flw_hdl;
	int used;

	uint32_t flow_stat_id;

	uint64_t stat_pkts;
	uint64_t stat_bytes;
	uint8_t stat_tcp_flags;

	uint16_t caller_id;
};

enum nt_rte_flow_item_type {
	NT_RTE_FLOW_ITEM_TYPE_END = INT_MIN,
	NT_RTE_FLOW_ITEM_TYPE_TUNNEL,
};

extern rte_spinlock_t flow_lock;

#endif	/* __CREATE_ELEMENTS_H__ */
