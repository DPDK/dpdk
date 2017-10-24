/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#ifndef TEST_FLOW_CLASSIFY_H_
#define TEST_FLOW_CLASSIFY_H_

#define MAX_PKT_BURST      (32)
#define NB_SOCKETS         (1)
#define MEMPOOL_CACHE_SIZE (256)
#define MBUF_SIZE          (512)
#define NB_MBUF            (512)

/* test UDP, TCP and SCTP packets */
static struct rte_mempool *mbufpool[NB_SOCKETS];
static struct rte_mbuf *bufs[MAX_PKT_BURST];

/* ACL field definitions for IPv4 5 tuple rule */

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

enum {
	PROTO_INPUT_IPV4,
	SRC_INPUT_IPV4,
	DST_INPUT_IPV4,
	SRCP_DESTP_INPUT_IPV4
};

static struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
	/* first input field - always one byte long. */
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = PROTO_INPUT_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, next_proto_id),
	},
	/* next input field (IPv4 source address) - 4 consecutive bytes. */
	{
		/* rte_flow uses a bit mask for IPv4 addresses */
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = SRC_INPUT_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, src_addr),
	},
	/* next input field (IPv4 destination address) - 4 consecutive bytes. */
	{
		/* rte_flow uses a bit mask for IPv4 addresses */
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = DST_INPUT_IPV4,
		.offset = sizeof(struct ether_hdr) +
			offsetof(struct ipv4_hdr, dst_addr),
	},
	/*
	 * Next 2 fields (src & dst ports) form 4 consecutive bytes.
	 * They share the same input index.
	 */
	{
		/* rte_flow uses a bit mask for protocol ports */
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = SRCP_DESTP_INPUT_IPV4,
		.offset = sizeof(struct ether_hdr) +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, src_port),
	},
	{
		/* rte_flow uses a bit mask for protocol ports */
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = SRCP_DESTP_INPUT_IPV4,
		.offset = sizeof(struct ether_hdr) +
			sizeof(struct ipv4_hdr) +
			offsetof(struct tcp_hdr, dst_port),
	},
};

/* parameters for rte_flow_classify_validate and rte_flow_classify_create */

/* test UDP pattern:
 * "eth / ipv4 src spec 2.2.2.3 src mask 255.255.255.00 dst spec 2.2.2.7
 *  dst mask 255.255.255.00 / udp src is 32 dst is 33 / end"
 */
static struct rte_flow_item_ipv4 ipv4_udp_spec_1 = {
	{ 0, 0, 0, 0, 0, 0, IPPROTO_UDP, 0, IPv4(2, 2, 2, 3), IPv4(2, 2, 2, 7)}
};
static const struct rte_flow_item_ipv4 ipv4_mask_24 = {
	.hdr = {
		.next_proto_id = 0xff,
		.src_addr = 0xffffff00,
		.dst_addr = 0xffffff00,
	},
};
static struct rte_flow_item_udp udp_spec_1 = {
	{ 32, 33, 0, 0 }
};

static struct rte_flow_item  eth_item = { RTE_FLOW_ITEM_TYPE_ETH,
	0, 0, 0 };
static struct rte_flow_item  eth_item_bad = { -1, 0, 0, 0 };

static struct rte_flow_item  ipv4_udp_item_1 = { RTE_FLOW_ITEM_TYPE_IPV4,
	&ipv4_udp_spec_1, 0, &ipv4_mask_24};
static struct rte_flow_item  ipv4_udp_item_bad = { RTE_FLOW_ITEM_TYPE_IPV4,
	NULL, 0, NULL};

static struct rte_flow_item  udp_item_1 = { RTE_FLOW_ITEM_TYPE_UDP,
	&udp_spec_1, 0, &rte_flow_item_udp_mask};
static struct rte_flow_item  udp_item_bad = { RTE_FLOW_ITEM_TYPE_UDP,
	NULL, 0, NULL};

static struct rte_flow_item  end_item = { RTE_FLOW_ITEM_TYPE_END,
	0, 0, 0 };
static struct rte_flow_item  end_item_bad = { -1, 0, 0, 0 };

/* test TCP pattern:
 * "eth / ipv4 src spec 1.2.3.4 src mask 255.255.255.00 dst spec 5.6.7.8
 *  dst mask 255.255.255.00 / tcp src is 16 dst is 17 / end"
 */
static struct rte_flow_item_ipv4 ipv4_tcp_spec_1 = {
	{ 0, 0, 0, 0, 0, 0, IPPROTO_TCP, 0, IPv4(1, 2, 3, 4), IPv4(5, 6, 7, 8)}
};

static struct rte_flow_item_tcp tcp_spec_1 = {
	{ 16, 17, 0, 0, 0, 0, 0, 0, 0}
};

static struct rte_flow_item  ipv4_tcp_item_1 = { RTE_FLOW_ITEM_TYPE_IPV4,
	&ipv4_tcp_spec_1, 0, &ipv4_mask_24};

static struct rte_flow_item  tcp_item_1 = { RTE_FLOW_ITEM_TYPE_TCP,
	&tcp_spec_1, 0, &rte_flow_item_tcp_mask};

/* test SCTP pattern:
 * "eth / ipv4 src spec 1.2.3.4 src mask 255.255.255.00 dst spec 5.6.7.8
 *  dst mask 255.255.255.00 / sctp src is 16 dst is 17/ end"
 */
static struct rte_flow_item_ipv4 ipv4_sctp_spec_1 = {
	{ 0, 0, 0, 0, 0, 0, IPPROTO_SCTP, 0, IPv4(11, 12, 13, 14),
	IPv4(15, 16, 17, 18)}
};

static struct rte_flow_item_sctp sctp_spec_1 = {
	{ 10, 11, 0, 0}
};

static struct rte_flow_item  ipv4_sctp_item_1 = { RTE_FLOW_ITEM_TYPE_IPV4,
	&ipv4_sctp_spec_1, 0, &ipv4_mask_24};

static struct rte_flow_item  sctp_item_1 = { RTE_FLOW_ITEM_TYPE_SCTP,
	&sctp_spec_1, 0, &rte_flow_item_sctp_mask};


/* test actions:
 * "actions count / end"
 */
static struct rte_flow_action count_action = { RTE_FLOW_ACTION_TYPE_COUNT, 0};
static struct rte_flow_action count_action_bad = { -1, 0};

static struct rte_flow_action end_action = { RTE_FLOW_ACTION_TYPE_END, 0};
static struct rte_flow_action end_action_bad =	{ -1, 0};

static struct rte_flow_action actions[2];

/* test attributes */
static struct rte_flow_attr attr;

/* test error */
static struct rte_flow_error error;

/* test pattern */
static struct rte_flow_item  pattern[4];

/* flow classify data for UDP burst */
static struct rte_flow_classify_ipv4_5tuple_stats udp_ntuple_stats;
static struct rte_flow_classify_stats udp_classify_stats = {
		.stats = (void *)&udp_ntuple_stats
};

/* flow classify data for TCP burst */
static struct rte_flow_classify_ipv4_5tuple_stats tcp_ntuple_stats;
static struct rte_flow_classify_stats tcp_classify_stats = {
		.stats = (void *)&tcp_ntuple_stats
};

/* flow classify data for SCTP burst */
static struct rte_flow_classify_ipv4_5tuple_stats sctp_ntuple_stats;
static struct rte_flow_classify_stats sctp_classify_stats = {
		.stats = (void *)&sctp_ntuple_stats
};
#endif /* TEST_FLOW_CLASSIFY_H_ */
