/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Red Hat, Inc.
 */

#include <time.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hexdump.h>
#include <rte_ip.h>
#include <rte_ip_frag.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_random.h>

#include "test.h"

#define NUM_MBUFS 128
#define BURST 32

static struct rte_mempool *pkt_pool,
			  *direct_pool,
			  *indirect_pool;

static int
setup_buf_pool(void)
{
	pkt_pool = rte_pktmbuf_pool_create("FRAG_MBUF_POOL",
					   NUM_MBUFS, BURST, 0,
					   RTE_MBUF_DEFAULT_BUF_SIZE,
					   SOCKET_ID_ANY);
	if (pkt_pool == NULL) {
		printf("%s: Error creating pkt mempool\n", __func__);
		goto bad_setup;
	}

	direct_pool = rte_pktmbuf_pool_create("FRAG_D_MBUF_POOL",
					      NUM_MBUFS, BURST, 0,
					      RTE_MBUF_DEFAULT_BUF_SIZE,
					      SOCKET_ID_ANY);
	if (direct_pool == NULL) {
		printf("%s: Error creating direct mempool\n", __func__);
		goto bad_setup;
	}

	indirect_pool = rte_pktmbuf_pool_create("FRAG_I_MBUF_POOL",
						NUM_MBUFS, BURST, 0,
						0, SOCKET_ID_ANY);
	if (indirect_pool == NULL) {
		printf("%s: Error creating indirect mempool\n", __func__);
		goto bad_setup;
	}

	return TEST_SUCCESS;

bad_setup:
	rte_mempool_free(pkt_pool);
	pkt_pool = NULL;

	rte_mempool_free(direct_pool);
	direct_pool = NULL;

	return TEST_FAILED;
}

static int testsuite_setup(void)
{
	return setup_buf_pool();
}

static void testsuite_teardown(void)
{
	rte_mempool_free(pkt_pool);
	rte_mempool_free(direct_pool);
	rte_mempool_free(indirect_pool);

	pkt_pool = NULL;
	direct_pool = NULL;
	indirect_pool = NULL;
}

static int ut_setup(void)
{
	return TEST_SUCCESS;
}

static void ut_teardown(void)
{
}

static void
v4_allocate_packet_of(struct rte_mbuf *b, int fill, size_t s, int df,
		      uint8_t ttl, uint8_t proto, uint16_t pktid)
{
	/* Create a packet, 2k bytes long */
	b->data_off = 0;
	char *data = rte_pktmbuf_mtod(b, char *);

	memset(data, fill, sizeof(struct rte_ipv4_hdr) + s);

	struct rte_ipv4_hdr *hdr = (struct rte_ipv4_hdr *)data;

	hdr->version_ihl = 0x45; /* standard IP header... */
	hdr->type_of_service = 0;
	b->pkt_len = s + sizeof(struct rte_ipv4_hdr);
	b->data_len = b->pkt_len;
	hdr->total_length = rte_cpu_to_be_16(b->pkt_len);
	hdr->packet_id = rte_cpu_to_be_16(pktid);
	hdr->fragment_offset = 0;
	if (df)
		hdr->fragment_offset = rte_cpu_to_be_16(0x4000);

	if (!ttl)
		ttl = 64; /* default to 64 */

	if (!proto)
		proto = 1; /* icmp */

	hdr->time_to_live = ttl;
	hdr->next_proto_id = proto;
	hdr->hdr_checksum = 0;
	hdr->src_addr = rte_cpu_to_be_32(0x8080808);
	hdr->dst_addr = rte_cpu_to_be_32(0x8080404);
}

static void
v6_allocate_packet_of(struct rte_mbuf *b, int fill, size_t s, uint8_t ttl,
		      uint8_t proto, uint16_t pktid)
{
	/* Create a packet, 2k bytes long */
	b->data_off = 0;
	char *data = rte_pktmbuf_mtod(b, char *);

	memset(data, fill, sizeof(struct rte_ipv6_hdr) + s);

	struct rte_ipv6_hdr *hdr = (struct rte_ipv6_hdr *)data;
	b->pkt_len = s + sizeof(struct rte_ipv6_hdr);
	b->data_len = b->pkt_len;

	/* basic v6 header */
	hdr->vtc_flow = rte_cpu_to_be_32(0x60 << 24 | pktid);
	hdr->payload_len = rte_cpu_to_be_16(b->pkt_len);
	hdr->proto = proto;
	hdr->hop_limits = ttl;

	memset(hdr->src_addr, 0x08, sizeof(hdr->src_addr));
	memset(hdr->dst_addr, 0x04, sizeof(hdr->src_addr));
}

static inline void
test_free_fragments(struct rte_mbuf *mb[], uint32_t num)
{
	uint32_t i;
	for (i = 0; i < num; i++)
		rte_pktmbuf_free(mb[i]);
}

static int
test_ip_frag(void)
{
	static const uint16_t RND_ID = UINT16_MAX;
	int result = TEST_SUCCESS;
	size_t i;

	struct test_ip_frags {
		int      ipv;
		size_t   mtu_size;
		size_t   pkt_size;
		int      set_df;
		uint8_t  ttl;
		uint8_t  proto;
		uint16_t pkt_id;
		int      expected_frags;
	} tests[] = {
		     {4, 1280, 1400, 0, 64, IPPROTO_ICMP, RND_ID, 2},
		     {4, 1280, 1400, 0, 64, IPPROTO_ICMP, 0,      2},
		     {4,  600, 1400, 0, 64, IPPROTO_ICMP, RND_ID, 3},
		     {4,    4, 1400, 0, 64, IPPROTO_ICMP, RND_ID, -EINVAL},
		     {4,  600, 1400, 1, 64, IPPROTO_ICMP, RND_ID, -ENOTSUP},
		     {4,  600, 1400, 0,  0, IPPROTO_ICMP, RND_ID, 3},

		     {6, 1280, 1400, 0, 64, IPPROTO_ICMP, RND_ID, 2},
		     {6, 1300, 1400, 0, 64, IPPROTO_ICMP, RND_ID, 2},
		     {6,    4, 1400, 0, 64, IPPROTO_ICMP, RND_ID, -EINVAL},
		     {6, 1300, 1400, 0,  0, IPPROTO_ICMP, RND_ID, 2},
	};

	for (i = 0; i < RTE_DIM(tests); i++) {
		int32_t len = 0;
		uint16_t pktid = tests[i].pkt_id;
		struct rte_mbuf *pkts_out[BURST];
		struct rte_mbuf *b = rte_pktmbuf_alloc(pkt_pool);

		RTE_TEST_ASSERT_NOT_EQUAL(b, NULL,
					  "Failed to allocate pkt.");

		if (tests[i].pkt_id == RND_ID)
			pktid = rte_rand_max(UINT16_MAX);

		if (tests[i].ipv == 4) {
			v4_allocate_packet_of(b, 0x41414141,
					      tests[i].pkt_size,
					      tests[i].set_df,
					      tests[i].ttl,
					      tests[i].proto,
					      pktid);
		} else if (tests[i].ipv == 6) {
			v6_allocate_packet_of(b, 0x41414141,
					      tests[i].pkt_size,
					      tests[i].ttl,
					      tests[i].proto,
					      pktid);
		}

		if (tests[i].ipv == 4)
			len = rte_ipv4_fragment_packet(b, pkts_out, BURST,
						       tests[i].mtu_size,
						       direct_pool,
						       indirect_pool);
		else if (tests[i].ipv == 6)
			len = rte_ipv6_fragment_packet(b, pkts_out, BURST,
						       tests[i].mtu_size,
						       direct_pool,
						       indirect_pool);

		rte_pktmbuf_free(b);

		if (len > 0)
			test_free_fragments(pkts_out, len);

		printf("%zd: checking %d with %d\n", i, len,
		       tests[i].expected_frags);
		RTE_TEST_ASSERT_EQUAL(len, tests[i].expected_frags,
				      "Failed case %zd.\n", i);

	}

	return result;
}

static struct unit_test_suite ipfrag_testsuite  = {
	.suite_name = "IP Frag Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			     test_ip_frag),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_ipfrag(void)
{
	rte_log_set_global_level(RTE_LOG_DEBUG);
	rte_log_set_level(RTE_LOGTYPE_EAL, RTE_LOG_DEBUG);

	return unit_test_suite_runner(&ipfrag_testsuite);
}

REGISTER_TEST_COMMAND(ipfrag_autotest, test_ipfrag);
