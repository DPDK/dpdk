/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_rawdev.h>
#include <rte_ioat_rawdev.h>

/* size of ring used for software copying between rx and tx. */
#define RTE_LOGTYPE_IOAT RTE_LOGTYPE_USER1
#define MAX_PKT_BURST 32
#define MEMPOOL_CACHE_SIZE 512
#define MIN_POOL_SIZE 65536U
#define CMD_LINE_OPT_MAC_UPDATING "mac-updating"
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"
#define CMD_LINE_OPT_PORTMASK "portmask"
#define CMD_LINE_OPT_NB_QUEUE "nb-queue"
#define CMD_LINE_OPT_COPY_TYPE "copy-type"
#define CMD_LINE_OPT_RING_SIZE "ring-size"

/* configurable number of RX/TX ring descriptors */
#define RX_DEFAULT_RINGSIZE 1024
#define TX_DEFAULT_RINGSIZE 1024

/* max number of RX queues per port */
#define MAX_RX_QUEUES_COUNT 8

struct rxtx_port_config {
	/* common config */
	uint16_t rxtx_port;
	uint16_t nb_queues;
	/* for software copy mode */
	struct rte_ring *rx_to_tx_ring;
	/* for IOAT rawdev copy mode */
	uint16_t ioat_ids[MAX_RX_QUEUES_COUNT];
};

struct rxtx_transmission_config {
	struct rxtx_port_config ports[RTE_MAX_ETHPORTS];
	uint16_t nb_ports;
	uint16_t nb_lcores;
};

/* per-port statistics struct */
struct ioat_port_statistics {
	uint64_t rx[RTE_MAX_ETHPORTS];
	uint64_t tx[RTE_MAX_ETHPORTS];
	uint64_t tx_dropped[RTE_MAX_ETHPORTS];
	uint64_t copy_dropped[RTE_MAX_ETHPORTS];
};
struct ioat_port_statistics port_statistics;

struct total_statistics {
	uint64_t total_packets_dropped;
	uint64_t total_packets_tx;
	uint64_t total_packets_rx;
	uint64_t total_successful_enqueues;
	uint64_t total_failed_enqueues;
};

typedef enum copy_mode_t {
#define COPY_MODE_SW "sw"
	COPY_MODE_SW_NUM,
#define COPY_MODE_IOAT "hw"
	COPY_MODE_IOAT_NUM,
	COPY_MODE_INVALID_NUM,
	COPY_MODE_SIZE_NUM = COPY_MODE_INVALID_NUM
} copy_mode_t;

/* mask of enabled ports */
static uint32_t ioat_enabled_port_mask;

/* number of RX queues per port */
static uint16_t nb_queues = 1;

/* MAC updating enabled by default. */
static int mac_updating = 1;

/* hardare copy mode enabled by default. */
static copy_mode_t copy_mode = COPY_MODE_IOAT_NUM;

/* size of IOAT rawdev ring for hardware copy mode or
 * rte_ring for software copy mode
 */
static unsigned short ring_size = 2048;

/* global transmission config */
struct rxtx_transmission_config cfg;

/* configurable number of RX/TX ring descriptors */
static uint16_t nb_rxd = RX_DEFAULT_RINGSIZE;
static uint16_t nb_txd = TX_DEFAULT_RINGSIZE;

static volatile bool force_quit;

/* ethernet addresses of ports */
static struct rte_ether_addr ioat_ports_eth_addr[RTE_MAX_ETHPORTS];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
struct rte_mempool *ioat_pktmbuf_pool;

/* Print out statistics for one port. */
static void
print_port_stats(uint16_t port_id)
{
	printf("\nStatistics for port %u ------------------------------"
		"\nPackets sent: %34"PRIu64
		"\nPackets received: %30"PRIu64
		"\nPackets dropped on tx: %25"PRIu64
		"\nPackets dropped on copy: %23"PRIu64,
		port_id,
		port_statistics.tx[port_id],
		port_statistics.rx[port_id],
		port_statistics.tx_dropped[port_id],
		port_statistics.copy_dropped[port_id]);
}

/* Print out statistics for one IOAT rawdev device. */
static void
print_rawdev_stats(uint32_t dev_id, uint64_t *xstats,
	unsigned int *ids_xstats, uint16_t nb_xstats,
	struct rte_rawdev_xstats_name *names_xstats)
{
	uint16_t i;

	printf("\nIOAT channel %u", dev_id);
	for (i = 0; i < nb_xstats; i++)
		printf("\n\t %s: %*"PRIu64,
			names_xstats[ids_xstats[i]].name,
			(int)(37 - strlen(names_xstats[ids_xstats[i]].name)),
			xstats[i]);
}

static void
print_total_stats(struct total_statistics *ts)
{
	printf("\nAggregate statistics ==============================="
		"\nTotal packets Tx: %24"PRIu64" [pps]"
		"\nTotal packets Rx: %24"PRIu64" [pps]"
		"\nTotal packets dropped: %19"PRIu64" [pps]",
		ts->total_packets_tx,
		ts->total_packets_rx,
		ts->total_packets_dropped);

	if (copy_mode == COPY_MODE_IOAT_NUM) {
		printf("\nTotal IOAT successful enqueues: %8"PRIu64" [enq/s]"
			"\nTotal IOAT failed enqueues: %12"PRIu64" [enq/s]",
			ts->total_successful_enqueues,
			ts->total_failed_enqueues);
	}

	printf("\n====================================================\n");
}

/* Print out statistics on packets dropped. */
static void
print_stats(char *prgname)
{
	struct total_statistics ts, delta_ts;
	uint32_t i, port_id, dev_id;
	struct rte_rawdev_xstats_name *names_xstats;
	uint64_t *xstats;
	unsigned int *ids_xstats, nb_xstats;
	char status_string[255]; /* to print at the top of the output */
	int status_strlen;
	int ret;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	status_strlen = snprintf(status_string, sizeof(status_string),
		"%s, ", prgname);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Worker Threads = %d, ",
		rte_lcore_count() > 2 ? 2 : 1);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Copy Mode = %s,\n", copy_mode == COPY_MODE_SW_NUM ?
		COPY_MODE_SW : COPY_MODE_IOAT);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Updating MAC = %s, ", mac_updating ?
		"enabled" : "disabled");
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Rx Queues = %d, ", nb_queues);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Ring Size = %d", ring_size);

	/* Allocate memory for xstats names and values */
	ret = rte_rawdev_xstats_names_get(
		cfg.ports[0].ioat_ids[0], NULL, 0);
	if (ret < 0)
		return;
	nb_xstats = (unsigned int)ret;

	names_xstats = malloc(sizeof(*names_xstats) * nb_xstats);
	if (names_xstats == NULL) {
		rte_exit(EXIT_FAILURE,
			"Error allocating xstat names memory\n");
	}
	rte_rawdev_xstats_names_get(cfg.ports[0].ioat_ids[0],
		names_xstats, nb_xstats);

	ids_xstats = malloc(sizeof(*ids_xstats) * 2);
	if (ids_xstats == NULL) {
		rte_exit(EXIT_FAILURE,
			"Error allocating xstat ids_xstats memory\n");
	}

	xstats = malloc(sizeof(*xstats) * 2);
	if (xstats == NULL) {
		rte_exit(EXIT_FAILURE,
			"Error allocating xstat memory\n");
	}

	/* Get failed/successful enqueues stats index */
	ids_xstats[0] = ids_xstats[1] = nb_xstats;
	for (i = 0; i < nb_xstats; i++) {
		if (!strcmp(names_xstats[i].name, "failed_enqueues"))
			ids_xstats[0] = i;
		else if (!strcmp(names_xstats[i].name, "successful_enqueues"))
			ids_xstats[1] = i;
		if (ids_xstats[0] < nb_xstats && ids_xstats[1] < nb_xstats)
			break;
	}
	if (ids_xstats[0] == nb_xstats || ids_xstats[1] == nb_xstats) {
		rte_exit(EXIT_FAILURE,
			"Error getting failed/successful enqueues stats index\n");
	}

	memset(&ts, 0, sizeof(struct total_statistics));

	while (!force_quit) {
		/* Sleep for 1 second each round - init sleep allows reading
		 * messages from app startup.
		 */
		sleep(1);

		/* Clear screen and move to top left */
		printf("%s%s", clr, topLeft);

		memset(&delta_ts, 0, sizeof(struct total_statistics));

		printf("%s\n", status_string);

		for (i = 0; i < cfg.nb_ports; i++) {
			port_id = cfg.ports[i].rxtx_port;
			print_port_stats(port_id);

			delta_ts.total_packets_dropped +=
				port_statistics.tx_dropped[port_id]
				+ port_statistics.copy_dropped[port_id];
			delta_ts.total_packets_tx +=
				port_statistics.tx[port_id];
			delta_ts.total_packets_rx +=
				port_statistics.rx[port_id];

			if (copy_mode == COPY_MODE_IOAT_NUM) {
				uint32_t j;

				for (j = 0; j < cfg.ports[i].nb_queues; j++) {
					dev_id = cfg.ports[i].ioat_ids[j];
					rte_rawdev_xstats_get(dev_id,
						ids_xstats, xstats, 2);

					print_rawdev_stats(dev_id, xstats,
						ids_xstats, 2, names_xstats);

					delta_ts.total_failed_enqueues +=
						xstats[ids_xstats[0]];
					delta_ts.total_successful_enqueues +=
						xstats[ids_xstats[1]];
				}
			}
		}

		delta_ts.total_packets_tx -= ts.total_packets_tx;
		delta_ts.total_packets_rx -= ts.total_packets_rx;
		delta_ts.total_packets_dropped -= ts.total_packets_dropped;
		delta_ts.total_failed_enqueues -= ts.total_failed_enqueues;
		delta_ts.total_successful_enqueues -=
			ts.total_successful_enqueues;

		printf("\n");
		print_total_stats(&delta_ts);

		fflush(stdout);

		ts.total_packets_tx += delta_ts.total_packets_tx;
		ts.total_packets_rx += delta_ts.total_packets_rx;
		ts.total_packets_dropped += delta_ts.total_packets_dropped;
		ts.total_failed_enqueues += delta_ts.total_failed_enqueues;
		ts.total_successful_enqueues +=
			delta_ts.total_successful_enqueues;
	}

	free(names_xstats);
	free(xstats);
	free(ids_xstats);
}

static void
update_mac_addrs(struct rte_mbuf *m, uint32_t dest_portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx - overwriting 2 bytes of source address but
	 * it's acceptable cause it gets overwritten by rte_ether_addr_copy
	 */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	/* src addr */
	rte_ether_addr_copy(&ioat_ports_eth_addr[dest_portid], &eth->s_addr);
}

static inline void
pktmbuf_sw_copy(struct rte_mbuf *src, struct rte_mbuf *dst)
{
	/* Copy packet metadata */
	rte_memcpy(&dst->rearm_data,
		&src->rearm_data,
		offsetof(struct rte_mbuf, cacheline1)
		- offsetof(struct rte_mbuf, rearm_data));

	/* Copy packet data */
	rte_memcpy(rte_pktmbuf_mtod(dst, char *),
		rte_pktmbuf_mtod(src, char *), src->data_len);
}

static uint32_t
ioat_enqueue_packets(struct rte_mbuf **pkts,
	uint32_t nb_rx, uint16_t dev_id)
{
	int ret;
	uint32_t i;
	struct rte_mbuf *pkts_copy[MAX_PKT_BURST];

	const uint64_t addr_offset = RTE_PTR_DIFF(pkts[0]->buf_addr,
		&pkts[0]->rearm_data);

	ret = rte_mempool_get_bulk(ioat_pktmbuf_pool,
		(void *)pkts_copy, nb_rx);

	if (unlikely(ret < 0))
		rte_exit(EXIT_FAILURE, "Unable to allocate memory.\n");

	for (i = 0; i < nb_rx; i++) {
		/* Perform data copy */
		ret = rte_ioat_enqueue_copy(dev_id,
			pkts[i]->buf_iova - addr_offset,
			pkts_copy[i]->buf_iova - addr_offset,
			rte_pktmbuf_data_len(pkts[i]) + addr_offset,
			(uintptr_t)pkts[i],
			(uintptr_t)pkts_copy[i]);

		if (ret != 1)
			break;
	}

	ret = i;
	/* Free any not enqueued packets. */
	rte_mempool_put_bulk(ioat_pktmbuf_pool, (void *)&pkts[i], nb_rx - i);
	rte_mempool_put_bulk(ioat_pktmbuf_pool, (void *)&pkts_copy[i],
		nb_rx - i);

	return ret;
}

/* Receive packets on one port and enqueue to IOAT rawdev or rte_ring. */
static void
ioat_rx_port(struct rxtx_port_config *rx_config)
{
	uint32_t nb_rx, nb_enq, i, j;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	for (i = 0; i < rx_config->nb_queues; i++) {

		nb_rx = rte_eth_rx_burst(rx_config->rxtx_port, i,
			pkts_burst, MAX_PKT_BURST);

		if (nb_rx == 0)
			continue;

		port_statistics.rx[rx_config->rxtx_port] += nb_rx;

		if (copy_mode == COPY_MODE_IOAT_NUM) {
			/* Perform packet hardware copy */
			nb_enq = ioat_enqueue_packets(pkts_burst,
				nb_rx, rx_config->ioat_ids[i]);
			if (nb_enq > 0)
				rte_ioat_perform_ops(rx_config->ioat_ids[i]);
		} else {
			/* Perform packet software copy, free source packets */
			int ret;
			struct rte_mbuf *pkts_burst_copy[MAX_PKT_BURST];

			ret = rte_mempool_get_bulk(ioat_pktmbuf_pool,
				(void *)pkts_burst_copy, nb_rx);

			if (unlikely(ret < 0))
				rte_exit(EXIT_FAILURE,
					"Unable to allocate memory.\n");

			for (j = 0; j < nb_rx; j++)
				pktmbuf_sw_copy(pkts_burst[j],
					pkts_burst_copy[j]);

			rte_mempool_put_bulk(ioat_pktmbuf_pool,
				(void *)pkts_burst, nb_rx);

			nb_enq = rte_ring_enqueue_burst(
				rx_config->rx_to_tx_ring,
				(void *)pkts_burst_copy, nb_rx, NULL);

			/* Free any not enqueued packets. */
			rte_mempool_put_bulk(ioat_pktmbuf_pool,
				(void *)&pkts_burst_copy[nb_enq],
				nb_rx - nb_enq);
		}

		port_statistics.copy_dropped[rx_config->rxtx_port] +=
			(nb_rx - nb_enq);
	}
}

/* Transmit packets from IOAT rawdev/rte_ring for one port. */
static void
ioat_tx_port(struct rxtx_port_config *tx_config)
{
	uint32_t i, j, nb_dq = 0;
	struct rte_mbuf *mbufs_src[MAX_PKT_BURST];
	struct rte_mbuf *mbufs_dst[MAX_PKT_BURST];

	for (i = 0; i < tx_config->nb_queues; i++) {
		if (copy_mode == COPY_MODE_IOAT_NUM) {
			/* Deque the mbufs from IOAT device. */
			nb_dq = rte_ioat_completed_ops(
				tx_config->ioat_ids[i], MAX_PKT_BURST,
				(void *)mbufs_src, (void *)mbufs_dst);
		} else {
			/* Deque the mbufs from rx_to_tx_ring. */
			nb_dq = rte_ring_dequeue_burst(
				tx_config->rx_to_tx_ring, (void *)mbufs_dst,
				MAX_PKT_BURST, NULL);
		}

		if ((int32_t) nb_dq <= 0)
			return;

		if (copy_mode == COPY_MODE_IOAT_NUM)
			rte_mempool_put_bulk(ioat_pktmbuf_pool,
				(void *)mbufs_src, nb_dq);

		/* Update macs if enabled */
		if (mac_updating) {
			for (j = 0; j < nb_dq; j++)
				update_mac_addrs(mbufs_dst[j],
					tx_config->rxtx_port);
		}

		const uint16_t nb_tx = rte_eth_tx_burst(
			tx_config->rxtx_port, 0,
			(void *)mbufs_dst, nb_dq);

		port_statistics.tx[tx_config->rxtx_port] += nb_tx;

		/* Free any unsent packets. */
		if (unlikely(nb_tx < nb_dq))
			rte_mempool_put_bulk(ioat_pktmbuf_pool,
			(void *)&mbufs_dst[nb_tx],
				nb_dq - nb_tx);
	}
}

/* Main rx processing loop for IOAT rawdev. */
static void
rx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, IOAT, "Entering main rx loop for copy on lcore %u\n",
		rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++)
			ioat_rx_port(&cfg.ports[i]);
}

/* Main tx processing loop for hardware copy. */
static void
tx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, IOAT, "Entering main tx loop for copy on lcore %u\n",
		rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++)
			ioat_tx_port(&cfg.ports[i]);
}

/* Main rx and tx loop if only one worker lcore available */
static void
rxtx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, IOAT, "Entering main rx and tx loop for copy on"
		" lcore %u\n", rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++) {
			ioat_rx_port(&cfg.ports[i]);
			ioat_tx_port(&cfg.ports[i]);
		}
}

static void start_forwarding_cores(void)
{
	uint32_t lcore_id = rte_lcore_id();

	RTE_LOG(INFO, IOAT, "Entering %s on lcore %u\n",
		__func__, rte_lcore_id());

	if (cfg.nb_lcores == 1) {
		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)rxtx_main_loop,
			NULL, lcore_id);
	} else if (cfg.nb_lcores > 1) {
		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)rx_main_loop,
			NULL, lcore_id);

		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)tx_main_loop, NULL,
			lcore_id);
	}
}

/* Display usage */
static void
ioat_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
		"  -p --portmask: hexadecimal bitmask of ports to configure\n"
		"  -q NQ: number of RX queues per port (default is 1)\n"
		"  --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)\n"
		"      When enabled:\n"
		"       - The source MAC address is replaced by the TX port MAC address\n"
		"       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n"
		"  -c --copy-type CT: type of copy: sw|hw\n"
		"  -s --ring-size RS: size of IOAT rawdev ring for hardware copy mode or rte_ring for software copy mode\n",
			prgname);
}

static int
ioat_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* Parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static copy_mode_t
ioat_parse_copy_mode(const char *copy_mode)
{
	if (strcmp(copy_mode, COPY_MODE_SW) == 0)
		return COPY_MODE_SW_NUM;
	else if (strcmp(copy_mode, COPY_MODE_IOAT) == 0)
		return COPY_MODE_IOAT_NUM;

	return COPY_MODE_INVALID_NUM;
}

/* Parse the argument given in the command line of the application */
static int
ioat_parse_args(int argc, char **argv, unsigned int nb_ports)
{
	static const char short_options[] =
		"p:"  /* portmask */
		"q:"  /* number of RX queues per port */
		"c:"  /* copy type (sw|hw) */
		"s:"  /* ring size */
		;

	static const struct option lgopts[] = {
		{CMD_LINE_OPT_MAC_UPDATING, no_argument, &mac_updating, 1},
		{CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, &mac_updating, 0},
		{CMD_LINE_OPT_PORTMASK, required_argument, NULL, 'p'},
		{CMD_LINE_OPT_NB_QUEUE, required_argument, NULL, 'q'},
		{CMD_LINE_OPT_COPY_TYPE, required_argument, NULL, 'c'},
		{CMD_LINE_OPT_RING_SIZE, required_argument, NULL, 's'},
		{NULL, 0, 0, 0}
	};

	const unsigned int default_port_mask = (1 << nb_ports) - 1;
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	ioat_enabled_port_mask = default_port_mask;
	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options,
			lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			ioat_enabled_port_mask = ioat_parse_portmask(optarg);
			if (ioat_enabled_port_mask & ~default_port_mask ||
					ioat_enabled_port_mask <= 0) {
				printf("Invalid portmask, %s, suggest 0x%x\n",
						optarg, default_port_mask);
				ioat_usage(prgname);
				return -1;
			}
			break;

		case 'q':
			nb_queues = atoi(optarg);
			if (nb_queues == 0 || nb_queues > MAX_RX_QUEUES_COUNT) {
				printf("Invalid RX queues number %s. Max %u\n",
					optarg, MAX_RX_QUEUES_COUNT);
				ioat_usage(prgname);
				return -1;
			}
			break;

		case 'c':
			copy_mode = ioat_parse_copy_mode(optarg);
			if (copy_mode == COPY_MODE_INVALID_NUM) {
				printf("Invalid copy type. Use: sw, hw\n");
				ioat_usage(prgname);
				return -1;
			}
			break;

		case 's':
			ring_size = atoi(optarg);
			if (ring_size == 0) {
				printf("Invalid ring size, %s.\n", optarg);
				ioat_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			break;

		default:
			ioat_usage(prgname);
			return -1;
		}
	}

	printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");
	if (optind >= 0)
		argv[optind - 1] = prgname;

	ret = optind - 1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/* check link status, return true if at least one port is up */
static int
check_link_status(uint32_t port_mask)
{
	uint16_t portid;
	struct rte_eth_link link;
	int ret, link_status = 0;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status\n");
	RTE_ETH_FOREACH_DEV(portid) {
		if ((port_mask & (1 << portid)) == 0)
			continue;

		memset(&link, 0, sizeof(link));
		ret = rte_eth_link_get(portid, &link);
		if (ret < 0) {
			printf("Port %u link get failed: err=%d\n",
					portid, ret);
			continue;
		}

		/* Print link status */
		rte_eth_link_to_str(link_status_text,
			sizeof(link_status_text), &link);
		printf("Port %d %s\n", portid, link_status_text);

		if (link.link_status)
			link_status = 1;
	}
	return link_status;
}

static void
configure_rawdev_queue(uint32_t dev_id)
{
	struct rte_ioat_rawdev_config dev_config = { .ring_size = ring_size };
	struct rte_rawdev_info info = { .dev_private = &dev_config };

	if (rte_rawdev_configure(dev_id, &info, sizeof(dev_config)) != 0) {
		rte_exit(EXIT_FAILURE,
			"Error with rte_rawdev_configure()\n");
	}
	if (rte_rawdev_start(dev_id) != 0) {
		rte_exit(EXIT_FAILURE,
			"Error with rte_rawdev_start()\n");
	}
}

static void
assign_rawdevs(void)
{
	uint16_t nb_rawdev = 0, rdev_id = 0;
	uint32_t i, j;

	for (i = 0; i < cfg.nb_ports; i++) {
		for (j = 0; j < cfg.ports[i].nb_queues; j++) {
			struct rte_rawdev_info rdev_info = { 0 };

			do {
				if (rdev_id == rte_rawdev_count())
					goto end;
				rte_rawdev_info_get(rdev_id++, &rdev_info, 0);
			} while (rdev_info.driver_name == NULL ||
					strcmp(rdev_info.driver_name,
						IOAT_PMD_RAWDEV_NAME_STR) != 0);

			cfg.ports[i].ioat_ids[j] = rdev_id - 1;
			configure_rawdev_queue(cfg.ports[i].ioat_ids[j]);
			++nb_rawdev;
		}
	}
end:
	if (nb_rawdev < cfg.nb_ports * cfg.ports[0].nb_queues)
		rte_exit(EXIT_FAILURE,
			"Not enough IOAT rawdevs (%u) for all queues (%u).\n",
			nb_rawdev, cfg.nb_ports * cfg.ports[0].nb_queues);
	RTE_LOG(INFO, IOAT, "Number of used rawdevs: %u.\n", nb_rawdev);
}

static void
assign_rings(void)
{
	uint32_t i;

	for (i = 0; i < cfg.nb_ports; i++) {
		char ring_name[RTE_RING_NAMESIZE];

		snprintf(ring_name, sizeof(ring_name), "rx_to_tx_ring_%u", i);
		/* Create ring for inter core communication */
		cfg.ports[i].rx_to_tx_ring = rte_ring_create(
			ring_name, ring_size,
			rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (cfg.ports[i].rx_to_tx_ring == NULL)
			rte_exit(EXIT_FAILURE, "Ring create failed: %s\n",
				rte_strerror(rte_errno));
	}
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline void
port_init(uint16_t portid, struct rte_mempool *mbuf_pool, uint16_t nb_queues)
{
	/* configuring port to use RSS for multiple RX queues */
	static const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_PROTO_MASK,
			}
		}
	};

	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_conf local_port_conf = port_conf;
	struct rte_eth_dev_info dev_info;
	int ret, i;

	/* Skip ports that are not enabled */
	if ((ioat_enabled_port_mask & (1 << portid)) == 0) {
		printf("Skipping disabled port %u\n", portid);
		return;
	}

	/* Init port */
	printf("Initializing port %u... ", portid);
	fflush(stdout);
	ret = rte_eth_dev_info_get(portid, &dev_info);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot get device info: %s, port=%u\n",
			rte_strerror(-ret), portid);

	local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	ret = rte_eth_dev_configure(portid, nb_queues, 1, &local_port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device:"
			" err=%d, port=%u\n", ret, portid);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
			&nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"Cannot adjust number of descriptors: err=%d, port=%u\n",
			ret, portid);

	rte_eth_macaddr_get(portid, &ioat_ports_eth_addr[portid]);

	/* Init RX queues */
	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	for (i = 0; i < nb_queues; i++) {
		ret = rte_eth_rx_queue_setup(portid, i, nb_rxd,
			rte_eth_dev_socket_id(portid), &rxq_conf,
			mbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup:err=%d,port=%u, queue_id=%u\n",
				ret, portid, i);
	}

	/* Init one TX queue on each port */
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
			rte_eth_dev_socket_id(portid),
			&txq_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_tx_queue_setup:err=%d,port=%u\n",
			ret, portid);

	/* Initialize TX buffers */
	tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
			rte_eth_dev_socket_id(portid));
	if (tx_buffer[portid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Cannot allocate buffer for tx on port %u\n",
			portid);

	rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
		rte_eth_tx_buffer_count_callback,
		&port_statistics.tx_dropped[portid]);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
			portid);

	/* Start device */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, portid);

	rte_eth_promiscuous_enable(portid);

	printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
			portid,
			ioat_ports_eth_addr[portid].addr_bytes[0],
			ioat_ports_eth_addr[portid].addr_bytes[1],
			ioat_ports_eth_addr[portid].addr_bytes[2],
			ioat_ports_eth_addr[portid].addr_bytes[3],
			ioat_ports_eth_addr[portid].addr_bytes[4],
			ioat_ports_eth_addr[portid].addr_bytes[5]);

	cfg.ports[cfg.nb_ports].rxtx_port = portid;
	cfg.ports[cfg.nb_ports++].nb_queues = nb_queues;
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
			signum);
		force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t nb_ports, portid;
	uint32_t i;
	unsigned int nb_mbufs;

	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* Parse application arguments (after the EAL ones) */
	ret = ioat_parse_args(argc, argv, nb_ports);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid IOAT arguments\n");

	nb_mbufs = RTE_MAX(nb_ports * (nb_queues * (nb_rxd + nb_txd +
		4 * MAX_PKT_BURST) + rte_lcore_count() * MEMPOOL_CACHE_SIZE),
		MIN_POOL_SIZE);

	/* Create the mbuf pool */
	ioat_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if (ioat_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	/* Initialise each port */
	cfg.nb_ports = 0;
	RTE_ETH_FOREACH_DEV(portid)
		port_init(portid, ioat_pktmbuf_pool, nb_queues);

	/* Initialize port xstats */
	memset(&port_statistics, 0, sizeof(port_statistics));

	while (!check_link_status(ioat_enabled_port_mask) && !force_quit)
		sleep(1);

	/* Check if there is enough lcores for all ports. */
	cfg.nb_lcores = rte_lcore_count() - 1;
	if (cfg.nb_lcores < 1)
		rte_exit(EXIT_FAILURE,
			"There should be at least one worker lcore.\n");

	if (copy_mode == COPY_MODE_IOAT_NUM)
		assign_rawdevs();
	else /* copy_mode == COPY_MODE_SW_NUM */
		assign_rings();

	start_forwarding_cores();
	/* main core prints stats while other cores forward */
	print_stats(argv[0]);

	/* force_quit is true when we get here */
	rte_eal_mp_wait_lcore();

	uint32_t j;
	for (i = 0; i < cfg.nb_ports; i++) {
		printf("Closing port %d\n", cfg.ports[i].rxtx_port);
		ret = rte_eth_dev_stop(cfg.ports[i].rxtx_port);
		if (ret != 0)
			RTE_LOG(ERR, IOAT, "rte_eth_dev_stop: err=%s, port=%u\n",
				rte_strerror(-ret), cfg.ports[i].rxtx_port);

		rte_eth_dev_close(cfg.ports[i].rxtx_port);
		if (copy_mode == COPY_MODE_IOAT_NUM) {
			for (j = 0; j < cfg.ports[i].nb_queues; j++) {
				printf("Stopping rawdev %d\n",
					cfg.ports[i].ioat_ids[j]);
				rte_rawdev_stop(cfg.ports[i].ioat_ids[j]);
			}
		} else /* copy_mode == COPY_MODE_SW_NUM */
			rte_ring_free(cfg.ports[i].rx_to_tx_ring);
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	printf("Bye...\n");
	return 0;
}
