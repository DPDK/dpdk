/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#define RTE_GRAPH_MODEL_SELECT RTE_GRAPH_MODEL_RTC
#include <rte_graph_worker.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_lpm6.h>
#include <rte_mempool.h>
#include <rte_node_eth_api.h>
#include <rte_node_ip4_api.h>
#include <rte_node_ip6_api.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>
#include <rte_vect.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

/* Log type */
#define RTE_LOGTYPE_L3FWD_GRAPH RTE_LOGTYPE_USER1

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024

#define MAX_TX_QUEUE_PER_PORT RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT 128

#define MAX_RX_QUEUE_PER_LCORE 16

#define MAX_LCORE_PARAMS 1024

#define NB_SOCKETS 8

/* Graph module */
#define WORKER_MODEL_RTC "rtc"
#define WORKER_MODEL_MCORE_DISPATCH "dispatch"
/* Static global variables used within this file. */
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;

/**< Ports set in promiscuous mode off by default. */
static int promiscuous_on;

static int numa_on = 1;	  /**< NUMA is enabled by default. */
static int per_port_pool; /**< Use separate buffer pools per port; disabled */
			  /**< by default */

static volatile bool force_quit;

/* Ethernet addresses of ports */
static uint64_t dest_eth_addr[RTE_MAX_ETHPORTS];
static struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
xmm_t val_eth[RTE_MAX_ETHPORTS];

/* Mask of enabled ports */
static uint32_t enabled_port_mask;

/* Pcap trace */
static char pcap_filename[RTE_GRAPH_PCAP_FILE_SZ];
static uint64_t packet_to_capture;
static int pcap_trace_enable;


struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
	char node_name[RTE_NODE_NAMESIZE];
};

static uint8_t model_conf = RTE_GRAPH_MODEL_DEFAULT;

/* Lcore conf */
struct lcore_conf {
	uint16_t n_rx_queue;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];

	struct rte_graph *graph;
	char name[RTE_GRAPH_NAMESIZE];
	rte_graph_t graph_id;
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
	{0, 0, 2}, {0, 1, 2}, {0, 2, 2}, {1, 0, 2}, {1, 1, 2},
	{1, 2, 2}, {2, 0, 2}, {3, 0, 3}, {3, 1, 3},
};

static struct lcore_params *lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = RTE_DIM(lcore_params_array_default);

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
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

static uint32_t max_pkt_len;

static struct rte_mempool *pktmbuf_pool[RTE_MAX_ETHPORTS][NB_SOCKETS];

static struct rte_node_ethdev_config ethdev_conf[RTE_MAX_ETHPORTS];

struct ipv4_l3fwd_lpm_route {
	uint32_t ip;
	uint8_t depth;
	uint8_t if_out;
};

struct ipv6_l3fwd_lpm_route {
	uint8_t ip[RTE_LPM6_IPV6_ADDR_SIZE];
	uint8_t depth;
	uint8_t if_out;
};

#define IPV4_L3FWD_LPM_NUM_ROUTES                                              \
	(sizeof(ipv4_l3fwd_lpm_route_array) /                                  \
	 sizeof(ipv4_l3fwd_lpm_route_array[0]))
/* 198.18.0.0/16 are set aside for RFC2544 benchmarking. */
static struct ipv4_l3fwd_lpm_route ipv4_l3fwd_lpm_route_array[] = {
	{RTE_IPV4(198, 18, 0, 0), 24, 0}, {RTE_IPV4(198, 18, 1, 0), 24, 1},
	{RTE_IPV4(198, 18, 2, 0), 24, 2}, {RTE_IPV4(198, 18, 3, 0), 24, 3},
	{RTE_IPV4(198, 18, 4, 0), 24, 4}, {RTE_IPV4(198, 18, 5, 0), 24, 5},
	{RTE_IPV4(198, 18, 6, 0), 24, 6}, {RTE_IPV4(198, 18, 7, 0), 24, 7},
};

#define IPV6_L3FWD_LPM_NUM_ROUTES                                              \
	(sizeof(ipv6_l3fwd_lpm_route_array) /                                  \
	 sizeof(ipv6_l3fwd_lpm_route_array[0]))
static struct ipv6_l3fwd_lpm_route ipv6_l3fwd_lpm_route_array[] = {
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00}, 48, 0},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01}, 48, 1},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02}, 48, 2},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x03}, 48, 3},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04}, 48, 4},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05}, 48, 5},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x06}, 48, 6},
	{{0x20, 0x01, 0xdb, 0x08, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02}, 48, 7},
};

static int
check_worker_model_params(void)
{
	if (model_conf == RTE_GRAPH_MODEL_MCORE_DISPATCH &&
	    nb_lcore_params > 1) {
		printf("Exceeded max number of lcore params for remote model: %hu\n",
		       nb_lcore_params);
		return -1;
	}

	return 0;
}

static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	int socketid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			printf("Invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("Error: lcore %hhu is not enabled in lcore mask\n",
			       lcore);
			return -1;
		}

		if (lcore == rte_get_main_lcore()) {
			printf("Error: lcore %u is main lcore\n", lcore);
			return -1;
		}
		socketid = rte_lcore_to_socket_id(lcore);
		if ((socketid != 0) && (numa_on == 0)) {
			printf("Warning: lcore %hhu is on socket %d with numa off\n",
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
			printf("Port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (!rte_eth_dev_is_valid_port(portid)) {
			printf("Port %u is not present on the board\n", portid);
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
			if (lcore_params[i].queue_id == queue + 1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE,
					 "Queue ids of the port %d must be"
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
			printf("Error: too many queues (%u) for lcore: %u\n",
			       (unsigned int)nb_rx_queue + 1,
			       (unsigned int)lcore);
			return -1;
		}

		lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
			lcore_params[i].port_id;
		lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
			lcore_params[i].queue_id;
		lcore_conf[lcore].n_rx_queue++;
	}

	return 0;
}

/* Display usage */
static void
print_usage(const char *prgname)
{
	fprintf(stderr,
		"%s [EAL options] --"
		" -p PORTMASK"
		" [-P]"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--eth-dest=X,MM:MM:MM:MM:MM:MM]"
		" [--max-pkt-len PKTLEN]"
		" [--no-numa]"
		" [--per-port-pool]"
		" [--num-pkt-cap]\n\n"

		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --eth-dest=X,MM:MM:MM:MM:MM:MM: Ethernet destination for "
		"port X\n"
		"  --max-pkt-len PKTLEN: maximum packet length in decimal (64-9600)\n"
		"  --model NAME: walking model name, dispatch or rtc(by default)\n"
		"  --no-numa: Disable numa awareness\n"
		"  --per-port-pool: Use separate buffer pool per port\n"
		"  --pcap-enable: Enables pcap capture\n"
		"  --pcap-num-cap NUMPKT: Number of packets to capture\n"
		"  --pcap-file-name NAME: Pcap file name\n\n",
		prgname);
}

static uint64_t
parse_num_pkt_cap(const char *num_pkt_cap)
{
	uint64_t num_pkt;
	char *end = NULL;

	/* Parse decimal string */
	num_pkt = strtoull(num_pkt_cap, &end, 10);
	if ((num_pkt_cap[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	if (num_pkt == 0)
		return 0;

	return num_pkt;
}

static int
parse_max_pkt_len(const char *pktlen)
{
	unsigned long len;
	char *end = NULL;

	/* Parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

static void
parse_worker_model(const char *model)
{
	if (strcmp(model, WORKER_MODEL_MCORE_DISPATCH) == 0)
		model_conf = RTE_GRAPH_MODEL_MCORE_DISPATCH;
	else if (strcmp(model, WORKER_MODEL_RTC) == 0)
		model_conf = RTE_GRAPH_MODEL_RTC;
	else
		rte_exit(EXIT_FAILURE, "Invalid worker model: %s", model);

#if defined(RTE_GRAPH_MODEL_SELECT)
	if (model_conf != RTE_GRAPH_MODEL_SELECT)
		printf("Warning: model mismatch, will use the RTE_GRAPH_MODEL_SELECT model\n");
	model_conf = RTE_GRAPH_MODEL_SELECT;
#endif
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* Parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
parse_config(const char *q_arg)
{
	enum fieldnames { FLD_PORT = 0, FLD_QUEUE, FLD_LCORE, _NUM_FLD };
	unsigned long int_fld[_NUM_FLD];
	const char *p, *p0 = q_arg;
	char *str_fld[_NUM_FLD];
	uint32_t size;
	char s[256];
	char *end;
	int i;

	nb_lcore_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		memcpy(s, p, size);
		s[size] = '\0';
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') !=
		    _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i])
				return -1;
		}

		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("Exceeded max number of lcore params: %hu\n",
			       nb_lcore_params);
			return -1;
		}

		if (int_fld[FLD_PORT] >= RTE_MAX_ETHPORTS ||
		    int_fld[FLD_LCORE] >= RTE_MAX_LCORE) {
			printf("Invalid port/lcore id\n");
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
	uint8_t c, *dest, peer_addr[6];
	uint16_t portid;
	char *port_end;

	errno = 0;
	portid = strtoul(optarg, &port_end, 10);
	if (errno != 0 || port_end == optarg || *port_end++ != ',')
		rte_exit(EXIT_FAILURE, "Invalid eth-dest: %s", optarg);
	if (portid >= RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE,
			 "eth-dest: port %d >= RTE_MAX_ETHPORTS(%d)\n", portid,
			 RTE_MAX_ETHPORTS);

	if (cmdline_parse_etheraddr(NULL, port_end, &peer_addr,
				    sizeof(peer_addr)) < 0)
		rte_exit(EXIT_FAILURE, "Invalid ethernet address: %s\n",
			 port_end);
	dest = (uint8_t *)&dest_eth_addr[portid];
	for (c = 0; c < 6; c++)
		dest[c] = peer_addr[c];
	*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
}

#define MAX_JUMBO_PKT_LEN  9600
#define MEMPOOL_CACHE_SIZE 256

static const char short_options[] = "p:" /* portmask */
				    "P"	 /* promiscuous */
	;

#define CMD_LINE_OPT_CONFIG	   "config"
#define CMD_LINE_OPT_ETH_DEST	   "eth-dest"
#define CMD_LINE_OPT_NO_NUMA	   "no-numa"
#define CMD_LINE_OPT_MAX_PKT_LEN   "max-pkt-len"
#define CMD_LINE_OPT_PER_PORT_POOL "per-port-pool"
#define CMD_LINE_OPT_PCAP_ENABLE   "pcap-enable"
#define CMD_LINE_OPT_NUM_PKT_CAP   "pcap-num-cap"
#define CMD_LINE_OPT_PCAP_FILENAME "pcap-file-name"
#define CMD_LINE_OPT_WORKER_MODEL  "model"

enum {
	/* Long options mapped to a short option */

	/* First long only option value must be >= 256, so that we won't
	 * conflict with short options
	 */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_ETH_DEST_NUM,
	CMD_LINE_OPT_NO_NUMA_NUM,
	CMD_LINE_OPT_MAX_PKT_LEN_NUM,
	CMD_LINE_OPT_PARSE_PER_PORT_POOL,
	CMD_LINE_OPT_PARSE_PCAP_ENABLE,
	CMD_LINE_OPT_PARSE_NUM_PKT_CAP,
	CMD_LINE_OPT_PCAP_FILENAME_CAP,
	CMD_LINE_OPT_WORKER_MODEL_TYPE,
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_ETH_DEST, 1, 0, CMD_LINE_OPT_ETH_DEST_NUM},
	{CMD_LINE_OPT_NO_NUMA, 0, 0, CMD_LINE_OPT_NO_NUMA_NUM},
	{CMD_LINE_OPT_MAX_PKT_LEN, 1, 0, CMD_LINE_OPT_MAX_PKT_LEN_NUM},
	{CMD_LINE_OPT_PER_PORT_POOL, 0, 0, CMD_LINE_OPT_PARSE_PER_PORT_POOL},
	{CMD_LINE_OPT_PCAP_ENABLE, 0, 0, CMD_LINE_OPT_PARSE_PCAP_ENABLE},
	{CMD_LINE_OPT_NUM_PKT_CAP, 1, 0, CMD_LINE_OPT_PARSE_NUM_PKT_CAP},
	{CMD_LINE_OPT_PCAP_FILENAME, 1, 0, CMD_LINE_OPT_PCAP_FILENAME_CAP},
	{CMD_LINE_OPT_WORKER_MODEL, 1, 0, CMD_LINE_OPT_WORKER_MODEL_TYPE},
	{NULL, 0, 0, 0},
};

/*
 * This expression is used to calculate the number of mbufs needed
 * depending on user input, taking  into account memory for rx and
 * tx hardware rings, cache per lcore and mtable per port per lcore.
 * RTE_MAX is used to ensure that NB_MBUF never goes below a minimum
 * value of 8192
 */
#define NB_MBUF(nports)                                                        \
	RTE_MAX((nports * nb_rx_queue * nb_rxd +                               \
		 nports * nb_lcores * RTE_GRAPH_BURST_SIZE +                   \
		 nports * n_tx_queue * nb_txd +                                \
		 nb_lcores * MEMPOOL_CACHE_SIZE), 8192u)

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	char *prgname = argv[0];
	int option_index;
	char **argvopt;
	int opt, ret;

	argvopt = argv;

	/* Error or normal output strings. */
	while ((opt = getopt_long(argc, argvopt, short_options, lgopts,
				  &option_index)) != EOF) {

		switch (opt) {
		/* Portmask */
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

		/* Long options */
		case CMD_LINE_OPT_CONFIG_NUM:
			ret = parse_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid config\n");
				print_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_ETH_DEST_NUM:
			parse_eth_dest(optarg);
			break;

		case CMD_LINE_OPT_NO_NUMA_NUM:
			numa_on = 0;
			break;

		case CMD_LINE_OPT_MAX_PKT_LEN_NUM: {
			max_pkt_len = parse_max_pkt_len(optarg);
			break;
		}

		case CMD_LINE_OPT_PARSE_PER_PORT_POOL:
			printf("Per port buffer pool is enabled\n");
			per_port_pool = 1;
			break;

		case CMD_LINE_OPT_PARSE_PCAP_ENABLE:
			printf("Packet capture enabled\n");
			pcap_trace_enable = 1;
			break;

		case CMD_LINE_OPT_PARSE_NUM_PKT_CAP:
			packet_to_capture = parse_num_pkt_cap(optarg);
			printf("Number of packets to capture: %"PRIu64"\n",
			       packet_to_capture);
			break;

		case CMD_LINE_OPT_PCAP_FILENAME_CAP:
			rte_strlcpy(pcap_filename, optarg,
				    sizeof(pcap_filename));
			printf("Pcap file name: %s\n", pcap_filename);
			break;

		case CMD_LINE_OPT_WORKER_MODEL_TYPE:
			printf("Use new worker model: %s\n", optarg);
			parse_worker_model(optarg);
			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind - 1] = prgname;
	ret = optind - 1;
	optind = 1; /* Reset getopt lib */

	return ret;
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static int
init_mem(uint16_t portid, uint32_t nb_mbuf)
{
	uint32_t lcore_id;
	int socketid;
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
			snprintf(s, sizeof(s), "mbuf_pool_%d:%d", portid,
				 socketid);
			/* Create a pool with priv size of a cacheline */
			pktmbuf_pool[portid][socketid] =
				rte_pktmbuf_pool_create(
					s, nb_mbuf, MEMPOOL_CACHE_SIZE,
					RTE_CACHE_LINE_SIZE,
					RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
			if (pktmbuf_pool[portid][socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					 "Cannot init mbuf pool on socket %d\n",
					 socketid);
			else
				printf("Allocated mbuf pool on socket %d\n",
				       socketid);
		}
	}

	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	uint16_t portid;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid)
		{
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
			/* Print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* Clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* After finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* Set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("Done\n");
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

static void
print_stats(void)
{
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
	const char clr[] = {27, '[', '2', 'J', '\0'};
	struct rte_graph_cluster_stats_param s_param;
	struct rte_graph_cluster_stats *stats;
	const char *pattern = "worker_*";

	/* Prepare stats object */
	memset(&s_param, 0, sizeof(s_param));
	s_param.f = stdout;
	s_param.socket_id = SOCKET_ID_ANY;
	s_param.graph_patterns = &pattern;
	s_param.nb_graph_patterns = 1;

	stats = rte_graph_cluster_stats_create(&s_param);
	if (stats == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create stats object\n");

	while (!force_quit) {
		/* Clear screen and move to top left */
		printf("%s%s", clr, topLeft);
		rte_graph_cluster_stats_get(stats, 0);
		rte_delay_ms(1E3);
	}

	rte_graph_cluster_stats_destroy(stats);
}

/* Main processing loop. 8< */
static int
graph_main_loop(void *conf)
{
	struct lcore_conf *qconf;
	struct rte_graph *graph;
	uint32_t lcore_id;

	RTE_SET_USED(conf);

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];
	graph = qconf->graph;

	if (!graph) {
		RTE_LOG(INFO, L3FWD_GRAPH, "Lcore %u has nothing to do\n",
			lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD_GRAPH,
		"Entering main loop on lcore %u, graph %s(%p)\n", lcore_id,
		qconf->name, graph);

	while (likely(!force_quit))
		rte_graph_walk(graph);

	return 0;
}
/* >8 End of main processing loop. */

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

static int
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
graph_config_mcore_dispatch(struct rte_graph_param graph_conf)
{
	uint16_t nb_patterns = graph_conf.nb_node_patterns;
	int worker_count = rte_lcore_count() - 1;
	int main_lcore_id = rte_get_main_lcore();
	rte_graph_t main_graph_id = 0;
	struct rte_node *node_tmp;
	struct lcore_conf *qconf;
	struct rte_graph *graph;
	rte_graph_t graph_id;
	rte_graph_off_t off;
	int n_rx_node = 0;
	int worker_lcore;
	rte_node_t count;
	int i, j;
	int ret;

	for (j = 0; j < nb_lcore_params; j++) {
		qconf = &lcore_conf[lcore_params[j].lcore_id];
		/* Add rx node patterns of all lcore */
		for (i = 0; i < qconf->n_rx_queue; i++) {
			char *node_name = qconf->rx_queue_list[i].node_name;
			unsigned int lcore_id = lcore_params[j].lcore_id;

			graph_conf.node_patterns[nb_patterns + n_rx_node + i] = node_name;
			n_rx_node++;
			ret = rte_graph_model_mcore_dispatch_node_lcore_affinity_set(node_name,
										     lcore_id);
			if (ret == 0)
				printf("Set node %s affinity to lcore %u\n", node_name,
				       lcore_params[j].lcore_id);
		}
	}

	graph_conf.nb_node_patterns = nb_patterns + n_rx_node;
	graph_conf.socket_id = rte_lcore_to_socket_id(main_lcore_id);

	qconf = &lcore_conf[main_lcore_id];
	snprintf(qconf->name, sizeof(qconf->name), "worker_%u",
		 main_lcore_id);

	/* create main graph */
	main_graph_id = rte_graph_create(qconf->name, &graph_conf);
	if (main_graph_id == RTE_GRAPH_ID_INVALID)
		rte_exit(EXIT_FAILURE,
			 "rte_graph_create(): main_graph_id invalid for lcore %u\n",
			 main_lcore_id);

	/* set the graph model for the main graph */
	rte_graph_worker_model_set(RTE_GRAPH_MODEL_MCORE_DISPATCH);
	qconf->graph_id = main_graph_id;
	qconf->graph = rte_graph_lookup(qconf->name);
	if (!qconf->graph)
		rte_exit(EXIT_FAILURE,
			 "rte_graph_lookup(): graph %s not found\n",
			 qconf->name);

	graph = qconf->graph;
	worker_lcore = lcore_params[nb_lcore_params - 1].lcore_id;
	rte_graph_foreach_node(count, off, graph, node_tmp) {
		/* Need to set the node Lcore affinity before clone graph for each lcore */
		if (node_tmp->dispatch.lcore_id == RTE_MAX_LCORE) {
			worker_lcore = rte_get_next_lcore(worker_lcore, true, 1);
			ret = rte_graph_model_mcore_dispatch_node_lcore_affinity_set(node_tmp->name,
										     worker_lcore);
			if (ret == 0)
				printf("Set node %s affinity to lcore %u\n",
				       node_tmp->name, worker_lcore);
		}
	}

	worker_lcore = main_lcore_id;
	for (i = 0; i < worker_count; i++) {
		worker_lcore = rte_get_next_lcore(worker_lcore, true, 1);

		qconf = &lcore_conf[worker_lcore];
		snprintf(qconf->name, sizeof(qconf->name), "cloned-%u", worker_lcore);
		graph_id = rte_graph_clone(main_graph_id, qconf->name, &graph_conf);
		ret = rte_graph_model_mcore_dispatch_core_bind(graph_id, worker_lcore);
		if (ret == 0)
			printf("bind graph %d to lcore %u\n", graph_id, worker_lcore);

		/* full cloned graph name */
		snprintf(qconf->name, sizeof(qconf->name), "%s",
			 rte_graph_id_to_name(graph_id));
		qconf->graph_id = graph_id;
		qconf->graph = rte_graph_lookup(qconf->name);
		if (!qconf->graph)
			rte_exit(EXIT_FAILURE,
				 "Failed to lookup graph %s\n",
				 qconf->name);
		continue;
	}
}

static void
graph_config_rtc(struct rte_graph_param graph_conf)
{
	uint16_t nb_patterns = graph_conf.nb_node_patterns;
	struct lcore_conf *qconf;
	rte_graph_t graph_id;
	uint32_t lcore_id;
	rte_edge_t i;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		qconf = &lcore_conf[lcore_id];
		/* Skip graph creation if no source exists */
		if (!qconf->n_rx_queue)
			continue;
		/* Add rx node patterns of this lcore */
		for (i = 0; i < qconf->n_rx_queue; i++) {
			graph_conf.node_patterns[nb_patterns + i] =
				qconf->rx_queue_list[i].node_name;
		}
		graph_conf.nb_node_patterns = nb_patterns + i;
		graph_conf.socket_id = rte_lcore_to_socket_id(lcore_id);
		snprintf(qconf->name, sizeof(qconf->name), "worker_%u",
			 lcore_id);
		graph_id = rte_graph_create(qconf->name, &graph_conf);
		if (graph_id == RTE_GRAPH_ID_INVALID)
			rte_exit(EXIT_FAILURE,
				 "rte_graph_create(): graph_id invalid for lcore %u\n",
				 lcore_id);
		qconf->graph_id = graph_id;
		qconf->graph = rte_graph_lookup(qconf->name);
		if (!qconf->graph)
			rte_exit(EXIT_FAILURE,
				 "rte_graph_lookup(): graph %s not found\n",
				 qconf->name);
	}
}

int
main(int argc, char **argv)
{
	/* Rewrite data of src and dst ether addr */
	uint8_t rewrite_data[2 * sizeof(struct rte_ether_addr)];
	/* Graph initialization. 8< */
	static const char * const default_patterns[] = {
		"ip4*",
		"ethdev_tx-*",
		"pkt_drop",
	};
	uint8_t nb_rx_queue, queue, socketid;
	struct rte_graph_param graph_conf;
	struct rte_eth_dev_info dev_info;
	uint32_t nb_ports, nb_conf = 0;
	uint32_t n_tx_queue, nb_lcores;
	struct rte_eth_txconf *txconf;
	uint16_t queueid, portid, i;
	const char **node_patterns;
	struct lcore_conf *qconf;
	uint16_t nb_graphs = 0;
	uint16_t nb_patterns;
	uint8_t rewrite_len;
	uint32_t lcore_id;
	int ret;

	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Pre-init dst MACs for all ports to 02:00:00:00:00:xx */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		dest_eth_addr[portid] =
			RTE_ETHER_LOCAL_ADMIN_ADDR + ((uint64_t)portid << 40);
		*(uint64_t *)(val_eth + portid) = dest_eth_addr[portid];
	}

	/* Parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD_GRAPH parameters\n");

	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params() failed\n");

	if (check_worker_model_params() < 0)
		rte_exit(EXIT_FAILURE, "check_worker_model_params() failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues() failed\n");

	if (check_port_config() < 0)
		rte_exit(EXIT_FAILURE, "check_port_config() failed\n");

	nb_ports = rte_eth_dev_count_avail();
	nb_lcores = rte_lcore_count();

	/* Initialize all ports. 8< */
	RTE_ETH_FOREACH_DEV(portid)
	{
		struct rte_eth_conf local_port_conf = port_conf;

		/* Skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* Init port */
		printf("Initializing port %d ... ", portid);
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
		       nb_rx_queue, n_tx_queue);

		rte_eth_dev_info_get(portid, &dev_info);

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
		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
		    port_conf.rx_adv_conf.rss_conf.rss_hf) {
			printf("Port %u modified RSS hash function based on "
			       "hardware support,"
			       "requested:%#" PRIx64 " configured:%#" PRIx64
			       "\n",
			       portid, port_conf.rx_adv_conf.rss_conf.rss_hf,
			       local_port_conf.rx_adv_conf.rss_conf.rss_hf);
		}

		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					    n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot configure device: err=%d, port=%d\n",
				 ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, "
				 "port=%d\n",
				 ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");
		print_ethaddr(
			"Destination:",
			(const struct rte_ether_addr *)&dest_eth_addr[portid]);
		printf(", ");

		/*
		 * prepare src MACs for each port.
		 */
		rte_ether_addr_copy(
			&ports_eth_addr[portid],
			(struct rte_ether_addr *)(val_eth + portid) + 1);

		/* Init memory */
		if (!per_port_pool) {
			/* portid = 0; this is *not* signifying the first port,
			 * rather, it signifies that portid is ignored.
			 */
			ret = init_mem(0, NB_MBUF(nb_ports));
		} else {
			ret = init_mem(portid, NB_MBUF(1));
		}
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem() failed\n");

		/* Init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			qconf = &lcore_conf[lcore_id];

			if (numa_on)
				socketid = (uint8_t)rte_lcore_to_socket_id(
					lcore_id);
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
					 "port=%d\n",
					 ret, portid);
			queueid++;
		}

		/* Setup ethdev node config */
		ethdev_conf[nb_conf].port_id = portid;
		ethdev_conf[nb_conf].num_rx_queues = nb_rx_queue;
		ethdev_conf[nb_conf].num_tx_queues = n_tx_queue;
		if (!per_port_pool)
			ethdev_conf[nb_conf].mp = pktmbuf_pool[0];

		else
			ethdev_conf[nb_conf].mp = pktmbuf_pool[portid];
		ethdev_conf[nb_conf].mp_count = NB_SOCKETS;

		nb_conf++;
		printf("\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id);
		fflush(stdout);
		/* Init RX queues */
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			struct rte_eth_rxconf rxq_conf;

			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			if (numa_on)
				socketid = (uint8_t)rte_lcore_to_socket_id(
					lcore_id);
			else
				socketid = 0;

			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = port_conf.rxmode.offloads;
			if (!per_port_pool)
				ret = rte_eth_rx_queue_setup(
					portid, queueid, nb_rxd, socketid,
					&rxq_conf, pktmbuf_pool[0][socketid]);
			else
				ret = rte_eth_rx_queue_setup(
					portid, queueid, nb_rxd, socketid,
					&rxq_conf,
					pktmbuf_pool[portid][socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					 "rte_eth_rx_queue_setup: err=%d, "
					 "port=%d\n",
					 ret, portid);

			/* Add this queue node to its graph */
			snprintf(qconf->rx_queue_list[queue].node_name,
				 RTE_NODE_NAMESIZE, "ethdev_rx-%u-%u", portid,
				 queueid);
		}

		/* Alloc a graph to this lcore only if source exists  */
		if (qconf->n_rx_queue)
			nb_graphs++;
	}

	printf("\n");

	/* Ethdev node config, skip rx queue mapping */
	ret = rte_node_eth_config(ethdev_conf, nb_conf, nb_graphs);
	/* >8 End of graph creation. */
	if (ret)
		rte_exit(EXIT_FAILURE, "rte_node_eth_config: err=%d\n", ret);

	/* Start ports */
	RTE_ETH_FOREACH_DEV(portid)
	{
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_dev_start: err=%d, port=%d\n", ret,
				 portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	printf("\n");

	check_all_ports_link_status(enabled_port_mask);

	/* Graph Initialization */
	nb_patterns = RTE_DIM(default_patterns);
	node_patterns = malloc((MAX_RX_QUEUE_PER_LCORE + nb_patterns) *
			       sizeof(*node_patterns));
	if (!node_patterns)
		return -ENOMEM;
	memcpy(node_patterns, default_patterns,
	       nb_patterns * sizeof(*node_patterns));

	memset(&graph_conf, 0, sizeof(graph_conf));
	graph_conf.node_patterns = node_patterns;
	graph_conf.nb_node_patterns = nb_patterns;

	/* Pcap config */
	graph_conf.pcap_enable = pcap_trace_enable;
	graph_conf.num_pkt_to_capture = packet_to_capture;
	graph_conf.pcap_filename = pcap_filename;

	if (model_conf == RTE_GRAPH_MODEL_MCORE_DISPATCH)
		graph_config_mcore_dispatch(graph_conf);
	else
		graph_config_rtc(graph_conf);

	rte_graph_worker_model_set(model_conf);
	/* >8 End of graph initialization. */

	memset(&rewrite_data, 0, sizeof(rewrite_data));
	rewrite_len = sizeof(rewrite_data);

	/* Add routes and rewrite data to graph infra. 8< */
	for (i = 0; i < IPV4_L3FWD_LPM_NUM_ROUTES; i++) {
		char route_str[INET6_ADDRSTRLEN * 4];
		char abuf[INET6_ADDRSTRLEN];
		struct in_addr in;
		uint32_t dst_port;

		/* Skip unused ports */
		if ((1 << ipv4_l3fwd_lpm_route_array[i].if_out &
		     enabled_port_mask) == 0)
			continue;

		dst_port = ipv4_l3fwd_lpm_route_array[i].if_out;

		in.s_addr = htonl(ipv4_l3fwd_lpm_route_array[i].ip);
		snprintf(route_str, sizeof(route_str), "%s / %d (%d)",
			 inet_ntop(AF_INET, &in, abuf, sizeof(abuf)),
			 ipv4_l3fwd_lpm_route_array[i].depth,
			 ipv4_l3fwd_lpm_route_array[i].if_out);

		/* Use route index 'i' as next hop id */
		ret = rte_node_ip4_route_add(
			ipv4_l3fwd_lpm_route_array[i].ip,
			ipv4_l3fwd_lpm_route_array[i].depth, i,
			RTE_NODE_IP4_LOOKUP_NEXT_REWRITE);

		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Unable to add ip4 route %s to graph\n",
				 route_str);

		memcpy(rewrite_data, val_eth + dst_port, rewrite_len);

		/* Add next hop rewrite data for id 'i' */
		ret = rte_node_ip4_rewrite_add(i, rewrite_data,
					       rewrite_len, dst_port);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Unable to add next hop %u for "
				 "route %s\n", i, route_str);

		RTE_LOG(INFO, L3FWD_GRAPH, "Added route %s, next_hop %u\n",
			route_str, i);
	}

	for (i = 0; i < IPV6_L3FWD_LPM_NUM_ROUTES; i++) {
		char route_str[INET6_ADDRSTRLEN * 4];
		char abuf[INET6_ADDRSTRLEN];
		struct in6_addr in6;
		uint32_t dst_port;

		/* Skip unused ports */
		if ((1 << ipv6_l3fwd_lpm_route_array[i].if_out &
		     enabled_port_mask) == 0)
			continue;

		dst_port = ipv6_l3fwd_lpm_route_array[i].if_out;

		memcpy(in6.s6_addr, ipv6_l3fwd_lpm_route_array[i].ip, RTE_LPM6_IPV6_ADDR_SIZE);
		snprintf(route_str, sizeof(route_str), "%s / %d (%d)",
			 inet_ntop(AF_INET6, &in6, abuf, sizeof(abuf)),
			 ipv6_l3fwd_lpm_route_array[i].depth,
			 ipv6_l3fwd_lpm_route_array[i].if_out);

		/* Use route index 'i' as next hop id */
		ret = rte_node_ip6_route_add(ipv6_l3fwd_lpm_route_array[i].ip,
			ipv6_l3fwd_lpm_route_array[i].depth, i,
			RTE_NODE_IP6_LOOKUP_NEXT_REWRITE);

		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Unable to add ip6 route %s to graph\n",
				 route_str);

		memcpy(rewrite_data, val_eth + dst_port, rewrite_len);

		/* Add next hop rewrite data for id 'i' */
		ret = rte_node_ip6_rewrite_add(i, rewrite_data,
					       rewrite_len, dst_port);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Unable to add next hop %u for "
				 "route %s\n", i, route_str);

		RTE_LOG(INFO, L3FWD_GRAPH, "Added route %s, next_hop %u\n",
			route_str, i);
	}
	/* >8 End of adding routes and rewrite data to graph infa. */

	/* Launch per-lcore init on every worker lcore */
	rte_eal_mp_remote_launch(graph_main_loop, NULL, SKIP_MAIN);

	/* Accumulate and print stats on main until exit */
	if (rte_graph_has_stats_feature())
		print_stats();

	/* Wait for worker cores to exit */
	ret = 0;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		ret = rte_eal_wait_lcore(lcore_id);
		/* Destroy graph */
		if (ret < 0 || rte_graph_destroy(
			rte_graph_from_name(lcore_conf[lcore_id].name))) {
			ret = -1;
			break;
		}
	}
	free(node_patterns);

	/* Stop ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("Failed to stop port %u: %s\n",
			       portid, rte_strerror(-ret));
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	/* clean up the EAL */
	rte_eal_cleanup();
	printf("Bye...\n");

	return ret;
}
