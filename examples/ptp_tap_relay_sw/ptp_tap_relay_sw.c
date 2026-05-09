/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

/*
 * PTP Software Relay
 *
 * A minimal PTP relay between a DPDK-bound physical NIC and a kernel
 * TAP interface using software timestamps only.
 *
 * How it works:
 *   1. Physical NIC receives PTP (and non-PTP) packets via DPDK RX.
 *   2. For PTP event messages (Sync, Delay_Req, PDelay_Req, PDelay_Resp)
 *      the relay records an RX software timestamp (clock_gettime).
 *   3. Just before TX on the other side it records a TX software timestamp.
 *   4. The relay residence time (tx_ts − rx_ts) is added to the PTP
 *      correctionField via rte_ptp_add_correction() — standard
 *      Transparent Clock behaviour (IEEE 1588-2019 §10.2).
 *   5. Packets are forwarded bi-directionally:
 *        PHY → TAP   (network → ptp4l)
 *        TAP → PHY   (ptp4l → network)
 *
 * ptp4l runs in software-timestamping mode on the TAP interface:
 *
 *   ptp4l -i dtap0 -m -s -S   # -S = software timestamps
 *
 * Topology:
 *
 *   Time Transmitter (remote) ──L2── Physical NIC (DPDK)
 *                                      │
 *                                PTP SW Relay  ← correctionField update
 *                                      │
 *                                TAP (kernel) ── ptp4l -S (time receiver)
 *
 * Usage:
 *   dpdk-ptp_tap_relay_sw -l 0-1 --vdev=net_tap0,iface=dtap0 -- \
 *       -p 0 -t 1
 *
 * Parameters:
 *   -p PORT    Physical NIC port ID (default: 0)
 *   -t PORT    TAP port ID (default: 1)
 *   -T SECS    Stats print interval in seconds (default: 10)
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_lcore.h>

#include "ptp_parse.h"

/* Ring sizes */
#define RX_RING_SIZE  1024
#define TX_RING_SIZE  1024

/* Mempool */
#define NUM_MBUFS     8191
#define MBUF_CACHE    250
#define BURST_SIZE    32

#define NSEC_PER_SEC  1000000000ULL

/* Logging helpers */
#define LOG_INFO(fmt, ...) \
	fprintf(stdout, "[PTP-SW] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) \
	fprintf(stderr, "[PTP-SW ERROR] " fmt "\n", ##__VA_ARGS__)

static volatile bool force_quit;

/* Port IDs */
static uint16_t phy_port;
static uint16_t tap_port = 1;
static unsigned int stats_interval = 10;  /* seconds */

/* Statistics */
static struct {
	uint64_t phy_rx;        /* total packets from PHY */
	uint64_t phy_rx_ptp;    /* PTP packets from PHY */
	uint64_t tap_tx;        /* packets forwarded to TAP */
	uint64_t tap_rx;        /* total packets from TAP */
	uint64_t tap_rx_ptp;    /* PTP packets from TAP */
	uint64_t phy_tx;        /* packets forwarded to PHY */
	uint64_t corrections;   /* correctionField updates */
} stats;

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		LOG_INFO("Signal %d received, shutting down...", signum);
		force_quit = true;
	}
}

/* Helpers */

/* Read monotonic clock in nanoseconds (for residence time). */
static inline uint64_t
sw_timestamp_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}

/* Port Init */

static int
port_init(uint16_t port, struct rte_mempool *mp)
{
	struct rte_eth_conf port_conf;
	struct rte_eth_dev_info dev_info;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int ret;

	memset(&port_conf, 0, sizeof(port_conf));

	ret = rte_eth_dev_info_get(port, &dev_info);
	if (ret != 0) {
		LOG_ERR("rte_eth_dev_info_get(port %u) failed: %d", port, ret);
		return ret;
	}

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret != 0)
		return ret;

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (ret != 0)
		return ret;

	ret = rte_eth_rx_queue_setup(port, 0, nb_rxd,
			rte_eth_dev_socket_id(port), NULL, mp);
	if (ret < 0)
		return ret;

	ret = rte_eth_tx_queue_setup(port, 0, nb_txd,
			rte_eth_dev_socket_id(port), NULL);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_start(port);
	if (ret < 0)
		return ret;

	ret = rte_eth_promiscuous_enable(port);
	if (ret != 0) {
		LOG_ERR("Failed to enable promiscuous on port %u: %s",
			port, rte_strerror(-ret));
		return ret;
	}

	return 0;
}

/* Relay one direction */

/*
 * Forward packets from src_port to dst_port.
 * For PTP event messages, record SW timestamps around the
 * relay path and add the residence time to the correctionField.
 *
 * This implements a Transparent Clock (IEEE 1588-2019 §10.2):
 *   correctionField += (t_egress − t_ingress)
 *
 * Note: a single rx_ts / tx_ts pair is used for the entire burst.
 * At typical PTP rates (logSyncInterval >= -4, i.e. <= 16 pkt/s)
 * bursts contain at most one packet, so this is exact.  At higher
 * rates, early packets in a burst are slightly under-corrected and
 * late ones over-corrected by up to one poll-loop iteration.
 */
static void
relay_burst(uint16_t src_port, uint16_t dst_port,
	    uint64_t *rx_cnt, uint64_t *rx_ptp_cnt,
	    uint64_t *tx_cnt, uint64_t *corr_cnt)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	struct rte_ptp_hdr *ptp_hdrs[BURST_SIZE];
	uint64_t rx_ts;
	uint16_t nb_rx, nb_tx, i;

	nb_rx = rte_eth_rx_burst(src_port, 0, bufs, BURST_SIZE);
	if (nb_rx == 0)
		return;

	/* Record a single RX software timestamp for the whole burst.
	 * All packets in one burst arrived at essentially the same instant
	 * from rte_eth_rx_burst()'s perspective.
	 */
	rx_ts = sw_timestamp_ns();

	*rx_cnt += nb_rx;

	/*
	 * Pass 1: Parse each packet once and remember PTP event headers.
	 * This avoids taking the TX timestamp too early — we want it as
	 * close to the actual rte_eth_tx_burst() call as possible.
	 */
	memset(ptp_hdrs, 0, sizeof(ptp_hdrs[0]) * nb_rx);
	for (i = 0; i < nb_rx; i++) {
		struct rte_ptp_hdr *hdr = ptp_hdr_find(bufs[i]);

		if (hdr == NULL)
			continue;

		(*rx_ptp_cnt)++;

		/* Only event messages carry timestamps that need correction */
		if (!rte_ptp_is_event(hdr))
			continue;

		ptp_hdrs[i] = hdr;
	}

	/*
	 * Pass 2: Take a single TX timestamp right before transmission.
	 * This minimises the gap between the measured tx_ts and the
	 * actual kernel write inside rte_eth_tx_burst(), giving the
	 * most accurate residence time we can achieve with SW timestamps.
	 *
	 * residence_time = tx_ts − rx_ts
	 *
	 * Remaining untracked delays:
	 *   - Pre-RX:  NIC DMA → rx_burst return  (~1-5 µs, unavoidable)
	 *   - Post-TX:  tx_ts → kernel TAP write   (~1-2 µs)
	 * Both are symmetric for Sync and Delay_Req so they largely
	 * cancel in the ptp4l offset calculation.
	 */
	uint64_t tx_ts = sw_timestamp_ns();
	int64_t residence_ns = (int64_t)(tx_ts - rx_ts);

	for (i = 0; i < nb_rx; i++) {
		if (ptp_hdrs[i] == NULL)
			continue;
		rte_ptp_add_correction(ptp_hdrs[i], residence_ns);
		(*corr_cnt)++;
	}

	/* Forward the burst */
	nb_tx = rte_eth_tx_burst(dst_port, 0, bufs, nb_rx);
	*tx_cnt += nb_tx;

	/* Free any unsent packets */
	for (i = nb_tx; i < nb_rx; i++)
		rte_pktmbuf_free(bufs[i]);
}

/* Print statistics */

static void
print_stats(void)
{
	LOG_INFO("=== Statistics ===");
	LOG_INFO("  PHY RX total:   %"PRIu64, stats.phy_rx);
	LOG_INFO("  PHY RX PTP:     %"PRIu64, stats.phy_rx_ptp);
	LOG_INFO("  TAP TX:         %"PRIu64, stats.tap_tx);
	LOG_INFO("  TAP RX total:   %"PRIu64, stats.tap_rx);
	LOG_INFO("  TAP RX PTP:     %"PRIu64, stats.tap_rx_ptp);
	LOG_INFO("  PHY TX:         %"PRIu64, stats.phy_tx);
	LOG_INFO("  Corrections:    %"PRIu64, stats.corrections);
}

/* Main relay loop */

static int
relay_loop(__rte_unused void *arg)
{
	uint64_t last_stats = rte_rdtsc();
	uint64_t stats_tsc = rte_get_tsc_hz() * stats_interval;

	LOG_INFO("Relay loop started on lcore %u", rte_lcore_id());
	LOG_INFO("  PHY port %u  <-->  TAP port %u", phy_port, tap_port);
	LOG_INFO("  Correction field updates: enabled for event messages");

	while (!force_quit) {
		/* PHY → TAP */
		relay_burst(phy_port, tap_port,
			    &stats.phy_rx, &stats.phy_rx_ptp,
			    &stats.tap_tx, &stats.corrections);

		/* TAP → PHY */
		relay_burst(tap_port, phy_port,
			    &stats.tap_rx, &stats.tap_rx_ptp,
			    &stats.phy_tx, &stats.corrections);

		/* Periodic stats */
		if (rte_rdtsc() - last_stats > stats_tsc) {
			print_stats();
			last_stats = rte_rdtsc();
		}
	}

	print_stats();
	return 0;
}

/* Argument parsing */

static void
usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [EAL options] -- [options]\n"
		"  -p PORT   Physical NIC port ID (default: 0)\n"
		"  -t PORT   TAP port ID (default: 1)\n"
		"  -T SECS   Stats interval in seconds (default: 10)\n"
		"\n"
		"Example:\n"
		"  %s -l 0-1 --vdev=net_tap0,iface=dtap0 -- -p 0 -t 1\n"
		"\n"
		"Then run ptp4l with software timestamps:\n"
		"  ptp4l -i dtap0 -m -s -S\n",
		prog, prog);
}

static int
parse_args(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "p:t:T:h")) != -1) {
		switch (opt) {
		case 'p':
			phy_port = (uint16_t)atoi(optarg);
			break;
		case 't':
			tap_port = (uint16_t)atoi(optarg);
			break;
		case 'T':
			stats_interval = (unsigned int)atoi(optarg);
			break;
		case 'h':
		default:
			usage(argv[0]);
			return -1;
		}
	}

	return 0;
}

/* Main */

int
main(int argc, char **argv)
{
	struct rte_mempool *mp;
	uint16_t nb_ports;
	int ret;

	/* EAL init */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed\n");
	argc -= ret;
	argv += ret;

	/* App args */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 2)
		rte_exit(EXIT_FAILURE,
			 "Need at least 2 ports (PHY + TAP).\n"
			 "Use --vdev=net_tap0,iface=dtap0\n");

	if (!rte_eth_dev_is_valid_port(phy_port))
		rte_exit(EXIT_FAILURE, "Invalid PHY port %u\n", phy_port);
	if (!rte_eth_dev_is_valid_port(tap_port))
		rte_exit(EXIT_FAILURE, "Invalid TAP port %u\n", tap_port);

	mp = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
				     MBUF_CACHE, 0,
				     RTE_MBUF_DEFAULT_BUF_SIZE,
				     rte_socket_id());
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	LOG_INFO("Initializing PHY port %u...", phy_port);
	ret = port_init(phy_port, mp);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "Cannot init PHY port %u (%d)\n",
			 phy_port, ret);

	LOG_INFO("Initializing TAP port %u...", tap_port);
	ret = port_init(tap_port, mp);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "Cannot init TAP port %u (%d)\n",
			 tap_port, ret);

	LOG_INFO("PTP Software Relay ready");
	LOG_INFO("  PHY port:     %u", phy_port);
	LOG_INFO("  TAP port:     %u", tap_port);
	LOG_INFO("  Stats every:  %u seconds", stats_interval);
	LOG_INFO("  Correction:   Transparent Clock (SW timestamps)");
	LOG_INFO("");
	LOG_INFO("Run ptp4l:  ptp4l -i dtap0 -m -s -S");

	/* Run relay on main lcore */
	relay_loop(NULL);

	/* Cleanup */
	LOG_INFO("Stopping ports...");
	rte_eth_dev_stop(phy_port);
	rte_eth_dev_stop(tap_port);
	rte_eth_dev_close(phy_port);
	rte_eth_dev_close(tap_port);
	rte_eal_cleanup();

	return 0;
}
