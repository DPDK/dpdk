/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_CPCHNL_H_
#define _CPFL_CPCHNL_H_

/** @brief      Command Opcodes
 *              Values are to be different from virtchnl.h opcodes
 */
enum cpchnl2_ops {
	/* vport info */
	CPCHNL2_OP_GET_VPORT_LIST		= 0x8025,
	CPCHNL2_OP_GET_VPORT_INFO		= 0x8026,

	/* DPHMA Event notifications */
	CPCHNL2_OP_EVENT			= 0x8050,
};

/* Note! This affects the size of structs below */
#define CPCHNL2_MAX_TC_AMOUNT		8

#define CPCHNL2_ETH_LENGTH_OF_ADDRESS	6

/* vport statuses - must match the DB ones - see enum cp_vport_status*/
#define CPCHNL2_VPORT_STATUS_CREATED	0
#define CPCHNL2_VPORT_STATUS_ENABLED	1
#define CPCHNL2_VPORT_STATUS_DISABLED	2
#define CPCHNL2_VPORT_STATUS_DESTROYED	3

/* Queue Groups Extension */
/**************************************************/

#define MAX_Q_REGIONS 16
/* TBD - with current structure sizes, in order not to exceed 4KB ICQH buffer
 * no more than 11 queue groups are allowed per a single vport..
 * More will be possible only with future msg fragmentation.
 */
#define MAX_Q_VPORT_GROUPS 11

#define CPCHNL2_CHECK_STRUCT_LEN(n, X) enum static_assert_enum_##X	\
	{ static_assert_##X = (n) / ((sizeof(struct X) == (n)) ? 1 : 0) }

struct cpchnl2_queue_chunk {
	u32 type;	       /* 0:QUEUE_TYPE_TX, 1:QUEUE_TYPE_RX */ /* enum nsl_lan_queue_type */
	u32 start_queue_id;
	u32 num_queues;
	u8 pad[4];
};
CPCHNL2_CHECK_STRUCT_LEN(16, cpchnl2_queue_chunk);

/* structure to specify several chunks of contiguous queues */
struct cpchnl2_queue_grp_chunks {
	u16 num_chunks;
	u8 reserved[6];
	struct cpchnl2_queue_chunk chunks[MAX_Q_REGIONS];
};
CPCHNL2_CHECK_STRUCT_LEN(264, cpchnl2_queue_grp_chunks);

struct cpchnl2_rx_queue_group_info {
	/* User can ask to update rss_lut size originally allocated
	 * by CreateVport command. New size will be returned if allocation succeeded,
	 * otherwise original rss_size from CreateVport will be returned.
	 */
	u16 rss_lut_size;
	u8 pad[6]; /*Future extension purpose*/
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_rx_queue_group_info);

struct cpchnl2_tx_queue_group_info {
	u8 tx_tc; /*TX TC queue group will be connected to*/
	/* Each group can have its own priority, value 0-7, while each group with unique
	 * priority is strict priority. It can be single set of queue groups which configured with
	 * same priority, then they are assumed part of WFQ arbitration group and are expected to be
	 * assigned with weight.
	 */
	u8 priority;
	/* Determines if queue group is expected to be Strict Priority according to its priority */
	u8 is_sp;
	u8 pad;
	/* Peak Info Rate Weight in case Queue Group is part of WFQ arbitration set.
	 * The weights of the groups are independent of each other. Possible values: 1-200.
	 */
	u16 pir_weight;
	/* Future extension purpose for CIR only */
	u8 cir_pad[2];
	u8 pad2[8]; /* Future extension purpose*/
};
CPCHNL2_CHECK_STRUCT_LEN(16, cpchnl2_tx_queue_group_info);

struct cpchnl2_queue_group_id {
	/* Queue group ID - depended on it's type:
	 * Data & p2p - is an index which is relative to Vport.
	 * Config & Mailbox - is an ID which is relative to func.
	 * This ID is used in future calls, i.e. delete.
	 * Requested by host and assigned by Control plane.
	 */
	u16 queue_group_id;
	/* Functional type: see CPCHNL2_QUEUE_GROUP_TYPE definitions */
	u16 queue_group_type;
	u8 pad[4];
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_queue_group_id);

struct cpchnl2_queue_group_info {
	/* IN */
	struct cpchnl2_queue_group_id qg_id;

	/* IN, Number of queues of different types in the group. */
	u16 num_tx_q;
	u16 num_tx_complq;
	u16 num_rx_q;
	u16 num_rx_bufq;

	struct cpchnl2_tx_queue_group_info tx_q_grp_info;
	struct cpchnl2_rx_queue_group_info rx_q_grp_info;

	u8 egress_port;
	u8 pad[39]; /*Future extension purpose*/
	struct cpchnl2_queue_grp_chunks chunks;
};
CPCHNL2_CHECK_STRUCT_LEN(344, cpchnl2_queue_group_info);

struct cpchnl2_queue_groups {
	u16 num_queue_groups; /* Number of queue groups in struct below */
	u8 pad[6];
	/* group information , number is determined by param above */
	struct cpchnl2_queue_group_info groups[MAX_Q_VPORT_GROUPS];
};
CPCHNL2_CHECK_STRUCT_LEN(3792, cpchnl2_queue_groups);

/**
 * @brief function types
 */
enum cpchnl2_func_type {
	CPCHNL2_FTYPE_LAN_VF = 0x0,
	CPCHNL2_FTYPE_LAN_RSV1 = 0x1,
	CPCHNL2_FTYPE_LAN_PF = 0x2,
	CPCHNL2_FTYPE_LAN_RSV2 = 0x3,
	CPCHNL2_FTYPE_LAN_MAX
};

/**
 * @brief containing vport id & type
 */
struct cpchnl2_vport_id {
	u32 vport_id;
	u16 vport_type;
	u8 pad[2];
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_vport_id);

struct cpchnl2_func_id {
	/* Function type: 0 - LAN PF, 1 -  LAN VF, Rest - "reserved" */
	u8 func_type;
	/* Always relevant, indexing is according to LAN PE 0-15, while only 0-4 APFs
	 * and 8-12 CPFs are valid
	 */
	u8 pf_id;
	/* Valid only if "type" above is VF, indexing is relative to PF specified above. */
	u16 vf_id;
	u8 pad[4];
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_func_id);

/* Note! Do not change the fields and especially their order as should eventually
 * be aligned to 32bit. Must match the virtchnl structure definition.
 * If should change, change also the relevant FAS and virtchnl code, under permission.
 */
struct cpchnl2_vport_info {
	u16 vport_index;
	/* VSI index, global indexing aligned to HW.
	 * Index of HW VSI is allocated by HMA during "CreateVport" virtChnl command.
	 * Relevant for VSI backed Vports only, not relevant for vport_type = "Qdev".
	 */
	u16 vsi_id;
	u8 vport_status;	/* enum cpchnl2_vport_status */
	/* 0 - LAN VF, 2 - LAN PF. Rest - reserved. Can be later expanded to other PEs */
	u8 func_type;
	/* Valid only if "type" above is VF, indexing is relative to PF specified above. */
	u16 vf_id;
	/* Always relevant, indexing is according to LAN PE 0-15,
	 * while only 0-4 APFs and 8-12 CPFs are valid.
	 */
	u8 pf_id;
	u8 rss_enabled; /* if RSS is enabled for Vport. Driven by Node Policy. Currently '0' */
	/* MAC Address assigned for this vport, all 0s for "Qdev" Vport type */
	u8 mac_addr[CPCHNL2_ETH_LENGTH_OF_ADDRESS];
	u16 vmrl_id;
	/* Indicates if IMC created SEM MAC rule for this Vport.
	 * Currently this is done by IMC for all Vport of type "Default" only,
	 * but can be different in the future.
	 */
	u8 sem_mac_rule_exist;
	/* Bitmask to inform which TC is valid.
	 * 0x1 << TCnum. 1b: valid else 0.
	 * Driven by Node Policy on system level, then Sysetm level TCs are
	 * reported to IDPF and it can enable Vport level TCs on TX according
	 * to Syetm enabled ones.
	 * If TC aware mode - bit set for valid TC.
	 * otherwise =1 (only bit 0 is set. represents the VSI
	 */
	u8 tx_tc_bitmask;
	/* For each valid TC, TEID of VPORT node over TC in TX LAN WS.
	 * If TC aware mode - up to 8 TC TEIDs. Otherwise vport_tc_teid[0] shall hold VSI TEID
	 */
	u32 vport_tc_teid[CPCHNL2_MAX_TC_AMOUNT];
	/* For each valid TC, bandwidth in mbps.
	 * Default BW per Vport is from Node policy
	 * If TC aware mode -per TC. Otherwise, bandwidth[0] holds VSI bandwidth
	 */
	u32 bandwidth[CPCHNL2_MAX_TC_AMOUNT];
	/* From Node Policy. */
	u16 max_mtu;
	u16 default_rx_qid;	/* Default LAN RX Queue ID */
	u16 vport_flags; /* see: VPORT_FLAGS */
	u8 egress_port;
	/* Host LAN APF: 0; ACC LAN APF: 4; IMC LAN APF: 5; ACC LAN CPF: 4; IMC LAN CPF: 5 */
	u8 host_id;
	u8 pad_reserved[4];
};
CPCHNL2_CHECK_STRUCT_LEN(96, cpchnl2_vport_info);

/*
 * CPCHNL2_OP_GET_VPORT_LIST
 */

/**
 * @brief Used for CPCHNL2_OP_GET_VPORT_LIST opcode request
 * @param func_type Func type: 0 - LAN_VF, 2 - LAN_PF. Rest - reserved (see enum cpchnl2_func_type)
 * @param pf_id Always relevant, indexing is according to LAN PE 0-15, while only 0-4 APFs and 8-12
 *        CPFs are valid
 * @param vf_id Valid only if "type" above is VF, indexing is relative to PF specified above
 */
struct cpchnl2_get_vport_list_request {
	u8 func_type;
	u8 pf_id;
	u16 vf_id;
	u8 pad[4];
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_get_vport_list_request);

/**
 * @brief Used for CPCHNL2_OP_GET_VPORT_LIST opcode response
 * @param func_type Func type: 0 - LAN_VF, 2 - LAN_PF. Rest - reserved. Can be later extended to
 *        other PE types
 * @param pf_id Always relevant, indexing is according to LAN PE 0-15, while only 0-4 APFs and 8-12
 *        CPFs are valid
 * @param vf_id Valid only if "type" above is VF, indexing is relative to PF specified above
 * @param nof_vports Number of vports created on the function
 * @param vports array of the IDs and types. vport ID is elative to its func (PF/VF). same as in
 *        Create Vport
 * vport_type: Aligned to VirtChnl types: Default, SIOV, etc.
 */
struct cpchnl2_get_vport_list_response {
	u8 func_type;
	u8 pf_id;
	u16 vf_id;
	u16 nof_vports;
	u8 pad[2];
	struct cpchnl2_vport_id vports[];
};
CPCHNL2_CHECK_STRUCT_LEN(8, cpchnl2_get_vport_list_response);

/*
 * CPCHNL2_OP_GET_VPORT_INFO
 */
/**
 * @brief Used for CPCHNL2_OP_GET_VPORT_INFO opcode request
 * @param vport a structure containing vport_id (relative to function) and type
 * @param func a structure containing function type, pf_id, vf_id
 */
struct cpchnl2_get_vport_info_request {
	struct cpchnl2_vport_id vport;
	struct cpchnl2_func_id func;
};
CPCHNL2_CHECK_STRUCT_LEN(16, cpchnl2_get_vport_info_request);

/**
 * @brief Used for CPCHNL2_OP_GET_VPORT_INFO opcode response
 * @param vport a structure containing vport_id (relative to function) and type to get info for
 * @param info a structure all the information for a given vport
 * @param queue_groups a structure containing all the queue groups of the given vport
 */
struct cpchnl2_get_vport_info_response {
	struct cpchnl2_vport_id vport;
	struct cpchnl2_vport_info info;
	struct cpchnl2_queue_groups queue_groups;
};
CPCHNL2_CHECK_STRUCT_LEN(3896, cpchnl2_get_vport_info_response);

 /* Cpchnl events
  * Sends event message to inform the peer of notification that may affect it.
  * No direct response is expected from the peer, though it may generate other
  * messages in response to this one.
  */
enum cpchnl2_event {
	CPCHNL2_EVENT_UNKNOWN = 0,
	CPCHNL2_EVENT_VPORT_CREATED,
	CPCHNL2_EVENT_VPORT_DESTROYED,
	CPCHNL2_EVENT_VPORT_ENABLED,
	CPCHNL2_EVENT_VPORT_DISABLED,
	CPCHNL2_PKG_EVENT,
	CPCHNL2_EVENT_ADD_QUEUE_GROUPS,
	CPCHNL2_EVENT_DEL_QUEUE_GROUPS,
	CPCHNL2_EVENT_ADD_QUEUES,
	CPCHNL2_EVENT_DEL_QUEUES
};

/*
 * This is for CPCHNL2_EVENT_VPORT_CREATED
 */
struct cpchnl2_event_vport_created {
	struct cpchnl2_vport_id vport; /* Vport identifier to point to specific Vport */
	struct cpchnl2_vport_info info; /* Vport configuration info */
	struct cpchnl2_queue_groups queue_groups; /* Vport assign queue groups configuration info */
};
CPCHNL2_CHECK_STRUCT_LEN(3896, cpchnl2_event_vport_created);

/*
 * This is for CPCHNL2_EVENT_VPORT_DESTROYED
 */
struct cpchnl2_event_vport_destroyed {
	/* Vport identifier to point to specific Vport */
	struct cpchnl2_vport_id vport;
	struct cpchnl2_func_id func;
};
CPCHNL2_CHECK_STRUCT_LEN(16, cpchnl2_event_vport_destroyed);

struct cpchnl2_event_info {
	struct {
		s32 type;		/* See enum cpchnl2_event */
		uint8_t reserved[4];	/* Reserved */
	} header;
	union {
		struct cpchnl2_event_vport_created vport_created;
		struct cpchnl2_event_vport_destroyed vport_destroyed;
	} data;
};

#endif /* _CPFL_CPCHNL_H_ */
