/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2019 Cisco Systems, Inc.  All rights reserved.
 */

#include <errno.h>
#include <stdint.h>
#include <rte_log.h>
#include <rte_ethdev_driver.h>
#include <rte_flow_driver.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_memzone.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_nic.h"

#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP6_VTC_FLOW 0x60000000

/* Highest Item type supported by Flowman */
#define FM_MAX_ITEM_TYPE RTE_FLOW_ITEM_TYPE_VXLAN

/* Up to 1024 TCAM entries */
#define FM_MAX_TCAM_TABLE_SIZE 1024

/* Up to 4096 entries per exact match table */
#define FM_MAX_EXACT_TABLE_SIZE 4096

/* Number of counters to increase on for each increment */
#define FM_COUNTERS_EXPAND  100

#define FM_INVALID_HANDLE 0

/* Low priority used for implicit VF -> representor flow */
#define FM_LOWEST_PRIORITY 100000

/* High priority used for implicit representor -> VF flow */
#define FM_HIGHEST_PRIORITY 0

/* Tag used for implicit VF <-> representor flows */
#define FM_VF_REP_TAG 1

/* Max number of actions supported by VIC is 2K. Make hash table double that. */
#define FM_MAX_ACTION_TABLE_SIZE 4096

/*
 * Flow exact match tables (FET) in the VIC and rte_flow groups.
 * Use a simple scheme to map groups to tables.
 * Group 0 uses the single TCAM tables, one for each direction.
 * Group 1, 2, ... uses its own exact match table.
 *
 * The TCAM tables are allocated upfront during init.
 *
 * Exact match tables are allocated on demand. 3 paths that lead allocations.
 *
 * 1. Add a flow that jumps from group 0 to group N.
 *
 * If N does not exist, we allocate an exact match table for it, using
 * a dummy key. A key is required for the table.
 *
 * 2. Add a flow that uses group N.
 *
 * If N does not exist, we allocate an exact match table for it, using
 * the flow's key. Subsequent flows to the same group all should have
 * the same key.
 *
 * Without a jump flow to N, N is not reachable in hardware. No packets
 * reach N and match.
 *
 * 3. Add a flow to an empty group N.
 *
 * N has been created via (1) and the dummy key. We free that table, allocate
 * a new table using the new flow's key. Also re-do the existing jump flow to
 * point to the new table.
 */
#define FM_TCAM_RTE_GROUP 0

struct enic_fm_fet {
	TAILQ_ENTRY(enic_fm_fet) list;
	uint32_t group; /* rte_flow group ID */
	uint64_t handle; /* Exact match table handle from flowman */
	uint8_t ingress;
	uint8_t default_key;
	int ref; /* Reference count via get/put */
	struct fm_key_template key; /* Key associated with the table */
};

struct enic_fm_counter {
	SLIST_ENTRY(enic_fm_counter) next;
	uint32_t handle;
};

struct enic_fm_action {
	int ref;
	uint64_t handle;
	struct fm_action key;
};

/* rte_flow.fm */
struct enic_fm_flow {
	bool counter_valid;
	uint64_t entry_handle;
	struct enic_fm_action  *action;
	struct enic_fm_counter *counter;
	struct enic_fm_fet *fet;
	/* Auto-added steer action for hairpin flows (e.g. vnic->vnic) */
	struct enic_fm_flow *hairpin_steer_flow;
};

struct enic_fm_jump_flow {
	TAILQ_ENTRY(enic_fm_jump_flow) list;
	struct rte_flow *flow;
	uint32_t group;
	struct fm_tcam_match_entry match;
	struct fm_action action;
};

/*
 * Flowman uses host memory for commands. This structure is allocated
 * in DMA-able memory.
 */
union enic_flowman_cmd_mem {
	struct fm_tcam_match_table fm_tcam_match_table;
	struct fm_exact_match_table fm_exact_match_table;
	struct fm_tcam_match_entry fm_tcam_match_entry;
	struct fm_exact_match_entry fm_exact_match_entry;
	struct fm_action fm_action;
};

/*
 * PF has a flowman instance, and VF representors share it with PF.
 * PF allocates this structure and owns it. VF representors borrow
 * the PF's structure during API calls (e.g. create, query).
 */
struct enic_flowman {
	struct enic *owner_enic; /* PF */
	struct enic *user_enic;  /* API caller (PF or representor) */
	/*
	 * Representors and PF share the same underlying flowman.
	 * Lock API calls to serialize accesses from them. Only used
	 * when VF representors are present.
	 */
	rte_spinlock_t lock;
	/* Command buffer */
	struct {
		union enic_flowman_cmd_mem *va;
		dma_addr_t pa;
	} cmd;
	/* TCAM tables allocated upfront, used for group 0 */
	uint64_t ig_tcam_hndl;
	uint64_t eg_tcam_hndl;
	/* Counters */
	SLIST_HEAD(enic_free_counters, enic_fm_counter) counters;
	void *counter_stack;
	uint32_t counters_alloced;
	/* Exact match tables for groups != 0, dynamically allocated */
	TAILQ_HEAD(fet_list, enic_fm_fet) fet_list;
	/*
	 * Default exact match tables used for jump actions to
	 * non-existent groups.
	 */
	struct enic_fm_fet *default_eg_fet;
	struct enic_fm_fet *default_ig_fet;
	/* hash table for Action reuse */
	struct rte_hash *action_hash;
	/* Flows that jump to the default table above */
	TAILQ_HEAD(jump_flow_list, enic_fm_jump_flow) jump_list;
	/*
	 * Scratch data used during each invocation of flow_create
	 * and flow_validate.
	 */
	struct enic_fm_fet *fet;
	struct fm_tcam_match_entry tcam_entry;
	struct fm_action action;
	struct fm_action action_tmp; /* enic_fm_reorder_action_op */
	int action_op_count;
	/* Tags used for representor flows */
	uint8_t vf_rep_tag;
	/* For auto-added steer action for hairpin */
	int need_hairpin_steer;
	uint64_t hairpin_steer_vnic_h;
};

static int enic_fm_tbl_free(struct enic_flowman *fm, uint64_t handle);
/*
 * API functions (create, destroy, validate, flush) call begin_fm()
 * upon entering to save the caller enic (PF or VF representor) and
 * lock. Upon exit, they call end_fm() to unlock.
 */
static struct enic_flowman *begin_fm(struct enic *enic);
static void end_fm(struct enic_flowman *fm);
/* Delete internal flows created for representor paths */
static void delete_rep_flows(struct enic *enic);

/*
 * Common arguments passed to copy_item functions. Use this structure
 * so we can easily add new arguments.
 * item: Item specification.
 * fm_tcam_entry: Flowman TCAM match entry.
 * header_level: 0 for outer header, 1 for inner header.
 */
struct copy_item_args {
	const struct rte_flow_item *item;
	struct fm_tcam_match_entry *fm_tcam_entry;
	uint8_t header_level;
};

/* functions for copying items into flowman match */
typedef int (enic_copy_item_fn)(struct copy_item_args *arg);

/* Info about how to copy items into flowman match */
struct enic_fm_items {
	/* Function for copying and validating an item. */
	enic_copy_item_fn * const copy_item;
	/* List of valid previous items. */
	const enum rte_flow_item_type * const prev_items;
	/*
	 * True if it's OK for this item to be the first item. For some NIC
	 * versions, it's invalid to start the stack above layer 3.
	 */
	const uint8_t valid_start_item;
};

static enic_copy_item_fn enic_fm_copy_item_eth;
static enic_copy_item_fn enic_fm_copy_item_ipv4;
static enic_copy_item_fn enic_fm_copy_item_ipv6;
static enic_copy_item_fn enic_fm_copy_item_raw;
static enic_copy_item_fn enic_fm_copy_item_sctp;
static enic_copy_item_fn enic_fm_copy_item_tcp;
static enic_copy_item_fn enic_fm_copy_item_udp;
static enic_copy_item_fn enic_fm_copy_item_vlan;
static enic_copy_item_fn enic_fm_copy_item_vxlan;

/* Ingress actions */
static const enum rte_flow_action_type enic_fm_supported_ig_actions[] = {
	RTE_FLOW_ACTION_TYPE_COUNT,
	RTE_FLOW_ACTION_TYPE_DROP,
	RTE_FLOW_ACTION_TYPE_FLAG,
	RTE_FLOW_ACTION_TYPE_JUMP,
	RTE_FLOW_ACTION_TYPE_MARK,
	RTE_FLOW_ACTION_TYPE_OF_POP_VLAN,
	RTE_FLOW_ACTION_TYPE_PORT_ID,
	RTE_FLOW_ACTION_TYPE_PASSTHRU,
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_RSS,
	RTE_FLOW_ACTION_TYPE_VOID,
	RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP,
	RTE_FLOW_ACTION_TYPE_VXLAN_DECAP,
	RTE_FLOW_ACTION_TYPE_END, /* END must be the last entry */
};

/* Egress actions */
static const enum rte_flow_action_type enic_fm_supported_eg_actions[] = {
	RTE_FLOW_ACTION_TYPE_COUNT,
	RTE_FLOW_ACTION_TYPE_DROP,
	RTE_FLOW_ACTION_TYPE_JUMP,
	RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP,
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,
	RTE_FLOW_ACTION_TYPE_PORT_ID,
	RTE_FLOW_ACTION_TYPE_PASSTHRU,
	RTE_FLOW_ACTION_TYPE_VOID,
	RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP,
	RTE_FLOW_ACTION_TYPE_END,
};

static const struct enic_fm_items enic_fm_items[] = {
	[RTE_FLOW_ITEM_TYPE_RAW] = {
		.copy_item = enic_fm_copy_item_raw,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.copy_item = enic_fm_copy_item_eth,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.copy_item = enic_fm_copy_item_vlan,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_fm_copy_item_ipv4,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.copy_item = enic_fm_copy_item_ipv6,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_fm_copy_item_udp,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_fm_copy_item_tcp,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.copy_item = enic_fm_copy_item_sctp,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.copy_item = enic_fm_copy_item_vxlan,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
};

static int
enic_fm_copy_item_eth(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	/* Match all if no spec */
	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_eth_mask;
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_header_select |= FKH_ETHER;
	fm_mask->fk_header_select |= FKH_ETHER;
	memcpy(&fm_data->l2.eth, spec, sizeof(struct rte_ether_hdr));
	memcpy(&fm_mask->l2.eth, mask, sizeof(struct rte_ether_hdr));
	return 0;
}

static int
enic_fm_copy_item_vlan(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;
	struct rte_ether_hdr *eth_mask;
	struct rte_ether_hdr *eth_val;
	uint32_t meta;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	/* Outer and inner packet vlans need different flags */
	meta = FKM_VLAN_PRES;
	if (lvl > 0)
		meta = FKM_QTAG;
	fm_data->fk_metadata |= meta;
	fm_mask->fk_metadata |= meta;

	/* Match all if no spec */
	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_vlan_mask;

	eth_mask = (void *)&fm_mask->l2.eth;
	eth_val = (void *)&fm_data->l2.eth;

	/*
	 * Outer TPID cannot be matched. If inner_type is 0, use what is
	 * in the eth header.
	 */
	if (eth_mask->ether_type && mask->inner_type)
		return -ENOTSUP;

	/*
	 * When packet matching, the VIC always compares vlan-stripped
	 * L2, regardless of vlan stripping settings. So, the inner type
	 * from vlan becomes the ether type of the eth header.
	 */
	if (mask->inner_type) {
		eth_mask->ether_type = mask->inner_type;
		eth_val->ether_type = spec->inner_type;
	}
	fm_data->fk_header_select |= FKH_ETHER | FKH_QTAG;
	fm_mask->fk_header_select |= FKH_ETHER | FKH_QTAG;
	fm_data->fk_vlan = rte_be_to_cpu_16(spec->tci);
	fm_mask->fk_vlan = rte_be_to_cpu_16(mask->tci);
	return 0;
}

static int
enic_fm_copy_item_ipv4(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_metadata |= FKM_IPV4;
	fm_mask->fk_metadata |= FKM_IPV4;

	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_ipv4_mask;

	fm_data->fk_header_select |= FKH_IPV4;
	fm_mask->fk_header_select |= FKH_IPV4;
	memcpy(&fm_data->l3.ip4, spec, sizeof(*spec));
	memcpy(&fm_mask->l3.ip4, mask, sizeof(*mask));
	return 0;
}

static int
enic_fm_copy_item_ipv6(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_metadata |= FKM_IPV6;
	fm_mask->fk_metadata |= FKM_IPV6;

	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;

	fm_data->fk_header_select |= FKH_IPV6;
	fm_mask->fk_header_select |= FKH_IPV6;
	memcpy(&fm_data->l3.ip6, spec, sizeof(struct rte_ipv6_hdr));
	memcpy(&fm_mask->l3.ip6, mask, sizeof(struct rte_ipv6_hdr));
	return 0;
}

static int
enic_fm_copy_item_udp(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_metadata |= FKM_UDP;
	fm_mask->fk_metadata |= FKM_UDP;

	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_udp_mask;

	fm_data->fk_header_select |= FKH_UDP;
	fm_mask->fk_header_select |= FKH_UDP;
	memcpy(&fm_data->l4.udp, spec, sizeof(*spec));
	memcpy(&fm_mask->l4.udp, mask, sizeof(*mask));
	return 0;
}

static int
enic_fm_copy_item_tcp(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_metadata |= FKM_TCP;
	fm_mask->fk_metadata |= FKM_TCP;

	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_tcp_mask;

	fm_data->fk_header_select |= FKH_TCP;
	fm_mask->fk_header_select |= FKH_TCP;
	memcpy(&fm_data->l4.tcp, spec, sizeof(*spec));
	memcpy(&fm_mask->l4.tcp, mask, sizeof(*mask));
	return 0;
}

static int
enic_fm_copy_item_sctp(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_sctp *spec = item->spec;
	const struct rte_flow_item_sctp *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;
	uint8_t *ip_proto_mask = NULL;
	uint8_t *ip_proto = NULL;
	uint32_t l3_fkh;

	ENICPMD_FUNC_TRACE();
	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	/*
	 * The NIC filter API has no flags for "match sctp", so explicitly
	 * set the protocol number in the IP pattern.
	 */
	if (fm_data->fk_metadata & FKM_IPV4) {
		struct rte_ipv4_hdr *ip;
		ip = (struct rte_ipv4_hdr *)&fm_mask->l3.ip4;
		ip_proto_mask = &ip->next_proto_id;
		ip = (struct rte_ipv4_hdr *)&fm_data->l3.ip4;
		ip_proto = &ip->next_proto_id;
		l3_fkh = FKH_IPV4;
	} else if (fm_data->fk_metadata & FKM_IPV6) {
		struct rte_ipv6_hdr *ip;
		ip = (struct rte_ipv6_hdr *)&fm_mask->l3.ip6;
		ip_proto_mask = &ip->proto;
		ip = (struct rte_ipv6_hdr *)&fm_data->l3.ip6;
		ip_proto = &ip->proto;
		l3_fkh = FKH_IPV6;
	} else {
		/* Need IPv4/IPv6 pattern first */
		return -EINVAL;
	}
	*ip_proto = IPPROTO_SCTP;
	*ip_proto_mask = 0xff;
	fm_data->fk_header_select |= l3_fkh;
	fm_mask->fk_header_select |= l3_fkh;

	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_sctp_mask;

	fm_data->fk_header_select |= FKH_L4RAW;
	fm_mask->fk_header_select |= FKH_L4RAW;
	memcpy(fm_data->l4.rawdata, spec, sizeof(*spec));
	memcpy(fm_mask->l4.rawdata, mask, sizeof(*mask));
	return 0;
}

static int
enic_fm_copy_item_vxlan(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	/* Only 2 header levels (outer and inner) allowed */
	if (arg->header_level > 0)
		return -EINVAL;

	fm_data = &entry->ftm_data.fk_hdrset[0];
	fm_mask = &entry->ftm_mask.fk_hdrset[0];
	fm_data->fk_metadata |= FKM_VXLAN;
	fm_mask->fk_metadata |= FKM_VXLAN;
	/* items from here on out are inner header items */
	arg->header_level = 1;

	/* Match all if no spec */
	if (!spec)
		return 0;
	if (!mask)
		mask = &rte_flow_item_vxlan_mask;

	fm_data->fk_header_select |= FKH_VXLAN;
	fm_mask->fk_header_select |= FKH_VXLAN;
	memcpy(&fm_data->vxlan, spec, sizeof(*spec));
	memcpy(&fm_mask->vxlan, mask, sizeof(*mask));
	return 0;
}

/*
 * Currently, raw pattern match is very limited. It is intended for matching
 * UDP tunnel header (e.g. vxlan or geneve).
 */
static int
enic_fm_copy_item_raw(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	const struct rte_flow_item_raw *spec = item->spec;
	const struct rte_flow_item_raw *mask = item->mask;
	const uint8_t lvl = arg->header_level;
	struct fm_tcam_match_entry *entry = arg->fm_tcam_entry;
	struct fm_header_set *fm_data, *fm_mask;

	ENICPMD_FUNC_TRACE();
	/* Cannot be used for inner packet */
	if (lvl > 0)
		return -EINVAL;
	/* Need both spec and mask */
	if (!spec || !mask)
		return -EINVAL;
	/* Only supports relative with offset 0 */
	if (!spec->relative || spec->offset != 0 || spec->search ||
	    spec->limit)
		return -EINVAL;
	/* Need non-null pattern that fits within the NIC's filter pattern */
	if (spec->length == 0 ||
	    spec->length + sizeof(struct rte_udp_hdr) > FM_LAYER_SIZE ||
	    !spec->pattern || !mask->pattern)
		return -EINVAL;
	/*
	 * Mask fields, including length, are often set to zero. Assume that
	 * means "same as spec" to avoid breaking existing apps. If length
	 * is not zero, then it should be >= spec length.
	 *
	 * No more pattern follows this, so append to the L4 layer instead of
	 * L5 to work with both recent and older VICs.
	 */
	if (mask->length != 0 && mask->length < spec->length)
		return -EINVAL;

	fm_data = &entry->ftm_data.fk_hdrset[lvl];
	fm_mask = &entry->ftm_mask.fk_hdrset[lvl];
	fm_data->fk_header_select |= FKH_L4RAW;
	fm_mask->fk_header_select |= FKH_L4RAW;
	fm_data->fk_header_select &= ~FKH_UDP;
	fm_mask->fk_header_select &= ~FKH_UDP;
	memcpy(fm_data->l4.rawdata + sizeof(struct rte_udp_hdr),
	       spec->pattern, spec->length);
	memcpy(fm_mask->l4.rawdata + sizeof(struct rte_udp_hdr),
	       mask->pattern, spec->length);
	return 0;
}

static int
flowman_cmd(struct enic_flowman *fm, uint64_t *args, int nargs)
{
	return vnic_dev_flowman_cmd(fm->owner_enic->vdev, args, nargs);
}

static int
enic_fet_alloc(struct enic_flowman *fm, uint8_t ingress,
	       struct fm_key_template *key, int entries,
	       struct enic_fm_fet **fet_out)
{
	struct fm_exact_match_table *cmd;
	struct fm_header_set *hdr;
	struct enic_fm_fet *fet;
	uint64_t args[3];
	int ret;

	ENICPMD_FUNC_TRACE();
	fet = calloc(1, sizeof(struct enic_fm_fet));
	if (fet == NULL)
		return -ENOMEM;
	cmd = &fm->cmd.va->fm_exact_match_table;
	memset(cmd, 0, sizeof(*cmd));
	cmd->fet_direction = ingress ? FM_INGRESS : FM_EGRESS;
	cmd->fet_stage = FM_STAGE_LAST;
	cmd->fet_max_entries = entries ? entries : FM_MAX_EXACT_TABLE_SIZE;
	if (key == NULL) {
		hdr = &cmd->fet_key.fk_hdrset[0];
		memset(hdr, 0, sizeof(*hdr));
		hdr->fk_header_select = FKH_IPV4 | FKH_UDP;
		hdr->l3.ip4.fk_saddr = 0xFFFFFFFF;
		hdr->l3.ip4.fk_daddr = 0xFFFFFFFF;
		hdr->l4.udp.fk_source = 0xFFFF;
		hdr->l4.udp.fk_dest = 0xFFFF;
		fet->default_key = 1;
	} else {
		memcpy(&cmd->fet_key, key, sizeof(*key));
		memcpy(&fet->key, key, sizeof(*key));
		fet->default_key = 0;
	}
	cmd->fet_key.fk_packet_tag = 1;

	args[0] = FM_EXACT_TABLE_ALLOC;
	args[1] = fm->cmd.pa;
	ret = flowman_cmd(fm, args, 2);
	if (ret) {
		ENICPMD_LOG(ERR, "cannot alloc exact match table: rc=%d", ret);
		free(fet);
		return ret;
	}
	fet->handle = args[0];
	fet->ingress = ingress;
	ENICPMD_LOG(DEBUG, "allocated exact match table: handle=0x%" PRIx64,
		    fet->handle);
	*fet_out = fet;
	return 0;
}

static void
enic_fet_free(struct enic_flowman *fm, struct enic_fm_fet *fet)
{
	ENICPMD_FUNC_TRACE();
	enic_fm_tbl_free(fm, fet->handle);
	if (!fet->default_key)
		TAILQ_REMOVE(&fm->fet_list, fet, list);
	free(fet);
}

/*
 * Get the exact match table for the given combination of
 * <group, ingress, key>. Allocate one on the fly as necessary.
 */
static int
enic_fet_get(struct enic_flowman *fm,
	     uint32_t group,
	     uint8_t ingress,
	     struct fm_key_template *key,
	     struct enic_fm_fet **fet_out,
	     struct rte_flow_error *error)
{
	struct enic_fm_fet *fet;

	ENICPMD_FUNC_TRACE();
	/* See if we already have this table open */
	TAILQ_FOREACH(fet, &fm->fet_list, list) {
		if (fet->group == group && fet->ingress == ingress)
			break;
	}
	if (fet == NULL) {
		/* Jumping to a non-existing group? Use the default table */
		if (key == NULL) {
			fet = ingress ? fm->default_ig_fet : fm->default_eg_fet;
		} else if (enic_fet_alloc(fm, ingress, key, 0, &fet)) {
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "enic: cannot get exact match table");
		}
		fet->group = group;
		/* Default table is never on the open table list */
		if (!fet->default_key)
			TAILQ_INSERT_HEAD(&fm->fet_list, fet, list);
	}
	fet->ref++;
	*fet_out = fet;
	ENICPMD_LOG(DEBUG, "fet_get: %s %s group=%u ref=%u",
		    fet->default_key ? "default" : "",
		    fet->ingress ? "ingress" : "egress",
		    fet->group, fet->ref);
	return 0;
}

static void
enic_fet_put(struct enic_flowman *fm, struct enic_fm_fet *fet)
{
	ENICPMD_FUNC_TRACE();
	RTE_ASSERT(fet->ref > 0);
	fet->ref--;
	ENICPMD_LOG(DEBUG, "fet_put: %s %s group=%u ref=%u",
		    fet->default_key ? "default" : "",
		    fet->ingress ? "ingress" : "egress",
		    fet->group, fet->ref);
	if (fet->ref == 0)
		enic_fet_free(fm, fet);
}

/* Return 1 if current item is valid on top of the previous one. */
static int
fm_item_stacking_valid(enum rte_flow_item_type prev_item,
		       const struct enic_fm_items *item_info,
		       uint8_t is_first_item)
{
	enum rte_flow_item_type const *allowed_items = item_info->prev_items;

	ENICPMD_FUNC_TRACE();
	for (; *allowed_items != RTE_FLOW_ITEM_TYPE_END; allowed_items++) {
		if (prev_item == *allowed_items)
			return 1;
	}

	/* This is the first item in the stack. Check if that's cool */
	if (is_first_item && item_info->valid_start_item)
		return 1;
	return 0;
}

/*
 * Build the flow manager match entry structure from the provided pattern.
 * The pattern is validated as the items are copied.
 */
static int
enic_fm_copy_entry(struct enic_flowman *fm,
		   const struct rte_flow_item pattern[],
		   struct rte_flow_error *error)
{
	const struct enic_fm_items *item_info;
	enum rte_flow_item_type prev_item;
	const struct rte_flow_item *item;
	struct copy_item_args args;
	uint8_t prev_header_level;
	uint8_t is_first_item;
	int ret;

	ENICPMD_FUNC_TRACE();
	item = pattern;
	is_first_item = 1;
	prev_item = RTE_FLOW_ITEM_TYPE_END;

	args.fm_tcam_entry = &fm->tcam_entry;
	args.header_level = 0;
	prev_header_level = 0;
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		/*
		 * Get info about how to validate and copy the item. If NULL
		 * is returned the nic does not support the item.
		 */
		if (item->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		item_info = &enic_fm_items[item->type];

		if (item->type > FM_MAX_ITEM_TYPE ||
		    item_info->copy_item == NULL) {
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "enic: unsupported item");
		}

		/* check to see if item stacking is valid */
		if (!fm_item_stacking_valid(prev_item, item_info,
					    is_first_item))
			goto stacking_error;

		args.item = item;
		ret = item_info->copy_item(&args);
		if (ret)
			goto item_not_supported;
		/* Going from outer to inner? Treat it as a new packet start */
		if (prev_header_level != args.header_level) {
			prev_item = RTE_FLOW_ITEM_TYPE_END;
			is_first_item = 1;
		} else {
			prev_item = item->type;
			is_first_item = 0;
		}
		prev_header_level = args.header_level;
	}
	return 0;

item_not_supported:
	return rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_ITEM,
				  NULL, "enic: unsupported item type");

stacking_error:
	return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, "enic: unsupported item stack");
}

static void
flow_item_skip_void(const struct rte_flow_item **item)
{
	for ( ; ; (*item)++)
		if ((*item)->type != RTE_FLOW_ITEM_TYPE_VOID)
			return;
}

static void
append_template(void **template, uint8_t *off, const void *data, int len)
{
	memcpy(*template, data, len);
	*template = (char *)*template + len;
	*off = *off + len;
}

static int
enic_fm_append_action_op(struct enic_flowman *fm,
			 struct fm_action_op *fm_op,
			 struct rte_flow_error *error)
{
	int count;

	count = fm->action_op_count;
	ENICPMD_LOG(DEBUG, "append action op: idx=%d op=%u",
		    count, fm_op->fa_op);
	if (count == FM_ACTION_OP_MAX) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"too many action operations");
	}
	fm->action.fma_action_ops[count] = *fm_op;
	fm->action_op_count = count + 1;
	return 0;
}

static struct fm_action_op *
find_prev_action_op(struct enic_flowman *fm, uint32_t opcode)
{
	struct fm_action_op *op;
	int i;

	for (i = 0; i < fm->action_op_count; i++) {
		op = &fm->action.fma_action_ops[i];
		if (op->fa_op == opcode)
			return op;
	}
	return NULL;
}

/* NIC requires that 1st steer appear before decap.
 * Correct example: steer, decap, steer, steer, ...
 */
static void
enic_fm_reorder_action_op(struct enic_flowman *fm)
{
	struct fm_action_op *op, *steer, *decap;
	struct fm_action_op tmp_op;

	ENICPMD_FUNC_TRACE();
	/* Find 1st steer and decap */
	op = fm->action.fma_action_ops;
	steer = NULL;
	decap = NULL;
	while (op->fa_op != FMOP_END) {
		if (!decap && (op->fa_op == FMOP_DECAP_NOSTRIP ||
			       op->fa_op == FMOP_DECAP_STRIP))
			decap = op;
		else if (!steer && op->fa_op == FMOP_RQ_STEER)
			steer = op;
		op++;
	}
	/* If decap is before steer, swap */
	if (steer && decap && decap < steer) {
		op = fm->action.fma_action_ops;
		ENICPMD_LOG(DEBUG, "swap decap %ld <-> steer %ld",
			    (long)(decap - op), (long)(steer - op));
		tmp_op = *decap;
		*decap = *steer;
		*steer = tmp_op;
	}
}

/* VXLAN decap is done via flowman compound action */
static int
enic_fm_copy_vxlan_decap(struct enic_flowman *fm,
			 struct fm_tcam_match_entry *fmt,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	struct fm_header_set *fm_data;
	struct fm_action_op fm_op;

	ENICPMD_FUNC_TRACE();
	fm_data = &fmt->ftm_data.fk_hdrset[0];
	if (!(fm_data->fk_metadata & FKM_VXLAN)) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"vxlan-decap: vxlan must be in pattern");
	}

	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_DECAP_NOSTRIP;
	return enic_fm_append_action_op(fm, &fm_op, error);
}

/* Generate a reasonable source port number */
static uint16_t
gen_src_port(void)
{
	/* Min/max below are the default values in OVS-DPDK and Linux */
	uint16_t p = rte_rand();
	p = RTE_MAX(p, 32768);
	p = RTE_MIN(p, 61000);
	return rte_cpu_to_be_16(p);
}

/* VXLAN encap is done via flowman compound action */
static int
enic_fm_copy_vxlan_encap(struct enic_flowman *fm,
			 const struct rte_flow_item *item,
			 struct rte_flow_error *error)
{
	struct fm_action_op fm_op;
	struct rte_ether_hdr *eth;
	struct rte_udp_hdr *udp;
	uint16_t *ethertype;
	void *template;
	uint8_t off;

	ENICPMD_FUNC_TRACE();
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_ENCAP;
	template = fm->action.fma_data;
	off = 0;
	/*
	 * Copy flow items to the flowman template starting L2.
	 * L2 must be ethernet.
	 */
	flow_item_skip_void(&item);
	if (item->type != RTE_FLOW_ITEM_TYPE_ETH)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"vxlan-encap: first item should be ethernet");
	eth = (struct rte_ether_hdr *)template;
	ethertype = &eth->ether_type;
	append_template(&template, &off, item->spec,
			sizeof(struct rte_ether_hdr));
	item++;
	flow_item_skip_void(&item);
	/* Optional VLAN */
	if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		const struct rte_flow_item_vlan *spec;

		ENICPMD_LOG(DEBUG, "vxlan-encap: vlan");
		spec = item->spec;
		fm_op.encap.outer_vlan = rte_be_to_cpu_16(spec->tci);
		item++;
		flow_item_skip_void(&item);
	}
	/* L3 must be IPv4, IPv6 */
	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_IPV4:
	{
		struct rte_ipv4_hdr *ip4;

		ENICPMD_LOG(DEBUG, "vxlan-encap: ipv4");
		*ethertype = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		ip4 = (struct rte_ipv4_hdr *)template;
		/*
		 * Offset of IPv4 length field and its initial value
		 * (IP + UDP + VXLAN) are specified in the action. The NIC
		 * will add inner packet length.
		 */
		fm_op.encap.len1_offset = off +
			offsetof(struct rte_ipv4_hdr, total_length);
		fm_op.encap.len1_delta = sizeof(struct rte_ipv4_hdr) +
			sizeof(struct rte_udp_hdr) +
			sizeof(struct rte_vxlan_hdr);
		append_template(&template, &off, item->spec,
				sizeof(struct rte_ipv4_hdr));
		ip4->version_ihl = RTE_IPV4_VHL_DEF;
		if (ip4->time_to_live == 0)
			ip4->time_to_live = IP_DEFTTL;
		ip4->next_proto_id = IPPROTO_UDP;
		break;
	}
	case RTE_FLOW_ITEM_TYPE_IPV6:
	{
		struct rte_ipv6_hdr *ip6;

		ENICPMD_LOG(DEBUG, "vxlan-encap: ipv6");
		*ethertype = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		ip6 = (struct rte_ipv6_hdr *)template;
		fm_op.encap.len1_offset = off +
			offsetof(struct rte_ipv6_hdr, payload_len);
		fm_op.encap.len1_delta = sizeof(struct rte_udp_hdr) +
			sizeof(struct rte_vxlan_hdr);
		append_template(&template, &off, item->spec,
				sizeof(struct rte_ipv6_hdr));
		ip6->vtc_flow |= rte_cpu_to_be_32(IP6_VTC_FLOW);
		if (ip6->hop_limits == 0)
			ip6->hop_limits = IP_DEFTTL;
		ip6->proto = IPPROTO_UDP;
		break;
	}
	default:
		return rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"vxlan-encap: L3 must be IPv4/IPv6");
	}
	item++;
	flow_item_skip_void(&item);

	/* L4 is UDP */
	if (item->type != RTE_FLOW_ITEM_TYPE_UDP)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"vxlan-encap: UDP must follow IPv4/IPv6");
	/* UDP length = UDP + VXLAN. NIC will add inner packet length. */
	fm_op.encap.len2_offset =
		off + offsetof(struct rte_udp_hdr, dgram_len);
	fm_op.encap.len2_delta =
		sizeof(struct rte_udp_hdr) + sizeof(struct rte_vxlan_hdr);
	udp = (struct rte_udp_hdr *)template;
	append_template(&template, &off, item->spec,
			sizeof(struct rte_udp_hdr));
	/*
	 * Firmware does not hash/fill source port yet. Generate a
	 * random port, as there is *usually* one rte_flow for the
	 * given inner packet stream (i.e. a single stream has one
	 * random port).
	 */
	if (udp->src_port == 0)
		udp->src_port = gen_src_port();
	item++;
	flow_item_skip_void(&item);

	/* Finally VXLAN */
	if (item->type != RTE_FLOW_ITEM_TYPE_VXLAN)
		return rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"vxlan-encap: VXLAN must follow UDP");
	append_template(&template, &off, item->spec,
			sizeof(struct rte_flow_item_vxlan));

	/*
	 * Fill in the rest of the action structure.
	 * Indicate that we want to encap with vxlan at packet start.
	 */
	fm_op.encap.template_offset = 0;
	fm_op.encap.template_len = off;
	return enic_fm_append_action_op(fm, &fm_op, error);
}

static int
enic_fm_find_vnic(struct enic *enic, const struct rte_pci_addr *addr,
		  uint64_t *handle)
{
	uint32_t bdf;
	uint64_t args[2];
	int rc;

	ENICPMD_FUNC_TRACE();
	ENICPMD_LOG(DEBUG, "bdf=%x:%x:%x", addr->bus, addr->devid,
		    addr->function);
	bdf = addr->bus << 8 | addr->devid << 3 | addr->function;
	args[0] = FM_VNIC_FIND;
	args[1] = bdf;
	rc = vnic_dev_flowman_cmd(enic->vdev, args, 2);
	if (rc != 0) {
		/* Expected to fail if BDF is not on the adapter */
		ENICPMD_LOG(DEBUG, "cannot find vnic handle: rc=%d", rc);
		return rc;
	}
	*handle = args[0];
	ENICPMD_LOG(DEBUG, "found vnic: handle=0x%" PRIx64, *handle);
	return 0;
}

/*
 * Egress: target port should be either PF uplink or VF.
 * Supported cases
 * 1. VF egress -> PF uplink
 *   PF may be this VF's PF, or another PF, as long as they are on the same VIC.
 * 2. VF egress -> VF
 *
 * Unsupported cases
 * 1. PF egress -> VF
 *   App should be using representor to pass packets to VF
 */
static int
vf_egress_port_id_action(struct enic_flowman *fm,
			 struct rte_eth_dev *dst_dev,
			 uint64_t dst_vnic_h,
			 struct fm_action_op *fm_op,
			 struct rte_flow_error *error)
{
	struct enic *src_enic, *dst_enic;
	struct enic_vf_representor *vf;
	uint8_t uif;
	int ret;

	ENICPMD_FUNC_TRACE();
	src_enic = fm->user_enic;
	dst_enic = pmd_priv(dst_dev);
	if (!(src_enic->rte_dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION,
			NULL, "source port is not VF representor");
	}

	/* VF -> PF uplink. dst is not VF representor */
	if (!(dst_dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)) {
		/* PF is the VF's PF? Then nothing to do */
		vf = VF_ENIC_TO_VF_REP(src_enic);
		if (vf->pf == dst_enic) {
			ENICPMD_LOG(DEBUG, "destination port is VF's PF");
			return 0;
		}
		/* If not, steer to the remote PF's uplink */
		uif = dst_enic->fm_vnic_uif;
		ENICPMD_LOG(DEBUG, "steer to uplink %u", uif);
		memset(fm_op, 0, sizeof(*fm_op));
		fm_op->fa_op = FMOP_SET_EGPORT;
		fm_op->set_egport.egport = uif;
		ret = enic_fm_append_action_op(fm, fm_op, error);
		return ret;
	}

	/* VF -> VF loopback. Hairpin and steer to vnic */
	memset(fm_op, 0, sizeof(*fm_op));
	fm_op->fa_op = FMOP_EG_HAIRPIN;
	ret = enic_fm_append_action_op(fm, fm_op, error);
	if (ret)
		return ret;
	ENICPMD_LOG(DEBUG, "egress hairpin");
	fm->hairpin_steer_vnic_h = dst_vnic_h;
	fm->need_hairpin_steer = 1;
	return 0;
}

/* Translate flow actions to flowman TCAM entry actions */
static int
enic_fm_copy_action(struct enic_flowman *fm,
		    const struct rte_flow_action actions[],
		    uint8_t ingress,
		    struct rte_flow_error *error)
{
	enum {
		FATE = 1 << 0,
		DECAP = 1 << 1,
		PASSTHRU = 1 << 2,
		COUNT = 1 << 3,
		ENCAP = 1 << 4,
		PUSH_VLAN = 1 << 5,
		PORT_ID = 1 << 6,
	};
	struct fm_tcam_match_entry *fmt;
	struct fm_action_op fm_op;
	bool need_ovlan_action;
	struct enic *enic;
	uint32_t overlap;
	uint64_t vnic_h;
	uint16_t ovlan;
	bool first_rq;
	bool steer;
	int ret;

	ENICPMD_FUNC_TRACE();
	fmt = &fm->tcam_entry;
	need_ovlan_action = false;
	ovlan = 0;
	first_rq = true;
	steer = false;
	enic = fm->user_enic;
	overlap = 0;
	vnic_h = enic->fm_vnic_handle;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		case RTE_FLOW_ACTION_TYPE_PASSTHRU: {
			if (overlap & PASSTHRU)
				goto unsupported;
			overlap |= PASSTHRU;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_JUMP: {
			const struct rte_flow_action_jump *jump =
				actions->conf;
			struct enic_fm_fet *fet;

			if (overlap & FATE)
				goto unsupported;
			ret = enic_fet_get(fm, jump->group, ingress, NULL,
					   &fet, error);
			if (ret)
				return ret;
			overlap |= FATE;
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_EXACT_MATCH;
			fm_op.exact.handle = fet->handle;
			fm->fet = fet;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_MARK: {
			const struct rte_flow_action_mark *mark =
				actions->conf;

			if (mark->id >= ENIC_MAGIC_FILTER_ID - 1)
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "invalid mark id");
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_MARK;
			fm_op.mark.mark = mark->id + 1;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_FLAG: {
			/* ENIC_MAGIC_FILTER_ID is reserved for flagging */
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_MARK;
			fm_op.mark.mark = ENIC_MAGIC_FILTER_ID;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_QUEUE: {
			const struct rte_flow_action_queue *queue =
				actions->conf;

			/*
			 * If fate other than QUEUE or RSS, fail. Multiple
			 * rss and queue actions are ok.
			 */
			if ((overlap & FATE) && first_rq)
				goto unsupported;
			first_rq = false;
			overlap |= FATE;
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_RQ_STEER;
			fm_op.rq_steer.rq_index =
				enic_rte_rq_idx_to_sop_idx(queue->index);
			fm_op.rq_steer.rq_count = 1;
			fm_op.rq_steer.vnic_handle = vnic_h;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			ENICPMD_LOG(DEBUG, "create QUEUE action rq: %u",
				    fm_op.rq_steer.rq_index);
			steer = true;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_DROP: {
			if (overlap & FATE)
				goto unsupported;
			overlap |= FATE;
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_DROP;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			ENICPMD_LOG(DEBUG, "create DROP action");
			break;
		}
		case RTE_FLOW_ACTION_TYPE_COUNT: {
			if (overlap & COUNT)
				goto unsupported;
			overlap |= COUNT;
			/* Count is associated with entry not action on VIC. */
			fmt->ftm_flags |= FMEF_COUNTER;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_RSS: {
			const struct rte_flow_action_rss *rss = actions->conf;
			bool allow;
			uint16_t i;

			/*
			 * If fate other than QUEUE or RSS, fail. Multiple
			 * rss and queue actions are ok.
			 */
			if ((overlap & FATE) && first_rq)
				goto unsupported;
			first_rq = false;
			overlap |= FATE;

			/*
			 * Hardware only supports RSS actions on outer level
			 * with default type and function. Queues must be
			 * sequential.
			 */
			allow = rss->func == RTE_ETH_HASH_FUNCTION_DEFAULT &&
				rss->level == 0 && (rss->types == 0 ||
				rss->types == enic->rss_hf) &&
				rss->queue_num <= enic->rq_count &&
				rss->queue[rss->queue_num - 1] < enic->rq_count;


			/* Identity queue map needs to be sequential */
			for (i = 1; i < rss->queue_num; i++)
				allow = allow && (rss->queue[i] ==
					rss->queue[i - 1] + 1);
			if (!allow)
				goto unsupported;

			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_RQ_STEER;
			fm_op.rq_steer.rq_index =
				enic_rte_rq_idx_to_sop_idx(rss->queue[0]);
			fm_op.rq_steer.rq_count = rss->queue_num;
			fm_op.rq_steer.vnic_handle = vnic_h;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			ENICPMD_LOG(DEBUG, "create QUEUE action rq: %u",
				    fm_op.rq_steer.rq_index);
			steer = true;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_PORT_ID: {
			const struct rte_flow_action_port_id *port;
			struct rte_eth_dev *dev;

			if (!ingress && (overlap & PORT_ID)) {
				ENICPMD_LOG(DEBUG, "cannot have multiple egress PORT_ID actions");
				goto unsupported;
			}
			port = actions->conf;
			if (port->original) {
				vnic_h = enic->fm_vnic_handle; /* This port */
				break;
			}
			ENICPMD_LOG(DEBUG, "port id %u", port->id);
			if (!rte_eth_dev_is_valid_port(port->id)) {
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "invalid port_id");
			}
			dev = &rte_eth_devices[port->id];
			if (!dev_is_enic(dev)) {
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "port_id is not enic");
			}
			if (enic->switch_domain_id !=
			    pmd_priv(dev)->switch_domain_id) {
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "destination and source ports are not in the same switch domain");
			}
			vnic_h = pmd_priv(dev)->fm_vnic_handle;
			overlap |= PORT_ID;
			/*
			 * Ingress. Nothing more to do. We add an implicit
			 * steer at the end if needed.
			 */
			if (ingress)
				break;
			/* Egress */
			ret = vf_egress_port_id_action(fm, dev, vnic_h, &fm_op,
				error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP: {
			if (overlap & DECAP)
				goto unsupported;
			overlap |= DECAP;

			ret = enic_fm_copy_vxlan_decap(fm, fmt, actions,
				error);
			if (ret != 0)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP: {
			const struct rte_flow_action_vxlan_encap *encap;

			encap = actions->conf;
			if (overlap & ENCAP)
				goto unsupported;
			overlap |= ENCAP;
			ret = enic_fm_copy_vxlan_encap(fm, encap->definition,
				error);
			if (ret != 0)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN: {
			struct fm_action_op *decap;

			/*
			 * If decap-nostrip appears before pop vlan, this pop
			 * applies to the inner packet vlan. Turn it into
			 * decap-strip.
			 */
			decap = find_prev_action_op(fm, FMOP_DECAP_NOSTRIP);
			if (decap) {
				ENICPMD_LOG(DEBUG, "pop-vlan inner: decap-nostrip => decap-strip");
				decap->fa_op = FMOP_DECAP_STRIP;
				break;
			}
			memset(&fm_op, 0, sizeof(fm_op));
			fm_op.fa_op = FMOP_POP_VLAN;
			ret = enic_fm_append_action_op(fm, &fm_op, error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN: {
			const struct rte_flow_action_of_push_vlan *vlan;

			if (overlap & PASSTHRU)
				goto unsupported;
			vlan = actions->conf;
			if (vlan->ethertype != RTE_BE16(RTE_ETHER_TYPE_VLAN)) {
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "unexpected push_vlan ethertype");
			}
			overlap |= PUSH_VLAN;
			need_ovlan_action = true;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP: {
			const struct rte_flow_action_of_set_vlan_pcp *pcp;

			pcp = actions->conf;
			if (pcp->vlan_pcp > 7) {
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "invalid vlan_pcp");
			}
			need_ovlan_action = true;
			ovlan |= ((uint16_t)pcp->vlan_pcp) << 13;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID: {
			const struct rte_flow_action_of_set_vlan_vid *vid;

			vid = actions->conf;
			need_ovlan_action = true;
			ovlan |= rte_be_to_cpu_16(vid->vlan_vid);
			break;
		}
		default:
			goto unsupported;
		}
	}

	if (!(overlap & (FATE | PASSTHRU | COUNT | PORT_ID)))
		goto unsupported;
	/* Egress from VF: need implicit WQ match */
	if (enic_is_vf_rep(enic) && !ingress) {
		fmt->ftm_data.fk_wq_id = 0;
		fmt->ftm_mask.fk_wq_id = 0xffff;
		fmt->ftm_data.fk_wq_vnic = enic->fm_vnic_handle;
		ENICPMD_LOG(DEBUG, "add implicit wq id match for vf %d",
			    VF_ENIC_TO_VF_REP(enic)->vf_id);
	}
	if (need_ovlan_action) {
		memset(&fm_op, 0, sizeof(fm_op));
		fm_op.fa_op = FMOP_SET_OVLAN;
		fm_op.ovlan.vlan = ovlan;
		ret = enic_fm_append_action_op(fm, &fm_op, error);
		if (ret)
			return ret;
	}
	/* Add steer op for PORT_ID without QUEUE */
	if ((overlap & PORT_ID) && !steer && ingress) {
		memset(&fm_op, 0, sizeof(fm_op));
		/* Always to queue 0 for now as generic RSS is not available */
		fm_op.fa_op = FMOP_RQ_STEER;
		fm_op.rq_steer.rq_index = 0;
		fm_op.rq_steer.vnic_handle = vnic_h;
		ret = enic_fm_append_action_op(fm, &fm_op, error);
		if (ret)
			return ret;
		ENICPMD_LOG(DEBUG, "add implicit steer op");
	}
	/* Add required END */
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	ret = enic_fm_append_action_op(fm, &fm_op, error);
	if (ret)
		return ret;
	enic_fm_reorder_action_op(fm);
	return 0;

unsupported:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				  NULL, "enic: unsupported action");
}

/** Check if the action is supported */
static int
enic_fm_match_action(const struct rte_flow_action *action,
		     const enum rte_flow_action_type *supported_actions)
{
	for (; *supported_actions != RTE_FLOW_ACTION_TYPE_END;
	     supported_actions++) {
		if (action->type == *supported_actions)
			return 1;
	}
	return 0;
}

/* Debug function to dump internal NIC action structure. */
static void
enic_fm_dump_tcam_actions(const struct fm_action *fm_action)
{
	/* Manually keep in sync with FMOP commands */
	const char *fmop_str[FMOP_OP_MAX] = {
		[FMOP_END] = "end",
		[FMOP_DROP] = "drop",
		[FMOP_RQ_STEER] = "steer",
		[FMOP_EXACT_MATCH] = "exmatch",
		[FMOP_MARK] = "mark",
		[FMOP_EXT_MARK] = "ext_mark",
		[FMOP_TAG] = "tag",
		[FMOP_EG_HAIRPIN] = "eg_hairpin",
		[FMOP_IG_HAIRPIN] = "ig_hairpin",
		[FMOP_ENCAP_IVLAN] = "encap_ivlan",
		[FMOP_ENCAP_NOIVLAN] = "encap_noivlan",
		[FMOP_ENCAP] = "encap",
		[FMOP_SET_OVLAN] = "set_ovlan",
		[FMOP_DECAP_NOSTRIP] = "decap_nostrip",
		[FMOP_DECAP_STRIP] = "decap_strip",
		[FMOP_POP_VLAN] = "pop_vlan",
		[FMOP_SET_EGPORT] = "set_egport",
		[FMOP_RQ_STEER_ONLY] = "rq_steer_only",
		[FMOP_SET_ENCAP_VLAN] = "set_encap_vlan",
		[FMOP_EMIT] = "emit",
		[FMOP_MODIFY] = "modify",
	};
	const struct fm_action_op *op = &fm_action->fma_action_ops[0];
	char buf[128], *bp = buf;
	const char *op_str;
	int i, n, buf_len;

	buf[0] = '\0';
	buf_len = sizeof(buf);
	for (i = 0; i < FM_ACTION_OP_MAX; i++) {
		if (op->fa_op == FMOP_END)
			break;
		if (op->fa_op >= FMOP_OP_MAX)
			op_str = "unknown";
		else
			op_str = fmop_str[op->fa_op];
		n = snprintf(bp, buf_len, "%s,", op_str);
		if (n > 0 && n < buf_len) {
			bp += n;
			buf_len -= n;
		}
		op++;
	}
	/* Remove trailing comma */
	if (buf[0])
		*(bp - 1) = '\0';
	ENICPMD_LOG(DEBUG, "       Acions: %s", buf);
}

static int
bits_to_str(uint32_t bits, const char *strings[], int max,
	    char *buf, int buf_len)
{
	int i, n = 0, len = 0;

	for (i = 0; i < max; i++) {
		if (bits & (1 << i)) {
			n = snprintf(buf, buf_len, "%s,", strings[i]);
			if (n > 0 && n < buf_len) {
				buf += n;
				buf_len -= n;
				len += n;
			}
		}
	}
	/* Remove trailing comma */
	if (len) {
		*(buf - 1) = '\0';
		len--;
	}
	return len;
}

/* Debug function to dump internal NIC filter structure. */
static void
__enic_fm_dump_tcam_match(const struct fm_header_set *fk_hdrset, char *buf,
			  int buf_len)
{
	/* Manually keep in sync with FKM_BITS */
	const char *fm_fkm_str[FKM_BIT_COUNT] = {
		[FKM_QTAG_BIT] = "qtag",
		[FKM_CMD_BIT] = "cmd",
		[FKM_IPV4_BIT] = "ip4",
		[FKM_IPV6_BIT] = "ip6",
		[FKM_ROCE_BIT] = "roce",
		[FKM_UDP_BIT] = "udp",
		[FKM_TCP_BIT] = "tcp",
		[FKM_TCPORUDP_BIT] = "tcpportudp",
		[FKM_IPFRAG_BIT] = "ipfrag",
		[FKM_NVGRE_BIT] = "nvgre",
		[FKM_VXLAN_BIT] = "vxlan",
		[FKM_GENEVE_BIT] = "geneve",
		[FKM_NSH_BIT] = "nsh",
		[FKM_ROCEV2_BIT] = "rocev2",
		[FKM_VLAN_PRES_BIT] = "vlan_pres",
		[FKM_IPOK_BIT] = "ipok",
		[FKM_L4OK_BIT] = "l4ok",
		[FKM_ROCEOK_BIT] = "roceok",
		[FKM_FCSOK_BIT] = "fcsok",
		[FKM_EG_SPAN_BIT] = "eg_span",
		[FKM_IG_SPAN_BIT] = "ig_span",
		[FKM_EG_HAIRPINNED_BIT] = "eg_hairpinned",
	};
	/* Manually keep in sync with FKH_BITS */
	const char *fm_fkh_str[FKH_BIT_COUNT] = {
		[FKH_ETHER_BIT] = "eth",
		[FKH_QTAG_BIT] = "qtag",
		[FKH_L2RAW_BIT] = "l2raw",
		[FKH_IPV4_BIT] = "ip4",
		[FKH_IPV6_BIT] = "ip6",
		[FKH_L3RAW_BIT] = "l3raw",
		[FKH_UDP_BIT] = "udp",
		[FKH_TCP_BIT] = "tcp",
		[FKH_ICMP_BIT] = "icmp",
		[FKH_VXLAN_BIT] = "vxlan",
		[FKH_L4RAW_BIT] = "l4raw",
	};
	uint32_t fkh_bits = fk_hdrset->fk_header_select;
	uint32_t fkm_bits = fk_hdrset->fk_metadata;
	int n;

	if (!fkm_bits && !fkh_bits)
		return;
	n = snprintf(buf, buf_len, "metadata(");
	if (n > 0 && n < buf_len) {
		buf += n;
		buf_len -= n;
	}
	n = bits_to_str(fkm_bits, fm_fkm_str, FKM_BIT_COUNT, buf, buf_len);
	if (n > 0 && n < buf_len) {
		buf += n;
		buf_len -= n;
	}
	n = snprintf(buf, buf_len, ") valid hdr fields(");
	if (n > 0 && n < buf_len) {
		buf += n;
		buf_len -= n;
	}
	n = bits_to_str(fkh_bits, fm_fkh_str, FKH_BIT_COUNT, buf, buf_len);
	if (n > 0 && n < buf_len) {
		buf += n;
		buf_len -= n;
	}
	snprintf(buf, buf_len, ")");
}

static void
enic_fm_dump_tcam_match(const struct fm_tcam_match_entry *match,
			uint8_t ingress)
{
	char buf[256];

	memset(buf, 0, sizeof(buf));
	__enic_fm_dump_tcam_match(&match->ftm_mask.fk_hdrset[0],
				  buf, sizeof(buf));
	ENICPMD_LOG(DEBUG, " TCAM %s Outer: %s %scounter position %u",
		    (ingress) ? "IG" : "EG", buf,
		    (match->ftm_flags & FMEF_COUNTER) ? "" : "no ",
		    match->ftm_position);
	memset(buf, 0, sizeof(buf));
	__enic_fm_dump_tcam_match(&match->ftm_mask.fk_hdrset[1],
				  buf, sizeof(buf));
	if (buf[0])
		ENICPMD_LOG(DEBUG, "         Inner: %s", buf);
}

/* Debug function to dump internal NIC flow structures. */
static void
enic_fm_dump_tcam_entry(const struct fm_tcam_match_entry *fm_match,
			const struct fm_action *fm_action,
			uint8_t ingress)
{
	if (!rte_log_can_log(enic_pmd_logtype, RTE_LOG_DEBUG))
		return;
	enic_fm_dump_tcam_match(fm_match, ingress);
	enic_fm_dump_tcam_actions(fm_action);
}

static int
enic_fm_flow_parse(struct enic_flowman *fm,
		   const struct rte_flow_attr *attrs,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	const struct rte_flow_action *action;
	unsigned int ret;
	static const enum rte_flow_action_type *sa;

	ENICPMD_FUNC_TRACE();
	ret = 0;
	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "no pattern specified");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "no action specified");
		return -rte_errno;
	}

	if (attrs) {
		if (attrs->group != FM_TCAM_RTE_GROUP && attrs->priority) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					   NULL,
					   "priorities are not supported for non-default (0) groups");
			return -rte_errno;
		} else if (!fm->owner_enic->switchdev_mode && attrs->transfer) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					   NULL,
					   "transfer is not supported");
			return -rte_errno;
		} else if (attrs->ingress && attrs->egress) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					   NULL,
					   "bidirectional rules not supported");
			return -rte_errno;
		}

	} else {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "no attribute specified");
		return -rte_errno;
	}

	/* Verify Actions. */
	sa = (attrs->ingress) ? enic_fm_supported_ig_actions :
	     enic_fm_supported_eg_actions;
	for (action = &actions[0]; action->type != RTE_FLOW_ACTION_TYPE_END;
	     action++) {
		if (action->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;
		else if (!enic_fm_match_action(action, sa))
			break;
	}
	if (action->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_ACTION,
				   action, "invalid action");
		return -rte_errno;
	}
	ret = enic_fm_copy_entry(fm, pattern, error);
	if (ret)
		return ret;
	ret = enic_fm_copy_action(fm, actions, attrs->ingress, error);
	return ret;
}

static void
enic_fm_counter_free(struct enic_flowman *fm, struct enic_fm_flow *fm_flow)
{
	if (!fm_flow->counter_valid)
		return;
	SLIST_INSERT_HEAD(&fm->counters, fm_flow->counter, next);
	fm_flow->counter_valid = false;
}

static int
enic_fm_more_counters(struct enic_flowman *fm)
{
	struct enic_fm_counter *new_stack;
	struct enic_fm_counter *ctrs;
	int i, rc;
	uint64_t args[2];

	ENICPMD_FUNC_TRACE();
	new_stack = rte_realloc(fm->counter_stack, (fm->counters_alloced +
				FM_COUNTERS_EXPAND) *
				sizeof(struct enic_fm_counter), 0);
	if (new_stack == NULL) {
		ENICPMD_LOG(ERR, "cannot alloc counter memory");
		return -ENOMEM;
	}
	fm->counter_stack = new_stack;

	args[0] = FM_COUNTER_BRK;
	args[1] = fm->counters_alloced + FM_COUNTERS_EXPAND;
	rc = flowman_cmd(fm, args, 2);
	if (rc != 0) {
		ENICPMD_LOG(ERR, "cannot alloc counters rc=%d", rc);
		return rc;
	}
	ctrs = (struct enic_fm_counter *)fm->counter_stack +
		fm->counters_alloced;
	for (i = 0; i < FM_COUNTERS_EXPAND; i++, ctrs++) {
		ctrs->handle = fm->counters_alloced + i;
		SLIST_INSERT_HEAD(&fm->counters, ctrs, next);
	}
	fm->counters_alloced += FM_COUNTERS_EXPAND;
	ENICPMD_LOG(DEBUG, "%u counters allocated, total: %u",
		    FM_COUNTERS_EXPAND, fm->counters_alloced);
	return 0;
}

static int
enic_fm_counter_zero(struct enic_flowman *fm, struct enic_fm_counter *c)
{
	uint64_t args[3];
	int ret;

	ENICPMD_FUNC_TRACE();
	args[0] = FM_COUNTER_QUERY;
	args[1] = c->handle;
	args[2] = 1; /* clear */
	ret = flowman_cmd(fm, args, 3);
	if (ret) {
		ENICPMD_LOG(ERR, "counter init: rc=%d handle=0x%x",
			    ret, c->handle);
		return ret;
	}
	return 0;
}

static int
enic_fm_counter_alloc(struct enic_flowman *fm, struct rte_flow_error *error,
		      struct enic_fm_counter **ctr)
{
	struct enic_fm_counter *c;
	int ret;

	ENICPMD_FUNC_TRACE();
	*ctr = NULL;
	if (SLIST_EMPTY(&fm->counters)) {
		ret = enic_fm_more_counters(fm);
		if (ret)
			return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "enic: out of counters");
	}
	c = SLIST_FIRST(&fm->counters);
	SLIST_REMOVE_HEAD(&fm->counters, next);
	*ctr = c;
	return 0;
}

static int
enic_fm_action_free(struct enic_flowman *fm, struct enic_fm_action *ah)
{
	uint64_t args[2];
	int ret = 0;

	ENICPMD_FUNC_TRACE();
	RTE_ASSERT(ah->ref > 0);
	ah->ref--;
	if (ah->ref == 0) {
		args[0] = FM_ACTION_FREE;
		args[1] = ah->handle;
		ret = flowman_cmd(fm, args, 2);
		if (ret)
			/* This is a "should never happen" error. */
			ENICPMD_LOG(ERR, "freeing action rc=%d handle=0x%"
				    PRIx64, ret, ah->handle);
		rte_hash_del_key(fm->action_hash, (const void *)&ah->key);
		free(ah);
	}
	return ret;
}

static int
enic_fm_entry_free(struct enic_flowman *fm, uint64_t handle)
{
	uint64_t args[2];
	int rc;

	ENICPMD_FUNC_TRACE();
	args[0] = FM_MATCH_ENTRY_REMOVE;
	args[1] = handle;
	rc = flowman_cmd(fm, args, 2);
	if (rc)
		ENICPMD_LOG(ERR, "cannot free match entry: rc=%d"
			    " handle=0x%" PRIx64, rc, handle);
	return rc;
}

static struct enic_fm_jump_flow *
find_jump_flow(struct enic_flowman *fm, uint32_t group)
{
	struct enic_fm_jump_flow *j;

	ENICPMD_FUNC_TRACE();
	TAILQ_FOREACH(j, &fm->jump_list, list) {
		if (j->group == group)
			return j;
	}
	return NULL;
}

static void
remove_jump_flow(struct enic_flowman *fm, struct rte_flow *flow)
{
	struct enic_fm_jump_flow *j;

	ENICPMD_FUNC_TRACE();
	TAILQ_FOREACH(j, &fm->jump_list, list) {
		if (j->flow == flow) {
			TAILQ_REMOVE(&fm->jump_list, j, list);
			free(j);
			return;
		}
	}
}

static int
save_jump_flow(struct enic_flowman *fm,
	       struct rte_flow *flow,
	       uint32_t group,
	       struct fm_tcam_match_entry *match,
	       struct fm_action *action)
{
	struct enic_fm_jump_flow *j;

	ENICPMD_FUNC_TRACE();
	j = calloc(1, sizeof(struct enic_fm_jump_flow));
	if (j == NULL)
		return -ENOMEM;
	j->flow = flow;
	j->group = group;
	j->match = *match;
	j->action = *action;
	TAILQ_INSERT_HEAD(&fm->jump_list, j, list);
	ENICPMD_LOG(DEBUG, "saved jump flow: flow=%p group=%u", flow, group);
	return 0;
}

static void
__enic_fm_flow_free(struct enic_flowman *fm, struct enic_fm_flow *fm_flow)
{
	if (fm_flow->entry_handle != FM_INVALID_HANDLE) {
		enic_fm_entry_free(fm, fm_flow->entry_handle);
		fm_flow->entry_handle = FM_INVALID_HANDLE;
	}
	if (fm_flow->action != NULL) {
		enic_fm_action_free(fm, fm_flow->action);
		fm_flow->action = NULL;
	}
	enic_fm_counter_free(fm, fm_flow);
	if (fm_flow->fet) {
		enic_fet_put(fm, fm_flow->fet);
		fm_flow->fet = NULL;
	}
}

static void
enic_fm_flow_free(struct enic_flowman *fm, struct rte_flow *flow)
{
	struct enic_fm_flow *steer = flow->fm->hairpin_steer_flow;

	if (flow->fm->fet && flow->fm->fet->default_key)
		remove_jump_flow(fm, flow);
	__enic_fm_flow_free(fm, flow->fm);
	if (steer) {
		__enic_fm_flow_free(fm, steer);
		free(steer);
	}
	free(flow->fm);
	free(flow);
}

static int
enic_fm_add_tcam_entry(struct enic_flowman *fm,
		       struct fm_tcam_match_entry *match_in,
		       uint64_t *entry_handle,
		       uint8_t ingress,
		       struct rte_flow_error *error)
{
	struct fm_tcam_match_entry *ftm;
	uint64_t args[3];
	int ret;

	ENICPMD_FUNC_TRACE();
	/* Copy entry to the command buffer */
	ftm = &fm->cmd.va->fm_tcam_match_entry;
	memcpy(ftm, match_in, sizeof(*ftm));
	/* Add TCAM entry */
	args[0] = FM_TCAM_ENTRY_INSTALL;
	args[1] = ingress ? fm->ig_tcam_hndl : fm->eg_tcam_hndl;
	args[2] = fm->cmd.pa;
	ret = flowman_cmd(fm, args, 3);
	if (ret != 0) {
		ENICPMD_LOG(ERR, "cannot add %s TCAM entry: rc=%d",
			    ingress ? "ingress" : "egress", ret);
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			NULL, "enic: devcmd(tcam-entry-install)");
		return ret;
	}
	ENICPMD_LOG(DEBUG, "installed %s TCAM entry: handle=0x%" PRIx64,
		    ingress ? "ingress" : "egress", (uint64_t)args[0]);
	*entry_handle = args[0];
	return 0;
}

static int
enic_fm_add_exact_entry(struct enic_flowman *fm,
			struct fm_tcam_match_entry *match_in,
			uint64_t *entry_handle,
			struct enic_fm_fet *fet,
			struct rte_flow_error *error)
{
	struct fm_exact_match_entry *fem;
	uint64_t args[3];
	int ret;

	ENICPMD_FUNC_TRACE();
	/* The new entry must have the table's key */
	if (memcmp(fet->key.fk_hdrset, match_in->ftm_mask.fk_hdrset,
		   sizeof(struct fm_header_set) * FM_HDRSET_MAX)) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"enic: key does not match group's key");
	}

	/* Copy entry to the command buffer */
	fem = &fm->cmd.va->fm_exact_match_entry;
	/*
	 * Translate TCAM entry to exact entry. As is only need to drop
	 * position and mask. The mask is part of the exact match table.
	 * Position (aka priority) is not supported in the exact match table.
	 */
	fem->fem_data = match_in->ftm_data;
	fem->fem_flags = match_in->ftm_flags;
	fem->fem_action = match_in->ftm_action;
	fem->fem_counter = match_in->ftm_counter;

	/* Add exact entry */
	args[0] = FM_EXACT_ENTRY_INSTALL;
	args[1] = fet->handle;
	args[2] = fm->cmd.pa;
	ret = flowman_cmd(fm, args, 3);
	if (ret != 0) {
		ENICPMD_LOG(ERR, "cannot add %s exact entry: group=%u",
			    fet->ingress ? "ingress" : "egress", fet->group);
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			NULL, "enic: devcmd(exact-entry-install)");
		return ret;
	}
	ENICPMD_LOG(DEBUG, "installed %s exact entry: group=%u"
		    " handle=0x%" PRIx64,
		    fet->ingress ? "ingress" : "egress", fet->group,
		    (uint64_t)args[0]);
	*entry_handle = args[0];
	return 0;
}

static int
enic_action_handle_get(struct enic_flowman *fm, struct fm_action *action_in,
		       struct rte_flow_error *error,
		       struct enic_fm_action **ah_o)
{
	struct enic_fm_action *ah;
	struct fm_action *fma;
	uint64_t args[2];
	int ret = 0;

	ret = rte_hash_lookup_data(fm->action_hash, action_in,
				   (void **)&ah);
	if (ret < 0 && ret != -ENOENT)
		return rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "enic: rte_hash_lookup(aciton)");

	if (ret == -ENOENT) {
		/* Allocate a new action on the NIC. */
		fma = &fm->cmd.va->fm_action;
		memcpy(fma, action_in, sizeof(*fma));

		ah = calloc(1, sizeof(*ah));
		memcpy(&ah->key, action_in, sizeof(struct fm_action));
		if (ah == NULL)
			return rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "enic: calloc(fm-action)");
		args[0] = FM_ACTION_ALLOC;
		args[1] = fm->cmd.pa;
		ret = flowman_cmd(fm, args, 2);
		if (ret != 0) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "enic: devcmd(action-alloc)");
			goto error_with_ah;
		}
		ah->handle = args[0];
		ret = rte_hash_add_key_data(fm->action_hash,
					    (const void *)action_in,
					    (void *)ah);
		if (ret != 0) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "enic: rte_hash_add_key_data(actn)");
			goto error_with_action_handle;
		}
		ENICPMD_LOG(DEBUG, "action allocated: handle=0x%" PRIx64,
			    ah->handle);
	}

	/* Action handle struct is valid, increment reference count. */
	ah->ref++;
	*ah_o = ah;
	return 0;
error_with_action_handle:
	args[0] = FM_ACTION_FREE;
	args[1] = ah->handle;
	ret = flowman_cmd(fm, args, 2);
	if (ret != 0)
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "enic: devcmd(action-free)");
error_with_ah:
	free(ah);
	return ret;
}

/* Push match-action to the NIC. */
static int
__enic_fm_flow_add_entry(struct enic_flowman *fm,
			 struct enic_fm_flow *fm_flow,
			 struct fm_tcam_match_entry *match_in,
			 struct fm_action *action_in,
			 uint32_t group,
			 uint8_t ingress,
			 struct rte_flow_error *error)
{
	struct enic_fm_counter *ctr;
	struct enic_fm_action *ah = NULL;
	uint64_t entry_h;
	int ret;

	ENICPMD_FUNC_TRACE();

	/* Get or create an aciton handle. */
	ret = enic_action_handle_get(fm, action_in, error, &ah);
	if (ret)
		return ret;
	match_in->ftm_action = ah->handle;
	fm_flow->action = ah;

	/* Allocate counter if requested. */
	if (match_in->ftm_flags & FMEF_COUNTER) {
		ret = enic_fm_counter_alloc(fm, error, &ctr);
		if (ret) /* error has been filled in */
			return ret;
		fm_flow->counter_valid = true;
		fm_flow->counter = ctr;
		match_in->ftm_counter = ctr->handle;
	}

	/*
	 * Get the group's table (either TCAM or exact match table) and
	 * add entry to it. If we use the exact match table, the handler
	 * will translate the TCAM entry (match_in) to the appropriate
	 * exact match entry and use that instead.
	 */
	entry_h = FM_INVALID_HANDLE;
	if (group == FM_TCAM_RTE_GROUP) {
		ret = enic_fm_add_tcam_entry(fm, match_in, &entry_h, ingress,
					     error);
		if (ret)
			return ret;
		/* Jump action might have a ref to fet */
		fm_flow->fet = fm->fet;
		fm->fet = NULL;
	} else {
		struct enic_fm_fet *fet = NULL;

		ret = enic_fet_get(fm, group, ingress,
				   &match_in->ftm_mask, &fet, error);
		if (ret)
			return ret;
		fm_flow->fet = fet;
		ret = enic_fm_add_exact_entry(fm, match_in, &entry_h, fet,
					      error);
		if (ret)
			return ret;
	}
	/* Clear counter after adding entry, as it requires in-use counter */
	if (fm_flow->counter_valid) {
		ret = enic_fm_counter_zero(fm, fm_flow->counter);
		if (ret)
			return ret;
	}
	fm_flow->entry_handle = entry_h;
	return 0;
}

/* Push match-action to the NIC. */
static struct rte_flow *
enic_fm_flow_add_entry(struct enic_flowman *fm,
		       struct fm_tcam_match_entry *match_in,
		       struct fm_action *action_in,
		       const struct rte_flow_attr *attrs,
		       struct rte_flow_error *error)
{
	struct enic_fm_flow *fm_flow;
	struct rte_flow *flow;

	ENICPMD_FUNC_TRACE();
	match_in->ftm_position = attrs->priority;
	enic_fm_dump_tcam_entry(match_in, action_in, attrs->ingress);
	flow = calloc(1, sizeof(*flow));
	fm_flow = calloc(1, sizeof(*fm_flow));
	if (flow == NULL || fm_flow == NULL) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
			NULL, "enic: cannot allocate rte_flow");
		free(flow);
		free(fm_flow);
		return NULL;
	}
	flow->fm = fm_flow;
	fm_flow->action = NULL;
	fm_flow->entry_handle = FM_INVALID_HANDLE;
	if (__enic_fm_flow_add_entry(fm, fm_flow, match_in, action_in,
				     attrs->group, attrs->ingress, error)) {
		enic_fm_flow_free(fm, flow);
		return NULL;
	}
	return flow;
}

static void
convert_jump_flows(struct enic_flowman *fm, struct enic_fm_fet *fet,
		   struct rte_flow_error *error)
{
	struct enic_fm_flow *fm_flow;
	struct enic_fm_jump_flow *j;
	struct fm_action *fma;
	uint32_t group;

	ENICPMD_FUNC_TRACE();
	/*
	 * Find the saved flows that should jump to the new table (fet).
	 * Then delete the old TCAM entry that jumps to the default table,
	 * and add a new one that jumps to the new table.
	 */
	group = fet->group;
	j = find_jump_flow(fm, group);
	while (j) {
		ENICPMD_LOG(DEBUG, "convert jump flow: flow=%p group=%u",
			    j->flow, group);
		/* Delete old entry */
		fm_flow = j->flow->fm;
		__enic_fm_flow_free(fm, fm_flow);

		/* Add new entry */
		fma = &j->action;
		fma->fma_action_ops[0].exact.handle = fet->handle;
		if (__enic_fm_flow_add_entry(fm, fm_flow, &j->match, fma,
			FM_TCAM_RTE_GROUP, fet->ingress, error)) {
			/* Cannot roll back changes at the moment */
			ENICPMD_LOG(ERR, "cannot convert jump flow: flow=%p",
				    j->flow);
		} else {
			fm_flow->fet = fet;
			fet->ref++;
			ENICPMD_LOG(DEBUG, "convert ok: group=%u ref=%u",
				    fet->group, fet->ref);
		}

		TAILQ_REMOVE(&fm->jump_list, j, list);
		free(j);
		j = find_jump_flow(fm, group);
	}
}

static int
add_hairpin_steer(struct enic_flowman *fm, struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct fm_tcam_match_entry *fm_tcam_entry;
	struct enic_fm_flow *fm_flow;
	struct fm_action *fm_action;
	struct fm_action_op fm_op;
	int ret;

	ENICPMD_FUNC_TRACE();
	fm_flow = calloc(1, sizeof(*fm_flow));
	if (fm_flow == NULL) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
			NULL, "enic: cannot allocate rte_flow");
		return -ENOMEM;
	}
	/* Original egress hairpin flow */
	fm_tcam_entry = &fm->tcam_entry;
	fm_action = &fm->action;
	/* Use the match pattern of the egress flow as is, without counters */
	fm_tcam_entry->ftm_flags &= ~FMEF_COUNTER;
	/* The only action is steer to vnic */
	fm->action_op_count = 0;
	memset(fm_action, 0, sizeof(*fm_action));
	memset(&fm_op, 0, sizeof(fm_op));
	/* Always to queue 0 for now */
	fm_op.fa_op = FMOP_RQ_STEER;
	fm_op.rq_steer.rq_index = 0;
	fm_op.rq_steer.vnic_handle = fm->hairpin_steer_vnic_h;
	ret = enic_fm_append_action_op(fm, &fm_op, error);
	if (ret)
		goto error_with_flow;
	ENICPMD_LOG(DEBUG, "add steer op");
	/* Add required END */
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	ret = enic_fm_append_action_op(fm, &fm_op, error);
	if (ret)
		goto error_with_flow;
	/* Add the ingress flow */
	fm_flow->action = NULL;
	fm_flow->entry_handle = FM_INVALID_HANDLE;
	ret = __enic_fm_flow_add_entry(fm, fm_flow, fm_tcam_entry, fm_action,
				       FM_TCAM_RTE_GROUP, 1 /* ingress */, error);
	if (ret) {
		ENICPMD_LOG(ERR, "cannot add hairpin-steer flow");
		goto error_with_flow;
	}
	/* The new flow is now the egress flow's paired flow */
	flow->fm->hairpin_steer_flow = fm_flow;
	return 0;

error_with_flow:
	free(fm_flow);
	return ret;
}

static void
enic_fm_open_scratch(struct enic_flowman *fm)
{
	fm->action_op_count = 0;
	fm->fet = NULL;
	fm->need_hairpin_steer = 0;
	fm->hairpin_steer_vnic_h = 0;
	memset(&fm->tcam_entry, 0, sizeof(fm->tcam_entry));
	memset(&fm->action, 0, sizeof(fm->action));
}

static void
enic_fm_close_scratch(struct enic_flowman *fm)
{
	if (fm->fet) {
		enic_fet_put(fm, fm->fet);
		fm->fet = NULL;
	}
	fm->action_op_count = 0;
}

static int
enic_fm_flow_validate(struct rte_eth_dev *dev,
		      const struct rte_flow_attr *attrs,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	struct fm_tcam_match_entry *fm_tcam_entry;
	struct fm_action *fm_action;
	struct enic_flowman *fm;
	int ret;

	ENICPMD_FUNC_TRACE();
	fm = begin_fm(pmd_priv(dev));
	if (fm == NULL)
		return -ENOTSUP;
	enic_fm_open_scratch(fm);
	ret = enic_fm_flow_parse(fm, attrs, pattern, actions, error);
	if (!ret) {
		fm_tcam_entry = &fm->tcam_entry;
		fm_action = &fm->action;
		enic_fm_dump_tcam_entry(fm_tcam_entry, fm_action,
					attrs->ingress);
	}
	enic_fm_close_scratch(fm);
	end_fm(fm);
	return ret;
}

static int
enic_fm_flow_query_count(struct rte_eth_dev *dev,
			 struct rte_flow *flow, void *data,
			 struct rte_flow_error *error)
{
	struct rte_flow_query_count *query;
	struct enic_fm_flow *fm_flow;
	struct enic_flowman *fm;
	uint64_t args[3];
	int rc;

	ENICPMD_FUNC_TRACE();
	fm = begin_fm(pmd_priv(dev));
	query = data;
	fm_flow = flow->fm;
	if (!fm_flow->counter_valid) {
		rc = rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"enic: flow does not have counter");
		goto exit;
	}

	args[0] = FM_COUNTER_QUERY;
	args[1] = fm_flow->counter->handle;
	args[2] = query->reset;
	rc = flowman_cmd(fm, args, 3);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot query counter: rc=%d handle=0x%x",
			    rc, fm_flow->counter->handle);
		goto exit;
	}
	query->hits_set = 1;
	query->hits = args[0];
	query->bytes_set = 1;
	query->bytes = args[1];
	rc = 0;
exit:
	end_fm(fm);
	return rc;
}

static int
enic_fm_flow_query(struct rte_eth_dev *dev,
		   struct rte_flow *flow,
		   const struct rte_flow_action *actions,
		   void *data,
		   struct rte_flow_error *error)
{
	int ret = 0;

	ENICPMD_FUNC_TRACE();
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = enic_fm_flow_query_count(dev, flow, data, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
		if (ret < 0)
			return ret;
	}
	return 0;
}

static struct rte_flow *
enic_fm_flow_create(struct rte_eth_dev *dev,
		    const struct rte_flow_attr *attrs,
		    const struct rte_flow_item pattern[],
		    const struct rte_flow_action actions[],
		    struct rte_flow_error *error)
{
	struct fm_tcam_match_entry *fm_tcam_entry;
	struct fm_action *fm_action;
	struct enic_flowman *fm;
	struct enic_fm_fet *fet;
	struct rte_flow *flow;
	struct enic *enic;
	int ret;

	ENICPMD_FUNC_TRACE();
	enic = pmd_priv(dev);
	fm = begin_fm(enic);
	if (fm == NULL) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"flowman is not initialized");
		return NULL;
	}
	enic_fm_open_scratch(fm);
	flow = NULL;
	ret = enic_fm_flow_parse(fm, attrs, pattern, actions, error);
	if (ret < 0)
		goto error_with_scratch;
	fm_tcam_entry = &fm->tcam_entry;
	fm_action = &fm->action;
	flow = enic_fm_flow_add_entry(fm, fm_tcam_entry, fm_action,
				      attrs, error);
	if (flow) {
		/* Add ingress rule that pairs with hairpin rule */
		if (fm->need_hairpin_steer) {
			ret = add_hairpin_steer(fm, flow, error);
			if (ret) {
				enic_fm_flow_free(fm, flow);
				flow = NULL;
				goto error_with_scratch;
			}
		}
		LIST_INSERT_HEAD(&enic->flows, flow, next);
		fet = flow->fm->fet;
		if (fet && fet->default_key) {
			/*
			 * Jump to non-existent group? Save the relevant info
			 * so we can convert this flow when that group
			 * materializes.
			 */
			save_jump_flow(fm, flow, fet->group,
				       fm_tcam_entry, fm_action);
		} else if (fet && fet->ref == 1) {
			/*
			 * A new table is created. Convert the saved flows
			 * that should jump to this group.
			 */
			convert_jump_flows(fm, fet, error);
		}
	}

error_with_scratch:
	enic_fm_close_scratch(fm);
	end_fm(fm);
	return flow;
}

static int
enic_fm_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		     __rte_unused struct rte_flow_error *error)
{
	struct enic *enic = pmd_priv(dev);
	struct enic_flowman *fm;

	ENICPMD_FUNC_TRACE();
	fm = begin_fm(enic);
	if (fm == NULL)
		return 0;
	LIST_REMOVE(flow, next);
	enic_fm_flow_free(fm, flow);
	end_fm(fm);
	return 0;
}

static int
enic_fm_flow_flush(struct rte_eth_dev *dev,
		   __rte_unused struct rte_flow_error *error)
{
	LIST_HEAD(enic_flows, rte_flow) internal;
	struct enic_fm_flow *fm_flow;
	struct enic_flowman *fm;
	struct rte_flow *flow;
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();

	fm = begin_fm(enic);
	if (fm == NULL)
		return 0;
	/* Destroy all non-internal flows */
	LIST_INIT(&internal);
	while (!LIST_EMPTY(&enic->flows)) {
		flow = LIST_FIRST(&enic->flows);
		fm_flow = flow->fm;
		LIST_REMOVE(flow, next);
		if (flow->internal) {
			LIST_INSERT_HEAD(&internal, flow, next);
			continue;
		}
		/*
		 * If tables are null, then vNIC is closing, and the firmware
		 * has already cleaned up flowman state. So do not try to free
		 * resources, as it only causes errors.
		 */
		if (fm->ig_tcam_hndl == FM_INVALID_HANDLE) {
			fm_flow->entry_handle = FM_INVALID_HANDLE;
			fm_flow->action = NULL;
			fm_flow->fet = NULL;
		}
		enic_fm_flow_free(fm, flow);
	}
	while (!LIST_EMPTY(&internal)) {
		flow = LIST_FIRST(&internal);
		LIST_REMOVE(flow, next);
		LIST_INSERT_HEAD(&enic->flows, flow, next);
	}
	end_fm(fm);
	return 0;
}

static int
enic_fm_tbl_free(struct enic_flowman *fm, uint64_t handle)
{
	uint64_t args[2];
	int rc;

	args[0] = FM_MATCH_TABLE_FREE;
	args[1] = handle;
	rc = flowman_cmd(fm, args, 2);
	if (rc)
		ENICPMD_LOG(ERR, "cannot free table: rc=%d handle=0x%" PRIx64,
			    rc, handle);
	return rc;
}

static int
enic_fm_tcam_tbl_alloc(struct enic_flowman *fm, uint32_t direction,
			uint32_t max_entries, uint64_t *handle)
{
	struct fm_tcam_match_table *tcam_tbl;
	uint64_t args[2];
	int rc;

	ENICPMD_FUNC_TRACE();
	tcam_tbl = &fm->cmd.va->fm_tcam_match_table;
	tcam_tbl->ftt_direction = direction;
	tcam_tbl->ftt_stage = FM_STAGE_LAST;
	tcam_tbl->ftt_max_entries = max_entries;
	args[0] = FM_TCAM_TABLE_ALLOC;
	args[1] = fm->cmd.pa;
	rc = flowman_cmd(fm, args, 2);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot alloc %s TCAM table: rc=%d",
			    (direction == FM_INGRESS) ? "IG" : "EG", rc);
		return rc;
	}
	*handle = args[0];
	ENICPMD_LOG(DEBUG, "%s TCAM table allocated, handle=0x%" PRIx64,
		    (direction == FM_INGRESS) ? "IG" : "EG", *handle);
	return 0;
}

static int
enic_fm_init_actions(struct enic_flowman *fm)
{
	struct rte_hash *a_hash;
	char name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters params = {
		.entries = FM_MAX_ACTION_TABLE_SIZE,
		.key_len = sizeof(struct fm_action),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	ENICPMD_FUNC_TRACE();
	snprintf((char *)name, sizeof(name), "fm-ah-%s",
		 fm->owner_enic->bdf_name);
	params.name = name;

	a_hash = rte_hash_create(&params);
	if (a_hash == NULL)
		return -rte_errno;
	fm->action_hash = a_hash;
	return 0;
}

static int
enic_fm_init_counters(struct enic_flowman *fm)
{
	ENICPMD_FUNC_TRACE();
	SLIST_INIT(&fm->counters);
	return enic_fm_more_counters(fm);
}

static void
enic_fm_free_all_counters(struct enic_flowman *fm)
{
	uint64_t args[2];
	int rc;

	args[0] = FM_COUNTER_BRK;
	args[1] = 0;
	rc = flowman_cmd(fm, args, 2);
	if (rc != 0)
		ENICPMD_LOG(ERR, "cannot free counters: rc=%d", rc);
	rte_free(fm->counter_stack);
}

static int
enic_fm_alloc_tcam_tables(struct enic_flowman *fm)
{
	int rc;

	ENICPMD_FUNC_TRACE();
	rc = enic_fm_tcam_tbl_alloc(fm, FM_INGRESS, FM_MAX_TCAM_TABLE_SIZE,
				    &fm->ig_tcam_hndl);
	if (rc)
		return rc;
	rc = enic_fm_tcam_tbl_alloc(fm, FM_EGRESS, FM_MAX_TCAM_TABLE_SIZE,
				    &fm->eg_tcam_hndl);
	return rc;
}

static void
enic_fm_free_tcam_tables(struct enic_flowman *fm)
{
	ENICPMD_FUNC_TRACE();
	if (fm->ig_tcam_hndl) {
		ENICPMD_LOG(DEBUG, "free IG TCAM table handle=0x%" PRIx64,
			    fm->ig_tcam_hndl);
		enic_fm_tbl_free(fm, fm->ig_tcam_hndl);
		fm->ig_tcam_hndl = FM_INVALID_HANDLE;
	}
	if (fm->eg_tcam_hndl) {
		ENICPMD_LOG(DEBUG, "free EG TCAM table handle=0x%" PRIx64,
			    fm->eg_tcam_hndl);
		enic_fm_tbl_free(fm, fm->eg_tcam_hndl);
		fm->eg_tcam_hndl = FM_INVALID_HANDLE;
	}
}

int
enic_fm_init(struct enic *enic)
{
	const struct rte_pci_addr *addr;
	struct enic_flowman *fm;
	uint8_t name[RTE_MEMZONE_NAMESIZE];
	int rc;

	if (enic->flow_filter_mode != FILTER_FLOWMAN)
		return 0;
	ENICPMD_FUNC_TRACE();
	/* Get vnic handle and save for port-id action */
	if (enic_is_vf_rep(enic))
		addr = &VF_ENIC_TO_VF_REP(enic)->bdf;
	else
		addr = &RTE_ETH_DEV_TO_PCI(enic->rte_dev)->addr;
	rc = enic_fm_find_vnic(enic, addr, &enic->fm_vnic_handle);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot find vnic handle for %x:%x:%x",
			    addr->bus, addr->devid, addr->function);
		return rc;
	}
	/* Save UIF for egport action */
	enic->fm_vnic_uif = vnic_dev_uif(enic->vdev);
	ENICPMD_LOG(DEBUG, "uif %u", enic->fm_vnic_uif);
	/* Nothing else to do for representor. It will share the PF flowman */
	if (enic_is_vf_rep(enic))
		return 0;
	fm = calloc(1, sizeof(*fm));
	if (fm == NULL) {
		ENICPMD_LOG(ERR, "cannot alloc flowman struct");
		return -ENOMEM;
	}
	fm->owner_enic = enic;
	rte_spinlock_init(&fm->lock);
	TAILQ_INIT(&fm->fet_list);
	TAILQ_INIT(&fm->jump_list);
	/* Allocate host memory for flowman commands */
	snprintf((char *)name, sizeof(name), "fm-cmd-%s", enic->bdf_name);
	fm->cmd.va = enic_alloc_consistent(enic,
		sizeof(union enic_flowman_cmd_mem), &fm->cmd.pa, name);
	if (!fm->cmd.va) {
		ENICPMD_LOG(ERR, "cannot allocate flowman command memory");
		rc = -ENOMEM;
		goto error_fm;
	}
	/* Allocate TCAM tables upfront as they are the main tables */
	rc = enic_fm_alloc_tcam_tables(fm);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot alloc TCAM tables");
		goto error_cmd;
	}
	/* Then a number of counters */
	rc = enic_fm_init_counters(fm);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot alloc counters");
		goto error_tables;
	}
	/* set up action handle hash */
	rc = enic_fm_init_actions(fm);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot create action hash, error:%d", rc);
		goto error_counters;
	}
	/*
	 * One default exact match table for each direction. We hold onto
	 * it until close.
	 */
	rc = enic_fet_alloc(fm, 1, NULL, 128, &fm->default_ig_fet);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot alloc default IG exact match table");
		goto error_actions;
	}
	fm->default_ig_fet->ref = 1;
	rc = enic_fet_alloc(fm, 0, NULL, 128, &fm->default_eg_fet);
	if (rc) {
		ENICPMD_LOG(ERR, "cannot alloc default EG exact match table");
		goto error_ig_fet;
	}
	fm->default_eg_fet->ref = 1;
	fm->vf_rep_tag = FM_VF_REP_TAG;
	enic->fm = fm;
	return 0;

error_ig_fet:
	enic_fet_free(fm, fm->default_ig_fet);
error_actions:
	rte_hash_free(fm->action_hash);
error_counters:
	enic_fm_free_all_counters(fm);
error_tables:
	enic_fm_free_tcam_tables(fm);
error_cmd:
	enic_free_consistent(enic, sizeof(union enic_flowman_cmd_mem),
		fm->cmd.va, fm->cmd.pa);
error_fm:
	free(fm);
	return rc;
}

void
enic_fm_destroy(struct enic *enic)
{
	struct enic_flowman *fm;
	struct enic_fm_fet *fet;

	ENICPMD_FUNC_TRACE();
	if (enic_is_vf_rep(enic)) {
		delete_rep_flows(enic);
		return;
	}
	if (enic->fm == NULL)
		return;
	fm = enic->fm;
	enic_fm_flow_flush(enic->rte_dev, NULL);
	enic_fet_free(fm, fm->default_eg_fet);
	enic_fet_free(fm, fm->default_ig_fet);
	/* Free all exact match tables still open */
	while (!TAILQ_EMPTY(&fm->fet_list)) {
		fet = TAILQ_FIRST(&fm->fet_list);
		enic_fet_free(fm, fet);
	}
	enic_fm_free_tcam_tables(fm);
	enic_fm_free_all_counters(fm);
	rte_hash_free(fm->action_hash);
	enic_free_consistent(enic, sizeof(union enic_flowman_cmd_mem),
		fm->cmd.va, fm->cmd.pa);
	fm->cmd.va = NULL;
	free(fm);
	enic->fm = NULL;
}

int
enic_fm_allocate_switch_domain(struct enic *pf)
{
	const struct rte_pci_addr *cur_a, *prev_a;
	struct rte_eth_dev *dev;
	struct enic *cur, *prev;
	uint16_t domain_id;
	uint64_t vnic_h;
	uint16_t pid;
	int ret;

	ENICPMD_FUNC_TRACE();
	if (enic_is_vf_rep(pf))
		return -EINVAL;
	cur = pf;
	cur_a = &RTE_ETH_DEV_TO_PCI(cur->rte_dev)->addr;
	/* Go through ports and find another PF that is on the same adapter */
	RTE_ETH_FOREACH_DEV(pid) {
		dev = &rte_eth_devices[pid];
		if (!dev_is_enic(dev))
			continue;
		if (dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)
			continue;
		if (dev == cur->rte_dev)
			continue;
		/* dev is another PF. Is it on the same adapter? */
		prev = pmd_priv(dev);
		prev_a = &RTE_ETH_DEV_TO_PCI(dev)->addr;
		if (!enic_fm_find_vnic(cur, prev_a, &vnic_h)) {
			ENICPMD_LOG(DEBUG, "Port %u (PF BDF %x:%x:%x) and port %u (PF BDF %x:%x:%x domain %u) are on the same VIC",
				cur->rte_dev->data->port_id,
				cur_a->bus, cur_a->devid, cur_a->function,
				dev->data->port_id,
				prev_a->bus, prev_a->devid, prev_a->function,
				prev->switch_domain_id);
			cur->switch_domain_id = prev->switch_domain_id;
			return 0;
		}
	}
	ret = rte_eth_switch_domain_alloc(&domain_id);
	if (ret) {
		ENICPMD_LOG(WARNING, "failed to allocate switch domain for device %d",
			    ret);
	}
	cur->switch_domain_id = domain_id;
	ENICPMD_LOG(DEBUG, "Port %u (PF BDF %x:%x:%x) is the 1st PF on the VIC. Allocated switch domain id %u",
		    cur->rte_dev->data->port_id,
		    cur_a->bus, cur_a->devid, cur_a->function,
		    domain_id);
	return ret;
}

const struct rte_flow_ops enic_fm_flow_ops = {
	.validate = enic_fm_flow_validate,
	.create = enic_fm_flow_create,
	.destroy = enic_fm_flow_destroy,
	.flush = enic_fm_flow_flush,
	.query = enic_fm_flow_query,
};

/* Add a high priority flow that loops representor packets to VF */
int
enic_fm_add_rep2vf_flow(struct enic_vf_representor *vf)
{
	struct fm_tcam_match_entry *fm_tcam_entry;
	struct rte_flow *flow0, *flow1;
	struct fm_action *fm_action;
	struct rte_flow_error error;
	struct rte_flow_attr attrs;
	struct fm_action_op fm_op;
	struct enic_flowman *fm;
	struct enic *pf;
	uint8_t tag;

	pf = vf->pf;
	fm = pf->fm;
	tag = fm->vf_rep_tag;
	enic_fm_open_scratch(fm);
	fm_tcam_entry = &fm->tcam_entry;
	fm_action = &fm->action;
	/* Egress rule: match WQ ID and tag+hairpin */
	fm_tcam_entry->ftm_data.fk_wq_id = vf->pf_wq_idx;
	fm_tcam_entry->ftm_mask.fk_wq_id = 0xffff;
	fm_tcam_entry->ftm_flags |= FMEF_COUNTER;
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_TAG;
	fm_op.tag.tag = tag;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_EG_HAIRPIN;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	enic_fm_append_action_op(fm, &fm_op, &error);
	attrs.group = 0;
	attrs.ingress = 0;
	attrs.egress = 1;
	attrs.priority = FM_HIGHEST_PRIORITY;
	flow0 = enic_fm_flow_add_entry(fm, fm_tcam_entry, fm_action,
				       &attrs, &error);
	enic_fm_close_scratch(fm);
	if (flow0 == NULL) {
		ENICPMD_LOG(ERR, "Cannot create flow 0 for representor->VF");
		return -EINVAL;
	}
	LIST_INSERT_HEAD(&pf->flows, flow0, next);
	/* Make this flow internal, so the user app cannot delete it */
	flow0->internal = 1;
	ENICPMD_LOG(DEBUG, "representor->VF %d flow created: wq %d -> tag %d hairpin",
		    vf->vf_id, vf->pf_wq_idx, tag);

	/* Ingress: steer hairpinned to VF RQ 0 */
	enic_fm_open_scratch(fm);
	fm_tcam_entry->ftm_flags |= FMEF_COUNTER;
	fm_tcam_entry->ftm_data.fk_hdrset[0].fk_metadata |= FKM_EG_HAIRPINNED;
	fm_tcam_entry->ftm_mask.fk_hdrset[0].fk_metadata |= FKM_EG_HAIRPINNED;
	fm_tcam_entry->ftm_data.fk_packet_tag = tag;
	fm_tcam_entry->ftm_mask.fk_packet_tag = 0xff;
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_RQ_STEER;
	fm_op.rq_steer.rq_index = 0;
	fm_op.rq_steer.vnic_handle = vf->enic.fm_vnic_handle;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	enic_fm_append_action_op(fm, &fm_op, &error);
	attrs.group = 0;
	attrs.ingress = 1;
	attrs.egress = 0;
	attrs.priority = FM_HIGHEST_PRIORITY;
	flow1 = enic_fm_flow_add_entry(fm, fm_tcam_entry, fm_action,
				       &attrs, &error);
	enic_fm_close_scratch(fm);
	if (flow1 == NULL) {
		ENICPMD_LOG(ERR, "Cannot create flow 1 for representor->VF");
		enic_fm_flow_destroy(pf->rte_dev, flow0, &error);
		return -EINVAL;
	}
	LIST_INSERT_HEAD(&pf->flows, flow1, next);
	flow1->internal = 1;
	ENICPMD_LOG(DEBUG, "representor->VF %d flow created: tag %d hairpinned -> VF RQ %d",
		    vf->vf_id, tag, fm_op.rq_steer.rq_index);
	vf->rep2vf_flow[0] = flow0;
	vf->rep2vf_flow[1] = flow1;
	/* Done with this tag, use a different one next time */
	fm->vf_rep_tag++;
	return 0;
}

/*
 * Add a low priority flow that matches all packets from VF and loops them
 * back to the representor.
 */
int
enic_fm_add_vf2rep_flow(struct enic_vf_representor *vf)
{
	struct fm_tcam_match_entry *fm_tcam_entry;
	struct rte_flow *flow0, *flow1;
	struct fm_action *fm_action;
	struct rte_flow_error error;
	struct rte_flow_attr attrs;
	struct fm_action_op fm_op;
	struct enic_flowman *fm;
	struct enic *pf;
	uint8_t tag;

	pf = vf->pf;
	fm = pf->fm;
	tag = fm->vf_rep_tag;
	enic_fm_open_scratch(fm);
	fm_tcam_entry = &fm->tcam_entry;
	fm_action = &fm->action;
	/* Egress rule: match-any and tag+hairpin */
	fm_tcam_entry->ftm_data.fk_wq_id = 0;
	fm_tcam_entry->ftm_mask.fk_wq_id = 0xffff;
	fm_tcam_entry->ftm_data.fk_wq_vnic = vf->enic.fm_vnic_handle;
	fm_tcam_entry->ftm_flags |= FMEF_COUNTER;
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_TAG;
	fm_op.tag.tag = tag;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_EG_HAIRPIN;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	enic_fm_append_action_op(fm, &fm_op, &error);
	attrs.group = 0;
	attrs.ingress = 0;
	attrs.egress = 1;
	attrs.priority = FM_LOWEST_PRIORITY;
	flow0 = enic_fm_flow_add_entry(fm, fm_tcam_entry, fm_action,
				       &attrs, &error);
	enic_fm_close_scratch(fm);
	if (flow0 == NULL) {
		ENICPMD_LOG(ERR, "Cannot create flow 0 for VF->representor");
		return -EINVAL;
	}
	LIST_INSERT_HEAD(&pf->flows, flow0, next);
	/* Make this flow internal, so the user app cannot delete it */
	flow0->internal = 1;
	ENICPMD_LOG(DEBUG, "VF %d->representor flow created: wq %d (low prio) -> tag %d hairpin",
		    vf->vf_id, fm_tcam_entry->ftm_data.fk_wq_id, tag);

	/* Ingress: steer hairpinned to VF rep RQ */
	enic_fm_open_scratch(fm);
	fm_tcam_entry->ftm_flags |= FMEF_COUNTER;
	fm_tcam_entry->ftm_data.fk_hdrset[0].fk_metadata |= FKM_EG_HAIRPINNED;
	fm_tcam_entry->ftm_mask.fk_hdrset[0].fk_metadata |= FKM_EG_HAIRPINNED;
	fm_tcam_entry->ftm_data.fk_packet_tag = tag;
	fm_tcam_entry->ftm_mask.fk_packet_tag = 0xff;
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_RQ_STEER;
	fm_op.rq_steer.rq_index = vf->pf_rq_sop_idx;
	fm_op.rq_steer.vnic_handle = pf->fm_vnic_handle;
	enic_fm_append_action_op(fm, &fm_op, &error);
	memset(&fm_op, 0, sizeof(fm_op));
	fm_op.fa_op = FMOP_END;
	enic_fm_append_action_op(fm, &fm_op, &error);
	attrs.group = 0;
	attrs.ingress = 1;
	attrs.egress = 0;
	attrs.priority = FM_HIGHEST_PRIORITY;
	flow1 = enic_fm_flow_add_entry(fm, fm_tcam_entry, fm_action,
				       &attrs, &error);
	enic_fm_close_scratch(fm);
	if (flow1 == NULL) {
		ENICPMD_LOG(ERR, "Cannot create flow 1 for VF->representor");
		enic_fm_flow_destroy(pf->rte_dev, flow0, &error);
		return -EINVAL;
	}
	LIST_INSERT_HEAD(&pf->flows, flow1, next);
	flow1->internal = 1;
	ENICPMD_LOG(DEBUG, "VF %d->representor flow created: tag %d hairpinned -> PF RQ %d",
		    vf->vf_id, tag, vf->pf_rq_sop_idx);
	vf->vf2rep_flow[0] = flow0;
	vf->vf2rep_flow[1] = flow1;
	/* Done with this tag, use a different one next time */
	fm->vf_rep_tag++;
	return 0;
}

/* Destroy representor flows created by enic_fm_add_{rep2vf,vf2rep}_flow */
static void
delete_rep_flows(struct enic *enic)
{
	struct enic_vf_representor *vf;
	struct rte_flow_error error;
	struct rte_eth_dev *dev;
	uint32_t i;

	RTE_ASSERT(enic_is_vf_rep(enic));
	vf = VF_ENIC_TO_VF_REP(enic);
	dev = vf->pf->rte_dev;
	for (i = 0; i < ARRAY_SIZE(vf->vf2rep_flow); i++) {
		if (vf->vf2rep_flow[i])
			enic_fm_flow_destroy(dev, vf->vf2rep_flow[i], &error);
	}
	for (i = 0; i < ARRAY_SIZE(vf->rep2vf_flow); i++) {
		if (vf->rep2vf_flow[i])
			enic_fm_flow_destroy(dev, vf->rep2vf_flow[i], &error);
	}
}

static struct enic_flowman *
begin_fm(struct enic *enic)
{
	struct enic_vf_representor *vf;
	struct enic_flowman *fm;

	/* Representor uses PF flowman */
	if (enic_is_vf_rep(enic)) {
		vf = VF_ENIC_TO_VF_REP(enic);
		fm = vf->pf->fm;
	} else {
		fm = enic->fm;
	}
	/* Save the API caller and lock if representors exist */
	if (fm) {
		if (fm->owner_enic->switchdev_mode)
			rte_spinlock_lock(&fm->lock);
		fm->user_enic = enic;
	}
	return fm;
}

static void
end_fm(struct enic_flowman *fm)
{
	fm->user_enic = NULL;
	if (fm->owner_enic->switchdev_mode)
		rte_spinlock_unlock(&fm->lock);
}
