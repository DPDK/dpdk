/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include "gro_tcp6.h"
#include "gro_tcp_internal.h"

void *
gro_tcp6_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow)
{
	struct gro_tcp6_tbl *tbl;
	size_t size;
	uint32_t entries_num, i;

	entries_num = max_flow_num * max_item_per_flow;
	entries_num = RTE_MIN(entries_num, GRO_TCP6_TBL_MAX_ITEM_NUM);

	if (entries_num == 0)
		return NULL;

	tbl = rte_zmalloc_socket(__func__,
			sizeof(struct gro_tcp6_tbl),
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl == NULL)
		return NULL;

	size = sizeof(struct gro_tcp_item) * entries_num;
	tbl->items = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->items == NULL) {
		rte_free(tbl);
		return NULL;
	}
	tbl->max_item_num = entries_num;

	size = sizeof(struct gro_tcp6_flow) * entries_num;
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
gro_tcp6_tbl_destroy(void *tbl)
{
	struct gro_tcp6_tbl *tcp_tbl = tbl;

	if (tcp_tbl) {
		rte_free(tcp_tbl->items);
		rte_free(tcp_tbl->flows);
	}
	rte_free(tcp_tbl);
}

static inline uint32_t
find_an_empty_flow(struct gro_tcp6_tbl *tbl)
{
	uint32_t i;
	uint32_t max_flow_num = tbl->max_flow_num;

	for (i = 0; i < max_flow_num; i++)
		if (tbl->flows[i].start_index == INVALID_ARRAY_INDEX)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
insert_new_flow(struct gro_tcp6_tbl *tbl,
		struct tcp6_flow_key *src,
		uint32_t item_idx)
{
	struct tcp6_flow_key *dst;
	uint32_t flow_idx;

	flow_idx = find_an_empty_flow(tbl);
	if (unlikely(flow_idx == INVALID_ARRAY_INDEX))
		return INVALID_ARRAY_INDEX;

	dst = &(tbl->flows[flow_idx].key);

	ASSIGN_COMMON_TCP_KEY((&src->cmn_key), (&dst->cmn_key));
	memcpy(&dst->src_addr[0], &src->src_addr[0], sizeof(dst->src_addr));
	memcpy(&dst->dst_addr[0], &src->dst_addr[0], sizeof(dst->dst_addr));
	dst->vtc_flow = src->vtc_flow;

	tbl->flows[flow_idx].start_index = item_idx;
	tbl->flow_num++;

	return flow_idx;
}

/*
 * update the packet length for the flushed packet.
 */
static inline void
update_header(struct gro_tcp_item *item)
{
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_mbuf *pkt = item->firstseg;

	ipv6_hdr = (struct rte_ipv6_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			pkt->l2_len);
	ipv6_hdr->payload_len = rte_cpu_to_be_16(pkt->pkt_len -
			pkt->l2_len - pkt->l3_len);
}

int32_t
gro_tcp6_reassemble(struct rte_mbuf *pkt,
		struct gro_tcp6_tbl *tbl,
		uint64_t start_time)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	int32_t tcp_dl;
	uint16_t ip_tlen;
	struct tcp6_flow_key key;
	uint32_t i, max_flow_num, remaining_flow_num;
	uint32_t sent_seq;
	struct rte_tcp_hdr *tcp_hdr;
	uint8_t find;
	uint32_t item_idx;
	/*
	 * Don't process the packet whose TCP header length is greater
	 * than 60 bytes or less than 20 bytes.
	 */
	if (unlikely(INVALID_TCP_HDRLEN(pkt->l4_len)))
		return -1;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	ipv6_hdr = (struct rte_ipv6_hdr *)((char *)eth_hdr + pkt->l2_len);
	tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, pkt->l2_len + pkt->l3_len);

	/*
	 * Don't process the packet which has FIN, SYN, RST, PSH, URG, ECE
	 * or CWR set.
	 */
	if (tcp_hdr->tcp_flags != RTE_TCP_ACK_FLAG)
		return -1;

	ip_tlen = rte_be_to_cpu_16(ipv6_hdr->payload_len);
	/*
	 * Don't process the packet whose payload length is less than or
	 * equal to 0.
	 */
	tcp_dl = ip_tlen - pkt->l4_len;
	if (tcp_dl <= 0)
		return -1;

	rte_ether_addr_copy(&(eth_hdr->src_addr), &(key.cmn_key.eth_saddr));
	rte_ether_addr_copy(&(eth_hdr->dst_addr), &(key.cmn_key.eth_daddr));
	memcpy(&key.src_addr[0], &ipv6_hdr->src_addr, sizeof(key.src_addr));
	memcpy(&key.dst_addr[0], &ipv6_hdr->dst_addr, sizeof(key.dst_addr));
	key.cmn_key.src_port = tcp_hdr->src_port;
	key.cmn_key.dst_port = tcp_hdr->dst_port;
	key.cmn_key.recv_ack = tcp_hdr->recv_ack;
	key.vtc_flow = ipv6_hdr->vtc_flow;

	/* Search for a matched flow. */
	max_flow_num = tbl->max_flow_num;
	remaining_flow_num = tbl->flow_num;
	find = 0;
	for (i = 0; i < max_flow_num && remaining_flow_num; i++) {
		if (tbl->flows[i].start_index != INVALID_ARRAY_INDEX) {
			if (is_same_tcp6_flow(&tbl->flows[i].key, &key)) {
				find = 1;
				break;
			}
			remaining_flow_num--;
		}
	}

	if (find == 0) {
		sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
		item_idx = insert_new_tcp_item(pkt, tbl->items, &tbl->item_num,
						tbl->max_item_num, start_time,
						INVALID_ARRAY_INDEX, sent_seq, 0, true);
		if (item_idx == INVALID_ARRAY_INDEX)
			return -1;
		if (insert_new_flow(tbl, &key, item_idx) ==
			INVALID_ARRAY_INDEX) {
			/*
			 * Fail to insert a new flow, so delete the
			 * stored packet.
			 */
			delete_tcp_item(tbl->items, item_idx, &tbl->item_num, INVALID_ARRAY_INDEX);
			return -1;
		}
		return 0;
	}

	return process_tcp_item(pkt, tcp_hdr, tcp_dl, tbl->items, tbl->flows[i].start_index,
						&tbl->item_num, tbl->max_item_num,
						0, true, start_time);
}

uint16_t
gro_tcp6_tbl_timeout_flush(struct gro_tcp6_tbl *tbl,
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
				j = delete_tcp_item(tbl->items, j,
						&tbl->item_num, INVALID_ARRAY_INDEX);
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
gro_tcp6_tbl_pkt_count(void *tbl)
{
	struct gro_tcp6_tbl *gro_tbl = tbl;

	if (gro_tbl)
		return gro_tbl->item_num;

	return 0;
}
