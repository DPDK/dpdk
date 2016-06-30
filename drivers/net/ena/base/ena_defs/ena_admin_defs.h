/*-
* BSD LICENSE
*
* Copyright (c) 2015-2016 Amazon.com, Inc. or its affiliates.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in
* the documentation and/or other materials provided with the
* distribution.
* * Neither the name of copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _ENA_ADMIN_H_
#define _ENA_ADMIN_H_

/* admin commands opcodes */
enum ena_admin_aq_opcode {
	/* create submission queue */
	ENA_ADMIN_CREATE_SQ = 1,

	/* destroy submission queue */
	ENA_ADMIN_DESTROY_SQ = 2,

	/* create completion queue */
	ENA_ADMIN_CREATE_CQ = 3,

	/* destroy completion queue */
	ENA_ADMIN_DESTROY_CQ = 4,

	/* get capabilities of particular feature */
	ENA_ADMIN_GET_FEATURE = 8,

	/* get capabilities of particular feature */
	ENA_ADMIN_SET_FEATURE = 9,

	/* get statistics */
	ENA_ADMIN_GET_STATS = 11,
};

/* admin command completion status codes */
enum ena_admin_aq_completion_status {
	/* Request completed successfully */
	ENA_ADMIN_SUCCESS = 0,

	/* no resources to satisfy request */
	ENA_ADMIN_RESOURCE_ALLOCATION_FAILURE = 1,

	/* Bad opcode in request descriptor */
	ENA_ADMIN_BAD_OPCODE = 2,

	/* Unsupported opcode in request descriptor */
	ENA_ADMIN_UNSUPPORTED_OPCODE = 3,

	/* Wrong request format */
	ENA_ADMIN_MALFORMED_REQUEST = 4,

	/* One of parameters is not valid. Provided in ACQ entry
	 * extended_status
	 */
	ENA_ADMIN_ILLEGAL_PARAMETER = 5,

	/* unexpected error */
	ENA_ADMIN_UNKNOWN_ERROR = 6,
};

/* get/set feature subcommands opcodes */
enum ena_admin_aq_feature_id {
	/* list of all supported attributes/capabilities in the ENA */
	ENA_ADMIN_DEVICE_ATTRIBUTES = 1,

	/* max number of supported queues per for every queues type */
	ENA_ADMIN_MAX_QUEUES_NUM = 2,

	/* Receive Side Scaling (RSS) function */
	ENA_ADMIN_RSS_HASH_FUNCTION = 10,

	/* stateless TCP/UDP/IP offload capabilities. */
	ENA_ADMIN_STATELESS_OFFLOAD_CONFIG = 11,

	/* Multiple tuples flow table configuration */
	ENA_ADMIN_RSS_REDIRECTION_TABLE_CONFIG = 12,

	/* max MTU, current MTU */
	ENA_ADMIN_MTU = 14,

	/* Receive Side Scaling (RSS) hash input */
	ENA_ADMIN_RSS_HASH_INPUT = 18,

	/* interrupt moderation parameters */
	ENA_ADMIN_INTERRUPT_MODERATION = 20,

	/* AENQ configuration */
	ENA_ADMIN_AENQ_CONFIG = 26,

	/* Link configuration */
	ENA_ADMIN_LINK_CONFIG = 27,

	/* Host attributes configuration */
	ENA_ADMIN_HOST_ATTR_CONFIG = 28,

	/* Number of valid opcodes */
	ENA_ADMIN_FEATURES_OPCODE_NUM = 32,
};

/* descriptors and headers placement */
enum ena_admin_placement_policy_type {
	/* descriptors and headers are in OS memory */
	ENA_ADMIN_PLACEMENT_POLICY_HOST = 1,

	/* descriptors and headers in device memory (a.k.a Low Latency
	 * Queue)
	 */
	ENA_ADMIN_PLACEMENT_POLICY_DEV = 3,
};

/* link speeds */
enum ena_admin_link_types {
	ENA_ADMIN_LINK_SPEED_1G = 0x1,

	ENA_ADMIN_LINK_SPEED_2_HALF_G = 0x2,

	ENA_ADMIN_LINK_SPEED_5G = 0x4,

	ENA_ADMIN_LINK_SPEED_10G = 0x8,

	ENA_ADMIN_LINK_SPEED_25G = 0x10,

	ENA_ADMIN_LINK_SPEED_40G = 0x20,

	ENA_ADMIN_LINK_SPEED_50G = 0x40,

	ENA_ADMIN_LINK_SPEED_100G = 0x80,

	ENA_ADMIN_LINK_SPEED_200G = 0x100,

	ENA_ADMIN_LINK_SPEED_400G = 0x200,
};

/* completion queue update policy */
enum ena_admin_completion_policy_type {
	/* cqe for each sq descriptor */
	ENA_ADMIN_COMPLETION_POLICY_DESC = 0,

	/* cqe upon request in sq descriptor */
	ENA_ADMIN_COMPLETION_POLICY_DESC_ON_DEMAND = 1,

	/* current queue head pointer is updated in OS memory upon sq
	 * descriptor request
	 */
	ENA_ADMIN_COMPLETION_POLICY_HEAD_ON_DEMAND = 2,

	/* current queue head pointer is updated in OS memory for each sq
	 * descriptor
	 */
	ENA_ADMIN_COMPLETION_POLICY_HEAD = 3,
};

/* type of get statistics command */
enum ena_admin_get_stats_type {
	/* Basic statistics */
	ENA_ADMIN_GET_STATS_TYPE_BASIC = 0,

	/* Extended statistics */
	ENA_ADMIN_GET_STATS_TYPE_EXTENDED = 1,
};

/* scope of get statistics command */
enum ena_admin_get_stats_scope {
	ENA_ADMIN_SPECIFIC_QUEUE = 0,

	ENA_ADMIN_ETH_TRAFFIC = 1,
};

/* ENA Admin Queue (AQ) common descriptor */
struct ena_admin_aq_common_desc {
	/* word 0 : */
	/* command identificator to associate it with the completion
	 * 11:0 : command_id
	 * 15:12 : reserved12
	 */
	uint16_t command_id;

	/* as appears in ena_aq_opcode */
	uint8_t opcode;

	/* 0 : phase
	 * 1 : ctrl_data - control buffer address valid
	 * 2 : ctrl_data_indirect - control buffer address
	 *    points to list of pages with addresses of control
	 *    buffers
	 * 7:3 : reserved3
	 */
	uint8_t flags;
};

/* used in ena_aq_entry. Can point directly to control data, or to a page
 * list chunk. Used also at the end of indirect mode page list chunks, for
 * chaining.
 */
struct ena_admin_ctrl_buff_info {
	/* word 0 : indicates length of the buffer pointed by
	 * control_buffer_address.
	 */
	uint32_t length;

	/* words 1:2 : points to control buffer (direct or indirect) */
	struct ena_common_mem_addr address;
};

/* submission queue full identification */
struct ena_admin_sq {
	/* word 0 : */
	/* queue id */
	uint16_t sq_idx;

	/* 4:0 : reserved
	 * 7:5 : sq_direction - 0x1 - Tx; 0x2 - Rx
	 */
	uint8_t sq_identity;

	uint8_t reserved1;
};

/* AQ entry format */
struct ena_admin_aq_entry {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* words 1:3 :  */
	union {
		/* command specific inline data */
		uint32_t inline_data_w1[3];

		/* words 1:3 : points to control buffer (direct or
		 * indirect, chained if needed)
		 */
		struct ena_admin_ctrl_buff_info control_buffer;
	} u;

	/* command specific inline data */
	uint32_t inline_data_w4[12];
};

/* ENA Admin Completion Queue (ACQ) common descriptor */
struct ena_admin_acq_common_desc {
	/* word 0 : */
	/* command identifier to associate it with the aq descriptor
	 * 11:0 : command_id
	 * 15:12 : reserved12
	 */
	uint16_t command;

	/* status of request execution */
	uint8_t status;

	/* 0 : phase
	 * 7:1 : reserved1
	 */
	uint8_t flags;

	/* word 1 : */
	/* provides additional info */
	uint16_t extended_status;

	/* submission queue head index, serves as a hint what AQ entries can
	 *    be revoked
	 */
	uint16_t sq_head_indx;
};

/* ACQ entry format */
struct ena_admin_acq_entry {
	/* words 0:1 :  */
	struct ena_admin_acq_common_desc acq_common_descriptor;

	/* response type specific data */
	uint32_t response_specific_data[14];
};

/* ENA AQ Create Submission Queue command. Placed in control buffer pointed
 * by AQ entry
 */
struct ena_admin_aq_create_sq_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* word 1 : */
	/* 4:0 : reserved0_w1
	 * 7:5 : sq_direction - 0x1 - Tx, 0x2 - Rx
	 */
	uint8_t sq_identity;

	uint8_t reserved8_w1;

	/* 3:0 : placement_policy - Describing where the SQ
	 *    descriptor ring and the SQ packet headers reside:
	 *    0x1 - descriptors and headers are in OS memory,
	 *    0x3 - descriptors and headers in device memory
	 *    (a.k.a Low Latency Queue)
	 * 6:4 : completion_policy - Describing what policy
	 *    to use for generation completion entry (cqe) in
	 *    the CQ associated with this SQ: 0x0 - cqe for each
	 *    sq descriptor, 0x1 - cqe upon request in sq
	 *    descriptor, 0x2 - current queue head pointer is
	 *    updated in OS memory upon sq descriptor request
	 *    0x3 - current queue head pointer is updated in OS
	 *    memory for each sq descriptor
	 * 7 : reserved15_w1
	 */
	uint8_t sq_caps_2;

	/* 0 : is_physically_contiguous - Described if the
	 *    queue ring memory is allocated in physical
	 *    contiguous pages or split.
	 * 7:1 : reserved17_w1
	 */
	uint8_t sq_caps_3;

	/* word 2 : */
	/* associated completion queue id. This CQ must be created prior to
	 *    SQ creation
	 */
	uint16_t cq_idx;

	/* submission queue depth in entries */
	uint16_t sq_depth;

	/* words 3:4 : SQ physical base address in OS memory. This field
	 * should not be used for Low Latency queues. Has to be page
	 * aligned.
	 */
	struct ena_common_mem_addr sq_ba;

	/* words 5:6 : specifies queue head writeback location in OS
	 * memory. Valid if completion_policy is set to
	 * completion_policy_head_on_demand or completion_policy_head. Has
	 * to be cache aligned
	 */
	struct ena_common_mem_addr sq_head_writeback;

	/* word 7 : reserved word */
	uint32_t reserved0_w7;

	/* word 8 : reserved word */
	uint32_t reserved0_w8;
};

/* submission queue direction */
enum ena_admin_sq_direction {
	ENA_ADMIN_SQ_DIRECTION_TX = 1,

	ENA_ADMIN_SQ_DIRECTION_RX = 2,
};

/* ENA Response for Create SQ Command. Appears in ACQ entry as
 * response_specific_data
 */
struct ena_admin_acq_create_sq_resp_desc {
	/* words 0:1 : Common Admin Queue completion descriptor */
	struct ena_admin_acq_common_desc acq_common_desc;

	/* word 2 : */
	/* sq identifier */
	uint16_t sq_idx;

	uint16_t reserved;

	/* word 3 : queue doorbell address as an offset to PCIe MMIO REG BAR */
	uint32_t sq_doorbell_offset;

	/* word 4 : low latency queue ring base address as an offset to
	 * PCIe MMIO LLQ_MEM BAR
	 */
	uint32_t llq_descriptors_offset;

	/* word 5 : low latency queue headers' memory as an offset to PCIe
	 * MMIO LLQ_MEM BAR
	 */
	uint32_t llq_headers_offset;
};

/* ENA AQ Destroy Submission Queue command. Placed in control buffer
 * pointed by AQ entry
 */
struct ena_admin_aq_destroy_sq_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* words 1 :  */
	struct ena_admin_sq sq;
};

/* ENA Response for Destroy SQ Command. Appears in ACQ entry as
 * response_specific_data
 */
struct ena_admin_acq_destroy_sq_resp_desc {
	/* words 0:1 : Common Admin Queue completion descriptor */
	struct ena_admin_acq_common_desc acq_common_desc;
};

/* ENA AQ Create Completion Queue command */
struct ena_admin_aq_create_cq_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* word 1 : */
	/* 4:0 : reserved5
	 * 5 : interrupt_mode_enabled - if set, cq operates
	 *    in interrupt mode, otherwise - polling
	 * 7:6 : reserved6
	 */
	uint8_t cq_caps_1;

	/* 4:0 : cq_entry_size_words - size of CQ entry in
	 *    32-bit words, valid values: 4, 8.
	 * 7:5 : reserved7
	 */
	uint8_t cq_caps_2;

	/* completion queue depth in # of entries. must be power of 2 */
	uint16_t cq_depth;

	/* word 2 : msix vector assigned to this cq */
	uint32_t msix_vector;

	/* words 3:4 : cq physical base address in OS memory. CQ must be
	 * physically contiguous
	 */
	struct ena_common_mem_addr cq_ba;
};

/* ENA Response for Create CQ Command. Appears in ACQ entry as response
 * specific data
 */
struct ena_admin_acq_create_cq_resp_desc {
	/* words 0:1 : Common Admin Queue completion descriptor */
	struct ena_admin_acq_common_desc acq_common_desc;

	/* word 2 : */
	/* cq identifier */
	uint16_t cq_idx;

	/* actual cq depth in # of entries */
	uint16_t cq_actual_depth;

	/* word 3 : cpu numa node address as an offset to PCIe MMIO REG BAR */
	uint32_t numa_node_register_offset;

	/* word 4 : completion head doorbell address as an offset to PCIe
	 * MMIO REG BAR
	 */
	uint32_t cq_head_db_register_offset;

	/* word 5 : interrupt unmask register address as an offset into
	 * PCIe MMIO REG BAR
	 */
	uint32_t cq_interrupt_unmask_register_offset;
};

/* ENA AQ Destroy Completion Queue command. Placed in control buffer
 * pointed by AQ entry
 */
struct ena_admin_aq_destroy_cq_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* word 1 : */
	/* associated queue id. */
	uint16_t cq_idx;

	uint16_t reserved1;
};

/* ENA Response for Destroy CQ Command. Appears in ACQ entry as
 * response_specific_data
 */
struct ena_admin_acq_destroy_cq_resp_desc {
	/* words 0:1 : Common Admin Queue completion descriptor */
	struct ena_admin_acq_common_desc acq_common_desc;
};

/* ENA AQ Get Statistics command. Extended statistics are placed in control
 * buffer pointed by AQ entry
 */
struct ena_admin_aq_get_stats_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* words 1:3 :  */
	union {
		/* command specific inline data */
		uint32_t inline_data_w1[3];

		/* words 1:3 : points to control buffer (direct or
		 * indirect, chained if needed)
		 */
		struct ena_admin_ctrl_buff_info control_buffer;
	} u;

	/* word 4 : */
	/* stats type as defined in enum ena_admin_get_stats_type */
	uint8_t type;

	/* stats scope defined in enum ena_admin_get_stats_scope */
	uint8_t scope;

	uint16_t reserved3;

	/* word 5 : */
	/* queue id. used when scope is specific_queue */
	uint16_t queue_idx;

	/* device id, value 0xFFFF means mine. only privileged device can get
	 *    stats of other device
	 */
	uint16_t device_id;
};

/* Basic Statistics Command. */
struct ena_admin_basic_stats {
	/* word 0 :  */
	uint32_t tx_bytes_low;

	/* word 1 :  */
	uint32_t tx_bytes_high;

	/* word 2 :  */
	uint32_t tx_pkts_low;

	/* word 3 :  */
	uint32_t tx_pkts_high;

	/* word 4 :  */
	uint32_t rx_bytes_low;

	/* word 5 :  */
	uint32_t rx_bytes_high;

	/* word 6 :  */
	uint32_t rx_pkts_low;

	/* word 7 :  */
	uint32_t rx_pkts_high;

	/* word 8 :  */
	uint32_t rx_drops_low;

	/* word 9 :  */
	uint32_t rx_drops_high;
};

/* ENA Response for Get Statistics Command. Appears in ACQ entry as
 * response_specific_data
 */
struct ena_admin_acq_get_stats_resp {
	/* words 0:1 : Common Admin Queue completion descriptor */
	struct ena_admin_acq_common_desc acq_common_desc;

	/* words 2:11 :  */
	struct ena_admin_basic_stats basic_stats;
};

/* ENA Get/Set Feature common descriptor. Appears as inline word in
 * ena_aq_entry
 */
struct ena_admin_get_set_feature_common_desc {
	/* word 0 : */
	/* 1:0 : select - 0x1 - current value; 0x3 - default
	 *    value
	 * 7:3 : reserved3
	 */
	uint8_t flags;

	/* as appears in ena_feature_id */
	uint8_t feature_id;

	/* reserved16 */
	uint16_t reserved16;
};

/* ENA Device Attributes Feature descriptor. */
struct ena_admin_device_attr_feature_desc {
	/* word 0 : implementation id */
	uint32_t impl_id;

	/* word 1 : device version */
	uint32_t device_version;

	/* word 2 : bit map of which bits are supported value of 1
	 * indicated that this feature is supported and can perform SET/GET
	 * for it
	 */
	uint32_t supported_features;

	/* word 3 :  */
	uint32_t reserved3;

	/* word 4 : Indicates how many bits are used physical address
	 * access.
	 */
	uint32_t phys_addr_width;

	/* word 5 : Indicates how many bits are used virtual address access. */
	uint32_t virt_addr_width;

	/* unicast MAC address (in Network byte order) */
	uint8_t mac_addr[6];

	uint8_t reserved7[2];

	/* word 8 : Max supported MTU value */
	uint32_t max_mtu;
};

/* ENA Max Queues Feature descriptor. */
struct ena_admin_queue_feature_desc {
	/* word 0 : Max number of submission queues (including LLQs) */
	uint32_t max_sq_num;

	/* word 1 : Max submission queue depth */
	uint32_t max_sq_depth;

	/* word 2 : Max number of completion queues */
	uint32_t max_cq_num;

	/* word 3 : Max completion queue depth */
	uint32_t max_cq_depth;

	/* word 4 : Max number of LLQ submission queues */
	uint32_t max_llq_num;

	/* word 5 : Max submission queue depth of LLQ */
	uint32_t max_llq_depth;

	/* word 6 : Max header size */
	uint32_t max_header_size;

	/* word 7 : */
	/* Maximum Descriptors number, including meta descriptors, allowed
	 *    for a single Tx packet
	 */
	uint16_t max_packet_tx_descs;

	/* Maximum Descriptors number allowed for a single Rx packet */
	uint16_t max_packet_rx_descs;
};

/* ENA MTU Set Feature descriptor. */
struct ena_admin_set_feature_mtu_desc {
	/* word 0 : mtu payload size (exclude L2) */
	uint32_t mtu;
};

/* ENA host attributes Set Feature descriptor. */
struct ena_admin_set_feature_host_attr_desc {
	/* words 0:1 : host OS info base address in OS memory. host info is
	 * 4KB of physically contiguous
	 */
	struct ena_common_mem_addr os_info_ba;

	/* words 2:3 : host debug area base address in OS memory. debug
	 * area must be physically contiguous
	 */
	struct ena_common_mem_addr debug_ba;

	/* word 4 : debug area size */
	uint32_t debug_area_size;
};

/* ENA Interrupt Moderation Get Feature descriptor. */
struct ena_admin_feature_intr_moder_desc {
	/* word 0 : */
	/* interrupt delay granularity in usec */
	uint16_t intr_delay_resolution;

	uint16_t reserved;
};

/* ENA Link Get Feature descriptor. */
struct ena_admin_get_feature_link_desc {
	/* word 0 : Link speed in Mb */
	uint32_t speed;

	/* word 1 : supported speeds (bit field of enum ena_admin_link
	 * types)
	 */
	uint32_t supported;

	/* word 2 : */
	/* 0 : autoneg - auto negotiation
	 * 1 : duplex - Full Duplex
	 * 31:2 : reserved2
	 */
	uint32_t flags;
};

/* ENA AENQ Feature descriptor. */
struct ena_admin_feature_aenq_desc {
	/* word 0 : bitmask for AENQ groups the device can report */
	uint32_t supported_groups;

	/* word 1 : bitmask for AENQ groups to report */
	uint32_t enabled_groups;
};

/* ENA Stateless Offload Feature descriptor. */
struct ena_admin_feature_offload_desc {
	/* word 0 : */
	/* Trasmit side stateless offload
	 * 0 : TX_L3_csum_ipv4 - IPv4 checksum
	 * 1 : TX_L4_ipv4_csum_part - TCP/UDP over IPv4
	 *    checksum, the checksum field should be initialized
	 *    with pseudo header checksum
	 * 2 : TX_L4_ipv4_csum_full - TCP/UDP over IPv4
	 *    checksum
	 * 3 : TX_L4_ipv6_csum_part - TCP/UDP over IPv6
	 *    checksum, the checksum field should be initialized
	 *    with pseudo header checksum
	 * 4 : TX_L4_ipv6_csum_full - TCP/UDP over IPv6
	 *    checksum
	 * 5 : tso_ipv4 - TCP/IPv4 Segmentation Offloading
	 * 6 : tso_ipv6 - TCP/IPv6 Segmentation Offloading
	 * 7 : tso_ecn - TCP Segmentation with ECN
	 */
	uint32_t tx;

	/* word 1 : */
	/* Receive side supported stateless offload
	 * 0 : RX_L3_csum_ipv4 - IPv4 checksum
	 * 1 : RX_L4_ipv4_csum - TCP/UDP/IPv4 checksum
	 * 2 : RX_L4_ipv6_csum - TCP/UDP/IPv6 checksum
	 * 3 : RX_hash - Hash calculation
	 */
	uint32_t rx_supported;

	/* word 2 : */
	/* Receive side enabled stateless offload */
	uint32_t rx_enabled;
};

/* hash functions */
enum ena_admin_hash_functions {
	/* Toeplitz hash */
	ENA_ADMIN_TOEPLITZ = 1,

	/* CRC32 hash */
	ENA_ADMIN_CRC32 = 2,
};

/* ENA RSS flow hash control buffer structure */
struct ena_admin_feature_rss_flow_hash_control {
	/* word 0 : number of valid keys */
	uint32_t keys_num;

	/* word 1 :  */
	uint32_t reserved;

	/* Toeplitz keys */
	uint32_t key[10];
};

/* ENA RSS Flow Hash Function */
struct ena_admin_feature_rss_flow_hash_function {
	/* word 0 : */
	/* supported hash functions
	 * 7:0 : funcs - supported hash functions (bitmask
	 *    accroding to ena_admin_hash_functions)
	 */
	uint32_t supported_func;

	/* word 1 : */
	/* selected hash func
	 * 7:0 : selected_func - selected hash function
	 *    (bitmask accroding to ena_admin_hash_functions)
	 */
	uint32_t selected_func;

	/* word 2 : initial value */
	uint32_t init_val;
};

/* RSS flow hash protocols */
enum ena_admin_flow_hash_proto {
	/* tcp/ipv4 */
	ENA_ADMIN_RSS_TCP4 = 0,

	/* udp/ipv4 */
	ENA_ADMIN_RSS_UDP4 = 1,

	/* tcp/ipv6 */
	ENA_ADMIN_RSS_TCP6 = 2,

	/* udp/ipv6 */
	ENA_ADMIN_RSS_UDP6 = 3,

	/* ipv4 not tcp/udp */
	ENA_ADMIN_RSS_IP4 = 4,

	/* ipv6 not tcp/udp */
	ENA_ADMIN_RSS_IP6 = 5,

	/* fragmented ipv4 */
	ENA_ADMIN_RSS_IP4_FRAG = 6,

	/* not ipv4/6 */
	ENA_ADMIN_RSS_NOT_IP = 7,

	/* max number of protocols */
	ENA_ADMIN_RSS_PROTO_NUM = 16,
};

/* RSS flow hash fields */
enum ena_admin_flow_hash_fields {
	/* Ethernet Dest Addr */
	ENA_ADMIN_RSS_L2_DA = 0,

	/* Ethernet Src Addr */
	ENA_ADMIN_RSS_L2_SA = 1,

	/* ipv4/6 Dest Addr */
	ENA_ADMIN_RSS_L3_DA = 2,

	/* ipv4/6 Src Addr */
	ENA_ADMIN_RSS_L3_SA = 5,

	/* tcp/udp Dest Port */
	ENA_ADMIN_RSS_L4_DP = 6,

	/* tcp/udp Src Port */
	ENA_ADMIN_RSS_L4_SP = 7,
};

/* hash input fields for flow protocol */
struct ena_admin_proto_input {
	/* word 0 : */
	/* flow hash fields (bitwise according to ena_admin_flow_hash_fields) */
	uint16_t fields;

	uint16_t reserved2;
};

/* ENA RSS hash control buffer structure */
struct ena_admin_feature_rss_hash_control {
	/* supported input fields */
	struct ena_admin_proto_input supported_fields[ENA_ADMIN_RSS_PROTO_NUM];

	/* selected input fields */
	struct ena_admin_proto_input selected_fields[ENA_ADMIN_RSS_PROTO_NUM];

	struct ena_admin_proto_input reserved2[ENA_ADMIN_RSS_PROTO_NUM];

	struct ena_admin_proto_input reserved3[ENA_ADMIN_RSS_PROTO_NUM];
};

/* ENA RSS flow hash input */
struct ena_admin_feature_rss_flow_hash_input {
	/* word 0 : */
	/* supported hash input sorting
	 * 1 : L3_sort - support swap L3 addresses if DA
	 *    smaller than SA
	 * 2 : L4_sort - support swap L4 ports if DP smaller
	 *    SP
	 */
	uint16_t supported_input_sort;

	/* enabled hash input sorting
	 * 1 : enable_L3_sort - enable swap L3 addresses if
	 *    DA smaller than SA
	 * 2 : enable_L4_sort - enable swap L4 ports if DP
	 *    smaller than SP
	 */
	uint16_t enabled_input_sort;
};

/* Operating system type */
enum ena_admin_os_type {
	/* Linux OS */
	ENA_ADMIN_OS_LINUX = 1,

	/* Windows OS */
	ENA_ADMIN_OS_WIN = 2,

	/* DPDK OS */
	ENA_ADMIN_OS_DPDK = 3,

	/* FreeBSD OS */
	ENA_ADMIN_OS_FREEBSD = 4,

	/* PXE OS */
	ENA_ADMIN_OS_IPXE = 5,
};

/* host info */
struct ena_admin_host_info {
	/* word 0 : OS type defined in enum ena_os_type */
	uint32_t os_type;

	/* os distribution string format */
	uint8_t os_dist_str[128];

	/* word 33 : OS distribution numeric format */
	uint32_t os_dist;

	/* kernel version string format */
	uint8_t kernel_ver_str[32];

	/* word 42 : Kernel version numeric format */
	uint32_t kernel_ver;

	/* word 43 : */
	/* driver version
	 * 7:0 : major - major
	 * 15:8 : minor - minor
	 * 23:16 : sub_minor - sub minor
	 */
	uint32_t driver_version;

	/* features bitmap */
	uint32_t supported_network_features[4];
};

/* ENA RSS indirection table entry */
struct ena_admin_rss_ind_table_entry {
	/* word 0 : */
	/* cq identifier */
	uint16_t cq_idx;

	uint16_t reserved;
};

/* ENA RSS indirection table */
struct ena_admin_feature_rss_ind_table {
	/* word 0 : */
	/* min supported table size (2^min_size) */
	uint16_t min_size;

	/* max supported table size (2^max_size) */
	uint16_t max_size;

	/* word 1 : */
	/* table size (2^size) */
	uint16_t size;

	uint16_t reserved;

	/* word 2 : index of the inline entry. 0xFFFFFFFF means invalid */
	uint32_t inline_index;

	/* words 3 : used for updating single entry, ignored when setting
	 * the entire table through the control buffer.
	 */
	struct ena_admin_rss_ind_table_entry inline_entry;
};

/* ENA Get Feature command */
struct ena_admin_get_feat_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* words 1:3 : points to control buffer (direct or indirect,
	 * chained if needed)
	 */
	struct ena_admin_ctrl_buff_info control_buffer;

	/* words 4 :  */
	struct ena_admin_get_set_feature_common_desc feat_common;

	/* words 5:15 :  */
	union {
		/* raw words */
		uint32_t raw[11];
	} u;
};

/* ENA Get Feature command response */
struct ena_admin_get_feat_resp {
	/* words 0:1 :  */
	struct ena_admin_acq_common_desc acq_common_desc;

	/* words 2:15 :  */
	union {
		/* raw words */
		uint32_t raw[14];

		/* words 2:10 : Get Device Attributes */
		struct ena_admin_device_attr_feature_desc dev_attr;

		/* words 2:5 : Max queues num */
		struct ena_admin_queue_feature_desc max_queue;

		/* words 2:3 : AENQ configuration */
		struct ena_admin_feature_aenq_desc aenq;

		/* words 2:4 : Get Link configuration */
		struct ena_admin_get_feature_link_desc link;

		/* words 2:4 : offload configuration */
		struct ena_admin_feature_offload_desc offload;

		/* words 2:4 : rss flow hash function */
		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;

		/* words 2 : rss flow hash input */
		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;

		/* words 2:3 : rss indirection table */
		struct ena_admin_feature_rss_ind_table ind_table;

		/* words 2 : interrupt moderation configuration */
		struct ena_admin_feature_intr_moder_desc intr_moderation;
	} u;
};

/* ENA Set Feature command */
struct ena_admin_set_feat_cmd {
	/* words 0 :  */
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* words 1:3 : points to control buffer (direct or indirect,
	 * chained if needed)
	 */
	struct ena_admin_ctrl_buff_info control_buffer;

	/* words 4 :  */
	struct ena_admin_get_set_feature_common_desc feat_common;

	/* words 5:15 :  */
	union {
		/* raw words */
		uint32_t raw[11];

		/* words 5 : mtu size */
		struct ena_admin_set_feature_mtu_desc mtu;

		/* words 5:7 : host attributes */
		struct ena_admin_set_feature_host_attr_desc host_attr;

		/* words 5:6 : AENQ configuration */
		struct ena_admin_feature_aenq_desc aenq;

		/* words 5:7 : rss flow hash function */
		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;

		/* words 5 : rss flow hash input */
		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;

		/* words 5:6 : rss indirection table */
		struct ena_admin_feature_rss_ind_table ind_table;
	} u;
};

/* ENA Set Feature command response */
struct ena_admin_set_feat_resp {
	/* words 0:1 :  */
	struct ena_admin_acq_common_desc acq_common_desc;

	/* words 2:15 :  */
	union {
		/* raw words */
		uint32_t raw[14];
	} u;
};

/* ENA Asynchronous Event Notification Queue descriptor.  */
struct ena_admin_aenq_common_desc {
	/* word 0 : */
	uint16_t group;

	uint16_t syndrom;

	/* word 1 : */
	/* 0 : phase */
	uint8_t flags;

	uint8_t reserved1[3];

	/* word 2 : Timestamp LSB */
	uint32_t timestamp_low;

	/* word 3 : Timestamp MSB */
	uint32_t timestamp_high;
};

/* asynchronous event notification groups */
enum ena_admin_aenq_group {
	/* Link State Change */
	ENA_ADMIN_LINK_CHANGE = 0,

	ENA_ADMIN_FATAL_ERROR = 1,

	ENA_ADMIN_WARNING = 2,

	ENA_ADMIN_NOTIFICATION = 3,

	ENA_ADMIN_KEEP_ALIVE = 4,

	ENA_ADMIN_AENQ_GROUPS_NUM = 5,
};

/* syndorm of AENQ notification group */
enum ena_admin_aenq_notification_syndrom {
	ENA_ADMIN_SUSPEND = 0,

	ENA_ADMIN_RESUME = 1,
};

/* ENA Asynchronous Event Notification generic descriptor.  */
struct ena_admin_aenq_entry {
	/* words 0:3 :  */
	struct ena_admin_aenq_common_desc aenq_common_desc;

	/* command specific inline data */
	uint32_t inline_data_w4[12];
};

/* ENA Asynchronous Event Notification Queue Link Change descriptor.  */
struct ena_admin_aenq_link_change_desc {
	/* words 0:3 :  */
	struct ena_admin_aenq_common_desc aenq_common_desc;

	/* word 4 : */
	/* 0 : link_status */
	uint32_t flags;
};

/* ENA MMIO Readless response interface */
struct ena_admin_ena_mmio_req_read_less_resp {
	/* word 0 : */
	/* request id */
	uint16_t req_id;

	/* register offset */
	uint16_t reg_off;

	/* word 1 : value is valid when poll is cleared */
	uint32_t reg_val;
};

/* aq_common_desc */
#define ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK GENMASK(11, 0)
#define ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK BIT(0)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT 1
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK BIT(1)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT 2
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK BIT(2)

/* sq */
#define ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT 5
#define ENA_ADMIN_SQ_SQ_DIRECTION_MASK GENMASK(7, 5)

/* acq_common_desc */
#define ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK GENMASK(11, 0)
#define ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK BIT(0)

/* aq_create_sq_cmd */
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT 5
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK GENMASK(7, 5)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK GENMASK(3, 0)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT 4
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK GENMASK(6, 4)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK BIT(0)

/* aq_create_cq_cmd */
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT 5
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK BIT(5)
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK GENMASK(4, 0)

/* get_set_feature_common_desc */
#define ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK GENMASK(1, 0)

/* get_feature_link_desc */
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK BIT(0)
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT 1
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK BIT(1)

/* feature_offload_desc */
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT 3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK BIT(3)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT 4
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK BIT(4)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT 5
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK BIT(5)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT 6
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK BIT(6)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT 7
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK BIT(7)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT 3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK BIT(3)

/* feature_rss_flow_hash_function */
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK GENMASK(7, 0)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK \
	GENMASK(7, 0)

/* feature_rss_flow_hash_input */
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK BIT(2)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK BIT(2)

/* host_info */
#define ENA_ADMIN_HOST_INFO_MAJOR_MASK GENMASK(7, 0)
#define ENA_ADMIN_HOST_INFO_MINOR_SHIFT 8
#define ENA_ADMIN_HOST_INFO_MINOR_MASK GENMASK(15, 8)
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT 16
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK GENMASK(23, 16)

/* aenq_common_desc */
#define ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK BIT(0)

/* aenq_link_change_desc */
#define ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK BIT(0)

#if !defined(ENA_DEFS_LINUX_MAINLINE)
static inline uint16_t
get_ena_admin_aq_common_desc_command_id(
		const struct ena_admin_aq_common_desc *p)
{
	return p->command_id & ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline void
set_ena_admin_aq_common_desc_command_id(struct ena_admin_aq_common_desc *p,
					uint16_t val)
{
	p->command_id |= val & ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline uint8_t
get_ena_admin_aq_common_desc_phase(const struct ena_admin_aq_common_desc *p)
{
	return p->flags & ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK;
}

static inline void
set_ena_admin_aq_common_desc_phase(struct ena_admin_aq_common_desc *p,
				   uint8_t val)
{
	p->flags |= val & ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK;
}

static inline uint8_t
get_ena_admin_aq_common_desc_ctrl_data(
		const struct ena_admin_aq_common_desc *p)
{
	return (p->flags & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK) >>
	       ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT;
}

static inline void
set_ena_admin_aq_common_desc_ctrl_data(struct ena_admin_aq_common_desc *p,
				       uint8_t val)
{
	p->flags |= (val << ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT)
		     & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK;
}

static inline uint8_t
get_ena_admin_aq_common_desc_ctrl_data_indirect(
		const struct ena_admin_aq_common_desc *p)
{
	return (p->flags & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK)
		>> ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT;
}

static inline void
set_ena_admin_aq_common_desc_ctrl_data_indirect(
		struct ena_admin_aq_common_desc *p,
		uint8_t val)
{
	p->flags |= (val << ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT)
		     & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK;
}

static inline uint8_t
get_ena_admin_sq_sq_direction(const struct ena_admin_sq *p)
{
	return (p->sq_identity & ENA_ADMIN_SQ_SQ_DIRECTION_MASK)
		>> ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT;
}

static inline void
set_ena_admin_sq_sq_direction(struct ena_admin_sq *p, uint8_t val)
{
	p->sq_identity |= (val << ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT) &
			  ENA_ADMIN_SQ_SQ_DIRECTION_MASK;
}

static inline uint16_t
get_ena_admin_acq_common_desc_command_id(
		const struct ena_admin_acq_common_desc *p)
{
	return p->command & ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline void
set_ena_admin_acq_common_desc_command_id(struct ena_admin_acq_common_desc *p,
					 uint16_t val)
{
	p->command |= val & ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline uint8_t
get_ena_admin_acq_common_desc_phase(const struct ena_admin_acq_common_desc *p)
{
	return p->flags & ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK;
}

static inline void
set_ena_admin_acq_common_desc_phase(struct ena_admin_acq_common_desc *p,
				    uint8_t val)
{
	p->flags |= val & ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_sq_cmd_sq_direction(
		const struct ena_admin_aq_create_sq_cmd *p)
{
	return (p->sq_identity & ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK)
		>> ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT;
}

static inline void
set_ena_admin_aq_create_sq_cmd_sq_direction(
		struct ena_admin_aq_create_sq_cmd *p,
		uint8_t val)
{
	p->sq_identity |= (val <<
			   ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT)
			  & ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_sq_cmd_placement_policy(
		const struct ena_admin_aq_create_sq_cmd *p)
{
	return p->sq_caps_2 & ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK;
}

static inline void
set_ena_admin_aq_create_sq_cmd_placement_policy(
		struct ena_admin_aq_create_sq_cmd *p,
		uint8_t val)
{
	p->sq_caps_2 |= val & ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_sq_cmd_completion_policy(
		const struct ena_admin_aq_create_sq_cmd *p)
{
	return (p->sq_caps_2
		& ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK)
		>> ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT;
}

static inline void
set_ena_admin_aq_create_sq_cmd_completion_policy(
		struct ena_admin_aq_create_sq_cmd *p,
		uint8_t val)
{
	p->sq_caps_2 |=
		(val << ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT)
		& ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_sq_cmd_is_physically_contiguous(
		const struct ena_admin_aq_create_sq_cmd *p)
{
	return p->sq_caps_3 &
		ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK;
}

static inline void
set_ena_admin_aq_create_sq_cmd_is_physically_contiguous(
		struct ena_admin_aq_create_sq_cmd *p,
		uint8_t val)
{
	p->sq_caps_3 |= val &
		ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_cq_cmd_interrupt_mode_enabled(
		const struct ena_admin_aq_create_cq_cmd *p)
{
	return (p->cq_caps_1 &
		ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK)
		>> ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT;
}

static inline void
set_ena_admin_aq_create_cq_cmd_interrupt_mode_enabled(
		struct ena_admin_aq_create_cq_cmd *p,
		uint8_t val)
{
	p->cq_caps_1 |=
	(val <<	ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT)
	& ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK;
}

static inline uint8_t
get_ena_admin_aq_create_cq_cmd_cq_entry_size_words(
		const struct ena_admin_aq_create_cq_cmd *p)
{
	return p->cq_caps_2
	       & ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK;
}

static inline void
set_ena_admin_aq_create_cq_cmd_cq_entry_size_words(
		struct ena_admin_aq_create_cq_cmd *p,
		uint8_t val)
{
	p->cq_caps_2 |=
		val & ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK;
}

static inline uint8_t
get_ena_admin_get_set_feature_common_desc_select(
		const struct ena_admin_get_set_feature_common_desc *p)
{
	return p->flags & ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK;
}

static inline void
set_ena_admin_get_set_feature_common_desc_select(
		struct ena_admin_get_set_feature_common_desc *p,
		uint8_t val)
{
	p->flags |= val & ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK;
}

static inline uint32_t
get_ena_admin_get_feature_link_desc_autoneg(
		const struct ena_admin_get_feature_link_desc *p)
{
	return p->flags & ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK;
}

static inline void
set_ena_admin_get_feature_link_desc_autoneg(
		struct ena_admin_get_feature_link_desc *p,
		uint32_t val)
{
	p->flags |= val & ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK;
}

static inline uint32_t
get_ena_admin_get_feature_link_desc_duplex(
		const struct ena_admin_get_feature_link_desc *p)
{
	return (p->flags & ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK)
		>> ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT;
}

static inline void
set_ena_admin_get_feature_link_desc_duplex(
		struct ena_admin_get_feature_link_desc *p,
		uint32_t val)
{
	p->flags |= (val << ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT)
			& ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_TX_L3_csum_ipv4(
		const struct ena_admin_feature_offload_desc *p)
{
	return p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK;
}

static inline void
set_ena_admin_feature_offload_desc_TX_L3_csum_ipv4(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= val & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_part(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_part(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val <<
		  ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_full(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_full(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val <<
		  ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_part(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_part(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val <<
		  ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_full(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_full(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val <<
		  ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_tso_ipv4(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_tso_ipv4(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_tso_ipv6(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_tso_ipv6(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_tso_ecn(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_tso_ecn(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT)
		 & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_RX_L3_csum_ipv4(
		const struct ena_admin_feature_offload_desc *p)
{
	return p->rx_supported &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK;
}

static inline void
set_ena_admin_feature_offload_desc_RX_L3_csum_ipv4(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->rx_supported |=
		val & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_RX_L4_ipv4_csum(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_RX_L4_ipv4_csum(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->rx_supported |=
		(val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT)
		& ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_RX_L4_ipv6_csum(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_RX_L4_ipv6_csum(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->rx_supported |=
		(val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT)
		& ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK;
}

static inline uint32_t
get_ena_admin_feature_offload_desc_RX_hash(
		const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported &
		ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK)
		>> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT;
}

static inline void
set_ena_admin_feature_offload_desc_RX_hash(
		struct ena_admin_feature_offload_desc *p,
		uint32_t val)
{
	p->rx_supported |=
		(val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT)
		& ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK;
}

static inline uint32_t
get_ena_admin_feature_rss_flow_hash_function_funcs(
		const struct ena_admin_feature_rss_flow_hash_function *p)
{
	return p->supported_func &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK;
}

static inline void
set_ena_admin_feature_rss_flow_hash_function_funcs(
		struct ena_admin_feature_rss_flow_hash_function *p,
		uint32_t val)
{
	p->supported_func |=
		val & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK;
}

static inline uint32_t
get_ena_admin_feature_rss_flow_hash_function_selected_func(
		const struct ena_admin_feature_rss_flow_hash_function *p)
{
	return p->selected_func &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK;
}

static inline void
set_ena_admin_feature_rss_flow_hash_function_selected_func(
		struct ena_admin_feature_rss_flow_hash_function *p,
		uint32_t val)
{
	p->selected_func |=
		val &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK;
}

static inline uint16_t
get_ena_admin_feature_rss_flow_hash_input_L3_sort(
		const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->supported_input_sort &
			ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK)
		>> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT;
}

static inline void
set_ena_admin_feature_rss_flow_hash_input_L3_sort(
		struct ena_admin_feature_rss_flow_hash_input *p, uint16_t val)
{
	p->supported_input_sort |=
		(val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT)
		& ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK;
}

static inline uint16_t
get_ena_admin_feature_rss_flow_hash_input_L4_sort(
		const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->supported_input_sort &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK)
		>> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT;
}

static inline void
set_ena_admin_feature_rss_flow_hash_input_L4_sort(
		struct ena_admin_feature_rss_flow_hash_input *p,
		uint16_t val)
{
	p->supported_input_sort |=
		(val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT)
		 & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK;
}

static inline uint16_t
get_ena_admin_feature_rss_flow_hash_input_enable_L3_sort(
		const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->enabled_input_sort &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK)
		>> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT;
}

static inline void
set_ena_admin_feature_rss_flow_hash_input_enable_L3_sort(
		struct ena_admin_feature_rss_flow_hash_input *p,
		uint16_t val)
{
	p->enabled_input_sort |=
		(val <<
		 ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT)
		& ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK;
}

static inline uint16_t
get_ena_admin_feature_rss_flow_hash_input_enable_L4_sort(
		const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->enabled_input_sort &
		ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK)
		>> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT;
}

static inline void
set_ena_admin_feature_rss_flow_hash_input_enable_L4_sort(
		struct ena_admin_feature_rss_flow_hash_input *p,
		uint16_t val)
{
	p->enabled_input_sort |=
		(val <<
		 ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT)
		& ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK;
}

static inline uint32_t
get_ena_admin_host_info_major(const struct ena_admin_host_info *p)
{
	return p->driver_version & ENA_ADMIN_HOST_INFO_MAJOR_MASK;
}

static inline void
set_ena_admin_host_info_major(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= val & ENA_ADMIN_HOST_INFO_MAJOR_MASK;
}

static inline uint32_t
get_ena_admin_host_info_minor(const struct ena_admin_host_info *p)
{
	return (p->driver_version & ENA_ADMIN_HOST_INFO_MINOR_MASK)
		>> ENA_ADMIN_HOST_INFO_MINOR_SHIFT;
}

static inline void
set_ena_admin_host_info_minor(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= (val << ENA_ADMIN_HOST_INFO_MINOR_SHIFT)
		& ENA_ADMIN_HOST_INFO_MINOR_MASK;
}

static inline uint32_t
get_ena_admin_host_info_sub_minor(const struct ena_admin_host_info *p)
{
	return (p->driver_version & ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK)
		>> ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT;
}

static inline void
set_ena_admin_host_info_sub_minor(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= (val << ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT)
			     & ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK;
}

static inline uint8_t
get_ena_admin_aenq_common_desc_phase(
		const struct ena_admin_aenq_common_desc *p)
{
	return p->flags & ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK;
}

static inline void
set_ena_admin_aenq_common_desc_phase(
		struct ena_admin_aenq_common_desc *p,
		uint8_t val)
{
	p->flags |= val & ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK;
}

static inline uint32_t
get_ena_admin_aenq_link_change_desc_link_status(
		const struct ena_admin_aenq_link_change_desc *p)
{
	return p->flags & ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK;
}

static inline void
set_ena_admin_aenq_link_change_desc_link_status(
		struct ena_admin_aenq_link_change_desc *p,
		uint32_t val)
{
	p->flags |= val & ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK;
}

#endif /* !defined(ENA_DEFS_LINUX_MAINLINE) */
#endif /*_ENA_ADMIN_H_ */
