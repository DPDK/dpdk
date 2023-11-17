/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Microsoft Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_bus_vdev.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_net.h>
#include <rte_pcapng.h>
#include <rte_random.h>
#include <rte_reciprocal.h>
#include <rte_time.h>
#include <rte_udp.h>

#include <pcap/pcap.h>

#include "test.h"

#define PCAPNG_TEST_DEBUG 0

#define TOTAL_PACKETS	4096
#define MAX_BURST	64
#define MAX_GAP_US	100000
#define DUMMY_MBUF_NUM	3

static struct rte_mempool *mp;
static const uint32_t pkt_len = 200;
static uint16_t port_id;
static const char null_dev[] = "net_null0";

/* first mbuf in the packet, should always be at offset 0 */
struct dummy_mbuf {
	struct rte_mbuf mb[DUMMY_MBUF_NUM];
	uint8_t buf[DUMMY_MBUF_NUM][RTE_MBUF_DEFAULT_BUF_SIZE];
};

static void
dummy_mbuf_prep(struct rte_mbuf *mb, uint8_t buf[], uint32_t buf_len,
	uint32_t data_len)
{
	uint32_t i;
	uint8_t *db;

	mb->buf_addr = buf;
	rte_mbuf_iova_set(mb, (uintptr_t)buf);
	mb->buf_len = buf_len;
	rte_mbuf_refcnt_set(mb, 1);

	/* set pool pointer to dummy value, test doesn't use it */
	mb->pool = (void *)buf;

	rte_pktmbuf_reset(mb);
	db = (uint8_t *)rte_pktmbuf_append(mb, data_len);

	for (i = 0; i != data_len; i++)
		db[i] = i;
}

/* Make an IP packet consisting of chain of one packets */
static void
mbuf1_prepare(struct dummy_mbuf *dm, uint32_t plen)
{
	struct {
		struct rte_ether_hdr eth;
		struct rte_ipv4_hdr ip;
		struct rte_udp_hdr udp;
	} pkt = {
		.eth = {
			.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4),
		},
		.ip = {
			.version_ihl = RTE_IPV4_VHL_DEF,
			.time_to_live = 1,
			.next_proto_id = IPPROTO_UDP,
			.src_addr = rte_cpu_to_be_32(RTE_IPV4_LOOPBACK),
			.dst_addr = rte_cpu_to_be_32(RTE_IPV4_BROADCAST),
		},
		.udp = {
			.dst_port = rte_cpu_to_be_16(9), /* Discard port */
		},
	};

	memset(dm, 0, sizeof(*dm));
	dummy_mbuf_prep(&dm->mb[0], dm->buf[0], sizeof(dm->buf[0]), plen);

	rte_eth_random_addr(pkt.eth.src_addr.addr_bytes);
	plen -= sizeof(struct rte_ether_hdr);

	pkt.ip.total_length = rte_cpu_to_be_16(plen);
	pkt.ip.hdr_checksum = rte_ipv4_cksum(&pkt.ip);

	plen -= sizeof(struct rte_ipv4_hdr);
	pkt.udp.src_port = rte_rand();
	pkt.udp.dgram_len = rte_cpu_to_be_16(plen);

	memcpy(rte_pktmbuf_mtod(dm->mb, void *), &pkt, sizeof(pkt));
}

static int
test_setup(void)
{
	port_id = rte_eth_dev_count_avail();

	/* Make a dummy null device to snoop on */
	if (rte_vdev_init(null_dev, NULL) != 0) {
		fprintf(stderr, "Failed to create vdev '%s'\n", null_dev);
		goto fail;
	}

	/* Make a pool for cloned packets */
	mp = rte_pktmbuf_pool_create_by_ops("pcapng_test_pool",
					    MAX_BURST, 0, 0,
					    rte_pcapng_mbuf_size(pkt_len) + 128,
					    SOCKET_ID_ANY, "ring_mp_sc");
	if (mp == NULL) {
		fprintf(stderr, "Cannot create mempool\n");
		goto fail;
	}

	return 0;

fail:
	rte_vdev_uninit(null_dev);
	rte_mempool_free(mp);
	return -1;
}

static int
fill_pcapng_file(rte_pcapng_t *pcapng, unsigned int num_packets)
{
	struct dummy_mbuf mbfs;
	struct rte_mbuf *orig;
	unsigned int burst_size;
	unsigned int count;
	ssize_t len;

	/* make a dummy packet */
	mbuf1_prepare(&mbfs, pkt_len);
	orig  = &mbfs.mb[0];

	for (count = 0; count < num_packets; count += burst_size) {
		struct rte_mbuf *clones[MAX_BURST];
		unsigned int i;

		/* put 1 .. MAX_BURST packets in one write call */
		burst_size = rte_rand_max(MAX_BURST) + 1;
		for (i = 0; i < burst_size; i++) {
			struct rte_mbuf *mc;

			mc = rte_pcapng_copy(port_id, 0, orig, mp, pkt_len,
					     RTE_PCAPNG_DIRECTION_IN, NULL);
			if (mc == NULL) {
				fprintf(stderr, "Cannot copy packet\n");
				return -1;
			}
			clones[i] = mc;
		}

		/* write it to capture file */
		len = rte_pcapng_write_packets(pcapng, clones, burst_size);
		rte_pktmbuf_free_bulk(clones, burst_size);

		if (len <= 0) {
			fprintf(stderr, "Write of packets failed: %s\n",
				rte_strerror(rte_errno));
			return -1;
		}

		/* Leave a small gap between packets to test for time wrap */
		usleep(rte_rand_max(MAX_GAP_US));
	}

	return count;
}

static char *
fmt_time(char *buf, size_t size, uint64_t ts_ns)
{
	time_t sec;
	size_t len;

	sec = ts_ns / NS_PER_S;
	len = strftime(buf, size, "%X", localtime(&sec));
	snprintf(buf + len, size - len, ".%09lu",
		 (unsigned long)(ts_ns % NS_PER_S));

	return buf;
}

/* Context for the pcap_loop callback */
struct pkt_print_ctx {
	pcap_t *pcap;
	unsigned int count;
	uint64_t start_ns;
	uint64_t end_ns;
};

static void
print_packet(uint64_t ts_ns, const struct rte_ether_hdr *eh, size_t len)
{
	char tbuf[128], src[64], dst[64];

	fmt_time(tbuf, sizeof(tbuf), ts_ns);
	rte_ether_format_addr(dst, sizeof(dst), &eh->dst_addr);
	rte_ether_format_addr(src, sizeof(src), &eh->src_addr);
	printf("%s: %s -> %s type %x length %zu\n",
	       tbuf, src, dst, rte_be_to_cpu_16(eh->ether_type), len);
}

/* Callback from pcap_loop used to validate packets in the file */
static void
parse_pcap_packet(u_char *user, const struct pcap_pkthdr *h,
		  const u_char *bytes)
{
	struct pkt_print_ctx *ctx = (struct pkt_print_ctx *)user;
	const struct rte_ether_hdr *eh;
	const struct rte_ipv4_hdr *ip;
	uint64_t ns;

	eh = (const struct rte_ether_hdr *)bytes;
	ip = (const struct rte_ipv4_hdr *)(eh + 1);

	ctx->count += 1;

	/* The pcap library is misleading in reporting timestamp.
	 * packet header struct gives timestamp as a timeval (ie. usec);
	 * but the file is open in nanonsecond mode therefore
	 * the timestamp is really in timespec (ie. nanoseconds).
	 */
	ns = h->ts.tv_sec * NS_PER_S + h->ts.tv_usec;
	if (ns < ctx->start_ns || ns > ctx->end_ns) {
		char tstart[128], tend[128];

		fmt_time(tstart, sizeof(tstart), ctx->start_ns);
		fmt_time(tend, sizeof(tend), ctx->end_ns);
		fprintf(stderr, "Timestamp out of range [%s .. %s]\n",
			tstart, tend);
		goto error;
	}

	if (!rte_is_broadcast_ether_addr(&eh->dst_addr)) {
		fprintf(stderr, "Destination is not broadcast\n");
		goto error;
	}

	if (rte_ipv4_cksum(ip) != 0) {
		fprintf(stderr, "Bad IPv4 checksum\n");
		goto error;
	}

	return;		/* packet is normal */

error:
	print_packet(ns, eh, h->len);

	/* Stop parsing at first error */
	pcap_breakloop(ctx->pcap);
}

static uint64_t
current_timestamp(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return rte_timespec_to_ns(&ts);
}

/*
 * Open the resulting pcapng file with libpcap
 * Would be better to use capinfos from wireshark
 * but that creates an unwanted dependency.
 */
static int
valid_pcapng_file(const char *file_name, uint64_t started, unsigned int expected)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	struct pkt_print_ctx ctx = { };
	int ret;

	ctx.start_ns = started;
	ctx.end_ns = current_timestamp();

	ctx.pcap = pcap_open_offline_with_tstamp_precision(file_name,
							   PCAP_TSTAMP_PRECISION_NANO,
							   errbuf);
	if (ctx.pcap == NULL) {
		fprintf(stderr, "pcap_open_offline('%s') failed: %s\n",
			file_name, errbuf);
		return -1;
	}

	ret = pcap_loop(ctx.pcap, 0, parse_pcap_packet, (u_char *)&ctx);
	if (ret != 0) {
		fprintf(stderr, "pcap_dispatch: failed: %s\n",
			pcap_geterr(ctx.pcap));
	} else if (ctx.count != expected) {
		printf("Only %u packets, expected %u\n",
		       ctx.count, expected);
		ret = -1;
	}

	pcap_close(ctx.pcap);

	return ret;
}

static int
test_add_interface(void)
{
	char file_name[] = "/tmp/pcapng_test_XXXXXX.pcapng";
	static rte_pcapng_t *pcapng;
	int ret, tmp_fd;
	uint64_t now = current_timestamp();

	tmp_fd = mkstemps(file_name, strlen(".pcapng"));
	if (tmp_fd == -1) {
		perror("mkstemps() failure");
		goto fail;
	}
	printf("pcapng: output file %s\n", file_name);

	/* open a test capture file */
	pcapng = rte_pcapng_fdopen(tmp_fd, NULL, NULL, "pcapng_addif", NULL);
	if (pcapng == NULL) {
		fprintf(stderr, "rte_pcapng_fdopen failed\n");
		close(tmp_fd);
		goto fail;
	}

	/* Add interface to the file */
	ret = rte_pcapng_add_interface(pcapng, port_id,
				       NULL, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "can not add port %u\n", port_id);
		goto fail;
	}

	/* Add interface with ifname and ifdescr */
	ret = rte_pcapng_add_interface(pcapng, port_id,
				       "myeth", "Some long description", NULL);
	if (ret < 0) {
		fprintf(stderr, "can not add port %u with ifname\n", port_id);
		goto fail;
	}

	/* Add interface with filter */
	ret = rte_pcapng_add_interface(pcapng, port_id,
				       NULL, NULL, "tcp port 8080");
	if (ret < 0) {
		fprintf(stderr, "can not add port %u with filter\n", port_id);
		goto fail;
	}

	rte_pcapng_close(pcapng);

	ret = valid_pcapng_file(file_name, now, 0);
	/* if test fails want to investigate the file */
	if (ret == 0)
		unlink(file_name);

	return ret;

fail:
	rte_pcapng_close(pcapng);
	return -1;
}

static int
test_write_packets(void)
{
	char file_name[] = "/tmp/pcapng_test_XXXXXX.pcapng";
	static rte_pcapng_t *pcapng;
	int ret, tmp_fd, count;
	uint64_t now = current_timestamp();

	tmp_fd = mkstemps(file_name, strlen(".pcapng"));
	if (tmp_fd == -1) {
		perror("mkstemps() failure");
		goto fail;
	}
	printf("pcapng: output file %s\n", file_name);

	/* open a test capture file */
	pcapng = rte_pcapng_fdopen(tmp_fd, NULL, NULL, "pcapng_test", NULL);
	if (pcapng == NULL) {
		fprintf(stderr, "rte_pcapng_fdopen failed\n");
		close(tmp_fd);
		goto fail;
	}

	/* Add interface to the file */
	ret = rte_pcapng_add_interface(pcapng, port_id,
				       NULL, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "can not add port %u\n", port_id);
		goto fail;
	}

	count = fill_pcapng_file(pcapng, TOTAL_PACKETS);
	if (count < 0)
		goto fail;

	/* write a statistics block */
	ret = rte_pcapng_write_stats(pcapng, port_id,
				     count, 0, "end of test");
	if (ret <= 0) {
		fprintf(stderr, "Write of statistics failed\n");
		goto fail;
	}

	rte_pcapng_close(pcapng);

	ret = valid_pcapng_file(file_name, now, count);
	/* if test fails want to investigate the file */
	if (ret == 0)
		unlink(file_name);

	return ret;

fail:
	rte_pcapng_close(pcapng);
	return -1;
}

static void
test_cleanup(void)
{
	rte_mempool_free(mp);
	rte_vdev_uninit(null_dev);
}

static struct
unit_test_suite test_pcapng_suite  = {
	.setup = test_setup,
	.teardown = test_cleanup,
	.suite_name = "Test Pcapng Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_add_interface),
		TEST_CASE(test_write_packets),
		TEST_CASES_END()
	}
};

static int
test_pcapng(void)
{
	return unit_test_suite_runner(&test_pcapng_suite);
}

REGISTER_FAST_TEST(pcapng_autotest, true, true, test_pcapng);
