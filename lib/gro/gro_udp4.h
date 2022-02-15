/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Inspur Corporation
 */

#ifndef _GRO_UDP4_H_
#define _GRO_UDP4_H_

#include <rte_ip.h>

#define INVALID_ARRAY_INDEX 0xffffffffUL
#define GRO_UDP4_TBL_MAX_ITEM_NUM (1024UL * 1024UL)

/*
 * The max length of a IPv4 packet, which includes the length of the L3
 * header, the L4 header and the data payload.
 */
#define MAX_IPV4_PKT_LENGTH UINT16_MAX

/* Header fields representing a UDP/IPv4 flow */
struct udp4_flow_key {
	struct rte_ether_addr eth_saddr;
	struct rte_ether_addr eth_daddr;
	uint32_t ip_src_addr;
	uint32_t ip_dst_addr;

	/* IP fragment for UDP does not contain UDP header
	 * except the first one. But IP ID must be same.
	 */
	uint16_t ip_id;
};

struct gro_udp4_flow {
	struct udp4_flow_key key;
	/*
	 * The index of the first packet in the flow.
	 * INVALID_ARRAY_INDEX indicates an empty flow.
	 */
	uint32_t start_index;
};

struct gro_udp4_item {
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
	/* offset of IP fragment packet */
	uint16_t frag_offset;
	/* is last IP fragment? */
	uint8_t is_last_frag;
	/* the number of merged packets */
	uint16_t nb_merged;
};

/*
 * UDP/IPv4 reassembly table structure.
 */
struct gro_udp4_tbl {
	/* item array */
	struct gro_udp4_item *items;
	/* flow array */
	struct gro_udp4_flow *flows;
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
 * This function creates a UDP/IPv4 reassembly table.
 *
 * @param socket_id
 *  Socket index for allocating the UDP/IPv4 reassemble table
 * @param max_flow_num
 *  The maximum number of flows in the UDP/IPv4 GRO table
 * @param max_item_per_flow
 *  The maximum number of packets per flow
 *
 * @return
 *  - Return the table pointer on success.
 *  - Return NULL on failure.
 */
void *gro_udp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow);

/**
 * This function destroys a UDP/IPv4 reassembly table.
 *
 * @param tbl
 *  Pointer pointing to the UDP/IPv4 reassembly table.
 */
void gro_udp4_tbl_destroy(void *tbl);

/**
 * This function merges a UDP/IPv4 packet.
 *
 * This function does not check if the packet has correct checksums and
 * does not re-calculate checksums for the merged packet. It returns the
 * packet if it isn't UDP fragment or there is no available space in
 * the table.
 *
 * @param pkt
 *  Packet to reassemble
 * @param tbl
 *  Pointer pointing to the UDP/IPv4 reassembly table
 * @start_time
 *  The time when the packet is inserted into the table
 *
 * @return
 *  - Return a positive value if the packet is merged.
 *  - Return zero if the packet isn't merged but stored in the table.
 *  - Return a negative value for invalid parameters or no available
 *    space in the table.
 */
int32_t gro_udp4_reassemble(struct rte_mbuf *pkt,
		struct gro_udp4_tbl *tbl,
		uint64_t start_time);

/**
 * This function flushes timeout packets in a UDP/IPv4 reassembly table,
 * and without updating checksums.
 *
 * @param tbl
 *  UDP/IPv4 reassembly table pointer
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
uint16_t gro_udp4_tbl_timeout_flush(struct gro_udp4_tbl *tbl,
		uint64_t flush_timestamp,
		struct rte_mbuf **out,
		uint16_t nb_out);

/**
 * This function returns the number of the packets in a UDP/IPv4
 * reassembly table.
 *
 * @param tbl
 *  UDP/IPv4 reassembly table pointer
 *
 * @return
 *  The number of packets in the table
 */
uint32_t gro_udp4_tbl_pkt_count(void *tbl);

/*
 * Check if two UDP/IPv4 packets belong to the same flow.
 */
static inline int
is_same_udp4_flow(struct udp4_flow_key k1, struct udp4_flow_key k2)
{
	return (rte_is_same_ether_addr(&k1.eth_saddr, &k2.eth_saddr) &&
			rte_is_same_ether_addr(&k1.eth_daddr, &k2.eth_daddr) &&
			(k1.ip_src_addr == k2.ip_src_addr) &&
			(k1.ip_dst_addr == k2.ip_dst_addr) &&
			(k1.ip_id == k2.ip_id));
}

/*
 * Merge two UDP/IPv4 packets without updating checksums.
 * If cmp is larger than 0, append the new packet to the
 * original packet. Otherwise, pre-pend the new packet to
 * the original packet.
 */
static inline int
merge_two_udp4_packets(struct gro_udp4_item *item,
		struct rte_mbuf *pkt,
		int cmp,
		uint16_t frag_offset,
		uint8_t is_last_frag,
		uint16_t l2_offset)
{
	struct rte_mbuf *pkt_head, *pkt_tail, *lastseg;
	uint16_t hdr_len, l2_len;
	uint32_t ip_len;

	if (cmp > 0) {
		pkt_head = item->firstseg;
		pkt_tail = pkt;
	} else {
		pkt_head = pkt;
		pkt_tail = item->firstseg;
	}

	/* check if the IPv4 packet length is greater than the max value */
	hdr_len = l2_offset + pkt_head->l2_len + pkt_head->l3_len;
	l2_len = l2_offset > 0 ? pkt_head->outer_l2_len : pkt_head->l2_len;
	ip_len = pkt_head->pkt_len - l2_len
		 + pkt_tail->pkt_len - hdr_len;
	if (unlikely(ip_len > MAX_IPV4_PKT_LENGTH))
		return 0;

	/* remove the packet header for the tail packet */
	rte_pktmbuf_adj(pkt_tail, hdr_len);

	/* chain two packets together */
	if (cmp > 0) {
		item->lastseg->next = pkt;
		item->lastseg = rte_pktmbuf_lastseg(pkt);
	} else {
		lastseg = rte_pktmbuf_lastseg(pkt);
		lastseg->next = item->firstseg;
		item->firstseg = pkt;
		item->frag_offset = frag_offset;
	}
	item->nb_merged++;
	if (is_last_frag)
		item->is_last_frag = is_last_frag;

	/* update MBUF metadata for the merged packet */
	pkt_head->nb_segs += pkt_tail->nb_segs;
	pkt_head->pkt_len += pkt_tail->pkt_len;

	return 1;
}

/*
 * Check if two UDP/IPv4 packets are neighbors.
 */
static inline int
udp4_check_neighbor(struct gro_udp4_item *item,
		uint16_t frag_offset,
		uint16_t ip_dl,
		uint16_t l2_offset)
{
	struct rte_mbuf *pkt_orig = item->firstseg;
	uint16_t len;

	/* check if the two packets are neighbors */
	len = pkt_orig->pkt_len - l2_offset - pkt_orig->l2_len -
		pkt_orig->l3_len;
	if (frag_offset == item->frag_offset + len)
		/* append the new packet */
		return 1;
	else if (frag_offset + ip_dl == item->frag_offset)
		/* pre-pend the new packet */
		return -1;

	return 0;
}

static inline int
is_ipv4_fragment(const struct rte_ipv4_hdr *hdr)
{
	uint16_t flag_offset, ip_flag, ip_ofs;

	flag_offset = rte_be_to_cpu_16(hdr->fragment_offset);
	ip_ofs = (uint16_t)(flag_offset & RTE_IPV4_HDR_OFFSET_MASK);
	ip_flag = (uint16_t)(flag_offset & RTE_IPV4_HDR_MF_FLAG);

	return ip_flag != 0 || ip_ofs  != 0;
}
#endif
