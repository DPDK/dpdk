/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 * 
 */

#include <stdint.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <inttypes.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>

#include "main.h"

/* basic constants used in application */
#define NUM_QUEUES 128

#define NUM_MBUFS 64*1024
#define MBUF_CACHE_SIZE 64
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

#define RX_PORT 0
#define TX_PORT 1

/* number of pools (if user does not specify any, 16 by default */
static enum rte_eth_nb_pools num_pools = ETH_16_POOLS;

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
/* Default configuration for rx and tx thresholds etc. */
static const struct rte_eth_rxconf rx_conf_default = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
};

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
static const struct rte_eth_txconf tx_conf_default = {
	.tx_thresh = {
		.pthresh = 36,
		.hthresh = 0,
		.wthresh = 0,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

/* empty vmdq+dcb configuration structure. Filled in programatically */
static const struct rte_eth_conf vmdq_dcb_conf_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_DCB,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		/*
		 * should be overridden separately in code with
		 * appropriate values
		 */
		.vmdq_dcb_conf = {
			.nb_queue_pools = ETH_16_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
			.dcb_queue = {0},
		},
	},
};

/* array used for printing out statistics */
volatile unsigned long rxPackets[ NUM_QUEUES ] = {0};

const uint16_t vlan_tags[] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,	12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31
};

/* Builds up the correct configuration for vmdq+dcb based on the vlan tags array
 * given above, and the number of traffic classes available for use. */
static inline int
get_eth_conf(struct rte_eth_conf *eth_conf, enum rte_eth_nb_pools num_pools)
{
	struct rte_eth_vmdq_dcb_conf conf;
	unsigned i;

	if (num_pools != ETH_16_POOLS && num_pools != ETH_32_POOLS ) return -1;

	conf.nb_queue_pools = num_pools;
	conf.enable_default_pool = 0;
	conf.default_pool = 0; /* set explicit value, even if not used */
	conf.nb_pool_maps = sizeof( vlan_tags )/sizeof( vlan_tags[ 0 ]);
	for (i = 0; i < conf.nb_pool_maps; i++){
		conf.pool_map[i].vlan_id = vlan_tags[ i ];
		conf.pool_map[i].pools = 1 << (i % num_pools);
	}
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++){
		conf.dcb_queue[i] = (uint8_t)(i % (NUM_QUEUES/num_pools));
	}
	(void)(rte_memcpy(eth_conf, &vmdq_dcb_conf_default, sizeof(*eth_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_dcb_conf, &conf,
		   sizeof(eth_conf->rx_adv_conf.vmdq_dcb_conf)));
	return 0;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf;
	const uint16_t rxRings = ETH_VMDQ_DCB_NUM_QUEUES,
		txRings = (uint16_t)rte_lcore_count();
	const uint16_t rxRingSize = 128, txRingSize = 512;
	int retval;
	uint16_t q;

	retval = get_eth_conf(&port_conf, num_pools);
	if (retval < 0)
		return retval;

	if (port >= rte_eth_dev_count()) return -1;

	retval = rte_eth_dev_configure(port, rxRings, txRings, &port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rxRings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, rxRingSize,
						rte_eth_dev_socket_id(port), &rx_conf_default,
						mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < txRings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, txRingSize,
						rte_eth_dev_socket_id(port), &tx_conf_default);
		if (retval < 0)
			return retval;
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	return 0;
}

/* Check num_pools parameter and set it if OK*/
static int
vmdq_parse_num_pools(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n != 16 && n != 32)
		return -1;
	if (n == 16)
		num_pools = ETH_16_POOLS;
	else
		num_pools = ETH_32_POOLS;

	return 0;
}

/* Display usage */
static void
vmdq_usage(const char *prgname)
{
	printf("%s [EAL options] -- [-nb-pools NP]\n"
	       "  -nb-pools NP: number of pools (16 default, 32)\n",
	       prgname);
}

/*  Parse the argument (num_pools) given in the command line of the application */
static int
vmdq_parse_args(int argc, char **argv)
{
	int opt;
	int option_index;
	const char *prgname = argv[0];

	static struct option long_option[] = {
		{"nb-pools", required_argument, NULL, 0},
		{NULL, 0, 0, 0}
	};

	/* Parse command line */
	while ((opt = getopt_long_only(argc, argv, "",long_option,&option_index)) != EOF) {
		switch (opt) {
		case 0:
			if (vmdq_parse_num_pools(optarg) == -1){
				printf("invalid number of pools\n");
				vmdq_usage(prgname);
				return -1;
			}
			break;
		default:
			vmdq_usage(prgname);
			return -1;
		}
	}
	return 0;


}


#ifndef RTE_EXEC_ENV_BAREMETAL
/* When we receive a HUP signal, print out our stats */
static void
sighup_handler(int signum)
{
	unsigned q;
	for (q = 0; q < NUM_QUEUES; q ++) {
		if (q % (NUM_QUEUES/num_pools) == 0)
			printf("\nPool %u: ", q/(NUM_QUEUES/num_pools));
		printf("%lu ", rxPackets[ q ]);
	}
	printf("\nFinished handling signal %d\n", signum);
}
#endif

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static  __attribute__((noreturn)) int
lcore_main(void *arg)
{
	const uintptr_t core_num = (uintptr_t)arg;
	const unsigned num_cores = rte_lcore_count();
	uint16_t startQueue = (uint16_t)(core_num * (NUM_QUEUES/num_cores));
	uint16_t endQueue = (uint16_t)(startQueue + (NUM_QUEUES/num_cores));
	uint16_t q, i;

	printf("Core %u(lcore %u) reading queues %i-%i\n", (unsigned)core_num,
	       rte_lcore_id(), startQueue, endQueue - 1);

	for (;;) {
		struct rte_mbuf *buf[32];
		const uint16_t buf_size = sizeof(buf) / sizeof(buf[0]);

		for (q = startQueue; q < endQueue; q++) {
			const uint16_t rxCount = rte_eth_rx_burst(RX_PORT,
					q, buf, buf_size);
			if (rxCount == 0)
				continue;
			rxPackets[q] += rxCount;

			const uint16_t txCount = rte_eth_tx_burst(TX_PORT,
					(uint16_t)core_num, buf, rxCount);
			if (txCount != rxCount) {
				for (i = txCount; i < rxCount; i++)
					rte_pktmbuf_free(buf[i]);
			}
		}
	}
}

/* Main function, does initialisation and calls the per-lcore functions */
int
MAIN(int argc, char *argv[])
{
	unsigned cores;
	struct rte_mempool *mbuf_pool;
	unsigned lcore_id;
	uintptr_t i;
	int ret;

#ifndef RTE_EXEC_ENV_BAREMETAL
	signal(SIGHUP, sighup_handler);
#endif

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* parse app arguments */
	ret = vmdq_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid VMDQ argument\n");

	if (rte_ixgbe_pmd_init() != 0 ||
			rte_eal_pci_probe() != 0)
		rte_exit(EXIT_FAILURE, "Error with NIC driver initialization\n");

	cores = rte_lcore_count();
	if ((cores & (cores - 1)) != 0 || cores > 16) {
		rte_exit(EXIT_FAILURE,
			 "This program can only run on 2,4,8 or 16 cores\n\n");
	}

	mbuf_pool = rte_mempool_create("MBUF_POOL", NUM_MBUFS,
				       MBUF_SIZE, MBUF_CACHE_SIZE,
				       sizeof(struct rte_pktmbuf_pool_private),
				       rte_pktmbuf_pool_init, NULL,
				       rte_pktmbuf_init, NULL,
				       rte_socket_id(), 0);
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	if (port_init(RX_PORT, mbuf_pool) != 0 ||
	    port_init(TX_PORT, mbuf_pool) != 0)
		rte_exit(EXIT_FAILURE, "Cannot initialize network ports\n");

	/* call lcore_main() on every slave lcore */
	i = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_main, (void*)i++, lcore_id);
	}
	/* call on master too */
	(void) lcore_main((void*)i);

	return 0;
}
