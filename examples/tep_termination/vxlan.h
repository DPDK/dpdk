/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#ifndef _VXLAN_H_
#define _VXLAN_H_

#include <rte_ether.h>
#include <rte_ip.h>

#define PORT_MIN	49152
#define PORT_MAX	65535
#define PORT_RANGE ((PORT_MAX - PORT_MIN) + 1)

#define VXLAN_N_PORTS  2
#define VXLAN_HF_VNI 0x08000000
#define DEFAULT_VXLAN_PORT 4789

extern struct ipv4_hdr app_ip_hdr[VXLAN_N_PORTS];
extern struct ether_hdr app_l2_hdr[VXLAN_N_PORTS];
extern uint8_t tx_checksum;
extern uint16_t tso_segsz;

struct vxlan_port {
	uint32_t vport_id;           /**< VirtIO port id */
	uint32_t peer_ip;            /**< remote VTEP IP address */
	struct ether_addr peer_mac;  /**< remote VTEP MAC address */
	struct ether_addr vport_mac; /**< VirtIO port MAC address */
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
