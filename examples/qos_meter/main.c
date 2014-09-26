/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_eal.h>
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
#define MBUF_SIZE           (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
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
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0,
		.hw_ip_checksum = 1,
		.hw_vlan_filter = 0,
		.jumbo_frame    = 0,
		.hw_strip_crc   = 0,
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

#define NIC_RX_QUEUE_DESC               128
#define NIC_TX_QUEUE_DESC               512

#define NIC_RX_QUEUE                    0
#define NIC_TX_QUEUE                    0

/*
 * Packet RX/TX
 *
 ***/
#define PKT_RX_BURST_MAX                32
#define PKT_TX_BURST_MAX                32
#define TIME_TX_DRAIN                   200000ULL

static uint8_t port_rx;
static uint8_t port_tx;
static struct rte_mbuf *pkts_rx[PKT_RX_BURST_MAX];
static struct rte_mbuf *pkts_tx[PKT_TX_BURST_MAX];
static uint16_t pkts_tx_len = 0;


struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000 * 46,  .cbs = 2048, .ebs = 2048},
};

struct rte_meter_trtcm_params app_trtcm_params[] = {
	{.cir = 1000000 * 46,  .pir = 1500000 * 46,  .cbs = 2048, .pbs = 2048},
};

#define APP_FLOWS_MAX  256

FLOW_METER app_flows[APP_FLOWS_MAX];

static void
app_configure_flow_table(void)
{
	uint32_t i, j;

	for (i = 0, j = 0; i < APP_FLOWS_MAX; i ++, j = (j + 1) % RTE_DIM(PARAMS)){
		FUNC_CONFIG(&app_flows[i], &PARAMS[j]);
	}
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
	uint32_t pkt_len = rte_pktmbuf_pkt_len(pkt) - sizeof(struct ether_hdr);
	uint8_t flow_id = (uint8_t)(pkt_data[APP_PKT_FLOW_POS] & (APP_FLOWS_MAX - 1));
	input_color = pkt_data[APP_PKT_COLOR_POS];
	enum policer_action action;

	/* color input is not used for blind modes */
	output_color = (uint8_t) FUNC_METER(&app_flows[flow_id], time, pkt_len,
		(enum rte_meter_color) input_color);

	/* Apply policing and set the output color */
	action = policer_table[input_color][output_color];
	app_set_pkt_color(pkt_data, action);

	return action;
}


static __attribute__((noreturn)) int
main_loop(__attribute__((unused)) void *dummy)
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
			int ret;

			if (pkts_tx_len == 0) {
				last_time = current_time;

				continue;
			}

			/* Write packet burst to NIC TX */
			ret = rte_eth_tx_burst(port_tx, NIC_TX_QUEUE, pkts_tx, pkts_tx_len);

			/* Free buffers for any packets not written successfully */
			if (unlikely(ret < pkts_tx_len)) {
				for ( ; ret < pkts_tx_len; ret ++) {
					rte_pktmbuf_free(pkts_tx[ret]);
				}
			}

			/* Empty the output buffer */
			pkts_tx_len = 0;

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
			else {
				pkts_tx[pkts_tx_len] = pkt;
				pkts_tx_len ++;
			}

			/* Write packets from output buffer to NIC TX when full burst is available */
			if (unlikely(pkts_tx_len == PKT_TX_BURST_MAX)) {
				/* Write packet burst to NIC TX */
				int ret = rte_eth_tx_burst(port_tx, NIC_TX_QUEUE, pkts_tx, PKT_TX_BURST_MAX);

				/* Free buffers for any packets not written successfully */
				if (unlikely(ret < PKT_TX_BURST_MAX)) {
					for ( ; ret < PKT_TX_BURST_MAX; ret ++) {
						rte_pktmbuf_free(pkts_tx[ret]);
					}
				}

				/* Empty the output buffer */
				pkts_tx_len = 0;
			}
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
		return -1;

	if (pm == 0)
		return -1;

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

	optind = 0; /* reset getopt lib */
	return 0;
}

int
main(int argc, char **argv)
{
	uint32_t lcore_id;
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
	pool = rte_mempool_create("pool", NB_MBUF, MBUF_SIZE, MEMPOOL_CACHE_SIZE,
		sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init, NULL,
		rte_pktmbuf_init, NULL, rte_socket_id(), 0);
	if (pool == NULL)
		rte_exit(EXIT_FAILURE, "Buffer pool creation error\n");

	/* NIC init */
	ret = rte_eth_dev_configure(port_rx, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d configuration error (%d)\n", port_rx, ret);

	ret = rte_eth_rx_queue_setup(port_rx, NIC_RX_QUEUE, NIC_RX_QUEUE_DESC,
				rte_eth_dev_socket_id(port_rx),
				NULL, pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d RX queue setup error (%d)\n", port_rx, ret);

	ret = rte_eth_tx_queue_setup(port_rx, NIC_TX_QUEUE, NIC_TX_QUEUE_DESC,
				rte_eth_dev_socket_id(port_rx),
				NULL);
	if (ret < 0)
	rte_exit(EXIT_FAILURE, "Port %d TX queue setup error (%d)\n", port_rx, ret);

	ret = rte_eth_dev_configure(port_tx, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d configuration error (%d)\n", port_tx, ret);

	ret = rte_eth_rx_queue_setup(port_tx, NIC_RX_QUEUE, NIC_RX_QUEUE_DESC,
				rte_eth_dev_socket_id(port_tx),
				NULL, pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d RX queue setup error (%d)\n", port_tx, ret);

	ret = rte_eth_tx_queue_setup(port_tx, NIC_TX_QUEUE, NIC_TX_QUEUE_DESC,
				rte_eth_dev_socket_id(port_tx),
				NULL);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d TX queue setup error (%d)\n", port_tx, ret);

	ret = rte_eth_dev_start(port_rx);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d start error (%d)\n", port_rx, ret);

	ret = rte_eth_dev_start(port_tx);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Port %d start error (%d)\n", port_tx, ret);

	rte_eth_promiscuous_enable(port_rx);

	rte_eth_promiscuous_enable(port_tx);

	/* App configuration */
	app_configure_flow_table();

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
