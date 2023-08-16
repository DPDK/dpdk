/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <inttypes.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_arp.h>
#include <rte_spinlock.h>
#include <rte_devargs.h>
#include <rte_byteorder.h>
#include <rte_cpuflags.h>
#include <rte_eth_bond.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "main.h"

#define RTE_LOGTYPE_DCB RTE_LOGTYPE_USER1

#define NB_MBUF   (1024*8)

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100      /* TX drain every ~100us */
#define BURST_RX_INTERVAL_NS (10) /* RX poll interval ~100ns */

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */
#define RX_FTHRESH (MAX_PKT_BURST * 2)/**< Default values of RX free threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_RX_DESC_DEFAULT 1024
#define RTE_TX_DESC_DEFAULT 1024

#define BOND_IP_1	7
#define BOND_IP_2	0
#define BOND_IP_3	0
#define BOND_IP_4	10

/* not defined under linux */
#ifndef NIPQUAD
#define NIPQUAD_FMT "%u.%u.%u.%u"
#endif

#define MAX_PORTS	4
#define PRINT_MAC(addr)		printf("%02"PRIx8":%02"PRIx8":%02"PRIx8 \
		":%02"PRIx8":%02"PRIx8":%02"PRIx8,	\
		RTE_ETHER_ADDR_BYTES(&addr))

uint16_t members[RTE_MAX_ETHPORTS];
uint16_t members_count;

static uint16_t BOND_PORT = 0xffff;

static struct rte_mempool *mbuf_pool;

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

static void
member_port_init(uint16_t portid, struct rte_mempool *mbuf_pool)
{
	int retval;
	uint16_t nb_rxd = RTE_RX_DESC_DEFAULT;
	uint16_t nb_txd = RTE_TX_DESC_DEFAULT;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_conf local_port_conf = port_conf;

	if (!rte_eth_dev_is_valid_port(portid))
		rte_exit(EXIT_FAILURE, "Invalid port\n");

	retval = rte_eth_dev_info_get(portid, &dev_info);
	if (retval != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			portid, strerror(-retval));

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf.rx_adv_conf.rss_conf.rss_hf) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			portid,
			port_conf.rx_adv_conf.rss_conf.rss_hf,
			local_port_conf.rx_adv_conf.rss_conf.rss_hf);
	}

	retval = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: configuration failed (res=%d)\n",
				portid, retval);

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd, &nb_txd);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: rte_eth_dev_adjust_nb_rx_tx_desc "
				"failed (res=%d)\n", portid, retval);

	/* RX setup */
	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	retval = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					rte_eth_dev_socket_id(portid),
					&rxq_conf,
					mbuf_pool);
	if (retval < 0)
		rte_exit(retval, " port %u: RX queue 0 setup failed (res=%d)",
				portid, retval);

	/* TX setup */
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	retval = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid), &txq_conf);

	if (retval < 0)
		rte_exit(retval, "port %u: TX queue 0 setup failed (res=%d)",
				portid, retval);

	retval  = rte_eth_dev_start(portid);
	if (retval < 0)
		rte_exit(retval,
				"Start port %d failed (res=%d)",
				portid, retval);

	struct rte_ether_addr addr;

	retval = rte_eth_macaddr_get(portid, &addr);
	if (retval != 0)
		rte_exit(retval,
				"Mac address get port %d failed (res=%d)",
				portid, retval);

	printf("Port %u MAC: ", portid);
	PRINT_MAC(addr);
	printf("\n");
}

static void
bond_port_init(struct rte_mempool *mbuf_pool)
{
	int retval;
	uint8_t i;
	uint16_t nb_rxd = RTE_RX_DESC_DEFAULT;
	uint16_t nb_txd = RTE_TX_DESC_DEFAULT;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_conf local_port_conf = port_conf;
	uint16_t wait_counter = 20;

	retval = rte_eth_bond_create("net_bonding0", BONDING_MODE_ALB,
			0 /*SOCKET_ID_ANY*/);
	if (retval < 0)
		rte_exit(EXIT_FAILURE,
				"Failed to create bond port\n");

	BOND_PORT = retval;

	retval = rte_eth_dev_info_get(BOND_PORT, &dev_info);
	if (retval != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			BOND_PORT, strerror(-retval));

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	retval = rte_eth_dev_configure(BOND_PORT, 1, 1, &local_port_conf);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: configuration failed (res=%d)\n",
				BOND_PORT, retval);

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(BOND_PORT, &nb_rxd, &nb_txd);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: rte_eth_dev_adjust_nb_rx_tx_desc "
				"failed (res=%d)\n", BOND_PORT, retval);

	for (i = 0; i < members_count; i++) {
		if (rte_eth_bond_member_add(BOND_PORT, members[i]) == -1)
			rte_exit(-1, "Oooops! adding member (%u) to bond (%u) failed!\n",
					members[i], BOND_PORT);

	}

	/* RX setup */
	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	retval = rte_eth_rx_queue_setup(BOND_PORT, 0, nb_rxd,
					rte_eth_dev_socket_id(BOND_PORT),
					&rxq_conf, mbuf_pool);
	if (retval < 0)
		rte_exit(retval, " port %u: RX queue 0 setup failed (res=%d)",
				BOND_PORT, retval);

	/* TX setup */
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	retval = rte_eth_tx_queue_setup(BOND_PORT, 0, nb_txd,
				rte_eth_dev_socket_id(BOND_PORT), &txq_conf);

	if (retval < 0)
		rte_exit(retval, "port %u: TX queue 0 setup failed (res=%d)",
				BOND_PORT, retval);

	retval  = rte_eth_dev_start(BOND_PORT);
	if (retval < 0)
		rte_exit(retval, "Start port %d failed (res=%d)", BOND_PORT, retval);

	printf("Waiting for members to become active...");
	while (wait_counter) {
		uint16_t act_members[16] = {0};
		if (rte_eth_bond_active_members_get(BOND_PORT, act_members, 16) ==
				members_count) {
			printf("\n");
			break;
		}
		sleep(1);
		printf("...");
		if (--wait_counter == 0)
			rte_exit(-1, "\nFailed to activate members\n");
	}

	retval = rte_eth_promiscuous_enable(BOND_PORT);
	if (retval != 0) {
		rte_exit(EXIT_FAILURE,
				"port %u: promiscuous mode enable failed: %s\n",
				BOND_PORT, rte_strerror(-retval));
		return;
	}

	struct rte_ether_addr addr;

	retval = rte_eth_macaddr_get(BOND_PORT, &addr);
	if (retval != 0)
		rte_exit(retval, "port %u: Mac address get failed (res=%d)",
				BOND_PORT, retval);

	printf("Port %u MAC: ", (unsigned)BOND_PORT);
		PRINT_MAC(addr);
		printf("\n");
}

static inline size_t
get_vlan_offset(struct rte_ether_hdr *eth_hdr, uint16_t *proto)
{
	size_t vlan_offset = 0;

	if (rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) == *proto) {
		struct rte_vlan_hdr *vlan_hdr =
			(struct rte_vlan_hdr *)(eth_hdr + 1);

		vlan_offset = sizeof(struct rte_vlan_hdr);
		*proto = vlan_hdr->eth_proto;

		if (rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) == *proto) {
			vlan_hdr = vlan_hdr + 1;

			*proto = vlan_hdr->eth_proto;
			vlan_offset += sizeof(struct rte_vlan_hdr);
		}
	}
	return vlan_offset;
}

struct global_flag_stru_t {
	int LcoreMainIsRunning;
	int LcoreMainCore;
	uint32_t port_packets[4];
	rte_spinlock_t lock;
};
struct global_flag_stru_t global_flag_stru;
struct global_flag_stru_t *global_flag_stru_p = &global_flag_stru;

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static int lcore_main(__rte_unused void *arg1)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST] __rte_cache_aligned;
	struct rte_ether_addr dst_addr;

	struct rte_ether_addr bond_mac_addr;
	struct rte_ether_hdr *eth_hdr;
	struct rte_arp_hdr *arp_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	uint16_t ether_type, offset;

	uint16_t rx_cnt;
	uint32_t bond_ip;
	int i = 0;
	uint8_t is_free;
	int ret;

	bond_ip = BOND_IP_1 | (BOND_IP_2 << 8) |
				(BOND_IP_3 << 16) | (BOND_IP_4 << 24);

	rte_spinlock_lock(&global_flag_stru_p->lock);

	while (global_flag_stru_p->LcoreMainIsRunning) {
		rte_spinlock_unlock(&global_flag_stru_p->lock);
		rx_cnt = rte_eth_rx_burst(BOND_PORT, 0, pkts, MAX_PKT_BURST);
		is_free = 0;

		/* If didn't receive any packets, wait and go to next iteration */
		if (rx_cnt == 0) {
			rte_delay_us(50);
			continue;
		}

		ret = rte_eth_macaddr_get(BOND_PORT, &bond_mac_addr);
		if (ret != 0) {
			printf("Bond (port %u) MAC address get failed: %s.\n"
			       "%u packets dropped", BOND_PORT, strerror(-ret),
			       rx_cnt);
			rte_pktmbuf_free(pkts[i]);
			continue;
		}

		/* Search incoming data for ARP packets and prepare response */
		for (i = 0; i < rx_cnt; i++) {
			if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1) {
				global_flag_stru_p->port_packets[0]++;
				rte_spinlock_unlock(&global_flag_stru_p->lock);
			}
			eth_hdr = rte_pktmbuf_mtod(pkts[i],
						struct rte_ether_hdr *);
			ether_type = eth_hdr->ether_type;
			if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))
				printf("VLAN tagged frame, offset:");
			offset = get_vlan_offset(eth_hdr, &ether_type);
			if (offset > 0)
				printf("%d\n", offset);
			if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {
				if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1)     {
					global_flag_stru_p->port_packets[1]++;
					rte_spinlock_unlock(&global_flag_stru_p->lock);
				}
				arp_hdr = (struct rte_arp_hdr *)(
					(char *)(eth_hdr + 1) + offset);
				if (arp_hdr->arp_data.arp_tip == bond_ip) {
					if (arp_hdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST)) {
						arp_hdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);
						/* Switch src and dst data and set bonding MAC */
						rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
						rte_ether_addr_copy(&bond_mac_addr, &eth_hdr->src_addr);
						rte_ether_addr_copy(&arp_hdr->arp_data.arp_sha,
								&arp_hdr->arp_data.arp_tha);
						arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
						rte_ether_addr_copy(&bond_mac_addr, &dst_addr);
						rte_ether_addr_copy(&dst_addr, &arp_hdr->arp_data.arp_sha);
						arp_hdr->arp_data.arp_sip = bond_ip;
						rte_eth_tx_burst(BOND_PORT, 0, &pkts[i], 1);
						is_free = 1;
					} else {
						rte_eth_tx_burst(BOND_PORT, 0, NULL, 0);
					}
				}
			} else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
				if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1)     {
					global_flag_stru_p->port_packets[2]++;
					rte_spinlock_unlock(&global_flag_stru_p->lock);
				 }
				ipv4_hdr = (struct rte_ipv4_hdr *)((char *)(eth_hdr + 1) + offset);
				if (ipv4_hdr->dst_addr == bond_ip) {
					rte_ether_addr_copy(&eth_hdr->src_addr,
							&eth_hdr->dst_addr);
					rte_ether_addr_copy(&bond_mac_addr,
							&eth_hdr->src_addr);
					ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
					ipv4_hdr->src_addr = bond_ip;
					rte_eth_tx_burst(BOND_PORT, 0, &pkts[i], 1);
				}

			}

			/* Free processed packets */
			if (is_free == 0)
				rte_pktmbuf_free(pkts[i]);
		}
		rte_spinlock_lock(&global_flag_stru_p->lock);
	}
	rte_spinlock_unlock(&global_flag_stru_p->lock);
	printf("BYE lcore_main\n");
	return 0;
}

struct cmd_obj_send_result {
	cmdline_fixed_string_t action;
	cmdline_ipaddr_t ip;
};
static inline void get_string(struct cmd_obj_send_result *res, char *buf, uint8_t size)
{
	snprintf(buf, size, NIPQUAD_FMT,
		((unsigned)((unsigned char *)&(res->ip.addr.ipv4))[0]),
		((unsigned)((unsigned char *)&(res->ip.addr.ipv4))[1]),
		((unsigned)((unsigned char *)&(res->ip.addr.ipv4))[2]),
		((unsigned)((unsigned char *)&(res->ip.addr.ipv4))[3])
		);
}
static void cmd_obj_send_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
			       __rte_unused void *data)
{

	struct cmd_obj_send_result *res = parsed_result;
	char ip_str[INET6_ADDRSTRLEN];

	struct rte_ether_addr bond_mac_addr;
	struct rte_mbuf *created_pkt;
	struct rte_ether_hdr *eth_hdr;
	struct rte_arp_hdr *arp_hdr;

	uint32_t bond_ip;
	size_t pkt_size;
	int ret;

	if (res->ip.family == AF_INET)
		get_string(res, ip_str, INET_ADDRSTRLEN);
	else
		cmdline_printf(cl, "Wrong IP format. Only IPv4 is supported\n");

	bond_ip = BOND_IP_1 | (BOND_IP_2 << 8) |
				(BOND_IP_3 << 16) | (BOND_IP_4 << 24);

	ret = rte_eth_macaddr_get(BOND_PORT, &bond_mac_addr);
	if (ret != 0) {
		cmdline_printf(cl,
			       "Failed to get bond (port %u) MAC address: %s\n",
			       BOND_PORT, strerror(-ret));
	}

	created_pkt = rte_pktmbuf_alloc(mbuf_pool);
	if (created_pkt == NULL) {
		cmdline_printf(cl, "Failed to allocate mbuf\n");
		return;
	}

	pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr);
	created_pkt->data_len = pkt_size;
	created_pkt->pkt_len = pkt_size;

	eth_hdr = rte_pktmbuf_mtod(created_pkt, struct rte_ether_hdr *);
	rte_ether_addr_copy(&bond_mac_addr, &eth_hdr->src_addr);
	memset(&eth_hdr->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
	eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);

	arp_hdr = (struct rte_arp_hdr *)(
		(char *)eth_hdr + sizeof(struct rte_ether_hdr));
	arp_hdr->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
	arp_hdr->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	arp_hdr->arp_hlen = RTE_ETHER_ADDR_LEN;
	arp_hdr->arp_plen = sizeof(uint32_t);
	arp_hdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REQUEST);

	rte_ether_addr_copy(&bond_mac_addr, &arp_hdr->arp_data.arp_sha);
	arp_hdr->arp_data.arp_sip = bond_ip;
	memset(&arp_hdr->arp_data.arp_tha, 0, RTE_ETHER_ADDR_LEN);
	arp_hdr->arp_data.arp_tip =
			  ((unsigned char *)&res->ip.addr.ipv4)[0]        |
			 (((unsigned char *)&res->ip.addr.ipv4)[1] << 8)  |
			 (((unsigned char *)&res->ip.addr.ipv4)[2] << 16) |
			 (((unsigned char *)&res->ip.addr.ipv4)[3] << 24);
	rte_eth_tx_burst(BOND_PORT, 0, &created_pkt, 1);

	rte_delay_ms(100);
	cmdline_printf(cl, "\n");
}

cmdline_parse_token_string_t cmd_obj_action_send =
	TOKEN_STRING_INITIALIZER(struct cmd_obj_send_result, action, "send");
cmdline_parse_token_ipaddr_t cmd_obj_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_obj_send_result, ip);

cmdline_parse_inst_t cmd_obj_send = {
	.f = cmd_obj_send_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "send client_ip",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_obj_action_send,
		(void *)&cmd_obj_ip,
		NULL,
	},
};

struct cmd_start_result {
	cmdline_fixed_string_t start;
};

static void cmd_start_parsed(__rte_unused void *parsed_result,
			       struct cmdline *cl,
			       __rte_unused void *data)
{
	int worker_core_id = rte_lcore_id();

	rte_spinlock_lock(&global_flag_stru_p->lock);
	if (global_flag_stru_p->LcoreMainIsRunning == 0) {
		if (rte_eal_get_lcore_state(global_flag_stru_p->LcoreMainCore)
		    != WAIT) {
			rte_spinlock_unlock(&global_flag_stru_p->lock);
			return;
		}
		rte_spinlock_unlock(&global_flag_stru_p->lock);
	} else {
		cmdline_printf(cl, "lcore_main already running on core:%d\n",
				global_flag_stru_p->LcoreMainCore);
		rte_spinlock_unlock(&global_flag_stru_p->lock);
		return;
	}

	/* start lcore main on core != main_core - ARP response thread */
	worker_core_id = rte_get_next_lcore(rte_lcore_id(), 1, 0);
	if ((worker_core_id >= RTE_MAX_LCORE) || (worker_core_id == 0))
		return;

	rte_spinlock_lock(&global_flag_stru_p->lock);
	global_flag_stru_p->LcoreMainIsRunning = 1;
	rte_spinlock_unlock(&global_flag_stru_p->lock);
	cmdline_printf(cl,
			"Starting lcore_main on core %d:%d "
			"Our IP:%d.%d.%d.%d\n",
			worker_core_id,
			rte_eal_remote_launch(lcore_main, NULL, worker_core_id),
			BOND_IP_1,
			BOND_IP_2,
			BOND_IP_3,
			BOND_IP_4
		);
}

cmdline_parse_token_string_t cmd_start_start =
	TOKEN_STRING_INITIALIZER(struct cmd_start_result, start, "start");

cmdline_parse_inst_t cmd_start = {
	.f = cmd_start_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "starts listening if not started at startup",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_start_start,
		NULL,
	},
};

struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl,
			"ALB - link bonding mode 6 example\n"
			"send IP	- sends one ARPrequest through bonding for IP.\n"
			"start		- starts listening ARPs.\n"
			"stop		- stops lcore_main.\n"
			"show		- shows some bond info: ex. active members etc.\n"
			"help		- prints help.\n"
			"quit		- terminate all threads and quit.\n"
		       );
}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "show help",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_help_help,
		NULL,
	},
};

struct cmd_stop_result {
	cmdline_fixed_string_t stop;
};

static void cmd_stop_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	rte_spinlock_lock(&global_flag_stru_p->lock);
	if (global_flag_stru_p->LcoreMainIsRunning == 0)	{
		cmdline_printf(cl,
					"lcore_main not running on core:%d\n",
					global_flag_stru_p->LcoreMainCore);
		rte_spinlock_unlock(&global_flag_stru_p->lock);
		return;
	}
	global_flag_stru_p->LcoreMainIsRunning = 0;
	if (rte_eal_wait_lcore(global_flag_stru_p->LcoreMainCore) < 0)
		cmdline_printf(cl,
				"error: lcore_main can not stop on core:%d\n",
				global_flag_stru_p->LcoreMainCore);
	else
		cmdline_printf(cl,
				"lcore_main stopped on core:%d\n",
				global_flag_stru_p->LcoreMainCore);
	rte_spinlock_unlock(&global_flag_stru_p->lock);
}

cmdline_parse_token_string_t cmd_stop_stop =
	TOKEN_STRING_INITIALIZER(struct cmd_stop_result, stop, "stop");

cmdline_parse_inst_t cmd_stop = {
	.f = cmd_stop_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "this command do not handle any arguments",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_stop_stop,
		NULL,
	},
};

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	rte_spinlock_lock(&global_flag_stru_p->lock);
	if (global_flag_stru_p->LcoreMainIsRunning == 0)	{
		cmdline_printf(cl,
					"lcore_main not running on core:%d\n",
					global_flag_stru_p->LcoreMainCore);
		rte_spinlock_unlock(&global_flag_stru_p->lock);
		cmdline_quit(cl);
		return;
	}
	global_flag_stru_p->LcoreMainIsRunning = 0;
	if (rte_eal_wait_lcore(global_flag_stru_p->LcoreMainCore) < 0)
		cmdline_printf(cl,
				"error: lcore_main can not stop on core:%d\n",
				global_flag_stru_p->LcoreMainCore);
	else
		cmdline_printf(cl,
				"lcore_main stopped on core:%d\n",
				global_flag_stru_p->LcoreMainCore);
	rte_spinlock_unlock(&global_flag_stru_p->lock);
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "this command do not handle any arguments",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

struct cmd_show_result {
	cmdline_fixed_string_t show;
};

static void cmd_show_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	uint16_t members[16] = {0};
	uint8_t len = 16;
	struct rte_ether_addr addr;
	uint16_t i;
	int ret;

	for (i = 0; i < members_count; i++) {
		ret = rte_eth_macaddr_get(i, &addr);
		if (ret != 0) {
			cmdline_printf(cl,
				"Failed to get port %u MAC address: %s\n",
				i, strerror(-ret));
			continue;
		}

		PRINT_MAC(addr);
		printf("\n");
	}

	rte_spinlock_lock(&global_flag_stru_p->lock);
	cmdline_printf(cl,
			"Active_members:%d "
			"packets received:Tot:%d Arp:%d IPv4:%d\n",
			rte_eth_bond_active_members_get(BOND_PORT, members, len),
			global_flag_stru_p->port_packets[0],
			global_flag_stru_p->port_packets[1],
			global_flag_stru_p->port_packets[2]);
	rte_spinlock_unlock(&global_flag_stru_p->lock);
}

cmdline_parse_token_string_t cmd_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_result, show, "show");

cmdline_parse_inst_t cmd_show = {
	.f = cmd_show_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "this command do not handle any arguments",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_show_show,
		NULL,
	},
};

/****** CONTEXT (list of instruction) */

cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_start,
	(cmdline_parse_inst_t *)&cmd_obj_send,
	(cmdline_parse_inst_t *)&cmd_stop,
	(cmdline_parse_inst_t *)&cmd_show,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_help,
	NULL,
};

/* prompt function, called from main on MAIN lcore */
static void prompt(__rte_unused void *arg1)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "bond6>");
	if (cl != NULL) {
		cmdline_interact(cl);
		cmdline_stdin_exit(cl);
	}
}

/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	int ret, worker_core_id;
	uint16_t nb_ports, i;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	rte_devargs_dump(stdout);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "Give at least one port\n");
	else if (nb_ports > MAX_PORTS)
		rte_exit(EXIT_FAILURE, "You can have max 4 ports\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF, 32,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	members_count = nb_ports;
	RTE_ETH_FOREACH_DEV(i) {
		member_port_init(i, mbuf_pool);
		members[i] = i;
	}

	bond_port_init(mbuf_pool);

	rte_spinlock_init(&global_flag_stru_p->lock);

	/* check state of lcores */
	RTE_LCORE_FOREACH_WORKER(worker_core_id) {
		if (rte_eal_get_lcore_state(worker_core_id) != WAIT)
			return -EBUSY;
	}

	/* start lcore main on core != main_core - ARP response thread */
	worker_core_id = rte_get_next_lcore(rte_lcore_id(), 1, 0);
	if ((worker_core_id >= RTE_MAX_LCORE) || (worker_core_id == 0))
		return -EPERM;

	global_flag_stru_p->LcoreMainIsRunning = 1;
	global_flag_stru_p->LcoreMainCore = worker_core_id;
	printf("Starting lcore_main on core %d:%d Our IP:%d.%d.%d.%d\n",
			worker_core_id,
			rte_eal_remote_launch((lcore_function_t *)lcore_main,
					NULL,
					worker_core_id),
			BOND_IP_1,
			BOND_IP_2,
			BOND_IP_3,
			BOND_IP_4
		);

	/* Start prompt for user interact */
	prompt(NULL);

	rte_delay_ms(100);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
