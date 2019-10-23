/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _VXLAN_H_
#define _VXLAN_H_

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_vxlan.h>

#define PORT_MIN	49152
#define PORT_MAX	65535
#define PORT_RANGE ((PORT_MAX - PORT_MIN) + 1)

#define VXLAN_N_PORTS  2
#define VXLAN_HF_VNI 0x08000000

extern struct rte_ipv4_hdr app_ip_hdr[VXLAN_N_PORTS];
extern struct rte_ether_hdr app_l2_hdr[VXLAN_N_PORTS];
extern uint8_t tx_checksum;
extern uint16_t tso_segsz;

struct vxlan_port {
	uint32_t vport_id;           /**< VirtIO port id */
	uint32_t peer_ip;            /**< remote VTEP IP address */
	struct rte_ether_addr peer_mac;  /**< remote VTEP MAC address */
	struct rte_ether_addr vport_mac; /**< VirtIO port MAC address */
} __rte_cache_aligned;

struct vxlan_conf {
	uint16_t dst_port;      /**< VXLAN UDP destination port */
	uint32_t port_ip;       /**< DPDK port IP address*/
	uint32_t in_key;        /**< VLAN  ID */
	uint32_t out_key;       /**< VXLAN VNI */
	struct vxlan_port port[VXLAN_N_PORTS]; /**< VXLAN configuration */
} __rte_cache_aligned;

extern struct vxlan_conf vxdev;

/* structure that caches offload info for the current packet */
union tunnel_offload_info {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t outer_l2_len:7; /**< outer L2 Header Length */
		uint64_t outer_l3_len:16; /**< outer L3 Header Length */
	};
} __rte_cache_aligned;

int decapsulation(struct rte_mbuf *pkt);
void encapsulation(struct rte_mbuf *m, uint8_t queue_id);

#endif /* _VXLAN_H_ */
