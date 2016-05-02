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
#include <rte_virtio_net.h>
#include <rte_ip.h>
#include <rte_tcp.h>

#include "main.h"

#ifndef MAX_QUEUES
#define MAX_QUEUES 128
#endif

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

/*
 * Calculate the number of buffers needed per port
 */
#define NUM_MBUFS_PER_PORT ((MAX_QUEUES*RTE_TEST_RX_DESC_DEFAULT) +		\
							(num_switching_cores*MAX_PKT_BURST) +  			\
							(num_switching_cores*RTE_TEST_TX_DESC_DEFAULT) +\
							((num_switching_cores+1)*MBUF_CACHE_SIZE))

#define MBUF_CACHE_SIZE	128
#define MBUF_DATA_SIZE	RTE_MBUF_DEFAULT_BUF_SIZE

#define MAX_PKT_BURST 32		/* Max burst size for RX/TX */
#define BURST_TX_DRAIN_US 100	/* TX drain every ~100us */

#define BURST_RX_WAIT_US 15	/* Defines how long we wait between retries on RX */
#define BURST_RX_RETRIES 4		/* Number of retries on RX. */

#define JUMBO_FRAME_MAX_SIZE    0x2600

/* State of virtio device. */
#define DEVICE_MAC_LEARNING 0
#define DEVICE_RX			1
#define DEVICE_SAFE_REMOVE	2

/* Config_core_flag status definitions. */
#define REQUEST_DEV_REMOVAL 1
#define ACK_DEV_REMOVAL 0

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 512

#define INVALID_PORT_ID 0xFF

/* Max number of devices. Limited by vmdq. */
#define MAX_DEVICES 64

/* Size of buffers used for snprintfs. */
#define MAX_PRINT_BUFF 6072

/* Maximum character device basename size. */
#define MAX_BASENAME_SZ 10

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64

/* Used to compare MAC addresses. */
#define MAC_ADDR_CMP 0xFFFFFFFFFFFFULL

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

/* Promiscuous mode */
static uint32_t promiscuous;

/*Number of switching cores enabled*/
static uint32_t num_switching_cores = 0;

/* number of devices/queues to support*/
static uint32_t num_queues = 0;
static uint32_t num_devices;

static struct rte_mempool *mbuf_pool;
static int mergeable;

/* Do vlan strip on host, enabled on default */
static uint32_t vlan_strip = 1;

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

/* Specify timeout (in useconds) between retries on RX. */
static uint32_t burst_rx_delay_time = BURST_RX_WAIT_US;
/* Specify the number of retries on RX. */
static uint32_t burst_rx_retry_num = BURST_RX_RETRIES;

/* Character device basename. Can be set by user. */
static char dev_basename[MAX_BASENAME_SZ] = "vhost-net";

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
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
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

/* heads for the main used and free linked lists for the data path. */
static struct virtio_net_data_ll *ll_root_used = NULL;
static struct virtio_net_data_ll *ll_root_free = NULL;

/* Array of data core structures containing information on individual core linked lists. */
static struct lcore_info lcore_info[RTE_MAX_LCORE];

/* Used for queueing bursts of TX packets. */
struct mbuf_table {
	unsigned len;
	unsigned txq_id;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

/* TX queue for each data core. */
struct mbuf_table lcore_tx_queue[RTE_MAX_LCORE];

#define VLAN_HLEN       4

/* Per-device statistics struct */
struct device_statistics {
	uint64_t tx_total;
	rte_atomic64_t rx_total_atomic;
	uint64_t tx;
	rte_atomic64_t rx_atomic;
} __rte_cache_aligned;
struct device_statistics dev_statistics[MAX_DEVICES];

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

	if (enable_tx_csum == 0)
		rte_vhost_feature_disable(1ULL << VIRTIO_NET_F_CSUM);

	if (enable_tso == 0) {
		rte_vhost_feature_disable(1ULL << VIRTIO_NET_F_HOST_TSO4);
		rte_vhost_feature_disable(1ULL << VIRTIO_NET_F_HOST_TSO6);
	}

	rx_rings = (uint16_t)dev_info.max_rx_queues;
	/* Configure ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Setup the queues. */
	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
						rte_eth_dev_socket_id(port),
						rxconf,
						mbuf_pool);
		if (retval < 0)
			return retval;
	}
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
						rte_eth_dev_socket_id(port),
						txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the device. */
	retval  = rte_eth_dev_start(port);
	if (retval < 0) {
		RTE_LOG(ERR, VHOST_DATA, "Failed to start the device.\n");
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
 * Set character device basename.
 */
static int
us_vhost_parse_basename(const char *q_arg)
{
	/* parse number string */

	if (strnlen(q_arg, MAX_BASENAME_SZ) > MAX_BASENAME_SZ)
		return -1;
	else
		snprintf((char*)&dev_basename, MAX_BASENAME_SZ, "%s", q_arg);

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
	"		--dev-basename <name>\n"
	"		--nb-devices ND\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--vm2vm [0|1|2]: disable/software(default)/hardware vm2vm comms\n"
	"		--rx-retry [0|1]: disable/enable(default) retries on rx. Enable retry if destintation queue is full\n"
	"		--rx-retry-delay [0-N]: timeout(in usecond) between retries on RX. This makes effect only if retries on rx enabled\n"
	"		--rx-retry-num [0-N]: the number of retries on rx. This makes effect only if retries on rx enabled\n"
	"		--mergeable [0|1]: disable(default)/enable RX mergeable buffers\n"
	"		--vlan-strip [0|1]: disable/enable(default) RX VLAN strip on host\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n"
	"		--dev-basename: The basename to be used for the character device.\n"
	"		--tx-csum [0|1] disable/enable TX checksum offload.\n"
	"		--tso [0|1] disable/enable TCP segment offload.\n",
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
		{"vlan-strip", required_argument, NULL, 0},
		{"stats", required_argument, NULL, 0},
		{"dev-basename", required_argument, NULL, 0},
		{"tx-csum", required_argument, NULL, 0},
		{"tso", required_argument, NULL, 0},
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
			rte_vhost_feature_enable(1ULL << VIRTIO_NET_F_CTRL_RX);

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

			/* Enable/disable RX VLAN strip on host. */
			if (!strncmp(long_option[option_index].name,
				"vlan-strip", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for VLAN strip [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					vlan_strip = !!ret;
					vmdq_conf_default.rxmode.hw_vlan_strip =
						vlan_strip;
				}
			}

			/* Enable/disable stats. */
			if (!strncmp(long_option[option_index].name, "stats", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for stats [0..N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_stats = ret;
				}
			}

			/* Set character device basename. */
			if (!strncmp(long_option[option_index].name, "dev-basename", MAX_LONG_OPT_SZ)) {
				if (us_vhost_parse_basename(optarg) == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for character device basename (Max %d characters)\n", MAX_BASENAME_SZ);
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

/*
 * Compares a packet destination MAC address to a device MAC address.
 */
static inline int __attribute__((always_inline))
ether_addr_cmp(struct ether_addr *ea, struct ether_addr *eb)
{
	return ((*(uint64_t *)ea ^ *(uint64_t *)eb) & MAC_ADDR_CMP) == 0;
}

/*
 * This function learns the MAC address of the device and registers this along with a
 * vlan tag to a VMDQ.
 */
static int
link_vmdq(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct ether_hdr *pkt_hdr;
	struct virtio_net_data_ll *dev_ll;
	struct virtio_net *dev = vdev->dev;
	int i, ret;

	/* Learn MAC address of guest device from packet */
	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	dev_ll = ll_root_used;

	while (dev_ll != NULL) {
		if (ether_addr_cmp(&(pkt_hdr->s_addr), &dev_ll->vdev->mac_address)) {
			RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") WARNING: This device is using an existing MAC address and has not been registered.\n", dev->device_fh);
			return -1;
		}
		dev_ll = dev_ll->next;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		vdev->mac_address.addr_bytes[i] = pkt_hdr->s_addr.addr_bytes[i];

	/* vlan_tag currently uses the device_id. */
	vdev->vlan_tag = vlan_tags[dev->device_fh];

	/* Print out VMDQ registration info. */
	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") MAC_ADDRESS %02x:%02x:%02x:%02x:%02x:%02x and VLAN_TAG %d registered\n",
		dev->device_fh,
		vdev->mac_address.addr_bytes[0], vdev->mac_address.addr_bytes[1],
		vdev->mac_address.addr_bytes[2], vdev->mac_address.addr_bytes[3],
		vdev->mac_address.addr_bytes[4], vdev->mac_address.addr_bytes[5],
		vdev->vlan_tag);

	/* Register the MAC address. */
	ret = rte_eth_dev_mac_addr_add(ports[0], &vdev->mac_address,
				(uint32_t)dev->device_fh + vmdq_pool_base);
	if (ret)
		RTE_LOG(ERR, VHOST_DATA, "(%"PRIu64") Failed to add device MAC address to VMDQ\n",
					dev->device_fh);

	/* Enable stripping of the vlan tag as we handle routing. */
	if (vlan_strip)
		rte_eth_dev_set_vlan_strip_on_queue(ports[0],
			(uint16_t)vdev->vmdq_rx_q, 1);

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

/*
 * Check if the packet destination MAC address is for a local device. If so then put
 * the packet on that devices RX queue. If not then return.
 */
static inline int __attribute__((always_inline))
virtio_tx_local(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct virtio_net_data_ll *dev_ll;
	struct ether_hdr *pkt_hdr;
	uint64_t ret = 0;
	struct virtio_net *dev = vdev->dev;
	struct virtio_net *tdev; /* destination virito device */

	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/*get the used devices list*/
	dev_ll = ll_root_used;

	while (dev_ll != NULL) {
		if ((dev_ll->vdev->ready == DEVICE_RX) && ether_addr_cmp(&(pkt_hdr->d_addr),
				          &dev_ll->vdev->mac_address)) {

			/* Drop the packet if the TX packet is destined for the TX device. */
			if (dev_ll->vdev->dev->device_fh == dev->device_fh) {
				RTE_LOG(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
					"Source and destination MAC addresses are the same. "
					"Dropping packet.\n",
					dev->device_fh);
				return 0;
			}
			tdev = dev_ll->vdev->dev;


			RTE_LOG(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
				"MAC address is local\n", tdev->device_fh);

			if (unlikely(dev_ll->vdev->remove)) {
				/*drop the packet if the device is marked for removal*/
				RTE_LOG(DEBUG, VHOST_DATA, "(%" PRIu64 ") "
					"Device is marked for removal\n", tdev->device_fh);
			} else {
				/*send the packet to the local virtio device*/
				ret = rte_vhost_enqueue_burst(tdev, VIRTIO_RXQ, &m, 1);
				if (enable_stats) {
					rte_atomic64_add(
					&dev_statistics[tdev->device_fh].rx_total_atomic,
					1);
					rte_atomic64_add(
					&dev_statistics[tdev->device_fh].rx_atomic,
					ret);
					dev_statistics[dev->device_fh].tx_total++;
					dev_statistics[dev->device_fh].tx += ret;
				}
			}

			return 0;
		}
		dev_ll = dev_ll->next;
	}

	return -1;
}

/*
 * Check if the destination MAC of a packet is one local VM,
 * and get its vlan tag, and offset if it is.
 */
static inline int __attribute__((always_inline))
find_local_dest(struct virtio_net *dev, struct rte_mbuf *m,
	uint32_t *offset, uint16_t *vlan_tag)
{
	struct virtio_net_data_ll *dev_ll = ll_root_used;
	struct ether_hdr *pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	while (dev_ll != NULL) {
		if ((dev_ll->vdev->ready == DEVICE_RX)
			&& ether_addr_cmp(&(pkt_hdr->d_addr),
		&dev_ll->vdev->mac_address)) {
			/*
			 * Drop the packet if the TX packet is
			 * destined for the TX device.
			 */
			if (dev_ll->vdev->dev->device_fh == dev->device_fh) {
				RTE_LOG(DEBUG, VHOST_DATA,
				"(%"PRIu64") TX: Source and destination"
				" MAC addresses are the same. Dropping "
				"packet.\n",
				dev_ll->vdev->dev->device_fh);
				return -1;
			}

			/*
			 * HW vlan strip will reduce the packet length
			 * by minus length of vlan tag, so need restore
			 * the packet length by plus it.
			 */
			*offset = VLAN_HLEN;
			*vlan_tag =
			(uint16_t)
			vlan_tags[(uint16_t)dev_ll->vdev->dev->device_fh];

			RTE_LOG(DEBUG, VHOST_DATA,
			"(%"PRIu64") TX: pkt to local VM device id:"
			"(%"PRIu64") vlan tag: %d.\n",
			dev->device_fh, dev_ll->vdev->dev->device_fh,
			(int)*vlan_tag);

			break;
		}
		dev_ll = dev_ll->next;
	}
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

/*
 * This function routes the TX packet to the correct interface. This may be a local device
 * or the physical port.
 */
static inline void __attribute__((always_inline))
virtio_tx_route(struct vhost_dev *vdev, struct rte_mbuf *m, uint16_t vlan_tag)
{
	struct mbuf_table *tx_q;
	struct rte_mbuf **m_table;
	unsigned len, ret, offset = 0;
	const uint16_t lcore_id = rte_lcore_id();
	struct virtio_net *dev = vdev->dev;
	struct ether_hdr *nh;

	/*check if destination is local VM*/
	if ((vm2vm_mode == VM2VM_SOFTWARE) && (virtio_tx_local(vdev, m) == 0)) {
		rte_pktmbuf_free(m);
		return;
	}

	if (unlikely(vm2vm_mode == VM2VM_HARDWARE)) {
		if (unlikely(find_local_dest(dev, m, &offset, &vlan_tag) != 0)) {
			rte_pktmbuf_free(m);
			return;
		}
	}

	RTE_LOG(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
		"MAC address is external\n", dev->device_fh);

	/*Add packet to the port tx queue*/
	tx_q = &lcore_tx_queue[lcore_id];
	len = tx_q->len;

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

	tx_q->m_table[len] = m;
	len++;
	if (enable_stats) {
		dev_statistics[dev->device_fh].tx_total++;
		dev_statistics[dev->device_fh].tx++;
	}

	if (unlikely(len == MAX_PKT_BURST)) {
		m_table = (struct rte_mbuf **)tx_q->m_table;
		ret = rte_eth_tx_burst(ports[0], (uint16_t)tx_q->txq_id, m_table, (uint16_t) len);
		/* Free any buffers not handled by TX and update the port stats. */
		if (unlikely(ret < len)) {
			do {
				rte_pktmbuf_free(m_table[ret]);
			} while (++ret < len);
		}

		len = 0;
	}

	tx_q->len = len;
	return;
}
/*
 * This function is called by each data core. It handles all RX/TX registered with the
 * core. For TX the specific lcore linked list is used. For RX, MAC addresses are compared
 * with all devices in the main linked list.
 */
static int
switch_worker(__attribute__((unused)) void *arg)
{
	struct virtio_net *dev = NULL;
	struct vhost_dev *vdev = NULL;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct virtio_net_data_ll *dev_ll;
	struct mbuf_table *tx_q;
	volatile struct lcore_ll_info *lcore_ll;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;
	uint64_t prev_tsc, diff_tsc, cur_tsc, ret_count = 0;
	unsigned ret, i;
	const uint16_t lcore_id = rte_lcore_id();
	const uint16_t num_cores = (uint16_t)rte_lcore_count();
	uint16_t rx_count = 0;
	uint16_t tx_count;
	uint32_t retry = 0;

	RTE_LOG(INFO, VHOST_DATA, "Procesing on Core %u started\n", lcore_id);
	lcore_ll = lcore_info[lcore_id].lcore_ll;
	prev_tsc = 0;

	tx_q = &lcore_tx_queue[lcore_id];
	for (i = 0; i < num_cores; i ++) {
		if (lcore_ids[i] == lcore_id) {
			tx_q->txq_id = i;
			break;
		}
	}

	while(1) {
		cur_tsc = rte_rdtsc();
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			if (tx_q->len) {
				RTE_LOG(DEBUG, VHOST_DATA,
					"TX queue drained after timeout with burst size %u\n",
					tx_q->len);

				/*Tx any packets in the queue*/
				ret = rte_eth_tx_burst(ports[0], (uint16_t)tx_q->txq_id,
									   (struct rte_mbuf **)tx_q->m_table,
									   (uint16_t)tx_q->len);
				if (unlikely(ret < tx_q->len)) {
					do {
						rte_pktmbuf_free(tx_q->m_table[ret]);
					} while (++ret < tx_q->len);
				}

				tx_q->len = 0;
			}

			prev_tsc = cur_tsc;

		}

		rte_prefetch0(lcore_ll->ll_root_used);
		/*
		 * Inform the configuration core that we have exited the linked list and that no devices are
		 * in use if requested.
		 */
		if (lcore_ll->dev_removal_flag == REQUEST_DEV_REMOVAL)
			lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;

		/*
		 * Process devices
		 */
		dev_ll = lcore_ll->ll_root_used;

		while (dev_ll != NULL) {
			/*get virtio device ID*/
			vdev = dev_ll->vdev;
			dev = vdev->dev;

			if (unlikely(vdev->remove)) {
				dev_ll = dev_ll->next;
				unlink_vmdq(vdev);
				vdev->ready = DEVICE_SAFE_REMOVE;
				continue;
			}
			if (likely(vdev->ready == DEVICE_RX)) {
				/*Handle guest RX*/
				rx_count = rte_eth_rx_burst(ports[0],
					vdev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);

				if (rx_count) {
					/*
					* Retry is enabled and the queue is full then we wait and retry to avoid packet loss
					* Here MAX_PKT_BURST must be less than virtio queue size
					*/
					if (enable_retry && unlikely(rx_count > rte_vring_available_entries(dev, VIRTIO_RXQ))) {
						for (retry = 0; retry < burst_rx_retry_num; retry++) {
							rte_delay_us(burst_rx_delay_time);
							if (rx_count <= rte_vring_available_entries(dev, VIRTIO_RXQ))
								break;
						}
					}
					ret_count = rte_vhost_enqueue_burst(dev, VIRTIO_RXQ, pkts_burst, rx_count);
					if (enable_stats) {
						rte_atomic64_add(
						&dev_statistics[dev_ll->vdev->dev->device_fh].rx_total_atomic,
						rx_count);
						rte_atomic64_add(
						&dev_statistics[dev_ll->vdev->dev->device_fh].rx_atomic, ret_count);
					}
					while (likely(rx_count)) {
						rx_count--;
						rte_pktmbuf_free(pkts_burst[rx_count]);
					}

				}
			}

			if (likely(!vdev->remove)) {
				/* Handle guest TX*/
				tx_count = rte_vhost_dequeue_burst(dev, VIRTIO_TXQ, mbuf_pool, pkts_burst, MAX_PKT_BURST);
				/* If this is the first received packet we need to learn the MAC and setup VMDQ */
				if (unlikely(vdev->ready == DEVICE_MAC_LEARNING) && tx_count) {
					if (vdev->remove || (link_vmdq(vdev, pkts_burst[0]) == -1)) {
						while (tx_count)
							rte_pktmbuf_free(pkts_burst[--tx_count]);
					}
				}
				for (i = 0; i < tx_count; ++i) {
					virtio_tx_route(vdev, pkts_burst[i],
						vlan_tags[(uint16_t)dev->device_fh]);
				}
			}

			/*move to the next device in the list*/
			dev_ll = dev_ll->next;
		}
	}

	return 0;
}

/*
 * Add an entry to a used linked list. A free entry must first be found
 * in the free linked list using get_data_ll_free_entry();
 */
static void
add_data_ll_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev)
{
	struct virtio_net_data_ll *ll = *ll_root_addr;

	/* Set next as NULL and use a compiler barrier to avoid reordering. */
	ll_dev->next = NULL;
	rte_compiler_barrier();

	/* If ll == NULL then this is the first device. */
	if (ll) {
		/* Increment to the tail of the linked list. */
		while ((ll->next != NULL) )
			ll = ll->next;

		ll->next = ll_dev;
	} else {
		*ll_root_addr = ll_dev;
	}
}

/*
 * Remove an entry from a used linked list. The entry must then be added to
 * the free linked list using put_data_ll_free_entry().
 */
static void
rm_data_ll_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev,
	struct virtio_net_data_ll *ll_dev_last)
{
	struct virtio_net_data_ll *ll = *ll_root_addr;

	if (unlikely((ll == NULL) || (ll_dev == NULL)))
		return;

	if (ll_dev == ll)
		*ll_root_addr = ll_dev->next;
	else
		if (likely(ll_dev_last != NULL))
			ll_dev_last->next = ll_dev->next;
		else
			RTE_LOG(ERR, VHOST_CONFIG, "Remove entry form ll failed.\n");
}

/*
 * Find and return an entry from the free linked list.
 */
static struct virtio_net_data_ll *
get_data_ll_free_entry(struct virtio_net_data_ll **ll_root_addr)
{
	struct virtio_net_data_ll *ll_free = *ll_root_addr;
	struct virtio_net_data_ll *ll_dev;

	if (ll_free == NULL)
		return NULL;

	ll_dev = ll_free;
	*ll_root_addr = ll_free->next;

	return ll_dev;
}

/*
 * Place an entry back on to the free linked list.
 */
static void
put_data_ll_free_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev)
{
	struct virtio_net_data_ll *ll_free = *ll_root_addr;

	if (ll_dev == NULL)
		return;

	ll_dev->next = ll_free;
	*ll_root_addr = ll_dev;
}

/*
 * Creates a linked list of a given size.
 */
static struct virtio_net_data_ll *
alloc_data_ll(uint32_t size)
{
	struct virtio_net_data_ll *ll_new;
	uint32_t i;

	/* Malloc and then chain the linked list. */
	ll_new = malloc(size * sizeof(struct virtio_net_data_ll));
	if (ll_new == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG, "Failed to allocate memory for ll_new.\n");
		return NULL;
	}

	for (i = 0; i < size - 1; i++) {
		ll_new[i].vdev = NULL;
		ll_new[i].next = &ll_new[i+1];
	}
	ll_new[i].next = NULL;

	return ll_new;
}

/*
 * Create the main linked list along with each individual cores linked list. A used and a free list
 * are created to manage entries.
 */
static int
init_data_ll (void)
{
	int lcore;

	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll = malloc(sizeof(struct lcore_ll_info));
		if (lcore_info[lcore].lcore_ll == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG, "Failed to allocate memory for lcore_ll.\n");
			return -1;
		}

		lcore_info[lcore].lcore_ll->device_num = 0;
		lcore_info[lcore].lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;
		lcore_info[lcore].lcore_ll->ll_root_used = NULL;
		if (num_devices % num_switching_cores)
			lcore_info[lcore].lcore_ll->ll_root_free = alloc_data_ll((num_devices / num_switching_cores) + 1);
		else
			lcore_info[lcore].lcore_ll->ll_root_free = alloc_data_ll(num_devices / num_switching_cores);
	}

	/* Allocate devices up to a maximum of MAX_DEVICES. */
	ll_root_free = alloc_data_ll(MIN((num_devices), MAX_DEVICES));

	return 0;
}

/*
 * Remove a device from the specific data core linked list and from the main linked list. Synchonization
 * occurs through the use of the lcore dev_removal_flag. Device is made volatile here to avoid re-ordering
 * of dev->remove=1 which can cause an infinite loop in the rte_pause loop.
 */
static void
destroy_device (volatile struct virtio_net *dev)
{
	struct virtio_net_data_ll *ll_lcore_dev_cur;
	struct virtio_net_data_ll *ll_main_dev_cur;
	struct virtio_net_data_ll *ll_lcore_dev_last = NULL;
	struct virtio_net_data_ll *ll_main_dev_last = NULL;
	struct vhost_dev *vdev;
	int lcore;

	dev->flags &= ~VIRTIO_DEV_RUNNING;

	vdev = (struct vhost_dev *)dev->priv;
	/*set the remove flag. */
	vdev->remove = 1;
	while(vdev->ready != DEVICE_SAFE_REMOVE) {
		rte_pause();
	}

	/* Search for entry to be removed from lcore ll */
	ll_lcore_dev_cur = lcore_info[vdev->coreid].lcore_ll->ll_root_used;
	while (ll_lcore_dev_cur != NULL) {
		if (ll_lcore_dev_cur->vdev == vdev) {
			break;
		} else {
			ll_lcore_dev_last = ll_lcore_dev_cur;
			ll_lcore_dev_cur = ll_lcore_dev_cur->next;
		}
	}

	if (ll_lcore_dev_cur == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find the dev to be destroy.\n",
			dev->device_fh);
		return;
	}

	/* Search for entry to be removed from main ll */
	ll_main_dev_cur = ll_root_used;
	ll_main_dev_last = NULL;
	while (ll_main_dev_cur != NULL) {
		if (ll_main_dev_cur->vdev == vdev) {
			break;
		} else {
			ll_main_dev_last = ll_main_dev_cur;
			ll_main_dev_cur = ll_main_dev_cur->next;
		}
	}

	/* Remove entries from the lcore and main ll. */
	rm_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used, ll_lcore_dev_cur, ll_lcore_dev_last);
	rm_data_ll_entry(&ll_root_used, ll_main_dev_cur, ll_main_dev_last);

	/* Set the dev_removal_flag on each lcore. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll->dev_removal_flag = REQUEST_DEV_REMOVAL;
	}

	/*
	 * Once each core has set the dev_removal_flag to ACK_DEV_REMOVAL we can be sure that
	 * they can no longer access the device removed from the linked lists and that the devices
	 * are no longer in use.
	 */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		while (lcore_info[lcore].lcore_ll->dev_removal_flag != ACK_DEV_REMOVAL) {
			rte_pause();
		}
	}

	/* Add the entries back to the lcore and main free ll.*/
	put_data_ll_free_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_free, ll_lcore_dev_cur);
	put_data_ll_free_entry(&ll_root_free, ll_main_dev_cur);

	/* Decrement number of device on the lcore. */
	lcore_info[vdev->coreid].lcore_ll->device_num--;

	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been removed from data core\n", dev->device_fh);

	rte_free(vdev);

}

/*
 * A new device is added to a data core. First the device is added to the main linked list
 * and the allocated to a specific data core.
 */
static int
new_device (struct virtio_net *dev)
{
	struct virtio_net_data_ll *ll_dev;
	int lcore, core_add = 0;
	uint32_t device_num_min = num_devices;
	struct vhost_dev *vdev;

	vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
	if (vdev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Couldn't allocate memory for vhost dev\n",
			dev->device_fh);
		return -1;
	}
	vdev->dev = dev;
	dev->priv = vdev;

	/* Add device to main ll */
	ll_dev = get_data_ll_free_entry(&ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") No free entry found in linked list. Device limit "
			"of %d devices per core has been reached\n",
			dev->device_fh, num_devices);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	add_data_ll_entry(&ll_root_used, ll_dev);
	vdev->vmdq_rx_q
		= dev->device_fh * queues_per_pool + vmdq_queue_base;

	/*reset ready flag*/
	vdev->ready = DEVICE_MAC_LEARNING;
	vdev->remove = 0;

	/* Find a suitable lcore to add the device. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (lcore_info[lcore].lcore_ll->device_num < device_num_min) {
			device_num_min = lcore_info[lcore].lcore_ll->device_num;
			core_add = lcore;
		}
	}
	/* Add device to lcore ll */
	ll_dev = get_data_ll_free_entry(&lcore_info[core_add].lcore_ll->ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Failed to add device to data core\n", dev->device_fh);
		vdev->ready = DEVICE_SAFE_REMOVE;
		destroy_device(dev);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	vdev->coreid = core_add;

	add_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used, ll_dev);

	/* Initialize device stats */
	memset(&dev_statistics[dev->device_fh], 0, sizeof(struct device_statistics));

	/* Disable notifications. */
	rte_vhost_enable_guest_notification(dev, VIRTIO_RXQ, 0);
	rte_vhost_enable_guest_notification(dev, VIRTIO_TXQ, 0);
	lcore_info[vdev->coreid].lcore_ll->device_num++;
	dev->flags |= VIRTIO_DEV_RUNNING;

	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been added to data core %d\n", dev->device_fh, vdev->coreid);

	return 0;
}

/*
 * These callback allow devices to be added to the data core when configuration
 * has been fully complete.
 */
static const struct virtio_net_device_ops virtio_net_device_ops =
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
	struct virtio_net_data_ll *dev_ll;
	uint64_t tx_dropped, rx_dropped;
	uint64_t tx, tx_total, rx, rx_total;
	uint32_t device_fh;
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char top_left[] = { 27, '[', '1', ';', '1', 'H','\0' };

	while(1) {
		sleep(enable_stats);

		/* Clear screen and move to top left */
		printf("%s%s", clr, top_left);

		printf("\nDevice statistics ====================================");

		dev_ll = ll_root_used;
		while (dev_ll != NULL) {
			device_fh = (uint32_t)dev_ll->vdev->dev->device_fh;
			tx_total = dev_statistics[device_fh].tx_total;
			tx = dev_statistics[device_fh].tx;
			tx_dropped = tx_total - tx;
			rx_total = rte_atomic64_read(
				&dev_statistics[device_fh].rx_total_atomic);
			rx = rte_atomic64_read(
				&dev_statistics[device_fh].rx_atomic);
			rx_dropped = rx_total - rx;

			printf("\nStatistics for device %"PRIu32" ------------------------------"
					"\nTX total: 		%"PRIu64""
					"\nTX dropped: 		%"PRIu64""
					"\nTX successful: 		%"PRIu64""
					"\nRX total: 		%"PRIu64""
					"\nRX dropped: 		%"PRIu64""
					"\nRX successful: 		%"PRIu64"",
					device_fh,
					tx_total,
					tx_dropped,
					tx,
					rx_total,
					rx_dropped,
					rx);

			dev_ll = dev_ll->next;
		}
		printf("\n======================================================\n");
	}
}

/* When we receive a INT signal, unregister vhost driver */
static void
sigint_handler(__rte_unused int signum)
{
	/* Unregister vhost driver. */
	int ret = rte_vhost_driver_unregister((char *)&dev_basename);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "vhost driver unregister failure.\n");
	exit(0);
}

/*
 * Main function, does initialisation and calls the per-lcore functions. The CUSE
 * device is also registered here to handle the IOCTLs.
 */
int
main(int argc, char *argv[])
{
	unsigned lcore_id, core_id = 0;
	unsigned nb_ports, valid_num_ports;
	int ret;
	uint8_t portid;
	static pthread_t tid;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

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

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id ++)
		if (rte_lcore_is_enabled(lcore_id))
			lcore_ids[core_id ++] = lcore_id;

	if (rte_lcore_count() > RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE,"Not enough cores\n");

	/*set the number of swithcing cores available*/
	num_switching_cores = rte_lcore_count()-1;

	/* Get the number of physical ports. */
	nb_ports = rte_eth_dev_count();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

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

	/* Create the mbuf pool. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
		NUM_MBUFS_PER_PORT * valid_num_ports, MBUF_CACHE_SIZE,
		0, MBUF_DATA_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

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

	/* Initialise all linked lists. */
	if (init_data_ll() == -1)
		rte_exit(EXIT_FAILURE, "Failed to initialize linked list\n");

	/* Initialize device stats */
	memset(&dev_statistics, 0, sizeof(dev_statistics));

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
			RTE_LOG(ERR, VHOST_CONFIG,
				"Cannot set print-stats name\n");
	}

	/* Launch all data cores. */
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
		rte_eal_remote_launch(switch_worker, NULL, lcore_id);

	if (mergeable == 0)
		rte_vhost_feature_disable(1ULL << VIRTIO_NET_F_MRG_RXBUF);

	/* Register vhost(cuse or user) driver to handle vhost messages. */
	ret = rte_vhost_driver_register((char *)&dev_basename);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "vhost driver register failure.\n");

	rte_vhost_driver_callback_register(&virtio_net_device_ops);

	/* Start CUSE session. */
	rte_vhost_driver_session_start();
	return 0;

}
