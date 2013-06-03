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
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

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
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_kni.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

/* Max size of a single packet */
#define MAX_PACKET_SZ           2048

/* Number of bytes needed for each mbuf */
#define MBUF_SZ \
	(MAX_PACKET_SZ + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

/* Number of mbufs in mempool that is created */
#define NB_MBUF                 (8192 * 16)

/* How many packets to attempt to read from NIC in one go */
#define PKT_BURST_SZ            32

/* How many objects (mbufs) to keep in per-lcore mempool cache */
#define MEMPOOL_CACHE_SZ        PKT_BURST_SZ

/* Number of RX ring descriptors */
#define NB_RXD                  128

/* Number of TX ring descriptors */
#define NB_TXD                  512

/* Total octets in ethernet header */
#define KNI_ENET_HEADER_SIZE    14

/* Total octets in the FCS */
#define KNI_ENET_FCS_SIZE       4

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
/* RX ring configuration */
static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 8,   /* Ring prefetch threshold */
		.hthresh = 8,   /* Ring host threshold */
		.wthresh = 4,   /* Ring writeback threshold */
	},
	.rx_free_thresh = 0,    /* Immediately free RX descriptors */
};

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
/* TX ring configuration */
static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = 36,  /* Ring prefetch threshold */
		.hthresh = 0,   /* Ring host threshold */
		.wthresh = 0,   /* Ring writeback threshold */
	},
	.tx_free_thresh = 0,    /* Use PMD default values */
	.tx_rs_thresh = 0,      /* Use PMD default values */
};

/* Options for configuring ethernet port */
static struct rte_eth_conf port_conf = {
	.rxmode = {
		.header_split = 0,      /* Header Split disabled */
		.hw_ip_checksum = 0,    /* IP checksum offload disabled */
		.hw_vlan_filter = 0,    /* VLAN filtering disabled */
		.jumbo_frame = 0,       /* Jumbo Frame Support disabled */
		.hw_strip_crc = 0,      /* CRC stripped by hardware */
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
static uint32_t input_cores_mask = 0;

/* Mask of cores that read from tap and write to NIC */
static uint32_t output_cores_mask = 0;

/* Structure type for recording kni interface specific stats */
struct kni_interface_stats {
	/* number of pkts received from NIC, and sent to KNI */
	uint64_t rx_packets;

	/* number of pkts received from NIC, but failed to send to KNI */
	uint64_t rx_dropped;

	/* number of pkts received from KNI, and sent to NIC */
	uint64_t tx_packets;

	/* number of pkts received from KNI, but failed to send to NIC */
	uint64_t tx_dropped;
};

/* Structure type for recording port specific information */
struct kni_port_info_t {
	/* lcore id for ingress */
	unsigned lcore_id_ingress;

	/* lcore id for egress */
	unsigned lcore_id_egress;

	/* pointer to kni interface */
	struct rte_kni *kni;
};

/* kni port specific information array*/
static struct kni_port_info_t kni_port_info[RTE_MAX_ETHPORTS];

/* kni device statistics array */
static struct kni_interface_stats kni_stats[RTE_MAX_ETHPORTS];

/* Get the pointer to kni interface */
static struct rte_kni * kni_lcore_to_kni(unsigned lcore_id);

static int kni_change_mtu(uint8_t port_id, unsigned new_mtu);
static int kni_config_network_interface(uint8_t port_id, uint8_t if_up);

static struct rte_kni_ops kni_ops = {
	.change_mtu = kni_change_mtu,
	.config_network_if = kni_config_network_interface,
};

/* Print out statistics on packets handled */
static void
print_stats(void)
{
	uint8_t i;

	printf("\n**KNI example application statistics**\n"
	       "======  ==============  ============  ============  ============  ============\n"
	       " Port    Lcore(RX/TX)    rx_packets    rx_dropped    tx_packets    tx_dropped\n"
	       "------  --------------  ------------  ------------  ------------  ------------\n");
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (kni_port_info[i].kni == NULL)
			continue;

		printf("%7d %10u/%2u %13"PRIu64" %13"PRIu64" %13"PRIu64" "
							"%13"PRIu64"\n", i,
					kni_port_info[i].lcore_id_ingress,
					kni_port_info[i].lcore_id_egress,
						kni_stats[i].rx_packets,
						kni_stats[i].rx_dropped,
						kni_stats[i].tx_packets,
						kni_stats[i].tx_dropped);
	}
	printf("======  ==============  ============  ============  ============  ============\n");
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
		memset(&kni_stats, 0, sizeof(kni_stats));
		printf("\n**Statistics have been reset**\n");
		return;
	}
}

static void
kni_burst_free_mbufs(struct rte_mbuf **pkts, unsigned num)
{
	unsigned i;

	if (pkts == NULL)
		return;

	for (i = 0; i < num; i++) {
		rte_pktmbuf_free(pkts[i]);
		pkts[i] = NULL;
	}
}

/**
 * Interface to burst rx and enqueue mbufs into rx_q
 */
static void
kni_ingress(struct rte_kni *kni)
{
	uint8_t port_id = rte_kni_get_port_id(kni);
	unsigned nb_rx, num;
	struct rte_mbuf *pkts_burst[PKT_BURST_SZ];

	if (kni == NULL || port_id >= RTE_MAX_ETHPORTS)
		return;

	/* Burst rx from eth */
	nb_rx = rte_eth_rx_burst(port_id, 0, pkts_burst, PKT_BURST_SZ);
	if (nb_rx > PKT_BURST_SZ) {
		RTE_LOG(ERR, APP, "Error receiving from eth\n");
		return;
	}

	/* Burst tx to kni */
	num = rte_kni_tx_burst(kni, pkts_burst, nb_rx);
	kni_stats[port_id].rx_packets += num;

	if (unlikely(num < nb_rx)) {
		/* Free mbufs not tx to kni interface */
		kni_burst_free_mbufs(&pkts_burst[num], nb_rx - num);
		kni_stats[port_id].rx_dropped += nb_rx - num;
	}
}

/**
 * Interface to dequeue mbufs from tx_q and burst tx
 */
static void
kni_egress(struct rte_kni *kni)
{
	uint8_t port_id = rte_kni_get_port_id(kni);;
	unsigned nb_tx, num;
	struct rte_mbuf *pkts_burst[PKT_BURST_SZ];

	if (kni == NULL || port_id >= RTE_MAX_ETHPORTS)
		return;

	/* Burst rx from kni */
	num = rte_kni_rx_burst(kni, pkts_burst, PKT_BURST_SZ);
	if (num > PKT_BURST_SZ) {
		RTE_LOG(ERR, APP, "Error receiving from KNI\n");
		return;
	}

	/* Burst tx to eth */
	nb_tx = rte_eth_tx_burst(port_id, 0, pkts_burst, (uint16_t)num);
	kni_stats[port_id].tx_packets += nb_tx;

	if (unlikely(nb_tx < num)) {
		/* Free mbufs not tx to NIC */
		kni_burst_free_mbufs(&pkts_burst[nb_tx], num - nb_tx);
		kni_stats[port_id].tx_dropped += num - nb_tx;
	}
}

/* Main processing loop */
static __attribute__((noreturn)) int
main_loop(__rte_unused void *arg)
{
	uint8_t pid;
	const unsigned lcore_id = rte_lcore_id();
	struct rte_kni *kni = kni_lcore_to_kni(lcore_id);

	if (kni == NULL) {
		RTE_LOG(INFO, APP, "Lcore %u has nothing to do\n", lcore_id);
		for (;;)
			; /* loop doing nothing */
	} else {
		pid = rte_kni_get_port_id(kni);
		if (pid >= RTE_MAX_ETHPORTS)
			rte_exit(EXIT_FAILURE, "Failure: port id >= %d\n",
							RTE_MAX_ETHPORTS);

		if (kni_port_info[pid].lcore_id_ingress == lcore_id) {
			/* Running on lcores for input packets */
			RTE_LOG(INFO, APP, "Lcore %u is reading from "
						"port %d\n", lcore_id, pid);
			fflush(stdout);

			/* rx loop */
			while (1)
				kni_ingress(kni);
		} else if (kni_port_info[pid].lcore_id_egress == lcore_id) {
			/* Running on lcores for output packets */
			RTE_LOG(INFO, APP, "Lcore %u is writing to port %d\n",
							lcore_id, pid);
			fflush(stdout);

			/* tx loop */
			while (1)
				kni_egress(kni);
		} else {
			RTE_LOG(INFO, APP, "Lcore %u has nothing to do\n",
								lcore_id);
			for (;;)
				; /* loop doing nothing */
		}
	}
}

/* Display usage instructions */
static void
print_usage(const char *prgname)
{
	RTE_LOG(INFO, APP, "\nUsage: %s [EAL options] -- -p PORTMASK "
					"-i IN_CORES -o OUT_CORES\n"
	           "    -p PORTMASK: hex bitmask of ports to use\n"
	           "    -i IN_CORES: hex bitmask of cores which read "
		   "from NIC\n"
	           "    -o OUT_CORES: hex bitmask of cores which write to NIC\n",
	           prgname);
}

/* Convert string to unsigned number. 0 is returned if error occurs */
static uint32_t
parse_unsigned(const char *portmask)
{
	char *end = NULL;
	unsigned long num;

	num = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return (uint32_t)num;
}

static int
kni_setup_port_affinities(uint8_t nb_port)
{
	unsigned i;
	uint32_t in_lcore, out_lcore;
	uint8_t rx_port = 0, tx_port = 0;
	uint8_t pid;

	if (nb_port > RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, APP, "The number of ports exceeds the maximum "
					"number of 0x%x\n", RTE_MAX_ETHPORTS);
		return -1;
	}

	RTE_LCORE_FOREACH(i) {
		in_lcore = input_cores_mask & (1 << i);
		out_lcore = output_cores_mask & (1 << i);

		/* Check if it is in input lcore or output lcore mask */
		if (in_lcore == 0 && out_lcore == 0)
			continue;

		/* Check if it is in both input lcore and output lcore mask */
		if (in_lcore != 0 && out_lcore != 0) {
			RTE_LOG(ERR, APP, "Lcore 0x%x can not be used in both "
				"input lcore and output lcore mask\n", i);
			return -1;
		}

		/* Check if the lcore is enabled or not */
		if (rte_lcore_is_enabled(i) == 0) {
			RTE_LOG(ERR, APP, "Lcore 0x%x is not enabled\n", i);
			return -1;
		}

		if (in_lcore != 0) {
			/* It is be for packet receiving */
			while ((rx_port < nb_port) &&
					((ports_mask & (1 << rx_port)) == 0))
				rx_port++;

			if (rx_port >= nb_port) {
				RTE_LOG(ERR, APP, "There is no enough ports "
						"for ingress lcores\n");
				return -1;
			}
			kni_port_info[rx_port].lcore_id_ingress = i;
			rx_port++;
		} else {
			/* It is for packet transmitting */
			while ((tx_port < nb_port) &&
					((ports_mask & (1 << tx_port)) == 0))
				tx_port++;

			if (tx_port >= nb_port) {
				RTE_LOG(ERR, APP, "There is no enough ports "
						"for engree lcores\n");
				return -1;
			}
			kni_port_info[tx_port].lcore_id_egress = i;
			tx_port++;
		}
	}

	/* Display all the port/lcore affinity */
	for (pid = 0; pid < nb_port; pid++) {
		RTE_LOG(INFO, APP, "Port%d, ingress lcore id: %u, "
						"egress lcore id: %u\n", pid,
				kni_port_info[pid].lcore_id_ingress,
				kni_port_info[pid].lcore_id_egress);
	}

	return 0;
}

static struct rte_kni *
kni_lcore_to_kni(unsigned lcore_id)
{
	uint8_t pid;
	struct kni_port_info_t *p = kni_port_info;

	for (pid = 0; pid < RTE_MAX_ETHPORTS; pid++) {
		if (p[pid].kni != NULL && (p[pid].lcore_id_ingress == lcore_id
					|| p[pid].lcore_id_egress == lcore_id))
			return p[pid].kni;
	}

	return NULL;
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
			rte_exit(EXIT_FAILURE, "Invalid option specified");
		}
	}

	/* Check that options were parsed ok */
	if (input_cores_mask == 0) {
		print_usage(prgname);
		rte_exit(EXIT_FAILURE, "IN_CORES not specified correctly");
	}
	if (output_cores_mask == 0) {
		print_usage(prgname);
		rte_exit(EXIT_FAILURE, "OUT_CORES not specified correctly");
	}
	if (ports_mask == 0) {
		print_usage(prgname);
		rte_exit(EXIT_FAILURE, "PORTMASK not specified correctly");
	}
}

/* Initialise a single port on an Ethernet device */
static void
init_port(uint8_t port)
{
	int ret;

	/* Initialise device and RX/TX queues */
	RTE_LOG(INFO, APP, "Initialising port %u ...\n", (unsigned)port);
	fflush(stdout);
	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not configure port%u (%d)",
		            (unsigned)port, ret);

	ret = rte_eth_rx_queue_setup(port, 0, NB_RXD, rte_eth_dev_socket_id(port),
                                 &rx_conf, pktmbuf_pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not setup up RX queue for "
					"port%u (%d)", (unsigned)port, ret);

	ret = rte_eth_tx_queue_setup(port, 0, NB_TXD, rte_eth_dev_socket_id(port),
                                 &tx_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not setup up TX queue for "
					"port%u (%d)", (unsigned)port, ret);

	ret = rte_eth_dev_start(port);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not start port%u (%d)",
						(unsigned)port, ret);

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
			if (link.link_status == 0) {
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

/* Callback for request of changing MTU */
static int
kni_change_mtu(uint8_t port_id, unsigned new_mtu)
{
	int ret;
	struct rte_eth_conf conf;

	if (port_id >= rte_eth_dev_count()) {
		RTE_LOG(ERR, APP, "Invalid port id %d\n", port_id);
		return -EINVAL;
	}

	RTE_LOG(INFO, APP, "Change MTU of port %d to %u\n", port_id, new_mtu);

	/* Stop specific port */
	rte_eth_dev_stop(port_id);

	memcpy(&conf, &port_conf, sizeof(conf));
	/* Set new MTU */
	if (new_mtu > ETHER_MAX_LEN)
		conf.rxmode.jumbo_frame = 1;
	else 
		conf.rxmode.jumbo_frame = 0;

	/* mtu + length of header + length of FCS = max pkt length */
	conf.rxmode.max_rx_pkt_len = new_mtu + KNI_ENET_HEADER_SIZE +
							KNI_ENET_FCS_SIZE;
	ret = rte_eth_dev_configure(port_id, 1, 1, &conf);
	if (ret < 0) {
		RTE_LOG(ERR, APP, "Fail to reconfigure port %d\n", port_id);
		return ret;
	}

	/* Restart specific port */
	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		RTE_LOG(ERR, APP, "Fail to restart port %d\n", port_id);
		return ret;
	}

	return 0;
}

/* Callback for request of configuring network interface up/down */
static int
kni_config_network_interface(uint8_t port_id, uint8_t if_up)
{
	int ret = 0;

	if (port_id >= rte_eth_dev_count() || port_id >= RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, APP, "Invalid port id %d\n", port_id);
		return -EINVAL;
	}

	RTE_LOG(INFO, APP, "Configure network interface of %d %s\n",
					port_id, if_up ? "up" : "down");

	if (if_up != 0) { /* Configure network interface up */
		rte_eth_dev_stop(port_id);
		ret = rte_eth_dev_start(port_id);
	} else /* Configure network interface down */
		rte_eth_dev_stop(port_id);

	if (ret < 0)
		RTE_LOG(ERR, APP, "Failed to start port %d\n", port_id);

	return ret;
}

/* Initialise ports/queues etc. and start main loop on each core */
int
main(int argc, char** argv)
{
	int ret;
	unsigned i, cfg_ports = 0;
	uint8_t nb_sys_ports, port;

	/* Associate signal_hanlder function with USR signals */
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	/* Initialise EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not initialise EAL (%d)", ret);
	argc -= ret;
	argv += ret;

	/* Parse application arguments (after the EAL ones) */
	parse_args(argc, argv);

	/* Create the mbuf pool */
	pktmbuf_pool = rte_mempool_create("mbuf_pool", NB_MBUF, MBUF_SZ,
			MEMPOOL_CACHE_SZ,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
			rte_socket_id(), 0);
	if (pktmbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Could not initialise mbuf pool");
		return -1;
	}

	/* Initialise PMD driver(s) */
	ret = rte_pmd_init_all();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not initialise PMD (%d)", ret);

	/* Scan PCI bus for recognised devices */
	ret = rte_eal_pci_probe();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Could not probe PCI (%d)", ret);

	/* Get number of ports found in scan */
	nb_sys_ports = rte_eth_dev_count();
	if (nb_sys_ports == 0)
		rte_exit(EXIT_FAILURE, "No supported Ethernet devices found - "
			"check that CONFIG_RTE_LIBRTE_IGB_PMD=y and/or "
			"CONFIG_RTE_LIBRTE_IXGBE_PMD=y in the config file");
	/* Find the number of configured ports in the port mask */
	for (i = 0; i < sizeof(ports_mask) * 8; i++)
		cfg_ports += !! (ports_mask & (1 << i));

	if (cfg_ports > nb_sys_ports)
		rte_exit(EXIT_FAILURE, "Port mask requires more ports than "
								"available");

	if (kni_setup_port_affinities(nb_sys_ports) < 0)
		rte_exit(EXIT_FAILURE, "Fail to setup port affinities\n");	

	/* Initialise each port */
	for (port = 0; port < nb_sys_ports; port++) {
		struct rte_kni *kni;

		/* Skip ports that are not enabled */
		if ((ports_mask & (1 << port)) == 0) {
			continue;
		}
		init_port(port);

		if (port >= RTE_MAX_ETHPORTS)
			rte_exit(EXIT_FAILURE, "Can not use more than "
				"%d ports for kni\n", RTE_MAX_ETHPORTS);

		kni = rte_kni_create(port, MAX_PACKET_SZ, pktmbuf_pool,
								&kni_ops);
		if (kni == NULL)
			rte_exit(EXIT_FAILURE, "Fail to create kni dev "
						"for port: %d\n", port);
		kni_port_info[port].kni = kni;
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

