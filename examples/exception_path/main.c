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
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <netinet/in.h>
#include <net/if.h>
#ifdef RTE_EXEC_ENV_LINUXAPP
#include <linux/if_tun.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1
#define FATAL_ERROR(fmt, args...)       rte_exit(EXIT_FAILURE, fmt "\n", ##args)
#define PRINT_INFO(fmt, args...)        RTE_LOG(INFO, APP, fmt "\n", ##args)

/* Max ports than can be used (each port is associated with two lcores) */
#define MAX_PORTS               (RTE_MAX_LCORE / 2)

/* Max size of a single packet */
#define MAX_PACKET_SZ (2048)

/* Size of the data buffer in each mbuf */
#define MBUF_DATA_SZ (MAX_PACKET_SZ + RTE_PKTMBUF_HEADROOM)

/* Number of mbufs in mempool that is created */
#define NB_MBUF                 8192

/* How many packets to attempt to read from NIC in one go */
#define PKT_BURST_SZ            32

/* How many objects (mbufs) to keep in per-lcore mempool cache */
#define MEMPOOL_CACHE_SZ        PKT_BURST_SZ

/* Number of RX ring descriptors */
#define NB_RXD                  128

/* Number of TX ring descriptors */
#define NB_TXD                  512

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */

/* Options for configuring ethernet port */
static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.header_split = 0,      /* Header Split disabled */
		.hw_ip_checksum = 0,    /* IP checksum offload disabled */
		.hw_vlan_filter = 0,    /* VLAN filtering disabled */
		.jumbo_frame = 0,       /* Jumbo Frame Support disabled */
		.hw_strip_crc = 1,      /* CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

/* Mempool for mbufs */
static struct rte_mempool * pktmbuf_pool = NULL;

/* Mask of enabled ports */
static uint32_t ports_mask = 0;

/* Mask of cores that read from NIC and write to tap */
static uint64_t input_cores_mask = 0;

/* Mask of cores that read from tap and write to NIC */
static uint64_t output_cores_mask = 0;

/* Array storing port_id that is associated with each lcore */
static uint8_t port_ids[RTE_MAX_LCORE];

/* Structure type for recording lcore-specific stats */
struct stats {
	uint64_t rx;
	uint64_t tx;
	uint64_t dropped;
};

/* Array of lcore-specific stats */
static struct stats lcore_stats[RTE_MAX_LCORE];

/* Print out statistics on packets handled */
static void
print_stats(void)
{
	unsigned i;

	printf("\n**Exception-Path example application statistics**\n"
	       "=======  ======  ============  ============  ===============\n"
	       " Lcore    Port            RX            TX    Dropped on TX\n"
	       "-------  ------  ------------  ------------  ---------------\n");
	RTE_LCORE_FOREACH(i) {
		printf("%6u %7u %13"PRIu64" %13"PRIu64" %16"PRIu64"\n",
		       i, (unsigned)port_ids[i],
		       lcore_stats[i].rx, lcore_stats[i].tx,
		       lcore_stats[i].dropped);
	}
	printf("=======  ======  ============  ============  ===============\n");
}

/* Custom handling of signals to handle stats */
static void
signal_handler(int signum)
{
	/* When we receive a USR1 signal, print stats */
	if (signum == SIGUSR1) {
		print_stats();
	}

	/* When we receive a USR2 signal, reset stats */
	if (signum == SIGUSR2) {
		memset(&lcore_stats, 0, sizeof(lcore_stats));
		printf("\n**Statistics have been reset**\n");
		return;
	}
}

#ifdef RTE_EXEC_ENV_LINUXAPP
/*
 * Create a tap network interface, or use existing one with same name.
 * If name[0]='\0' then a name is automatically assigned and returned in name.
 */
static int tap_create(char *name)
{
	struct ifreq ifr;
	int fd, ret;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0)
		return fd;

	memset(&ifr, 0, sizeof(ifr));

	/* TAP device without packet information */
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (name && *name)
		snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);

	ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	if (name)
		snprintf(name, IFNAMSIZ, "%s", ifr.ifr_name);

	return fd;
}
#else
/*
 * Find a free tap network interface, or create a new one.
 * The name is automatically assigned and returned in name.
 */
static int tap_create(char *name)
{
	int i, fd = -1;
	char devname[PATH_MAX];

	for (i = 0; i < 255; i++) {
		snprintf(devname, sizeof(devname), "/dev/tap%d", i);
		fd = open(devname, O_RDWR);
		if (fd >= 0 || errno != EBUSY)
			break;
	}

	if (name)
		snprintf(name, IFNAMSIZ, "tap%d", i);

	return fd;
}
#endif

/* Main processing loop */
static int
main_loop(__attribute__((unused)) void *arg)
{
	const unsigned lcore_id = rte_lcore_id();
	char tap_name[IFNAMSIZ];
	int tap_fd;

	if ((1ULL << lcore_id) & input_cores_mask) {
		/* Create new tap interface */
		snprintf(tap_name, IFNAMSIZ, "tap_dpdk_%.2u", lcore_id);
		tap_fd = tap_create(tap_name);
		if (tap_fd < 0)
			FATAL_ERROR("Could not create tap interface \"%s\" (%d)",
					tap_name, tap_fd);

		PRINT_INFO("Lcore %u is reading from port %u and writing to %s",
		           lcore_id, (unsigned)port_ids[lcore_id], tap_name);
		fflush(stdout);
		/* Loop forever reading from NIC and writing to tap */
		for (;;) {
			struct rte_mbuf *pkts_burst[PKT_BURST_SZ];
			unsigned i;
			const unsigned nb_rx =
					rte_eth_rx_burst(port_ids[lcore_id], 0,
					    pkts_burst, PKT_BURST_SZ);
			lcore_stats[lcore_id].rx += nb_rx;
			for (i = 0; likely(i < nb_rx); i++) {
				struct rte_mbuf *m = pkts_burst[i];
				/* Ignore return val from write() */
				int ret = write(tap_fd,
				                rte_pktmbuf_mtod(m, void*),
				                rte_pktmbuf_data_len(m));
				rte_pktmbuf_free(m);
				if (unlikely(ret < 0))
					lcore_stats[lcore_id].dropped++;
				else
					lcore_stats[lcore_id].tx++;
			}
		}
	}
	else if ((1ULL << lcore_id) & output_cores_mask) {
		/* Create new tap interface */
		snprintf(tap_name, IFNAMSIZ, "tap_dpdk_%.2u", lcore_id);
		tap_fd = tap_create(tap_name);
		if (tap_fd < 0)
			FATAL_ERROR("Could not create tap interface \"%s\" (%d)",
					tap_name, tap_fd);

		PRINT_INFO("Lcore %u is reading from %s and writing to port %u",
		           lcore_id, tap_name, (unsigned)port_ids[lcore_id]);
		fflush(stdout);
		/* Loop forever reading from tap and writing to NIC */
		for (;;) {
			int ret;
			struct rte_mbuf *m = rte_pktmbuf_alloc(pktmbuf_pool);
			if (m == NULL)
				continue;

			ret = read(tap_fd, rte_pktmbuf_mtod(m, void *),
				MAX_PACKET_SZ);
			lcore_stats[lcore_id].rx++;
			if (unlikely(ret < 0)) {
				FATAL_ERROR("Reading from %s interface failed",
				            tap_name);
			}
			m->nb_segs = 1;
			m->next = NULL;
			m->pkt_len = (uint16_t)ret;
			m->data_len = (uint16_t)ret;
			ret = rte_eth_tx_burst(port_ids[lcore_id], 0, &m, 1);
			if (unlikely(ret < 1)) {
				rte_pktmbuf_free(m);
				lcore_stats[lcore_id].dropped++;
			}
			else {
				lcore_stats[lcore_id].tx++;
			}
		}
	}
	else {
		PRINT_INFO("Lcore %u has nothing to do", lcore_id);
		return 0;
	}
	/*
	 * Tap file is closed automatically when program exits. Putting close()
	 * here will cause the compiler to give an error about unreachable code.
	 */
}

/* Display usage instructions */
static void
print_usage(const char *prgname)
{
	PRINT_INFO("\nUsage: %s [EAL options] -- -p PORTMASK -i IN_CORES -o OUT_CORES\n"
	           "    -p PORTMASK: hex bitmask of ports to use\n"
	           "    -i IN_CORES: hex bitmask of cores which read from NIC\n"
	           "    -o OUT_CORES: hex bitmask of cores which write to NIC",
	           prgname);
}

/* Convert string to unsigned number. 0 is returned if error occurs */
static uint64_t
parse_unsigned(const char *portmask)
{
	char *end = NULL;
	uint64_t num;

	num = strtoull(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return (uint64_t)num;
}

/* Record affinities between ports and lcores in global port_ids[] array */
static void
setup_port_lcore_affinities(void)
{
	unsigned long i;
	uint8_t tx_port = 0;
	uint8_t rx_port = 0;

	/* Setup port_ids[] array, and check masks were ok */
	RTE_LCORE_FOREACH(i) {
		if (input_cores_mask & (1ULL << i)) {
			/* Skip ports that are not enabled */
			while ((ports_mask & (1 << rx_port)) == 0) {
				rx_port++;
				if (rx_port > (sizeof(ports_mask) * 8))
					goto fail; /* not enough ports */
			}

			port_ids[i] = rx_port++;
		} else if (output_cores_mask & (1ULL << (i & 0x3f))) {
			/* Skip ports that are not enabled */
			while ((ports_mask & (1 << tx_port)) == 0) {
				tx_port++;
				if (tx_port > (sizeof(ports_mask) * 8))
					goto fail; /* not enough ports */
			}

			port_ids[i] = tx_port++;
		}
	}

	if (rx_port != tx_port)
		goto fail; /* uneven number of cores in masks */

	if (ports_mask & (~((1 << rx_port) - 1)))
		goto fail; /* unused ports */

	return;
fail:
	FATAL_ERROR("Invalid core/port masks specified on command line");
}

/* Parse the arguments given in the command line of the application */
static void
parse_args(int argc, char **argv)
{
	int opt;
	const char *prgname = argv[0];

	/* Disable printing messages within getopt() */
	opterr = 0;

	/* Parse command line */
	while ((opt = getopt(argc, argv, "i:o:p:")) != EOF) {
		switch (opt) {
		case 'i':
			input_cores_mask = parse_unsigned(optarg);
			break;
		case 'o':
			output_cores_mask = parse_unsigned(optarg);
			break;
		case 'p':
			ports_mask = parse_unsigned(optarg);
			break;
		default:
			print_usage(prgname);
			FATAL_ERROR("Invalid option specified");
		}
	}

	/* Check that options were parsed ok */
	if (input_cores_mask == 0) {
		print_usage(prgname);
		FATAL_ERROR("IN_CORES not specified correctly");
	}
	if (output_cores_mask == 0) {
		print_usage(prgname);
		FATAL_ERROR("OUT_CORES not specified correctly");
	}
	if (ports_mask == 0) {
		print_usage(prgname);
		FATAL_ERROR("PORTMASK not specified correctly");
	}

	setup_port_lcore_affinities();
}

/* Initialise a single port on an Ethernet device */
static void
init_port(uint8_t port)
{
	int ret;
	uint16_t nb_rxd = NB_RXD;
	uint16_t nb_txd = NB_TXD;

	/* Initialise device and RX/TX queues */
	PRINT_INFO("Initialising port %u ...", (unsigned)port);
	fflush(stdout);
	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret < 0)
		FATAL_ERROR("Could not configure port%u (%d)",
		            (unsigned)port, ret);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (ret < 0)
		FATAL_ERROR("Could not adjust number of descriptors for port%u (%d)",
			    (unsigned)port, ret);

	ret = rte_eth_rx_queue_setup(port, 0, nb_rxd,
				rte_eth_dev_socket_id(port),
				NULL,
				pktmbuf_pool);
	if (ret < 0)
		FATAL_ERROR("Could not setup up RX queue for port%u (%d)",
		            (unsigned)port, ret);

	ret = rte_eth_tx_queue_setup(port, 0, nb_txd,
				rte_eth_dev_socket_id(port),
				NULL);
	if (ret < 0)
		FATAL_ERROR("Could not setup up TX queue for port%u (%d)",
		            (unsigned)port, ret);

	ret = rte_eth_dev_start(port);
	if (ret < 0)
		FATAL_ERROR("Could not start port%u (%d)", (unsigned)port, ret);

	rte_eth_promiscuous_enable(port);
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

/* Initialise ports/queues etc. and start main loop on each core */
int
main(int argc, char** argv)
{
	int ret;
	unsigned i,high_port;
	uint8_t nb_sys_ports, port;

	/* Associate signal_hanlder function with USR signals */
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	/* Initialise EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		FATAL_ERROR("Could not initialise EAL (%d)", ret);
	argc -= ret;
	argv += ret;

	/* Parse application arguments (after the EAL ones) */
	parse_args(argc, argv);

	/* Create the mbuf pool */
	pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
			MEMPOOL_CACHE_SZ, 0, MBUF_DATA_SZ, rte_socket_id());
	if (pktmbuf_pool == NULL) {
		FATAL_ERROR("Could not initialise mbuf pool");
		return -1;
	}

	/* Get number of ports found in scan */
	nb_sys_ports = rte_eth_dev_count();
	if (nb_sys_ports == 0)
		FATAL_ERROR("No supported Ethernet device found");
	/* Find highest port set in portmask */
	for (high_port = (sizeof(ports_mask) * 8) - 1;
			(high_port != 0) && !(ports_mask & (1 << high_port));
			high_port--)
		; /* empty body */
	if (high_port > nb_sys_ports)
		FATAL_ERROR("Port mask requires more ports than available");

	/* Initialise each port */
	for (port = 0; port < nb_sys_ports; port++) {
		/* Skip ports that are not enabled */
		if ((ports_mask & (1 << port)) == 0) {
			continue;
		}
		init_port(port);
	}
	check_all_ports_link_status(nb_sys_ports, ports_mask);

	/* Launch per-lcore function on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(i) {
		if (rte_eal_wait_lcore(i) < 0)
			return -1;
	}

	return 0;
}
