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
		return -1;

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
	int retval = 0;

	printf("\nChecking link status\n");
	RTE_ETH_FOREACH_DEV(portid) {
		if ((port_mask & (1 << portid)) == 0)
			continue;

		memset(&link, 0, sizeof(link));
		rte_eth_link_get(portid, &link);

		/* Print link status */
		if (link.link_status) {
			printf(
				"Port %d Link Up. Speed %u Mbps - %s\n",
				portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				("full-duplex") : ("half-duplex\n"));
			retval = 1;
		} else
			printf("Port %d Link Down\n", portid);
	}
	return retval;
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
	rte_eth_dev_info_get(portid, &dev_info);
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

	while (!check_link_status(ioat_enabled_port_mask) && !force_quit)
		sleep(1);

	/* Check if there is enough lcores for all ports. */
	cfg.nb_lcores = rte_lcore_count() - 1;
	if (cfg.nb_lcores < 1)
		rte_exit(EXIT_FAILURE,
			"There should be at least one slave lcore.\n");
	for (i = 0; i < cfg.nb_ports; i++) {
		printf("Closing port %d\n", cfg.ports[i].rxtx_port);
		rte_eth_dev_stop(cfg.ports[i].rxtx_port);
		rte_eth_dev_close(cfg.ports[i].rxtx_port);
	}

	printf("Bye...\n");
	return 0;
}
