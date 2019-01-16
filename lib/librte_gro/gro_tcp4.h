/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _GRO_TCP4_H_
#define _GRO_TCP4_H_

#include <rte_ip.h>
#include <rte_tcp.h>

#define INVALID_ARRAY_INDEX 0xffffffffUL
#define GRO_TCP4_TBL_MAX_ITEM_NUM (1024UL * 1024UL)

/*
 * The max length of a IPv4 packet, which includes the length of the L3
 * header, the L4 header and the data payload.
 */
#define MAX_IPV4_PKT_LENGTH UINT16_MAX

/* The maximum TCP header length */
#define MAX_TCP_HLEN 60
#define INVALID_TCP_HDRLEN(len) \
	(((len) < sizeof(struct tcp_hdr)) || ((len) > MAX_TCP_HLEN))

/* Header fields representing a TCP/IPv4 flow */
struct tcp4_flow_key {
	struct ether_addr eth_saddr;
	struct ether_addr eth_daddr;
	uint32_t ip_src_addr;
	uint32_t ip_dst_addr;

	uint32_t recv_ack;
	uint16_t src_port;
	uint16_t dst_port;
};

struct gro_tcp4_flow {
	struct tcp4_flow_key key;
	/*
	 * The index of the first packet in the flow.
	 * INVALID_ARRAY_INDEX indicates an empty flow.
	 */
	uint32_t start_index;
};

struct gro_tcp4_item {
	/*
	 * The first MBUF segment of the packet. If the value
	 * is NULL, it means the item is empty.
	 */
	struct rte_mbuf *firstseg;
	/* The last MBUF segment of the packet */
	struct rte_mbuf *lastseg;
	/*
	 * The time when the first packet is inserted into the table.
	 * This value won't be updated, even if the packet is merged
	 * with other packets.
	 */
	uint64_t start_time;
	/*
	 * next_pkt_idx is used to chain the packets that
	 * are in the same flow but can't be merged together
	 * (e.g. caused by packet reordering).
	 */
	uint32_t next_pkt_idx;
	/* TCP sequence number of the packet */
	uint32_t sent_seq;
	/* IPv4 ID of the packet */
	uint16_t ip_id;
	/* the number of merged packets */
	uint16_t nb_merged;
	/* Indicate if IPv4 ID can be ignored */
	uint8_t is_atomic;
};

/*
 * TCP/IPv4 reassembly table structure.
 */
struct gro_tcp4_tbl {
	/* item array */
	struct gro_tcp4_item *items;
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
	return (is_same_ether_addr(&k1.eth_saddr, &k2.eth_saddr) &&
			is_same_ether_addr(&k1.eth_daddr, &k2.eth_daddr) &&
			(k1.ip_src_addr == k2.ip_src_addr) &&
			(k1.ip_dst_addr == k2.ip_dst_addr) &&
			(k1.recv_ack == k2.recv_ack) &&
			(k1.src_port == k2.src_port) &&
			(k1.dst_port == k2.dst_port));
}

/*
 * Merge two TCP/IPv4 packets without updating checksums.
 * If cmp is larger than 0, append the new packet to the
 * original packet. Otherwise, pre-pend the new packet to
 * the original packet.
 */
static inline int
merge_two_tcp4_packets(struct gro_tcp4_item *item,
		struct rte_mbuf *pkt,
		int cmp,
		uint32_t sent_seq,
		uint16_t ip_id,
		uint16_t l2_offset)
{
	struct rte_mbuf *pkt_head, *pkt_tail, *lastseg;
	uint16_t hdr_len, l2_len;

	if (cmp > 0) {
		pkt_head = item->firstseg;
		pkt_tail = pkt;
	} else {
		pkt_head = pkt;
		pkt_tail = item->firstseg;
	}

	/* check if the IPv4 packet length is greater than the max value */
	hdr_len = l2_offset + pkt_head->l2_len + pkt_head->l3_len +
		pkt_head->l4_len;
	l2_len = l2_offset > 0 ? pkt_head->outer_l2_len : pkt_head->l2_len;
	if (unlikely(pkt_head->pkt_len - l2_len + pkt_tail->pkt_len -
				hdr_len > MAX_IPV4_PKT_LENGTH))
		return 0;

	/* remove the packet header for the tail packet */
	rte_pktmbuf_adj(pkt_tail, hdr_len);

	/* chain two packets together */
	if (cmp > 0) {
		item->lastseg->next = pkt;
		item->lastseg = rte_pktmbuf_lastseg(pkt);
		/* update IP ID to the larger value */
		item->ip_id = ip_id;
	} else {
		lastseg = rte_pktmbuf_lastseg(pkt);
		lastseg->next = item->firstseg;
		item->firstseg = pkt;
		/* update sent_seq to the smaller value */
		item->sent_seq = sent_seq;
		item->ip_id = ip_id;
	}
	item->nb_merged++;

	/* update MBUF metadata for the merged packet */
	pkt_head->nb_segs += pkt_tail->nb_segs;
	pkt_head->pkt_len += pkt_tail->pkt_len;

	return 1;
}

/*
 * Check if two TCP/IPv4 packets are neighbors.
 */
static inline int
check_seq_option(struct gro_tcp4_item *item,
		struct tcp_hdr *tcph,
		uint32_t sent_seq,
		uint16_t ip_id,
		uint16_t tcp_hl,
		uint16_t tcp_dl,
		uint16_t l2_offset,
		uint8_t is_atomic)
{
	struct rte_mbuf *pkt_orig = item->firstseg;
	struct ipv4_hdr *iph_orig;
	struct tcp_hdr *tcph_orig;
	uint16_t len, tcp_hl_orig;

	iph_orig = (struct ipv4_hdr *)(rte_pktmbuf_mtod(pkt_orig, char *) +
			l2_offset + pkt_orig->l2_len);
	tcph_orig = (struct tcp_hdr *)((char *)iph_orig + pkt_orig->l3_len);
	tcp_hl_orig = pkt_orig->l4_len;

	/* Check if TCP option fields equal */
	len = RTE_MAX(tcp_hl, tcp_hl_orig) - sizeof(struct tcp_hdr);
	if ((tcp_hl != tcp_hl_orig) || ((len > 0) &&
				(memcmp(tcph + 1, tcph_orig + 1,
					len) != 0)))
		return 0;

	/* Don't merge packets whose DF bits are different */
	if (unlikely(item->is_atomic ^ is_atomic))
		return 0;

	/* check if the two packets are neighbors */
	len = pkt_orig->pkt_len - l2_offset - pkt_orig->l2_len -
		pkt_orig->l3_len - tcp_hl_orig;
	if ((sent_seq == item->sent_seq + len) && (is_atomic ||
				(ip_id == item->ip_id + 1)))
		/* append the new packet */
		return 1;
	else if ((sent_seq + tcp_dl == item->sent_seq) && (is_atomic ||
				(ip_id + item->nb_merged == item->ip_id)))
		/* pre-pend the new packet */
		return -1;

	return 0;
}
#endif
