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
struct flow_classifier *cls;

struct flow_classifier {
	struct rte_flow_classifier *cls;
	uint32_t table_id[RTE_FLOW_CLASSIFY_TABLE_MAX];
	uint32_t n_tables;
};

struct flow_classifier_acl {
	struct flow_classifier cls;
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

	rule = rte_flow_classify_table_entry_add(NULL, 1, NULL, NULL, NULL,
			NULL, NULL);
	if (rule) {
		printf("Line %i: flow_classifier_table_entry_add", __LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(NULL, 1, NULL);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(NULL, 1, NULL, 0, NULL, NULL);
	if (!ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" with NULL param should have failed!\n");
		return -1;
	}

	rule = rte_flow_classify_table_entry_add(NULL, 1, NULL, NULL, NULL,
		NULL, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add ", __LINE__);
		printf("with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(NULL, 1, NULL);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf("with NULL param should have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(NULL, 1, NULL, 0, NULL, NULL);
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
	 * set up parameters for rte_flow_classify_table_entry_add and
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

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	 * set up parameters for rte_flow_classify_table_entry_add and
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
	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	pattern[1] = ipv4_udp_item_1;
	pattern[2] = udp_item_bad;
	pattern[3] = end_item_bad;
	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	 * set up parameters for rte_flow_classify_table_entry_add and
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

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
	if (!ret) {
		printf("Line %i: rte_flow_classify_table_entry_delete",
			__LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	actions[0] = count_action;
	actions[1] = end_action_bad;

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	 * set up parameters for rte_flow_classify_table_entry_add and
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

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, 0, bufs, MAX_PKT_BURST,
			rule, &udp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	 * set up parameters for rte_flow_classify_table_entry_add and
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

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, 0, bufs, MAX_PKT_BURST,
			rule, &tcp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	 * set up parameters rte_flow_classify_table_entry_add and
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

	rule = rte_flow_classify_table_entry_add(cls->cls, 0, &key_found,
			&attr, pattern, actions, &error);
	if (!rule) {
		printf("Line %i: flow_classify_table_entry_add", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classifier_query(cls->cls, 0, bufs, MAX_PKT_BURST,
			rule, &sctp_classify_stats);
	if (ret) {
		printf("Line %i: flow_classifier_query", __LINE__);
		printf(" should not have failed!\n");
		return -1;
	}

	ret = rte_flow_classify_table_entry_delete(cls->cls, 0, rule);
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
	int socket_id;
	int ret;
	uint32_t size;

	socket_id = rte_eth_dev_socket_id(0);

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct flow_classifier_acl));
	cls = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);

	cls_params.name = "flow_classifier";
	cls_params.socket_id = socket_id;
	cls_params.type = RTE_FLOW_CLASSIFY_TABLE_TYPE_ACL;
	cls->cls = rte_flow_classifier_create(&cls_params);

	/* initialise ACL table params */
	table_acl_params.n_rule_fields = RTE_DIM(ipv4_defs);
	table_acl_params.name = "table_acl_ipv4_5tuple";
	table_acl_params.n_rules = FLOW_CLASSIFY_MAX_RULE_NUM;
	memcpy(table_acl_params.field_format, ipv4_defs, sizeof(ipv4_defs));

	/* initialise table create params */
	cls_table_params.ops = &rte_table_acl_ops,
	cls_table_params.arg_create = &table_acl_params,

	ret = rte_flow_classify_table_create(cls->cls, &cls_table_params,
			&cls->table_id[0]);
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
