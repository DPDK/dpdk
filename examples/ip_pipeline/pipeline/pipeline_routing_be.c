/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_table_lpm.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "pipeline_routing_be.h"
#include "pipeline_actions_common.h"
#include "parser.h"
#include "hash_func.h"

#define MPLS_LABEL(label, exp, s, ttl)					\
	(((((uint64_t) (label)) & 0xFFFFFLLU) << 12) |		\
	((((uint64_t) (exp)) & 0x7LLU) << 9) |				\
	((((uint64_t) (s)) & 0x1LLU) << 8) |				\
	(((uint64_t) (ttl)) & 0xFFLU))

#define RTE_SCHED_PORT_HIERARCHY(subport, pipe,		\
	traffic_class, queue, color)				\
	((((uint64_t) (queue)) & 0x3) |                \
	((((uint64_t) (traffic_class)) & 0x3) << 2) |  \
	((((uint64_t) (color)) & 0x3) << 4) |          \
	((((uint64_t) (subport)) & 0xFFFF) << 16) |    \
	((((uint64_t) (pipe)) & 0xFFFFFFFF) << 32))


/* Network Byte Order (NBO) */
#define SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr, ethertype)	\
	(((uint64_t) macaddr) | (((uint64_t) rte_cpu_to_be_16(ethertype)) << 48))

#ifndef PIPELINE_ROUTING_LPM_TABLE_NUMBER_TABLE8s
#define PIPELINE_ROUTING_LPM_TABLE_NUMBER_TABLE8s 256
#endif

struct pipeline_routing {
	struct pipeline p;
	struct pipeline_routing_params params;
	pipeline_msg_req_handler custom_handlers[PIPELINE_ROUTING_MSG_REQS];
	uint64_t macaddr[PIPELINE_MAX_PORT_OUT];
} __rte_cache_aligned;

/*
 * Message handlers
 */
static void *
pipeline_routing_msg_req_custom_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler handlers[] = {
	[PIPELINE_MSG_REQ_PING] =
		pipeline_msg_req_ping_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_IN] =
		pipeline_msg_req_stats_port_in_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_OUT] =
		pipeline_msg_req_stats_port_out_handler,
	[PIPELINE_MSG_REQ_STATS_TABLE] =
		pipeline_msg_req_stats_table_handler,
	[PIPELINE_MSG_REQ_PORT_IN_ENABLE] =
		pipeline_msg_req_port_in_enable_handler,
	[PIPELINE_MSG_REQ_PORT_IN_DISABLE] =
		pipeline_msg_req_port_in_disable_handler,
	[PIPELINE_MSG_REQ_CUSTOM] =
		pipeline_routing_msg_req_custom_handler,
};

static void *
pipeline_routing_msg_req_route_add_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_del_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_add_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_del_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_add_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_del_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_add_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_del_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_set_macaddr_handler(struct pipeline *p,
	void *msg);

static pipeline_msg_req_handler custom_handlers[] = {
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD] =
		pipeline_routing_msg_req_route_add_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL] =
		pipeline_routing_msg_req_route_del_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD_DEFAULT] =
		pipeline_routing_msg_req_route_add_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL_DEFAULT] =
		pipeline_routing_msg_req_route_del_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_ADD] =
		pipeline_routing_msg_req_arp_add_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_DEL] =
		pipeline_routing_msg_req_arp_del_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_ADD_DEFAULT] =
		pipeline_routing_msg_req_arp_add_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_DEL_DEFAULT] =
		pipeline_routing_msg_req_arp_del_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_SET_MACADDR] =
		pipeline_routing_msg_req_set_macaddr_handler,
};

/*
 * Routing table
 */
struct routing_table_entry {
	struct rte_pipeline_table_entry head;
	uint32_t flags;
	uint32_t port_id; /* Output port ID */
	uint32_t ip; /* Next hop IP address (only valid for remote routes) */

	/* ether_l2 */
	uint16_t data_offset;
	uint16_t ether_l2_length;
	uint64_t slab[4];
	uint16_t slab_offset[4];
};

struct layout {
	uint16_t a;
	uint32_t b;
	uint16_t c;
} __attribute__((__packed__));

#define MACADDR_DST_WRITE(slab_ptr, slab)			\
{								\
	struct layout *dst = (struct layout *) (slab_ptr);	\
	struct layout *src = (struct layout *) &(slab);		\
								\
	dst->b = src->b;					\
	dst->c = src->c;					\
}

static __rte_always_inline void
pkt_work_routing(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	void *arg,
	int arp,
	int qinq,
	int qinq_sched,
	int mpls,
	int mpls_color_mark)
{
	struct pipeline_routing *p_rt = arg;

	struct routing_table_entry *entry =
		(struct routing_table_entry *) table_entry;

	struct ipv4_hdr *ip = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT8_PTR(pkt, p_rt->params.ip_hdr_offset);

	enum rte_meter_color pkt_color = (enum rte_meter_color)
		RTE_MBUF_METADATA_UINT32(pkt, p_rt->params.color_offset);

	struct pipeline_routing_arp_key_ipv4 *arp_key =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkt, p_rt->params.arp_key_offset);

	uint64_t *slab0_ptr, *slab1_ptr, *slab2_ptr, *slab3_ptr, sched;
	uint32_t ip_da, nh_ip, port_id;
	uint16_t total_length, data_offset, ether_l2_length;

	/* Read */
	total_length = rte_bswap16(ip->total_length);
	ip_da = ip->dst_addr;
	data_offset = entry->data_offset;
	ether_l2_length = entry->ether_l2_length;
	slab0_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt, entry->slab_offset[0]);
	slab1_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt, entry->slab_offset[1]);
	slab2_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt, entry->slab_offset[2]);
	slab3_ptr = RTE_MBUF_METADATA_UINT64_PTR(pkt, entry->slab_offset[3]);

	if (arp) {
		port_id = entry->port_id;
		nh_ip = entry->ip;
		if (entry->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			nh_ip = ip_da;
	}

	/* Compute */
	total_length += ether_l2_length;

	if (qinq && qinq_sched) {
		uint32_t dscp = ip->type_of_service >> 2;
		uint32_t svlan, cvlan, tc, tc_q;

		if (qinq_sched == 1) {
			uint64_t slab_qinq = rte_bswap64(entry->slab[0]);

			svlan = (slab_qinq >> 48) & 0xFFF;
			cvlan = (slab_qinq >> 16) & 0xFFF;
			tc = (dscp >> 2) & 0x3;
			tc_q = dscp & 0x3;
		} else {
			uint32_t ip_src = rte_bswap32(ip->src_addr);

			svlan = 0;
			cvlan = (ip_src >> 16) & 0xFFF;
			tc = (ip_src >> 2) & 0x3;
			tc_q = ip_src & 0x3;
		}
		sched = RTE_SCHED_PORT_HIERARCHY(svlan,
			cvlan,
			tc,
			tc_q,
			e_RTE_METER_GREEN);
	}

	/* Write */
	pkt->data_off = data_offset;
	pkt->data_len = total_length;
	pkt->pkt_len = total_length;

	if ((qinq == 0) && (mpls == 0)) {
		*slab0_ptr = entry->slab[0];

		if (arp == 0)
			MACADDR_DST_WRITE(slab1_ptr, entry->slab[1]);
	}

	if (qinq) {
		*slab0_ptr = entry->slab[0];
		*slab1_ptr = entry->slab[1];

		if (arp == 0)
			MACADDR_DST_WRITE(slab2_ptr, entry->slab[2]);

		if (qinq_sched) {
			pkt->hash.sched.lo = sched & 0xFFFFFFFF;
			pkt->hash.sched.hi = sched >> 32;
		}
	}

	if (mpls) {
		if (mpls_color_mark) {
			uint64_t mpls_exp = rte_bswap64(
				(MPLS_LABEL(0, pkt_color, 0, 0) << 32) |
				MPLS_LABEL(0, pkt_color, 0, 0));

			*slab0_ptr = entry->slab[0] | mpls_exp;
			*slab1_ptr = entry->slab[1] | mpls_exp;
			*slab2_ptr = entry->slab[2];
		} else {
			*slab0_ptr = entry->slab[0];
			*slab1_ptr = entry->slab[1];
			*slab2_ptr = entry->slab[2];
		}

		if (arp == 0)
			MACADDR_DST_WRITE(slab3_ptr, entry->slab[3]);
	}

	if (arp) {
		arp_key->port_id = port_id;
		arp_key->ip = nh_ip;
	}
}

static __rte_always_inline void
pkt4_work_routing(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	void *arg,
	int arp,
	int qinq,
	int qinq_sched,
	int mpls,
	int mpls_color_mark)
{
	struct pipeline_routing *p_rt = arg;

	struct routing_table_entry *entry0 =
		(struct routing_table_entry *) table_entries[0];
	struct routing_table_entry *entry1 =
		(struct routing_table_entry *) table_entries[1];
	struct routing_table_entry *entry2 =
		(struct routing_table_entry *) table_entries[2];
	struct routing_table_entry *entry3 =
		(struct routing_table_entry *) table_entries[3];

	struct ipv4_hdr *ip0 = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[0],
			p_rt->params.ip_hdr_offset);
	struct ipv4_hdr *ip1 = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[1],
			p_rt->params.ip_hdr_offset);
	struct ipv4_hdr *ip2 = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[2],
			p_rt->params.ip_hdr_offset);
	struct ipv4_hdr *ip3 = (struct ipv4_hdr *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[3],
			p_rt->params.ip_hdr_offset);

	enum rte_meter_color pkt0_color = (enum rte_meter_color)
		RTE_MBUF_METADATA_UINT32(pkts[0], p_rt->params.color_offset);
	enum rte_meter_color pkt1_color = (enum rte_meter_color)
		RTE_MBUF_METADATA_UINT32(pkts[1], p_rt->params.color_offset);
	enum rte_meter_color pkt2_color = (enum rte_meter_color)
		RTE_MBUF_METADATA_UINT32(pkts[2], p_rt->params.color_offset);
	enum rte_meter_color pkt3_color = (enum rte_meter_color)
		RTE_MBUF_METADATA_UINT32(pkts[3], p_rt->params.color_offset);

	struct pipeline_routing_arp_key_ipv4 *arp_key0 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[0],
			p_rt->params.arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key1 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[1],
			p_rt->params.arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key2 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[2],
			p_rt->params.arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key3 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[3],
			p_rt->params.arp_key_offset);

	uint64_t *slab0_ptr0, *slab1_ptr0, *slab2_ptr0, *slab3_ptr0;
	uint64_t *slab0_ptr1, *slab1_ptr1, *slab2_ptr1, *slab3_ptr1;
	uint64_t *slab0_ptr2, *slab1_ptr2, *slab2_ptr2, *slab3_ptr2;
	uint64_t *slab0_ptr3, *slab1_ptr3, *slab2_ptr3, *slab3_ptr3;
	uint64_t sched0, sched1, sched2, sched3;

	uint32_t ip_da0, nh_ip0, port_id0;
	uint32_t ip_da1, nh_ip1, port_id1;
	uint32_t ip_da2, nh_ip2, port_id2;
	uint32_t ip_da3, nh_ip3, port_id3;

	uint16_t total_length0, data_offset0, ether_l2_length0;
	uint16_t total_length1, data_offset1, ether_l2_length1;
	uint16_t total_length2, data_offset2, ether_l2_length2;
	uint16_t total_length3, data_offset3, ether_l2_length3;

	/* Read */
	total_length0 = rte_bswap16(ip0->total_length);
	total_length1 = rte_bswap16(ip1->total_length);
	total_length2 = rte_bswap16(ip2->total_length);
	total_length3 = rte_bswap16(ip3->total_length);

	ip_da0 = ip0->dst_addr;
	ip_da1 = ip1->dst_addr;
	ip_da2 = ip2->dst_addr;
	ip_da3 = ip3->dst_addr;

	data_offset0 = entry0->data_offset;
	data_offset1 = entry1->data_offset;
	data_offset2 = entry2->data_offset;
	data_offset3 = entry3->data_offset;

	ether_l2_length0 = entry0->ether_l2_length;
	ether_l2_length1 = entry1->ether_l2_length;
	ether_l2_length2 = entry2->ether_l2_length;
	ether_l2_length3 = entry3->ether_l2_length;

	slab0_ptr0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		entry0->slab_offset[0]);
	slab1_ptr0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		entry0->slab_offset[1]);
	slab2_ptr0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		entry0->slab_offset[2]);
	slab3_ptr0 = RTE_MBUF_METADATA_UINT64_PTR(pkts[0],
		entry0->slab_offset[3]);

	slab0_ptr1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		entry1->slab_offset[0]);
	slab1_ptr1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		entry1->slab_offset[1]);
	slab2_ptr1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		entry1->slab_offset[2]);
	slab3_ptr1 = RTE_MBUF_METADATA_UINT64_PTR(pkts[1],
		entry1->slab_offset[3]);

	slab0_ptr2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		entry2->slab_offset[0]);
	slab1_ptr2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		entry2->slab_offset[1]);
	slab2_ptr2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		entry2->slab_offset[2]);
	slab3_ptr2 = RTE_MBUF_METADATA_UINT64_PTR(pkts[2],
		entry2->slab_offset[3]);

	slab0_ptr3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		entry3->slab_offset[0]);
	slab1_ptr3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		entry3->slab_offset[1]);
	slab2_ptr3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		entry3->slab_offset[2]);
	slab3_ptr3 = RTE_MBUF_METADATA_UINT64_PTR(pkts[3],
		entry3->slab_offset[3]);

	if (arp) {
		port_id0 = entry0->port_id;
		nh_ip0 = entry0->ip;
		if (entry0->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			nh_ip0 = ip_da0;

		port_id1 = entry1->port_id;
		nh_ip1 = entry1->ip;
		if (entry1->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			nh_ip1 = ip_da1;

		port_id2 = entry2->port_id;
		nh_ip2 = entry2->ip;
		if (entry2->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			nh_ip2 = ip_da2;

		port_id3 = entry3->port_id;
		nh_ip3 = entry3->ip;
		if (entry3->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			nh_ip3 = ip_da3;
	}

	/* Compute */
	total_length0 += ether_l2_length0;
	total_length1 += ether_l2_length1;
	total_length2 += ether_l2_length2;
	total_length3 += ether_l2_length3;

	if (qinq && qinq_sched) {
		uint32_t dscp0 = ip0->type_of_service >> 2;
		uint32_t dscp1 = ip1->type_of_service >> 2;
		uint32_t dscp2 = ip2->type_of_service >> 2;
		uint32_t dscp3 = ip3->type_of_service >> 2;
		uint32_t svlan0, cvlan0, tc0, tc_q0;
		uint32_t svlan1, cvlan1, tc1, tc_q1;
		uint32_t svlan2, cvlan2, tc2, tc_q2;
		uint32_t svlan3, cvlan3, tc3, tc_q3;

		if (qinq_sched == 1) {
			uint64_t slab_qinq0 = rte_bswap64(entry0->slab[0]);
			uint64_t slab_qinq1 = rte_bswap64(entry1->slab[0]);
			uint64_t slab_qinq2 = rte_bswap64(entry2->slab[0]);
			uint64_t slab_qinq3 = rte_bswap64(entry3->slab[0]);

			svlan0 = (slab_qinq0 >> 48) & 0xFFF;
			svlan1 = (slab_qinq1 >> 48) & 0xFFF;
			svlan2 = (slab_qinq2 >> 48) & 0xFFF;
			svlan3 = (slab_qinq3 >> 48) & 0xFFF;

			cvlan0 = (slab_qinq0 >> 16) & 0xFFF;
			cvlan1 = (slab_qinq1 >> 16) & 0xFFF;
			cvlan2 = (slab_qinq2 >> 16) & 0xFFF;
			cvlan3 = (slab_qinq3 >> 16) & 0xFFF;

			tc0 = (dscp0 >> 2) & 0x3;
			tc1 = (dscp1 >> 2) & 0x3;
			tc2 = (dscp2 >> 2) & 0x3;
			tc3 = (dscp3 >> 2) & 0x3;

			tc_q0 = dscp0 & 0x3;
			tc_q1 = dscp1 & 0x3;
			tc_q2 = dscp2 & 0x3;
			tc_q3 = dscp3 & 0x3;
		} else {
			uint32_t ip_src0 = rte_bswap32(ip0->src_addr);
			uint32_t ip_src1 = rte_bswap32(ip1->src_addr);
			uint32_t ip_src2 = rte_bswap32(ip2->src_addr);
			uint32_t ip_src3 = rte_bswap32(ip3->src_addr);

			svlan0 = 0;
			svlan1 = 0;
			svlan2 = 0;
			svlan3 = 0;

			cvlan0 = (ip_src0 >> 16) & 0xFFF;
			cvlan1 = (ip_src1 >> 16) & 0xFFF;
			cvlan2 = (ip_src2 >> 16) & 0xFFF;
			cvlan3 = (ip_src3 >> 16) & 0xFFF;

			tc0 = (ip_src0 >> 2) & 0x3;
			tc1 = (ip_src1 >> 2) & 0x3;
			tc2 = (ip_src2 >> 2) & 0x3;
			tc3 = (ip_src3 >> 2) & 0x3;

			tc_q0 = ip_src0 & 0x3;
			tc_q1 = ip_src1 & 0x3;
			tc_q2 = ip_src2 & 0x3;
			tc_q3 = ip_src3 & 0x3;
		}

		sched0 = RTE_SCHED_PORT_HIERARCHY(svlan0,
			cvlan0,
			tc0,
			tc_q0,
			e_RTE_METER_GREEN);
		sched1 = RTE_SCHED_PORT_HIERARCHY(svlan1,
			cvlan1,
			tc1,
			tc_q1,
			e_RTE_METER_GREEN);
		sched2 = RTE_SCHED_PORT_HIERARCHY(svlan2,
			cvlan2,
			tc2,
			tc_q2,
			e_RTE_METER_GREEN);
		sched3 = RTE_SCHED_PORT_HIERARCHY(svlan3,
			cvlan3,
			tc3,
			tc_q3,
			e_RTE_METER_GREEN);

	}

	/* Write */
	pkts[0]->data_off = data_offset0;
	pkts[1]->data_off = data_offset1;
	pkts[2]->data_off = data_offset2;
	pkts[3]->data_off = data_offset3;

	pkts[0]->data_len = total_length0;
	pkts[1]->data_len = total_length1;
	pkts[2]->data_len = total_length2;
	pkts[3]->data_len = total_length3;

	pkts[0]->pkt_len = total_length0;
	pkts[1]->pkt_len = total_length1;
	pkts[2]->pkt_len = total_length2;
	pkts[3]->pkt_len = total_length3;

	if ((qinq == 0) && (mpls == 0)) {
		*slab0_ptr0 = entry0->slab[0];
		*slab0_ptr1 = entry1->slab[0];
		*slab0_ptr2 = entry2->slab[0];
		*slab0_ptr3 = entry3->slab[0];

		if (arp == 0) {
			MACADDR_DST_WRITE(slab1_ptr0, entry0->slab[1]);
			MACADDR_DST_WRITE(slab1_ptr1, entry1->slab[1]);
			MACADDR_DST_WRITE(slab1_ptr2, entry2->slab[1]);
			MACADDR_DST_WRITE(slab1_ptr3, entry3->slab[1]);
		}
	}

	if (qinq) {
		*slab0_ptr0 = entry0->slab[0];
		*slab0_ptr1 = entry1->slab[0];
		*slab0_ptr2 = entry2->slab[0];
		*slab0_ptr3 = entry3->slab[0];

		*slab1_ptr0 = entry0->slab[1];
		*slab1_ptr1 = entry1->slab[1];
		*slab1_ptr2 = entry2->slab[1];
		*slab1_ptr3 = entry3->slab[1];

		if (arp == 0) {
			MACADDR_DST_WRITE(slab2_ptr0, entry0->slab[2]);
			MACADDR_DST_WRITE(slab2_ptr1, entry1->slab[2]);
			MACADDR_DST_WRITE(slab2_ptr2, entry2->slab[2]);
			MACADDR_DST_WRITE(slab2_ptr3, entry3->slab[2]);
		}

		if (qinq_sched) {
			pkts[0]->hash.sched.lo = sched0 & 0xFFFFFFFF;
			pkts[0]->hash.sched.hi = sched0 >> 32;
			pkts[1]->hash.sched.lo = sched1 & 0xFFFFFFFF;
			pkts[1]->hash.sched.hi = sched1 >> 32;
			pkts[2]->hash.sched.lo = sched2 & 0xFFFFFFFF;
			pkts[2]->hash.sched.hi = sched2 >> 32;
			pkts[3]->hash.sched.lo = sched3 & 0xFFFFFFFF;
			pkts[3]->hash.sched.hi = sched3 >> 32;
		}
	}

	if (mpls) {
		if (mpls_color_mark) {
			uint64_t mpls_exp0 = rte_bswap64(
				(MPLS_LABEL(0, pkt0_color, 0, 0) << 32) |
				MPLS_LABEL(0, pkt0_color, 0, 0));
			uint64_t mpls_exp1 = rte_bswap64(
				(MPLS_LABEL(0, pkt1_color, 0, 0) << 32) |
				MPLS_LABEL(0, pkt1_color, 0, 0));
			uint64_t mpls_exp2 = rte_bswap64(
				(MPLS_LABEL(0, pkt2_color, 0, 0) << 32) |
				MPLS_LABEL(0, pkt2_color, 0, 0));
			uint64_t mpls_exp3 = rte_bswap64(
				(MPLS_LABEL(0, pkt3_color, 0, 0) << 32) |
				MPLS_LABEL(0, pkt3_color, 0, 0));

			*slab0_ptr0 = entry0->slab[0] | mpls_exp0;
			*slab0_ptr1 = entry1->slab[0] | mpls_exp1;
			*slab0_ptr2 = entry2->slab[0] | mpls_exp2;
			*slab0_ptr3 = entry3->slab[0] | mpls_exp3;

			*slab1_ptr0 = entry0->slab[1] | mpls_exp0;
			*slab1_ptr1 = entry1->slab[1] | mpls_exp1;
			*slab1_ptr2 = entry2->slab[1] | mpls_exp2;
			*slab1_ptr3 = entry3->slab[1] | mpls_exp3;

			*slab2_ptr0 = entry0->slab[2];
			*slab2_ptr1 = entry1->slab[2];
			*slab2_ptr2 = entry2->slab[2];
			*slab2_ptr3 = entry3->slab[2];
		} else {
			*slab0_ptr0 = entry0->slab[0];
			*slab0_ptr1 = entry1->slab[0];
			*slab0_ptr2 = entry2->slab[0];
			*slab0_ptr3 = entry3->slab[0];

			*slab1_ptr0 = entry0->slab[1];
			*slab1_ptr1 = entry1->slab[1];
			*slab1_ptr2 = entry2->slab[1];
			*slab1_ptr3 = entry3->slab[1];

			*slab2_ptr0 = entry0->slab[2];
			*slab2_ptr1 = entry1->slab[2];
			*slab2_ptr2 = entry2->slab[2];
			*slab2_ptr3 = entry3->slab[2];
		}

		if (arp == 0) {
			MACADDR_DST_WRITE(slab3_ptr0, entry0->slab[3]);
			MACADDR_DST_WRITE(slab3_ptr1, entry1->slab[3]);
			MACADDR_DST_WRITE(slab3_ptr2, entry2->slab[3]);
			MACADDR_DST_WRITE(slab3_ptr3, entry3->slab[3]);
		}
	}

	if (arp) {
		arp_key0->port_id = port_id0;
		arp_key1->port_id = port_id1;
		arp_key2->port_id = port_id2;
		arp_key3->port_id = port_id3;

		arp_key0->ip = nh_ip0;
		arp_key1->ip = nh_ip1;
		arp_key2->ip = nh_ip2;
		arp_key3->ip = nh_ip3;
	}
}

#define PKT_WORK_ROUTING_ETHERNET(arp)				\
static inline void						\
pkt_work_routing_ether_arp##arp(				\
	struct rte_mbuf *pkt,					\
	struct rte_pipeline_table_entry *table_entry,		\
	void *arg)						\
{								\
	pkt_work_routing(pkt, table_entry, arg, arp, 0, 0, 0, 0);\
}

#define PKT4_WORK_ROUTING_ETHERNET(arp)				\
static inline void						\
pkt4_work_routing_ether_arp##arp(				\
	struct rte_mbuf **pkts,					\
	struct rte_pipeline_table_entry **table_entries,	\
	void *arg)						\
{								\
	pkt4_work_routing(pkts, table_entries, arg, arp, 0, 0, 0, 0);\
}

#define routing_table_ah_hit_ether(arp)				\
PKT_WORK_ROUTING_ETHERNET(arp)					\
PKT4_WORK_ROUTING_ETHERNET(arp)					\
PIPELINE_TABLE_AH_HIT(routing_table_ah_hit_ether_arp##arp,	\
	pkt_work_routing_ether_arp##arp,			\
	pkt4_work_routing_ether_arp##arp)

routing_table_ah_hit_ether(0)
routing_table_ah_hit_ether(1)

#define PKT_WORK_ROUTING_ETHERNET_QINQ(sched, arp)		\
static inline void						\
pkt_work_routing_ether_qinq_sched##sched##_arp##arp(		\
	struct rte_mbuf *pkt,					\
	struct rte_pipeline_table_entry *table_entry,		\
	void *arg)						\
{								\
	pkt_work_routing(pkt, table_entry, arg, arp, 1, sched, 0, 0);\
}

#define PKT4_WORK_ROUTING_ETHERNET_QINQ(sched, arp)		\
static inline void						\
pkt4_work_routing_ether_qinq_sched##sched##_arp##arp(		\
	struct rte_mbuf **pkts,					\
	struct rte_pipeline_table_entry **table_entries,	\
	void *arg)						\
{								\
	pkt4_work_routing(pkts, table_entries, arg, arp, 1, sched, 0, 0);\
}

#define routing_table_ah_hit_ether_qinq(sched, arp)		\
PKT_WORK_ROUTING_ETHERNET_QINQ(sched, arp)			\
PKT4_WORK_ROUTING_ETHERNET_QINQ(sched, arp)			\
PIPELINE_TABLE_AH_HIT(routing_table_ah_hit_ether_qinq_sched##sched##_arp##arp,\
	pkt_work_routing_ether_qinq_sched##sched##_arp##arp,	\
	pkt4_work_routing_ether_qinq_sched##sched##_arp##arp)

routing_table_ah_hit_ether_qinq(0, 0)
routing_table_ah_hit_ether_qinq(1, 0)
routing_table_ah_hit_ether_qinq(2, 0)
routing_table_ah_hit_ether_qinq(0, 1)
routing_table_ah_hit_ether_qinq(1, 1)
routing_table_ah_hit_ether_qinq(2, 1)

#define PKT_WORK_ROUTING_ETHERNET_MPLS(color, arp)		\
static inline void						\
pkt_work_routing_ether_mpls_color##color##_arp##arp(		\
	struct rte_mbuf *pkt,					\
	struct rte_pipeline_table_entry *table_entry,		\
	void *arg)						\
{								\
	pkt_work_routing(pkt, table_entry, arg, arp, 0, 0, 1, color);\
}

#define PKT4_WORK_ROUTING_ETHERNET_MPLS(color, arp)		\
static inline void						\
pkt4_work_routing_ether_mpls_color##color##_arp##arp(		\
	struct rte_mbuf **pkts,					\
	struct rte_pipeline_table_entry **table_entries,	\
	void *arg)						\
{								\
	pkt4_work_routing(pkts, table_entries, arg, arp, 0, 0, 1, color);\
}

#define routing_table_ah_hit_ether_mpls(color, arp)		\
PKT_WORK_ROUTING_ETHERNET_MPLS(color, arp)			\
PKT4_WORK_ROUTING_ETHERNET_MPLS(color, arp)			\
PIPELINE_TABLE_AH_HIT(routing_table_ah_hit_ether_mpls_color##color##_arp##arp,\
	pkt_work_routing_ether_mpls_color##color##_arp##arp,	\
	pkt4_work_routing_ether_mpls_color##color##_arp##arp)

routing_table_ah_hit_ether_mpls(0, 0)
routing_table_ah_hit_ether_mpls(1, 0)
routing_table_ah_hit_ether_mpls(0, 1)
routing_table_ah_hit_ether_mpls(1, 1)

static rte_pipeline_table_action_handler_hit
get_routing_table_ah_hit(struct pipeline_routing *p)
{
	if (p->params.dbg_ah_disable)
		return NULL;

	switch (p->params.encap) {
	case PIPELINE_ROUTING_ENCAP_ETHERNET:
		return (p->params.n_arp_entries) ?
			routing_table_ah_hit_ether_arp1 :
			routing_table_ah_hit_ether_arp0;

	case PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ:
		if (p->params.n_arp_entries)
			switch (p->params.qinq_sched) {
			case 0:
				return routing_table_ah_hit_ether_qinq_sched0_arp1;
			case 1:
				return routing_table_ah_hit_ether_qinq_sched1_arp1;
			case 2:
				return routing_table_ah_hit_ether_qinq_sched2_arp1;
			default:
				return NULL;
			}
		 else
			switch (p->params.qinq_sched) {
			case 0:
				return routing_table_ah_hit_ether_qinq_sched0_arp0;
			case 1:
				return routing_table_ah_hit_ether_qinq_sched1_arp0;
			case 2:
				return routing_table_ah_hit_ether_qinq_sched2_arp0;
			default:
				return NULL;
			}

	case PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS:
		if (p->params.n_arp_entries)
			if (p->params.mpls_color_mark)
				return routing_table_ah_hit_ether_mpls_color1_arp1;
			else
				return routing_table_ah_hit_ether_mpls_color0_arp1;
		else
			if (p->params.mpls_color_mark)
				return routing_table_ah_hit_ether_mpls_color1_arp0;
			else
				return routing_table_ah_hit_ether_mpls_color0_arp0;

	default:
		return NULL;
	}
}

/*
 * ARP table
 */
struct arp_table_entry {
	struct rte_pipeline_table_entry head;
	uint64_t macaddr;
};

/**
 * ARP table AH
 */
static inline void
pkt_work_arp(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	__rte_unused void *arg)
{
	struct arp_table_entry *entry = (struct arp_table_entry *) table_entry;

	/* Read */
	uint64_t macaddr_dst = entry->macaddr;
	uint64_t *slab_ptr = (uint64_t *) ((char *) pkt->buf_addr +
		(pkt->data_off - 2));

	/* Compute */

	/* Write */
	MACADDR_DST_WRITE(slab_ptr, macaddr_dst);
}

static inline void
pkt4_work_arp(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	__rte_unused void *arg)
{
	struct arp_table_entry *entry0 =
		(struct arp_table_entry *) table_entries[0];
	struct arp_table_entry *entry1 =
		(struct arp_table_entry *) table_entries[1];
	struct arp_table_entry *entry2 =
		(struct arp_table_entry *) table_entries[2];
	struct arp_table_entry *entry3 =
		(struct arp_table_entry *) table_entries[3];

	/* Read */
	uint64_t macaddr_dst0 = entry0->macaddr;
	uint64_t macaddr_dst1 = entry1->macaddr;
	uint64_t macaddr_dst2 = entry2->macaddr;
	uint64_t macaddr_dst3 = entry3->macaddr;

	uint64_t *slab_ptr0 = (uint64_t *) ((char *) pkts[0]->buf_addr +
		(pkts[0]->data_off - 2));
	uint64_t *slab_ptr1 = (uint64_t *) ((char *) pkts[1]->buf_addr +
		(pkts[1]->data_off - 2));
	uint64_t *slab_ptr2 = (uint64_t *) ((char *) pkts[2]->buf_addr +
		(pkts[2]->data_off - 2));
	uint64_t *slab_ptr3 = (uint64_t *) ((char *) pkts[3]->buf_addr +
		(pkts[3]->data_off - 2));

	/* Compute */

	/* Write */
	MACADDR_DST_WRITE(slab_ptr0, macaddr_dst0);
	MACADDR_DST_WRITE(slab_ptr1, macaddr_dst1);
	MACADDR_DST_WRITE(slab_ptr2, macaddr_dst2);
	MACADDR_DST_WRITE(slab_ptr3, macaddr_dst3);
}

PIPELINE_TABLE_AH_HIT(arp_table_ah_hit,
	pkt_work_arp,
	pkt4_work_arp);

static rte_pipeline_table_action_handler_hit
get_arp_table_ah_hit(struct pipeline_routing *p)
{
	if (p->params.dbg_ah_disable)
		return NULL;

	return arp_table_ah_hit;
}

/*
 * Argument parsing
 */
int
pipeline_routing_parse_args(struct pipeline_routing_params *p,
	struct pipeline_params *params)
{
	uint32_t n_routes_present = 0;
	uint32_t port_local_dest_present = 0;
	uint32_t encap_present = 0;
	uint32_t qinq_sched_present = 0;
	uint32_t mpls_color_mark_present = 0;
	uint32_t n_arp_entries_present = 0;
	uint32_t ip_hdr_offset_present = 0;
	uint32_t arp_key_offset_present = 0;
	uint32_t color_offset_present = 0;
	uint32_t dbg_ah_disable_present = 0;
	uint32_t i;

	/* default values */
	p->n_routes = PIPELINE_ROUTING_N_ROUTES_DEFAULT;
	p->port_local_dest = params->n_ports_out - 1;
	p->encap = PIPELINE_ROUTING_ENCAP_ETHERNET;
	p->qinq_sched = 0;
	p->mpls_color_mark = 0;
	p->n_arp_entries = 0;
	p->dbg_ah_disable = 0;

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* n_routes */
		if (strcmp(arg_name, "n_routes") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				n_routes_present == 0, params->name,
				arg_name);
			n_routes_present = 1;

			status = parser_read_uint32(&p->n_routes,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status != -EINVAL) &&
				(p->n_routes != 0)), params->name,
				arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}
		/* port_local_dest */
		if (strcmp(arg_name, "port_local_dest") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				port_local_dest_present == 0, params->name,
				arg_name);
			port_local_dest_present = 1;

			status = parser_read_uint32(&p->port_local_dest,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL(((status == 0) &&
				(p->port_local_dest < params->n_ports_out)),
				params->name, arg_name, arg_value);

			continue;
		}

		/* encap */
		if (strcmp(arg_name, "encap") == 0) {
			PIPELINE_PARSE_ERR_DUPLICATE(encap_present == 0,
				params->name, arg_name);
			encap_present = 1;

			/* ethernet */
			if (strcmp(arg_value, "ethernet") == 0) {
				p->encap = PIPELINE_ROUTING_ENCAP_ETHERNET;
				continue;
			}

			/* ethernet_qinq */
			if (strcmp(arg_value, "ethernet_qinq") == 0) {
				p->encap = PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ;
				continue;
			}

			/* ethernet_mpls */
			if (strcmp(arg_value, "ethernet_mpls") == 0) {
				p->encap = PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS;
				continue;
			}

			/* any other */
			PIPELINE_PARSE_ERR_INV_VAL(0, params->name,
				arg_name, arg_value);
		}

		/* qinq_sched */
		if (strcmp(arg_name, "qinq_sched") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				qinq_sched_present == 0, params->name,
				arg_name);
			qinq_sched_present = 1;

			status = parser_read_arg_bool(arg_value);
			if (status == -EINVAL) {
				if (strcmp(arg_value, "test") == 0) {
					p->qinq_sched = 2;
					continue;
				}
			} else {
				p->qinq_sched = status;
				continue;
			}

			PIPELINE_PARSE_ERR_INV_VAL(0, params->name,
				arg_name, arg_value);
		}

		/* mpls_color_mark */
		if (strcmp(arg_name, "mpls_color_mark") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				mpls_color_mark_present == 0,
				params->name, arg_name);
			mpls_color_mark_present = 1;


			status = parser_read_arg_bool(arg_value);
			if (status >= 0) {
				p->mpls_color_mark = status;
				continue;
			}

			PIPELINE_PARSE_ERR_INV_VAL(0, params->name,
				arg_name, arg_value);
		}

		/* n_arp_entries */
		if (strcmp(arg_name, "n_arp_entries") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				n_arp_entries_present == 0, params->name,
				arg_name);
			n_arp_entries_present = 1;

			status = parser_read_uint32(&p->n_arp_entries,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* ip_hdr_offset */
		if (strcmp(arg_name, "ip_hdr_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				ip_hdr_offset_present == 0, params->name,
				arg_name);
			ip_hdr_offset_present = 1;

			status = parser_read_uint32(&p->ip_hdr_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* arp_key_offset */
		if (strcmp(arg_name, "arp_key_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				arp_key_offset_present == 0, params->name,
				arg_name);
			arp_key_offset_present = 1;

			status = parser_read_uint32(&p->arp_key_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* color_offset */
		if (strcmp(arg_name, "color_offset") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				color_offset_present == 0, params->name,
				arg_name);
			color_offset_present = 1;

			status = parser_read_uint32(&p->color_offset,
				arg_value);
			PIPELINE_PARSE_ERR_INV_VAL((status != -EINVAL),
				params->name, arg_name, arg_value);
			PIPELINE_PARSE_ERR_OUT_RNG((status != -ERANGE),
				params->name, arg_name, arg_value);

			continue;
		}

		/* debug */
		if (strcmp(arg_name, "dbg_ah_disable") == 0) {
			int status;

			PIPELINE_PARSE_ERR_DUPLICATE(
				dbg_ah_disable_present == 0, params->name,
				arg_name);
			dbg_ah_disable_present = 1;

			status = parser_read_arg_bool(arg_value);
			if (status >= 0) {
				p->dbg_ah_disable = status;
				continue;
			}

			PIPELINE_PARSE_ERR_INV_VAL(0, params->name,
				arg_name, arg_value);

			continue;
		}

		/* any other */
		PIPELINE_PARSE_ERR_INV_ENT(0, params->name, arg_name);
	}

	/* Check that mandatory arguments are present */
	PIPELINE_PARSE_ERR_MANDATORY(ip_hdr_offset_present, params->name,
		"ip_hdr_offset");

	/* Check relations between arguments */
	switch (p->encap) {
	case PIPELINE_ROUTING_ENCAP_ETHERNET:
		PIPELINE_ARG_CHECK((!p->qinq_sched), "Parse error in "
			"section \"%s\": encap = ethernet, therefore "
			"qinq_sched = yes/test is not allowed",
			params->name);
		PIPELINE_ARG_CHECK((!p->mpls_color_mark), "Parse error "
			"in section \"%s\": encap = ethernet, therefore "
			"mpls_color_mark = yes is not allowed",
			params->name);
		PIPELINE_ARG_CHECK((!color_offset_present), "Parse error "
			"in section \"%s\": encap = ethernet, therefore "
			"color_offset is not allowed",
			params->name);
		break;

	case PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ:
		PIPELINE_ARG_CHECK((!p->mpls_color_mark), "Parse error "
			"in section \"%s\": encap = ethernet_qinq, "
			"therefore mpls_color_mark = yes is not allowed",
			params->name);
		PIPELINE_ARG_CHECK((!color_offset_present), "Parse error "
			"in section \"%s\": encap = ethernet_qinq, "
			"therefore color_offset is not allowed",
			params->name);
		break;

	case PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS:
		PIPELINE_ARG_CHECK((!p->qinq_sched), "Parse error in "
			"section \"%s\": encap = ethernet_mpls, therefore "
			"qinq_sched  = yes/test is not allowed",
			params->name);
		break;
	}

	PIPELINE_ARG_CHECK((!(p->n_arp_entries &&
		(!arp_key_offset_present))), "Parse error in section "
			"\"%s\": n_arp_entries is set while "
			"arp_key_offset is not set", params->name);

	PIPELINE_ARG_CHECK((!((p->n_arp_entries == 0) &&
		arp_key_offset_present)), "Parse error in section "
			"\"%s\": arp_key_offset present while "
			"n_arp_entries is not set", params->name);

	return 0;
}

static void *
pipeline_routing_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_routing *p_rt;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_routing));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	p_rt = (struct pipeline_routing *) p;
	if (p == NULL)
		return NULL;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Routing");

	/* Parse arguments */
	if (pipeline_routing_parse_args(&p_rt->params, params))
		return NULL;

	/* Pipeline */
	{
		struct rte_pipeline_params pipeline_params = {
			.name = params->name,
			.socket_id = params->socket_id,
			.offset_port_id = 0,
		};

		p->p = rte_pipeline_create(&pipeline_params);
		if (p->p == NULL) {
			rte_free(p);
			return NULL;
		}
	}

	/* Input ports */
	p->n_ports_in = params->n_ports_in;
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_port_in_params port_params = {
			.ops = pipeline_port_in_params_get_ops(
				&params->port_in[i]),
			.arg_create = pipeline_port_in_params_convert(
				&params->port_in[i]),
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = params->port_in[i].burst_size,
		};

		int status = rte_pipeline_port_in_create(p->p,
			&port_params,
			&p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Output ports */
	p->n_ports_out = params->n_ports_out;
	for (i = 0; i < p->n_ports_out; i++) {
		struct rte_pipeline_port_out_params port_params = {
			.ops = pipeline_port_out_params_get_ops(
				&params->port_out[i]),
			.arg_create = pipeline_port_out_params_convert(
				&params->port_out[i]),
			.f_action = NULL,
			.arg_ah = NULL,
		};

		int status = rte_pipeline_port_out_create(p->p,
			&port_params,
			&p->port_out_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Routing table */
	p->n_tables = 1;
	{
		struct rte_table_lpm_params table_lpm_params = {
			.name = p->name,
			.n_rules = p_rt->params.n_routes,
			.number_tbl8s = PIPELINE_ROUTING_LPM_TABLE_NUMBER_TABLE8s,
			.flags = 0,
			.entry_unique_size = sizeof(struct routing_table_entry),
			.offset = p_rt->params.ip_hdr_offset +
				__builtin_offsetof(struct ipv4_hdr, dst_addr),
		};

		struct rte_pipeline_table_params table_params = {
				.ops = &rte_table_lpm_ops,
				.arg_create = &table_lpm_params,
				.f_action_hit = get_routing_table_ah_hit(p_rt),
				.f_action_miss = NULL,
				.arg_ah = p_rt,
				.action_data_size =
					sizeof(struct routing_table_entry) -
					sizeof(struct rte_pipeline_table_entry),
			};

		int status;

		status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* ARP table configuration */
	if (p_rt->params.n_arp_entries) {
		struct rte_table_hash_key8_ext_params table_arp_params = {
			.n_entries = p_rt->params.n_arp_entries,
			.n_entries_ext = p_rt->params.n_arp_entries,
			.f_hash = hash_default_key8,
			.seed = 0,
			.signature_offset = 0, /* Unused */
			.key_offset = p_rt->params.arp_key_offset,
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_hash_key8_ext_dosig_ops,
			.arg_create = &table_arp_params,
			.f_action_hit = get_arp_table_ah_hit(p_rt),
			.f_action_miss = NULL,
			.arg_ah = p_rt,
			.action_data_size = sizeof(struct arp_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		int status;

		status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[1]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}

		p->n_tables++;
	}

	/* Connecting input ports to tables */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_connect_to_table(p->p,
			p->port_in_id[i],
			p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Enable input ports */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_enable(p->p,
			p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Check pipeline consistency */
	if (rte_pipeline_check(p->p) < 0) {
		rte_pipeline_free(p->p);
		rte_free(p);
		return NULL;
	}

	/* Message queues */
	p->n_msgq = params->n_msgq;
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_in[i] = params->msgq_in[i];
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_out[i] = params->msgq_out[i];

	/* Message handlers */
	memcpy(p->handlers, handlers, sizeof(p->handlers));
	memcpy(p_rt->custom_handlers,
		custom_handlers,
		sizeof(p_rt->custom_handlers));

	return p;
}

static int
pipeline_routing_free(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	rte_pipeline_free(p->p);
	rte_free(p);
	return 0;
}

static int
pipeline_routing_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

void *
pipeline_routing_msg_req_custom_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing *p_rt = (struct pipeline_routing *) p;
	struct pipeline_custom_msg_req *req = msg;
	pipeline_msg_req_handler f_handle;

	f_handle = (req->subtype < PIPELINE_ROUTING_MSG_REQS) ?
		p_rt->custom_handlers[req->subtype] :
		pipeline_msg_req_invalid_handler;

	if (f_handle == NULL)
		f_handle = pipeline_msg_req_invalid_handler;

	return f_handle(p, req);
}

void *
pipeline_routing_msg_req_route_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing *p_rt = (struct pipeline_routing *) p;
	struct pipeline_routing_route_add_msg_req *req = msg;
	struct pipeline_routing_route_add_msg_rsp *rsp = msg;

	struct rte_table_lpm_key key = {
		.ip = req->key.key.ipv4.ip,
		.depth = req->key.key.ipv4.depth,
	};

	struct routing_table_entry entry_arp0 = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->data.port_id]},
		},

		.flags = req->data.flags,
		.port_id = req->data.port_id,
		.ip = 0,
		.data_offset = 0,
		.ether_l2_length = 0,
		.slab = {0},
		.slab_offset = {0},
	};

	struct routing_table_entry entry_arp1 = {
		.head = {
			.action = RTE_PIPELINE_ACTION_TABLE,
			{.table_id = p->table_id[1]},
		},

		.flags = req->data.flags,
		.port_id = req->data.port_id,
		.ip = rte_bswap32(req->data.ethernet.ip),
		.data_offset = 0,
		.ether_l2_length = 0,
		.slab = {0},
		.slab_offset = {0},
	};

	struct rte_pipeline_table_entry *entry = (p_rt->params.n_arp_entries) ?
		(struct rte_pipeline_table_entry *) &entry_arp1 :
		(struct rte_pipeline_table_entry *) &entry_arp0;

	if ((req->key.type != PIPELINE_ROUTING_ROUTE_IPV4) ||
		((p_rt->params.n_arp_entries == 0) &&
			(req->data.flags & PIPELINE_ROUTING_ROUTE_ARP)) ||
		(p_rt->params.n_arp_entries &&
			((req->data.flags & PIPELINE_ROUTING_ROUTE_ARP) == 0)) ||
		((p_rt->params.encap != PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ) &&
			(req->data.flags & PIPELINE_ROUTING_ROUTE_QINQ)) ||
		((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ) &&
			((req->data.flags & PIPELINE_ROUTING_ROUTE_QINQ) == 0)) ||
		((p_rt->params.encap != PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) &&
			(req->data.flags & PIPELINE_ROUTING_ROUTE_MPLS)) ||
		((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) &&
			((req->data.flags & PIPELINE_ROUTING_ROUTE_MPLS) == 0))) {
		rsp->status = -1;
		return rsp;
	}

	/* Ether - ARP off */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET) &&
		(p_rt->params.n_arp_entries == 0)) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t macaddr_dst;
		uint64_t ethertype = ETHER_TYPE_IPv4;

		macaddr_dst = *((uint64_t *)&(req->data.ethernet.macaddr));
		macaddr_dst = rte_bswap64(macaddr_dst << 16);

		entry_arp0.slab[0] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype);
		entry_arp0.slab_offset[0] = p_rt->params.ip_hdr_offset - 8;

		entry_arp0.slab[1] = rte_bswap64(macaddr_dst);
		entry_arp0.slab_offset[1] = p_rt->params.ip_hdr_offset - 2 * 8;

		entry_arp0.data_offset = entry_arp0.slab_offset[1] + 2
			- sizeof(struct rte_mbuf);
		entry_arp0.ether_l2_length = 14;
	}

	/* Ether - ARP on */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET) &&
		p_rt->params.n_arp_entries) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t ethertype = ETHER_TYPE_IPv4;

		entry_arp1.slab[0] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype);
		entry_arp1.slab_offset[0] = p_rt->params.ip_hdr_offset - 8;

		entry_arp1.data_offset = entry_arp1.slab_offset[0] - 6
			- sizeof(struct rte_mbuf);
		entry_arp1.ether_l2_length = 14;
	}

	/* Ether QinQ - ARP off */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ) &&
		(p_rt->params.n_arp_entries == 0)) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t macaddr_dst;
		uint64_t ethertype_ipv4 = ETHER_TYPE_IPv4;
		uint64_t ethertype_vlan = 0x8100;
		uint64_t ethertype_qinq = 0x9100;
		uint64_t svlan = req->data.l2.qinq.svlan;
		uint64_t cvlan = req->data.l2.qinq.cvlan;

		macaddr_dst = *((uint64_t *)&(req->data.ethernet.macaddr));
		macaddr_dst = rte_bswap64(macaddr_dst << 16);

		entry_arp0.slab[0] = rte_bswap64((svlan << 48) |
			(ethertype_vlan << 32) |
			(cvlan << 16) |
			ethertype_ipv4);
		entry_arp0.slab_offset[0] = p_rt->params.ip_hdr_offset - 8;

		entry_arp0.slab[1] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype_qinq);
		entry_arp0.slab_offset[1] = p_rt->params.ip_hdr_offset - 2 * 8;

		entry_arp0.slab[2] = rte_bswap64(macaddr_dst);
		entry_arp0.slab_offset[2] = p_rt->params.ip_hdr_offset - 3 * 8;

		entry_arp0.data_offset = entry_arp0.slab_offset[2] + 2
			- sizeof(struct rte_mbuf);
		entry_arp0.ether_l2_length = 22;
	}

	/* Ether QinQ - ARP on */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ) &&
		p_rt->params.n_arp_entries) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t ethertype_ipv4 = ETHER_TYPE_IPv4;
		uint64_t ethertype_vlan = 0x8100;
		uint64_t ethertype_qinq = 0x9100;
		uint64_t svlan = req->data.l2.qinq.svlan;
		uint64_t cvlan = req->data.l2.qinq.cvlan;

		entry_arp1.slab[0] = rte_bswap64((svlan << 48) |
			(ethertype_vlan << 32) |
			(cvlan << 16) |
			ethertype_ipv4);
		entry_arp1.slab_offset[0] = p_rt->params.ip_hdr_offset - 8;

		entry_arp1.slab[1] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype_qinq);
		entry_arp1.slab_offset[1] = p_rt->params.ip_hdr_offset - 2 * 8;

		entry_arp1.data_offset = entry_arp1.slab_offset[1] - 6
			- sizeof(struct rte_mbuf);
		entry_arp1.ether_l2_length = 22;
	}

	/* Ether MPLS - ARP off */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) &&
		(p_rt->params.n_arp_entries == 0)) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t macaddr_dst;
		uint64_t ethertype_mpls = 0x8847;

		uint64_t label0 = req->data.l2.mpls.labels[0];
		uint64_t label1 = req->data.l2.mpls.labels[1];
		uint64_t label2 = req->data.l2.mpls.labels[2];
		uint64_t label3 = req->data.l2.mpls.labels[3];
		uint32_t n_labels = req->data.l2.mpls.n_labels;

		macaddr_dst = *((uint64_t *)&(req->data.ethernet.macaddr));
		macaddr_dst = rte_bswap64(macaddr_dst << 16);

		switch (n_labels) {
		case 1:
			entry_arp0.slab[0] = 0;
			entry_arp0.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp0.slab[1] = rte_bswap64(
				MPLS_LABEL(label0, 0, 1, 0));
			entry_arp0.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 8;
			break;

		case 2:
			entry_arp0.slab[0] = 0;
			entry_arp0.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp0.slab[1] = rte_bswap64(
				(MPLS_LABEL(label0, 0, 0, 0) << 32) |
				MPLS_LABEL(label1, 0, 1, 0));
			entry_arp0.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 8;
			break;

		case 3:
			entry_arp0.slab[0] = rte_bswap64(
				(MPLS_LABEL(label1, 0, 0, 0) << 32) |
				MPLS_LABEL(label2, 0, 1, 0));
			entry_arp0.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp0.slab[1] = rte_bswap64(
				MPLS_LABEL(label0, 0, 0, 0));
			entry_arp0.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 2 * 8;
			break;

		case 4:
			entry_arp0.slab[0] = rte_bswap64(
				(MPLS_LABEL(label2, 0, 0, 0) << 32) |
				MPLS_LABEL(label3, 0, 1, 0));
			entry_arp0.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp0.slab[1] = rte_bswap64(
				(MPLS_LABEL(label0, 0, 0, 0) << 32) |
				MPLS_LABEL(label1, 0, 0, 0));
			entry_arp0.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 2 * 8;
			break;

		default:
			rsp->status = -1;
			return rsp;
		}

		entry_arp0.slab[2] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype_mpls);
		entry_arp0.slab_offset[2] = p_rt->params.ip_hdr_offset -
			(n_labels * 4 + 8);

		entry_arp0.slab[3] = rte_bswap64(macaddr_dst);
		entry_arp0.slab_offset[3] = p_rt->params.ip_hdr_offset -
			(n_labels * 4 + 2 * 8);

		entry_arp0.data_offset = entry_arp0.slab_offset[3] + 2
			- sizeof(struct rte_mbuf);
		entry_arp0.ether_l2_length = n_labels * 4 + 14;
	}

	/* Ether MPLS - ARP on */
	if ((p_rt->params.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) &&
		p_rt->params.n_arp_entries) {
		uint64_t macaddr_src = p_rt->macaddr[req->data.port_id];
		uint64_t ethertype_mpls = 0x8847;

		uint64_t label0 = req->data.l2.mpls.labels[0];
		uint64_t label1 = req->data.l2.mpls.labels[1];
		uint64_t label2 = req->data.l2.mpls.labels[2];
		uint64_t label3 = req->data.l2.mpls.labels[3];
		uint32_t n_labels = req->data.l2.mpls.n_labels;

		switch (n_labels) {
		case 1:
			entry_arp1.slab[0] = 0;
			entry_arp1.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp1.slab[1] = rte_bswap64(
				MPLS_LABEL(label0, 0, 1, 0));
			entry_arp1.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 8;
			break;

		case 2:
			entry_arp1.slab[0] = 0;
			entry_arp1.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp1.slab[1] = rte_bswap64(
				(MPLS_LABEL(label0, 0, 0, 0) << 32) |
				MPLS_LABEL(label1, 0, 1, 0));
			entry_arp1.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 8;
			break;

		case 3:
			entry_arp1.slab[0] = rte_bswap64(
				(MPLS_LABEL(label1, 0, 0, 0) << 32) |
				MPLS_LABEL(label2, 0, 1, 0));
			entry_arp1.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp1.slab[1] = rte_bswap64(
				MPLS_LABEL(label0, 0, 0, 0));
			entry_arp1.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 2 * 8;
			break;

		case 4:
			entry_arp1.slab[0] = rte_bswap64(
				(MPLS_LABEL(label2, 0, 0, 0) << 32) |
				MPLS_LABEL(label3, 0, 1, 0));
			entry_arp1.slab_offset[0] =
				p_rt->params.ip_hdr_offset - 8;

			entry_arp1.slab[1] = rte_bswap64(
				(MPLS_LABEL(label0, 0, 0, 0) << 32) |
				MPLS_LABEL(label1, 0, 0, 0));
			entry_arp1.slab_offset[1] =
				p_rt->params.ip_hdr_offset - 2 * 8;
			break;

		default:
			rsp->status = -1;
			return rsp;
		}

		entry_arp1.slab[2] =
			SLAB_NBO_MACADDRSRC_ETHERTYPE(macaddr_src, ethertype_mpls);
		entry_arp1.slab_offset[2] = p_rt->params.ip_hdr_offset -
			(n_labels * 4 + 8);

		entry_arp1.data_offset = entry_arp1.slab_offset[2] - 6
			- sizeof(struct rte_mbuf);
		entry_arp1.ether_l2_length = n_labels * 4 + 14;
	}

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[0],
		&key,
		entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_route_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_route_delete_msg_req *req = msg;
	struct pipeline_routing_route_delete_msg_rsp *rsp = msg;

	struct rte_table_lpm_key key = {
		.ip = req->key.key.ipv4.ip,
		.depth = req->key.key.ipv4.depth,
	};

	if (req->key.type != PIPELINE_ROUTING_ROUTE_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[0],
		&key,
		&rsp->key_found,
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_route_add_default_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing_route_add_default_msg_req *req = msg;
	struct pipeline_routing_route_add_default_msg_rsp *rsp = msg;

	struct routing_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},

		.flags = 0,
		.port_id = 0,
		.ip = 0,
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[0],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_route_del_default_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing_route_delete_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[0],
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_add_msg_req *req = msg;
	struct pipeline_routing_arp_add_msg_rsp *rsp = msg;

	struct pipeline_routing_arp_key_ipv4 key = {
		.port_id = req->key.key.ipv4.port_id,
		.ip = rte_bswap32(req->key.key.ipv4.ip),
	};

	struct arp_table_entry entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->key.key.ipv4.port_id]},
		},

		.macaddr = 0, /* set below */
	};

	if (req->key.type != PIPELINE_ROUTING_ARP_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	entry.macaddr = *((uint64_t *)&(req->macaddr));
	entry.macaddr = entry.macaddr << 16;

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[1],
		&key,
		(struct rte_pipeline_table_entry *) &entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_delete_msg_req *req = msg;
	struct pipeline_routing_arp_delete_msg_rsp *rsp = msg;

	struct pipeline_routing_arp_key_ipv4 key = {
		.port_id = req->key.key.ipv4.port_id,
		.ip = rte_bswap32(req->key.key.ipv4.ip),
	};

	if (req->key.type != PIPELINE_ROUTING_ARP_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[1],
		&key,
		&rsp->key_found,
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_add_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_add_default_msg_req *req = msg;
	struct pipeline_routing_arp_add_default_msg_rsp *rsp = msg;

	struct arp_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},

		.macaddr = 0,
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[1],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_del_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_delete_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[1],
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_set_macaddr_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing *p_rt = (struct pipeline_routing *) p;
	struct pipeline_routing_set_macaddr_msg_req *req = msg;
	struct pipeline_routing_set_macaddr_msg_rsp *rsp = msg;
	uint32_t port_id;

	for (port_id = 0; port_id < p->n_ports_out; port_id++)
		p_rt->macaddr[port_id] = req->macaddr[port_id];

	rsp->status = 0;

	return rsp;
}

struct pipeline_be_ops pipeline_routing_be_ops = {
	.f_init = pipeline_routing_init,
	.f_free = pipeline_routing_free,
	.f_run = NULL,
	.f_timer = pipeline_routing_timer,
};
