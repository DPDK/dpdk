/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdio.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_meter.h>

/*
 * Traffic metering configuration
 *
 */
#define APP_MODE_FWD                    0
#define APP_MODE_SRTCM_COLOR_BLIND      1
#define APP_MODE_SRTCM_COLOR_AWARE      2
#define APP_MODE_TRTCM_COLOR_BLIND      3
#define APP_MODE_TRTCM_COLOR_AWARE      4

#define APP_MODE	APP_MODE_SRTCM_COLOR_BLIND


#include "main.h"


#define APP_PKT_FLOW_POS                33
#define APP_PKT_COLOR_POS               5


#if APP_PKT_FLOW_POS > 64 || APP_PKT_COLOR_POS > 64
#error Byte offset needs to be less than 64
#endif

/*
 * Buffer pool configuration
 *
 ***/
#define NB_MBUF             8192
#define MEMPOOL_CACHE_SIZE  256

static struct rte_mempool *pool = NULL;

/*
 * NIC configuration
 *
 ***/
static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= ETH_MQ_RX_RSS,
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.offloads = DEV_RX_OFFLOAD_CHECKSUM,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_DCB_NONE,
	},
};

#define NIC_RX_QUEUE_DESC               1024
#define NIC_TX_QUEUE_DESC               1024

#define NIC_RX_QUEUE                    0
#define NIC_TX_QUEUE                    0

/*
 * Packet RX/TX
 *
 ***/
#define PKT_RX_BURST_MAX                32
#define PKT_TX_BURST_MAX                32
#define TIME_TX_DRAIN                   200000ULL

static uint16_t port_rx;
static uint16_t port_tx;
static struct rte_mbuf *pkts_rx[PKT_RX_BURST_MAX];
struct rte_eth_dev_tx_buffer *tx_buffer;

struct rte_meter_srtcm_params app_srtcm_params = {
	.cir = 1000000 * 46,
	.cbs = 2048,
	.ebs = 2048
};

struct rte_meter_srtcm_profile app_srtcm_profile;

struct rte_meter_trtcm_params app_trtcm_params = {
	.cir = 1000000 * 46,
	.pir = 1500000 * 46,
	.cbs = 2048,
	.pbs = 2048
};

struct rte_meter_trtcm_profile app_trtcm_profile;

#define APP_FLOWS_MAX  256

FLOW_METER app_flows[APP_FLOWS_MAX];

static int
app_configure_flow_table(void)
{
	uint32_t i;
	int ret;

	ret = rte_meter_srtcm_profile_config(&app_srtcm_profile,
		&app_srtcm_params);
	if (ret)
		return ret;

	ret = rte_meter_trtcm_profile_config(&app_trtcm_profile,
		&app_trtcm_params);
	if (ret)
		return ret;

	for (i = 0; i < APP_FLOWS_MAX; i++) {
		ret = FUNC_CONFIG(&app_flows[i], &PROFILE);
		if (ret)
			return ret;
	}

	return 0;
}

static inline void
app_set_pkt_color(uint8_t *pkt_data, enum policer_action color)
{
	pkt_data[APP_PKT_COLOR_POS] = (uint8_t)color;
}

static inline int
app_pkt_handle(struct rte_mbuf *pkt, uint64_t time)
{
	uint8_t input_color, output_color;
	uint8_t *pkt_data = rte_pktmbuf_mtod(pkt, uint8_t *);
	uint32_t pkt_len = rte_pktmbuf_pkt_len(pkt) -
		sizeof(struct rte_ether_hdr);
	uint8_t flow_id = (uint8_t)(pkt_data[APP_PKT_FLOW_POS] & (APP_FLOWS_MAX - 1));
	input_color = pkt_data[APP_PKT_COLOR_POS];
	enum policer_action action;

	/* color input is not used for blind modes */
	output_color = (uint8_t) FUNC_METER(&app_flows[flow_id],
		&PROFILE,
		time,
		pkt_len,
		(enum rte_color) input_color);

	/* Apply policing and set the output color */
	action = policer_table[input_color][output_color];
	app_set_pkt_color(pkt_data, action);

	return action;
}


static __rte_noreturn int
main_loop(__rte_unused void *dummy)
{
	uint64_t current_time, last_time = rte_rdtsc();
	uint32_t lcore_id = rte_lcore_id();

	printf("Core %u: port RX = %d, port TX = %d\n", lcore_id, port_rx, port_tx);

	while (1) {
		uint64_t time_diff;
		int i, nb_rx;

		/* Mechanism to avoid stale packets in the output buffer */
		current_time = rte_rdtsc();
		time_diff = current_time - last_time;
		if (unlikely(time_diff > TIME_TX_DRAIN)) {
			/* Flush tx buffer */
			rte_eth_tx_buffer_flush(port_tx, NIC_TX_QUEUE, tx_buffer);
			last_time = current_time;
		}

		/* Read packet burst from NIC RX */
		nb_rx = rte_eth_rx_burst(port_rx, NIC_RX_QUEUE, pkts_rx, PKT_RX_BURST_MAX);

		/* Handle packets */
		for (i = 0; i < nb_rx; i ++) {
			struct rte_mbuf *pkt = pkts_rx[i];

			/* Handle current packet */
			if (app_pkt_handle(pkt, current_time) == DROP)
				rte_pktmbuf_free(pkt);
			else
				rte_eth_tx_buffer(port_tx, NIC_TX_QUEUE, tx_buffer, pkt);
		}
	}
}

static void
print_usage(const char *prgname)
{
	printf ("%s [EAL options] -- -p PORTMASK\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n",
		prgname);
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};
	uint64_t port_mask, i, mask;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:", lgopts, &option_index)) != EOF) {
		switch (opt) {
		case 'p':
			port_mask = parse_portmask(optarg);
			if (port_mask == 0) {
				printf("invalid port mask (null port mask)\n");
				print_usage(prgname);
				return -1;
			}

			for (i = 0, mask = 1; i < 64; i ++, mask <<= 1){
				if (mask & port_mask){
					port_rx = i;
					port_mask &= ~ mask;
					break;
				}
			}

			for (i = 0, mask = 1; i < 64; i ++, mask <<= 1){
				if (mask & port_mask){
					port_tx = i;
					port_mask &= ~ mask;
					break;
				}
			}

			if (port_mask != 0) {
				printf("invalid port mask (more than 2 ports)\n");
				print_usage(prgname);
				return -1;
			}
			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (optind <= 1) {
		print_usage(prgname);
		return -1;
	}

	argv[optind-1] = prgname;

	optind = 1; /* reset getopt lib */
	return 0;
}

int
main(int argc, char **argv)
{
	uint32_t lcore_id;
	uint16_t nb_rxd = NIC_RX_QUEUE_DESC;
	uint16_t nb_txd = NIC_TX_QUEUE_DESC;
	struct rte_eth_conf conf;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_dev_info dev_info;
	int ret;

	/* EAL init */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;
	if (rte_lcore_count() != 1) {
		rte_exit(EXIT_FAILURE, "This application does not accept more than one core. "
		"Please adjust the \"-c COREMASK\" parameter accordingly.\n");
	}

	/* Application non-EAL arguments parse */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid input arguments\n");

	/* Buffer pool init */
	pool = rte_pktmbuf_pool_create("pool", NB_MBUF, MEMPOOL_CACHE_SIZE,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (pool == NULL)
		rte_exit(EXIT_FAILURE, "Buffer pool creation error\n");

	/* NIC init */
	conf = port_conf;

	ret = rte_eth_dev_info_get(port_rx, &dev_info);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			port_rx, strerror(-ret));

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;
	if (conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf.rx_adv_conf.rss_conf.rss_hf) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			port_rx,
			port_conf.rx_adv_conf.rss_conf.rss_hf,
			conf.rx_adv_conf.rss_conf.rss_hf);
	}

	ret = rte_eth_dev_configure(port_rx, 1, 1, &conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d configuration error (%d)\n", port_rx, ret);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_rx, &nb_rxd, &nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d adjust number of descriptors error (%d)\n",
				port_rx, ret);

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = conf.rxmode.offloads;
	ret = rte_eth_rx_queue_setup(port_rx, NIC_RX_QUEUE, nb_rxd,
				rte_eth_dev_socket_id(port_rx),
				&rxq_conf, pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d RX queue setup error (%d)\n", port_rx, ret);

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(port_rx, NIC_TX_QUEUE, nb_txd,
				rte_eth_dev_socket_id(port_rx),
				&txq_conf);
	if (ret < 0)
	rte_exit(EXIT_FAILURE, "Port %d TX queue setup error (%d)\n", port_rx, ret);

	conf = port_conf;

	ret = rte_eth_dev_info_get(port_tx, &dev_info);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			port_tx, strerror(-ret));

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;
	if (conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf.rx_adv_conf.rss_conf.rss_hf) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			port_tx,
			port_conf.rx_adv_conf.rss_conf.rss_hf,
			conf.rx_adv_conf.rss_conf.rss_hf);
	}

	ret = rte_eth_dev_configure(port_tx, 1, 1, &conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d configuration error (%d)\n", port_tx, ret);

	nb_rxd = NIC_RX_QUEUE_DESC;
	nb_txd = NIC_TX_QUEUE_DESC;
	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_tx, &nb_rxd, &nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d adjust number of descriptors error (%d)\n",
				port_tx, ret);

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = conf.rxmode.offloads;
	ret = rte_eth_rx_queue_setup(port_tx, NIC_RX_QUEUE, nb_rxd,
				rte_eth_dev_socket_id(port_tx),
				NULL, pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d RX queue setup error (%d)\n", port_tx, ret);

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(port_tx, NIC_TX_QUEUE, nb_txd,
				rte_eth_dev_socket_id(port_tx),
				NULL);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d TX queue setup error (%d)\n", port_tx, ret);

	tx_buffer = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(PKT_TX_BURST_MAX), 0,
			rte_eth_dev_socket_id(port_tx));
	if (tx_buffer == NULL)
		rte_exit(EXIT_FAILURE, "Port %d TX buffer allocation error\n",
				port_tx);

	rte_eth_tx_buffer_init(tx_buffer, PKT_TX_BURST_MAX);

	ret = rte_eth_dev_start(port_rx);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d start error (%d)\n", port_rx, ret);

	ret = rte_eth_dev_start(port_tx);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d start error (%d)\n", port_tx, ret);

	ret = rte_eth_promiscuous_enable(port_rx);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Port %d promiscuous mode enable error (%s)\n",
			port_rx, rte_strerror(-ret));

	ret = rte_eth_promiscuous_enable(port_tx);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Port %d promiscuous mode enable error (%s)\n",
			port_rx, rte_strerror(-ret));

	/* App configuration */
	ret = app_configure_flow_table();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid configure flow table\n");

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
