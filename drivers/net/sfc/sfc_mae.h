/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_MAE_H
#define _SFC_MAE_H

#include <stdbool.h>

#include <rte_spinlock.h>

#include "efx.h"

#include "sfc_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

/** FW-allocatable resource context */
struct sfc_mae_fw_rsrc {
	unsigned int			refcnt;
	union {
		efx_counter_t		counter_id;
		efx_mae_aset_id_t	aset_id;
		efx_mae_rule_id_t	rule_id;
		efx_mae_mac_id_t	mac_id;
		efx_mae_eh_id_t		eh_id;
	};
};

/** Outer rule registry entry */
struct sfc_mae_outer_rule {
	TAILQ_ENTRY(sfc_mae_outer_rule)	entries;
	unsigned int			refcnt;
	efx_mae_match_spec_t		*match_spec;
	efx_tunnel_protocol_t		encap_type;
	struct sfc_mae_fw_rsrc		fw_rsrc;
};

TAILQ_HEAD(sfc_mae_outer_rules, sfc_mae_outer_rule);

/** MAC address registry entry */
struct sfc_mae_mac_addr {
	TAILQ_ENTRY(sfc_mae_mac_addr)	entries;
	unsigned int			refcnt;
	uint8_t				addr_bytes[EFX_MAC_ADDR_LEN];
	struct sfc_mae_fw_rsrc		fw_rsrc;
};

TAILQ_HEAD(sfc_mae_mac_addrs, sfc_mae_mac_addr);

/** Encap. header registry entry */
struct sfc_mae_encap_header {
	TAILQ_ENTRY(sfc_mae_encap_header)	entries;
	unsigned int				refcnt;
	uint8_t					*buf;
	size_t					size;
	efx_tunnel_protocol_t			type;
	struct sfc_mae_fw_rsrc			fw_rsrc;
};

TAILQ_HEAD(sfc_mae_encap_headers, sfc_mae_encap_header);

/* Counter object registry entry */
struct sfc_mae_counter {
	TAILQ_ENTRY(sfc_mae_counter)	entries;
	unsigned int			refcnt;

	/* ID of a counter in RTE */
	uint32_t			rte_id;
	/* RTE counter ID validity status */
	bool				rte_id_valid;

	/* Flow Tunnel (FT) SWITCH hit counter (or NULL) */
	uint64_t			*ft_switch_hit_counter;
	/* Flow Tunnel (FT) context (for TUNNEL rules; otherwise, NULL) */
	struct sfc_ft_ctx		*ft_ctx;

	struct sfc_mae_fw_rsrc		fw_rsrc;

	bool				indirect;
	efx_counter_type_t		type;
};

TAILQ_HEAD(sfc_mae_counters, sfc_mae_counter);

/** Action set registry entry */
struct sfc_mae_action_set {
	TAILQ_ENTRY(sfc_mae_action_set)	entries;
	unsigned int			refcnt;
	efx_mae_actions_t		*spec;
	struct sfc_mae_encap_header	*encap_header;
	struct sfc_mae_mac_addr		*dst_mac_addr;
	struct sfc_mae_mac_addr		*src_mac_addr;
	struct sfc_mae_counter		*counter;
	struct sfc_mae_fw_rsrc		fw_rsrc;
};

TAILQ_HEAD(sfc_mae_action_sets, sfc_mae_action_set);

/** Action rule registry entry */
struct sfc_mae_action_rule {
	TAILQ_ENTRY(sfc_mae_action_rule)	entries;
	uint32_t				ct_mark;
	struct sfc_mae_outer_rule		*outer_rule;
	struct sfc_mae_action_set		*action_set;
	efx_mae_match_spec_t			*match_spec;
	struct sfc_mae_fw_rsrc			fw_rsrc;
	unsigned int				refcnt;
};
TAILQ_HEAD(sfc_mae_action_rules, sfc_mae_action_rule);

/** Options for MAE support status */
enum sfc_mae_status {
	SFC_MAE_STATUS_UNKNOWN = 0,
	SFC_MAE_STATUS_UNSUPPORTED,
	SFC_MAE_STATUS_SUPPORTED,
	SFC_MAE_STATUS_ADMIN,
};

/*
 * Encap. header bounce buffer. It is used to store header data
 * when parsing the header definition in the action VXLAN_ENCAP.
 */
struct sfc_mae_bounce_eh {
	uint8_t				*buf;
	size_t				buf_size;
	size_t				size;
	efx_tunnel_protocol_t		type;
};

/** Counter collection entry */
struct sfc_mae_counter_record {
	bool				inuse;
	uint32_t			generation_count;
	union sfc_pkts_bytes		value;
	union sfc_pkts_bytes		reset;

	uint64_t			*ft_switch_hit_counter;
};

struct sfc_mae_counters_xstats {
	uint64_t			not_inuse_update;
	uint64_t			realloc_update;
};

struct sfc_mae_counter_records {
	/** An array of all MAE counters */
	struct sfc_mae_counter_record	*mae_counters;
	/** Extra statistics for counters */
	struct sfc_mae_counters_xstats	xstats;
	/** Count of all MAE counters */
	unsigned int			n_mae_counters;
	/** Counter type, for logging */
	efx_counter_type_t		type;
};

/** Options for MAE counter polling mode */
enum sfc_mae_counter_polling_mode {
	SFC_MAE_COUNTER_POLLING_OFF = 0,
	SFC_MAE_COUNTER_POLLING_SERVICE,
	SFC_MAE_COUNTER_POLLING_THREAD,
};

struct sfc_mae_counter_registry {
	/* Common counter information */
	/** Action rule counter record collection */
	struct sfc_mae_counter_records	action_counters;
	/** Conntrack counter record collection */
	struct sfc_mae_counter_records	conntrack_counters;

	/* Information used by counter update service */
	/** Callback to get packets from RxQ */
	eth_rx_burst_t			rx_pkt_burst;
	/** Data for the callback to get packets */
	struct sfc_dp_rxq		*rx_dp;
	/** Number of buffers pushed to the RxQ */
	unsigned int			pushed_n_buffers;
	/** Are credits used by counter stream */
	bool				use_credits;

	/* Information used by configuration routines */
	enum sfc_mae_counter_polling_mode polling_mode;
	union {
		struct {
			/** Counter service core ID */
			uint32_t			core_id;
			/** Counter service ID */
			uint32_t			id;
		} service;
		struct {
			/** Counter thread ID */
			pthread_t			id;
			/** The thread should keep running */
			bool				run;
		} thread;
	} polling;
};

struct sfc_mae {
	/** Assigned switch domain identifier */
	uint16_t			switch_domain_id;
	/** Assigned switch port identifier */
	uint16_t			switch_port_id;
	/** NIC support for MAE status */
	enum sfc_mae_status		status;
	/** Priority level limit for MAE outer rules */
	unsigned int			nb_outer_rule_prios_max;
	/** Priority level limit for MAE action rules */
	unsigned int			nb_action_rule_prios_max;
	/** Encapsulation support status */
	uint32_t			encap_types_supported;
	/** Outer rule registry */
	struct sfc_mae_outer_rules	outer_rules;
	/** Encap. header registry */
	struct sfc_mae_encap_headers	encap_headers;
	/** MAC address registry */
	struct sfc_mae_mac_addrs	mac_addrs;
	/** Action set registry */
	struct sfc_mae_action_sets	action_sets;
	/** Action rule registry */
	struct sfc_mae_action_rules	action_rules;
	/** Encap. header bounce buffer */
	struct sfc_mae_bounce_eh	bounce_eh;
	/** Flag indicating whether counter-only RxQ is running */
	bool				counter_rxq_running;
	/** Counter record registry */
	struct sfc_mae_counter_registry	counter_registry;
	/** Counter object registry */
	struct sfc_mae_counters		counters;
	/**
	 * Switchdev default rules. They forward traffic from PHY port
	 * to PF and vice versa.
	 */
	struct rte_flow			*switchdev_rule_pf_to_ext;
	struct rte_flow			*switchdev_rule_ext_to_pf;
};

struct sfc_adapter;
struct sfc_flow_spec;

/** This implementation supports double-tagging */
#define SFC_MAE_MATCH_VLAN_MAX_NTAGS	(2)

/** It is possible to keep track of one item ETH and two items VLAN */
#define SFC_MAE_L2_MAX_NITEMS		(SFC_MAE_MATCH_VLAN_MAX_NTAGS + 1)

/** Auxiliary entry format to keep track of L2 "type" ("inner_type") */
struct sfc_mae_ethertype {
	rte_be16_t	value;
	rte_be16_t	mask;
};

struct sfc_mae_pattern_data {
	/**
	 * Keeps track of "type" ("inner_type") mask and value for each
	 * parsed L2 item in a pattern. These values/masks get filled
	 * in MAE match specification at the end of parsing. Also, this
	 * information is used to conduct consistency checks:
	 *
	 * - If an item ETH is followed by a single item VLAN,
	 *   the former must have "type" set to one of supported
	 *   TPID values (0x8100, 0x88a8, 0x9100, 0x9200, 0x9300),
	 *   or 0x0000/0x0000.
	 *
	 * - If an item ETH is followed by two items VLAN, the
	 *   item ETH must have "type" set to one of supported TPID
	 *   values (0x88a8, 0x9100, 0x9200, 0x9300), or 0x0000/0x0000,
	 *   and the outermost VLAN item must have "inner_type" set
	 *   to TPID value 0x8100, or 0x0000/0x0000
	 *
	 * - If a L2 item is followed by a L3 one, the former must
	 *   indicate "type" ("inner_type") which corresponds to
	 *   the protocol used in the L3 item, or 0x0000/0x0000.
	 *
	 * In turn, mapping between RTE convention (above requirements) and
	 * MAE fields is non-trivial. The following scheme indicates
	 * which item EtherTypes go to which MAE fields in the case
	 * of single tag:
	 *
	 * ETH	(0x8100)	--> VLAN0_PROTO_BE
	 * VLAN	(L3 EtherType)	--> ETHER_TYPE_BE
	 *
	 * Similarly, in the case of double tagging:
	 *
	 * ETH	(0x88a8)	--> VLAN0_PROTO_BE
	 * VLAN	(0x8100)	--> VLAN1_PROTO_BE
	 * VLAN	(L3 EtherType)	--> ETHER_TYPE_BE
	 */
	struct sfc_mae_ethertype	ethertypes[SFC_MAE_L2_MAX_NITEMS];

	rte_be16_t			tci_masks[SFC_MAE_MATCH_VLAN_MAX_NTAGS];

	unsigned int			nb_vlan_tags;

	/**
	 * L3 requirement for the innermost L2 item's "type" ("inner_type").
	 * This contains one of:
	 * - 0x0800/0xffff: IPV4
	 * - 0x86dd/0xffff: IPV6
	 * - 0x0000/0x0000: no L3 item
	 */
	struct sfc_mae_ethertype	innermost_ethertype_restriction;

	/**
	 * The following two fields keep track of L3 "proto" mask and value.
	 * The corresponding fields get filled in MAE match specification
	 * at the end of parsing. Also, the information is used by a
	 * post-check to enforce consistency requirements:
	 *
	 * - If a L3 item is followed by an item TCP, the former has
	 *   its "proto" set to either 0x06/0xff or 0x00/0x00.
	 *
	 * - If a L3 item is followed by an item UDP, the former has
	 *   its "proto" set to either 0x11/0xff or 0x00/0x00.
	 */
	uint8_t				l3_next_proto_value;
	uint8_t				l3_next_proto_mask;

	/*
	 * L4 requirement for L3 item's "proto".
	 * This contains one of:
	 * - 0x06/0xff: TCP
	 * - 0x11/0xff: UDP
	 * - 0x00/0x00: no L4 item
	 */
	uint8_t				l3_next_proto_restriction_value;
	uint8_t				l3_next_proto_restriction_mask;

	rte_be16_t			l3_frag_ofst_value;
	rte_be16_t			l3_frag_ofst_mask;
	rte_be16_t			l3_frag_ofst_last;

	/* Projected state of EFX_MAE_FIELD_HAS_OVLAN match bit */
	bool				has_ovlan_value;
	bool				has_ovlan_mask;

	/* Projected state of EFX_MAE_FIELD_HAS_IVLAN match bit */
	bool				has_ivlan_value;
	bool				has_ivlan_mask;
};

struct sfc_mae_parse_ctx {
	struct sfc_adapter		*sa;
	efx_mae_match_spec_t		*match_spec_action;
	efx_mae_match_spec_t		*match_spec_outer;
	/*
	 * This points to either of the above two specifications depending
	 * on which part of the pattern is being parsed (outer / inner).
	 */
	efx_mae_match_spec_t		*match_spec;
	/*
	 * This points to either "field_ids_remap_to_encap"
	 * or "field_ids_no_remap" (see sfc_mae.c) depending on
	 * which part of the pattern is being parsed.
	 */
	const efx_mae_field_id_t	*field_ids_remap;
	/* These two fields correspond to the tunnel-specific default mask. */
	size_t				tunnel_def_mask_size;
	const void			*tunnel_def_mask;
	bool				match_mport_set;
	bool				internal;
	enum sfc_ft_rule_type		ft_rule_type;
	struct sfc_mae_pattern_data	pattern_data;
	efx_tunnel_protocol_t		encap_type;
	const struct rte_flow_item	*pattern;
	unsigned int			priority;
	struct sfc_ft_ctx		*ft_ctx;
};

int sfc_mae_attach(struct sfc_adapter *sa);
void sfc_mae_detach(struct sfc_adapter *sa);

int sfc_mae_rule_parse(struct sfc_adapter *sa,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct rte_flow *flow, struct rte_flow_error *error);

sfc_flow_cleanup_cb_t sfc_mae_flow_cleanup;
sfc_flow_verify_cb_t sfc_mae_flow_verify;
sfc_flow_insert_cb_t sfc_mae_flow_insert;
sfc_flow_remove_cb_t sfc_mae_flow_remove;
sfc_flow_query_cb_t sfc_mae_flow_query;

/**
 * The value used to represent the lowest priority.
 * Used in MAE rule API.
 */
#define SFC_MAE_RULE_PRIO_LOWEST	(-1)

/**
 * Insert a driver-internal flow rule that matches traffic originating from
 * a source port (REPRESENTED_PORT or PORT_REPRESENTOR) and directs it to
 * its destination counterpart (PORT_REPRESENTOR or REPRESENTED_PORT).
 *
 * If the prio argument is negative, the lowest level will be picked.
 */
struct rte_flow *sfc_mae_repr_flow_create(struct sfc_adapter *sa,
					  int prio, uint16_t port_id,
					  enum rte_flow_action_type dst_type,
					  enum rte_flow_item_type src_type);

void sfc_mae_repr_flow_destroy(struct sfc_adapter *sa, struct rte_flow *flow);

int sfc_mae_switchdev_init(struct sfc_adapter *sa);
void sfc_mae_switchdev_fini(struct sfc_adapter *sa);

int sfc_mae_indir_action_create(struct sfc_adapter *sa,
				const struct rte_flow_action *action,
				struct rte_flow_action_handle *handle,
				struct rte_flow_error *error);

int sfc_mae_indir_action_destroy(struct sfc_adapter *sa,
				 const struct rte_flow_action_handle *handle,
				 struct rte_flow_error *error);

int sfc_mae_indir_action_query(struct sfc_adapter *sa,
			       const struct rte_flow_action_handle *handle,
			       void *data, struct rte_flow_error *error);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_MAE_H */
