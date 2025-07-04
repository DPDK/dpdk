/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Inspur Corporation
 */

#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_udp.h>

#include "gro_vxlan_udp4.h"

void *
gro_vxlan_udp4_tbl_create(uint16_t socket_id,
		uint16_t max_flow_num,
		uint16_t max_item_per_flow)
{
	struct gro_vxlan_udp4_tbl *tbl;
	size_t size;
	uint32_t entries_num, i;

	entries_num = max_flow_num * max_item_per_flow;
	entries_num = RTE_MIN(entries_num, GRO_VXLAN_UDP4_TBL_MAX_ITEM_NUM);

	if (entries_num == 0)
		return NULL;

	tbl = rte_zmalloc_socket(__func__,
			sizeof(struct gro_vxlan_udp4_tbl),
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl == NULL)
		return NULL;

	size = sizeof(struct gro_vxlan_udp4_item) * entries_num;
	tbl->items = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->items == NULL) {
		rte_free(tbl);
		return NULL;
	}
	tbl->max_item_num = entries_num;

	size = sizeof(struct gro_vxlan_udp4_flow) * entries_num;
	tbl->flows = rte_zmalloc_socket(__func__,
			size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (tbl->flows == NULL) {
		rte_free(tbl->items);
		rte_free(tbl);
		return NULL;
	}

	for (i = 0; i < entries_num; i++)
		tbl->flows[i].start_index = INVALID_ARRAY_INDEX;
	tbl->max_flow_num = entries_num;

	return tbl;
}

void
gro_vxlan_udp4_tbl_destroy(void *tbl)
{
	struct gro_vxlan_udp4_tbl *vxlan_tbl = tbl;

	if (vxlan_tbl) {
		rte_free(vxlan_tbl->items);
		rte_free(vxlan_tbl->flows);
	}
	rte_free(vxlan_tbl);
}

static inline uint32_t
find_an_empty_item(struct gro_vxlan_udp4_tbl *tbl)
{
	uint32_t max_item_num = tbl->max_item_num, i;

	for (i = 0; i < max_item_num; i++)
		if (tbl->items[i].inner_item.firstseg == NULL)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
find_an_empty_flow(struct gro_vxlan_udp4_tbl *tbl)
{
	uint32_t max_flow_num = tbl->max_flow_num, i;

	for (i = 0; i < max_flow_num; i++)
		if (tbl->flows[i].start_index == INVALID_ARRAY_INDEX)
			return i;
	return INVALID_ARRAY_INDEX;
}

static inline uint32_t
insert_new_item(struct gro_vxlan_udp4_tbl *tbl,
		struct rte_mbuf *pkt,
		uint64_t start_time,
		uint32_t prev_idx,
		uint16_t frag_offset,
		uint8_t is_last_frag)
{
	uint32_t item_idx;

	item_idx = find_an_empty_item(tbl);
	if (unlikely(item_idx == INVALID_ARRAY_INDEX))
		return INVALID_ARRAY_INDEX;

	tbl->items[item_idx].inner_item.firstseg = pkt;
	tbl->items[item_idx].inner_item.lastseg = rte_pktmbuf_lastseg(pkt);
	tbl->items[item_idx].inner_item.start_time = start_time;
	tbl->items[item_idx].inner_item.next_pkt_idx = INVALID_ARRAY_INDEX;
	tbl->items[item_idx].inner_item.frag_offset = frag_offset;
	tbl->items[item_idx].inner_item.is_last_frag = is_last_frag;
	tbl->items[item_idx].inner_item.nb_merged = 1;
	tbl->item_num++;

	/* If the previous packet exists, chain the new one with it. */
	if (prev_idx != INVALID_ARRAY_INDEX) {
		tbl->items[item_idx].inner_item.next_pkt_idx =
			tbl->items[prev_idx].inner_item.next_pkt_idx;
		tbl->items[prev_idx].inner_item.next_pkt_idx = item_idx;
	}

	return item_idx;
}

static inline uint32_t
delete_item(struct gro_vxlan_udp4_tbl *tbl,
		uint32_t item_idx,
		uint32_t prev_item_idx)
{
	uint32_t next_idx = tbl->items[item_idx].inner_item.next_pkt_idx;

	/* NULL indicates an empty item. */
	tbl->items[item_idx].inner_item.firstseg = NULL;
	tbl->item_num--;
	if (prev_item_idx != INVALID_ARRAY_INDEX)
		tbl->items[prev_item_idx].inner_item.next_pkt_idx = next_idx;

	return next_idx;
}

static inline uint32_t
insert_new_flow(struct gro_vxlan_udp4_tbl *tbl,
		struct vxlan_udp4_flow_key *src,
		uint32_t item_idx)
{
	struct vxlan_udp4_flow_key *dst;
	uint32_t flow_idx;

	flow_idx = find_an_empty_flow(tbl);
	if (unlikely(flow_idx == INVALID_ARRAY_INDEX))
		return INVALID_ARRAY_INDEX;

	dst = &(tbl->flows[flow_idx].key);

	rte_ether_addr_copy(&(src->inner_key.eth_saddr),
			&(dst->inner_key.eth_saddr));
	rte_ether_addr_copy(&(src->inner_key.eth_daddr),
			&(dst->inner_key.eth_daddr));
	dst->inner_key.ip_src_addr = src->inner_key.ip_src_addr;
	dst->inner_key.ip_dst_addr = src->inner_key.ip_dst_addr;
	dst->inner_key.ip_id = src->inner_key.ip_id;

	dst->vxlan_hdr.vx_flags = src->vxlan_hdr.vx_flags;
	dst->vxlan_hdr.vx_vni = src->vxlan_hdr.vx_vni;
	rte_ether_addr_copy(&(src->outer_eth_saddr), &(dst->outer_eth_saddr));
	rte_ether_addr_copy(&(src->outer_eth_daddr), &(dst->outer_eth_daddr));
	dst->outer_ip_src_addr = src->outer_ip_src_addr;
	dst->outer_ip_dst_addr = src->outer_ip_dst_addr;
	dst->outer_dst_port = src->outer_dst_port;

	tbl->flows[flow_idx].start_index = item_idx;
	tbl->flow_num++;

	return flow_idx;
}

static inline int
is_same_vxlan_udp4_flow(struct vxlan_udp4_flow_key k1,
		struct vxlan_udp4_flow_key k2)
{
	/* For VxLAN packet, outer udp src port is calculated from
	 * inner packet RSS hash, udp src port of the first UDP
	 * fragment is different from one of other UDP fragments
	 * even if they are same flow, so we have to skip outer udp
	 * src port comparison here.
	 */
	return (rte_is_same_ether_addr(&k1.outer_eth_saddr,
					&k2.outer_eth_saddr) &&
			rte_is_same_ether_addr(&k1.outer_eth_daddr,
				&k2.outer_eth_daddr) &&
			(k1.outer_ip_src_addr == k2.outer_ip_src_addr) &&
			(k1.outer_ip_dst_addr == k2.outer_ip_dst_addr) &&
			(k1.outer_dst_port == k2.outer_dst_port) &&
			(k1.vxlan_hdr.vx_flags == k2.vxlan_hdr.vx_flags) &&
			(k1.vxlan_hdr.vx_vni == k2.vxlan_hdr.vx_vni) &&
			is_same_udp4_flow(k1.inner_key, k2.inner_key));
}

static inline int
udp4_check_vxlan_neighbor(struct gro_vxlan_udp4_item *item,
		uint16_t frag_offset,
		uint16_t ip_dl)
{
	struct rte_mbuf *pkt = item->inner_item.firstseg;
	int cmp;
	uint16_t l2_offset;
	int ret = 0;

	/* Note: if outer DF bit is set, i.e outer_is_atomic is 0,
	 * we needn't compare outer_ip_id because they are same,
	 * for the case outer_is_atomic is 1, we also have no way
	 * to compare outer_ip_id because the difference between
	 * outer_ip_ids of two received packets isn't always +/-1.
	 * So skip outer_ip_id comparison here.
	 */

	l2_offset = pkt->outer_l2_len + pkt->outer_l3_len;
	cmp = udp4_check_neighbor(&item->inner_item, frag_offset, ip_dl,
					l2_offset);
	if (cmp > 0)
		/* Append the new packet. */
		ret = 1;
	else if (cmp < 0)
		/* Prepend the new packet. */
		ret = -1;

	return ret;
}

static inline int
merge_two_vxlan_udp4_packets(struct gro_vxlan_udp4_item *item,
		struct rte_mbuf *pkt,
		int cmp,
		uint16_t frag_offset,
		uint8_t is_last_frag)
{
	if (merge_two_udp4_packets(&item->inner_item, pkt, cmp, frag_offset,
				is_last_frag,
				pkt->outer_l2_len + pkt->outer_l3_len)) {
		return 1;
	}

	return 0;
}

static inline void
update_vxlan_header(struct gro_vxlan_udp4_item *item)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_udp_hdr *udp_hdr;
	struct rte_mbuf *pkt = item->inner_item.firstseg;
	uint16_t len;
	uint16_t frag_offset;

	/* Update the outer IPv4 header. */
	len = pkt->pkt_len - pkt->outer_l2_len;
	ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
					   pkt->outer_l2_len);
	ipv4_hdr->total_length = rte_cpu_to_be_16(len);

	/* Update the outer UDP header. */
	len -= pkt->outer_l3_len;
	udp_hdr = (struct rte_udp_hdr *)((char *)ipv4_hdr + pkt->outer_l3_len);
	udp_hdr->dgram_len = rte_cpu_to_be_16(len);

	/* Update the inner IPv4 header. */
	len -= pkt->l2_len;
	ipv4_hdr = (struct rte_ipv4_hdr *)((char *)udp_hdr + pkt->l2_len);
	ipv4_hdr->total_length = rte_cpu_to_be_16(len);

	/* Clear MF bit if it is last fragment */
	if (item->inner_item.is_last_frag) {
		frag_offset = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
		ipv4_hdr->fragment_offset =
			rte_cpu_to_be_16(frag_offset & ~RTE_IPV4_HDR_MF_FLAG);
	}
}

int32_t
gro_vxlan_udp4_reassemble(struct rte_mbuf *pkt,
		struct gro_vxlan_udp4_tbl *tbl,
		uint64_t start_time)
{
	struct rte_ether_hdr *outer_eth_hdr, *eth_hdr;
	struct rte_ipv4_hdr *outer_ipv4_hdr, *ipv4_hdr;
	struct rte_udp_hdr *udp_hdr;
	struct rte_vxlan_hdr *vxlan_hdr;
	uint16_t frag_offset;
	uint8_t is_last_frag;
	int16_t ip_dl;
	uint16_t ip_id;

	struct vxlan_udp4_flow_key key;
	uint32_t cur_idx, prev_idx, item_idx;
	uint32_t i, max_flow_num, remaining_flow_num;
	int cmp;
	uint16_t hdr_len;
	uint8_t find;

	outer_eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	outer_ipv4_hdr = (struct rte_ipv4_hdr *)((char *)outer_eth_hdr +
			pkt->outer_l2_len);

	udp_hdr = (struct rte_udp_hdr *)((char *)outer_ipv4_hdr +
			pkt->outer_l3_len);
	vxlan_hdr = (struct rte_vxlan_hdr *)((char *)udp_hdr +
			sizeof(struct rte_udp_hdr));
	eth_hdr = (struct rte_ether_hdr *)((char *)vxlan_hdr +
			sizeof(struct rte_vxlan_hdr));
	/* l2_len = outer udp hdr len + vxlan hdr len + inner l2 len */
	ipv4_hdr = (struct rte_ipv4_hdr *)((char *)udp_hdr + pkt->l2_len);

	/*
	 * Don't process the packet which has non-fragment inner IP.
	 */
	if (!is_ipv4_fragment(ipv4_hdr))
		return -1;

	hdr_len = pkt->outer_l2_len + pkt->outer_l3_len + pkt->l2_len +
		pkt->l3_len;
	/*
	 * Don't process the packet whose payload length is less than or
	 * equal to 0.
	 */
	if (pkt->pkt_len <= hdr_len)
		return -1;

	ip_dl = pkt->pkt_len - hdr_len;

	ip_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);
	frag_offset = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
	is_last_frag = ((frag_offset & RTE_IPV4_HDR_MF_FLAG) == 0) ? 1 : 0;
	frag_offset = (uint16_t)(frag_offset & RTE_IPV4_HDR_OFFSET_MASK) << 3;

	rte_ether_addr_copy(&(eth_hdr->src_addr), &(key.inner_key.eth_saddr));
	rte_ether_addr_copy(&(eth_hdr->dst_addr), &(key.inner_key.eth_daddr));
	key.inner_key.ip_src_addr = ipv4_hdr->src_addr;
	key.inner_key.ip_dst_addr = ipv4_hdr->dst_addr;
	key.inner_key.ip_id = ip_id;

	key.vxlan_hdr.vx_flags = vxlan_hdr->vx_flags;
	key.vxlan_hdr.vx_vni = vxlan_hdr->vx_vni;
	rte_ether_addr_copy(&(outer_eth_hdr->src_addr), &(key.outer_eth_saddr));
	rte_ether_addr_copy(&(outer_eth_hdr->dst_addr), &(key.outer_eth_daddr));
	key.outer_ip_src_addr = outer_ipv4_hdr->src_addr;
	key.outer_ip_dst_addr = outer_ipv4_hdr->dst_addr;
	/* Note: It is unnecessary to save outer_src_port here because it can
	 * be different for VxLAN UDP fragments from the same flow.
	 */
	key.outer_dst_port = udp_hdr->dst_port;

	/* Search for a matched flow. */
	max_flow_num = tbl->max_flow_num;
	remaining_flow_num = tbl->flow_num;
	find = 0;
	for (i = 0; i < max_flow_num && remaining_flow_num; i++) {
		if (tbl->flows[i].start_index != INVALID_ARRAY_INDEX) {
			if (is_same_vxlan_udp4_flow(tbl->flows[i].key, key)) {
				find = 1;
				break;
			}
			remaining_flow_num--;
		}
	}

	/*
	 * Can't find a matched flow. Insert a new flow and store the
	 * packet into the flow.
	 */
	if (find == 0) {
		item_idx = insert_new_item(tbl, pkt, start_time,
				INVALID_ARRAY_INDEX, frag_offset,
				is_last_frag);
		if (unlikely(item_idx == INVALID_ARRAY_INDEX))
			return -1;
		if (insert_new_flow(tbl, &key, item_idx) ==
				INVALID_ARRAY_INDEX) {
			/*
			 * Fail to insert a new flow, so
			 * delete the inserted packet.
			 */
			delete_item(tbl, item_idx, INVALID_ARRAY_INDEX);
			return -1;
		}
		return 0;
	}

	/* Check all packets in the flow and try to find a neighbor. */
	cur_idx = tbl->flows[i].start_index;
	prev_idx = cur_idx;
	do {
		cmp = udp4_check_vxlan_neighbor(&(tbl->items[cur_idx]),
				frag_offset, ip_dl);
		if (cmp) {
			if (merge_two_vxlan_udp4_packets(
						&(tbl->items[cur_idx]),
						pkt, cmp, frag_offset,
						is_last_frag)) {
				return 1;
			}
			/*
			 * Can't merge two packets, as the packet
			 * length will be greater than the max value.
			 * Insert the packet into the flow.
			 */
			if (insert_new_item(tbl, pkt, start_time, prev_idx,
						frag_offset, is_last_frag) ==
					INVALID_ARRAY_INDEX)
				return -1;
			return 0;
		}

		/* Ensure inserted items are ordered by frag_offset */
		if (frag_offset
			< tbl->items[cur_idx].inner_item.frag_offset) {
			break;
		}

		prev_idx = cur_idx;
		cur_idx = tbl->items[cur_idx].inner_item.next_pkt_idx;
	} while (cur_idx != INVALID_ARRAY_INDEX);

	/* Can't find neighbor. Insert the packet into the flow. */
	if (cur_idx == tbl->flows[i].start_index) {
		/* Insert it before the first packet of the flow */
		item_idx = insert_new_item(tbl, pkt, start_time,
				INVALID_ARRAY_INDEX, frag_offset,
				is_last_frag);
		if (unlikely(item_idx == INVALID_ARRAY_INDEX))
			return -1;
		tbl->items[item_idx].inner_item.next_pkt_idx = cur_idx;
		tbl->flows[i].start_index = item_idx;
	} else {
		if (insert_new_item(tbl, pkt, start_time, prev_idx,
					frag_offset, is_last_frag
					) == INVALID_ARRAY_INDEX)
			return -1;
	}

	return 0;
}

static int
gro_vxlan_udp4_merge_items(struct gro_vxlan_udp4_tbl *tbl,
			   uint32_t start_idx)
{
	uint16_t frag_offset;
	uint8_t is_last_frag;
	int16_t ip_dl;
	struct rte_mbuf *pkt;
	int cmp;
	uint32_t item_idx;
	uint16_t hdr_len;

	item_idx = tbl->items[start_idx].inner_item.next_pkt_idx;
	while (item_idx != INVALID_ARRAY_INDEX) {
		pkt = tbl->items[item_idx].inner_item.firstseg;
		hdr_len = pkt->outer_l2_len + pkt->outer_l3_len + pkt->l2_len +
			pkt->l3_len;
		ip_dl = pkt->pkt_len - hdr_len;
		frag_offset = tbl->items[item_idx].inner_item.frag_offset;
		is_last_frag = tbl->items[item_idx].inner_item.is_last_frag;
		cmp = udp4_check_vxlan_neighbor(&(tbl->items[start_idx]),
					frag_offset, ip_dl);
		if (cmp) {
			if (merge_two_vxlan_udp4_packets(
					&(tbl->items[start_idx]),
					pkt, cmp, frag_offset,
					is_last_frag)) {
				item_idx = delete_item(tbl, item_idx,
							INVALID_ARRAY_INDEX);
				tbl->items[start_idx].inner_item.next_pkt_idx
					= item_idx;
			} else
				return 0;
		} else
			return 0;
	}

	return 0;
}

uint16_t
gro_vxlan_udp4_tbl_timeout_flush(struct gro_vxlan_udp4_tbl *tbl,
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
			if (tbl->items[j].inner_item.start_time <=
					flush_timestamp) {
				gro_vxlan_udp4_merge_items(tbl, j);
				out[k++] = tbl->items[j].inner_item.firstseg;
				if (tbl->items[j].inner_item.nb_merged > 1)
					update_vxlan_header(&(tbl->items[j]));
				/*
				 * Delete the item and get the next packet
				 * index.
				 */
				j = delete_item(tbl, j, INVALID_ARRAY_INDEX);
				tbl->flows[i].start_index = j;
				if (j == INVALID_ARRAY_INDEX)
					tbl->flow_num--;

				if (unlikely(k == nb_out))
					return k;
			} else
				/*
				 * Flushing packets does not strictly follow
				 * timestamp. It does not flush left packets of
				 * the flow this time once it finds one item
				 * whose start_time is greater than
				 * flush_timestamp. So go to check other flows.
				 */
				break;
		}
	}
	return k;
}

uint32_t
gro_vxlan_udp4_tbl_pkt_count(void *tbl)
{
	struct gro_vxlan_udp4_tbl *gro_tbl = tbl;

	if (gro_tbl)
		return gro_tbl->item_num;

	return 0;
}
