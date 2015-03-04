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

/* basic constants used in application */
#define NUM_QUEUES 128

#define NUM_MBUFS 64*1024
#define MBUF_CACHE_SIZE 64
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

#define INVALID_PORT_ID 0xFF

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

/* number of pools (if user does not specify any, 16 by default */
static enum rte_eth_nb_pools num_pools = ETH_16_POOLS;

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

static uint8_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports = 0;

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
						rte_eth_dev_socket_id(port),
						NULL,
						mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < txRings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, txRingSize,
						rte_eth_dev_socket_id(port),
						NULL);
		if (retval < 0)
			return retval;
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
			addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

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

/* Display usage */
static void
vmdq_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK]\n"
	       "  --nb-pools NP: number of pools (16 default, 32)\n",
	       prgname);
}

/*  Parse the argument (num_pools) given in the command line of the application */
static int
vmdq_parse_args(int argc, char **argv)
{
	int opt;
	int option_index;
	unsigned i;
	const char *prgname = argv[0];
	static struct option long_option[] = {
		{"nb-pools", required_argument, NULL, 0},
		{NULL, 0, 0, 0}
	};

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:",long_option,&option_index)) != EOF) {
		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("invalid portmask\n");
				vmdq_usage(prgname);
				return -1;
			}
			break;
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

	for(i = 0; i < RTE_MAX_ETHPORTS; i++)
	{
		if (enabled_port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;
	}

	if (num_ports < 2 || num_ports % 2) {
		printf("Current enabled port number is %u,"
			"but it should be even and at least 2\n",num_ports);
		return -1;
	}

	return 0;
}


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
	uint16_t q, i, p;

	printf("Core %u(lcore %u) reading queues %i-%i\n", (unsigned)core_num,
	       rte_lcore_id(), startQueue, endQueue - 1);

	for (;;) {
		struct rte_mbuf *buf[32];
		const uint16_t buf_size = sizeof(buf) / sizeof(buf[0]);
		for (p = 0; p < num_ports; p++) {
			const uint8_t src = ports[p];
			const uint8_t dst = ports[p ^ 1]; /* 0 <-> 1, 2 <-> 3 etc */

			if ((src == INVALID_PORT_ID) || (dst == INVALID_PORT_ID))
				continue;

			for (q = startQueue; q < endQueue; q++) {
				const uint16_t rxCount = rte_eth_rx_burst(src,
					q, buf, buf_size);
				if (rxCount == 0)
					continue;
				rxPackets[q] += rxCount;

				const uint16_t txCount = rte_eth_tx_burst(dst,
					(uint16_t)core_num, buf, rxCount);
				if (txCount != rxCount) {
					for (i = txCount; i < rxCount; i++)
						rte_pktmbuf_free(buf[i]);
				}
			}
		}
	}
}

/*
 * Update the global var NUM_PORTS and array PORTS according to system ports number
 * and return valid ports number
 */
static unsigned check_ports_num(unsigned nb_ports)
{
	unsigned valid_num_ports = num_ports;
	unsigned portid;

	if (num_ports > nb_ports) {
		printf("\nSpecified port number(%u) exceeds total system port number(%u)\n",
			num_ports, nb_ports);
		num_ports = nb_ports;
	}

	for (portid = 0; portid < num_ports; portid ++) {
		if (ports[portid] >= nb_ports) {
			printf("\nSpecified port ID(%u) exceeds max system port ID(%u)\n",
				ports[portid], (nb_ports - 1));
			ports[portid] = INVALID_PORT_ID;
			valid_num_ports --;
		}
	}
	return valid_num_ports;
}


/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	unsigned cores;
	struct rte_mempool *mbuf_pool;
	unsigned lcore_id;
	uintptr_t i;
	int ret;
	unsigned nb_ports, valid_num_ports;
	uint8_t portid;

	signal(SIGHUP, sighup_handler);

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

	cores = rte_lcore_count();
	if ((cores & (cores - 1)) != 0 || cores > 128) {
		rte_exit(EXIT_FAILURE,"This program can only run on an even"
				"number of cores(1-128)\n\n");
	}

	nb_ports = rte_eth_dev_count();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

        /*
	 * Update the global var NUM_PORTS and global array PORTS
	 * and get value of var VALID_NUM_PORTS according to system ports number
	 */
	valid_num_ports = check_ports_num(nb_ports);

	if (valid_num_ports < 2 || valid_num_ports % 2) {
		printf("Current valid ports number is %u\n", valid_num_ports);
		rte_exit(EXIT_FAILURE, "Error with valid ports number is not even or less than 2\n");
	}

	mbuf_pool = rte_mempool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
				       MBUF_SIZE, MBUF_CACHE_SIZE,
				       sizeof(struct rte_pktmbuf_pool_private),
				       rte_pktmbuf_pool_init, NULL,
				       rte_pktmbuf_init, NULL,
				       rte_socket_id(), 0);
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize network ports\n");
	}

	/* call lcore_main() on every slave lcore */
	i = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_main, (void*)i++, lcore_id);
	}
	/* call on master too */
	(void) lcore_main((void*)i);

	return 0;
}
