/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_ETH_CTRL_H_
#define _RTE_ETH_CTRL_H_

#include <stdint.h>
#include <rte_common.h>
#include "rte_ether.h"

/**
 * @file
 *
 * Ethernet device features and related data structures used
 * by control APIs should be defined in this file.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A packet can be identified by hardware as different flow types. Different
 * NIC hardwares may support different flow types.
 * Basically, the NIC hardware identifies the flow type as deep protocol as
 * possible, and exclusively. For example, if a packet is identified as
 * 'RTE_ETH_FLOW_NONFRAG_IPV4_TCP', it will not be any of other flow types,
 * though it is an actual IPV4 packet.
 * Note that the flow types are used to define RSS offload types in
 * rte_ethdev.h.
 */
#define RTE_ETH_FLOW_UNKNOWN             0
#define RTE_ETH_FLOW_RAW                 1
#define RTE_ETH_FLOW_IPV4                2
#define RTE_ETH_FLOW_FRAG_IPV4           3
#define RTE_ETH_FLOW_NONFRAG_IPV4_TCP    4
#define RTE_ETH_FLOW_NONFRAG_IPV4_UDP    5
#define RTE_ETH_FLOW_NONFRAG_IPV4_SCTP   6
#define RTE_ETH_FLOW_NONFRAG_IPV4_OTHER  7
#define RTE_ETH_FLOW_IPV6                8
#define RTE_ETH_FLOW_FRAG_IPV6           9
#define RTE_ETH_FLOW_NONFRAG_IPV6_TCP   10
#define RTE_ETH_FLOW_NONFRAG_IPV6_UDP   11
#define RTE_ETH_FLOW_NONFRAG_IPV6_SCTP  12
#define RTE_ETH_FLOW_NONFRAG_IPV6_OTHER 13
#define RTE_ETH_FLOW_L2_PAYLOAD         14
#define RTE_ETH_FLOW_IPV6_EX            15
#define RTE_ETH_FLOW_IPV6_TCP_EX        16
#define RTE_ETH_FLOW_IPV6_UDP_EX        17
#define RTE_ETH_FLOW_PORT               18
	/**< Consider device port number as a flow differentiator */
#define RTE_ETH_FLOW_VXLAN              19 /**< VXLAN protocol based flow */
#define RTE_ETH_FLOW_GENEVE             20 /**< GENEVE protocol based flow */
#define RTE_ETH_FLOW_NVGRE              21 /**< NVGRE protocol based flow */
#define RTE_ETH_FLOW_VXLAN_GPE          22 /**< VXLAN-GPE protocol based flow */
#define RTE_ETH_FLOW_MAX                23

/**
 * Feature filter types
 */
enum rte_filter_type {
	RTE_ETH_FILTER_NONE = 0,
	RTE_ETH_FILTER_MACVLAN,
	RTE_ETH_FILTER_ETHERTYPE,
	RTE_ETH_FILTER_FLEXIBLE,
	RTE_ETH_FILTER_SYN,
	RTE_ETH_FILTER_NTUPLE,
	RTE_ETH_FILTER_TUNNEL,
	RTE_ETH_FILTER_FDIR,
	RTE_ETH_FILTER_HASH,
	RTE_ETH_FILTER_L2_TUNNEL,
	RTE_ETH_FILTER_GENERIC,
	RTE_ETH_FILTER_MAX
};

/**
 * Generic operations on filters
 */
enum rte_filter_op {
	/** used to check whether the type filter is supported */
	RTE_ETH_FILTER_NOP = 0,
	RTE_ETH_FILTER_ADD,      /**< add filter entry */
	RTE_ETH_FILTER_UPDATE,   /**< update filter entry */
	RTE_ETH_FILTER_DELETE,   /**< delete filter entry */
	RTE_ETH_FILTER_FLUSH,    /**< flush all entries */
	RTE_ETH_FILTER_GET,      /**< get filter entry */
	RTE_ETH_FILTER_SET,      /**< configurations */
	RTE_ETH_FILTER_INFO,     /**< retrieve information */
	RTE_ETH_FILTER_STATS,    /**< retrieve statistics */
	RTE_ETH_FILTER_OP_MAX
};

/**
 * MAC filter type
 */
enum rte_mac_filter_type {
	RTE_MAC_PERFECT_MATCH = 1, /**< exact match of MAC addr. */
	RTE_MACVLAN_PERFECT_MATCH, /**< exact match of MAC addr and VLAN ID. */
	RTE_MAC_HASH_MATCH, /**< hash match of MAC addr. */
	/** hash match of MAC addr and exact match of VLAN ID. */
	RTE_MACVLAN_HASH_MATCH,
};

/**
 * MAC filter info
 */
struct rte_eth_mac_filter {
	uint8_t is_vf; /**< 1 for VF, 0 for port dev */
	uint16_t dst_id; /**< VF ID, available when is_vf is 1*/
	enum rte_mac_filter_type filter_type; /**< MAC filter type */
	struct ether_addr mac_addr;
};

/**
 * Define all structures for Ethertype Filter type.
 */

#define RTE_ETHTYPE_FLAGS_MAC    0x0001 /**< If set, compare mac */
#define RTE_ETHTYPE_FLAGS_DROP   0x0002 /**< If set, drop packet when match */

/**
 * A structure used to define the ethertype filter entry
 * to support RTE_ETH_FILTER_ETHERTYPE with RTE_ETH_FILTER_ADD,
 * RTE_ETH_FILTER_DELETE and RTE_ETH_FILTER_GET operations.
 */
struct rte_eth_ethertype_filter {
	struct ether_addr mac_addr;   /**< Mac address to match. */
	uint16_t ether_type;          /**< Ether type to match */
	uint16_t flags;               /**< Flags from RTE_ETHTYPE_FLAGS_* */
	uint16_t queue;               /**< Queue assigned to when match*/
};

#define RTE_FLEX_FILTER_MAXLEN	128	/**< bytes to use in flex filter. */
#define RTE_FLEX_FILTER_MASK_SIZE	\
	(RTE_ALIGN(RTE_FLEX_FILTER_MAXLEN, CHAR_BIT) / CHAR_BIT)
					/**< mask bytes in flex filter. */

/**
 *  A structure used to define the flex filter entry
 *  to support RTE_ETH_FILTER_FLEXIBLE with RTE_ETH_FILTER_ADD,
 *  RTE_ETH_FILTER_DELETE and RTE_ETH_FILTER_GET operations.
 */
struct rte_eth_flex_filter {
	uint16_t len;
	uint8_t bytes[RTE_FLEX_FILTER_MAXLEN];  /**< flex bytes in big endian.*/
	uint8_t mask[RTE_FLEX_FILTER_MASK_SIZE];    /**< if mask bit is 1b, do
					not compare corresponding byte. */
	uint8_t priority;
	uint16_t queue;       /**< Queue assigned to when match. */
};

/**
 * A structure used to define the TCP syn filter entry
 * to support RTE_ETH_FILTER_SYN with RTE_ETH_FILTER_ADD,
 * RTE_ETH_FILTER_DELETE and RTE_ETH_FILTER_GET operations.
 */
struct rte_eth_syn_filter {
	uint8_t hig_pri;     /**< 1 - higher priority than other filters,
				  0 - lower priority. */
	uint16_t queue;      /**< Queue assigned to when match */
};

/**
 * Define all structures for ntuple Filter type.
 */

#define RTE_NTUPLE_FLAGS_DST_IP    0x0001 /**< If set, dst_ip is part of ntuple */
#define RTE_NTUPLE_FLAGS_SRC_IP    0x0002 /**< If set, src_ip is part of ntuple */
#define RTE_NTUPLE_FLAGS_DST_PORT  0x0004 /**< If set, dst_port is part of ntuple */
#define RTE_NTUPLE_FLAGS_SRC_PORT  0x0008 /**< If set, src_port is part of ntuple */
#define RTE_NTUPLE_FLAGS_PROTO     0x0010 /**< If set, protocol is part of ntuple */
#define RTE_NTUPLE_FLAGS_TCP_FLAG  0x0020 /**< If set, tcp flag is involved */

#define RTE_5TUPLE_FLAGS ( \
		RTE_NTUPLE_FLAGS_DST_IP | \
		RTE_NTUPLE_FLAGS_SRC_IP | \
		RTE_NTUPLE_FLAGS_DST_PORT | \
		RTE_NTUPLE_FLAGS_SRC_PORT | \
		RTE_NTUPLE_FLAGS_PROTO)

#define RTE_2TUPLE_FLAGS ( \
		RTE_NTUPLE_FLAGS_DST_PORT | \
		RTE_NTUPLE_FLAGS_PROTO)

#define TCP_URG_FLAG 0x20
#define TCP_ACK_FLAG 0x10
#define TCP_PSH_FLAG 0x08
#define TCP_RST_FLAG 0x04
#define TCP_SYN_FLAG 0x02
#define TCP_FIN_FLAG 0x01
#define TCP_FLAG_ALL 0x3F

/**
 * A structure used to define the ntuple filter entry
 * to support RTE_ETH_FILTER_NTUPLE with RTE_ETH_FILTER_ADD,
 * RTE_ETH_FILTER_DELETE and RTE_ETH_FILTER_GET operations.
 */
struct rte_eth_ntuple_filter {
	uint16_t flags;          /**< Flags from RTE_NTUPLE_FLAGS_* */
	uint32_t dst_ip;         /**< Destination IP address in big endian. */
	uint32_t dst_ip_mask;    /**< Mask of destination IP address. */
	uint32_t src_ip;         /**< Source IP address in big endian. */
	uint32_t src_ip_mask;    /**< Mask of destination IP address. */
	uint16_t dst_port;       /**< Destination port in big endian. */
	uint16_t dst_port_mask;  /**< Mask of destination port. */
	uint16_t src_port;       /**< Source Port in big endian. */
	uint16_t src_port_mask;  /**< Mask of source port. */
	uint8_t proto;           /**< L4 protocol. */
	uint8_t proto_mask;      /**< Mask of L4 protocol. */
	/** tcp_flags only meaningful when the proto is TCP.
	    The packet matched above ntuple fields and contain
	    any set bit in tcp_flags will hit this filter. */
	uint8_t tcp_flags;
	uint16_t priority;       /**< seven levels (001b-111b), 111b is highest,
				      used when more than one filter matches. */
	uint16_t queue;          /**< Queue assigned to when match*/
};

/**
 * Tunneled type.
 */
enum rte_eth_tunnel_type {
	RTE_TUNNEL_TYPE_NONE = 0,
	RTE_TUNNEL_TYPE_VXLAN,
	RTE_TUNNEL_TYPE_GENEVE,
	RTE_TUNNEL_TYPE_TEREDO,
	RTE_TUNNEL_TYPE_NVGRE,
	RTE_TUNNEL_TYPE_IP_IN_GRE,
	RTE_L2_TUNNEL_TYPE_E_TAG,
	RTE_TUNNEL_TYPE_MAX,
};

/**
 * filter type of tunneling packet
 */
#define ETH_TUNNEL_FILTER_OMAC  0x01 /**< filter by outer MAC addr */
#define ETH_TUNNEL_FILTER_OIP   0x02 /**< filter by outer IP Addr */
#define ETH_TUNNEL_FILTER_TENID 0x04 /**< filter by tenant ID */
#define ETH_TUNNEL_FILTER_IMAC  0x08 /**< filter by inner MAC addr */
#define ETH_TUNNEL_FILTER_IVLAN 0x10 /**< filter by inner VLAN ID */
#define ETH_TUNNEL_FILTER_IIP   0x20 /**< filter by inner IP addr */

#define RTE_TUNNEL_FILTER_IMAC_IVLAN (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_IVLAN)
#define RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_IVLAN | \
					ETH_TUNNEL_FILTER_TENID)
#define RTE_TUNNEL_FILTER_IMAC_TENID (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_TENID)
#define RTE_TUNNEL_FILTER_OMAC_TENID_IMAC (ETH_TUNNEL_FILTER_OMAC | \
					ETH_TUNNEL_FILTER_TENID | \
					ETH_TUNNEL_FILTER_IMAC)

/**
 *  Select IPv4 or IPv6 for tunnel filters.
 */
enum rte_tunnel_iptype {
	RTE_TUNNEL_IPTYPE_IPV4 = 0, /**< IPv4. */
	RTE_TUNNEL_IPTYPE_IPV6,     /**< IPv6. */
};

/**
 * Tunneling Packet filter configuration.
 */
struct rte_eth_tunnel_filter_conf {
	struct ether_addr outer_mac;    /**< Outer MAC address to match. */
	struct ether_addr inner_mac;    /**< Inner MAC address to match. */
	uint16_t inner_vlan;            /**< Inner VLAN to match. */
	enum rte_tunnel_iptype ip_type; /**< IP address type. */
	/** Outer destination IP address to match if ETH_TUNNEL_FILTER_OIP
	    is set in filter_type, or inner destination IP address to match
	    if ETH_TUNNEL_FILTER_IIP is set in filter_type . */
	union {
		uint32_t ipv4_addr;     /**< IPv4 address in big endian. */
		uint32_t ipv6_addr[4];  /**< IPv6 address in big endian. */
	} ip_addr;
	/** Flags from ETH_TUNNEL_FILTER_XX - see above. */
	uint16_t filter_type;
	enum rte_eth_tunnel_type tunnel_type; /**< Tunnel Type. */
	uint32_t tenant_id;     /**< Tenant ID to match. VNI, GRE key... */
	uint16_t queue_id;      /**< Queue assigned to if match. */
};

/**
 * Global eth device configuration type.
 */
enum rte_eth_global_cfg_type {
	RTE_ETH_GLOBAL_CFG_TYPE_UNKNOWN = 0,
	RTE_ETH_GLOBAL_CFG_TYPE_GRE_KEY_LEN,
	RTE_ETH_GLOBAL_CFG_TYPE_MAX,
};

/**
 * Global eth device configuration.
 */
struct rte_eth_global_cfg {
	enum rte_eth_global_cfg_type cfg_type; /**< Global config type. */
	union {
		uint8_t gre_key_len; /**< Valid GRE key length in byte. */
		uint64_t reserved; /**< Reserve space for future use. */
	} cfg;
};

#define RTE_ETH_FDIR_MAX_FLEXLEN 16  /**< Max length of flexbytes. */
#define RTE_ETH_INSET_SIZE_MAX   128 /**< Max length of input set. */

/**
 * Input set fields for Flow Director and Hash filters
 */
enum rte_eth_input_set_field {
	RTE_ETH_INPUT_SET_UNKNOWN = 0,

	/* L2 */
	RTE_ETH_INPUT_SET_L2_SRC_MAC = 1,
	RTE_ETH_INPUT_SET_L2_DST_MAC,
	RTE_ETH_INPUT_SET_L2_OUTER_VLAN,
	RTE_ETH_INPUT_SET_L2_INNER_VLAN,
	RTE_ETH_INPUT_SET_L2_ETHERTYPE,

	/* L3 */
	RTE_ETH_INPUT_SET_L3_SRC_IP4 = 129,
	RTE_ETH_INPUT_SET_L3_DST_IP4,
	RTE_ETH_INPUT_SET_L3_SRC_IP6,
	RTE_ETH_INPUT_SET_L3_DST_IP6,
	RTE_ETH_INPUT_SET_L3_IP4_TOS,
	RTE_ETH_INPUT_SET_L3_IP4_PROTO,
	RTE_ETH_INPUT_SET_L3_IP6_TC,
	RTE_ETH_INPUT_SET_L3_IP6_NEXT_HEADER,
	RTE_ETH_INPUT_SET_L3_IP4_TTL,
	RTE_ETH_INPUT_SET_L3_IP6_HOP_LIMITS,

	/* L4 */
	RTE_ETH_INPUT_SET_L4_UDP_SRC_PORT = 257,
	RTE_ETH_INPUT_SET_L4_UDP_DST_PORT,
	RTE_ETH_INPUT_SET_L4_TCP_SRC_PORT,
	RTE_ETH_INPUT_SET_L4_TCP_DST_PORT,
	RTE_ETH_INPUT_SET_L4_SCTP_SRC_PORT,
	RTE_ETH_INPUT_SET_L4_SCTP_DST_PORT,
	RTE_ETH_INPUT_SET_L4_SCTP_VERIFICATION_TAG,

	/* Tunnel */
	RTE_ETH_INPUT_SET_TUNNEL_L2_INNER_DST_MAC = 385,
	RTE_ETH_INPUT_SET_TUNNEL_L2_INNER_SRC_MAC,
	RTE_ETH_INPUT_SET_TUNNEL_L2_INNER_VLAN,
	RTE_ETH_INPUT_SET_TUNNEL_L4_UDP_KEY,
	RTE_ETH_INPUT_SET_TUNNEL_GRE_KEY,

	/* Flexible Payload */
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_1ST_WORD = 641,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_2ND_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_3RD_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_4TH_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_5TH_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_6TH_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_7TH_WORD,
	RTE_ETH_INPUT_SET_FLEX_PAYLOAD_8TH_WORD,

	RTE_ETH_INPUT_SET_DEFAULT = 65533,
	RTE_ETH_INPUT_SET_NONE = 65534,
	RTE_ETH_INPUT_SET_MAX = 65535,
};

/**
 * Filters input set operations
 */
enum rte_filter_input_set_op {
	RTE_ETH_INPUT_SET_OP_UNKNOWN,
	RTE_ETH_INPUT_SET_SELECT, /**< select input set */
	RTE_ETH_INPUT_SET_ADD,    /**< add input set entry */
	RTE_ETH_INPUT_SET_OP_MAX
};


/**
 * A structure used to define the input set configuration for
 * flow director and hash filters
 */
struct rte_eth_input_set_conf {
	uint16_t flow_type;
	uint16_t inset_size;
	enum rte_eth_input_set_field field[RTE_ETH_INSET_SIZE_MAX];
	enum rte_filter_input_set_op op;
};

/**
 * A structure used to define the input for L2 flow
 */
struct rte_eth_l2_flow {
	uint16_t ether_type;          /**< Ether type in big endian */
};

/**
 * A structure used to define the input for IPV4 flow
 */
struct rte_eth_ipv4_flow {
	uint32_t src_ip;      /**< IPv4 source address in big endian. */
	uint32_t dst_ip;      /**< IPv4 destination address in big endian. */
	uint8_t  tos;         /**< Type of service to match. */
	uint8_t  ttl;         /**< Time to live to match. */
	uint8_t  proto;       /**< Protocol, next header in big endian. */
};

/**
 * A structure used to define the input for IPV4 UDP flow
 */
struct rte_eth_udpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint16_t src_port;           /**< UDP source port in big endian. */
	uint16_t dst_port;           /**< UDP destination port in big endian. */
};

/**
 * A structure used to define the input for IPV4 TCP flow
 */
struct rte_eth_tcpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint16_t src_port;           /**< TCP source port in big endian. */
	uint16_t dst_port;           /**< TCP destination port in big endian. */
};

/**
 * A structure used to define the input for IPV4 SCTP flow
 */
struct rte_eth_sctpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint16_t src_port;           /**< SCTP source port in big endian. */
	uint16_t dst_port;           /**< SCTP destination port in big endian. */
	uint32_t verify_tag;         /**< Verify tag in big endian */
};

/**
 * A structure used to define the input for IPV6 flow
 */
struct rte_eth_ipv6_flow {
	uint32_t src_ip[4];      /**< IPv6 source address in big endian. */
	uint32_t dst_ip[4];      /**< IPv6 destination address in big endian. */
	uint8_t  tc;             /**< Traffic class to match. */
	uint8_t  proto;          /**< Protocol, next header to match. */
	uint8_t  hop_limits;     /**< Hop limits to match. */
};

/**
 * A structure used to define the input for IPV6 UDP flow
 */
struct rte_eth_udpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint16_t src_port;           /**< UDP source port in big endian. */
	uint16_t dst_port;           /**< UDP destination port in big endian. */
};

/**
 * A structure used to define the input for IPV6 TCP flow
 */
struct rte_eth_tcpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint16_t src_port;           /**< TCP source port to in big endian. */
	uint16_t dst_port;           /**< TCP destination port in big endian. */
};

/**
 * A structure used to define the input for IPV6 SCTP flow
 */
struct rte_eth_sctpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint16_t src_port;           /**< SCTP source port in big endian. */
	uint16_t dst_port;           /**< SCTP destination port in big endian. */
	uint32_t verify_tag;         /**< Verify tag in big endian. */
};

/**
 * A structure used to define the input for MAC VLAN flow
 */
struct rte_eth_mac_vlan_flow {
	struct ether_addr mac_addr;  /**< Mac address to match. */
};

/**
 * Tunnel type for flow director.
 */
enum rte_eth_fdir_tunnel_type {
	RTE_FDIR_TUNNEL_TYPE_UNKNOWN = 0,
	RTE_FDIR_TUNNEL_TYPE_NVGRE,
	RTE_FDIR_TUNNEL_TYPE_VXLAN,
};

/**
 * A structure used to define the input for tunnel flow, now it's VxLAN or
 * NVGRE
 */
struct rte_eth_tunnel_flow {
	enum rte_eth_fdir_tunnel_type tunnel_type; /**< Tunnel type to match. */
	/** Tunnel ID to match. TNI, VNI... in big endian. */
	uint32_t tunnel_id;
	struct ether_addr mac_addr;                /**< Mac address to match. */
};

/**
 * An union contains the inputs for all types of flow
 * Items in flows need to be in big endian
 */
union rte_eth_fdir_flow {
	struct rte_eth_l2_flow     l2_flow;
	struct rte_eth_udpv4_flow  udp4_flow;
	struct rte_eth_tcpv4_flow  tcp4_flow;
	struct rte_eth_sctpv4_flow sctp4_flow;
	struct rte_eth_ipv4_flow   ip4_flow;
	struct rte_eth_udpv6_flow  udp6_flow;
	struct rte_eth_tcpv6_flow  tcp6_flow;
	struct rte_eth_sctpv6_flow sctp6_flow;
	struct rte_eth_ipv6_flow   ipv6_flow;
	struct rte_eth_mac_vlan_flow mac_vlan_flow;
	struct rte_eth_tunnel_flow   tunnel_flow;
};

/**
 * A structure used to contain extend input of flow
 */
struct rte_eth_fdir_flow_ext {
	uint16_t vlan_tci;
	uint8_t flexbytes[RTE_ETH_FDIR_MAX_FLEXLEN];
	/**< It is filled by the flexible payload to match. */
	uint8_t is_vf;   /**< 1 for VF, 0 for port dev */
	uint16_t dst_id; /**< VF ID, available when is_vf is 1*/
};

/**
 * A structure used to define the input for a flow director filter entry
 */
struct rte_eth_fdir_input {
	uint16_t flow_type;
	union rte_eth_fdir_flow flow;
	/**< Flow fields to match, dependent on flow_type */
	struct rte_eth_fdir_flow_ext flow_ext;
	/**< Additional fields to match */
};

/**
 * Behavior will be taken if FDIR match
 */
enum rte_eth_fdir_behavior {
	RTE_ETH_FDIR_ACCEPT = 0,
	RTE_ETH_FDIR_REJECT,
	RTE_ETH_FDIR_PASSTHRU,
};

/**
 * Flow director report status
 * It defines what will be reported if FDIR entry is matched.
 */
enum rte_eth_fdir_status {
	RTE_ETH_FDIR_NO_REPORT_STATUS = 0, /**< Report nothing. */
	RTE_ETH_FDIR_REPORT_ID,            /**< Only report FD ID. */
	RTE_ETH_FDIR_REPORT_ID_FLEX_4,     /**< Report FD ID and 4 flex bytes. */
	RTE_ETH_FDIR_REPORT_FLEX_8,        /**< Report 8 flex bytes. */
};

/**
 * A structure used to define an action when match FDIR packet filter.
 */
struct rte_eth_fdir_action {
	uint16_t rx_queue;        /**< Queue assigned to if FDIR match. */
	enum rte_eth_fdir_behavior behavior;     /**< Behavior will be taken */
	enum rte_eth_fdir_status report_status;  /**< Status report option */
	uint8_t flex_off;
	/**< If report_status is RTE_ETH_FDIR_REPORT_ID_FLEX_4 or
	     RTE_ETH_FDIR_REPORT_FLEX_8, flex_off specifies where the reported
	     flex bytes start from in flexible payload. */
};

/**
 * A structure used to define the flow director filter entry by filter_ctrl API
 * It supports RTE_ETH_FILTER_FDIR with RTE_ETH_FILTER_ADD and
 * RTE_ETH_FILTER_DELETE operations.
 */
struct rte_eth_fdir_filter {
	uint32_t soft_id;
	/**< ID, an unique value is required when deal with FDIR entry */
	struct rte_eth_fdir_input input;    /**< Input set */
	struct rte_eth_fdir_action action;  /**< Action taken when match */
};

/**
 *  A structure used to configure FDIR masks that are used by the device
 *  to match the various fields of RX packet headers.
 */
struct rte_eth_fdir_masks {
	uint16_t vlan_tci_mask;   /**< Bit mask for vlan_tci in big endian */
	/** Bit mask for ipv4 flow in big endian. */
	struct rte_eth_ipv4_flow   ipv4_mask;
	/** Bit maks for ipv6 flow in big endian. */
	struct rte_eth_ipv6_flow   ipv6_mask;
	/** Bit mask for L4 source port in big endian. */
	uint16_t src_port_mask;
	/** Bit mask for L4 destination port in big endian. */
	uint16_t dst_port_mask;
	/** 6 bit mask for proper 6 bytes of Mac address, bit 0 matches the
	    first byte on the wire */
	uint8_t mac_addr_byte_mask;
	/** Bit mask for tunnel ID in big endian. */
	uint32_t tunnel_id_mask;
	uint8_t tunnel_type_mask; /**< 1 - Match tunnel type,
				       0 - Ignore tunnel type. */
};

/**
 * Payload type
 */
enum rte_eth_payload_type {
	RTE_ETH_PAYLOAD_UNKNOWN = 0,
	RTE_ETH_RAW_PAYLOAD,
	RTE_ETH_L2_PAYLOAD,
	RTE_ETH_L3_PAYLOAD,
	RTE_ETH_L4_PAYLOAD,
	RTE_ETH_PAYLOAD_MAX = 8,
};

/**
 * A structure used to select bytes extracted from the protocol layers to
 * flexible payload for filter
 */
struct rte_eth_flex_payload_cfg {
	enum rte_eth_payload_type type;  /**< Payload type */
	uint16_t src_offset[RTE_ETH_FDIR_MAX_FLEXLEN];
	/**< Offset in bytes from the beginning of packet's payload
	     src_offset[i] indicates the flexbyte i's offset in original
	     packet payload. This value should be less than
	     flex_payload_limit in struct rte_eth_fdir_info.*/
};

/**
 * A structure used to define FDIR masks for flexible payload
 * for each flow type
 */
struct rte_eth_fdir_flex_mask {
	uint16_t flow_type;
	uint8_t mask[RTE_ETH_FDIR_MAX_FLEXLEN];
	/**< Mask for the whole flexible payload */
};

/**
 * A structure used to define all flexible payload related setting
 * include flex payload and flex mask
 */
struct rte_eth_fdir_flex_conf {
	uint16_t nb_payloads;  /**< The number of following payload cfg */
	uint16_t nb_flexmasks; /**< The number of following mask */
	struct rte_eth_flex_payload_cfg flex_set[RTE_ETH_PAYLOAD_MAX];
	/**< Flex payload configuration for each payload type */
	struct rte_eth_fdir_flex_mask flex_mask[RTE_ETH_FLOW_MAX];
	/**< Flex mask configuration for each flow type */
};

/**
 *  Flow Director setting modes: none, signature or perfect.
 */
enum rte_fdir_mode {
	RTE_FDIR_MODE_NONE      = 0, /**< Disable FDIR support. */
	RTE_FDIR_MODE_SIGNATURE,     /**< Enable FDIR signature filter mode. */
	RTE_FDIR_MODE_PERFECT,       /**< Enable FDIR perfect filter mode. */
	RTE_FDIR_MODE_PERFECT_MAC_VLAN, /**< Enable FDIR filter mode - MAC VLAN. */
	RTE_FDIR_MODE_PERFECT_TUNNEL,   /**< Enable FDIR filter mode - tunnel. */
};

#define UINT64_BIT (CHAR_BIT * sizeof(uint64_t))
#define RTE_FLOW_MASK_ARRAY_SIZE \
	(RTE_ALIGN(RTE_ETH_FLOW_MAX, UINT64_BIT)/UINT64_BIT)

/**
 * A structure used to get the information of flow director filter.
 * It supports RTE_ETH_FILTER_FDIR with RTE_ETH_FILTER_INFO operation.
 * It includes the mode, flexible payload configuration information,
 * capabilities and supported flow types, flexible payload characters.
 * It can be gotten to help taking specific configurations per device.
 */
struct rte_eth_fdir_info {
	enum rte_fdir_mode mode; /**< Flow director mode */
	struct rte_eth_fdir_masks mask;
	/** Flex payload configuration information */
	struct rte_eth_fdir_flex_conf flex_conf;
	uint32_t guarant_spc; /**< Guaranteed spaces.*/
	uint32_t best_spc; /**< Best effort spaces.*/
	/** Bit mask for every supported flow type. */
	uint64_t flow_types_mask[RTE_FLOW_MASK_ARRAY_SIZE];
	uint32_t max_flexpayload; /**< Total flex payload in bytes. */
	/** Flexible payload unit in bytes. Size and alignments of all flex
	    payload segments should be multiplies of this value. */
	uint32_t flex_payload_unit;
	/** Max number of flexible payload continuous segments.
	    Each segment should be a multiple of flex_payload_unit.*/
	uint32_t max_flex_payload_segment_num;
	/** Maximum src_offset in bytes allowed. It indicates that
	    src_offset[i] in struct rte_eth_flex_payload_cfg should be less
	    than this value. */
	uint16_t flex_payload_limit;
	/** Flex bitmask unit in bytes. Size of flex bitmasks should be a
	    multiply of this value. */
	uint32_t flex_bitmask_unit;
	/** Max supported size of flex bitmasks in flex_bitmask_unit */
	uint32_t max_flex_bitmask_num;
};

/**
 * A structure used to define the statistics of flow director.
 * It supports RTE_ETH_FILTER_FDIR with RTE_ETH_FILTER_STATS operation.
 */
struct rte_eth_fdir_stats {
	uint32_t collision;    /**< Number of filters with collision. */
	uint32_t free;         /**< Number of free filters. */
	uint32_t maxhash;
	/**< The lookup hash value of the added filter that updated the value
	   of the MAXLEN field */
	uint32_t maxlen;       /**< Longest linked list of filters. */
	uint64_t add;          /**< Number of added filters. */
	uint64_t remove;       /**< Number of removed filters. */
	uint64_t f_add;        /**< Number of failed added filters. */
	uint64_t f_remove;     /**< Number of failed removed filters. */
	uint32_t guarant_cnt;  /**< Number of filters in guaranteed spaces. */
	uint32_t best_cnt;     /**< Number of filters in best effort spaces. */
};

/**
 * Flow Director filter information types.
 */
enum rte_eth_fdir_filter_info_type {
	RTE_ETH_FDIR_FILTER_INFO_TYPE_UNKNOWN = 0,
	/** Flow Director filter input set configuration */
	RTE_ETH_FDIR_FILTER_INPUT_SET_SELECT,
	RTE_ETH_FDIR_FILTER_INFO_TYPE_MAX,
};

/**
 * A structure used to set FDIR filter information, to support filter type
 * of 'RTE_ETH_FILTER_FDIR' RTE_ETH_FDIR_FILTER_INPUT_SET_SELECT operation.
 */
struct rte_eth_fdir_filter_info {
	enum rte_eth_fdir_filter_info_type info_type; /**< Information type */
	/** Details of fdir filter information */
	union {
		/** Flow Director input set configuration per port */
		struct rte_eth_input_set_conf input_set_conf;
	} info;
};

/**
 * Hash filter information types.
 * - RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT is for getting/setting the
 *   information/configuration of 'symmetric hash enable' per port.
 * - RTE_ETH_HASH_FILTER_GLOBAL_CONFIG is for getting/setting the global
 *   configurations of hash filters. Those global configurations are valid
 *   for all ports of the same NIC.
 * - RTE_ETH_HASH_FILTER_INPUT_SET_SELECT is for setting the global
 *   hash input set fields
 */
enum rte_eth_hash_filter_info_type {
	RTE_ETH_HASH_FILTER_INFO_TYPE_UNKNOWN = 0,
	/** Symmetric hash enable per port */
	RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT,
	/** Configure globally for hash filter */
	RTE_ETH_HASH_FILTER_GLOBAL_CONFIG,
	/** Global Hash filter input set configuration */
	RTE_ETH_HASH_FILTER_INPUT_SET_SELECT,
	RTE_ETH_HASH_FILTER_INFO_TYPE_MAX,
};

/**
 * Hash function types.
 */
enum rte_eth_hash_function {
	RTE_ETH_HASH_FUNCTION_DEFAULT = 0,
	RTE_ETH_HASH_FUNCTION_TOEPLITZ, /**< Toeplitz */
	RTE_ETH_HASH_FUNCTION_SIMPLE_XOR, /**< Simple XOR */
	RTE_ETH_HASH_FUNCTION_MAX,
};

#define RTE_SYM_HASH_MASK_ARRAY_SIZE \
	(RTE_ALIGN(RTE_ETH_FLOW_MAX, UINT64_BIT)/UINT64_BIT)
/**
 * A structure used to set or get global hash function configurations which
 * include symmetric hash enable per flow type and hash function type.
 * Each bit in sym_hash_enable_mask[] indicates if the symmetric hash of the
 * corresponding flow type is enabled or not.
 * Each bit in valid_bit_mask[] indicates if the corresponding bit in
 * sym_hash_enable_mask[] is valid or not. For the configurations gotten, it
 * also means if the flow type is supported by hardware or not.
 */
struct rte_eth_hash_global_conf {
	enum rte_eth_hash_function hash_func; /**< Hash function type */
	/** Bit mask for symmetric hash enable per flow type */
	uint64_t sym_hash_enable_mask[RTE_SYM_HASH_MASK_ARRAY_SIZE];
	/** Bit mask indicates if the corresponding bit is valid */
	uint64_t valid_bit_mask[RTE_SYM_HASH_MASK_ARRAY_SIZE];
};

/**
 * A structure used to set or get hash filter information, to support filter
 * type of 'RTE_ETH_FILTER_HASH' and its operations.
 */
struct rte_eth_hash_filter_info {
	enum rte_eth_hash_filter_info_type info_type; /**< Information type */
	/** Details of hash filter information */
	union {
		/** For RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT */
		uint8_t enable;
		/** Global configurations of hash filter */
		struct rte_eth_hash_global_conf global_conf;
		/** Global configurations of hash filter input set */
		struct rte_eth_input_set_conf input_set_conf;
	} info;
};

/**
 * l2 tunnel configuration.
 */
struct rte_eth_l2_tunnel_conf {
	enum rte_eth_tunnel_type l2_tunnel_type;
	uint16_t ether_type; /* ether type in l2 header */
	uint32_t tunnel_id; /* port tag id for e-tag */
	uint16_t vf_id; /* VF id for tag insertion */
	uint32_t pool; /* destination pool for tag based forwarding */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETH_CTRL_H_ */
