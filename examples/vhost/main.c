/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#include <arpa/inet.h>
#include <getopt.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/virtio_net.h>
#include <linux/virtio_ring.h>
#include <signal.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/param.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_vhost.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_pause.h>

#include "main.h"

#ifndef MAX_QUEUES
#define MAX_QUEUES 128
#endif

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

#define MBUF_CACHE_SIZE	128
#define MBUF_DATA_SIZE	RTE_MBUF_DEFAULT_BUF_SIZE

#define BURST_TX_DRAIN_US 100	/* TX drain every ~100us */

#define BURST_RX_WAIT_US 15	/* Defines how long we wait between retries on RX */
#define BURST_RX_RETRIES 4		/* Number of retries on RX. */

#define JUMBO_FRAME_MAX_SIZE    0x2600

/* State of virtio device. */
#define DEVICE_MAC_LEARNING 0
#define DEVICE_RX			1
#define DEVICE_SAFE_REMOVE	2

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 512

#define INVALID_PORT_ID 0xFF

/* Max number of devices. Limited by vmdq. */
#define MAX_DEVICES 64

/* Size of buffers used for snprintfs. */
#define MAX_PRINT_BUFF 6072

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

/* Promiscuous mode */
static uint32_t promiscuous;

/* number of devices/queues to support*/
static uint32_t num_queues = 0;
static uint32_t num_devices;

static struct rte_mempool *mbuf_pool;
static int mergeable;

/* Enable VM2VM communications. If this is disabled then the MAC address compare is skipped. */
typedef enum {
	VM2VM_DISABLED = 0,
	VM2VM_SOFTWARE = 1,
	VM2VM_HARDWARE = 2,
	VM2VM_LAST
} vm2vm_type;
static vm2vm_type vm2vm_mode = VM2VM_SOFTWARE;

/* Enable stats. */
static uint32_t enable_stats = 0;
/* Enable retries on RX. */
static uint32_t enable_retry = 1;

/* Disable TX checksum offload */
static uint32_t enable_tx_csum;

/* Disable TSO offload */
static uint32_t enable_tso;

static int client_mode;
static int dequeue_zero_copy;

static int builtin_net_driver;

/* Specify timeout (in useconds) between retries on RX. */
static uint32_t burst_rx_delay_time = BURST_RX_WAIT_US;
/* Specify the number of retries on RX. */
static uint32_t burst_rx_retry_num = BURST_RX_RETRIES;

/* Socket file paths. Can be set by user */
static char *socket_files;
static int nb_sockets;

/* empty vmdq configuration structure. Filled in programatically */
static struct rte_eth_conf vmdq_conf_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_ONLY,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		/*
		 * It is necessary for 1G NIC such as I350,
		 * this fixes bug of ipv4 forwarding in guest can't
		 * forward pakets from one virtio dev to another virtio dev.
		 */
		.hw_vlan_strip  = 1, /**< VLAN strip enabled. */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
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
static uint8_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports = 0; /**< The number of ports specified in command line */
static uint16_t num_pf_queues, num_vmdq_queues;
static uint16_t vmdq_pool_base, vmdq_queue_base;
static uint16_t queues_per_pool;

const uint16_t vlan_tags[] = {
	1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007,
	1008, 1009, 1010, 1011,	1012, 1013, 1014, 1015,
	1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023,
	1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031,
	1032, 1033, 1034, 1035, 1036, 1037, 1038, 1039,
	1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047,
	1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055,
	1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063,
};

/* ethernet addresses of ports */
static struct ether_addr vmdq_ports_eth_addr[RTE_MAX_ETHPORTS];

static struct vhost_dev_tailq_list vhost_dev_list =
	TAILQ_HEAD_INITIALIZER(vhost_dev_list);

static struct lcore_info lcore_info[RTE_MAX_LCORE];

/* Used for queueing bursts of TX packets. */
struct mbuf_table {
	unsigned len;
	unsigned txq_id;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

/* TX queue for each data core. */
struct mbuf_table lcore_tx_queue[RTE_MAX_LCORE];

#define MBUF_TABLE_DRAIN_TSC	((rte_get_tsc_hz() + US_PER_S - 1) \
				 / US_PER_S * BURST_TX_DRAIN_US)
#define VLAN_HLEN       4

/*
 * Builds up the correct configuration for VMDQ VLAN pool map
 * according to the pool & queue limits.
 */
static inline int
get_eth_conf(struct rte_eth_conf *eth_conf, uint32_t num_devices)
{
	struct rte_eth_vmdq_rx_conf conf;
	struct rte_eth_vmdq_rx_conf *def_conf =
		&vmdq_conf_default.rx_adv_conf.vmdq_rx_conf;
	unsigned i;

	memset(&conf, 0, sizeof(conf));
	conf.nb_queue_pools = (enum rte_eth_nb_pools)num_devices;
	conf.nb_pool_maps = num_devices;
	conf.enable_loop_back = def_conf->enable_loop_back;
	conf.rx_mode = def_conf->rx_mode;

	for (i = 0; i < conf.nb_pool_maps; i++) {
		conf.pool_map[i].vlan_id = vlan_tags[ i ];
		conf.pool_map[i].pools = (1UL << i);
	}

	(void)(rte_memcpy(eth_conf, &vmdq_conf_default, sizeof(*eth_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &conf,
		   sizeof(eth_conf->rx_adv_conf.vmdq_rx_conf)));
	return 0;
}

/*
 * Validate the device number according to the max pool number gotten form
 * dev_info. If the device number is invalid, give the error message and
 * return -1. Each device must have its own pool.
 */
static inline int
validate_num_devices(uint32_t max_nb_devices)
{
	if (num_devices > max_nb_devices) {
		RTE_LOG(ERR, VHOST_PORT, "invalid number of devices\n");
		return -1;
	}
	return 0;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint8_t port)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_txconf *txconf;
	int16_t rx_rings, tx_rings;
	uint16_t rx_ring_size, tx_ring_size;
	int retval;
	uint16_t q;

	/* The max pool number from dev_info will be used to validate the pool number specified in cmd line */
	rte_eth_dev_info_get (port, &dev_info);

	if (dev_info.max_rx_queues > MAX_QUEUES) {
		rte_exit(EXIT_FAILURE,
			"please define MAX_QUEUES no less than %u in %s\n",
			dev_info.max_rx_queues, __FILE__);
	}

	rxconf = &dev_info.default_rxconf;
	txconf = &dev_info.default_txconf;
	rxconf->rx_drop_en = 1;

	/* Enable vlan offload */
	txconf->txq_flags &= ~ETH_TXQ_FLAGS_NOVLANOFFL;

	/*configure the number of supported virtio devices based on VMDQ limits */
	num_devices = dev_info.max_vmdq_pools;

	rx_ring_size = RTE_TEST_RX_DESC_DEFAULT;
	tx_ring_size = RTE_TEST_TX_DESC_DEFAULT;

	/*
	 * When dequeue zero copy is enabled, guest Tx used vring will be
	 * updated only when corresponding mbuf is freed. Thus, the nb_tx_desc
	 * (tx_ring_size here) must be small enough so that the driver will
	 * hit the free threshold easily and free mbufs timely. Otherwise,
	 * guest Tx vring would be starved.
	 */
	if (dequeue_zero_copy)
		tx_ring_size = 64;

	tx_rings = (uint16_t)rte_lcore_count();

	retval = validate_num_devices(MAX_DEVICES);
	if (retval < 0)
		return retval;

	/* Get port configuration. */
	retval = get_eth_conf(&port_conf, num_devices);
	if (retval < 0)
		return retval;
	/* NIC queues are divided into pf queues and vmdq queues.  */
	num_pf_queues = dev_info.max_rx_queues - dev_info.vmdq_queue_num;
	queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools;
	num_vmdq_queues = num_devices * queues_per_pool;
	num_queues = num_pf_queues + num_vmdq_queues;
	vmdq_queue_base = dev_info.vmdq_queue_base;
	vmdq_pool_base  = dev_info.vmdq_pool_base;
	printf("pf queue num: %u, configured vmdq pool num: %u, each vmdq pool has %u queues\n",
		num_pf_queues, num_devices, queues_per_pool);

	if (port >= rte_eth_dev_count()) return -1;

	rx_rings = (uint16_t)dev_info.max_rx_queues;
	/* Configure ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) {
		RTE_LOG(ERR, VHOST_PORT, "Failed to configure port %u: %s.\n",
			port, strerror(-retval));
		return retval;
	}

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &rx_ring_size,
		&tx_ring_size);
	if (retval != 0) {
		RTE_LOG(ERR, VHOST_PORT, "Failed to adjust number of descriptors "
			"for port %u: %s.\n", port, strerror(-retval));
		return retval;
	}
	if (rx_ring_size > RTE_TEST_RX_DESC_DEFAULT) {
		RTE_LOG(ERR, VHOST_PORT, "Mbuf pool has an insufficient size "
			"for Rx queues on port %u.\n", port);
		return -1;
	}

	/* Setup the queues. */
	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
						rte_eth_dev_socket_id(port),
						rxconf,
						mbuf_pool);
		if (retval < 0) {
			RTE_LOG(ERR, VHOST_PORT,
				"Failed to setup rx queue %u of port %u: %s.\n",
				q, port, strerror(-retval));
			return retval;
		}
	}
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
						rte_eth_dev_socket_id(port),
						txconf);
		if (retval < 0) {
			RTE_LOG(ERR, VHOST_PORT,
				"Failed to setup tx queue %u of port %u: %s.\n",
				q, port, strerror(-retval));
			return retval;
		}
	}

	/* Start the device. */
	retval  = rte_eth_dev_start(port);
	if (retval < 0) {
		RTE_LOG(ERR, VHOST_PORT, "Failed to start port %u: %s\n",
			port, strerror(-retval));
		return retval;
	}

	if (promiscuous)
		rte_eth_promiscuous_enable(port);

	rte_eth_macaddr_get(port, &vmdq_ports_eth_addr[port]);
	RTE_LOG(INFO, VHOST_PORT, "Max virtio devices supported: %u\n", num_devices);
	RTE_LOG(INFO, VHOST_PORT, "Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			vmdq_ports_eth_addr[port].addr_bytes[0],
			vmdq_ports_eth_addr[port].addr_bytes[1],
			vmdq_ports_eth_addr[port].addr_bytes[2],
			vmdq_ports_eth_addr[port].addr_bytes[3],
			vmdq_ports_eth_addr[port].addr_bytes[4],
			vmdq_ports_eth_addr[port].addr_bytes[5]);

	return 0;
}

/*
 * Set socket file path.
 */
static int
us_vhost_parse_socket_path(const char *q_arg)
{
	/* parse number string */
	if (strnlen(q_arg, PATH_MAX) == PATH_MAX)
		return -1;

	socket_files = realloc(socket_files, PATH_MAX * (nb_sockets + 1));
	snprintf(socket_files + nb_sockets * PATH_MAX, PATH_MAX, "%s", q_arg);
	nb_sockets++;

	return 0;
}

/*
 * Parse the portmask provided at run time.
 */
static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	errno = 0;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0') || (errno != 0))
		return -1;

	if (pm == 0)
		return -1;

	return pm;

}

/*
 * Parse num options at run time.
 */
static int
parse_num_opt(const char *q_arg, uint32_t max_valid_value)
{
	char *end = NULL;
	unsigned long num;

	errno = 0;

	/* parse unsigned int string */
	num = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0') || (errno != 0))
		return -1;

	if (num > max_valid_value)
		return -1;

	return num;

}

/*
 * Display usage
 */
static void
us_vhost_usage(const char *prgname)
{
	RTE_LOG(INFO, VHOST_CONFIG, "%s [EAL options] -- -p PORTMASK\n"
	"		--vm2vm [0|1|2]\n"
	"		--rx_retry [0|1] --mergeable [0|1] --stats [0-N]\n"
	"		--socket-file <path>\n"
	"		--nb-devices ND\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--vm2vm [0|1|2]: disable/software(default)/hardware vm2vm comms\n"
	"		--rx-retry [0|1]: disable/enable(default) retries on rx. Enable retry if destintation queue is full\n"
	"		--rx-retry-delay [0-N]: timeout(in usecond) between retries on RX. This makes effect only if retries on rx enabled\n"
	"		--rx-retry-num [0-N]: the number of retries on rx. This makes effect only if retries on rx enabled\n"
	"		--mergeable [0|1]: disable(default)/enable RX mergeable buffers\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n"
	"		--socket-file: The path of the socket file.\n"
	"		--tx-csum [0|1] disable/enable TX checksum offload.\n"
	"		--tso [0|1] disable/enable TCP segment offload.\n"
	"		--client register a vhost-user socket as client mode.\n"
	"		--dequeue-zero-copy enables dequeue zero copy\n",
	       prgname);
}

/*
 * Parse the arguments given in the command line of the application.
 */
static int
us_vhost_parse_args(int argc, char **argv)
{
	int opt, ret;
	int option_index;
	unsigned i;
	const char *prgname = argv[0];
	static struct option long_option[] = {
		{"vm2vm", required_argument, NULL, 0},
		{"rx-retry", required_argument, NULL, 0},
		{"rx-retry-delay", required_argument, NULL, 0},
		{"rx-retry-num", required_argument, NULL, 0},
		{"mergeable", required_argument, NULL, 0},
		{"stats", required_argument, NULL, 0},
		{"socket-file", required_argument, NULL, 0},
		{"tx-csum", required_argument, NULL, 0},
		{"tso", required_argument, NULL, 0},
		{"client", no_argument, &client_mode, 1},
		{"dequeue-zero-copy", no_argument, &dequeue_zero_copy, 1},
		{"builtin-net-driver", no_argument, &builtin_net_driver, 1},
		{NULL, 0, 0, 0},
	};

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:P",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		/* Portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid portmask\n");
				us_vhost_usage(prgname);
				return -1;
			}
			break;

		case 'P':
			promiscuous = 1;
			vmdq_conf_default.rx_adv_conf.vmdq_rx_conf.rx_mode =
				ETH_VMDQ_ACCEPT_BROADCAST |
				ETH_VMDQ_ACCEPT_MULTICAST;

			break;

		case 0:
			/* Enable/disable vm2vm comms. */
			if (!strncmp(long_option[option_index].name, "vm2vm",
				MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, (VM2VM_LAST - 1));
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for "
						"vm2vm [0|1|2]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					vm2vm_mode = (vm2vm_type)ret;
				}
			}

			/* Enable/disable retries on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_retry = ret;
				}
			}

			/* Enable/disable TX checksum offload. */
			if (!strncmp(long_option[option_index].name, "tx-csum", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for tx-csum [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else
					enable_tx_csum = ret;
			}

			/* Enable/disable TSO offload. */
			if (!strncmp(long_option[option_index].name, "tso", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for tso [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else
					enable_tso = ret;
			}

			/* Specify the retries delay time (in useconds) on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry-delay", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-delay [0-N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					burst_rx_delay_time = ret;
				}
			}

			/* Specify the retries number on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry-num", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-num [0-N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					burst_rx_retry_num = ret;
				}
			}

			/* Enable/disable RX mergeable buffers. */
			if (!strncmp(long_option[option_index].name, "mergeable", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for mergeable [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					mergeable = !!ret;
					if (ret) {
						vmdq_conf_default.rxmode.jumbo_frame = 1;
						vmdq_conf_default.rxmode.max_rx_pkt_len
							= JUMBO_FRAME_MAX_SIZE;
					}
				}
			}

			/* Enable/disable stats. */
			if (!strncmp(long_option[option_index].name, "stats", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for stats [0..N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_stats = ret;
				}
			}

			/* Set socket file path. */
			if (!strncmp(long_option[option_index].name,
						"socket-file", MAX_LONG_OPT_SZ)) {
				if (us_vhost_parse_socket_path(optarg) == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for socket name (Max %d characters)\n",
					PATH_MAX);
					us_vhost_usage(prgname);
					return -1;
				}
			}

			break;

			/* Invalid option - print options. */
		default:
			us_vhost_usage(prgname);
			return -1;
		}
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (enabled_port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;
	}

	if ((num_ports ==  0) || (num_ports > MAX_SUP_PORTS)) {
		RTE_LOG(INFO, VHOST_PORT, "Current enabled port number is %u,"
			"but only %u port can be enabled\n",num_ports, MAX_SUP_PORTS);
		return -1;
	}

	return 0;
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
		RTE_LOG(INFO, VHOST_PORT, "\nSpecified port number(%u) exceeds total system port number(%u)\n",
			num_ports, nb_ports);
		num_ports = nb_ports;
	}

	for (portid = 0; portid < num_ports; portid ++) {
		if (ports[portid] >= nb_ports) {
			RTE_LOG(INFO, VHOST_PORT, "\nSpecified port ID(%u) exceeds max system port ID(%u)\n",
				ports[portid], (nb_ports - 1));
			ports[portid] = INVALID_PORT_ID;
			valid_num_ports--;
		}
	}
	return valid_num_ports;
}

static __rte_always_inline struct vhost_dev *
find_vhost_dev(struct ether_addr *mac)
{
	struct vhost_dev *vdev;

	TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
		if (vdev->ready == DEVICE_RX &&
		    is_same_ether_addr(mac, &vdev->mac_address))
			return vdev;
	}

	return NULL;
}

/*
 * This function learns the MAC address of the device and registers this along with a
 * vlan tag to a VMDQ.
 */
static int
link_vmdq(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct ether_hdr *pkt_hdr;
	int i, ret;

	/* Learn MAC address of guest device from packet */
	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (find_vhost_dev(&pkt_hdr->s_addr)) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) device is using a registered MAC!\n",
			vdev->vid);
		return -1;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		vdev->mac_address.addr_bytes[i] = pkt_hdr->s_addr.addr_bytes[i];

	/* vlan_tag currently uses the device_id. */
	vdev->vlan_tag = vlan_tags[vdev->vid];

	/* Print out VMDQ registration info. */
	RTE_LOG(INFO, VHOST_DATA,
		"(%d) mac %02x:%02x:%02x:%02x:%02x:%02x and vlan %d registered\n",
		vdev->vid,
		vdev->mac_address.addr_bytes[0], vdev->mac_address.addr_bytes[1],
		vdev->mac_address.addr_bytes[2], vdev->mac_address.addr_bytes[3],
		vdev->mac_address.addr_bytes[4], vdev->mac_address.addr_bytes[5],
		vdev->vlan_tag);

	/* Register the MAC address. */
	ret = rte_eth_dev_mac_addr_add(ports[0], &vdev->mac_address,
				(uint32_t)vdev->vid + vmdq_pool_base);
	if (ret)
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) failed to add device MAC address to VMDQ\n",
			vdev->vid);

	rte_eth_dev_set_vlan_strip_on_queue(ports[0], vdev->vmdq_rx_q, 1);

	/* Set device as ready for RX. */
	vdev->ready = DEVICE_RX;

	return 0;
}

/*
 * Removes MAC address and vlan tag from VMDQ. Ensures that nothing is adding buffers to the RX
 * queue before disabling RX on the device.
 */
static inline void
unlink_vmdq(struct vhost_dev *vdev)
{
	unsigned i = 0;
	unsigned rx_count;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	if (vdev->ready == DEVICE_RX) {
		/*clear MAC and VLAN settings*/
		rte_eth_dev_mac_addr_remove(ports[0], &vdev->mac_address);
		for (i = 0; i < 6; i++)
			vdev->mac_address.addr_bytes[i] = 0;

		vdev->vlan_tag = 0;

		/*Clear out the receive buffers*/
		rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)vdev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);

		while (rx_count) {
			for (i = 0; i < rx_count; i++)
				rte_pktmbuf_free(pkts_burst[i]);

			rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)vdev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);
		}

		vdev->ready = DEVICE_MAC_LEARNING;
	}
}

static __rte_always_inline void
virtio_xmit(struct vhost_dev *dst_vdev, struct vhost_dev *src_vdev,
	    struct rte_mbuf *m)
{
	uint16_t ret;

	if (builtin_net_driver) {
		ret = vs_enqueue_pkts(dst_vdev, VIRTIO_RXQ, &m, 1);
	} else {
		ret = rte_vhost_enqueue_burst(dst_vdev->vid, VIRTIO_RXQ, &m, 1);
	}

	if (enable_stats) {
		rte_atomic64_inc(&dst_vdev->stats.rx_total_atomic);
		rte_atomic64_add(&dst_vdev->stats.rx_atomic, ret);
		src_vdev->stats.tx_total++;
		src_vdev->stats.tx += ret;
	}
}

/*
 * Check if the packet destination MAC address is for a local device. If so then put
 * the packet on that devices RX queue. If not then return.
 */
static __rte_always_inline int
virtio_tx_local(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct ether_hdr *pkt_hdr;
	struct vhost_dev *dst_vdev;

	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	dst_vdev = find_vhost_dev(&pkt_hdr->d_addr);
	if (!dst_vdev)
		return -1;

	if (vdev->vid == dst_vdev->vid) {
		RTE_LOG_DP(DEBUG, VHOST_DATA,
			"(%d) TX: src and dst MAC is same. Dropping packet.\n",
			vdev->vid);
		return 0;
	}

	RTE_LOG_DP(DEBUG, VHOST_DATA,
		"(%d) TX: MAC address is local\n", dst_vdev->vid);

	if (unlikely(dst_vdev->remove)) {
		RTE_LOG_DP(DEBUG, VHOST_DATA,
			"(%d) device is marked for removal\n", dst_vdev->vid);
		return 0;
	}

	virtio_xmit(dst_vdev, vdev, m);
	return 0;
}

/*
 * Check if the destination MAC of a packet is one local VM,
 * and get its vlan tag, and offset if it is.
 */
static __rte_always_inline int
find_local_dest(struct vhost_dev *vdev, struct rte_mbuf *m,
	uint32_t *offset, uint16_t *vlan_tag)
{
	struct vhost_dev *dst_vdev;
	struct ether_hdr *pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	dst_vdev = find_vhost_dev(&pkt_hdr->d_addr);
	if (!dst_vdev)
		return 0;

	if (vdev->vid == dst_vdev->vid) {
		RTE_LOG_DP(DEBUG, VHOST_DATA,
			"(%d) TX: src and dst MAC is same. Dropping packet.\n",
			vdev->vid);
		return -1;
	}

	/*
	 * HW vlan strip will reduce the packet length
	 * by minus length of vlan tag, so need restore
	 * the packet length by plus it.
	 */
	*offset  = VLAN_HLEN;
	*vlan_tag = vlan_tags[vdev->vid];

	RTE_LOG_DP(DEBUG, VHOST_DATA,
		"(%d) TX: pkt to local VM device id: (%d), vlan tag: %u.\n",
		vdev->vid, dst_vdev->vid, *vlan_tag);

	return 0;
}

static uint16_t
get_psd_sum(void *l3_hdr, uint64_t ol_flags)
{
	if (ol_flags & PKT_TX_IPV4)
		return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
	else /* assume ethertype == ETHER_TYPE_IPv6 */
		return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

static void virtio_tx_offload(struct rte_mbuf *m)
{
	void *l3_hdr;
	struct ipv4_hdr *ipv4_hdr = NULL;
	struct tcp_hdr *tcp_hdr = NULL;
	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	l3_hdr = (char *)eth_hdr + m->l2_len;

	if (m->ol_flags & PKT_TX_IPV4) {
		ipv4_hdr = l3_hdr;
		ipv4_hdr->hdr_checksum = 0;
		m->ol_flags |= PKT_TX_IP_CKSUM;
	}

	tcp_hdr = (struct tcp_hdr *)((char *)l3_hdr + m->l3_len);
	tcp_hdr->cksum = get_psd_sum(l3_hdr, m->ol_flags);
}

static inline void
free_pkts(struct rte_mbuf **pkts, uint16_t n)
{
	while (n--)
		rte_pktmbuf_free(pkts[n]);
}

static __rte_always_inline void
do_drain_mbuf_table(struct mbuf_table *tx_q)
{
	uint16_t count;

	count = rte_eth_tx_burst(ports[0], tx_q->txq_id,
				 tx_q->m_table, tx_q->len);
	if (unlikely(count < tx_q->len))
		free_pkts(&tx_q->m_table[count], tx_q->len - count);

	tx_q->len = 0;
}

/*
 * This function routes the TX packet to the correct interface. This
 * may be a local device or the physical port.
 */
static __rte_always_inline void
virtio_tx_route(struct vhost_dev *vdev, struct rte_mbuf *m, uint16_t vlan_tag)
{
	struct mbuf_table *tx_q;
	unsigned offset = 0;
	const uint16_t lcore_id = rte_lcore_id();
	struct ether_hdr *nh;


	nh = rte_pktmbuf_mtod(m, struct ether_hdr *);
	if (unlikely(is_broadcast_ether_addr(&nh->d_addr))) {
		struct vhost_dev *vdev2;

		TAILQ_FOREACH(vdev2, &vhost_dev_list, global_vdev_entry) {
			virtio_xmit(vdev2, vdev, m);
		}
		goto queue2nic;
	}

	/*check if destination is local VM*/
	if ((vm2vm_mode == VM2VM_SOFTWARE) && (virtio_tx_local(vdev, m) == 0)) {
		rte_pktmbuf_free(m);
		return;
	}

	if (unlikely(vm2vm_mode == VM2VM_HARDWARE)) {
		if (unlikely(find_local_dest(vdev, m, &offset,
					     &vlan_tag) != 0)) {
			rte_pktmbuf_free(m);
			return;
		}
	}

	RTE_LOG_DP(DEBUG, VHOST_DATA,
		"(%d) TX: MAC address is external\n", vdev->vid);

queue2nic:

	/*Add packet to the port tx queue*/
	tx_q = &lcore_tx_queue[lcore_id];

	nh = rte_pktmbuf_mtod(m, struct ether_hdr *);
	if (unlikely(nh->ether_type == rte_cpu_to_be_16(ETHER_TYPE_VLAN))) {
		/* Guest has inserted the vlan tag. */
		struct vlan_hdr *vh = (struct vlan_hdr *) (nh + 1);
		uint16_t vlan_tag_be = rte_cpu_to_be_16(vlan_tag);
		if ((vm2vm_mode == VM2VM_HARDWARE) &&
			(vh->vlan_tci != vlan_tag_be))
			vh->vlan_tci = vlan_tag_be;
	} else {
		m->ol_flags |= PKT_TX_VLAN_PKT;

		/*
		 * Find the right seg to adjust the data len when offset is
		 * bigger than tail room size.
		 */
		if (unlikely(vm2vm_mode == VM2VM_HARDWARE)) {
			if (likely(offset <= rte_pktmbuf_tailroom(m)))
				m->data_len += offset;
			else {
				struct rte_mbuf *seg = m;

				while ((seg->next != NULL) &&
					(offset > rte_pktmbuf_tailroom(seg)))
					seg = seg->next;

				seg->data_len += offset;
			}
			m->pkt_len += offset;
		}

		m->vlan_tci = vlan_tag;
	}

	if (m->ol_flags & PKT_TX_TCP_SEG)
		virtio_tx_offload(m);

	tx_q->m_table[tx_q->len++] = m;
	if (enable_stats) {
		vdev->stats.tx_total++;
		vdev->stats.tx++;
	}

	if (unlikely(tx_q->len == MAX_PKT_BURST))
		do_drain_mbuf_table(tx_q);
}


static __rte_always_inline void
drain_mbuf_table(struct mbuf_table *tx_q)
{
	static uint64_t prev_tsc;
	uint64_t cur_tsc;

	if (tx_q->len == 0)
		return;

	cur_tsc = rte_rdtsc();
	if (unlikely(cur_tsc - prev_tsc > MBUF_TABLE_DRAIN_TSC)) {
		prev_tsc = cur_tsc;

		RTE_LOG_DP(DEBUG, VHOST_DATA,
			"TX queue drained after timeout with burst size %u\n",
			tx_q->len);
		do_drain_mbuf_table(tx_q);
	}
}

static __rte_always_inline void
drain_eth_rx(struct vhost_dev *vdev)
{
	uint16_t rx_count, enqueue_count;
	struct rte_mbuf *pkts[MAX_PKT_BURST];

	rx_count = rte_eth_rx_burst(ports[0], vdev->vmdq_rx_q,
				    pkts, MAX_PKT_BURST);
	if (!rx_count)
		return;

	/*
	 * When "enable_retry" is set, here we wait and retry when there
	 * is no enough free slots in the queue to hold @rx_count packets,
	 * to diminish packet loss.
	 */
	if (enable_retry &&
	    unlikely(rx_count > rte_vhost_avail_entries(vdev->vid,
			VIRTIO_RXQ))) {
		uint32_t retry;

		for (retry = 0; retry < burst_rx_retry_num; retry++) {
			rte_delay_us(burst_rx_delay_time);
			if (rx_count <= rte_vhost_avail_entries(vdev->vid,
					VIRTIO_RXQ))
				break;
		}
	}

	if (builtin_net_driver) {
		enqueue_count = vs_enqueue_pkts(vdev, VIRTIO_RXQ,
						pkts, rx_count);
	} else {
		enqueue_count = rte_vhost_enqueue_burst(vdev->vid, VIRTIO_RXQ,
						pkts, rx_count);
	}
	if (enable_stats) {
		rte_atomic64_add(&vdev->stats.rx_total_atomic, rx_count);
		rte_atomic64_add(&vdev->stats.rx_atomic, enqueue_count);
	}

	free_pkts(pkts, rx_count);
}

static __rte_always_inline void
drain_virtio_tx(struct vhost_dev *vdev)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	uint16_t count;
	uint16_t i;

	if (builtin_net_driver) {
		count = vs_dequeue_pkts(vdev, VIRTIO_TXQ, mbuf_pool,
					pkts, MAX_PKT_BURST);
	} else {
		count = rte_vhost_dequeue_burst(vdev->vid, VIRTIO_TXQ,
					mbuf_pool, pkts, MAX_PKT_BURST);
	}

	/* setup VMDq for the first packet */
	if (unlikely(vdev->ready == DEVICE_MAC_LEARNING) && count) {
		if (vdev->remove || link_vmdq(vdev, pkts[0]) == -1)
			free_pkts(pkts, count);
	}

	for (i = 0; i < count; ++i)
		virtio_tx_route(vdev, pkts[i], vlan_tags[vdev->vid]);
}

/*
 * Main function of vhost-switch. It basically does:
 *
 * for each vhost device {
 *    - drain_eth_rx()
 *
 *      Which drains the host eth Rx queue linked to the vhost device,
 *      and deliver all of them to guest virito Rx ring associated with
 *      this vhost device.
 *
 *    - drain_virtio_tx()
 *
 *      Which drains the guest virtio Tx queue and deliver all of them
 *      to the target, which could be another vhost device, or the
 *      physical eth dev. The route is done in function "virtio_tx_route".
 * }
 */
static int
switch_worker(void *arg __rte_unused)
{
	unsigned i;
	unsigned lcore_id = rte_lcore_id();
	struct vhost_dev *vdev;
	struct mbuf_table *tx_q;

	RTE_LOG(INFO, VHOST_DATA, "Procesing on Core %u started\n", lcore_id);

	tx_q = &lcore_tx_queue[lcore_id];
	for (i = 0; i < rte_lcore_count(); i++) {
		if (lcore_ids[i] == lcore_id) {
			tx_q->txq_id = i;
			break;
		}
	}

	while(1) {
		drain_mbuf_table(tx_q);

		/*
		 * Inform the configuration core that we have exited the
		 * linked list and that no devices are in use if requested.
		 */
		if (lcore_info[lcore_id].dev_removal_flag == REQUEST_DEV_REMOVAL)
			lcore_info[lcore_id].dev_removal_flag = ACK_DEV_REMOVAL;

		/*
		 * Process vhost devices
		 */
		TAILQ_FOREACH(vdev, &lcore_info[lcore_id].vdev_list,
			      lcore_vdev_entry) {
			if (unlikely(vdev->remove)) {
				unlink_vmdq(vdev);
				vdev->ready = DEVICE_SAFE_REMOVE;
				continue;
			}

			if (likely(vdev->ready == DEVICE_RX))
				drain_eth_rx(vdev);

			if (likely(!vdev->remove))
				drain_virtio_tx(vdev);
		}
	}

	return 0;
}

/*
 * Remove a device from the specific data core linked list and from the
 * main linked list. Synchonization  occurs through the use of the
 * lcore dev_removal_flag. Device is made volatile here to avoid re-ordering
 * of dev->remove=1 which can cause an infinite loop in the rte_pause loop.
 */
static void
destroy_device(int vid)
{
	struct vhost_dev *vdev = NULL;
	int lcore;

	TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
		if (vdev->vid == vid)
			break;
	}
	if (!vdev)
		return;
	/*set the remove flag. */
	vdev->remove = 1;
	while(vdev->ready != DEVICE_SAFE_REMOVE) {
		rte_pause();
	}

	if (builtin_net_driver)
		vs_vhost_net_remove(vdev);

	TAILQ_REMOVE(&lcore_info[vdev->coreid].vdev_list, vdev,
		     lcore_vdev_entry);
	TAILQ_REMOVE(&vhost_dev_list, vdev, global_vdev_entry);


	/* Set the dev_removal_flag on each lcore. */
	RTE_LCORE_FOREACH_SLAVE(lcore)
		lcore_info[lcore].dev_removal_flag = REQUEST_DEV_REMOVAL;

	/*
	 * Once each core has set the dev_removal_flag to ACK_DEV_REMOVAL
	 * we can be sure that they can no longer access the device removed
	 * from the linked lists and that the devices are no longer in use.
	 */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		while (lcore_info[lcore].dev_removal_flag != ACK_DEV_REMOVAL)
			rte_pause();
	}

	lcore_info[vdev->coreid].device_num--;

	RTE_LOG(INFO, VHOST_DATA,
		"(%d) device has been removed from data core\n",
		vdev->vid);

	rte_free(vdev);
}

/*
 * A new device is added to a data core. First the device is added to the main linked list
 * and the allocated to a specific data core.
 */
static int
new_device(int vid)
{
	int lcore, core_add = 0;
	uint32_t device_num_min = num_devices;
	struct vhost_dev *vdev;

	vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
	if (vdev == NULL) {
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) couldn't allocate memory for vhost dev\n",
			vid);
		return -1;
	}
	vdev->vid = vid;

	if (builtin_net_driver)
		vs_vhost_net_setup(vdev);

	TAILQ_INSERT_TAIL(&vhost_dev_list, vdev, global_vdev_entry);
	vdev->vmdq_rx_q = vid * queues_per_pool + vmdq_queue_base;

	/*reset ready flag*/
	vdev->ready = DEVICE_MAC_LEARNING;
	vdev->remove = 0;

	/* Find a suitable lcore to add the device. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (lcore_info[lcore].device_num < device_num_min) {
			device_num_min = lcore_info[lcore].device_num;
			core_add = lcore;
		}
	}
	vdev->coreid = core_add;

	TAILQ_INSERT_TAIL(&lcore_info[vdev->coreid].vdev_list, vdev,
			  lcore_vdev_entry);
	lcore_info[vdev->coreid].device_num++;

	/* Disable notifications. */
	rte_vhost_enable_guest_notification(vid, VIRTIO_RXQ, 0);
	rte_vhost_enable_guest_notification(vid, VIRTIO_TXQ, 0);

	RTE_LOG(INFO, VHOST_DATA,
		"(%d) device has been added to data core %d\n",
		vid, vdev->coreid);

	return 0;
}

/*
 * These callback allow devices to be added to the data core when configuration
 * has been fully complete.
 */
static const struct vhost_device_ops virtio_net_device_ops =
{
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

/*
 * This is a thread will wake up after a period to print stats if the user has
 * enabled them.
 */
static void
print_stats(void)
{
	struct vhost_dev *vdev;
	uint64_t tx_dropped, rx_dropped;
	uint64_t tx, tx_total, rx, rx_total;
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char top_left[] = { 27, '[', '1', ';', '1', 'H','\0' };

	while(1) {
		sleep(enable_stats);

		/* Clear screen and move to top left */
		printf("%s%s\n", clr, top_left);
		printf("Device statistics =================================\n");

		TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
			tx_total   = vdev->stats.tx_total;
			tx         = vdev->stats.tx;
			tx_dropped = tx_total - tx;

			rx_total   = rte_atomic64_read(&vdev->stats.rx_total_atomic);
			rx         = rte_atomic64_read(&vdev->stats.rx_atomic);
			rx_dropped = rx_total - rx;

			printf("Statistics for device %d\n"
				"-----------------------\n"
				"TX total:              %" PRIu64 "\n"
				"TX dropped:            %" PRIu64 "\n"
				"TX successful:         %" PRIu64 "\n"
				"RX total:              %" PRIu64 "\n"
				"RX dropped:            %" PRIu64 "\n"
				"RX successful:         %" PRIu64 "\n",
				vdev->vid,
				tx_total, tx_dropped, tx,
				rx_total, rx_dropped, rx);
		}

		printf("===================================================\n");
	}
}

static void
unregister_drivers(int socket_num)
{
	int i, ret;

	for (i = 0; i < socket_num; i++) {
		ret = rte_vhost_driver_unregister(socket_files + i * PATH_MAX);
		if (ret != 0)
			RTE_LOG(ERR, VHOST_CONFIG,
				"Fail to unregister vhost driver for %s.\n",
				socket_files + i * PATH_MAX);
	}
}

/* When we receive a INT signal, unregister vhost driver */
static void
sigint_handler(__rte_unused int signum)
{
	/* Unregister vhost driver. */
	unregister_drivers(nb_sockets);

	exit(0);
}

/*
 * While creating an mbuf pool, one key thing is to figure out how
 * many mbuf entries is enough for our use. FYI, here are some
 * guidelines:
 *
 * - Each rx queue would reserve @nr_rx_desc mbufs at queue setup stage
 *
 * - For each switch core (A CPU core does the packet switch), we need
 *   also make some reservation for receiving the packets from virtio
 *   Tx queue. How many is enough depends on the usage. It's normally
 *   a simple calculation like following:
 *
 *       MAX_PKT_BURST * max packet size / mbuf size
 *
 *   So, we definitely need allocate more mbufs when TSO is enabled.
 *
 * - Similarly, for each switching core, we should serve @nr_rx_desc
 *   mbufs for receiving the packets from physical NIC device.
 *
 * - We also need make sure, for each switch core, we have allocated
 *   enough mbufs to fill up the mbuf cache.
 */
static void
create_mbuf_pool(uint16_t nr_port, uint32_t nr_switch_core, uint32_t mbuf_size,
	uint32_t nr_queues, uint32_t nr_rx_desc, uint32_t nr_mbuf_cache)
{
	uint32_t nr_mbufs;
	uint32_t nr_mbufs_per_core;
	uint32_t mtu = 1500;

	if (mergeable)
		mtu = 9000;
	if (enable_tso)
		mtu = 64 * 1024;

	nr_mbufs_per_core  = (mtu + mbuf_size) * MAX_PKT_BURST /
			(mbuf_size - RTE_PKTMBUF_HEADROOM);
	nr_mbufs_per_core += nr_rx_desc;
	nr_mbufs_per_core  = RTE_MAX(nr_mbufs_per_core, nr_mbuf_cache);

	nr_mbufs  = nr_queues * nr_rx_desc;
	nr_mbufs += nr_mbufs_per_core * nr_switch_core;
	nr_mbufs *= nr_port;

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", nr_mbufs,
					    nr_mbuf_cache, 0, mbuf_size,
					    rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
}

/*
 * Main function, does initialisation and calls the per-lcore functions.
 */
int
main(int argc, char *argv[])
{
	unsigned lcore_id, core_id = 0;
	unsigned nb_ports, valid_num_ports;
	int ret, i;
	uint8_t portid;
	static pthread_t tid;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];
	uint64_t flags = 0;

	signal(SIGINT, sigint_handler);

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* parse app arguments */
	ret = us_vhost_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid argument\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		TAILQ_INIT(&lcore_info[lcore_id].vdev_list);

		if (rte_lcore_is_enabled(lcore_id))
			lcore_ids[core_id++] = lcore_id;
	}

	if (rte_lcore_count() > RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE,"Not enough cores\n");

	/* Get the number of physical ports. */
	nb_ports = rte_eth_dev_count();

	/*
	 * Update the global var NUM_PORTS and global array PORTS
	 * and get value of var VALID_NUM_PORTS according to system ports number
	 */
	valid_num_ports = check_ports_num(nb_ports);

	if ((valid_num_ports ==  0) || (valid_num_ports > MAX_SUP_PORTS)) {
		RTE_LOG(INFO, VHOST_PORT, "Current enabled port number is %u,"
			"but only %u port can be enabled\n",num_ports, MAX_SUP_PORTS);
		return -1;
	}

	/*
	 * FIXME: here we are trying to allocate mbufs big enough for
	 * @MAX_QUEUES, but the truth is we're never going to use that
	 * many queues here. We probably should only do allocation for
	 * those queues we are going to use.
	 */
	create_mbuf_pool(valid_num_ports, rte_lcore_count() - 1, MBUF_DATA_SIZE,
			 MAX_QUEUES, RTE_TEST_RX_DESC_DEFAULT, MBUF_CACHE_SIZE);

	if (vm2vm_mode == VM2VM_HARDWARE) {
		/* Enable VT loop back to let L2 switch to do it. */
		vmdq_conf_default.rx_adv_conf.vmdq_rx_conf.enable_loop_back = 1;
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"Enable loop back for L2 switch in vmdq.\n");
	}

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			RTE_LOG(INFO, VHOST_PORT,
				"Skipping disabled port %d\n", portid);
			continue;
		}
		if (port_init(portid) != 0)
			rte_exit(EXIT_FAILURE,
				"Cannot initialize network ports\n");
	}

	/* Enable stats if the user option is set. */
	if (enable_stats) {
		ret = pthread_create(&tid, NULL, (void *)print_stats, NULL);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Cannot create print-stats thread\n");

		/* Set thread_name for aid in debugging.  */
		snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "print-stats");
		ret = rte_thread_setname(tid, thread_name);
		if (ret != 0)
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"Cannot set print-stats name\n");
	}

	/* Launch all data cores. */
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
		rte_eal_remote_launch(switch_worker, NULL, lcore_id);

	if (client_mode)
		flags |= RTE_VHOST_USER_CLIENT;

	if (dequeue_zero_copy)
		flags |= RTE_VHOST_USER_DEQUEUE_ZERO_COPY;

	/* Register vhost user driver to handle vhost messages. */
	for (i = 0; i < nb_sockets; i++) {
		char *file = socket_files + i * PATH_MAX;
		ret = rte_vhost_driver_register(file, flags);
		if (ret != 0) {
			unregister_drivers(i);
			rte_exit(EXIT_FAILURE,
				"vhost driver register failure.\n");
		}

		if (builtin_net_driver)
			rte_vhost_driver_set_features(file, VIRTIO_NET_FEATURES);

		if (mergeable == 0) {
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_MRG_RXBUF);
		}

		if (enable_tx_csum == 0) {
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_CSUM);
		}

		if (enable_tso == 0) {
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_HOST_TSO4);
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_HOST_TSO6);
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_GUEST_TSO4);
			rte_vhost_driver_disable_features(file,
				1ULL << VIRTIO_NET_F_GUEST_TSO6);
		}

		if (promiscuous) {
			rte_vhost_driver_enable_features(file,
				1ULL << VIRTIO_NET_F_CTRL_RX);
		}

		ret = rte_vhost_driver_callback_register(file,
			&virtio_net_device_ops);
		if (ret != 0) {
			rte_exit(EXIT_FAILURE,
				"failed to register vhost driver callbacks.\n");
		}

		if (rte_vhost_driver_start(file) < 0) {
			rte_exit(EXIT_FAILURE,
				"failed to start vhost driver.\n");
		}
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id)
		rte_eal_wait_lcore(lcore_id);

	return 0;

}
