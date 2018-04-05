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

/* basic constants used in application */
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
static uint16_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports;

/* number of pools (if user does not specify any, 32 by default */
static enum rte_eth_nb_pools num_pools = ETH_32_POOLS;
static enum rte_eth_nb_tcs   num_tcs   = ETH_4_TCS;
static uint16_t num_queues, num_vmdq_queues;
static uint16_t vmdq_pool_base, vmdq_queue_base;
static uint8_t rss_enable;

/* empty vmdq+dcb configuration structure. Filled in programatically */
static const struct rte_eth_conf vmdq_dcb_conf_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_DCB,
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_VMDQ_DCB,
	},
	/*
	 * should be overridden separately in code with
	 * appropriate values
	 */
	.rx_adv_conf = {
		.vmdq_dcb_conf = {
			.nb_queue_pools = ETH_32_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
			.dcb_tc = {0},
		},
		.dcb_rx_conf = {
				.nb_tcs = ETH_4_TCS,
				/** Traffic class each UP mapped to. */
				.dcb_tc = {0},
		},
		.vmdq_rx_conf = {
			.nb_queue_pools = ETH_32_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
	.tx_adv_conf = {
		.vmdq_dcb_tx_conf = {
			.nb_queue_pools = ETH_32_POOLS,
			.dcb_tc = {0},
		},
	},
};

/* array used for printing out statistics */
volatile unsigned long rxPackets[MAX_QUEUES] = {0};

const uint16_t vlan_tags[] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,	12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31
};

const uint16_t num_vlans = RTE_DIM(vlan_tags);
/* pool mac addr template, pool mac addr is like: 52 54 00 12 port# pool# */
static struct ether_addr pool_addr_template = {
	.addr_bytes = {0x52, 0x54, 0x00, 0x12, 0x00, 0x00}
};

/* ethernet addresses of ports */
static struct ether_addr vmdq_ports_eth_addr[RTE_MAX_ETHPORTS];

/* Builds up the correct configuration for vmdq+dcb based on the vlan tags array
 * given above, and the number of traffic classes available for use. */
static inline int
get_eth_conf(struct rte_eth_conf *eth_conf)
{
	struct rte_eth_vmdq_dcb_conf conf;
	struct rte_eth_vmdq_rx_conf  vmdq_conf;
	struct rte_eth_dcb_rx_conf   dcb_conf;
	struct rte_eth_vmdq_dcb_tx_conf tx_conf;
	uint8_t i;

	conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
	vmdq_conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
	tx_conf.nb_queue_pools = (enum rte_eth_nb_pools)num_pools;
	conf.nb_pool_maps = num_pools;
	vmdq_conf.nb_pool_maps = num_pools;
	conf.enable_default_pool = 0;
	vmdq_conf.enable_default_pool = 0;
	conf.default_pool = 0; /* set explicit value, even if not used */
	vmdq_conf.default_pool = 0;

	for (i = 0; i < conf.nb_pool_maps; i++) {
		conf.pool_map[i].vlan_id = vlan_tags[i];
		vmdq_conf.pool_map[i].vlan_id = vlan_tags[i];
		conf.pool_map[i].pools = 1UL << i;
		vmdq_conf.pool_map[i].pools = 1UL << i;
	}
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++){
		conf.dcb_tc[i] = i % num_tcs;
		dcb_conf.dcb_tc[i] = i % num_tcs;
		tx_conf.dcb_tc[i] = i % num_tcs;
	}
	dcb_conf.nb_tcs = (enum rte_eth_nb_tcs)num_tcs;
	(void)(rte_memcpy(eth_conf, &vmdq_dcb_conf_default, sizeof(*eth_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_dcb_conf, &conf,
			  sizeof(conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.dcb_rx_conf, &dcb_conf,
			  sizeof(dcb_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &vmdq_conf,
			  sizeof(vmdq_conf)));
	(void)(rte_memcpy(&eth_conf->tx_adv_conf.vmdq_dcb_tx_conf, &tx_conf,
			  sizeof(tx_conf)));
	if (rss_enable) {
		eth_conf->rxmode.mq_mode = ETH_MQ_RX_VMDQ_DCB_RSS;
		eth_conf->rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP |
							ETH_RSS_UDP |
							ETH_RSS_TCP |
							ETH_RSS_SCTP;
	}
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
	struct rte_eth_conf port_conf = {0};
	uint16_t rxRingSize = RTE_TEST_RX_DESC_DEFAULT;
	uint16_t txRingSize = RTE_TEST_TX_DESC_DEFAULT;
	int retval;
	uint16_t q;
	uint16_t queues_per_pool;
	uint32_t max_nb_pools;
	struct rte_eth_txconf txq_conf;

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

	/*
	 * NIC queues are divided into pf queues and vmdq queues.
	 * There is assumption here all ports have the same configuration!
	*/
	vmdq_queue_base = dev_info.vmdq_queue_base;
	vmdq_pool_base  = dev_info.vmdq_pool_base;
	printf("vmdq queue base: %d pool base %d\n",
		vmdq_queue_base, vmdq_pool_base);
	if (vmdq_pool_base == 0) {
		num_vmdq_queues = dev_info.max_rx_queues;
		num_queues = dev_info.max_rx_queues;
		if (num_tcs != num_vmdq_queues / num_pools) {
			printf("nb_tcs %d is invalid considering with"
				" nb_pools %d, nb_tcs * nb_pools should = %d\n",
				num_tcs, num_pools, num_vmdq_queues);
			return -1;
		}
	} else {
		queues_per_pool = dev_info.vmdq_queue_num /
				  dev_info.max_vmdq_pools;
		if (num_tcs > queues_per_pool) {
			printf("num_tcs %d > num of queues per pool %d\n",
				num_tcs, queues_per_pool);
			return -1;
		}
		num_vmdq_queues = num_pools * queues_per_pool;
		num_queues = vmdq_queue_base + num_vmdq_queues;
		printf("Configured vmdq pool num: %u,"
			" each vmdq pool has %u queues\n",
			num_pools, queues_per_pool);
	}

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = get_eth_conf(&port_conf);
	if (retval < 0)
		return retval;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	/*
	 * Though in this example, all queues including pf queues are setup.
	 * This is because VMDQ queues doesn't always start from zero, and the
	 * PMD layer doesn't support selectively initialising part of rx/tx
	 * queues.
	 */
	retval = rte_eth_dev_configure(port, num_queues, num_queues, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &rxRingSize,
				&txRingSize);
	if (retval != 0)
		return retval;
	if (RTE_MAX(rxRingSize, txRingSize) >
	    RTE_MAX(RTE_TEST_RX_DESC_DEFAULT, RTE_TEST_TX_DESC_DEFAULT)) {
		printf("Mbuf pool has an insufficient size for port %u.\n",
			port);
		return -1;
	}

	for (q = 0; q < num_queues; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rxRingSize,
					rte_eth_dev_socket_id(port),
					NULL,
					mbuf_pool);
		if (retval < 0) {
			printf("initialize rx queue %d failed\n", q);
			return retval;
		}
	}

	txq_conf = dev_info.default_txconf;
	txq_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
	txq_conf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < num_queues; q++) {
		retval = rte_eth_tx_queue_setup(port, q, txRingSize,
					rte_eth_dev_socket_id(port),
					&txq_conf);
		if (retval < 0) {
			printf("initialize tx queue %d failed\n", q);
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

	/* Set mac for each pool.*/
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
	if (n != 16 && n != 32)
		return -1;
	if (n == 16)
		num_pools = ETH_16_POOLS;
	else
		num_pools = ETH_32_POOLS;

	return 0;
}

/* Check num_tcs parameter and set it if OK*/
static int
vmdq_parse_num_tcs(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (n != 4 && n != 8)
		return -1;
	if (n == 4)
		num_tcs = ETH_4_TCS;
	else
		num_tcs = ETH_8_TCS;

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
	"  --nb-pools NP: number of pools (32 default, 16)\n"
	"  --nb-tcs NP: number of TCs (4 default, 8)\n"
	"  --enable-rss: enable RSS (disabled by default)\n",
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
		{"nb-tcs", required_argument, NULL, 0},
		{"enable-rss", 0, NULL, 0},
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
			if (!strcmp(long_option[option_index].name, "nb-pools")) {
				if (vmdq_parse_num_pools(optarg) == -1) {
					printf("invalid number of pools\n");
					return -1;
				}
			}

			if (!strcmp(long_option[option_index].name, "nb-tcs")) {
				if (vmdq_parse_num_tcs(optarg) == -1) {
					printf("invalid number of tcs\n");
					return -1;
				}
			}

			if (!strcmp(long_option[option_index].name, "enable-rss"))
				rss_enable = 1;
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
			" but it should be even and at least 2\n", num_ports);
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
	unsigned q = vmdq_queue_base;

	for (; q < num_queues; q++) {
		if (q % (num_vmdq_queues / num_pools) == 0)
			printf("\nPool %u: ", (q - vmdq_queue_base) /
					      (num_vmdq_queues / num_pools));
		printf("%lu ", rxPackets[q]);
	}
	printf("\nFinished handling signal %d\n", signum);
}

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static int
lcore_main(void *arg)
{
	const uintptr_t core_num = (uintptr_t)arg;
	const unsigned num_cores = rte_lcore_count();
	uint16_t startQueue, endQueue;
	uint16_t q, i, p;
	const uint16_t quot = (uint16_t)(num_vmdq_queues / num_cores);
	const uint16_t remainder = (uint16_t)(num_vmdq_queues % num_cores);


	if (remainder) {
		if (core_num < remainder) {
			startQueue = (uint16_t)(core_num * (quot + 1));
			endQueue = (uint16_t)(startQueue + quot + 1);
		} else {
			startQueue = (uint16_t)(core_num * quot + remainder);
			endQueue = (uint16_t)(startQueue + quot);
		}
	} else {
		startQueue = (uint16_t)(core_num * quot);
		endQueue = (uint16_t)(startQueue + quot);
	}

	/* vmdq queue idx doesn't always start from zero.*/
	startQueue += vmdq_queue_base;
	endQueue   += vmdq_queue_base;
	printf("Core %u(lcore %u) reading queues %i-%i\n", (unsigned)core_num,
	       rte_lcore_id(), startQueue, endQueue - 1);

	if (startQueue == endQueue) {
		printf("lcore %u has nothing to do\n", (unsigned)core_num);
		return 0;
	}

	for (;;) {
		struct rte_mbuf *buf[MAX_PKT_BURST];
		const uint16_t buf_size = sizeof(buf) / sizeof(buf[0]);
		for (p = 0; p < num_ports; p++) {
			const uint8_t src = ports[p];
			const uint8_t dst = ports[p ^ 1]; /* 0 <-> 1, 2 <-> 3 etc */

			if ((src == INVALID_PORT_ID) || (dst == INVALID_PORT_ID))
				continue;

			for (q = startQueue; q < endQueue; q++) {
				const uint16_t rxCount = rte_eth_rx_burst(src,
					q, buf, buf_size);

				if (unlikely(rxCount == 0))
					continue;

				rxPackets[q] += rxCount;

				for (i = 0; i < rxCount; i++)
					update_mac_address(buf[i], dst);

				const uint16_t txCount = rte_eth_tx_burst(dst,
					q, buf, rxCount);
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
	unsigned cores;
	struct rte_mempool *mbuf_pool;
	unsigned lcore_id;
	uintptr_t i;
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

	cores = rte_lcore_count();
	if ((cores & (cores - 1)) != 0 || cores > RTE_MAX_LCORE) {
		rte_exit(EXIT_FAILURE,"This program can only run on an even"
				" number of cores(1-%d)\n\n", RTE_MAX_LCORE);
	}

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

	/* call lcore_main() on every slave lcore */
	i = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_main, (void*)i++, lcore_id);
	}
	/* call on master too */
	(void) lcore_main((void*)i);

	return 0;
}
