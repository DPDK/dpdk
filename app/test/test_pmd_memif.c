/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Ericsson AB
 */

/*
 * Basic test of memif PMD functionality.
 * Based off of the TAP PMD test (test_pmd_tap.c).
 *
 * memif connects two endpoints over shared memory. Unlike a NIC or TAP
 * device, a memif port is only usable once its peer connects, so the test
 * creates a server and a client memif port in this one process, connected
 * over an abstract-namespace socket (the loopback setup documented in
 * doc/guides/nics/memif.rst). The control-channel handshake is asynchronous,
 * so the test waits for both ports to report link up before exercising the
 * data path.
 */

#include "test.h"

#include <stdio.h>

#ifndef RTE_EXEC_ENV_LINUX

/* memif PMD is only available on Linux */
static int
test_pmd_memif(void)
{
	printf("memif PMD not supported on this platform, skipping test\n");
	return TEST_SKIPPED;
}

#else /* RTE_EXEC_ENV_LINUX */

#include <string.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_bus_vdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#define SOCKET0 0
#define RING_SIZE 256
#define NB_MBUF 4096
#define MAX_PKT_BURST 32
#define PKT_LEN 64

/* memif ignores nb_tx_desc; the ring size is set via the rsize arg. */
#define MEMIF_LOG2_RING 8

/*
 * Same socket name and id on both ports makes them a connected pair.
 * The small-bsize pair forces chained descriptors; the default-bsize pair
 * exercises the single-pool Tx fast path.
 */
#define MEMIF_SRV_NAME "net_memif_test_srv"
#define MEMIF_CLI_NAME "net_memif_test_cli"
#define MEMIF_SRV_ARGS "role=server,id=0,bsize=512,rsize=8,socket-abstract=yes,socket=memif-autotest"
#define MEMIF_CLI_ARGS "role=client,id=0,bsize=512,rsize=8,socket-abstract=yes,socket=memif-autotest"
#define MEMIF_FP_SRV_NAME "net_memif_test_fsrv"
#define MEMIF_FP_CLI_NAME "net_memif_test_fcli"
#define MEMIF_FP_SRV_ARGS "role=server,id=2,socket-abstract=yes,socket=memif-autotest-fp"
#define MEMIF_FP_CLI_ARGS "role=client,id=2,socket-abstract=yes,socket=memif-autotest-fp"

/* Time to wait for the asynchronous control-channel handshake. */
#define CONNECT_TRIES 50
#define CONNECT_DELAY_US 100000
/* Shorter window for a negative check (peer must NOT connect). */
#define REJECT_TRIES 10

static struct rte_mempool *mp;
static int memif_srv_port = -1;
static int memif_cli_port = -1;
static int memif_fp_srv_port = -1;
static int memif_fp_cli_port = -1;

static int
find_port(const char *frag)
{
	uint16_t port_id;
	char name[RTE_ETH_NAME_MAX_LEN];

	RTE_ETH_FOREACH_DEV(port_id) {
		if (rte_eth_dev_get_name_by_port(port_id, name) == 0 &&
		    strstr(name, frag) != NULL)
			return port_id;
	}
	return -1;
}

static int
configure_port(int port)
{
	struct rte_eth_conf port_conf;
	int ret;

	memset(&port_conf, 0, sizeof(port_conf));

	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret < 0) {
		printf("Configure failed for port %d: %s\n",
		       port, rte_strerror(-ret));
		return -1;
	}

	ret = rte_eth_tx_queue_setup(port, 0, RING_SIZE, SOCKET0, NULL);
	if (ret < 0) {
		printf("TX queue setup failed for port %d: %s\n",
		       port, rte_strerror(-ret));
		return -1;
	}

	ret = rte_eth_rx_queue_setup(port, 0, RING_SIZE, SOCKET0, NULL, mp);
	if (ret < 0) {
		printf("RX queue setup failed for port %d: %s\n",
		       port, rte_strerror(-ret));
		return -1;
	}

	ret = rte_eth_dev_start(port);
	if (ret < 0) {
		printf("Error starting port %d: %s\n",
		       port, rte_strerror(-ret));
		return -1;
	}

	return 0;
}

/* Wait until both ports report link up, i.e. the peers have connected. */
static int
wait_for_link_pair_tries(int p0, int p1, int tries)
{
	struct rte_eth_link l0, l1;
	int i;

	for (i = 0; i < tries; i++) {
		if (rte_eth_link_get_nowait(p0, &l0) == 0 &&
		    rte_eth_link_get_nowait(p1, &l1) == 0 &&
		    l0.link_status == RTE_ETH_LINK_UP &&
		    l1.link_status == RTE_ETH_LINK_UP)
			return 0;
		usleep(CONNECT_DELAY_US);
	}
	return -1;
}

static int
wait_for_link_pair(int p0, int p1)
{
	return wait_for_link_pair_tries(p0, p1, CONNECT_TRIES);
}

static int
wait_for_link(void)
{
	return wait_for_link_pair(memif_srv_port, memif_cli_port);
}

static struct rte_mbuf *
create_test_packet(uint16_t pkt_len, uint8_t seed)
{
	struct rte_mbuf *mbuf;
	struct rte_ether_hdr *eth_hdr;
	uint8_t *payload;
	uint16_t i;

	mbuf = rte_pktmbuf_alloc(mp);
	if (mbuf == NULL)
		return NULL;

	if (pkt_len < RTE_ETHER_MIN_LEN)
		pkt_len = RTE_ETHER_MIN_LEN;

	eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_append(mbuf, pkt_len);
	if (eth_hdr == NULL) {
		rte_pktmbuf_free(mbuf);
		return NULL;
	}

	memset(&eth_hdr->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
	memset(&eth_hdr->src_addr, 0x02, RTE_ETHER_ADDR_LEN);
	eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	payload = (uint8_t *)(eth_hdr + 1);
	for (i = 0; i < pkt_len - sizeof(*eth_hdr); i++)
		payload[i] = (uint8_t)(seed + i);

	return mbuf;
}

static int
test_memif_dev_info(void)
{
	struct rte_eth_dev_info dev_info;
	int ret;

	ret = rte_eth_dev_info_get(memif_srv_port, &dev_info);
	TEST_ASSERT(ret == 0, "dev_info_get failed: %s", rte_strerror(-ret));
	TEST_ASSERT(strcmp(dev_info.driver_name, "net_memif") == 0,
		    "unexpected driver name: %s", dev_info.driver_name);
	return TEST_SUCCESS;
}

static int
test_memif_link_up(void)
{
	/* The pair must have connected during setup. */
	TEST_ASSERT(wait_for_link() == 0,
		    "memif server/client did not reach link up");
	return TEST_SUCCESS;
}

static int
test_memif_stats_get(void)
{
	struct rte_eth_stats stats;

	TEST_ASSERT(rte_eth_stats_get(memif_srv_port, &stats) == 0,
		    "stats_get failed for server port");
	TEST_ASSERT(rte_eth_stats_get(memif_cli_port, &stats) == 0,
		    "stats_get failed for client port");
	return TEST_SUCCESS;
}

static int
test_memif_stats_reset(void)
{
	struct rte_eth_stats stats;

	TEST_ASSERT(rte_eth_stats_reset(memif_srv_port) == 0,
		    "stats_reset failed");
	TEST_ASSERT(rte_eth_stats_get(memif_srv_port, &stats) == 0,
		    "stats_get after reset failed");
	TEST_ASSERT(stats.ipackets == 0 && stats.opackets == 0 &&
		    stats.ibytes == 0 && stats.obytes == 0,
		    "stats not zero after reset");
	return TEST_SUCCESS;
}

/*
 * Send a burst from the client and receive it on the server, verifying the
 * payload is delivered intact. This exercises the Rx/Tx data path over the
 * connected memif pair.
 */
static int
test_memif_send_receive(void)
{
	struct rte_mbuf *tx[MAX_PKT_BURST];
	struct rte_mbuf *rx[MAX_PKT_BURST];
	const uint16_t nb = MAX_PKT_BURST / 2;
	uint16_t nb_tx, nb_rx, got;
	int i, tries;

	TEST_ASSERT(wait_for_link() == 0, "link not up before send/receive");

	for (i = 0; i < nb; i++) {
		tx[i] = create_test_packet(PKT_LEN, (uint8_t)i);
		if (tx[i] == NULL) {
			while (--i >= 0)
				rte_pktmbuf_free(tx[i]);
			TEST_ASSERT(0, "failed to build test packet");
		}
	}

	nb_tx = rte_eth_tx_burst(memif_cli_port, 0, tx, nb);
	for (i = nb_tx; i < nb; i++)
		rte_pktmbuf_free(tx[i]);
	TEST_ASSERT(nb_tx > 0, "client transmitted no packets");

	/* Receive may take a moment; poll a few times. */
	got = 0;
	for (tries = 0; tries < 10 && got < nb_tx; tries++) {
		nb_rx = rte_eth_rx_burst(memif_srv_port, 0, rx, MAX_PKT_BURST);
		for (i = 0; i < nb_rx; i++) {
			/* Payload must match the position-dependent pattern. */
			uint8_t *p = rte_pktmbuf_mtod_offset(rx[i], uint8_t *,
					sizeof(struct rte_ether_hdr));
			uint16_t plen = PKT_LEN -
					sizeof(struct rte_ether_hdr);
			uint16_t k;
			int bad = rte_pktmbuf_pkt_len(rx[i]) != PKT_LEN;

			for (k = 0; !bad && k < plen; k++)
				if (p[k] != (uint8_t)(got + k))
					bad = 1;
			if (bad) {
				for (k = i; k < nb_rx; k++)
					rte_pktmbuf_free(rx[k]);
				TEST_ASSERT(0,
					    "packet %u payload/length mismatch",
					    got);
			}
			rte_pktmbuf_free(rx[i]);
			got++;
		}
		if (got < nb_tx)
			usleep(10000);
	}

	TEST_ASSERT(got == nb_tx,
		    "received %u packets, expected %u", got, nb_tx);
	return TEST_SUCCESS;
}

/*
 * Single-segment loopback on the default-bsize pair, where bsize exceeds
 * the mbuf data room so eth_memif_tx() takes the single-pool fast path.
 */
static int
test_memif_fastpath(void)
{
	struct rte_mbuf *tx[MAX_PKT_BURST];
	struct rte_mbuf *rx[MAX_PKT_BURST];
	const uint16_t nb = MAX_PKT_BURST / 2;
	uint16_t nb_tx, got;
	int i, tries;

	TEST_ASSERT(wait_for_link_pair(memif_fp_srv_port, memif_fp_cli_port)
		    == 0, "fast-path pair not up");

	for (i = 0; i < nb; i++) {
		tx[i] = create_test_packet(PKT_LEN, (uint8_t)i);
		if (tx[i] == NULL) {
			while (--i >= 0)
				rte_pktmbuf_free(tx[i]);
			TEST_ASSERT(0, "failed to build test packet");
		}
	}

	nb_tx = rte_eth_tx_burst(memif_fp_cli_port, 0, tx, nb);
	for (i = nb_tx; i < nb; i++)
		rte_pktmbuf_free(tx[i]);
	TEST_ASSERT(nb_tx > 0, "fast-path client transmitted no packets");

	got = 0;
	for (tries = 0; tries < 10 && got < nb_tx; tries++) {
		uint16_t nb_rx = rte_eth_rx_burst(memif_fp_srv_port, 0, rx,
						  MAX_PKT_BURST);
		for (i = 0; i < nb_rx; i++) {
			uint8_t *p = rte_pktmbuf_mtod_offset(rx[i], uint8_t *,
					sizeof(struct rte_ether_hdr));
			uint16_t plen = PKT_LEN -
					sizeof(struct rte_ether_hdr);
			uint16_t k;
			int bad = rte_pktmbuf_pkt_len(rx[i]) != PKT_LEN;

			for (k = 0; !bad && k < plen; k++)
				if (p[k] != (uint8_t)(got + k))
					bad = 1;
			if (bad) {
				for (k = i; k < nb_rx; k++)
					rte_pktmbuf_free(rx[k]);
				TEST_ASSERT(0,
					    "fast-path packet %u payload/length mismatch",
					    got);
			}
			rte_pktmbuf_free(rx[i]);
			got++;
		}
		if (got < nb_tx)
			usleep(10000);
	}

	TEST_ASSERT(got == nb_tx, "fast-path received %u expected %u",
		    got, nb_tx);
	return TEST_SUCCESS;
}

static int
test_memif_stop_start(void)
{
	TEST_ASSERT(rte_eth_dev_stop(memif_cli_port) == 0,
		    "stop client failed");
	TEST_ASSERT(rte_eth_dev_start(memif_cli_port) == 0,
		    "restart client failed");
	/* After a restart the pair must reconnect. */
	TEST_ASSERT(wait_for_link() == 0, "link did not recover after restart");
	return TEST_SUCCESS;
}

/*
 * A zero-length Tx burst must be accepted and return 0 without touching
 * the (empty) mbuf array. Regression test for the empty-burst guard in
 * eth_memif_tx().
 */
static int
test_memif_tx_empty_burst(void)
{
	struct rte_mbuf *dummy = NULL;

	TEST_ASSERT(wait_for_link() == 0, "link not up before empty burst");
	TEST_ASSERT(rte_eth_tx_burst(memif_cli_port, 0, &dummy, 0) == 0,
		    "zero-length Tx burst did not return 0");
	return TEST_SUCCESS;
}

/*
 * Close a configured but never-started port. The queues were never
 * allocated, so memif_disconnect() must skip the NULL queue entries
 * rather than dereference them. Regression test for the NULL-queue guard
 * in the disconnect-on-close fix. Uses a throwaway device so the suite
 * pair stays intact.
 */
static int
test_memif_close_without_stop(void)
{
	int sport, ret;
	struct rte_eth_conf port_conf;

	ret = rte_vdev_init("net_memif_test_tmps",
			    "role=server,id=1,socket-abstract=yes,socket=memif-autotest-tmp");
	if (ret < 0) {
		printf("could not create throwaway server: %s\n",
		       rte_strerror(-ret));
		return TEST_FAILED;
	}

	sport = find_port("net_memif_test_tmps");
	if (sport < 0) {
		rte_vdev_uninit("net_memif_test_tmps");
		TEST_ASSERT(0, "throwaway server port not found");
	}

	/* Configure but do not start: close is rejected on a started port. */
	memset(&port_conf, 0, sizeof(port_conf));
	ret = rte_eth_dev_configure(sport, 1, 1, &port_conf);
	if (ret < 0) {
		rte_vdev_uninit("net_memif_test_tmps");
		TEST_ASSERT(0, "configure of throwaway server failed: %s",
			    rte_strerror(-ret));
	}

	/* Close without ever starting; must not crash on the NULL queues. */
	ret = rte_eth_dev_close(sport);
	rte_vdev_uninit("net_memif_test_tmps");
	TEST_ASSERT(ret == 0, "close without stop failed: %s",
		    rte_strerror(-ret));

	return TEST_SUCCESS;
}

/*
 * A server that has been started and then stopped must refuse peers: a
 * connection accepted while stopped would leave a live control channel on
 * a stopped port. Regression test for the refuse-connection-on-stopped-
 * device fix. Without it the stopped server reports link up here.
 */
static int
test_memif_stopped_server_rejects_peer(void)
{
	int sport, cport, ret;

	ret = rte_vdev_init("net_memif_test_tmp2s",
			    "role=server,id=3,socket-abstract=yes,socket=memif-autotest-tmp2");
	if (ret < 0) {
		printf("could not create throwaway server: %s\n",
		       rte_strerror(-ret));
		return TEST_FAILED;
	}
	sport = find_port("net_memif_test_tmp2s");
	if (sport < 0) {
		rte_vdev_uninit("net_memif_test_tmp2s");
		TEST_ASSERT(0, "throwaway server port not found");
	}
	if (configure_port(sport) < 0) {
		rte_vdev_uninit("net_memif_test_tmp2s");
		TEST_ASSERT(0, "throwaway server start failed");
	}
	if (rte_eth_dev_stop(sport) < 0) {
		rte_eth_dev_close(sport);
		rte_vdev_uninit("net_memif_test_tmp2s");
		TEST_ASSERT(0, "throwaway server stop failed");
	}

	ret = rte_vdev_init("net_memif_test_tmp2c",
			    "role=client,id=3,socket-abstract=yes,socket=memif-autotest-tmp2");
	if (ret < 0) {
		rte_eth_dev_close(sport);
		rte_vdev_uninit("net_memif_test_tmp2s");
		TEST_ASSERT(0, "could not create throwaway client: %s",
			    rte_strerror(-ret));
	}
	cport = find_port("net_memif_test_tmp2c");
	if (cport < 0 || configure_port(cport) < 0) {
		rte_eth_dev_close(sport);
		rte_vdev_uninit("net_memif_test_tmp2c");
		rte_vdev_uninit("net_memif_test_tmp2s");
		TEST_ASSERT(0, "throwaway client setup failed");
	}

	/* The stopped server must not come up (short window; it never will). */
	ret = wait_for_link_pair_tries(sport, cport, REJECT_TRIES);

	rte_eth_dev_stop(cport);
	rte_eth_dev_close(cport);
	rte_eth_dev_close(sport);
	rte_vdev_uninit("net_memif_test_tmp2c");
	rte_vdev_uninit("net_memif_test_tmp2s");

	TEST_ASSERT(ret != 0, "stopped server accepted a peer");
	return TEST_SUCCESS;
}

#define BIG_PKT_LEN 4000	/* > bsize (512) => chained descriptors */
#define BIG_SEG_LEN 1000	/* each mbuf segment carries this many bytes */
/* ~8 descriptors per packet; enough to wrap the 2^MEMIF_LOG2_RING ring */
#define WRAP_PKTS ((1 << MEMIF_LOG2_RING) / 8 + 8)

/*
 * Build a multi-segment mbuf of pkt_len bytes, chaining as many pool mbufs
 * as needed (BIG_SEG_LEN bytes each) with a position-dependent pattern.
 */
static struct rte_mbuf *
create_big_packet(uint16_t pkt_len, uint8_t seed)
{
	struct rte_mbuf *head = NULL, *tail = NULL;
	uint16_t done = 0;

	while (done < pkt_len) {
		struct rte_mbuf *seg = rte_pktmbuf_alloc(mp);
		uint16_t seg_len = RTE_MIN((uint16_t)BIG_SEG_LEN,
					   (uint16_t)(pkt_len - done));
		uint8_t *data;
		uint16_t i;

		if (seg == NULL) {
			rte_pktmbuf_free(head);
			return NULL;
		}
		data = (uint8_t *)rte_pktmbuf_append(seg, seg_len);
		if (data == NULL) {
			rte_pktmbuf_free(seg);
			rte_pktmbuf_free(head);
			return NULL;
		}
		for (i = 0; i < seg_len; i++)
			data[i] = (uint8_t)(seed + done + i);

		if (head == NULL) {
			head = seg;
			tail = seg;
		} else {
			tail->next = seg;
			tail = seg;
			head->nb_segs++;
			head->pkt_len += seg_len;
		}
		done += seg_len;
	}
	return head;
}

/* Verify a received packet's payload, walking its segment chain. */
static int
check_big_packet(struct rte_mbuf *m, uint16_t pkt_len, uint8_t seed)
{
	uint16_t off = 0;
	struct rte_mbuf *seg;

	if (rte_pktmbuf_pkt_len(m) != pkt_len)
		return -1;
	for (seg = m; seg != NULL; seg = seg->next) {
		const uint8_t *data = rte_pktmbuf_mtod(seg, const uint8_t *);
		uint16_t i;

		for (i = 0; i < rte_pktmbuf_data_len(seg); i++, off++)
			if (data[i] != (uint8_t)(seed + off))
				return -1;
	}
	return off == pkt_len ? 0 : -1;
}

/*
 * Send multi-segment packets larger than the memif buffer size, repeated
 * enough to wrap the ring, and verify each is delivered intact. Exercises
 * the chained-descriptor Rx and Tx paths across a ring wrap.
 */
static int
test_memif_multiseg(void)
{
	unsigned int sent = 0, recv = 0;
	unsigned int iter;

	TEST_ASSERT(wait_for_link() == 0, "link not up before multiseg test");

	for (iter = 0; iter < WRAP_PKTS; iter++) {
		struct rte_mbuf *tx, *rx[MAX_PKT_BURST];
		uint16_t n, j;
		int tries;

		tx = create_big_packet(BIG_PKT_LEN, (uint8_t)iter);
		TEST_ASSERT(tx != NULL, "failed to build big packet %u", iter);
		if (tx->nb_segs <= 1) {
			rte_pktmbuf_free(tx);
			TEST_ASSERT(0, "test packet is not multi-segment");
		}

		if (rte_eth_tx_burst(memif_cli_port, 0, &tx, 1) != 1) {
			rte_pktmbuf_free(tx);
			continue;
		}
		sent++;

		for (tries = 0; tries < 10; tries++) {
			n = rte_eth_rx_burst(memif_srv_port, 0, rx,
					     MAX_PKT_BURST);
			for (j = 0; j < n; j++) {
				if (check_big_packet(rx[j], BIG_PKT_LEN,
						(uint8_t)recv) != 0) {
					uint16_t k;
					for (k = j; k < n; k++)
						rte_pktmbuf_free(rx[k]);
					TEST_ASSERT(0,
						"multi-seg payload corrupted at pkt %u",
						recv);
				}
				rte_pktmbuf_free(rx[j]);
				recv++;
			}
			if (recv >= sent)
				break;
			usleep(2000);
		}
	}

	TEST_ASSERT(recv == sent,
		    "multi-seg: sent %u received %u", sent, recv);
	return TEST_SUCCESS;
}

static void
test_memif_cleanup(void)
{
	if (memif_srv_port >= 0) {
		rte_eth_dev_stop(memif_srv_port);
		rte_eth_dev_close(memif_srv_port);
	}
	if (memif_cli_port >= 0) {
		rte_eth_dev_stop(memif_cli_port);
		rte_eth_dev_close(memif_cli_port);
	}
	if (memif_fp_srv_port >= 0) {
		rte_eth_dev_stop(memif_fp_srv_port);
		rte_eth_dev_close(memif_fp_srv_port);
	}
	if (memif_fp_cli_port >= 0) {
		rte_eth_dev_stop(memif_fp_cli_port);
		rte_eth_dev_close(memif_fp_cli_port);
	}
	rte_vdev_uninit(MEMIF_SRV_NAME);
	rte_vdev_uninit(MEMIF_CLI_NAME);
	rte_vdev_uninit(MEMIF_FP_SRV_NAME);
	rte_vdev_uninit(MEMIF_FP_CLI_NAME);
	if (mp != NULL) {
		rte_mempool_free(mp);
		mp = NULL;
	}
	memif_srv_port = -1;
	memif_cli_port = -1;
	memif_fp_srv_port = -1;
	memif_fp_cli_port = -1;
}

static int
test_memif_setup(void)
{
	int ret;

	mp = rte_pktmbuf_pool_create("memif_test_pool", NB_MBUF, 32, 0,
				     RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mp == NULL) {
		printf("Error: failed to create mempool: %s\n",
		       rte_strerror(rte_errno));
		return -1;
	}

	ret = rte_vdev_init(MEMIF_SRV_NAME, MEMIF_SRV_ARGS);
	if (ret < 0) {
		printf("Error: failed to create memif server: %s\n",
		       rte_strerror(-ret));
		rte_mempool_free(mp);
		mp = NULL;
		return TEST_FAILED;
	}

	ret = rte_vdev_init(MEMIF_CLI_NAME, MEMIF_CLI_ARGS);
	if (ret < 0) {
		printf("Error: failed to create memif client: %s\n",
		       rte_strerror(-ret));
		rte_vdev_uninit(MEMIF_SRV_NAME);
		rte_mempool_free(mp);
		mp = NULL;
		return -1;
	}

	memif_srv_port = find_port(MEMIF_SRV_NAME);
	memif_cli_port = find_port(MEMIF_CLI_NAME);
	if (memif_srv_port < 0 || memif_cli_port < 0) {
		printf("Error: failed to find memif ports\n");
		test_memif_cleanup();
		return -1;
	}

	if (configure_port(memif_srv_port) < 0 ||
	    configure_port(memif_cli_port) < 0) {
		test_memif_cleanup();
		return -1;
	}

	/* Second pair with default bsize to exercise the Tx fast path. */
	if (rte_vdev_init(MEMIF_FP_SRV_NAME, MEMIF_FP_SRV_ARGS) < 0 ||
	    rte_vdev_init(MEMIF_FP_CLI_NAME, MEMIF_FP_CLI_ARGS) < 0) {
		printf("Error: failed to create fast-path memif pair\n");
		test_memif_cleanup();
		return TEST_FAILED;
	}

	memif_fp_srv_port = find_port(MEMIF_FP_SRV_NAME);
	memif_fp_cli_port = find_port(MEMIF_FP_CLI_NAME);
	if (memif_fp_srv_port < 0 || memif_fp_cli_port < 0) {
		printf("Error: failed to find fast-path memif ports\n");
		test_memif_cleanup();
		return TEST_FAILED;
	}

	if (configure_port(memif_fp_srv_port) < 0 ||
	    configure_port(memif_fp_cli_port) < 0) {
		test_memif_cleanup();
		return -1;
	}

	return 0;
}

static struct unit_test_suite test_pmd_memif_suite = {
	.setup = test_memif_setup,
	.teardown = test_memif_cleanup,
	.suite_name = "memif PMD Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_memif_dev_info),
		TEST_CASE(test_memif_link_up),
		TEST_CASE(test_memif_stats_get),
		TEST_CASE(test_memif_send_receive),
		TEST_CASE(test_memif_fastpath),
		TEST_CASE(test_memif_multiseg),
		TEST_CASE(test_memif_stats_reset),
		TEST_CASE(test_memif_stop_start),
		TEST_CASE(test_memif_tx_empty_burst),
		TEST_CASE(test_memif_close_without_stop),
		TEST_CASE(test_memif_stopped_server_rejects_peer),
		TEST_CASES_END()
	}
};

static int
test_pmd_memif(void)
{
	return unit_test_suite_runner(&test_pmd_memif_suite);
}

#endif /* RTE_EXEC_ENV_LINUX */

REGISTER_FAST_TEST(memif_pmd_autotest, NOHUGE_OK, ASAN_OK, test_pmd_memif);
