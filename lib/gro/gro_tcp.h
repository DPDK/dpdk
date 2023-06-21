/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */
#ifndef _GRO_TCP_H_
#define _GRO_TCP_H_

#define INVALID_ARRAY_INDEX 0xffffffffUL

#include <rte_tcp.h>

/*
 * The max length of a IPv4 packet, which includes the length of the L3
 * header, the L4 header and the data payload.
 */
#define MAX_IP_PKT_LENGTH UINT16_MAX

/* The maximum TCP header length */
#define MAX_TCP_HLEN 60
#define INVALID_TCP_HDRLEN(len) \
	(((len) < sizeof(struct rte_tcp_hdr)) || ((len) > MAX_TCP_HLEN))

struct cmn_tcp_key {
	struct rte_ether_addr eth_saddr;
	struct rte_ether_addr eth_daddr;
	uint32_t recv_ack;
	uint16_t src_port;
	uint16_t dst_port;
};

#define ASSIGN_COMMON_TCP_KEY(k1, k2) \
	do {\
		rte_ether_addr_copy(&(k1->eth_saddr), &(k2->eth_saddr)); \
		rte_ether_addr_copy(&(k1->eth_daddr), &(k2->eth_daddr)); \
		k2->recv_ack = k1->recv_ack; \
		k2->src_port = k1->src_port; \
		k2->dst_port = k1->dst_port; \
	} while (0)

struct gro_tcp_item {
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
	union {
		/* IPv4 ID of the packet */
		uint16_t ip_id;
		/* Unused field for IPv6 */
		uint16_t unused;
	} l3;
	/* the number of merged packets */
	uint16_t nb_merged;
	/* Indicate if IPv4 ID can be ignored */
	uint8_t is_atomic;
};

/*
 * Merge two TCP packets without updating checksums.
 * If cmp is larger than 0, append the new packet to the
 * original packet. Otherwise, pre-pend the new packet to
 * the original packet.
 */
static inline int
merge_two_tcp_packets(struct gro_tcp_item *item,
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
				hdr_len > MAX_IP_PKT_LENGTH))
		return 0;

	if (unlikely(pkt_head->nb_segs >= 20))
		return 0;

	/* remove the packet header for the tail packet */
	rte_pktmbuf_adj(pkt_tail, hdr_len);

	/* chain two packets together */
	if (cmp > 0) {
		item->lastseg->next = pkt;
		item->lastseg = rte_pktmbuf_lastseg(pkt);
		/* update IP ID to the larger value */
		item->l3.ip_id = ip_id;
	} else {
		lastseg = rte_pktmbuf_lastseg(pkt);
		lastseg->next = item->firstseg;
		item->firstseg = pkt;
		/* update sent_seq to the smaller value */
		item->sent_seq = sent_seq;
		item->l3.ip_id = ip_id;
	}
	item->nb_merged++;

	/* update MBUF metadata for the merged packet */
	pkt_head->nb_segs += pkt_tail->nb_segs;
	pkt_head->pkt_len += pkt_tail->pkt_len;

	return 1;
}

/*
 * Check if two TCP packets are neighbors.
 */
static inline int
check_seq_option(struct gro_tcp_item *item,
		struct rte_tcp_hdr *tcph,
		uint32_t sent_seq,
		uint16_t ip_id,
		uint16_t tcp_hl,
		uint16_t tcp_dl,
		uint16_t l2_offset,
		uint8_t is_atomic)
{
	struct rte_mbuf *pkt_orig = item->firstseg;
	char *iph_orig;
	struct rte_tcp_hdr *tcph_orig;
	uint16_t len, tcp_hl_orig;

	iph_orig = (char *)(rte_pktmbuf_mtod(pkt_orig, char *) +
			l2_offset + pkt_orig->l2_len);
	tcph_orig = (struct rte_tcp_hdr *)(iph_orig + pkt_orig->l3_len);
	tcp_hl_orig = pkt_orig->l4_len;

	/* Check if TCP option fields equal */
	len = RTE_MAX(tcp_hl, tcp_hl_orig) - sizeof(struct rte_tcp_hdr);
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
				(ip_id == item->l3.ip_id + 1)))
		/* append the new packet */
		return 1;
	else if ((sent_seq + tcp_dl == item->sent_seq) && (is_atomic ||
				(ip_id + item->nb_merged == item->l3.ip_id)))
		/* pre-pend the new packet */
		return -1;

	return 0;
}

static inline int
is_same_common_tcp_key(struct cmn_tcp_key *k1, struct cmn_tcp_key *k2)
{
	return (!memcmp(k1, k2, sizeof(struct cmn_tcp_key)));
}

#endif
