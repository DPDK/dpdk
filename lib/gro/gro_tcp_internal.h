
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _GRO_TCP_INTERNAL_H_
#define _GRO_TCP_INTERNAL_H_

static inline uint32_t
find_an_empty_item(struct gro_tcp_item *items,
	uint32_t max_item_num)
{
	uint32_t i;

	for (i = 0; i < max_item_num; i++)
		if (items[i].firstseg == NULL)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
insert_new_tcp_item(struct rte_mbuf *pkt,
		struct gro_tcp_item *items,
		uint32_t *item_num,
		uint32_t max_item_num,
		uint64_t start_time,
		uint32_t prev_idx,
		uint32_t sent_seq,
		uint16_t ip_id,
		uint8_t is_atomic)
{
	uint32_t item_idx;

	item_idx = find_an_empty_item(items, max_item_num);
	if (item_idx == INVALID_ARRAY_INDEX)
		return INVALID_ARRAY_INDEX;

	items[item_idx].firstseg = pkt;
	items[item_idx].lastseg = rte_pktmbuf_lastseg(pkt);
	items[item_idx].start_time = start_time;
	items[item_idx].next_pkt_idx = INVALID_ARRAY_INDEX;
	items[item_idx].sent_seq = sent_seq;
	items[item_idx].l3.ip_id = ip_id;
	items[item_idx].nb_merged = 1;
	items[item_idx].is_atomic = is_atomic;
	(*item_num) += 1;

	/* if the previous packet exists, chain them together. */
	if (prev_idx != INVALID_ARRAY_INDEX) {
		items[item_idx].next_pkt_idx =
			items[prev_idx].next_pkt_idx;
		items[prev_idx].next_pkt_idx = item_idx;
	}

	return item_idx;
}

static inline uint32_t
delete_tcp_item(struct gro_tcp_item *items, uint32_t item_idx,
		uint32_t *item_num,
		uint32_t prev_item_idx)
{
	uint32_t next_idx = items[item_idx].next_pkt_idx;

	/* NULL indicates an empty item */
	items[item_idx].firstseg = NULL;
	(*item_num) -= 1;
	if (prev_item_idx != INVALID_ARRAY_INDEX)
		items[prev_item_idx].next_pkt_idx = next_idx;

	return next_idx;
}

static inline int32_t
process_tcp_item(struct rte_mbuf *pkt,
	struct rte_tcp_hdr *tcp_hdr,
	int32_t tcp_dl,
	struct gro_tcp_item *items,
	uint32_t item_idx,
	uint32_t *item_num,
	uint32_t max_item_num,
	uint16_t ip_id,
	uint8_t is_atomic,
	uint64_t start_time)
{
	uint32_t cur_idx;
	uint32_t prev_idx;
	int cmp;
	uint32_t sent_seq;

	sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
	/*
	 * Check all packets in the flow and try to find a neighbor for
	 * the input packet.
	 */
	cur_idx = item_idx;
	prev_idx = cur_idx;
	do {
		cmp = check_seq_option(&items[cur_idx], tcp_hdr,
				sent_seq, ip_id, pkt->l4_len, tcp_dl, 0,
				is_atomic);
		if (cmp) {
			if (merge_two_tcp_packets(&items[cur_idx],
						pkt, cmp, sent_seq, ip_id, 0))
				return 1;
			/*
			 * Fail to merge the two packets, as the packet
			 * length is greater than the max value. Store
			 * the packet into the flow.
			 */
			if (insert_new_tcp_item(pkt, items, item_num, max_item_num,
						start_time, cur_idx, sent_seq, ip_id, is_atomic) ==
					INVALID_ARRAY_INDEX)
				return -1;
			return 0;
		}
		prev_idx = cur_idx;
		cur_idx = items[cur_idx].next_pkt_idx;
	} while (cur_idx != INVALID_ARRAY_INDEX);

	/* Fail to find a neighbor, so store the packet into the flow. */
	if (insert_new_tcp_item(pkt, items, item_num, max_item_num, start_time, prev_idx, sent_seq,
				ip_id, is_atomic) == INVALID_ARRAY_INDEX)
		return -1;

	return 0;
}
#endif
