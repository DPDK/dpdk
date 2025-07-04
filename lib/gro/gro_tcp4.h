/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _GRO_TCP4_H_
#define _GRO_TCP4_H_

#include "gro_tcp.h"

#define GRO_TCP4_TBL_MAX_ITEM_NUM (1024UL * 1024UL)

/* Header fields representing common fields in TCP flow */
struct tcp4_flow_key {
	struct cmn_tcp_key cmn_key;
	uint32_t ip_src_addr;
	uint32_t ip_dst_addr;
};

struct gro_tcp4_flow {
	struct tcp4_flow_key key;
	/*
	 * The index of the first packet in the flow.
	 * INVALID_ARRAY_INDEX indicates an empty flow.
	 */
	uint32_t start_index;
};

/*
 * TCP/IPv4 reassembly table structure.
 */
struct gro_tcp4_tbl {
	/* item array */
	struct gro_tcp_item *items;
	/* flow array */
	struct gro_tcp4_flow *flows;
	/* current item number */
	uint32_t item_num;
	/* current flow num */
	uint32_t flow_num;
	/* item array size */
	uint32_t max_item_num;
	/* flow array size */
	uint32_t max_flow_num;
};

/**
 * This function creates a TCP/IPv4 reassembly table.
 *
 * @param socket_id
 *  Socket index for allocating the TCP/IPv4 reassemble table
 * @param max_flow_num
 *  The maximum number of flows in the TCP/IPv4 GRO table
 * @param max_item_per_flow
 *  The maximum number of packets per flow
 *
 * @return
 *  - Return the table pointer on success.
 *  - Return NULL on failure.
 */
void *gro_tcp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);

/**
 * This function destroys a TCP/IPv4 reassembly table.
 *
 * @param tbl
 *  Pointer pointing to the TCP/IPv4 reassembly table.
 */
void gro_tcp4_tbl_destroy(void *tbl);

/**
 * This function merges a TCP/IPv4 packet. It doesn't process the packet,
 * which has SYN, FIN, RST, PSH, CWR, ECE or URG set, or doesn't have
 * payload.
 *
 * This function doesn't check if the packet has correct checksums and
 * doesn't re-calculate checksums for the merged packet. Additionally,
 * it assumes the packets are complete (i.e., MF==0 && frag_off==0),
 * when IP fragmentation is possible (i.e., DF==0). It returns the
 * packet, if the packet has invalid parameters (e.g. SYN bit is set)
 * or there is no available space in the table.
 *
 * @param pkt
 *  Packet to reassemble
 * @param tbl
 *  Pointer pointing to the TCP/IPv4 reassembly table
 * @start_time
 *  The time when the packet is inserted into the table
 *
 * @return
 *  - Return a positive value if the packet is merged.
 *  - Return zero if the packet isn't merged but stored in the table.
 *  - Return a negative value for invalid parameters or no available
 *    space in the table.
 */
int32_t gro_tcp4_reassemble(struct rte_mbuf *pkt,
		struct gro_tcp4_tbl *tbl,
		uint64_t start_time);

/**
 * This function flushes timeout packets in a TCP/IPv4 reassembly table,
 * and without updating checksums.
 *
 * @param tbl
 *  TCP/IPv4 reassembly table pointer
 * @param flush_timestamp
 *  Flush packets which are inserted into the table before or at the
 *  flush_timestamp.
 * @param out
 *  Pointer array used to keep flushed packets
 * @param nb_out
 *  The element number in 'out'. It also determines the maximum number of
 *  packets that can be flushed finally.
 *
 * @return
 *  The number of flushed packets
 */
uint16_t gro_tcp4_tbl_timeout_flush(struct gro_tcp4_tbl *tbl,
		uint64_t flush_timestamp,
		struct rte_mbuf **out,
		uint16_t nb_out);

/**
 * This function returns the number of the packets in a TCP/IPv4
 * reassembly table.
 *
 * @param tbl
 *  TCP/IPv4 reassembly table pointer
 *
 * @return
 *  The number of packets in the table
 */
uint32_t gro_tcp4_tbl_pkt_count(void *tbl);

/*
 * Check if two TCP/IPv4 packets belong to the same flow.
 */
static inline int
is_same_tcp4_flow(struct tcp4_flow_key k1, struct tcp4_flow_key k2)
{
	return ((k1.ip_src_addr == k2.ip_src_addr) &&
			(k1.ip_dst_addr == k2.ip_dst_addr) &&
			is_same_common_tcp_key(&k1.cmn_key, &k2.cmn_key));
}

#endif
