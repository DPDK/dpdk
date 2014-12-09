/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_ETH_CTRL_H_
#define _RTE_ETH_CTRL_H_

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

/**
 * Feature filter types
 */
enum rte_filter_type {
	RTE_ETH_FILTER_NONE = 0,
	RTE_ETH_FILTER_MACVLAN,
	RTE_ETH_FILTER_ETHERTYPE,
	RTE_ETH_FILTER_TUNNEL,
	RTE_ETH_FILTER_FDIR,
	RTE_ETH_FILTER_MAX
};

/**
 * Generic operations on filters
 */
enum rte_filter_op {
	RTE_ETH_FILTER_NOP = 0,
	/**< used to check whether the type filter is supported */
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
	RTE_MACVLAN_PERFECT_MATCH,
	/**< exact match of MAC addr and VLAN ID. */
	RTE_MAC_HASH_MATCH, /**< hash match of MAC addr. */
	RTE_MACVLAN_HASH_MATCH,
	/**< hash match of MAC addr and exact match of VLAN ID. */
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

/**
 * Tunneled type.
 */
enum rte_eth_tunnel_type {
	RTE_TUNNEL_TYPE_NONE = 0,
	RTE_TUNNEL_TYPE_VXLAN,
	RTE_TUNNEL_TYPE_GENEVE,
	RTE_TUNNEL_TYPE_TEREDO,
	RTE_TUNNEL_TYPE_NVGRE,
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
	struct ether_addr *outer_mac;  /**< Outer MAC address filter. */
	struct ether_addr *inner_mac;  /**< Inner MAC address filter. */
	uint16_t inner_vlan;           /**< Inner VLAN filter. */
	enum rte_tunnel_iptype ip_type; /**< IP address type. */
	union {
		uint32_t ipv4_addr;    /**< IPv4 source address to match. */
		uint32_t ipv6_addr[4]; /**< IPv6 source address to match. */
	} ip_addr; /**< IPv4/IPv6 source address to match (union of above). */

	uint16_t filter_type;   /**< Filter type. */
	enum rte_eth_tunnel_type tunnel_type; /**< Tunnel Type. */
	uint32_t tenant_id;     /** < Tenant number. */
	uint16_t queue_id;      /** < queue number. */
};

#define RTE_ETH_FDIR_MAX_FLEXLEN         16 /** < Max length of flexbytes. */

/**
 * Flow type
 */
enum rte_eth_flow_type {
	RTE_ETH_FLOW_TYPE_NONE = 0,
	RTE_ETH_FLOW_TYPE_UDPV4,
	RTE_ETH_FLOW_TYPE_TCPV4,
	RTE_ETH_FLOW_TYPE_SCTPV4,
	RTE_ETH_FLOW_TYPE_IPV4_OTHER,
	RTE_ETH_FLOW_TYPE_FRAG_IPV4,
	RTE_ETH_FLOW_TYPE_UDPV6,
	RTE_ETH_FLOW_TYPE_TCPV6,
	RTE_ETH_FLOW_TYPE_SCTPV6,
	RTE_ETH_FLOW_TYPE_IPV6_OTHER,
	RTE_ETH_FLOW_TYPE_FRAG_IPV6,
	RTE_ETH_FLOW_TYPE_MAX = 64,
};

/**
 * A structure used to define the input for IPV4 flow
 */
struct rte_eth_ipv4_flow {
	uint32_t src_ip;      /**< IPv4 source address to match. */
	uint32_t dst_ip;      /**< IPv4 destination address to match. */
};

/**
 * A structure used to define the input for IPV4 UDP flow
 */
struct rte_eth_udpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint16_t src_port;           /**< UDP source port to match. */
	uint16_t dst_port;           /**< UDP destination port to match. */
};

/**
 * A structure used to define the input for IPV4 TCP flow
 */
struct rte_eth_tcpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint16_t src_port;           /**< TCP source port to match. */
	uint16_t dst_port;           /**< TCP destination port to match. */
};

/**
 * A structure used to define the input for IPV4 SCTP flow
 */
struct rte_eth_sctpv4_flow {
	struct rte_eth_ipv4_flow ip; /**< IPv4 fields to match. */
	uint32_t verify_tag;         /**< Verify tag to match */
};

/**
 * A structure used to define the input for IPV6 flow
 */
struct rte_eth_ipv6_flow {
	uint32_t src_ip[4];      /**< IPv6 source address to match. */
	uint32_t dst_ip[4];      /**< IPv6 destination address to match. */
};

/**
 * A structure used to define the input for IPV6 UDP flow
 */
struct rte_eth_udpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint16_t src_port;           /**< UDP source port to match. */
	uint16_t dst_port;           /**< UDP destination port to match. */
};

/**
 * A structure used to define the input for IPV6 TCP flow
 */
struct rte_eth_tcpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint16_t src_port;           /**< TCP source port to match. */
	uint16_t dst_port;           /**< TCP destination port to match. */
};

/**
 * A structure used to define the input for IPV6 SCTP flow
 */
struct rte_eth_sctpv6_flow {
	struct rte_eth_ipv6_flow ip; /**< IPv6 fields to match. */
	uint32_t verify_tag;         /**< Verify tag to match */
};

/**
 * An union contains the inputs for all types of flow
 */
union rte_eth_fdir_flow {
	struct rte_eth_udpv4_flow  udp4_flow;
	struct rte_eth_tcpv4_flow  tcp4_flow;
	struct rte_eth_sctpv4_flow sctp4_flow;
	struct rte_eth_ipv4_flow   ip4_flow;
	struct rte_eth_udpv6_flow  udp6_flow;
	struct rte_eth_tcpv6_flow  tcp6_flow;
	struct rte_eth_sctpv6_flow sctp6_flow;
	struct rte_eth_ipv6_flow   ipv6_flow;
};

/**
 * A structure used to contain extend input of flow
 */
struct rte_eth_fdir_flow_ext {
	uint16_t vlan_tci;
	uint8_t flexbytes[RTE_ETH_FDIR_MAX_FLEXLEN];
	/**< It is filled by the flexible payload to match. */
};

/**
 * A structure used to define the input for a flow director filter entry
 */
struct rte_eth_fdir_input {
	enum rte_eth_flow_type flow_type;      /**< Type of flow */
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
 * Payload type
 */
enum rte_eth_payload_type {
	RTE_ETH_PAYLOAD_UNKNOWN = 0,
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
	enum rte_eth_flow_type flow_type;  /**< Flow type */
	uint8_t mask[RTE_ETH_FDIR_MAX_FLEXLEN];
	/**< Mask for the whole flexible payload */
};

/**
 * A structure used to define all flexible payload related setting
 * include flexpay load and flex mask
 */
struct rte_eth_fdir_flex_conf {
	uint16_t nb_payloads;  /**< The number of following payload cfg */
	uint16_t nb_flexmasks; /**< The number of following mask */
	struct rte_eth_flex_payload_cfg flex_set[RTE_ETH_PAYLOAD_MAX];
	/**< Flex payload configuration for each payload type */
	struct rte_eth_fdir_flex_mask flex_mask[RTE_ETH_FLOW_TYPE_MAX];
	/**< Flex mask configuration for each flow type */
};

/**
 *  Flow Director setting modes: none, signature or perfect.
 */
enum rte_fdir_mode {
	RTE_FDIR_MODE_NONE      = 0, /**< Disable FDIR support. */
	RTE_FDIR_MODE_SIGNATURE,     /**< Enable FDIR signature filter mode. */
	RTE_FDIR_MODE_PERFECT,       /**< Enable FDIR perfect filter mode. */
};

/**
 * A structure used to get the information of flow director filter.
 * It supports RTE_ETH_FILTER_FDIR with RTE_ETH_FILTER_INFO operation.
 * It includes the mode, flexible payload configuration information,
 * capabilities and supported flow types, flexible payload characters.
 * It can be gotten to help taking specific configurations per device.
 */
struct rte_eth_fdir_info {
	enum rte_fdir_mode mode;     /**< Flow director mode */
	struct rte_eth_fdir_flex_conf flex_conf;
	/**< Flex payload configuration information */
	uint32_t guarant_spc;          /**< Guaranteed spaces.*/
	uint32_t best_spc;             /**< Best effort spaces.*/
	uint32_t flow_types_mask[RTE_ETH_FLOW_TYPE_MAX / sizeof(uint32_t)];
	/**< Bit mask for every supported flow type. */
	uint32_t max_flexpayload;      /**< Total flex payload in bytes. */
	uint32_t flex_payload_unit;
	/**< Flexible payload unit in bytes. Size and alignments of all flex
	     payload segments should be multiplies of this value. */
	uint32_t max_flex_payload_segment_num;
	/**< Max number of flexible payload continuous segments.
	     Each segment should be a multiple of flex_payload_unit.*/
	uint16_t flex_payload_limit;
	/**< Maximum src_offset in bytes allowed. It indicates that
	     src_offset[i] in struct rte_eth_flex_payload_cfg should be
	     less than this value. */
	uint32_t flex_bitmask_unit;
	/**< Flex bitmask unit in bytes. Size of flex bitmasks should
	     be a multiply of this value. */
	uint32_t max_flex_bitmask_num;
	/**< Max supported size of flex bitmasks in flex_bitmask_unit */
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETH_CTRL_H_ */
