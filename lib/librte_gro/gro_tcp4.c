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

#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_tcp.h>

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

	size = sizeof(struct gro_tcp4_key) * entries_num;
	tbl->keys = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->keys == NULL) {
		rte_free(tbl->items);
		rte_free(tbl);
		return NULL;
	}
	/* INVALID_ARRAY_INDEX indicates empty key */
	for (i = 0; i < entries_num; i++)
		tbl->keys[i].start_index = INVALID_ARRAY_INDEX;
	tbl->max_key_num = entries_num;

	return tbl;
}

void
gro_tcp4_tbl_destroy(void *tbl)
{
	struct gro_tcp4_tbl *tcp_tbl = tbl;

	if (tcp_tbl) {
		rte_free(tcp_tbl->items);
		rte_free(tcp_tbl->keys);
	}
	rte_free(tcp_tbl);
}

/*
 * merge two TCP/IPv4 packets without updating checksums.
 * If cmp is larger than 0, append the new packet to the
 * original packet. Otherwise, pre-pend the new packet to
 * the original packet.
 */
static inline int
merge_two_tcp4_packets(struct gro_tcp4_item *item_src,
		struct rte_mbuf *pkt,
		uint16_t ip_id,
		uint32_t sent_seq,
		int cmp)
{
	struct rte_mbuf *pkt_head, *pkt_tail, *lastseg;
	uint16_t tcp_datalen;

	if (cmp > 0) {
		pkt_head = item_src->firstseg;
		pkt_tail = pkt;
	} else {
		pkt_head = pkt;
		pkt_tail = item_src->firstseg;
	}

	/* check if the packet length will be beyond the max value */
	tcp_datalen = pkt_tail->pkt_len - pkt_tail->l2_len -
		pkt_tail->l3_len - pkt_tail->l4_len;
	if (pkt_head->pkt_len - pkt_head->l2_len + tcp_datalen >
			TCP4_MAX_L3_LENGTH)
		return 0;

	/* remove packet header for the tail packet */
	rte_pktmbuf_adj(pkt_tail,
			pkt_tail->l2_len +
			pkt_tail->l3_len +
			pkt_tail->l4_len);

	/* chain two packets together */
	if (cmp > 0) {
		item_src->lastseg->next = pkt;
		item_src->lastseg = rte_pktmbuf_lastseg(pkt);
		/* update IP ID to the larger value */
		item_src->ip_id = ip_id;
	} else {
		lastseg = rte_pktmbuf_lastseg(pkt);
		lastseg->next = item_src->firstseg;
		item_src->firstseg = pkt;
		/* update sent_seq to the smaller value */
		item_src->sent_seq = sent_seq;
	}
	item_src->nb_merged++;

	/* update mbuf metadata for the merged packet */
	pkt_head->nb_segs += pkt_tail->nb_segs;
	pkt_head->pkt_len += pkt_tail->pkt_len;

	return 1;
}

static inline int
check_seq_option(struct gro_tcp4_item *item,
		struct tcp_hdr *tcp_hdr,
		uint16_t tcp_hl,
		uint16_t tcp_dl,
		uint16_t ip_id,
		uint32_t sent_seq)
{
	struct rte_mbuf *pkt0 = item->firstseg;
	struct ipv4_hdr *ipv4_hdr0;
	struct tcp_hdr *tcp_hdr0;
	uint16_t tcp_hl0, tcp_dl0;
	uint16_t len;

	ipv4_hdr0 = (struct ipv4_hdr *)(rte_pktmbuf_mtod(pkt0, char *) +
			pkt0->l2_len);
	tcp_hdr0 = (struct tcp_hdr *)((char *)ipv4_hdr0 + pkt0->l3_len);
	tcp_hl0 = pkt0->l4_len;

	/* check if TCP option fields equal. If not, return 0. */
	len = RTE_MAX(tcp_hl, tcp_hl0) - sizeof(struct tcp_hdr);
	if ((tcp_hl != tcp_hl0) ||
			((len > 0) && (memcmp(tcp_hdr + 1,
					tcp_hdr0 + 1,
					len) != 0)))
		return 0;

	/* check if the two packets are neighbors */
	tcp_dl0 = pkt0->pkt_len - pkt0->l2_len - pkt0->l3_len - tcp_hl0;
	if ((sent_seq == (item->sent_seq + tcp_dl0)) &&
			(ip_id == (item->ip_id + 1)))
		/* append the new packet */
		return 1;
	else if (((sent_seq + tcp_dl) == item->sent_seq) &&
			((ip_id + item->nb_merged) == item->ip_id))
		/* pre-pend the new packet */
		return -1;
	else
		return 0;
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
find_an_empty_key(struct gro_tcp4_tbl *tbl)
{
	uint32_t i;
	uint32_t max_key_num = tbl->max_key_num;

	for (i = 0; i < max_key_num; i++)
		if (tbl->keys[i].start_index == INVALID_ARRAY_INDEX)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
insert_new_item(struct gro_tcp4_tbl *tbl,
		struct rte_mbuf *pkt,
		uint16_t ip_id,
		uint32_t sent_seq,
		uint32_t prev_idx,
		uint64_t start_time)
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
	tbl->item_num++;

	/* if the previous packet exists, chain the new one with it */
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

	/* set NULL to firstseg to indicate it's an empty item */
	tbl->items[item_idx].firstseg = NULL;
	tbl->item_num--;
	if (prev_item_idx != INVALID_ARRAY_INDEX)
		tbl->items[prev_item_idx].next_pkt_idx = next_idx;

	return next_idx;
}

static inline uint32_t
insert_new_key(struct gro_tcp4_tbl *tbl,
		struct tcp4_key *key_src,
		uint32_t item_idx)
{
	struct tcp4_key *key_dst;
	uint32_t key_idx;

	key_idx = find_an_empty_key(tbl);
	if (key_idx == INVALID_ARRAY_INDEX)
		return INVALID_ARRAY_INDEX;

	key_dst = &(tbl->keys[key_idx].key);

	ether_addr_copy(&(key_src->eth_saddr), &(key_dst->eth_saddr));
	ether_addr_copy(&(key_src->eth_daddr), &(key_dst->eth_daddr));
	key_dst->ip_src_addr = key_src->ip_src_addr;
	key_dst->ip_dst_addr = key_src->ip_dst_addr;
	key_dst->recv_ack = key_src->recv_ack;
	key_dst->src_port = key_src->src_port;
	key_dst->dst_port = key_src->dst_port;

	/* non-INVALID_ARRAY_INDEX value indicates this key is valid */
	tbl->keys[key_idx].start_index = item_idx;
	tbl->key_num++;

	return key_idx;
}

static inline int
is_same_key(struct tcp4_key k1, struct tcp4_key k2)
{
	if (is_same_ether_addr(&k1.eth_saddr, &k2.eth_saddr) == 0)
		return 0;

	if (is_same_ether_addr(&k1.eth_daddr, &k2.eth_daddr) == 0)
		return 0;

	return ((k1.ip_src_addr == k2.ip_src_addr) &&
			(k1.ip_dst_addr == k2.ip_dst_addr) &&
			(k1.recv_ack == k2.recv_ack) &&
			(k1.src_port == k2.src_port) &&
			(k1.dst_port == k2.dst_port));
}

/*
 * update packet length for the flushed packet.
 */
static inline void
update_header(struct gro_tcp4_item *item)
{
	struct ipv4_hdr *ipv4_hdr;
	struct rte_mbuf *pkt = item->firstseg;

	ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			pkt->l2_len);
	ipv4_hdr->total_length = rte_cpu_to_be_16(pkt->pkt_len -
			pkt->l2_len);
}

int32_t
gro_tcp4_reassemble(struct rte_mbuf *pkt,
		struct gro_tcp4_tbl *tbl,
		uint64_t start_time)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;
	uint32_t sent_seq;
	uint16_t tcp_dl, ip_id;

	struct tcp4_key key;
	uint32_t cur_idx, prev_idx, item_idx;
	uint32_t i, max_key_num;
	int cmp;

	eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	ipv4_hdr = (struct ipv4_hdr *)((char *)eth_hdr + pkt->l2_len);
	tcp_hdr = (struct tcp_hdr *)((char *)ipv4_hdr + pkt->l3_len);

	/*
	 * if FIN, SYN, RST, PSH, URG, ECE or
	 * CWR is set, return immediately.
	 */
	if (tcp_hdr->tcp_flags != TCP_ACK_FLAG)
		return -1;
	/* if payload length is 0, return immediately */
	tcp_dl = rte_be_to_cpu_16(ipv4_hdr->total_length) - pkt->l3_len -
		pkt->l4_len;
	if (tcp_dl == 0)
		return -1;

	ip_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);
	sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);

	ether_addr_copy(&(eth_hdr->s_addr), &(key.eth_saddr));
	ether_addr_copy(&(eth_hdr->d_addr), &(key.eth_daddr));
	key.ip_src_addr = ipv4_hdr->src_addr;
	key.ip_dst_addr = ipv4_hdr->dst_addr;
	key.src_port = tcp_hdr->src_port;
	key.dst_port = tcp_hdr->dst_port;
	key.recv_ack = tcp_hdr->recv_ack;

	/* search for a key */
	max_key_num = tbl->max_key_num;
	for (i = 0; i < max_key_num; i++) {
		if ((tbl->keys[i].start_index != INVALID_ARRAY_INDEX) &&
				is_same_key(tbl->keys[i].key, key))
			break;
	}

	/* can't find a key, so insert a new key and a new item. */
	if (i == tbl->max_key_num) {
		item_idx = insert_new_item(tbl, pkt, ip_id, sent_seq,
				INVALID_ARRAY_INDEX, start_time);
		if (item_idx == INVALID_ARRAY_INDEX)
			return -1;
		if (insert_new_key(tbl, &key, item_idx) ==
				INVALID_ARRAY_INDEX) {
			/*
			 * fail to insert a new key, so
			 * delete the inserted item
			 */
			delete_item(tbl, item_idx, INVALID_ARRAY_INDEX);
			return -1;
		}
		return 0;
	}

	/* traverse all packets in the item group to find one to merge */
	cur_idx = tbl->keys[i].start_index;
	prev_idx = cur_idx;
	do {
		cmp = check_seq_option(&(tbl->items[cur_idx]), tcp_hdr,
				pkt->l4_len, tcp_dl, ip_id, sent_seq);
		if (cmp) {
			if (merge_two_tcp4_packets(&(tbl->items[cur_idx]),
						pkt, ip_id,
						sent_seq, cmp))
				return 1;
			/*
			 * fail to merge two packets since the packet
			 * length will be greater than the max value.
			 * So insert the packet into the item group.
			 */
			if (insert_new_item(tbl, pkt, ip_id, sent_seq,
						prev_idx, start_time) ==
					INVALID_ARRAY_INDEX)
				return -1;
			return 0;
		}
		prev_idx = cur_idx;
		cur_idx = tbl->items[cur_idx].next_pkt_idx;
	} while (cur_idx != INVALID_ARRAY_INDEX);

	/*
	 * can't find a packet in the item group to merge,
	 * so insert the packet into the item group.
	 */
	if (insert_new_item(tbl, pkt, ip_id, sent_seq, prev_idx,
				start_time) == INVALID_ARRAY_INDEX)
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
	uint32_t max_key_num = tbl->max_key_num;

	for (i = 0; i < max_key_num; i++) {
		/* all keys have been checked, return immediately */
		if (tbl->key_num == 0)
			return k;

		j = tbl->keys[i].start_index;
		while (j != INVALID_ARRAY_INDEX) {
			if (tbl->items[j].start_time <= flush_timestamp) {
				out[k++] = tbl->items[j].firstseg;
				if (tbl->items[j].nb_merged > 1)
					update_header(&(tbl->items[j]));
				/*
				 * delete the item and get
				 * the next packet index
				 */
				j = delete_item(tbl, j,
						INVALID_ARRAY_INDEX);

				/*
				 * delete the key as all of
				 * packets are flushed
				 */
				if (j == INVALID_ARRAY_INDEX) {
					tbl->keys[i].start_index =
						INVALID_ARRAY_INDEX;
					tbl->key_num--;
				} else
					/* update start_index of the key */
					tbl->keys[i].start_index = j;

				if (k == nb_out)
					return k;
			} else
				/*
				 * left packets of this key won't be
				 * timeout, so go to check other keys.
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
