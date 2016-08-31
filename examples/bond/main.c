/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_arp.h>
#include <rte_spinlock.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "main.h"

#include <rte_devargs.h>


#include "rte_byteorder.h"
#include "rte_cpuflags.h"
#include "rte_eth_bond.h"

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
#define RTE_RX_DESC_DEFAULT 128
#define RTE_TX_DESC_DEFAULT 512

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
		addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2], \
		addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5])

uint8_t slaves[RTE_MAX_ETHPORTS];
uint8_t slaves_count;

static uint8_t BOND_PORT = 0xff;

static struct rte_mempool *mbuf_pool;

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_NONE,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload enabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static void
slave_port_init(uint8_t portid, struct rte_mempool *mbuf_pool)
{
	int retval;

	if (portid >= rte_eth_dev_count())
		rte_exit(EXIT_FAILURE, "Invalid port\n");

	retval = rte_eth_dev_configure(portid, 1, 1, &port_conf);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: configuration failed (res=%d)\n",
				portid, retval);

	/* RX setup */
	retval = rte_eth_rx_queue_setup(portid, 0, RTE_RX_DESC_DEFAULT,
					rte_eth_dev_socket_id(portid), NULL,
					mbuf_pool);
	if (retval < 0)
		rte_exit(retval, " port %u: RX queue 0 setup failed (res=%d)",
				portid, retval);

	/* TX setup */
	retval = rte_eth_tx_queue_setup(portid, 0, RTE_TX_DESC_DEFAULT,
				rte_eth_dev_socket_id(portid), NULL);

	if (retval < 0)
		rte_exit(retval, "port %u: TX queue 0 setup failed (res=%d)",
				portid, retval);

	retval  = rte_eth_dev_start(portid);
	if (retval < 0)
		rte_exit(retval,
				"Start port %d failed (res=%d)",
				portid, retval);

	struct ether_addr addr;

	rte_eth_macaddr_get(portid, &addr);
	printf("Port %u MAC: ", (unsigned)portid);
	PRINT_MAC(addr);
	printf("\n");
}

static void
bond_port_init(struct rte_mempool *mbuf_pool)
{
	int retval;
	uint8_t i;

	retval = rte_eth_bond_create("bond0", BONDING_MODE_ALB,
			0 /*SOCKET_ID_ANY*/);
	if (retval < 0)
		rte_exit(EXIT_FAILURE,
				"Faled to create bond port\n");

	BOND_PORT = (uint8_t)retval;

	retval = rte_eth_dev_configure(BOND_PORT, 1, 1, &port_conf);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "port %u: configuration failed (res=%d)\n",
				BOND_PORT, retval);

	/* RX setup */
	retval = rte_eth_rx_queue_setup(BOND_PORT, 0, RTE_RX_DESC_DEFAULT,
					rte_eth_dev_socket_id(BOND_PORT), NULL,
					mbuf_pool);
	if (retval < 0)
		rte_exit(retval, " port %u: RX queue 0 setup failed (res=%d)",
				BOND_PORT, retval);

	/* TX setup */
	retval = rte_eth_tx_queue_setup(BOND_PORT, 0, RTE_TX_DESC_DEFAULT,
				rte_eth_dev_socket_id(BOND_PORT), NULL);

	if (retval < 0)
		rte_exit(retval, "port %u: TX queue 0 setup failed (res=%d)",
				BOND_PORT, retval);

	for (i = 0; i < slaves_count; i++) {
		if (rte_eth_bond_slave_add(BOND_PORT, slaves[i]) == -1)
			rte_exit(-1, "Oooops! adding slave (%u) to bond (%u) failed!\n",
					slaves[i], BOND_PORT);

	}

	retval  = rte_eth_dev_start(BOND_PORT);
	if (retval < 0)
		rte_exit(retval, "Start port %d failed (res=%d)", BOND_PORT, retval);

	rte_eth_promiscuous_enable(BOND_PORT);

	struct ether_addr addr;

	rte_eth_macaddr_get(BOND_PORT, &addr);
	printf("Port %u MAC: ", (unsigned)BOND_PORT);
		PRINT_MAC(addr);
		printf("\n");
}

static inline size_t
get_vlan_offset(struct ether_hdr *eth_hdr, uint16_t *proto)
{
	size_t vlan_offset = 0;

	if (rte_cpu_to_be_16(ETHER_TYPE_VLAN) == *proto) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);

		vlan_offset = sizeof(struct vlan_hdr);
		*proto = vlan_hdr->eth_proto;

		if (rte_cpu_to_be_16(ETHER_TYPE_VLAN) == *proto) {
			vlan_hdr = vlan_hdr + 1;

			*proto = vlan_hdr->eth_proto;
			vlan_offset += sizeof(struct vlan_hdr);
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
static int lcore_main(__attribute__((unused)) void *arg1)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST] __rte_cache_aligned;
	struct ether_addr d_addr;

	struct ether_hdr *eth_hdr;
	struct arp_hdr *arp_hdr;
	struct ipv4_hdr *ipv4_hdr;
	uint16_t ether_type, offset;

	uint16_t rx_cnt;
	uint32_t bond_ip;
	int i = 0;
	uint8_t is_free;

	bond_ip = BOND_IP_1 | (BOND_IP_2 << 8) |
				(BOND_IP_3 << 16) | (BOND_IP_4 << 24);

	rte_spinlock_trylock(&global_flag_stru_p->lock);

	while (global_flag_stru_p->LcoreMainIsRunning) {
		rte_spinlock_unlock(&global_flag_stru_p->lock);
		rx_cnt = rte_eth_rx_burst(BOND_PORT, 0, pkts, MAX_PKT_BURST);
		is_free = 0;

		/* If didn't receive any packets, wait and go to next iteration */
		if (rx_cnt == 0) {
			rte_delay_us(50);
			continue;
		}

		/* Search incoming data for ARP packets and prepare response */
		for (i = 0; i < rx_cnt; i++) {
			if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1) {
				global_flag_stru_p->port_packets[0]++;
				rte_spinlock_unlock(&global_flag_stru_p->lock);
			}
			eth_hdr = rte_pktmbuf_mtod(pkts[i], struct ether_hdr *);
			ether_type = eth_hdr->ether_type;
			if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_VLAN))
				printf("VLAN taged frame, offset:");
			offset = get_vlan_offset(eth_hdr, &ether_type);
			if (offset > 0)
				printf("%d\n", offset);
			if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_ARP)) {
				if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1)     {
					global_flag_stru_p->port_packets[1]++;
					rte_spinlock_unlock(&global_flag_stru_p->lock);
				}
				arp_hdr = (struct arp_hdr *)((char *)(eth_hdr + 1) + offset);
				if (arp_hdr->arp_data.arp_tip == bond_ip) {
					if (arp_hdr->arp_op == rte_cpu_to_be_16(ARP_OP_REQUEST)) {
						arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
						/* Switch src and dst data and set bonding MAC */
						ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
						rte_eth_macaddr_get(BOND_PORT, &eth_hdr->s_addr);
						ether_addr_copy(&arp_hdr->arp_data.arp_sha, &arp_hdr->arp_data.arp_tha);
						arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
						rte_eth_macaddr_get(BOND_PORT, &d_addr);
						ether_addr_copy(&d_addr, &arp_hdr->arp_data.arp_sha);
						arp_hdr->arp_data.arp_sip = bond_ip;
						rte_eth_tx_burst(BOND_PORT, 0, &pkts[i], 1);
						is_free = 1;
					} else {
						rte_eth_tx_burst(BOND_PORT, 0, NULL, 0);
					}
				}
			} else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
				if (rte_spinlock_trylock(&global_flag_stru_p->lock) == 1)     {
					global_flag_stru_p->port_packets[2]++;
					rte_spinlock_unlock(&global_flag_stru_p->lock);
				 }
				ipv4_hdr = (struct ipv4_hdr *)((char *)(eth_hdr + 1) + offset);
				if (ipv4_hdr->dst_addr == bond_ip) {
					ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
					rte_eth_macaddr_get(BOND_PORT, &eth_hdr->s_addr);
					ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
					ipv4_hdr->src_addr = bond_ip;
					rte_eth_tx_burst(BOND_PORT, 0, &pkts[i], 1);
				}

			}

			/* Free processed packets */
			if (is_free == 0)
				rte_pktmbuf_free(pkts[i]);
		}
		rte_spinlock_trylock(&global_flag_stru_p->lock);
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
		__attribute__((unused)) struct cmdline *cl,
			       __attribute__((unused)) void *data)
{

	struct cmd_obj_send_result *res = parsed_result;
	char ip_str[INET6_ADDRSTRLEN];

	struct rte_mbuf *created_pkt;
	struct ether_hdr *eth_hdr;
	struct arp_hdr *arp_hdr;

	uint32_t bond_ip;
	size_t pkt_size;

	if (res->ip.family == AF_INET)
		get_string(res, ip_str, INET_ADDRSTRLEN);
	else
		cmdline_printf(cl, "Wrong IP format. Only IPv4 is supported\n");

	bond_ip = BOND_IP_1 | (BOND_IP_2 << 8) |
				(BOND_IP_3 << 16) | (BOND_IP_4 << 24);

	created_pkt = rte_pktmbuf_alloc(mbuf_pool);
	pkt_size = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
	created_pkt->data_len = pkt_size;
	created_pkt->pkt_len = pkt_size;

	eth_hdr = rte_pktmbuf_mtod(created_pkt, struct ether_hdr *);
	rte_eth_macaddr_get(BOND_PORT, &eth_hdr->s_addr);
	memset(&eth_hdr->d_addr, 0xFF, ETHER_ADDR_LEN);
	eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_ARP);

	arp_hdr = (struct arp_hdr *)((char *)eth_hdr + sizeof(struct ether_hdr));
	arp_hdr->arp_hrd = rte_cpu_to_be_16(ARP_HRD_ETHER);
	arp_hdr->arp_pro = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	arp_hdr->arp_hln = ETHER_ADDR_LEN;
	arp_hdr->arp_pln = sizeof(uint32_t);
	arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REQUEST);

	rte_eth_macaddr_get(BOND_PORT, &arp_hdr->arp_data.arp_sha);
	arp_hdr->arp_data.arp_sip = bond_ip;
	memset(&arp_hdr->arp_data.arp_tha, 0, ETHER_ADDR_LEN);
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

static void cmd_start_parsed(__attribute__((unused)) void *parsed_result,
			       struct cmdline *cl,
			       __attribute__((unused)) void *data)
{
	int slave_core_id = rte_lcore_id();

	rte_spinlock_trylock(&global_flag_stru_p->lock);
	if (global_flag_stru_p->LcoreMainIsRunning == 0)	{
		if (lcore_config[global_flag_stru_p->LcoreMainCore].state != WAIT)	{
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

	/* start lcore main on core != master_core - ARP response thread */
	slave_core_id = rte_get_next_lcore(rte_lcore_id(), 1, 0);
	if ((slave_core_id >= RTE_MAX_LCORE) || (slave_core_id == 0))
		return;

	rte_spinlock_trylock(&global_flag_stru_p->lock);
	global_flag_stru_p->LcoreMainIsRunning = 1;
	rte_spinlock_unlock(&global_flag_stru_p->lock);
	cmdline_printf(cl,
			"Starting lcore_main on core %d:%d "
			"Our IP:%d.%d.%d.%d\n",
			slave_core_id,
			rte_eal_remote_launch(lcore_main, NULL, slave_core_id),
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

static void cmd_help_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	cmdline_printf(cl,
			"ALB - link bonding mode 6 example\n"
			"send IP	- sends one ARPrequest thru bonding for IP.\n"
			"start		- starts listening ARPs.\n"
			"stop		- stops lcore_main.\n"
			"show		- shows some bond info: ex. active slaves etc.\n"
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

static void cmd_stop_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	rte_spinlock_trylock(&global_flag_stru_p->lock);
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

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	rte_spinlock_trylock(&global_flag_stru_p->lock);
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

static void cmd_show_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	uint8_t slaves[16] = {0};
	uint8_t len = 16;
	struct ether_addr addr;
	uint8_t i = 0;

	while (i < slaves_count)	{
		rte_eth_macaddr_get(i, &addr);
		PRINT_MAC(addr);
		printf("\n");
		i++;
	}

	rte_spinlock_trylock(&global_flag_stru_p->lock);
	cmdline_printf(cl,
			"Active_slaves:%d "
			"packets received:Tot:%d Arp:%d IPv4:%d\n",
			rte_eth_bond_active_slaves_get(BOND_PORT, slaves, len),
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

/* prompt function, called from main on MASTER lcore */
static void prompt(__attribute__((unused)) void *arg1)
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
	int ret;
	uint8_t nb_ports, i;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	rte_eal_devargs_dump(stdout);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "Give at least one port\n");
	else if (nb_ports > MAX_PORTS)
		rte_exit(EXIT_FAILURE, "You can have max 4 ports\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF, 32,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	slaves_count = nb_ports;
	for (i = 0; i < nb_ports; i++) {
		slave_port_init(i, mbuf_pool);
		slaves[i] = i;
	}

	bond_port_init(mbuf_pool);

	rte_spinlock_init(&global_flag_stru_p->lock);
	int slave_core_id = rte_lcore_id();

	/* check state of lcores */
	RTE_LCORE_FOREACH_SLAVE(slave_core_id) {
	if (lcore_config[slave_core_id].state != WAIT)
		return -EBUSY;
	}
	/* start lcore main on core != master_core - ARP response thread */
	slave_core_id = rte_get_next_lcore(rte_lcore_id(), 1, 0);
	if ((slave_core_id >= RTE_MAX_LCORE) || (slave_core_id == 0))
		return -EPERM;

	global_flag_stru_p->LcoreMainIsRunning = 1;
	global_flag_stru_p->LcoreMainCore = slave_core_id;
	printf("Starting lcore_main on core %d:%d Our IP:%d.%d.%d.%d\n",
			slave_core_id,
			rte_eal_remote_launch((lcore_function_t *)lcore_main,
					NULL,
					slave_core_id),
			BOND_IP_1,
			BOND_IP_2,
			BOND_IP_3,
			BOND_IP_4
		);

	/* Start prompt for user interact */
	prompt(NULL);

	rte_delay_ms(100);
	return 0;
}
