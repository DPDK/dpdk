/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#define MAX_QUEUES 1024
/*
 * 1024 queues require to meet the needs of a large number of vmdq_pools.
 * (RX/TX_queue_nb * RX/TX_ring_descriptors_nb) per port.
 */
#define NUM_MBUFS_PER_PORT (MAX_QUEUES * RTE_MAX(RTE_TEST_RX_DESC_DEFAULT, \
						RTE_TEST_TX_DESC_DEFAULT))
#define MBUF_CACHE_SIZE 64

#define MAX_PKT_BURST 32

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024

#define INVALID_PORT_ID 0xFF

/* mask of enabled ports */
static uint32_t enabled_port_mask;

/* number of pools (if user does not specify any, 8 by default */
static uint32_t num_queues = 8;
static uint32_t num_pools = 8;

/* empty vmdq configuration structure. Filled in programatically */
static const struct rte_eth_conf vmdq_conf_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_ONLY,
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
	},

	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		/*
		 * should be overridden separately in code with
		 * appropriate values
		 */
		.vmdq_rx_conf = {
			.nb_queue_pools = ETH_8_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
};

static unsigned lcore_ids[RTE_MAX_LCORE];
static uint16_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports; /**< The number of ports specified in command line */

/* array used for printing out statistics */
volatile unsigned long rxPackets[MAX_QUEUES] = {0};

const uint16_t vlan_tags[] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,	12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63,
};
const uint16_t num_vlans = RTE_DIM(vlan_tags);
static uint16_t num_pf_queues,  num_vmdq_queues;
static uint16_t vmdq_pool_base, vmdq_queue_base;
/* pool mac addr template, pool mac addr is like: 52 54 00 12 port# pool# */
static struct ether_addr pool_addr_template = {
	.addr_bytes = {0x52, 0x54, 0x00, 0x12, 0x00, 0x00}
};

/* ethernet addresses of ports */
static struct ether_addr vmdq_ports_eth_addr[RTE_MAX_ETHPORTS];

#define MAX_QUEUE_NUM_10G 128
#define MAX_QUEUE_NUM_1G 8
#define MAX_POOL_MAP_NUM_10G 64
#define MAX_POOL_MAP_NUM_1G 32
#define MAX_POOL_NUM_10G 64
#define MAX_POOL_NUM_1G 8
/*
 * Builds up the correct configuration for vmdq based on the vlan tags array
 * given above, and determine the queue number and pool map number according to
 * valid pool number
 */
static inline int
get_eth_conf(struct rte_eth_conf *eth_conf, uint32_t num_pools)
{
	struct rte_eth_vmdq_rx_conf conf;
	unsigned i;

	conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
	conf.nb_pool_maps = num_pools;
	conf.enable_default_pool = 0;
	conf.default_pool = 0; /* set explicit value, even if not used */

	for (i = 0; i < conf.nb_pool_maps; i++) {
		conf.pool_map[i].vlan_id = vlan_tags[i];
		conf.pool_map[i].pools = (1UL << (i % num_pools));
	}

	(void)(rte_memcpy(eth_conf, &vmdq_conf_default, sizeof(*eth_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &conf,
		   sizeof(eth_conf->rx_adv_conf.vmdq_rx_conf)));
	return 0;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_txconf *txconf;
	struct rte_eth_conf port_conf;
	uint16_t rxRings, txRings;
	uint16_t rxRingSize = RTE_TEST_RX_DESC_DEFAULT;
	uint16_t txRingSize = RTE_TEST_TX_DESC_DEFAULT;
	int retval;
	uint16_t q;
	uint16_t queues_per_pool;
	uint32_t max_nb_pools;

	/*
	 * The max pool number from dev_info will be used to validate the pool
	 * number specified in cmd line
	 */
	rte_eth_dev_info_get(port, &dev_info);
	max_nb_pools = (uint32_t)dev_info.max_vmdq_pools;
	/*
	 * We allow to process part of VMDQ pools specified by num_pools in
	 * command line.
	 */
	if (num_pools > max_nb_pools) {
		printf("num_pools %d >max_nb_pools %d\n",
			num_pools, max_nb_pools);
		return -1;
	}
	retval = get_eth_conf(&port_conf, max_nb_pools);
	if (retval < 0)
		return retval;

	/*
	 * NIC queues are divided into pf queues and vmdq queues.
	 */
	/* There is assumption here all ports have the same configuration! */
	num_pf_queues = dev_info.max_rx_queues - dev_info.vmdq_queue_num;
	queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools;
	num_vmdq_queues = num_pools * queues_per_pool;
	num_queues = num_pf_queues + num_vmdq_queues;
	vmdq_queue_base = dev_info.vmdq_queue_base;
	vmdq_pool_base  = dev_info.vmdq_pool_base;

	printf("pf queue num: %u, configured vmdq pool num: %u,"
		" each vmdq pool has %u queues\n",
		num_pf_queues, num_pools, queues_per_pool);
	printf("vmdq queue base: %d pool base %d\n",
		vmdq_queue_base, vmdq_pool_base);
	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	/*
	 * Though in this example, we only receive packets from the first queue
	 * of each pool and send packets through first rte_lcore_count() tx
	 * queues of vmdq queues, all queues including pf queues are setup.
	 * This is because VMDQ queues doesn't always start from zero, and the
	 * PMD layer doesn't support selectively initialising part of rx/tx
	 * queues.
	 */
	rxRings = (uint16_t)dev_info.max_rx_queues;
	txRings = (uint16_t)dev_info.max_tx_queues;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	retval = rte_eth_dev_configure(port, rxRings, txRings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &rxRingSize,
				&txRingSize);
	if (retval != 0)
		return retval;
	if (RTE_MAX(rxRingSize, txRingSize) > RTE_MAX(RTE_TEST_RX_DESC_DEFAULT,
			RTE_TEST_TX_DESC_DEFAULT)) {
		printf("Mbuf pool has an insufficient size for port %u.\n",
			port);
		return -1;
	}

	rxconf = &dev_info.default_rxconf;
	rxconf->rx_drop_en = 1;
	txconf = &dev_info.default_txconf;
	txconf->txq_flags = ETH_TXQ_FLAGS_IGNORE;
	txconf->offloads = port_conf.txmode.offloads;
	for (q = 0; q < rxRings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rxRingSize,
					rte_eth_dev_socket_id(port),
					rxconf,
					mbuf_pool);
		if (retval < 0) {
			printf("initialise rx queue %d failed\n", q);
			return retval;
		}
	}

	for (q = 0; q < txRings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, txRingSize,
					rte_eth_dev_socket_id(port),
					txconf);
		if (retval < 0) {
			printf("initialise tx queue %d failed\n", q);
			return retval;
		}
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0) {
		printf("port %d start failed\n", port);
		return retval;
	}

	rte_eth_macaddr_get(port, &vmdq_ports_eth_addr[port]);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			vmdq_ports_eth_addr[port].addr_bytes[0],
			vmdq_ports_eth_addr[port].addr_bytes[1],
			vmdq_ports_eth_addr[port].addr_bytes[2],
			vmdq_ports_eth_addr[port].addr_bytes[3],
			vmdq_ports_eth_addr[port].addr_bytes[4],
			vmdq_ports_eth_addr[port].addr_bytes[5]);

	/*
	 * Set mac for each pool.
	 * There is no default mac for the pools in i40.
	 * Removes this after i40e fixes this issue.
	 */
	for (q = 0; q < num_pools; q++) {
		struct ether_addr mac;
		mac = pool_addr_template;
		mac.addr_bytes[4] = port;
		mac.addr_bytes[5] = q;
		printf("Port %u vmdq pool %u set mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			port, q,
			mac.addr_bytes[0], mac.addr_bytes[1],
			mac.addr_bytes[2], mac.addr_bytes[3],
			mac.addr_bytes[4], mac.addr_bytes[5]);
		retval = rte_eth_dev_mac_addr_add(port, &mac,
				q + vmdq_pool_base);
		if (retval) {
			printf("mac addr add failed at pool %d\n", q);
			return retval;
		}
	}

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

	if (num_pools > num_vlans) {
		printf("num_pools %d > num_vlans %d\n", num_pools, num_vlans);
		return -1;
	}

	num_pools = n;

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
	"  --nb-pools NP: number of pools\n",
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
	while ((opt = getopt_long(argc, argv, "p:", long_option,
		&option_index)) != EOF) {
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
			if (vmdq_parse_num_pools(optarg) == -1) {
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

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (enabled_port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;
	}

	if (num_ports < 2 || num_ports % 2) {
		printf("Current enabled port number is %u,"
			"but it should be even and at least 2\n", num_ports);
		return -1;
	}

	return 0;
}

static void
update_mac_address(struct rte_mbuf *m, unsigned dst_port)
{
	struct ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	ether_addr_copy(&vmdq_ports_eth_addr[dst_port], &eth->s_addr);
}

/* When we receive a HUP signal, print out our stats */
static void
sighup_handler(int signum)
{
	unsigned q;
	for (q = 0; q < num_queues; q++) {
		if (q % (num_queues/num_pools) == 0)
			printf("\nPool %u: ", q/(num_queues/num_pools));
		printf("%lu ", rxPackets[q]);
	}
	printf("\nFinished handling signal %d\n", signum);
}

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static int
lcore_main(__attribute__((__unused__)) void *dummy)
{
	const uint16_t lcore_id = (uint16_t)rte_lcore_id();
	const uint16_t num_cores = (uint16_t)rte_lcore_count();
	uint16_t core_id = 0;
	uint16_t startQueue, endQueue;
	uint16_t q, i, p;
	const uint16_t remainder = (uint16_t)(num_vmdq_queues % num_cores);

	for (i = 0; i < num_cores; i++)
		if (lcore_ids[i] == lcore_id) {
			core_id = i;
			break;
		}

	if (remainder != 0) {
		if (core_id < remainder) {
			startQueue = (uint16_t)(core_id *
					(num_vmdq_queues / num_cores + 1));
			endQueue = (uint16_t)(startQueue +
					(num_vmdq_queues / num_cores) + 1);
		} else {
			startQueue = (uint16_t)(core_id *
					(num_vmdq_queues / num_cores) +
					remainder);
			endQueue = (uint16_t)(startQueue +
					(num_vmdq_queues / num_cores));
		}
	} else {
		startQueue = (uint16_t)(core_id *
				(num_vmdq_queues / num_cores));
		endQueue = (uint16_t)(startQueue +
				(num_vmdq_queues / num_cores));
	}

	/* vmdq queue idx doesn't always start from zero.*/
	startQueue += vmdq_queue_base;
	endQueue   += vmdq_queue_base;
	printf("core %u(lcore %u) reading queues %i-%i\n", (unsigned)core_id,
		(unsigned)lcore_id, startQueue, endQueue - 1);

	if (startQueue == endQueue) {
		printf("lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	for (;;) {
		struct rte_mbuf *buf[MAX_PKT_BURST];
		const uint16_t buf_size = sizeof(buf) / sizeof(buf[0]);

		for (p = 0; p < num_ports; p++) {
			const uint8_t sport = ports[p];
			/* 0 <-> 1, 2 <-> 3 etc */
			const uint8_t dport = ports[p ^ 1];
			if ((sport == INVALID_PORT_ID) || (dport == INVALID_PORT_ID))
				continue;

			for (q = startQueue; q < endQueue; q++) {
				const uint16_t rxCount = rte_eth_rx_burst(sport,
					q, buf, buf_size);

				if (unlikely(rxCount == 0))
					continue;

				rxPackets[q] += rxCount;

				for (i = 0; i < rxCount; i++)
					update_mac_address(buf[i], dport);

				const uint16_t txCount = rte_eth_tx_burst(dport,
					vmdq_queue_base + core_id,
					buf,
					rxCount);

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

	for (portid = 0; portid < num_ports; portid++) {
		if (!rte_eth_dev_is_valid_port(ports[portid])) {
			printf("\nSpecified port ID(%u) is not valid\n",
				ports[portid]);
			ports[portid] = INVALID_PORT_ID;
			valid_num_ports--;
		}
	}
	return valid_num_ports;
}

/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned lcore_id, core_id = 0;
	int ret;
	unsigned nb_ports, valid_num_ports;
	uint16_t portid;

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

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		if (rte_lcore_is_enabled(lcore_id))
			lcore_ids[core_id++] = lcore_id;

	if (rte_lcore_count() > RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE, "Not enough cores\n");

	nb_ports = rte_eth_dev_count_avail();

	/*
	 * Update the global var NUM_PORTS and global array PORTS
	 * and get value of var VALID_NUM_PORTS according to system ports number
	 */
	valid_num_ports = check_ports_num(nb_ports);

	if (valid_num_ports < 2 || valid_num_ports % 2) {
		printf("Current valid ports number is %u\n", valid_num_ports);
		rte_exit(EXIT_FAILURE, "Error with valid ports number is not even or less than 2\n");
	}

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
		NUM_MBUFS_PER_PORT * nb_ports, MBUF_CACHE_SIZE,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize network ports\n");
	}

	/* call lcore_main() on every lcore */
	rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
