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
	RTE_ETH_FILTER_TUNNEL,
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
	RTE_ETH_FILTER_INFO,
	/**< get information of filter, such as status or statistics */
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
	uint16_t dst_id; /**<VF ID, available when is_vf is 1*/
	uint16_t filter_type; /**< MAC filter type */
	struct ether_addr mac_addr;
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETH_CTRL_H_ */
