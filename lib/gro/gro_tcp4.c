/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>

#include "gro_tcp4.h"

void *
gro_tcp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow)
{
	struct gro_tcp4_tbl *tbl;
	size_t size;
	uint32_t entries_num, i;

	entries_num = max_flow_num * max_item_per_flow;
	entries_num = RTE_MIN(entries_num, GRO_TCP4_TBL_MAX_ITEM_NUM);

	if (entries_num == 0)
		return NULL;

	tbl = rte_zmalloc_socket(__func__,
			sizeof(struct gro_tcp4_tbl),
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl == NULL)
		return NULL;

	size = sizeof(struct gro_tcp4_item) * entries_num;
	tbl->items = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->items == NULL) {
		rte_free(tbl);
		return NULL;
	}
	tbl->max_item_num = entries_num;

	size = sizeof(struct gro_tcp4_flow) * entries_num;
	tbl->flows = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->flows == NULL) {
		rte_free(tbl->items);
		rte_free(tbl);
		return NULL;
	}
	/* INVALID_ARRAY_INDEX indicates an empty flow */
	for (i = 0; i < entries_num; i++)
		tbl->flows[i].start_index = INVALID_ARRAY_INDEX;
	tbl->max_flow_num = entries_num;

	return tbl;
}

void
gro_tcp4_tbl_destroy(void *tbl)
{
	struct gro_tcp4_tbl *tcp_tbl = tbl;

	if (tcp_tbl) {
		rte_free(tcp_tbl->items);
		rte_free(tcp_tbl->flows);
	}
	rte_free(tcp_tbl);
}

static inline uint32_t
find_an_empty_item(struct gro_tcp4_tbl *tbl)
{
	uint32_t i;
	uint32_t max_item_num = tbl->max_item_num;

	for (i = 0; i < max_item_num; i++)
		if (tbl->items[i].firstseg == NULL)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
find_an_empty_flow(struct gro_tcp4_tbl *tbl)
{
	uint32_t i;
	uint32_t max_flow_num = tbl->max_flow_num;

	for (i = 0; i < max_flow_num; i++)
		if (tbl->flows[i].start_index == INVALID_ARRAY_INDEX)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
insert_new_item(struct gro_tcp4_tbl *tbl,
		struct rte_mbuf *pkt,
		uint64_t start_time,
		uint32_t prev_idx,
		uint32_t sent_seq,
		uint16_t ip_id,
		uint8_t is_atomic)
{
	uint32_t item_idx;

	item_idx = find_an_empty_item(tbl);
	if (item_idx == INVALID_ARRAY_INDEX)
		return INVALID_ARRAY_INDEX;

	tbl->items[item_idx].firstseg = pkt;
	tbl->items[item_idx].lastseg = rte_pktmbuf_lastseg(pkt);
	tbl->items[item_idx].start_time = start_time;
	tbl->items[item_idx].next_pkt_idx = INVALID_ARRAY_INDEX;
	tbl->items[item_idx].sent_seq = sent_seq;
	tbl->items[item_idx].ip_id = ip_id;
	tbl->items[item_idx].nb_merged = 1;
	tbl->items[item_idx].is_atomic = is_atomic;
	tbl->item_num++;

	/* if the previous packet exists, chain them together. */
	if (prev_idx != INVALID_ARRAY_INDEX) {
		tbl->items[item_idx].next_pkt_idx =
			tbl->items[prev_idx].next_pkt_idx;
		tbl->items[prev_idx].next_pkt_idx = item_idx;
	}

	return item_idx;
}

static inline uint32_t
delete_item(struct gro_tcp4_tbl *tbl, uint32_t item_idx,
		uint32_t prev_item_idx)
{
	uint32_t next_idx = tbl->items[item_idx].next_pkt_idx;

	/* NULL indicates an empty item */
	tbl->items[item_idx].firstseg = NULL;
	tbl->item_num--;
	if (prev_item_idx != INVALID_ARRAY_INDEX)
		tbl->items[prev_item_idx].next_pkt_idx = next_idx;

	return next_idx;
}

static inline uint32_t
insert_new_flow(struct gro_tcp4_tbl *tbl,
		struct tcp4_flow_key *src,
		uint32_t item_idx)
{
	struct tcp4_flow_key *dst;
	uint32_t flow_idx;

	flow_idx = find_an_empty_flow(tbl);
	if (unlikely(flow_idx == INVALID_ARRAY_INDEX))
		return INVALID_ARRAY_INDEX;

	dst = &(tbl->flows[flow_idx].key);

	rte_ether_addr_copy(&(src->eth_saddr), &(dst->eth_saddr));
	rte_ether_addr_copy(&(src->eth_daddr), &(dst->eth_daddr));
	dst->ip_src_addr = src->ip_src_addr;
	dst->ip_dst_addr = src->ip_dst_addr;
	dst->recv_ack = src->recv_ack;
	dst->src_port = src->src_port;
	dst->dst_port = src->dst_port;

	tbl->flows[flow_idx].start_index = item_idx;
	tbl->flow_num++;

	return flow_idx;
}

/*
 * update the packet length for the flushed packet.
 */
static inline void
update_header(struct gro_tcp4_item *item)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_mbuf *pkt = item->firstseg;

	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			pkt->l2_len);
	ipv4_hdr->total_length = rte_cpu_to_be_16(pkt->pkt_len -
			pkt->l2_len);
}

int32_t
gro_tcp4_reassemble(struct rte_mbuf *pkt,
		struct gro_tcp4_tbl *tbl,
		uint64_t start_time)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	uint32_t sent_seq;
	int32_t tcp_dl;
	uint16_t ip_id, hdr_len, frag_off;
	uint8_t is_atomic;

	struct tcp4_flow_key key;
	uint32_t cur_idx, prev_idx, item_idx;
	uint32_t i, max_flow_num, remaining_flow_num;
	int cmp;
	uint8_t find;

	/*
	 * Don't process the packet whose TCP header length is greater
	 * than 60 bytes or less than 20 bytes.
	 */
	if (unlikely(INVALID_TCP_HDRLEN(pkt->l4_len)))
		return -1;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	ipv4_hdr = (struct rte_ipv4_hdr *)((char *)eth_hdr + pkt->l2_len);
	tcp_hdr = (struct rte_tcp_hdr *)((char *)ipv4_hdr + pkt->l3_len);
	hdr_len = pkt->l2_len + pkt->l3_len + pkt->l4_len;

	/*
	 * Don't process the packet which has FIN, SYN, RST, PSH, URG, ECE
	 * or CWR set.
	 */
	if (tcp_hdr->tcp_flags != RTE_TCP_ACK_FLAG)
		return -1;
	/*
	 * Don't process the packet whose payload length is less than or
	 * equal to 0.
	 */
	tcp_dl = pkt->pkt_len - hdr_len;
	if (tcp_dl <= 0)
		return -1;

	/*
	 * Save IPv4 ID for the packet whose DF bit is 0. For the packet
	 * whose DF bit is 1, IPv4 ID is ignored.
	 */
	frag_off = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
	is_atomic = (frag_off & RTE_IPV4_HDR_DF_FLAG) == RTE_IPV4_HDR_DF_FLAG;
	ip_id = is_atomic ? 0 : rte_be_to_cpu_16(ipv4_hdr->packet_id);
	sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);

	rte_ether_addr_copy(&(eth_hdr->s_addr), &(key.eth_saddr));
	rte_ether_addr_copy(&(eth_hdr->d_addr), &(key.eth_daddr));
	key.ip_src_addr = ipv4_hdr->src_addr;
	key.ip_dst_addr = ipv4_hdr->dst_addr;
	key.src_port = tcp_hdr->src_port;
	key.dst_port = tcp_hdr->dst_port;
	key.recv_ack = tcp_hdr->recv_ack;

	/* Search for a matched flow. */
	max_flow_num = tbl->max_flow_num;
	remaining_flow_num = tbl->flow_num;
	find = 0;
	for (i = 0; i < max_flow_num && remaining_flow_num; i++) {
		if (tbl->flows[i].start_index != INVALID_ARRAY_INDEX) {
			if (is_same_tcp4_flow(tbl->flows[i].key, key)) {
				find = 1;
				break;
			}
			remaining_flow_num--;
		}
	}

	/*
	 * Fail to find a matched flow. Insert a new flow and store the
	 * packet into the flow.
	 */
	if (find == 0) {
		item_idx = insert_new_item(tbl, pkt, start_time,
				INVALID_ARRAY_INDEX, sent_seq, ip_id,
				is_atomic);
		if (item_idx == INVALID_ARRAY_INDEX)
			return -1;
		if (insert_new_flow(tbl, &key, item_idx) ==
				INVALID_ARRAY_INDEX) {
			/*
			 * Fail to insert a new flow, so delete the
			 * stored packet.
			 */
			delete_item(tbl, item_idx, INVALID_ARRAY_INDEX);
			return -1;
		}
		return 0;
	}

	/*
	 * Check all packets in the flow and try to find a neighbor for
	 * the input packet.
	 */
	cur_idx = tbl->flows[i].start_index;
	prev_idx = cur_idx;
	do {
		cmp = check_seq_option(&(tbl->items[cur_idx]), tcp_hdr,
				sent_seq, ip_id, pkt->l4_len, tcp_dl, 0,
				is_atomic);
		if (cmp) {
			if (merge_two_tcp4_packets(&(tbl->items[cur_idx]),
						pkt, cmp, sent_seq, ip_id, 0))
				return 1;
			/*
			 * Fail to merge the two packets, as the packet
			 * length is greater than the max value. Store
			 * the packet into the flow.
			 */
			if (insert_new_item(tbl, pkt, start_time, prev_idx,
						sent_seq, ip_id, is_atomic) ==
					INVALID_ARRAY_INDEX)
				return -1;
			return 0;
		}
		prev_idx = cur_idx;
		cur_idx = tbl->items[cur_idx].next_pkt_idx;
	} while (cur_idx != INVALID_ARRAY_INDEX);

	/* Fail to find a neighbor, so store the packet into the flow. */
	if (insert_new_item(tbl, pkt, start_time, prev_idx, sent_seq,
				ip_id, is_atomic) == INVALID_ARRAY_INDEX)
		return -1;

	return 0;
}

uint16_t
gro_tcp4_tbl_timeout_flush(struct gro_tcp4_tbl *tbl,
		uint64_t flush_timestamp,
		struct rte_mbuf **out,
		uint16_t nb_out)
{
	uint16_t k = 0;
	uint32_t i, j;
	uint32_t max_flow_num = tbl->max_flow_num;

	for (i = 0; i < max_flow_num; i++) {
		if (unlikely(tbl->flow_num == 0))
			return k;

		j = tbl->flows[i].start_index;
		while (j != INVALID_ARRAY_INDEX) {
			if (tbl->items[j].start_time <= flush_timestamp) {
				out[k++] = tbl->items[j].firstseg;
				if (tbl->items[j].nb_merged > 1)
					update_header(&(tbl->items[j]));
				/*
				 * Delete the packet and get the next
				 * packet in the flow.
				 */
				j = delete_item(tbl, j, INVALID_ARRAY_INDEX);
				tbl->flows[i].start_index = j;
				if (j == INVALID_ARRAY_INDEX)
					tbl->flow_num--;

				if (unlikely(k == nb_out))
					return k;
			} else
				/*
				 * The left packets in this flow won't be
				 * timeout. Go to check other flows.
				 */
				break;
		}
	}
	return k;
}

uint32_t
gro_tcp4_tbl_pkt_count(void *tbl)
{
	struct gro_tcp4_tbl *gro_tbl = tbl;

	if (gro_tbl)
		return gro_tbl->item_num;

	return 0;
}
