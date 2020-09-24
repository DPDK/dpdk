/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Inspur Corporation
 */

#ifndef _GRO_VXLAN_UDP4_H_
#define _GRO_VXLAN_UDP4_H_

#include "gro_udp4.h"

#define GRO_VXLAN_UDP4_TBL_MAX_ITEM_NUM (1024UL * 1024UL)

/* Header fields representing a VxLAN flow */
struct vxlan_udp4_flow_key {
	struct udp4_flow_key inner_key;
	struct rte_vxlan_hdr vxlan_hdr;

	struct rte_ether_addr outer_eth_saddr;
	struct rte_ether_addr outer_eth_daddr;

	uint32_t outer_ip_src_addr;
	uint32_t outer_ip_dst_addr;

	/* Note: It is unnecessary to save outer_src_port here because it can
	 * be different for VxLAN UDP fragments from the same flow.
	 */
	uint16_t outer_dst_port;
};

struct gro_vxlan_udp4_flow {
	struct vxlan_udp4_flow_key key;
	/*
	 * The index of the first packet in the flow. INVALID_ARRAY_INDEX
	 * indicates an empty flow.
	 */
	uint32_t start_index;
};

struct gro_vxlan_udp4_item {
	struct gro_udp4_item inner_item;
	/* Note: VXLAN UDP/IPv4 GRO needn't check outer_ip_id because
	 * the difference between outer_ip_ids of two received packets
	 * isn't always +/-1 in case of OVS DPDK. So no outer_ip_id
	 * and outer_is_atomic fields here.
	 */
};

/*
 * VxLAN (with an outer IPv4 header and an inner UDP/IPv4 packet)
 * reassembly table structure
 */
struct gro_vxlan_udp4_tbl {
	/* item array */
	struct gro_vxlan_udp4_item *items;
	/* flow array */
	struct gro_vxlan_udp4_flow *flows;
	/* current item number */
	uint32_t item_num;
	/* current flow number */
	uint32_t flow_num;
	/* the maximum item number */
	uint32_t max_item_num;
	/* the maximum flow number */
	uint32_t max_flow_num;
};

/**
 * This function creates a VxLAN reassembly table for VxLAN packets
 * which have an outer IPv4 header and an inner UDP/IPv4 packet.
 *
 * @param socket_id
 *  Socket index for allocating the table
 * @param max_flow_num
 *  The maximum number of flows in the table
 * @param max_item_per_flow
 *  The maximum number of packets per flow
 *
 * @return
 *  - Return the table pointer on success.
 *  - Return NULL on failure.
 */
void *gro_vxlan_udp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);

/**
 * This function destroys a VxLAN reassembly table.
 *
 * @param tbl
 *  Pointer pointing to the VxLAN reassembly table
 */
void gro_vxlan_udp4_tbl_destroy(void *tbl);

/**
 * This function merges a VxLAN packet which has an outer IPv4 header and
 * an inner UDP/IPv4 packet. It does not process the packet which does not
 * have payload.
 *
 * This function does not check if the packet has correct checksums and
 * does not re-calculate checksums for the merged packet. It returns the
 * packet if there is no available space in the table.
 *
 * @param pkt
 *  Packet to reassemble
 * @param tbl
 *  Pointer pointing to the VxLAN reassembly table
 * @start_time
 *  The time when the packet is inserted into the table
 *
 * @return
 *  - Return a positive value if the packet is merged.
 *  - Return zero if the packet isn't merged but stored in the table.
 *  - Return a negative value for invalid parameters or no available
 *    space in the table.
 */
int32_t gro_vxlan_udp4_reassemble(struct rte_mbuf *pkt,
		struct gro_vxlan_udp4_tbl *tbl,
		uint64_t start_time);

/**
 * This function flushes timeout packets in the VxLAN reassembly table,
 * and without updating checksums.
 *
 * @param tbl
 *  Pointer pointing to a VxLAN GRO table
 * @param flush_timestamp
 *  This function flushes packets which are inserted into the table
 *  before or at the flush_timestamp.
 * @param out
 *  Pointer array used to keep flushed packets
 * @param nb_out
 *  The element number in 'out'. It also determines the maximum number of
 *  packets that can be flushed finally.
 *
 * @return
 *  The number of flushed packets
 */
uint16_t gro_vxlan_udp4_tbl_timeout_flush(struct gro_vxlan_udp4_tbl *tbl,
		uint64_t flush_timestamp,
		struct rte_mbuf **out,
		uint16_t nb_out);

/**
 * This function returns the number of the packets in a VxLAN
 * reassembly table.
 *
 * @param tbl
 *  Pointer pointing to the VxLAN reassembly table
 *
 * @return
 *  The number of packets in the table
 */
uint32_t gro_vxlan_udp4_tbl_pkt_count(void *tbl);
#endif
