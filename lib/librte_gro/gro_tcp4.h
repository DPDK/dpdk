/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#ifndef _GRO_TCP4_H_
#define _GRO_TCP4_H_

#define INVALID_ARRAY_INDEX 0xffffffffUL
#define GRO_TCP4_TBL_MAX_ITEM_NUM (1024UL * 1024UL)

/*
 * the max L3 length of a TCP/IPv4 packet. The L3 length
 * is the sum of ipv4 header, tcp header and L4 payload.
 */
#define TCP4_MAX_L3_LENGTH UINT16_MAX

/* criteria of mergeing packets */
struct tcp4_key {
	struct ether_addr eth_saddr;
	struct ether_addr eth_daddr;
	uint32_t ip_src_addr;
	uint32_t ip_dst_addr;

	uint32_t recv_ack;
	uint16_t src_port;
	uint16_t dst_port;
};

struct gro_tcp4_key {
	struct tcp4_key key;
	/*
	 * the index of the first packet in the item group.
	 * If the value is INVALID_ARRAY_INDEX, it means
	 * the key is empty.
	 */
	uint32_t start_index;
};

struct gro_tcp4_item {
	/*
	 * first segment of the packet. If the value
	 * is NULL, it means the item is empty.
	 */
	struct rte_mbuf *firstseg;
	/* last segment of the packet */
	struct rte_mbuf *lastseg;
	/*
	 * the time when the first packet is inserted
	 * into the table. If a packet in the table is
	 * merged with an incoming packet, this value
	 * won't be updated. We set this value only
	 * when the first packet is inserted into the
	 * table.
	 */
	uint64_t start_time;
	/*
	 * we use next_pkt_idx to chain the packets that
	 * have same key value but can't be merged together.
	 */
	uint32_t next_pkt_idx;
	/* the sequence number of the packet */
	uint32_t sent_seq;
	/* the IP ID of the packet */
	uint16_t ip_id;
	/* the number of merged packets */
	uint16_t nb_merged;
};

/*
 * TCP/IPv4 reassembly table structure.
 */
struct gro_tcp4_tbl {
	/* item array */
	struct gro_tcp4_item *items;
	/* key array */
	struct gro_tcp4_key *keys;
	/* current item number */
	uint32_t item_num;
	/* current key num */
	uint32_t key_num;
	/* item array size */
	uint32_t max_item_num;
	/* key array size */
	uint32_t max_key_num;
};

/**
 * This function creates a TCP/IPv4 reassembly table.
 *
 * @param socket_id
 *  socket index for allocating TCP/IPv4 reassemblt table
 * @param max_flow_num
 *  the maximum number of flows in the TCP/IPv4 GRO table
 * @param max_item_per_flow
 *  the maximum packet number per flow.
 *
 * @return
 *  if create successfully, return a pointer which points to the
 *  created TCP/IPv4 GRO table. Otherwise, return NULL.
 */
void *gro_tcp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);

/**
 * This function destroys a TCP/IPv4 reassembly table.
 *
 * @param tbl
 *  a pointer points to the TCP/IPv4 reassembly table.
 */
void gro_tcp4_tbl_destroy(void *tbl);

/**
 * This function searches for a packet in the TCP/IPv4 reassembly table
 * to merge with the inputted one. To merge two packets is to chain them
 * together and update packet headers. Packets, whose SYN, FIN, RST, PSH
 * CWR, ECE or URG bit is set, are returned immediately. Packets which
 * only have packet headers (i.e. without data) are also returned
 * immediately. Otherwise, the packet is either merged, or inserted into
 * the table. Besides, if there is no available space to insert the
 * packet, this function returns immediately too.
 *
 * This function assumes the inputted packet is with correct IPv4 and
 * TCP checksums. And if two packets are merged, it won't re-calculate
 * IPv4 and TCP checksums. Besides, if the inputted packet is IP
 * fragmented, it assumes the packet is complete (with TCP header).
 *
 * @param pkt
 *  packet to reassemble.
 * @param tbl
 *  a pointer that points to a TCP/IPv4 reassembly table.
 * @start_time
 *  the start time that the packet is inserted into the table
 *
 * @return
 *  if the packet doesn't have data, or SYN, FIN, RST, PSH, CWR, ECE
 *  or URG bit is set, or there is no available space in the table to
 *  insert a new item or a new key, return a negative value. If the
 *  packet is merged successfully, return an positive value. If the
 *  packet is inserted into the table, return 0.
 */
int32_t gro_tcp4_reassemble(struct rte_mbuf *pkt,
		struct gro_tcp4_tbl *tbl,
		uint64_t start_time);

/**
 * This function flushes timeout packets in a TCP/IPv4 reassembly table
 * to applications, and without updating checksums for merged packets.
 * The max number of flushed timeout packets is the element number of
 * the array which is used to keep flushed packets.
 *
 * @param tbl
 *  a pointer that points to a TCP GRO table.
 * @param flush_timestamp
 *  this function flushes packets which are inserted into the table
 *  before or at the flush_timestamp.
 * @param out
 *  pointer array which is used to keep flushed packets.
 * @param nb_out
 *  the element number of out. It's also the max number of timeout
 *  packets that can be flushed finally.
 *
 * @return
 *  the number of packets that are returned.
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
 *  pointer points to a TCP/IPv4 reassembly table.
 *
 * @return
 *  the number of packets in the table
 */
uint32_t gro_tcp4_tbl_pkt_count(void *tbl);
#endif
