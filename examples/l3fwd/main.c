/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_vect.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
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
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include "l3fwd.h"
#include "l3fwd_event.h"
#include "l3fwd_route.h"

#define MAX_TX_QUEUE_PER_PORT RTE_MAX_LCORE
#define MAX_RX_QUEUE_PER_PORT 128

#define MAX_LCORE_PARAMS 1024

uint16_t nb_rxd = RX_DESC_DEFAULT;
uint16_t nb_txd = TX_DESC_DEFAULT;

/**< Ports set in promiscuous mode off by default. */
static int promiscuous_on;

/* Select Longest-Prefix, Exact match, Forwarding Information Base or Access Control. */
enum L3FWD_LOOKUP_MODE {
	L3FWD_LOOKUP_DEFAULT,
	L3FWD_LOOKUP_LPM,
	L3FWD_LOOKUP_EM,
	L3FWD_LOOKUP_FIB,
	L3FWD_LOOKUP_ACL
};
static enum L3FWD_LOOKUP_MODE lookup_mode;

/* Global variables. */
static int numa_on = 1; /**< NUMA is enabled by default. */
static int parse_ptype; /**< Parse packet type using rx callback, and */
			/**< disabled by default */
static int per_port_pool; /**< Use separate buffer pools per port; disabled */
			  /**< by default */

volatile bool force_quit;

/* ethernet addresses of ports */
uint64_t dest_eth_addr[RTE_MAX_ETHPORTS];
struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

xmm_t val_eth[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
uint32_t enabled_port_mask;

/* Used only in exact match mode. */
int ipv6; /**< ipv6 is false by default. */

struct lcore_conf lcore_conf[RTE_MAX_LCORE];

struct parm_cfg parm_config;

struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
	{0, 0, 2},
	{0, 1, 2},
	{0, 2, 2},
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
	{2, 0, 2},
	{3, 0, 3},
	{3, 1, 3},
};

static struct lcore_params * lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
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

uint32_t max_pkt_len;

static struct rte_mempool *pktmbuf_pool[RTE_MAX_ETHPORTS][NB_SOCKETS];
static struct rte_mempool *vector_pool[RTE_MAX_ETHPORTS];
static uint8_t lkp_per_socket[NB_SOCKETS];

struct l3fwd_lkp_mode {
	void  (*read_config_files)(void);
	void  (*setup)(int);
	int   (*check_ptype)(int);
	rte_rx_callback_fn cb_parse_ptype;
	int   (*main_loop)(void *);
	void* (*get_ipv4_lookup_struct)(int);
	void* (*get_ipv6_lookup_struct)(int);
	void  (*free_routes)(void);
};

static struct l3fwd_lkp_mode l3fwd_lkp;

static struct l3fwd_lkp_mode l3fwd_em_lkp = {
	.read_config_files		= read_config_files_em,
	.setup                  = setup_hash,
	.check_ptype		= em_check_ptype,
	.cb_parse_ptype		= em_cb_parse_ptype,
	.main_loop              = em_main_loop,
	.get_ipv4_lookup_struct = em_get_ipv4_l3fwd_lookup_struct,
	.get_ipv6_lookup_struct = em_get_ipv6_l3fwd_lookup_struct,
	.free_routes			= em_free_routes,
};

static struct l3fwd_lkp_mode l3fwd_lpm_lkp = {
	.read_config_files		= read_config_files_lpm,
	.setup                  = setup_lpm,
	.check_ptype		= lpm_check_ptype,
	.cb_parse_ptype		= lpm_cb_parse_ptype,
	.main_loop              = lpm_main_loop,
	.get_ipv4_lookup_struct = lpm_get_ipv4_l3fwd_lookup_struct,
	.get_ipv6_lookup_struct = lpm_get_ipv6_l3fwd_lookup_struct,
	.free_routes			= lpm_free_routes,
};

static struct l3fwd_lkp_mode l3fwd_fib_lkp = {
	.read_config_files		= read_config_files_lpm,
	.setup                  = setup_fib,
	.check_ptype            = lpm_check_ptype,
	.cb_parse_ptype         = lpm_cb_parse_ptype,
	.main_loop              = fib_main_loop,
	.get_ipv4_lookup_struct = fib_get_ipv4_l3fwd_lookup_struct,
	.get_ipv6_lookup_struct = fib_get_ipv6_l3fwd_lookup_struct,
	.free_routes			= lpm_free_routes,
};

static struct l3fwd_lkp_mode l3fwd_acl_lkp = {
	.read_config_files		= read_config_files_acl,
	.setup                  = setup_acl,
	.check_ptype            = em_check_ptype,
	.cb_parse_ptype         = em_cb_parse_ptype,
	.main_loop              = acl_main_loop,
	.get_ipv4_lookup_struct = acl_get_ipv4_l3fwd_lookup_struct,
	.get_ipv6_lookup_struct = acl_get_ipv6_l3fwd_lookup_struct,
	.free_routes			= acl_free_routes,
};

/*
 * 198.18.0.0/16 are set aside for RFC2544 benchmarking (RFC5735).
 * 198.18.{0-15}.0/24 = Port {0-15}
 */
const struct ipv4_l3fwd_route ipv4_l3fwd_route_array[] = {
	{RTE_IPV4(198, 18, 0, 0), 24, 0},
	{RTE_IPV4(198, 18, 1, 0), 24, 1},
	{RTE_IPV4(198, 18, 2, 0), 24, 2},
	{RTE_IPV4(198, 18, 3, 0), 24, 3},
	{RTE_IPV4(198, 18, 4, 0), 24, 4},
	{RTE_IPV4(198, 18, 5, 0), 24, 5},
	{RTE_IPV4(198, 18, 6, 0), 24, 6},
	{RTE_IPV4(198, 18, 7, 0), 24, 7},
	{RTE_IPV4(198, 18, 8, 0), 24, 8},
	{RTE_IPV4(198, 18, 9, 0), 24, 9},
	{RTE_IPV4(198, 18, 10, 0), 24, 10},
	{RTE_IPV4(198, 18, 11, 0), 24, 11},
	{RTE_IPV4(198, 18, 12, 0), 24, 12},
	{RTE_IPV4(198, 18, 13, 0), 24, 13},
	{RTE_IPV4(198, 18, 14, 0), 24, 14},
	{RTE_IPV4(198, 18, 15, 0), 24, 15},
};

/*
 * 2001:200::/48 is IANA reserved range for IPv6 benchmarking (RFC5180).
 * 2001:200:0:{0-f}::/64 = Port {0-15}
 */
const struct ipv6_l3fwd_route ipv6_l3fwd_route_array[] = {
	{{32, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 0},
	{{32, 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 1},
	{{32, 1, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 2},
	{{32, 1, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 3},
	{{32, 1, 2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 4},
	{{32, 1, 2, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 5},
	{{32, 1, 2, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 6},
	{{32, 1, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 7},
	{{32, 1, 2, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 8},
	{{32, 1, 2, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 9},
	{{32, 1, 2, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 10},
	{{32, 1, 2, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 11},
	{{32, 1, 2, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 12},
	{{32, 1, 2, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 13},
	{{32, 1, 2, 0, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 14},
	{{32, 1, 2, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0}, 64, 15},
};

/*
 * API's called during initialization to setup ACL/EM/LPM rules.
 */
void
l3fwd_set_rule_ipv4_name(const char *optarg)
{
	parm_config.rule_ipv4_name = optarg;
}

void
l3fwd_set_rule_ipv6_name(const char *optarg)
{
	parm_config.rule_ipv6_name = optarg;
}

void
l3fwd_set_alg(const char *optarg)
{
	parm_config.alg = parse_acl_alg(optarg);
}

/*
 * Setup lookup methods for forwarding.
 * Currently exact-match, longest-prefix-match and forwarding information
 * base are the supported ones.
 */
static void
setup_l3fwd_lookup_tables(void)
{
	/* Setup HASH lookup functions. */
	if (lookup_mode == L3FWD_LOOKUP_EM)
		l3fwd_lkp = l3fwd_em_lkp;
	/* Setup FIB lookup functions. */
	else if (lookup_mode == L3FWD_LOOKUP_FIB)
		l3fwd_lkp = l3fwd_fib_lkp;
	/* Setup ACL lookup functions. */
	else if (lookup_mode == L3FWD_LOOKUP_ACL)
		l3fwd_lkp = l3fwd_acl_lkp;
	/* Setup LPM lookup functions. */
	else
		l3fwd_lkp = l3fwd_lpm_lkp;
}

static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			printf("invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("error: lcore %hhu is not enabled in lcore mask\n", lcore);
			return -1;
		}
		if ((socketid = rte_lcore_to_socket_id(lcore) != 0) &&
			(numa_on == 0)) {
			printf("warning: lcore %hhu is on socket %d with numa off \n",
				lcore, socketid);
		}
	}
	return 0;
}

static int
check_port_config(void)
{
	uint16_t portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (!rte_eth_dev_is_valid_port(portid)) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_n_rx_queues(const uint16_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port) {
			if (lcore_params[i].queue_id == queue+1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE, "queue ids of the port %d must be"
						" in sequence and must start with 0\n",
						lcore_params[i].port_id);
		}
	}
	return (uint8_t)(++queue);
}

static int
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		} else {
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
				lcore_params[i].port_id;
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
				lcore_params[i].queue_id;
			lcore_conf[lcore].n_rx_queue++;
		}
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	char alg[PATH_MAX];

	usage_acl_alg(alg, sizeof(alg));
	fprintf(stderr, "%s [EAL options] --"
		" -p PORTMASK"
		"  --rule_ipv4=FILE"
		"  --rule_ipv6=FILE"
		" [-P]"
		" [--lookup]"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--rx-queue-size NPKTS]"
		" [--tx-queue-size NPKTS]"
		" [--eth-dest=X,MM:MM:MM:MM:MM:MM]"
		" [--max-pkt-len PKTLEN]"
		" [--no-numa]"
		" [--ipv6]"
		" [--parse-ptype]"
		" [--per-port-pool]"
		" [--mode]"
		" [--eventq-sched]"
		" [--event-vector [--event-vector-size SIZE] [--event-vector-tmo NS]]"
		" [-E]"
		" [-L]\n\n"

		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  --lookup: Select the lookup method\n"
		"            Default: lpm\n"
		"            Accepted: em (Exact Match), lpm (Longest Prefix Match), fib (Forwarding Information Base),\n"
		"                      acl (Access Control List)\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --rx-queue-size NPKTS: Rx queue size in decimal\n"
		"            Default: %d\n"
		"  --tx-queue-size NPKTS: Tx queue size in decimal\n"
		"            Default: %d\n"
		"  --eth-dest=X,MM:MM:MM:MM:MM:MM: Ethernet destination for port X\n"
		"  --max-pkt-len PKTLEN: maximum packet length in decimal (64-9600)\n"
		"  --no-numa: Disable numa awareness\n"
		"  --ipv6: Set if running ipv6 packets\n"
		"  --parse-ptype: Set to use software to analyze packet type\n"
		"  --per-port-pool: Use separate buffer pool per port\n"
		"  --mode: Packet transfer mode for I/O, poll or eventdev\n"
		"          Default mode = poll\n"
		"  --eventq-sched: Event queue synchronization method\n"
		"                  ordered, atomic or parallel.\n"
		"                  Default: atomic\n"
		"                  Valid only if --mode=eventdev\n"
		"  --event-eth-rxqs: Number of ethernet RX queues per device.\n"
		"                    Default: 1\n"
		"                    Valid only if --mode=eventdev\n"
		"  --event-vector:  Enable event vectorization.\n"
		"  --event-vector-size: Max vector size if event vectorization is enabled.\n"
		"  --event-vector-tmo: Max timeout to form vector in nanoseconds if event vectorization is enabled\n"
		"  -E : Enable exact match, legacy flag please use --lookup=em instead\n"
		"  -L : Enable longest prefix match, legacy flag please use --lookup=lpm instead\n"
		"  --rule_ipv4=FILE: Specify the ipv4 rules entries file.\n"
		"                    Each rule occupies one line.\n"
		"                    2 kinds of rules are supported.\n"
		"                    One is ACL entry at while line leads with character '%c',\n"
		"                    another is route entry at while line leads with character '%c'.\n"
		"  --rule_ipv6=FILE: Specify the ipv6 rules entries file.\n"
		"  --alg: ACL classify method to use, one of: %s.\n\n",
		prgname, RX_DESC_DEFAULT, TX_DESC_DEFAULT,
		ACL_LEAD_CHAR, ROUTE_LEAD_CHAR, alg);
}

static int
parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
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

static int
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++){
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}

static void
parse_eth_dest(const char *optarg)
{
	uint16_t portid;
	char *port_end;
	uint8_t c, *dest, peer_addr[6];

	errno = 0;
	portid = strtoul(optarg, &port_end, 10);
	if (errno != 0 || port_end == optarg || *port_end++ != ',')
		rte_exit(EXIT_FAILURE,
		"Invalid eth-dest: %s", optarg);
	if (portid >= RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE,
		"eth-dest: port %d >= RTE_MAX_ETHPORTS(%d)\n",
		portid, RTE_MAX_ETHPORTS);

	if (cmdline_parse_etheraddr(NULL, port_end,
		&peer_addr, sizeof(peer_addr)) < 0)
		rte_exit(EXIT_FAILURE,
		"Invalid ethernet address: %s\n",
		port_end);
	dest = (uint8_t *)&dest_eth_addr[portid];
	for (c = 0; c < 6; c++)
		dest[c] = peer_addr[c];
	*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
}

static void
parse_mode(const char *optarg)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	if (!strcmp(optarg, "poll"))
		evt_rsrc->enabled = false;
	else if (!strcmp(optarg, "eventdev"))
		evt_rsrc->enabled = true;
}

static void
parse_queue_size(const char *queue_size_arg, uint16_t *queue_size, int rx)
{
	char *end = NULL;
	unsigned long value;

	/* parse decimal string */
	value = strtoul(queue_size_arg, &end, 10);
	if ((queue_size_arg[0] == '\0') || (end == NULL) ||
		(*end != '\0') || (value == 0)) {
		if (rx == 1)
			rte_exit(EXIT_FAILURE, "Invalid rx-queue-size\n");
		else
			rte_exit(EXIT_FAILURE, "Invalid tx-queue-size\n");

		return;
	}

	if (value > UINT16_MAX) {
		if (rx == 1)
			rte_exit(EXIT_FAILURE, "rx-queue-size %lu > %d\n",
				value, UINT16_MAX);
		else
			rte_exit(EXIT_FAILURE, "tx-queue-size %lu > %d\n",
				value, UINT16_MAX);

		return;
	}

	*queue_size = value;
}

static void
parse_eventq_sched(const char *optarg)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	if (!strcmp(optarg, "ordered"))
		evt_rsrc->sched_type = RTE_SCHED_TYPE_ORDERED;
	if (!strcmp(optarg, "atomic"))
		evt_rsrc->sched_type = RTE_SCHED_TYPE_ATOMIC;
	if (!strcmp(optarg, "parallel"))
		evt_rsrc->sched_type = RTE_SCHED_TYPE_PARALLEL;
}

static void
parse_event_eth_rx_queues(const char *eth_rx_queues)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	char *end = NULL;
	uint8_t num_eth_rx_queues;

	/* parse decimal string */
	num_eth_rx_queues = strtoul(eth_rx_queues, &end, 10);
	if ((eth_rx_queues[0] == '\0') || (end == NULL) || (*end != '\0'))
		return;

	if (num_eth_rx_queues == 0)
		return;

	evt_rsrc->eth_rx_queues = num_eth_rx_queues;
}

static int
parse_lookup(const char *optarg)
{
	if (!strcmp(optarg, "em"))
		lookup_mode = L3FWD_LOOKUP_EM;
	else if (!strcmp(optarg, "lpm"))
		lookup_mode = L3FWD_LOOKUP_LPM;
	else if (!strcmp(optarg, "fib"))
		lookup_mode = L3FWD_LOOKUP_FIB;
	else if (!strcmp(optarg, "acl"))
		lookup_mode = L3FWD_LOOKUP_ACL;
	else {
		fprintf(stderr, "Invalid lookup option! Accepted options: acl, em, lpm, fib\n");
		return -1;
	}
	return 0;
}

#define MAX_JUMBO_PKT_LEN  9600

static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	"L"   /* legacy enable long prefix match */
	"E"   /* legacy enable exact match */
	;

#define CMD_LINE_OPT_CONFIG "config"
#define CMD_LINE_OPT_RX_QUEUE_SIZE "rx-queue-size"
#define CMD_LINE_OPT_TX_QUEUE_SIZE "tx-queue-size"
#define CMD_LINE_OPT_ETH_DEST "eth-dest"
#define CMD_LINE_OPT_NO_NUMA "no-numa"
#define CMD_LINE_OPT_IPV6 "ipv6"
#define CMD_LINE_OPT_MAX_PKT_LEN "max-pkt-len"
#define CMD_LINE_OPT_HASH_ENTRY_NUM "hash-entry-num"
#define CMD_LINE_OPT_PARSE_PTYPE "parse-ptype"
#define CMD_LINE_OPT_PER_PORT_POOL "per-port-pool"
#define CMD_LINE_OPT_MODE "mode"
#define CMD_LINE_OPT_EVENTQ_SYNC "eventq-sched"
#define CMD_LINE_OPT_EVENT_ETH_RX_QUEUES "event-eth-rxqs"
#define CMD_LINE_OPT_LOOKUP "lookup"
#define CMD_LINE_OPT_ENABLE_VECTOR "event-vector"
#define CMD_LINE_OPT_VECTOR_SIZE "event-vector-size"
#define CMD_LINE_OPT_VECTOR_TMO_NS "event-vector-tmo"
#define CMD_LINE_OPT_RULE_IPV4 "rule_ipv4"
#define CMD_LINE_OPT_RULE_IPV6 "rule_ipv6"
#define CMD_LINE_OPT_ALG "alg"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_RX_QUEUE_SIZE_NUM,
	CMD_LINE_OPT_TX_QUEUE_SIZE_NUM,
	CMD_LINE_OPT_ETH_DEST_NUM,
	CMD_LINE_OPT_NO_NUMA_NUM,
	CMD_LINE_OPT_IPV6_NUM,
	CMD_LINE_OPT_MAX_PKT_LEN_NUM,
	CMD_LINE_OPT_HASH_ENTRY_NUM_NUM,
	CMD_LINE_OPT_PARSE_PTYPE_NUM,
	CMD_LINE_OPT_RULE_IPV4_NUM,
	CMD_LINE_OPT_RULE_IPV6_NUM,
	CMD_LINE_OPT_ALG_NUM,
	CMD_LINE_OPT_PARSE_PER_PORT_POOL,
	CMD_LINE_OPT_MODE_NUM,
	CMD_LINE_OPT_EVENTQ_SYNC_NUM,
	CMD_LINE_OPT_EVENT_ETH_RX_QUEUES_NUM,
	CMD_LINE_OPT_LOOKUP_NUM,
	CMD_LINE_OPT_ENABLE_VECTOR_NUM,
	CMD_LINE_OPT_VECTOR_SIZE_NUM,
	CMD_LINE_OPT_VECTOR_TMO_NS_NUM
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_RX_QUEUE_SIZE, 1, 0, CMD_LINE_OPT_RX_QUEUE_SIZE_NUM},
	{CMD_LINE_OPT_TX_QUEUE_SIZE, 1, 0, CMD_LINE_OPT_TX_QUEUE_SIZE_NUM},
	{CMD_LINE_OPT_ETH_DEST, 1, 0, CMD_LINE_OPT_ETH_DEST_NUM},
	{CMD_LINE_OPT_NO_NUMA, 0, 0, CMD_LINE_OPT_NO_NUMA_NUM},
	{CMD_LINE_OPT_IPV6, 0, 0, CMD_LINE_OPT_IPV6_NUM},
	{CMD_LINE_OPT_MAX_PKT_LEN, 1, 0, CMD_LINE_OPT_MAX_PKT_LEN_NUM},
	{CMD_LINE_OPT_HASH_ENTRY_NUM, 1, 0, CMD_LINE_OPT_HASH_ENTRY_NUM_NUM},
	{CMD_LINE_OPT_PARSE_PTYPE, 0, 0, CMD_LINE_OPT_PARSE_PTYPE_NUM},
	{CMD_LINE_OPT_PER_PORT_POOL, 0, 0, CMD_LINE_OPT_PARSE_PER_PORT_POOL},
	{CMD_LINE_OPT_MODE, 1, 0, CMD_LINE_OPT_MODE_NUM},
	{CMD_LINE_OPT_EVENTQ_SYNC, 1, 0, CMD_LINE_OPT_EVENTQ_SYNC_NUM},
	{CMD_LINE_OPT_EVENT_ETH_RX_QUEUES, 1, 0,
					CMD_LINE_OPT_EVENT_ETH_RX_QUEUES_NUM},
	{CMD_LINE_OPT_LOOKUP, 1, 0, CMD_LINE_OPT_LOOKUP_NUM},
	{CMD_LINE_OPT_ENABLE_VECTOR, 0, 0, CMD_LINE_OPT_ENABLE_VECTOR_NUM},
	{CMD_LINE_OPT_VECTOR_SIZE, 1, 0, CMD_LINE_OPT_VECTOR_SIZE_NUM},
	{CMD_LINE_OPT_VECTOR_TMO_NS, 1, 0, CMD_LINE_OPT_VECTOR_TMO_NS_NUM},
	{CMD_LINE_OPT_RULE_IPV4,   1, 0, CMD_LINE_OPT_RULE_IPV4_NUM},
	{CMD_LINE_OPT_RULE_IPV6,   1, 0, CMD_LINE_OPT_RULE_IPV6_NUM},
	{CMD_LINE_OPT_ALG,   1, 0, CMD_LINE_OPT_ALG_NUM},
	{NULL, 0, 0, 0}
};

/*
 * This expression is used to calculate the number of mbufs needed
 * depending on user input, taking  into account memory for rx and
 * tx hardware rings, cache per lcore and mtable per port per lcore.
 * RTE_MAX is used to ensure that NB_MBUF never goes below a minimum
 * value of 8192
 */
#define NB_MBUF(nports) RTE_MAX(	\
	(nports*nb_rx_queue*nb_rxd +		\
	nports*nb_lcores*MAX_PKT_BURST +	\
	nports*n_tx_queue*nb_txd +		\
	nb_lcores*MEMPOOL_CACHE_SIZE),		\
	(unsigned)8192)

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	uint8_t lcore_params = 0;
	uint8_t eventq_sched = 0;
	uint8_t eth_rx_q = 0;
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	argvopt = argv;

	/* Error or normal output strings. */
	while ((opt = getopt_long(argc, argvopt, short_options,
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				fprintf(stderr, "Invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;

		case 'P':
			promiscuous_on = 1;
			break;

		case 'E':
			if (lookup_mode != L3FWD_LOOKUP_DEFAULT) {
				fprintf(stderr, "Only one lookup mode is allowed at a time!\n");
				return -1;
			}
			lookup_mode = L3FWD_LOOKUP_EM;
			break;

		case 'L':
			if (lookup_mode != L3FWD_LOOKUP_DEFAULT) {
				fprintf(stderr, "Only one lookup mode is allowed at a time!\n");
				return -1;
			}
			lookup_mode = L3FWD_LOOKUP_LPM;
			break;

		/* long options */
		case CMD_LINE_OPT_CONFIG_NUM:
			ret = parse_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid config\n");
				print_usage(prgname);
				return -1;
			}
			lcore_params = 1;
			break;

		case CMD_LINE_OPT_RX_QUEUE_SIZE_NUM:
			parse_queue_size(optarg, &nb_rxd, 1);
			break;

		case CMD_LINE_OPT_TX_QUEUE_SIZE_NUM:
			parse_queue_size(optarg, &nb_txd, 0);
			break;

		case CMD_LINE_OPT_ETH_DEST_NUM:
			parse_eth_dest(optarg);
			break;

		case CMD_LINE_OPT_NO_NUMA_NUM:
			numa_on = 0;
			break;

		case CMD_LINE_OPT_IPV6_NUM:
			ipv6 = 1;
			break;

		case CMD_LINE_OPT_MAX_PKT_LEN_NUM:
			max_pkt_len = parse_max_pkt_len(optarg);
			break;

		case CMD_LINE_OPT_HASH_ENTRY_NUM_NUM:
			fprintf(stderr, "Hash entry number will be ignored\n");
			break;

		case CMD_LINE_OPT_PARSE_PTYPE_NUM:
			printf("soft parse-ptype is enabled\n");
			parse_ptype = 1;
			break;

		case CMD_LINE_OPT_PARSE_PER_PORT_POOL:
			printf("per port buffer pool is enabled\n");
			per_port_pool = 1;
			break;

		case CMD_LINE_OPT_MODE_NUM:
			parse_mode(optarg);
			break;

		case CMD_LINE_OPT_EVENTQ_SYNC_NUM:
			parse_eventq_sched(optarg);
			eventq_sched = 1;
			break;

		case CMD_LINE_OPT_EVENT_ETH_RX_QUEUES_NUM:
			parse_event_eth_rx_queues(optarg);
			eth_rx_q = 1;
			break;

		case CMD_LINE_OPT_LOOKUP_NUM:
			if (lookup_mode != L3FWD_LOOKUP_DEFAULT) {
				fprintf(stderr, "Only one lookup mode is allowed at a time!\n");
				return -1;
			}
			ret = parse_lookup(optarg);
			/*
			 * If parse_lookup was passed an invalid lookup type
			 * then return -1. Error log included within
			 * parse_lookup for simplicity.
			 */
			if (ret)
				return -1;
			break;

		case CMD_LINE_OPT_ENABLE_VECTOR_NUM:
			printf("event vectorization is enabled\n");
			evt_rsrc->vector_enabled = 1;
			break;
		case CMD_LINE_OPT_VECTOR_SIZE_NUM:
			evt_rsrc->vector_size = strtol(optarg, NULL, 10);
			break;
		case CMD_LINE_OPT_VECTOR_TMO_NS_NUM:
			evt_rsrc->vector_tmo_ns = strtoull(optarg, NULL, 10);
			break;
		case CMD_LINE_OPT_RULE_IPV4_NUM:
			l3fwd_set_rule_ipv4_name(optarg);
			break;
		case CMD_LINE_OPT_RULE_IPV6_NUM:
			l3fwd_set_rule_ipv6_name(optarg);
			break;
		case CMD_LINE_OPT_ALG_NUM:
			l3fwd_set_alg(optarg);
			break;
		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (evt_rsrc->enabled && lcore_params) {
		fprintf(stderr, "lcore config is not valid when event mode is selected\n");
		return -1;
	}

	if (!evt_rsrc->enabled && eth_rx_q) {
		fprintf(stderr, "eth_rx_queues is valid only when event mode is selected\n");
		return -1;
	}

	if (!evt_rsrc->enabled && eventq_sched) {
		fprintf(stderr, "eventq_sched is valid only when event mode is selected\n");
		return -1;
	}

	if (evt_rsrc->vector_enabled && !evt_rsrc->vector_size) {
		evt_rsrc->vector_size = VECTOR_SIZE_DEFAULT;
		fprintf(stderr, "vector size set to default (%" PRIu16 ")\n",
			evt_rsrc->vector_size);
	}

	if (evt_rsrc->vector_enabled && !evt_rsrc->vector_tmo_ns) {
		evt_rsrc->vector_tmo_ns = VECTOR_TMO_NS_DEFAULT;
		fprintf(stderr,
			"vector timeout set to default (%" PRIu64 " ns)\n",
			evt_rsrc->vector_tmo_ns);
	}

	/*
	 * Nothing is selected, pick longest-prefix match
	 * as default match.
	 */
	if (lookup_mode == L3FWD_LOOKUP_DEFAULT) {
		fprintf(stderr, "Neither ACL, LPM, EM, or FIB selected, defaulting to LPM\n");
		lookup_mode = L3FWD_LOOKUP_LPM;
	}

	/* For ACL, update port config rss hash filter */
	if (lookup_mode == L3FWD_LOOKUP_ACL) {
		port_conf.rx_adv_conf.rss_conf.rss_hf |=
				RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_SCTP;
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

int
init_mem(uint16_t portid, unsigned int nb_mbuf)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	struct lcore_conf *qconf;
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= NB_SOCKETS) {
			rte_exit(EXIT_FAILURE,
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
		}

		if (pktmbuf_pool[portid][socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d:%d",
				 portid, socketid);
			pktmbuf_pool[portid][socketid] =
				rte_pktmbuf_pool_create(s, nb_mbuf,
					MEMPOOL_CACHE_SIZE, 0,
					RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
			if (pktmbuf_pool[portid][socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot init mbuf pool on socket %d\n",
					socketid);
			else
				printf("Allocated mbuf pool on socket %d\n",
					socketid);

			/* Setup ACL, LPM, EM(f.e Hash) or FIB. But, only once per
			 * available socket.
			 */
			if (!lkp_per_socket[socketid]) {
				l3fwd_lkp.setup(socketid);
				lkp_per_socket[socketid] = 1;
			}
		}

		if (evt_rsrc->vector_enabled && vector_pool[portid] == NULL) {
			unsigned int nb_vec;

			nb_vec = (nb_mbuf + evt_rsrc->vector_size - 1) /
				 evt_rsrc->vector_size;
			nb_vec = RTE_MAX(512U, nb_vec);
			nb_vec += rte_lcore_count() * 32;
			snprintf(s, sizeof(s), "vector_pool_%d", portid);
			vector_pool[portid] = rte_event_vector_pool_create(
				s, nb_vec, 32, evt_rsrc->vector_size, socketid);
			if (vector_pool[portid] == NULL)
				rte_exit(EXIT_FAILURE,
					 "Failed to create vector pool for port %d\n",
					 portid);
			else
				printf("Allocated vector pool for port %d\n",
				       portid);
		}

		qconf = &lcore_conf[lcore_id];
		qconf->ipv4_lookup_struct =
			l3fwd_lkp.get_ipv4_lookup_struct(socketid);
		qconf->ipv6_lookup_struct =
			l3fwd_lkp.get_ipv6_lookup_struct(socketid);
	}
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
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

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

static int
prepare_ptype_parser(uint16_t portid, uint16_t queueid)
{
	if (parse_ptype) {
		printf("Port %d: softly parse packet type info\n", portid);
		if (rte_eth_add_rx_callback(portid, queueid,
					    l3fwd_lkp.cb_parse_ptype,
					    NULL))
			return 1;

		printf("Failed to add rx callback: port=%d\n", portid);
		return 0;
	}

	if (l3fwd_lkp.check_ptype(portid))
		return 1;

	printf("port %d cannot parse packet type, please add --%s\n",
	       portid, CMD_LINE_OPT_PARSE_PTYPE);
	return 0;
}

static uint32_t
eth_dev_get_overhead_len(uint32_t max_rx_pktlen, uint16_t max_mtu)
{
	uint32_t overhead_len;

	if (max_mtu != UINT16_MAX && max_rx_pktlen > max_mtu)
		overhead_len = max_rx_pktlen - max_mtu;
	else
		overhead_len = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	return overhead_len;
}

int
config_port_max_pkt_len(struct rte_eth_conf *conf,
		struct rte_eth_dev_info *dev_info)
{
	uint32_t overhead_len;

	if (max_pkt_len == 0)
		return 0;

	if (max_pkt_len < RTE_ETHER_MIN_LEN || max_pkt_len > MAX_JUMBO_PKT_LEN)
		return -1;

	overhead_len = eth_dev_get_overhead_len(dev_info->max_rx_pktlen,
			dev_info->max_mtu);
	conf->rxmode.mtu = max_pkt_len - overhead_len;

	if (conf->rxmode.mtu > RTE_ETHER_MTU)
		conf->txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	return 0;
}

static void
l3fwd_poll_resource_setup(void)
{
	uint8_t nb_rx_queue, queue, socketid;
	struct rte_eth_dev_info dev_info;
	uint32_t n_tx_queue, nb_lcores;
	struct rte_eth_txconf *txconf;
	struct lcore_conf *qconf;
	uint16_t queueid, portid;
	unsigned int nb_ports;
	unsigned int lcore_id;
	int ret;

	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");

	nb_ports = rte_eth_dev_count_avail();

	if (check_port_config() < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	nb_lcores = rte_lcore_count();

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_conf local_port_conf = port_conf;

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid );
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)n_tx_queue );

		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				portid, strerror(-ret));

		ret = config_port_max_pkt_len(&local_port_conf, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Invalid max packet length: %u (port %u)\n",
				max_pkt_len, portid);

		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

		local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
			dev_info.flow_type_rss_offloads;

		if (dev_info.max_rx_queues == 1)
			local_port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;

		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
				port_conf.rx_adv_conf.rss_conf.rss_hf) {
			printf("Port %u modified RSS hash function based on hardware support,"
				"requested:%#"PRIx64" configured:%#"PRIx64"\n",
				portid,
				port_conf.rx_adv_conf.rss_conf.rss_hf,
				local_port_conf.rx_adv_conf.rss_conf.rss_hf);
		}

		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, "
				 "port=%d\n", ret, portid);

		ret = rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot get MAC address: err=%d, port=%d\n",
				 ret, portid);

		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");
		print_ethaddr("Destination:",
			(const struct rte_ether_addr *)&dest_eth_addr[portid]);
		printf(", ");

		/*
		 * prepare src MACs for each port.
		 */
		rte_ether_addr_copy(&ports_eth_addr[portid],
			(struct rte_ether_addr *)(val_eth + portid) + 1);

		/* init memory */
		if (!per_port_pool) {
			/* portid = 0; this is *not* signifying the first port,
			 * rather, it signifies that portid is ignored.
			 */
			ret = init_mem(0, NB_MBUF(nb_ports));
		} else {
			ret = init_mem(portid, NB_MBUF(1));
		}
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			txconf = &dev_info.default_txconf;
			txconf->offloads = local_port_conf.txmode.offloads;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socketid, txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			qconf = &lcore_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;

			qconf->tx_port_id[qconf->n_tx_port] = portid;
			qconf->n_tx_port++;
		}
		printf("\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id );
		fflush(stdout);
		/* init RX queues */
		for(queue = 0; queue < qconf->n_rx_queue; ++queue) {
			struct rte_eth_rxconf rxq_conf;

			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			ret = rte_eth_dev_info_get(portid, &dev_info);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"Error during getting device (port %u) info: %s\n",
					portid, strerror(-ret));

			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = port_conf.rxmode.offloads;
			if (!per_port_pool)
				ret = rte_eth_rx_queue_setup(portid, queueid,
						nb_rxd, socketid,
						&rxq_conf,
						pktmbuf_pool[0][socketid]);
			else
				ret = rte_eth_rx_queue_setup(portid, queueid,
						nb_rxd, socketid,
						&rxq_conf,
						pktmbuf_pool[portid][socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup: err=%d, port=%d\n",
				ret, portid);
		}
	}
}

static inline int
l3fwd_service_enable(uint32_t service_id)
{
	uint8_t min_service_count = UINT8_MAX;
	uint32_t slcore_array[RTE_MAX_LCORE];
	unsigned int slcore = 0;
	uint8_t service_count;
	int32_t slcore_count;

	if (!rte_service_lcore_count())
		return -ENOENT;

	slcore_count = rte_service_lcore_list(slcore_array, RTE_MAX_LCORE);
	if (slcore_count < 0)
		return -ENOENT;
	/* Get the core which has least number of services running. */
	while (slcore_count--) {
		/* Reset default mapping */
		if (rte_service_map_lcore_set(service_id,
				slcore_array[slcore_count], 0) != 0)
			return -ENOENT;
		service_count = rte_service_lcore_count_services(
				slcore_array[slcore_count]);
		if (service_count < min_service_count) {
			slcore = slcore_array[slcore_count];
			min_service_count = service_count;
		}
	}
	if (rte_service_map_lcore_set(service_id, slcore, 1))
		return -ENOENT;
	rte_service_lcore_start(slcore);

	return 0;
}

static void
l3fwd_event_service_setup(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	struct rte_event_dev_info evdev_info;
	uint32_t service_id, caps;
	int ret, i;

	rte_event_dev_info_get(evt_rsrc->event_d_id, &evdev_info);
	if (!(evdev_info.event_dev_cap & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED)) {
		ret = rte_event_dev_service_id_get(evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_exit(EXIT_FAILURE,
				 "Error in starting eventdev service\n");
		l3fwd_service_enable(service_id);
	}

	for (i = 0; i < evt_rsrc->rx_adptr.nb_rx_adptr; i++) {
		ret = rte_event_eth_rx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->rx_adptr.rx_adptr[i], &caps);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Failed to get Rx adapter[%d] caps\n",
				 evt_rsrc->rx_adptr.rx_adptr[i]);
		ret = rte_event_eth_rx_adapter_service_id_get(
				evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_exit(EXIT_FAILURE,
				 "Error in starting Rx adapter[%d] service\n",
				 evt_rsrc->rx_adptr.rx_adptr[i]);
		l3fwd_service_enable(service_id);
	}

	for (i = 0; i < evt_rsrc->tx_adptr.nb_tx_adptr; i++) {
		ret = rte_event_eth_tx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->tx_adptr.tx_adptr[i], &caps);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Failed to get Rx adapter[%d] caps\n",
				 evt_rsrc->tx_adptr.tx_adptr[i]);
		ret = rte_event_eth_tx_adapter_service_id_get(
				evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_exit(EXIT_FAILURE,
				 "Error in starting Rx adapter[%d] service\n",
				 evt_rsrc->tx_adptr.tx_adptr[i]);
		l3fwd_service_enable(service_id);
	}
}

int
main(int argc, char **argv)
{
	struct l3fwd_event_resources *evt_rsrc;
	struct lcore_conf *qconf;
	uint16_t queueid, portid;
	unsigned int lcore_id;
	uint8_t queue;
	int i, ret;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* pre-init dst MACs for all ports to 02:00:00:00:00:xx */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		dest_eth_addr[portid] =
			RTE_ETHER_LOCAL_ADMIN_ADDR + ((uint64_t)portid << 40);
		*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
	}

	evt_rsrc = l3fwd_get_eventdev_rsrc();
	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD parameters\n");

	/* Setup function pointers for lookup method. */
	setup_l3fwd_lookup_tables();

	/* Add the config file rules */
	l3fwd_lkp.read_config_files();

	evt_rsrc->per_port_pool = per_port_pool;
	evt_rsrc->pkt_pool = pktmbuf_pool;
	evt_rsrc->vec_pool = vector_pool;
	evt_rsrc->port_mask = enabled_port_mask;
	/* Configure eventdev parameters if user has requested */
	if (evt_rsrc->enabled) {
		l3fwd_event_resource_setup(&port_conf);
		if (lookup_mode == L3FWD_LOOKUP_EM)
			l3fwd_lkp.main_loop = evt_rsrc->ops.em_event_loop;
		else if (lookup_mode == L3FWD_LOOKUP_FIB)
			l3fwd_lkp.main_loop = evt_rsrc->ops.fib_event_loop;
		else
			l3fwd_lkp.main_loop = evt_rsrc->ops.lpm_event_loop;
		l3fwd_event_service_setup();
	} else
		l3fwd_poll_resource_setup();

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0) {
			continue;
		}
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on) {
			ret = rte_eth_promiscuous_enable(portid);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_promiscuous_enable: err=%s, port=%u\n",
					rte_strerror(-ret), portid);
		}
	}

	printf("\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;
			if (prepare_ptype_parser(portid, queueid) == 0)
				rte_exit(EXIT_FAILURE, "ptype check fails\n");
		}
	}

	check_all_ports_link_status(enabled_port_mask);

	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l3fwd_lkp.main_loop, NULL, CALL_MAIN);
	if (evt_rsrc->enabled) {
		for (i = 0; i < evt_rsrc->rx_adptr.nb_rx_adptr; i++)
			rte_event_eth_rx_adapter_stop(
					evt_rsrc->rx_adptr.rx_adptr[i]);
		for (i = 0; i < evt_rsrc->tx_adptr.nb_tx_adptr; i++)
			rte_event_eth_tx_adapter_stop(
					evt_rsrc->tx_adptr.tx_adptr[i]);

		RTE_ETH_FOREACH_DEV(portid) {
			if ((enabled_port_mask & (1 << portid)) == 0)
				continue;
			ret = rte_eth_dev_stop(portid);
			if (ret != 0)
				printf("rte_eth_dev_stop: err=%d, port=%u\n",
				       ret, portid);
		}

		rte_eal_mp_wait_lcore();
		RTE_ETH_FOREACH_DEV(portid) {
			if ((enabled_port_mask & (1 << portid)) == 0)
				continue;
			rte_eth_dev_close(portid);
		}

		rte_event_dev_stop(evt_rsrc->event_d_id);
		rte_event_dev_close(evt_rsrc->event_d_id);

	} else {
		rte_eal_mp_wait_lcore();

		RTE_ETH_FOREACH_DEV(portid) {
			if ((enabled_port_mask & (1 << portid)) == 0)
				continue;
			printf("Closing port %d...", portid);
			ret = rte_eth_dev_stop(portid);
			if (ret != 0)
				printf("rte_eth_dev_stop: err=%d, port=%u\n",
				       ret, portid);
			rte_eth_dev_close(portid);
			printf(" Done\n");
		}
	}

	/* clean up config file routes */
	l3fwd_lkp.free_routes();

	/* clean up the EAL */
	rte_eal_cleanup();

	printf("Bye...\n");

	return ret;
}
