/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */
#ifndef _VNIC_FLOWMAN_H_
#define _VNIC_FLOWMAN_H_

/* This file contains Flow Manager (FM) API of the firmware */

/* Flow manager sub-ops */
enum {
	FM_EXACT_TABLE_ALLOC,
	FM_TCAM_TABLE_ALLOC,
	FM_MATCH_TABLE_FREE,
	FM_COUNTER_BRK,
	FM_COUNTER_QUERY,
	FM_COUNTER_CLEAR_ALL,
	FM_COUNTER_DMA,
	FM_ACTION_ALLOC,
	FM_ACTION_FREE,
	FM_EXACT_ENTRY_INSTALL,
	FM_TCAM_ENTRY_INSTALL,
	FM_MATCH_ENTRY_REMOVE,
	FM_VNIC_FIND,
	FM_API_VERSION_QUERY,
	FM_API_VERSION_SELECT,
	FM_INFO_QUERY
};

/*
 * FKM (flow key metadata) flags used to match packet metadata
 * (e.g. packet is tcp)
 */
#define FKM_BITS		\
	FBIT(FKM_QTAG)		\
	FBIT(FKM_CMD)		\
	FBIT(FKM_IPV4)		\
	FBIT(FKM_IPV6)		\
	FBIT(FKM_ROCE)		\
	FBIT(FKM_UDP)		\
	FBIT(FKM_TCP)		\
	FBIT(FKM_TCPORUDP)	\
	FBIT(FKM_IPFRAG)	\
	FBIT(FKM_NVGRE)		\
	FBIT(FKM_VXLAN)		\
	FBIT(FKM_GENEVE)	\
	FBIT(FKM_NSH)		\
	FBIT(FKM_ROCEV2)	\
	FBIT(FKM_VLAN_PRES)	\
	FBIT(FKM_IPOK)		\
	FBIT(FKM_L4OK)		\
	FBIT(FKM_ROCEOK)	\
	FBIT(FKM_FCSOK)		\
	FBIT(FKM_EG_SPAN)	\
	FBIT(FKM_IG_SPAN)	\
	FBIT(FKM_EG_HAIRPINNED)

/*
 * FKH (flow key header) flags.
 * This selects which headers are valid in the struct.
 * This is distinct from metadata in that metadata is requesting actual
 * selection criteria.  If, for example, a TCAM match with metadata "FKM_UDP"
 * is feeding into an exact match table, there may be no need for the
 * exact match table to also specify FKM_UDP, so FKH_UDP is used to
 * specify that the UDP header fields should be used in the match.
 */
#define FKH_BITS	\
	FBIT(FKH_ETHER)	\
	FBIT(FKH_QTAG)	\
	FBIT(FKH_L2RAW)	\
	FBIT(FKH_IPV4)	\
	FBIT(FKH_IPV6)	\
	FBIT(FKH_L3RAW)	\
	FBIT(FKH_UDP)	\
	FBIT(FKH_TCP)	\
	FBIT(FKH_ICMP)	\
	FBIT(FKH_VXLAN)	\
	FBIT(FKH_L4RAW)

#define FBIT(X) X##_BIT,
enum {
	FKM_BITS
	FKM_BIT_COUNT
};

enum {
	FKH_BITS
	FKH_BIT_COUNT
};
#undef FBIT
#define FBIT(X) X = (1 << X##_BIT),
enum {
	FKM_BITS
};
enum {
	FKH_BITS
};
#undef FBIT

#define FM_ETH_ALEN 6
#define FM_LAYER_SIZE 64

/* Header match pattern */
struct fm_header_set {
	uint32_t fk_metadata;       /* FKM flags */
	uint32_t fk_header_select;  /* FKH flags */
	uint16_t fk_vlan;
	/* L2: Ethernet Header (valid if FKH_ETHER) */
	union {
		struct {
			uint8_t fk_dstmac[FM_ETH_ALEN];
			uint8_t fk_srcmac[FM_ETH_ALEN];
			uint16_t fk_ethtype;
		} __rte_packed eth;
		uint8_t rawdata[FM_LAYER_SIZE];
	} __rte_packed l2;
	/* L3: IPv4 or IPv6 (valid if FKH_IPV4,6) */
	union {
		/* Valid if FKH_IPV4 */
		struct {
			uint8_t fk_ihl_vers;
			uint8_t fk_tos;
			uint16_t fk_tot_len;
			uint16_t fk_id;
			uint16_t fk_frag_off;
			uint8_t fk_ttl;
			uint8_t fk_proto;
			uint16_t fk_check;
			uint32_t fk_saddr;
			uint32_t fk_daddr;
		} __rte_packed ip4;
		/* Valid if FKH_IPV6 */
		struct {
			union {
				struct {
					uint32_t fk_un1_flow;
					uint16_t fk_un1_plen;
					uint8_t fk_un1_nxt;
					uint8_t fk_un1_hlim;
				} unl;
				uint8_t fk_un2_vfc;
			} ctl;
			uint8_t fk_srcip[16];
			uint8_t fk_dstip[16];
		} __rte_packed ip6;
		uint8_t rawdata[FM_LAYER_SIZE];
	} __rte_packed l3;
	/* L4: UDP, TCP, or ICMP (valid if FKH_UDP,TCP,ICMP) */
	union {
		struct {
			uint16_t fk_source;
			uint16_t fk_dest;
			uint16_t fk_len;
			uint16_t fk_check;
		} __rte_packed udp;
		struct {
			uint16_t fk_source;
			uint16_t fk_dest;
			uint32_t fk_seq;
			uint32_t fk_ack_seq;
			uint16_t fk_flags;
			uint16_t fk_window;
			uint16_t fk_check;
			uint16_t fk_urg_ptr;
		} __rte_packed tcp;
		struct {
			uint8_t fk_code;
			uint8_t fk_type;
		} __rte_packed icmp;
		uint8_t rawdata[FM_LAYER_SIZE];
	} __rte_packed l4;
	/* VXLAN (valid if FKH_VXLAN) */
	struct {
		uint8_t fkvx_flags;
		uint8_t fkvx_res0[3];
		uint8_t fkvx_vni[3];
		uint8_t fkvx_res1;
	} __rte_packed vxlan;
	/* Payload or unknown inner-most protocol */
	uint8_t fk_l5_data[64];
} __rte_packed;

/*
 * FK (flow key) template.
 * fk_hdrset specifies a set of headers per layer of encapsulation.
 * Currently FM supports two header sets: outer (0) and inner(1)
 */
#define FM_HDRSET_MAX 2

struct fm_key_template {
	struct fm_header_set fk_hdrset[FM_HDRSET_MAX];
	uint32_t fk_flags;
	uint16_t fk_packet_tag;
	uint16_t fk_packet_size;
	uint16_t fk_port_id;
	uint32_t fk_wq_id;    /* WQ index */
	uint64_t fk_wq_vnic;  /* VNIC handle for WQ index */
} __rte_packed;

/* Action operation types */
enum {
	FMOP_NOP = 0,
	/* End the action chain. */
	FMOP_END,
	/* Drop packet and end the action chain. */
	FMOP_DROP,
	/* Steer packet to an RQ. */
	FMOP_RQ_STEER,
	/*
	 * Jump to an exact match table.
	 * arg1: exact match table handle
	 */
	FMOP_EXACT_MATCH,
	/* Apply CQ-visible mark on packet. Mark is written to RSS HASH. */
	FMOP_MARK,
	/*
	 * Apply CQ-visible mark on packet. Mark is written to a field in
	 * extended CQ. RSS HASH is preserved.
	 */
	FMOP_EXT_MARK,
	/*
	 * Apply internal tag which can be matched in subsequent
	 * stages or hairpin.
	 */
	FMOP_TAG,
	/* Hairpin packet from EG -> IG */
	FMOP_EG_HAIRPIN,
	/* Hairpin packet from IG -> EG */
	FMOP_IG_HAIRPIN,
	/* Encap with VXLAN and inner VLAN from metadata. */
	FMOP_ENCAP_IVLAN,
	/* Encap, no inner VLAN. */
	FMOP_ENCAP_NOIVLAN,
	/* Encap, add inner VLAN if present. */
	FMOP_ENCAP,
	/* Set outer VLAN. */
	FMOP_SET_OVLAN,
	/* Decap when vlan_strip is off */
	FMOP_DECAP_NOSTRIP,
	/* Decap and strip VLAN */
	FMOP_DECAP_STRIP,
	/* Remove outer VLAN */
	FMOP_POP_VLAN,
	/* Set Egress port */
	FMOP_SET_EGPORT,
	/* Steer to an RQ without entering EMIT state */
	FMOP_RQ_STEER_ONLY,
	/* Set VLAN when replicating encapped packets */
	FMOP_SET_ENCAP_VLAN,
	/* Enter EMIT state */
	FMOP_EMIT,
	/* Enter MODIFY state */
	FMOP_MODIFY,
	FMOP_OP_MAX,
};

/*
 * Action operation.
 * Complex actions are achieved by a series of "transform operations"
 * We can have complex transform operations like "decap" or "vxlan
 * encap" and also simple ops like insert this data, add PACKET_LEN to
 * this address, etc.
 */
struct fm_action_op {
	uint32_t fa_op;		/* FMOP flags */

	union {
		struct {
			uint8_t len1_offset;
			uint8_t len1_delta;
			uint8_t len2_offset;
			uint8_t len2_delta;
			uint16_t outer_vlan;
			uint8_t template_offset;
			uint8_t template_len;
		} __rte_packed encap;
		struct {
			uint16_t rq_index;
			uint16_t rq_count;
			uint64_t vnic_handle;
		} __rte_packed rq_steer;
		struct {
			uint16_t vlan;
		} __rte_packed ovlan;
		struct {
			uint16_t vlan;
		} __rte_packed set_encap_vlan;
		struct {
			uint16_t mark;
		} __rte_packed mark;
		struct {
			uint32_t ext_mark;
		} __rte_packed ext_mark;
		struct {
			uint8_t tag;
		} __rte_packed tag;
		struct {
			uint64_t handle;
		} __rte_packed exact;
		struct {
			uint32_t egport;
		} __rte_packed set_egport;
	} __rte_packed;
} __rte_packed;

#define FM_ACTION_OP_MAX 64
#define FM_ACTION_DATA_MAX 96

/*
 * Action is a series of action operations applied to matched
 * packet. FMA (flowman action).
 */
struct fm_action {
	struct fm_action_op fma_action_ops[FM_ACTION_OP_MAX];
	uint8_t fma_data[FM_ACTION_DATA_MAX];
} __rte_packed;

/* Match entry flags. FMEF (flow match entry flag) */
#define FMEF_COUNTER    0x0001  /* counter index is valid */

/* FEM (flow exact match) entry */
struct fm_exact_match_entry {
	struct fm_key_template fem_data;  /* Match data. Mask is per table */
	uint32_t fem_flags;               /* FMEF_xxx */
	uint64_t fem_action;              /* Action handle */
	uint32_t fem_counter;             /* Counter index */
} __rte_packed;

/* FTM (flow TCAM match) entry */
struct fm_tcam_match_entry {
	struct fm_key_template ftm_mask;  /* Key mask */
	struct fm_key_template ftm_data;  /* Match data */
	uint32_t ftm_flags;               /* FMEF_xxx */
	uint32_t ftm_position;            /* Entry position */
	uint64_t ftm_action;              /* Action handle */
	uint32_t ftm_counter;             /* Counter index */
} __rte_packed;

/* Match directions */
enum {
	FM_INGRESS,
	FM_EGRESS,
	FM_DIR_CNT
};

/* Last stage ID, independent of the number of stages in hardware */
#define FM_STAGE_LAST 0xff

/* Hash based exact match table. FET (flow exact match table) */
struct fm_exact_match_table {
	uint8_t fet_direction; /* FM_INGRESS or EGRESS*/
	uint8_t fet_stage;
	uint8_t pad[2];
	uint32_t fet_max_entries;
	uint64_t fet_dflt_action;
	struct fm_key_template fet_key;
} __rte_packed;

/* TCAM based match table. FTT (flow TCAM match table) */
struct fm_tcam_match_table {
	uint8_t ftt_direction;
	uint8_t ftt_stage;
	uint8_t pad[2];
	uint32_t ftt_max_entries;
} __rte_packed;

struct fm_counter_counts {
	uint64_t fcc_packets;
	uint64_t fcc_bytes;
} __rte_packed;

/*
 * Return structure for FM_INFO_QUERY devcmd
 */
#define FM_VERSION 1		/* This header file is for version 1 */

struct fm_info {
	uint64_t fm_op_mask;		/* Bitmask of action supported ops */
	uint64_t fm_current_ts;		/* Current VIC timestamp */
	uint64_t fm_clock_freq;		/* Timestamp clock frequency */
	uint16_t fm_max_ops;		/* Max ops in an action */
	uint8_t fm_stages;		/* Number of match-action stages */
	uint8_t pad[5];
	uint32_t fm_counter_count;	/* Number of allocated counters */
} __rte_packed;

#endif /* _VNIC_FLOWMAN_H_ */
