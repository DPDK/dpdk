/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_hexdump.h>
#include <rte_ip.h>
#include <rte_ip_frag.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_os_shim.h>
#include <rte_random.h>
#include <rte_udp.h>

#include "test.h"

#define MAX_FLOWS	    (1024 * 32)
#define MAX_BKTS	    MAX_FLOWS
#define MAX_ENTRIES_PER_BKT 16
#define MAX_FRAGMENTS	    RTE_LIBRTE_IP_FRAG_MAX_FRAG
#define MIN_FRAGMENTS	    2
#define MAX_PKTS	    (MAX_FLOWS * MAX_FRAGMENTS)

#define MAX_PKT_LEN 2048
#define MAX_TTL_MS  (5 * MS_PER_S)

/* use RFC863 Discard Protocol */
#define UDP_SRC_PORT 9
#define UDP_DST_PORT 9

/* use RFC5735 / RFC2544 reserved network test addresses */
#define IP_SRC_ADDR(x) ((198U << 24) | (18 << 16) | (0 << 8) | (x))
#define IP_DST_ADDR(x) ((198U << 24) | (18 << 16) | (1 << 15) | (x))

/* 2001:0200::/48 is IANA reserved range for IPv6 benchmarking (RFC5180) */
static uint8_t ip6_addr[16] = {32, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define IP6_VERSION 6

#define IP_DEFTTL 64 /* from RFC 1340. */

static struct rte_ip_frag_tbl *frag_tbl;
static struct rte_mempool *pkt_pool;
static struct rte_mbuf *mbufs[MAX_FLOWS][MAX_FRAGMENTS];
static uint8_t frag_per_flow[MAX_FLOWS];
static uint32_t flow_cnt;

#define FILL_MODE_LINEAR      0
#define FILL_MODE_RANDOM      1
#define FILL_MODE_INTERLEAVED 2

static int
reassembly_test_setup(void)
{
	uint64_t max_ttl_cyc = (MAX_TTL_MS * rte_get_timer_hz()) / 1E3;

	frag_tbl = rte_ip_frag_table_create(MAX_BKTS, MAX_ENTRIES_PER_BKT,
					    MAX_BKTS * MAX_ENTRIES_PER_BKT, max_ttl_cyc,
					    rte_socket_id());
	if (frag_tbl == NULL)
		return TEST_FAILED;

	rte_mbuf_set_user_mempool_ops("ring_mp_mc");
	pkt_pool = rte_pktmbuf_pool_create(
		"reassembly_perf_pool", MAX_FLOWS * MAX_FRAGMENTS, 0, 0,
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (pkt_pool == NULL) {
		printf("[%s] Failed to create pkt pool\n", __func__);
		rte_ip_frag_table_destroy(frag_tbl);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static void
reassembly_test_teardown(void)
{
	if (frag_tbl != NULL)
		rte_ip_frag_table_destroy(frag_tbl);

	if (pkt_pool != NULL)
		rte_mempool_free(pkt_pool);
}

static void
randomize_array_positions(void **array, uint8_t sz)
{
	void *tmp;
	int i, j;

	if (sz == 2) {
		tmp = array[0];
		array[0] = array[1];
		array[1] = tmp;
	} else {
		for (i = sz - 1; i > 0; i--) {
			j = rte_rand_max(i + 1);
			tmp = array[i];
			array[i] = array[j];
			array[j] = tmp;
		}
	}
}

static void
reassembly_print_banner(const char *proto_str)
{
	printf("+=============================================================="
	       "============================================+\n");
	printf("| %-32s| %-3s : %-58d|\n", proto_str, "Flow Count", MAX_FLOWS);
	printf("+================+================+=============+=============+"
	       "========================+===================+\n");
	printf("%-17s%-17s%-14s%-14s%-25s%-20s\n", "| Fragment Order",
	       "| Fragments/Flow", "| Outstanding", "| Cycles/Flow",
	       "| Cycles/Fragment insert", "| Cycles/Reassembly |");
	printf("+================+================+=============+=============+"
	       "========================+===================+\n");
}

static void
ipv4_frag_fill_data(struct rte_mbuf **mbuf, uint8_t nb_frags, uint32_t flow_id,
		    uint8_t fill_mode)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ip_hdr;
	struct rte_udp_hdr *udp_hdr;
	uint16_t frag_len;
	uint8_t i;

	frag_len = MAX_PKT_LEN / nb_frags;
	if (frag_len % 8)
		frag_len = RTE_ALIGN_MUL_CEIL(frag_len, 8);

	for (i = 0; i < nb_frags; i++) {
		struct rte_mbuf *frag = mbuf[i];
		uint16_t frag_offset = 0;
		uint32_t ip_cksum;
		uint16_t pkt_len;
		uint16_t *ptr16;

		frag_offset = i * (frag_len / 8);

		if (i == nb_frags - 1)
			frag_len = MAX_PKT_LEN - (frag_len * (nb_frags - 1));
		else
			frag_offset |= RTE_IPV4_HDR_MF_FLAG;

		rte_pktmbuf_reset_headroom(frag);
		eth_hdr = rte_pktmbuf_mtod(frag, struct rte_ether_hdr *);
		ip_hdr = rte_pktmbuf_mtod_offset(frag, struct rte_ipv4_hdr *,
						 sizeof(struct rte_ether_hdr));
		udp_hdr = rte_pktmbuf_mtod_offset(
			frag, struct rte_udp_hdr *,
			sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_ipv4_hdr));

		rte_ether_unformat_addr("02:00:00:00:00:01",
					&eth_hdr->dst_addr);
		rte_ether_unformat_addr("02:00:00:00:00:00",
					&eth_hdr->src_addr);
		eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

		pkt_len = frag_len;
		/*
		 * Initialize UDP header.
		 */
		if (i == 0) {
			udp_hdr->src_port = rte_cpu_to_be_16(UDP_SRC_PORT);
			udp_hdr->dst_port = rte_cpu_to_be_16(UDP_DST_PORT);
			udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
			udp_hdr->dgram_cksum = 0; /* No UDP checksum. */
		}

		/*
		 * Initialize IP header.
		 */
		pkt_len = (uint16_t)(pkt_len + sizeof(struct rte_ipv4_hdr));
		ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;
		ip_hdr->type_of_service = 0;
		ip_hdr->fragment_offset = rte_cpu_to_be_16(frag_offset);
		ip_hdr->time_to_live = IP_DEFTTL;
		ip_hdr->next_proto_id = IPPROTO_UDP;
		ip_hdr->packet_id =
			rte_cpu_to_be_16((flow_id + 1) % UINT16_MAX);
		ip_hdr->total_length = rte_cpu_to_be_16(pkt_len);
		/* Using more than 32K flows will modify the 2nd octet of the IP. */
		ip_hdr->src_addr = rte_cpu_to_be_32(IP_SRC_ADDR(flow_id));
		ip_hdr->dst_addr = rte_cpu_to_be_32(IP_DST_ADDR(flow_id));

		/*
		 * Compute IP header checksum.
		 */
		ptr16 = (unaligned_uint16_t *)ip_hdr;
		ip_cksum = 0;
		ip_cksum += ptr16[0];
		ip_cksum += ptr16[1];
		ip_cksum += ptr16[2];
		ip_cksum += ptr16[3];
		ip_cksum += ptr16[4];
		ip_cksum += ptr16[6];
		ip_cksum += ptr16[7];
		ip_cksum += ptr16[8];
		ip_cksum += ptr16[9];

		/*
		 * Reduce 32 bit checksum to 16 bits and complement it.
		 */
		ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
			   (ip_cksum & 0x0000FFFF);
		if (ip_cksum > 65535)
			ip_cksum -= 65535;
		ip_cksum = (~ip_cksum) & 0x0000FFFF;
		if (ip_cksum == 0)
			ip_cksum = 0xFFFF;
		ip_hdr->hdr_checksum = (uint16_t)ip_cksum;

		frag->data_len = sizeof(struct rte_ether_hdr) + pkt_len;
		frag->pkt_len = frag->data_len;
		frag->l2_len = sizeof(struct rte_ether_hdr);
		frag->l3_len = sizeof(struct rte_ipv4_hdr);
	}

	if (fill_mode == FILL_MODE_RANDOM)
		randomize_array_positions((void **)mbuf, nb_frags);
}

static uint8_t
get_rand_frags(uint8_t max_frag)
{
	uint8_t frags = rte_rand_max(max_frag + 1);

	return frags <= 1 ? MIN_FRAGMENTS : frags;
}

static int
ipv4_rand_frag_pkt_setup(uint8_t fill_mode, uint8_t max_frag)
{
	uint8_t nb_frag;
	int i;

	for (i = 0; i < MAX_FLOWS; i++) {
		nb_frag = get_rand_frags(max_frag);
		if (rte_mempool_get_bulk(pkt_pool, (void **)mbufs[i], nb_frag) <
		    0)
			return TEST_FAILED;
		ipv4_frag_fill_data(mbufs[i], nb_frag, i, fill_mode);
		frag_per_flow[i] = nb_frag;
	}
	flow_cnt = i;

	return TEST_SUCCESS;
}

static int
ipv4_frag_pkt_setup(uint8_t fill_mode, uint8_t nb_frag)
{
	int i;

	for (i = 0; i < MAX_FLOWS; i++) {
		if (rte_mempool_get_bulk(pkt_pool, (void **)mbufs[i], nb_frag) <
		    0)
			return TEST_FAILED;
		ipv4_frag_fill_data(mbufs[i], nb_frag, i, fill_mode);
		frag_per_flow[i] = nb_frag;
	}
	flow_cnt = i;

	return TEST_SUCCESS;
}

static void
ipv6_frag_fill_data(struct rte_mbuf **mbuf, uint8_t nb_frags, uint32_t flow_id,
		    uint8_t fill_mode)
{
	struct ipv6_extension_fragment *frag_hdr;
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv6_hdr *ip_hdr;
	struct rte_udp_hdr *udp_hdr;
	uint16_t frag_len;
	uint8_t i;

	frag_len = MAX_PKT_LEN / nb_frags;
	if (frag_len % 8)
		frag_len = RTE_ALIGN_MUL_CEIL(frag_len, 8);

	for (i = 0; i < nb_frags; i++) {
		struct rte_mbuf *frag = mbuf[i];
		uint16_t frag_offset = 0;
		uint16_t pkt_len;

		frag_offset = i * (frag_len / 8);
		frag_offset <<= 3;
		if (i == nb_frags - 1) {
			frag_len = MAX_PKT_LEN - (frag_len * (nb_frags - 1));
			frag_offset = RTE_IPV6_SET_FRAG_DATA(frag_offset, 0);
		} else {
			frag_offset = RTE_IPV6_SET_FRAG_DATA(frag_offset, 1);
		}

		rte_pktmbuf_reset_headroom(frag);
		eth_hdr = rte_pktmbuf_mtod(frag, struct rte_ether_hdr *);
		ip_hdr = rte_pktmbuf_mtod_offset(frag, struct rte_ipv6_hdr *,
						 sizeof(struct rte_ether_hdr));
		udp_hdr = rte_pktmbuf_mtod_offset(
			frag, struct rte_udp_hdr *,
			sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_ipv6_hdr) +
				RTE_IPV6_FRAG_HDR_SIZE);
		frag_hdr = rte_pktmbuf_mtod_offset(
			frag, struct ipv6_extension_fragment *,
			sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_ipv6_hdr));

		rte_ether_unformat_addr("02:00:00:00:00:01",
					&eth_hdr->dst_addr);
		rte_ether_unformat_addr("02:00:00:00:00:00",
					&eth_hdr->src_addr);
		eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);

		pkt_len = frag_len;
		/*
		 * Initialize UDP header.
		 */
		if (i == 0) {
			udp_hdr->src_port = rte_cpu_to_be_16(UDP_SRC_PORT);
			udp_hdr->dst_port = rte_cpu_to_be_16(UDP_DST_PORT);
			udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
			udp_hdr->dgram_cksum = 0; /* No UDP checksum. */
		}

		/*
		 * Initialize IP header.
		 */
		pkt_len = (uint16_t)(pkt_len + sizeof(struct rte_ipv6_hdr) +
				     RTE_IPV6_FRAG_HDR_SIZE);
		ip_hdr->vtc_flow = rte_cpu_to_be_32(IP6_VERSION << 28);
		ip_hdr->payload_len =
			rte_cpu_to_be_16(pkt_len - sizeof(struct rte_ipv6_hdr));
		ip_hdr->proto = IPPROTO_FRAGMENT;
		ip_hdr->hop_limits = IP_DEFTTL;
		memcpy(ip_hdr->src_addr, ip6_addr, sizeof(ip_hdr->src_addr));
		memcpy(ip_hdr->dst_addr, ip6_addr, sizeof(ip_hdr->dst_addr));
		ip_hdr->src_addr[7] = (flow_id >> 16) & 0xf;
		ip_hdr->src_addr[7] |= 0x10;
		ip_hdr->src_addr[8] = (flow_id >> 8) & 0xff;
		ip_hdr->src_addr[9] = flow_id & 0xff;

		ip_hdr->dst_addr[7] = (flow_id >> 16) & 0xf;
		ip_hdr->dst_addr[7] |= 0x20;
		ip_hdr->dst_addr[8] = (flow_id >> 8) & 0xff;
		ip_hdr->dst_addr[9] = flow_id & 0xff;

		frag_hdr->next_header = IPPROTO_UDP;
		frag_hdr->reserved = 0;
		frag_hdr->frag_data = rte_cpu_to_be_16(frag_offset);
		frag_hdr->id = rte_cpu_to_be_32(flow_id + 1);

		frag->data_len = sizeof(struct rte_ether_hdr) + pkt_len;
		frag->pkt_len = frag->data_len;
		frag->l2_len = sizeof(struct rte_ether_hdr);
		frag->l3_len =
			sizeof(struct rte_ipv6_hdr) + RTE_IPV6_FRAG_HDR_SIZE;
	}

	if (fill_mode == FILL_MODE_RANDOM)
		randomize_array_positions((void **)mbuf, nb_frags);
}

static int
ipv6_rand_frag_pkt_setup(uint8_t fill_mode, uint8_t max_frag)
{
	uint8_t nb_frag;
	int i;

	for (i = 0; i < MAX_FLOWS; i++) {
		nb_frag = get_rand_frags(max_frag);
		if (rte_mempool_get_bulk(pkt_pool, (void **)mbufs[i], nb_frag) <
		    0)
			return TEST_FAILED;
		ipv6_frag_fill_data(mbufs[i], nb_frag, i, fill_mode);
		frag_per_flow[i] = nb_frag;
	}
	flow_cnt = i;

	return TEST_SUCCESS;
}

static int
ipv6_frag_pkt_setup(uint8_t fill_mode, uint8_t nb_frag)
{
	int i;

	for (i = 0; i < MAX_FLOWS; i++) {
		if (rte_mempool_get_bulk(pkt_pool, (void **)mbufs[i], nb_frag) <
		    0)
			return TEST_FAILED;
		ipv6_frag_fill_data(mbufs[i], nb_frag, i, fill_mode);
		frag_per_flow[i] = nb_frag;
	}
	flow_cnt = i;

	return TEST_SUCCESS;
}

static void
frag_pkt_teardown(void)
{
	uint32_t i;

	for (i = 0; i < flow_cnt; i++)
		rte_pktmbuf_free(mbufs[i][0]);
}

static void
reassembly_print_stats(int8_t nb_frags, uint8_t fill_order,
		       uint32_t outstanding, uint64_t cyc_per_flow,
		       uint64_t cyc_per_frag_insert,
		       uint64_t cyc_per_reassembly)
{
	char frag_str[8], order_str[12];

	if (nb_frags > 0)
		snprintf(frag_str, sizeof(frag_str), "%d", nb_frags);
	else
		snprintf(frag_str, sizeof(frag_str), "RANDOM");

	switch (fill_order) {
	case FILL_MODE_LINEAR:
		snprintf(order_str, sizeof(order_str), "LINEAR");
		break;
	case FILL_MODE_RANDOM:
		snprintf(order_str, sizeof(order_str), "RANDOM");
		break;
	case FILL_MODE_INTERLEAVED:
		snprintf(order_str, sizeof(order_str), "INTERLEAVED");
		break;
	default:
		break;
	}

	printf("| %-14s | %-14s | %-11d | %-11" PRIu64 " | %-22" PRIu64
	       " | %-17" PRIu64 " |\n",
	       order_str, frag_str, outstanding, cyc_per_flow,
	       cyc_per_frag_insert, cyc_per_reassembly);
	printf("+================+================+=============+=============+"
	       "========================+===================+\n");
}

static void
join_array(struct rte_mbuf **dest_arr, struct rte_mbuf **src_arr,
	   uint8_t offset, uint8_t sz)
{
	int i, j;

	for (i = offset, j = 0; j < sz; i++, j++)
		dest_arr[i] = src_arr[j];
}

static int
ipv4_reassembly_perf(int8_t nb_frags, uint8_t fill_order)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j;

	for (i = 0; i < flow_cnt; i++) {
		struct rte_mbuf *buf_out = NULL;
		uint8_t reassembled = 0;

		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < frag_per_flow[i]; j++) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv4_hdr *, buf->l2_len);

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv4_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr);

			if (buf_out == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled = 1;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (!reassembled || buf_out->nb_segs != frag_per_flow[i])
			return TEST_FAILED;
		memset(mbufs[i], 0, sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
		mbufs[i][0] = buf_out;
	}

	reassembly_print_stats(nb_frags, fill_order, 0, total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv4_outstanding_reassembly_perf(int8_t nb_frags, uint8_t fill_order,
				 uint32_t outstanding)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j, k;

	k = outstanding;
	/* Insert outstanding fragments */
	for (i = 0; k && (i < flow_cnt); i++) {
		struct rte_mbuf *buf_out = NULL;

		flow_tstamp = rte_rdtsc_precise();
		for (j = frag_per_flow[i] - 1; j > 0; j--) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv4_hdr *, buf->l2_len);

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv4_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr);
			total_empty_cyc += rte_rdtsc_precise() - tstamp;
			frag_processed++;
			if (buf_out != NULL)
				return TEST_FAILED;

			k--;
		}
		frag_per_flow[i] = 1;
	}

	for (i = 0; i < flow_cnt; i++) {
		struct rte_mbuf *buf_out = NULL;
		uint8_t reassembled = 0;

		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < frag_per_flow[i]; j++) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv4_hdr *, buf->l2_len);

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv4_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr);

			if (buf_out == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled = 1;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (!reassembled)
			return TEST_FAILED;
		memset(mbufs[i], 0, sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
		mbufs[i][0] = buf_out;
	}

	reassembly_print_stats(nb_frags, fill_order, outstanding,
			       total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv4_reassembly_interleaved_flows_perf(uint8_t nb_frags)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j;

	for (i = 0; i < flow_cnt; i += 4) {
		struct rte_mbuf *buf_out[4] = {NULL};
		uint8_t reassembled = 0;
		uint8_t nb_frags = 0;
		uint8_t prev = 0;

		for (j = 0; j < 4; j++)
			nb_frags += frag_per_flow[i + j];

		struct rte_mbuf *buf_arr[nb_frags];
		for (j = 0; j < 4; j++) {
			join_array(buf_arr, mbufs[i + j], prev,
				   frag_per_flow[i + j]);
			prev += frag_per_flow[i + j];
		}
		randomize_array_positions((void **)buf_arr, nb_frags);
		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < nb_frags; j++) {
			struct rte_mbuf *buf = buf_arr[j];
			struct rte_ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv4_hdr *, buf->l2_len);

			tstamp = rte_rdtsc_precise();
			buf_out[reassembled] = rte_ipv4_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr);

			if (buf_out[reassembled] == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled++;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (reassembled != 4)
			return TEST_FAILED;
		for (j = 0; j < 4; j++) {
			memset(mbufs[i + j], 0,
			       sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
			mbufs[i + j][0] = buf_out[j];
		}
	}

	reassembly_print_stats(nb_frags, FILL_MODE_INTERLEAVED, 0,
			       total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv6_reassembly_perf(int8_t nb_frags, uint8_t fill_order)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j;

	for (i = 0; i < flow_cnt; i++) {
		struct rte_mbuf *buf_out = NULL;
		uint8_t reassembled = 0;

		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < frag_per_flow[i]; j++) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv6_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv6_hdr *, buf->l2_len);
			struct ipv6_extension_fragment *frag_hdr =
				rte_pktmbuf_mtod_offset(
					buf, struct ipv6_extension_fragment *,
					buf->l2_len +
						sizeof(struct rte_ipv6_hdr));

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv6_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr,
				frag_hdr);

			if (buf_out == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled = 1;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (!reassembled || buf_out->nb_segs != frag_per_flow[i])
			return TEST_FAILED;
		memset(mbufs[i], 0, sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
		mbufs[i][0] = buf_out;
	}

	reassembly_print_stats(nb_frags, fill_order, 0, total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv6_outstanding_reassembly_perf(int8_t nb_frags, uint8_t fill_order,
				 uint32_t outstanding)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j, k;

	k = outstanding;
	/* Insert outstanding fragments */
	for (i = 0; k && (i < flow_cnt); i++) {
		struct rte_mbuf *buf_out = NULL;

		flow_tstamp = rte_rdtsc_precise();
		for (j = frag_per_flow[i] - 1; j > 0; j--) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv6_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv6_hdr *, buf->l2_len);
			struct ipv6_extension_fragment *frag_hdr =
				rte_pktmbuf_mtod_offset(
					buf, struct ipv6_extension_fragment *,
					buf->l2_len +
						sizeof(struct rte_ipv6_hdr));

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv6_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr,
				frag_hdr);
			total_empty_cyc += rte_rdtsc_precise() - tstamp;
			frag_processed++;

			if (buf_out != NULL)
				return TEST_FAILED;

			k--;
		}
		frag_per_flow[i] = 1;
	}

	for (i = 0; i < flow_cnt; i++) {
		struct rte_mbuf *buf_out = NULL;
		uint8_t reassembled = 0;

		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < frag_per_flow[i]; j++) {
			struct rte_mbuf *buf = mbufs[i][j];
			struct rte_ipv6_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv6_hdr *, buf->l2_len);
			struct ipv6_extension_fragment *frag_hdr =
				rte_pktmbuf_mtod_offset(
					buf, struct ipv6_extension_fragment *,
					buf->l2_len +
						sizeof(struct rte_ipv6_hdr));

			tstamp = rte_rdtsc_precise();
			buf_out = rte_ipv6_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr,
				frag_hdr);

			if (buf_out == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled = 1;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (!reassembled)
			return TEST_FAILED;
		memset(mbufs[i], 0, sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
		mbufs[i][0] = buf_out;
	}

	reassembly_print_stats(nb_frags, fill_order, outstanding,
			       total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv6_reassembly_interleaved_flows_perf(int8_t nb_frags)
{
	struct rte_ip_frag_death_row death_row;
	uint64_t total_reassembled_cyc = 0;
	uint64_t total_empty_cyc = 0;
	uint64_t tstamp, flow_tstamp;
	uint64_t frag_processed = 0;
	uint64_t total_cyc = 0;
	uint32_t i, j;

	for (i = 0; i < flow_cnt; i += 4) {
		struct rte_mbuf *buf_out[4] = {NULL};
		uint8_t reassembled = 0;
		uint8_t nb_frags = 0;
		uint8_t prev = 0;

		for (j = 0; j < 4; j++)
			nb_frags += frag_per_flow[i + j];

		struct rte_mbuf *buf_arr[nb_frags];
		for (j = 0; j < 4; j++) {
			join_array(buf_arr, mbufs[i + j], prev,
				   frag_per_flow[i + j]);
			prev += frag_per_flow[i + j];
		}
		randomize_array_positions((void **)buf_arr, nb_frags);
		flow_tstamp = rte_rdtsc_precise();
		for (j = 0; j < nb_frags; j++) {
			struct rte_mbuf *buf = buf_arr[j];
			struct rte_ipv6_hdr *ip_hdr = rte_pktmbuf_mtod_offset(
				buf, struct rte_ipv6_hdr *, buf->l2_len);
			struct ipv6_extension_fragment *frag_hdr =
				rte_pktmbuf_mtod_offset(
					buf, struct ipv6_extension_fragment *,
					buf->l2_len +
						sizeof(struct rte_ipv6_hdr));

			tstamp = rte_rdtsc_precise();
			buf_out[reassembled] = rte_ipv6_frag_reassemble_packet(
				frag_tbl, &death_row, buf, flow_tstamp, ip_hdr,
				frag_hdr);

			if (buf_out[reassembled] == NULL) {
				total_empty_cyc += rte_rdtsc_precise() - tstamp;
				frag_processed++;
				continue;
			} else {
				/*Packet out*/
				total_reassembled_cyc +=
					rte_rdtsc_precise() - tstamp;
				reassembled++;
			}
		}
		total_cyc += rte_rdtsc_precise() - flow_tstamp;
		if (reassembled != 4)
			return TEST_FAILED;
		for (j = 0; j < 4; j++) {
			memset(mbufs[i + j], 0,
			       sizeof(struct rte_mbuf *) * MAX_FRAGMENTS);
			mbufs[i + j][0] = buf_out[j];
		}
	}

	reassembly_print_stats(nb_frags, FILL_MODE_INTERLEAVED, 0,
			       total_cyc / flow_cnt,
			       total_empty_cyc / frag_processed,
			       total_reassembled_cyc / flow_cnt);

	return TEST_SUCCESS;
}

static int
ipv4_reassembly_test(int8_t nb_frags, uint8_t fill_order, uint32_t outstanding)
{
	int rc;

	if (nb_frags > 0)
		rc = ipv4_frag_pkt_setup(fill_order, nb_frags);
	else
		rc = ipv4_rand_frag_pkt_setup(fill_order, MAX_FRAGMENTS);

	if (rc)
		return rc;

	if (outstanding)
		rc = ipv4_outstanding_reassembly_perf(nb_frags, fill_order,
						      outstanding);
	else if (fill_order == FILL_MODE_INTERLEAVED)
		rc = ipv4_reassembly_interleaved_flows_perf(nb_frags);
	else
		rc = ipv4_reassembly_perf(nb_frags, fill_order);

	frag_pkt_teardown();

	return rc;
}

static int
ipv6_reassembly_test(int8_t nb_frags, uint8_t fill_order, uint32_t outstanding)
{
	int rc;

	if (nb_frags > 0)
		rc = ipv6_frag_pkt_setup(fill_order, nb_frags);
	else
		rc = ipv6_rand_frag_pkt_setup(fill_order, MAX_FRAGMENTS);

	if (rc)
		return rc;

	if (outstanding)
		rc = ipv6_outstanding_reassembly_perf(nb_frags, fill_order,
						      outstanding);
	else if (fill_order == FILL_MODE_INTERLEAVED)
		rc = ipv6_reassembly_interleaved_flows_perf(nb_frags);
	else
		rc = ipv6_reassembly_perf(nb_frags, fill_order);

	frag_pkt_teardown();

	return rc;
}

static int
test_reassembly_perf(void)
{
	int8_t nb_fragments[] = {2, 3, MAX_FRAGMENTS, -1 /* Random */};
	uint8_t order_type[] = {FILL_MODE_LINEAR, FILL_MODE_RANDOM};
	uint32_t outstanding[] = {100, 500, 1000, 2000, 3000};
	uint32_t i, j;
	int rc;

	rc = reassembly_test_setup();
	if (rc)
		return rc;

	reassembly_print_banner("IPV4");
	/* Test variable fragment count and ordering. */
	for (i = 0; i < RTE_DIM(nb_fragments); i++) {
		for (j = 0; j < RTE_DIM(order_type); j++) {
			rc = ipv4_reassembly_test(nb_fragments[i],
						  order_type[j], 0);
			if (rc)
				return rc;
		}
	}

	/* Test outstanding fragments in the table. */
	for (i = 0; i < RTE_DIM(outstanding); i++) {
		rc = ipv4_reassembly_test(2, 0, outstanding[i]);
		if (rc)
			return rc;
	}
	for (i = 0; i < RTE_DIM(outstanding); i++) {
		rc = ipv4_reassembly_test(MAX_FRAGMENTS, 0, outstanding[i]);
		if (rc)
			return rc;
	}

	/* Test interleaved flow reassembly perf */
	for (i = 0; i < RTE_DIM(nb_fragments); i++) {
		rc = ipv4_reassembly_test(nb_fragments[i],
					  FILL_MODE_INTERLEAVED, 0);
		if (rc)
			return rc;
	}
	printf("\n");
	reassembly_print_banner("IPV6");
	/* Test variable fragment count and ordering. */
	for (i = 0; i < RTE_DIM(nb_fragments); i++) {
		for (j = 0; j < RTE_DIM(order_type); j++) {
			rc = ipv6_reassembly_test(nb_fragments[i],
						  order_type[j], 0);
			if (rc)
				return rc;
		}
	}

	/* Test outstanding fragments in the table. */
	for (i = 0; i < RTE_DIM(outstanding); i++) {
		rc = ipv6_reassembly_test(2, 0, outstanding[i]);
		if (rc)
			return rc;
	}

	for (i = 0; i < RTE_DIM(outstanding); i++) {
		rc = ipv6_reassembly_test(MAX_FRAGMENTS, 0, outstanding[i]);
		if (rc)
			return rc;
	}

	/* Test interleaved flow reassembly perf */
	for (i = 0; i < RTE_DIM(nb_fragments); i++) {
		rc = ipv6_reassembly_test(nb_fragments[i],
					  FILL_MODE_INTERLEAVED, 0);
		if (rc)
			return rc;
	}
	reassembly_test_teardown();

	return TEST_SUCCESS;
}

REGISTER_PERF_TEST(reassembly_perf_autotest, test_reassembly_perf);
