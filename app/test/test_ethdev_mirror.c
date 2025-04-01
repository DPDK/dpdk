/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Stephen Hemminger <stephen@networkplumber.org>
 */
#include "test.h"

#include <stdio.h>

#include <rte_eth_ring.h>
#include <rte_ethdev.h>
#include <rte_bus_vdev.h>

#define SOCKET0 0
#define RING_SIZE 256
#define BURST_SZ 64
#define NB_MBUF 512

static struct rte_mempool *mirror_pool;
static const uint32_t pkt_len = 200;

static struct rte_ring *rxtx_ring;
static const char ring_dev[] = "net_ring0";
static uint16_t ring_port;
static const char null_dev[] = "net_null0";
static uint16_t null_port;

static int
configure_port(uint16_t port, const char *name)
{
	struct rte_eth_conf eth_conf = { 0 };

	if (rte_eth_dev_configure(port, 1, 1, &eth_conf) < 0) {
		fprintf(stderr, "Configure failed for port %u: %s\n", port, name);
		return -1;
	}

	/* only single queue */
	if (rte_eth_tx_queue_setup(port, 0, RING_SIZE, SOCKET0, NULL) < 0) {
		fprintf(stderr, "TX queue setup failed port %u: %s\n", port, name);
		return -1;
	}

	if (rte_eth_rx_queue_setup(port, 0, RING_SIZE, SOCKET0, NULL, mirror_pool) < 0) {
		fprintf(stderr, "RX queue setup failed port %u: %s\n", port, name);
		return -1;
	}

	if (rte_eth_dev_start(port) < 0) {
		fprintf(stderr, "Error starting port %u:%s\n", port, name);
		return -1;
	}

	return 0;
}

static void
test_cleanup(void)
{
	rte_ring_free(rxtx_ring);

	rte_vdev_uninit("net_ring");
	rte_vdev_uninit("net_null");

	rte_mempool_free(mirror_pool);
}

/* Make two virtual devices ring and null */
static int
test_setup(void)
{
	char null_name[] = "net_null0";
	int ret;

	/* ring must support multiple enqueue for mirror to work */
	rxtx_ring = rte_ring_create("R0", RING_SIZE, SOCKET0, RING_F_MP_RTS_ENQ | RING_F_SC_DEQ);
	if (rxtx_ring == NULL) {
		fprintf(stderr, "rte_ring_create R0 failed\n");
		goto fail;
	}
	ret = rte_eth_from_rings(ring_dev, &rxtx_ring, 1, &rxtx_ring, 1, SOCKET0);
	if (ret < 0) {
		fprintf(stderr, "rte_eth_from_rings failed\n");
		goto fail;
	}
	ring_port = ret;

	/* Make a dummy null device to snoop on */
	if (rte_vdev_init(null_dev, NULL) != 0) {
		fprintf(stderr, "Failed to create vdev '%s'\n", null_dev);
		goto fail;
	}
	if (rte_eth_dev_get_port_by_name(null_dev, &null_port) != 0) {
		fprintf(stderr, "cannot find added vdev %s\n", null_name);
		goto fail;
	}

	mirror_pool = rte_pktmbuf_pool_create("mirror_pool", NB_MBUF, 32,
					      0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mirror_pool == NULL) {
		fprintf(stderr, "rte_pktmbuf_pool_create failed\n");
		goto fail;
	}

	ret = configure_port(ring_port, ring_dev);
	if (ret < 0)
		goto fail;

	ret = configure_port(null_port, null_dev);
	if (ret < 0)
		goto fail;

	return 0;
fail:
	test_cleanup();
	return -1;
}

/* Make sure mirror API checks args */
static int32_t
ethdev_mirror_api(void)
{
	struct rte_eth_mirror_conf conf = {
		.mp = mirror_pool,
		.direction = RTE_ETH_MIRROR_DIRECTION_EGRESS,
	};
	uint16_t invalid_port = RTE_MAX_ETHPORTS;
	int ret;

	conf.target = null_port;
	ret = rte_eth_add_mirror(invalid_port, &conf);
	TEST_ASSERT(ret != 0, "Created mirror from invalid port");

	conf.target = invalid_port;
	ret = rte_eth_add_mirror(null_port, &conf);
	TEST_ASSERT(ret != 0, "Created mirror to invalid port");

	conf.direction = 0;
	conf.target = ring_port;
	ret = rte_eth_add_mirror(null_port, &conf);
	TEST_ASSERT(ret != 0, "Created mirror with invalid direction");

	conf.direction = RTE_ETH_MIRROR_DIRECTION_INGRESS;
	conf.target = ring_port;
	ret = rte_eth_add_mirror(ring_port, &conf);
	TEST_ASSERT(ret != 0, "Created mirror to self");

	conf.target = null_port;
	ret = rte_eth_add_mirror(ring_port, &conf);
	TEST_ASSERT(ret == 0, "Could not create mirror from ring to null");

	ret = rte_eth_add_mirror(ring_port, &conf);
	TEST_ASSERT(ret != 0, "Able to create duplicate mirror");

	ret = rte_eth_remove_mirror(ring_port, null_port);
	TEST_ASSERT(ret == 0, "Unable to delete mirror");

	ret = rte_eth_remove_mirror(ring_port, null_port);
	TEST_ASSERT(ret != 0, "Able to delete port without mirror");

	return 0;
}

static int
init_mbuf(struct rte_mbuf *m, uint16_t id)
{
	struct {
		struct rte_ether_hdr eth;
		struct rte_ipv4_hdr ip;
		struct rte_udp_hdr udp;
	} hdrs = {
		.eth = {
			.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
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

	rte_eth_random_addr(hdrs.eth.src_addr.addr_bytes);
	uint16_t plen = pkt_len - sizeof(struct rte_ether_hdr);

	hdrs.ip.packet_id = rte_cpu_to_be_16(id);
	hdrs.ip.total_length = rte_cpu_to_be_16(plen);
	hdrs.ip.hdr_checksum = rte_ipv4_cksum(&hdrs.ip);

	plen -= sizeof(struct rte_ipv4_hdr);
	hdrs.udp.src_port = rte_rand();
	hdrs.udp.dgram_len = rte_cpu_to_be_16(plen);

	void *data = rte_pktmbuf_append(m, pkt_len);
	TEST_ASSERT(data != NULL, "could not add header");

	memcpy(data, &hdrs, sizeof(hdrs));
	return 0;
}

static int
init_burst(struct rte_mbuf *pkts[], unsigned int n)
{
	for (unsigned int i = 0; i < n; i++) {
		if (init_mbuf(pkts[i], i) < 0)
			return -1;
	}
	return 0;
}

static int
validate_burst(struct rte_mbuf *pkts[], unsigned int n)
{
	for (unsigned int i = 0; i < n; i++) {
		struct rte_mbuf *m = pkts[i];

		rte_mbuf_sanity_check(m, 1);
		TEST_ASSERT(m->pkt_len != pkt_len,
			    "mirror packet not right len");

		const struct rte_ether_hdr *eh
			= rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
		TEST_ASSERT(rte_is_broadcast_ether_addr(&eh->dst_addr),
			    "mirrored packet is not broadcast");

	}
	return 0;
}

static int32_t
ethdev_mirror_packets(void)
{
	struct rte_eth_mirror_conf conf = {
		.mp = mirror_pool,
		.target = ring_port,
		.direction = RTE_ETH_MIRROR_DIRECTION_EGRESS,
	};
	struct rte_mbuf *tx_pkts[BURST_SZ], *rx_pkts[BURST_SZ];
	uint16_t nb_tx, nb_rx;
	int ret;

	ret = rte_pktmbuf_alloc_bulk(mirror_pool, tx_pkts, BURST_SZ);
	TEST_ASSERT(ret == 0, "Could not allocate mbufs");

	ret = init_burst(tx_pkts, BURST_SZ);
	TEST_ASSERT(ret == 0, "Init mbufs failed");

	ret = rte_eth_add_mirror(null_port, &conf);
	TEST_ASSERT(ret == 0, "Could not create mirror from ring to null");

	nb_tx = rte_eth_tx_burst(null_port, 0, tx_pkts, BURST_SZ);
	TEST_ASSERT(nb_tx == BURST_SZ, "Only sent %u burst to null (vs %u)",
		    nb_tx, BURST_SZ);

	nb_rx = rte_eth_rx_burst(ring_port, 0, rx_pkts, BURST_SZ);
	TEST_ASSERT(nb_rx == BURST_SZ, "Only received %u of %u packets",
		    nb_rx, BURST_SZ);

	validate_burst(rx_pkts, nb_rx);
	rte_pktmbuf_free_bulk(rx_pkts, nb_rx);

	return 0;
}

static struct unit_test_suite ethdev_mirror_suite = {
	.suite_name = "port mirroring",
	.setup = test_setup,
	.teardown = test_cleanup,
	.unit_test_cases = {
		TEST_CASE(ethdev_mirror_api),
		TEST_CASE(ethdev_mirror_packets),
		TEST_CASES_END()
	}
};

static int
test_ethdev_mirror(void)
{
	return unit_test_suite_runner(&ethdev_mirror_suite);
}

REGISTER_FAST_TEST(ethdev_mirror, true, true, test_ethdev_mirror);
