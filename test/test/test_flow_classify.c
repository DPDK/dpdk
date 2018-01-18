/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <string.h>
#include <errno.h>

#include "test.h"

#include <rte_string_fns.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_acl.h>
#include <rte_common.h>
#include <rte_table_acl.h>
#include <rte_flow.h>
#include <rte_flow_classify.h>

#include "packet_burst_generator.h"
#include "test_flow_classify.h"


#define FLOW_CLASSIFY_MAX_RULE_NUM 100
#define MAX_PKT_BURST              32
#define NB_SOCKETS                 1
#define MEMPOOL_CACHE_SIZE         256
#define MBUF_SIZE                  512
#define NB_MBUF                    512

/* test UDP, TCP and SCTP packets */
static struct rte_mempool *mbufpool[NB_SOCKETS];
static struct rte_mbuf *bufs[MAX_PKT_BURST];

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
static struct rte_flow_query_count count = {
	.reset = 1,
	.hits_set = 1,
	.bytes_set = 1,
	.hits = 0,
	.bytes = 0,
};
static struct rte_flow_action count_action = { RTE_FLOW_ACTION_TYPE_COUNT,
	&count};
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

struct flow_classifier_acl *cls;

struct flow_classifier_acl {
	struct rte_flow_classifier *cls;
} __rte_cache_aligned;

/*
 * test functions by passing invalid or
 * non-workable parameters.
 */
static int
test_invalid_parameters(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;

	ret = rte_flow_classify_validate(NULL, NULL, NULL, NULL, NULL);
	if (!ret) {
		printf("Line %i: rte_flow_classify_validate",
			__LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(NULL, NULL, NULL, NULL,
			NULL, NULL);
	if (rule) {
		printf("Line %i: flow_classifier_table_entry_add", __LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(NULL, NULL);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(NULL, NULL, 0, NULL, NULL);
	if (!ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(NULL, NULL, NULL, NULL,
		NULL, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add ", __LINE__);
		printf("with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(NULL, NULL);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf("with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(NULL, NULL, 0, NULL, NULL);
	if (!ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_valid_parameters(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;
	int key_found;

	/*
	 * set up parameters for rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item;
	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action;
	actions[1] = end_action;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (ret) {
		printf("Line %i: rte_flow_classify_validate",
			__LINE__);
		printf(" should not have failed!\n");
		return -1;
	}
	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);

	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should not have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_invalid_patterns(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;
	int key_found;

	/*
	 * set up parameters for rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item_bad;
	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action;
	actions[1] = end_action;

	pattern[0] = eth_item;
	pattern[1] = ipv4_udp_item_bad;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (!ret) {
		printf("Line %i: rte_flow_classify_validate", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_bad;
	pattern[3] = end_item_bad;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (!ret) {
		printf("Line %i: rte_flow_classify_validate", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_invalid_actions(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;
	int key_found;

	/*
	 * set up parameters for rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item;
	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action_bad;
	actions[1] = end_action;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (!ret) {
		printf("Line %i: rte_flow_classify_validate", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	actions[0] = count_action;
	actions[1] = end_action_bad;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (!ret) {
		printf("Line %i: rte_flow_classify_validate", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf("should have failed!\n");
		return -1;
	}
	return 0;
}

static int
init_ipv4_udp_traffic(struct rte_mempool *mp,
	     struct rte_mbuf **pkts_burst, uint32_t burst_size)
{
	struct ether_hdr pkt_eth_hdr;
	struct ipv4_hdr pkt_ipv4_hdr;
	struct udp_hdr pkt_udp_hdr;
	uint32_t src_addr = IPV4_ADDR(2, 2, 2, 3);
	uint32_t dst_addr = IPV4_ADDR(2, 2, 2, 7);
	uint16_t src_port = 32;
	uint16_t dst_port = 33;
	uint16_t pktlen;

	static uint8_t src_mac[] = { 0x00, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };
	static uint8_t dst_mac[] = { 0x00, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };

	printf("Set up IPv4 UDP traffic\n");
	initialize_eth_header(&pkt_eth_hdr,
		(struct ether_addr *)src_mac,
		(struct ether_addr *)dst_mac, ETHER_TYPE_IPv4, 0, 0);
	pktlen = (uint16_t)(sizeof(struct ether_hdr));
	printf("ETH  pktlen %u\n", pktlen);

	pktlen = initialize_ipv4_header(&pkt_ipv4_hdr, src_addr, dst_addr,
					pktlen);
	printf("ETH + IPv4 pktlen %u\n", pktlen);

	pktlen = initialize_udp_header(&pkt_udp_hdr, src_port, dst_port,
					pktlen);
	printf("ETH + IPv4 + UDP pktlen %u\n\n", pktlen);

	return generate_packet_burst(mp, pkts_burst, &pkt_eth_hdr,
				     0, &pkt_ipv4_hdr, 1,
				     &pkt_udp_hdr, burst_size,
				     PACKET_BURST_GEN_PKT_LEN, 1);
}

static int
init_ipv4_tcp_traffic(struct rte_mempool *mp,
	     struct rte_mbuf **pkts_burst, uint32_t burst_size)
{
	struct ether_hdr pkt_eth_hdr;
	struct ipv4_hdr pkt_ipv4_hdr;
	struct tcp_hdr pkt_tcp_hdr;
	uint32_t src_addr = IPV4_ADDR(1, 2, 3, 4);
	uint32_t dst_addr = IPV4_ADDR(5, 6, 7, 8);
	uint16_t src_port = 16;
	uint16_t dst_port = 17;
	uint16_t pktlen;

	static uint8_t src_mac[] = { 0x00, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };
	static uint8_t dst_mac[] = { 0x00, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };

	printf("Set up IPv4 TCP traffic\n");
	initialize_eth_header(&pkt_eth_hdr,
		(struct ether_addr *)src_mac,
		(struct ether_addr *)dst_mac, ETHER_TYPE_IPv4, 0, 0);
	pktlen = (uint16_t)(sizeof(struct ether_hdr));
	printf("ETH  pktlen %u\n", pktlen);

	pktlen = initialize_ipv4_header_proto(&pkt_ipv4_hdr, src_addr,
					dst_addr, pktlen, IPPROTO_TCP);
	printf("ETH + IPv4 pktlen %u\n", pktlen);

	pktlen = initialize_tcp_header(&pkt_tcp_hdr, src_port, dst_port,
					pktlen);
	printf("ETH + IPv4 + TCP pktlen %u\n\n", pktlen);

	return generate_packet_burst_proto(mp, pkts_burst, &pkt_eth_hdr,
					0, &pkt_ipv4_hdr, 1, IPPROTO_TCP,
					&pkt_tcp_hdr, burst_size,
					PACKET_BURST_GEN_PKT_LEN, 1);
}

static int
init_ipv4_sctp_traffic(struct rte_mempool *mp,
	     struct rte_mbuf **pkts_burst, uint32_t burst_size)
{
	struct ether_hdr pkt_eth_hdr;
	struct ipv4_hdr pkt_ipv4_hdr;
	struct sctp_hdr pkt_sctp_hdr;
	uint32_t src_addr = IPV4_ADDR(11, 12, 13, 14);
	uint32_t dst_addr = IPV4_ADDR(15, 16, 17, 18);
	uint16_t src_port = 10;
	uint16_t dst_port = 11;
	uint16_t pktlen;

	static uint8_t src_mac[] = { 0x00, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };
	static uint8_t dst_mac[] = { 0x00, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };

	printf("Set up IPv4 SCTP traffic\n");
	initialize_eth_header(&pkt_eth_hdr,
		(struct ether_addr *)src_mac,
		(struct ether_addr *)dst_mac, ETHER_TYPE_IPv4, 0, 0);
	pktlen = (uint16_t)(sizeof(struct ether_hdr));
	printf("ETH  pktlen %u\n", pktlen);

	pktlen = initialize_ipv4_header_proto(&pkt_ipv4_hdr, src_addr,
					dst_addr, pktlen, IPPROTO_SCTP);
	printf("ETH + IPv4 pktlen %u\n", pktlen);

	pktlen = initialize_sctp_header(&pkt_sctp_hdr, src_port, dst_port,
					pktlen);
	printf("ETH + IPv4 + SCTP pktlen %u\n\n", pktlen);

	return generate_packet_burst_proto(mp, pkts_burst, &pkt_eth_hdr,
					0, &pkt_ipv4_hdr, 1, IPPROTO_SCTP,
					&pkt_sctp_hdr, burst_size,
					PACKET_BURST_GEN_PKT_LEN, 1);
}

static int
init_mbufpool(void)
{
	int socketid;
	int ret = 0;
	unsigned int lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		socketid = rte_lcore_to_socket_id(lcore_id);
		if (socketid >= NB_SOCKETS) {
			printf(
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
			ret = -1;
			break;
		}
		if (mbufpool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			mbufpool[socketid] =
				rte_pktmbuf_pool_create(s, NB_MBUF,
					MEMPOOL_CACHE_SIZE, 0, MBUF_SIZE,
					socketid);
			if (mbufpool[socketid]) {
				printf("Allocated mbuf pool on socket %d\n",
					socketid);
			} else {
				printf("Cannot init mbuf pool on socket %d\n",
					socketid);
				ret = -ENOMEM;
				break;
			}
		}
	}
	return ret;
}

static int
test_query_udp(void)
{
	struct rte_flow_error error;
	struct rte_flow_classify_rule *rule;
	int ret;
	int i;
	int key_found;

	ret = init_ipv4_udp_traffic(mbufpool[0], bufs, MAX_PKT_BURST);
	if (ret != MAX_PKT_BURST) {
		printf("Line %i: init_udp_ipv4_traffic has failed!\n",
				__LINE__);
		return -1;
	}

	for (i = 0; i < MAX_PKT_BURST; i++)
		bufs[i]->packet_type = RTE_PTYPE_L3_IPV4;

	/*
	 * set up parameters for rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item;
	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action;
	actions[1] = end_action;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (ret) {
		printf("Line %i: rte_flow_classify_validate", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, bufs, MAX_PKT_BURST,
			rule, &udp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should not have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_query_tcp(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;
	int i;
	int key_found;

	ret = init_ipv4_tcp_traffic(mbufpool[0], bufs, MAX_PKT_BURST);
	if (ret != MAX_PKT_BURST) {
		printf("Line %i: init_ipv4_tcp_traffic has failed!\n",
				__LINE__);
		return -1;
	}

	for (i = 0; i < MAX_PKT_BURST; i++)
		bufs[i]->packet_type = RTE_PTYPE_L3_IPV4;

	/*
	 * set up parameters for rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item;
	pattern[1] = ipv4_tcp_item_1;
	pattern[2] = tcp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action;
	actions[1] = end_action;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, bufs, MAX_PKT_BURST,
			rule, &tcp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should not have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_query_sctp(void)
{
	struct rte_flow_classify_rule *rule;
	int ret;
	int i;
	int key_found;

	ret = init_ipv4_sctp_traffic(mbufpool[0], bufs, MAX_PKT_BURST);
	if (ret != MAX_PKT_BURST) {
		printf("Line %i: init_ipv4_tcp_traffic has failed!\n",
			__LINE__);
		return -1;
	}

	for (i = 0; i < MAX_PKT_BURST; i++)
		bufs[i]->packet_type = RTE_PTYPE_L3_IPV4;

	/*
	 * set up parameters rte_flow_classify_validate,
	 * rte_flow_classify_table_entry_add and
	 * rte_flow_classify_table_entry_delete
	 */

	attr.ingress = 1;
	attr.priority = 1;
	pattern[0] = eth_item;
	pattern[1] = ipv4_sctp_item_1;
	pattern[2] = sctp_item_1;
	pattern[3] = end_item;
	actions[0] = count_action;
	actions[1] = end_action;

	ret = rte_flow_classify_validate(cls->cls, &attr, pattern,
			actions, &error);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(cls->cls, &attr, pattern,
			actions, &key_found, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, bufs, MAX_PKT_BURST,
			rule, &sctp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, rule);
	if (ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should not have failed!\n");
		return -1;
	}
	return 0;
}

static int
test_flow_classify(void)
{
	struct rte_table_acl_params table_acl_params;
	struct rte_flow_classify_table_params cls_table_params;
	struct rte_flow_classifier_params cls_params;
	int ret;
	uint32_t size;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct flow_classifier_acl));
	cls = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);

	cls_params.name = "flow_classifier";
	cls_params.socket_id = 0;
	cls->cls = rte_flow_classifier_create(&cls_params);

	/* initialise ACL table params */
	table_acl_params.n_rule_fields = RTE_DIM(ipv4_defs);
	table_acl_params.name = "table_acl_ipv4_5tuple";
	table_acl_params.n_rules = FLOW_CLASSIFY_MAX_RULE_NUM;
	memcpy(table_acl_params.field_format, ipv4_defs, sizeof(ipv4_defs));

	/* initialise table create params */
	cls_table_params.ops = &rte_table_acl_ops;
	cls_table_params.arg_create = &table_acl_params;
	cls_table_params.type = RTE_FLOW_CLASSIFY_TABLE_ACL_IP4_5TUPLE;

	ret = rte_flow_classify_table_create(cls->cls, &cls_table_params);
	if (ret) {
		printf("Line %i: f_create has failed!\n", __LINE__);
		rte_flow_classifier_free(cls->cls);
		rte_free(cls);
		return -1;
	}
	printf("Created table_acl for for IPv4 five tuple packets\n");

	ret = init_mbufpool();
	if (ret) {
		printf("Line %i: init_mbufpool has failed!\n", __LINE__);
		return -1;
	}

	if (test_invalid_parameters() < 0)
		return -1;
	if (test_valid_parameters() < 0)
		return -1;
	if (test_invalid_patterns() < 0)
		return -1;
	if (test_invalid_actions() < 0)
		return -1;
	if (test_query_udp() < 0)
		return -1;
	if (test_query_tcp() < 0)
		return -1;
	if (test_query_sctp() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(flow_classify_autotest, test_flow_classify);
