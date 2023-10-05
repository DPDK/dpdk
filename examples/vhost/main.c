/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <ctype.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/virtio_net.h>
#include <linux/virtio_ring.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/param.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_net.h>
#include <rte_vhost.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_pause.h>
#include <rte_dmadev.h>
#include <rte_vhost_async.h>
#include <rte_thread.h>

#include "main.h"

#ifndef MAX_QUEUES
#define MAX_QUEUES 128
#endif

#define NUM_MBUFS_DEFAULT 0x24000

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

#define MBUF_CACHE_SIZE	128
#define MBUF_DATA_SIZE	RTE_MBUF_DEFAULT_BUF_SIZE

#define BURST_TX_DRAIN_US 100	/* TX drain every ~100us */

#define BURST_RX_WAIT_US 15	/* Defines how long we wait between retries on RX */
#define BURST_RX_RETRIES 4		/* Number of retries on RX. */

#define JUMBO_FRAME_MAX_SIZE    0x2600
#define MAX_MTU (JUMBO_FRAME_MAX_SIZE - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN))

/* State of virtio device. */
#define DEVICE_MAC_LEARNING 0
#define DEVICE_RX			1
#define DEVICE_SAFE_REMOVE	2

/* Configurable number of RX/TX ring descriptors */
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 512

#define INVALID_PORT_ID 0xFF
#define INVALID_DMA_ID -1

#define DMA_RING_SIZE 4096

#define ASYNC_ENQUEUE_VHOST 1
#define ASYNC_DEQUEUE_VHOST 2

/* number of mbufs in all pools - if specified on command-line. */
static int total_num_mbufs = NUM_MBUFS_DEFAULT;

struct dma_for_vhost dma_bind[RTE_MAX_VHOST_DEVICE];
int16_t dmas_id[RTE_DMADEV_DEFAULT_MAX];
static int dma_count;

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

static int builtin_net_driver;

/* Specify timeout (in useconds) between retries on RX. */
static uint32_t burst_rx_delay_time = BURST_RX_WAIT_US;
/* Specify the number of retries on RX. */
static uint32_t burst_rx_retry_num = BURST_RX_RETRIES;

/* Socket file paths. Can be set by user */
static char *socket_files;
static int nb_sockets;

static struct vhost_queue_ops vdev_queue_ops[RTE_MAX_VHOST_DEVICE];

/* empty VMDq configuration structure. Filled in programmatically */
static struct rte_eth_conf vmdq_conf_default = {
	.rxmode = {
		.mq_mode        = RTE_ETH_MQ_RX_VMDQ_ONLY,
		/*
		 * VLAN strip is necessary for 1G NIC such as I350,
		 * this fixes bug of ipv4 forwarding in guest can't
		 * forward packets from one virtio dev to another virtio dev.
		 */
		.offloads = RTE_ETH_RX_OFFLOAD_VLAN_STRIP,
	},

	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
		.offloads = (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
			     RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
			     RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
			     RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
			     RTE_ETH_TX_OFFLOAD_TCP_TSO),
	},
	.rx_adv_conf = {
		/*
		 * should be overridden separately in code with
		 * appropriate values
		 */
		.vmdq_rx_conf = {
			.nb_queue_pools = RTE_ETH_8_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
};


static unsigned lcore_ids[RTE_MAX_LCORE];
static uint16_t ports[RTE_MAX_ETHPORTS];
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
static struct rte_ether_addr vmdq_ports_eth_addr[RTE_MAX_ETHPORTS];

static struct vhost_dev_tailq_list vhost_dev_list =
	TAILQ_HEAD_INITIALIZER(vhost_dev_list);

static struct lcore_info lcore_info[RTE_MAX_LCORE];

/* Used for queueing bursts of TX packets. */
struct mbuf_table {
	unsigned len;
	unsigned txq_id;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct vhost_bufftable {
	uint32_t len;
	uint64_t pre_tsc;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

/* TX queue for each data core. */
struct mbuf_table lcore_tx_queue[RTE_MAX_LCORE];

/*
 * Vhost TX buffer for each data core.
 * Every data core maintains a TX buffer for every vhost device,
 * which is used for batch pkts enqueue for higher performance.
 */
struct vhost_bufftable *vhost_txbuff[RTE_MAX_LCORE * RTE_MAX_VHOST_DEVICE];

#define MBUF_TABLE_DRAIN_TSC	((rte_get_tsc_hz() + US_PER_S - 1) \
				 / US_PER_S * BURST_TX_DRAIN_US)

static int vid2socketid[RTE_MAX_VHOST_DEVICE];

static inline uint32_t
get_async_flag_by_socketid(int socketid)
{
	return dma_bind[socketid].async_flag;
}

static inline void
init_vid2socketid_array(int vid, int socketid)
{
	vid2socketid[vid] = socketid;
}

static inline bool
is_dma_configured(int16_t dev_id)
{
	int i;

	for (i = 0; i < dma_count; i++)
		if (dmas_id[i] == dev_id)
			return true;
	return false;
}

static inline int
open_dma(const char *value)
{
	struct dma_for_vhost *dma_info = dma_bind;
	char *input = strndup(value, strlen(value) + 1);
	char *addrs = input;
	char *ptrs[2];
	char *start, *end, *substr;
	int64_t socketid, vring_id;

	struct rte_dma_info info;
	struct rte_dma_conf dev_config = { .nb_vchans = 1 };
	struct rte_dma_vchan_conf qconf = {
		.direction = RTE_DMA_DIR_MEM_TO_MEM,
		.nb_desc = DMA_RING_SIZE
	};

	int dev_id;
	int ret = 0;
	uint16_t i = 0;
	char *dma_arg[RTE_MAX_VHOST_DEVICE];
	int args_nr;

	while (isblank(*addrs))
		addrs++;
	if (*addrs == '\0') {
		ret = -1;
		goto out;
	}

	/* process DMA devices within bracket. */
	addrs++;
	substr = strtok(addrs, ";]");
	if (!substr) {
		ret = -1;
		goto out;
	}

	args_nr = rte_strsplit(substr, strlen(substr), dma_arg, RTE_MAX_VHOST_DEVICE, ',');
	if (args_nr <= 0) {
		ret = -1;
		goto out;
	}

	while (i < args_nr) {
		char *arg_temp = dma_arg[i];
		char *txd, *rxd;
		uint8_t sub_nr;
		int async_flag;

		sub_nr = rte_strsplit(arg_temp, strlen(arg_temp), ptrs, 2, '@');
		if (sub_nr != 2) {
			ret = -1;
			goto out;
		}

		txd = strstr(ptrs[0], "txd");
		rxd = strstr(ptrs[0], "rxd");
		if (txd) {
			start = txd;
			vring_id = VIRTIO_RXQ;
			async_flag = ASYNC_ENQUEUE_VHOST;
		} else if (rxd) {
			start = rxd;
			vring_id = VIRTIO_TXQ;
			async_flag = ASYNC_DEQUEUE_VHOST;
		} else {
			ret = -1;
			goto out;
		}

		start += 3;
		socketid = strtol(start, &end, 0);
		if (end == start) {
			ret = -1;
			goto out;
		}

		dev_id = rte_dma_get_dev_id_by_name(ptrs[1]);
		if (dev_id < 0) {
			RTE_LOG(ERR, VHOST_CONFIG, "Fail to find DMA %s.\n", ptrs[1]);
			ret = -1;
			goto out;
		}

		/* DMA device is already configured, so skip */
		if (is_dma_configured(dev_id))
			goto done;

		if (rte_dma_info_get(dev_id, &info) != 0) {
			RTE_LOG(ERR, VHOST_CONFIG, "Error with rte_dma_info_get()\n");
			ret = -1;
			goto out;
		}

		if (info.max_vchans < 1) {
			RTE_LOG(ERR, VHOST_CONFIG, "No channels available on device %d\n", dev_id);
			ret = -1;
			goto out;
		}

		if (rte_dma_configure(dev_id, &dev_config) != 0) {
			RTE_LOG(ERR, VHOST_CONFIG, "Fail to configure DMA %d.\n", dev_id);
			ret = -1;
			goto out;
		}

		/* Check the max desc supported by DMA device */
		rte_dma_info_get(dev_id, &info);
		if (info.nb_vchans != 1) {
			RTE_LOG(ERR, VHOST_CONFIG, "No configured queues reported by DMA %d.\n",
					dev_id);
			ret = -1;
			goto out;
		}

		qconf.nb_desc = RTE_MIN(DMA_RING_SIZE, info.max_desc);

		if (rte_dma_vchan_setup(dev_id, 0, &qconf) != 0) {
			RTE_LOG(ERR, VHOST_CONFIG, "Fail to set up DMA %d.\n", dev_id);
			ret = -1;
			goto out;
		}

		if (rte_dma_start(dev_id) != 0) {
			RTE_LOG(ERR, VHOST_CONFIG, "Fail to start DMA %u.\n", dev_id);
			ret = -1;
			goto out;
		}

		dmas_id[dma_count++] = dev_id;

done:
		(dma_info + socketid)->dmas[vring_id].dev_id = dev_id;
		(dma_info + socketid)->async_flag |= async_flag;
		i++;
	}
out:
	free(input);
	return ret;
}

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
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint16_t port)
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
	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		RTE_LOG(ERR, VHOST_PORT,
			"Error during getting device (port %u) info: %s\n",
			port, strerror(-retval));

		return retval;
	}
	if (dev_info.max_vmdq_pools == 0) {
		RTE_LOG(ERR, VHOST_PORT, "Failed to get VMDq info.\n");
		return -1;
	}

	rxconf = &dev_info.default_rxconf;
	txconf = &dev_info.default_txconf;
	rxconf->rx_drop_en = 1;

	/*configure the number of supported virtio devices based on VMDQ limits */
	num_devices = dev_info.max_vmdq_pools;

	rx_ring_size = RX_DESC_DEFAULT;
	tx_ring_size = TX_DESC_DEFAULT;

	tx_rings = (uint16_t)rte_lcore_count();

	if (mergeable) {
		if (dev_info.max_mtu != UINT16_MAX && dev_info.max_rx_pktlen > dev_info.max_mtu)
			vmdq_conf_default.rxmode.mtu = dev_info.max_mtu;
		else
			vmdq_conf_default.rxmode.mtu = MAX_MTU;
	}

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

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rx_rings = (uint16_t)dev_info.max_rx_queues;
	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
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
	if (rx_ring_size > RX_DESC_DEFAULT) {
		RTE_LOG(ERR, VHOST_PORT, "Mbuf pool has an insufficient size "
			"for Rx queues on port %u.\n", port);
		return -1;
	}

	/* Setup the queues. */
	rxconf->offloads = port_conf.rxmode.offloads;
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
	txconf->offloads = port_conf.txmode.offloads;
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

	if (promiscuous) {
		retval = rte_eth_promiscuous_enable(port);
		if (retval != 0) {
			RTE_LOG(ERR, VHOST_PORT,
				"Failed to enable promiscuous mode on port %u: %s\n",
				port, rte_strerror(-retval));
			return retval;
		}
	}

	retval = rte_eth_macaddr_get(port, &vmdq_ports_eth_addr[port]);
	if (retval < 0) {
		RTE_LOG(ERR, VHOST_PORT,
			"Failed to get MAC address on port %u: %s\n",
			port, rte_strerror(-retval));
		return retval;
	}

	RTE_LOG(INFO, VHOST_PORT, "Max virtio devices supported: %u\n", num_devices);
	RTE_LOG(INFO, VHOST_PORT, "Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
		" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
		port, RTE_ETHER_ADDR_BYTES(&vmdq_ports_eth_addr[port]));

	return 0;
}

/*
 * Set socket file path.
 */
static int
us_vhost_parse_socket_path(const char *q_arg)
{
	char *old;

	/* parse number string */
	if (strnlen(q_arg, PATH_MAX) == PATH_MAX)
		return -1;

	old = socket_files;
	socket_files = realloc(socket_files, PATH_MAX * (nb_sockets + 1));
	if (socket_files == NULL) {
		free(old);
		return -1;
	}

	strlcpy(socket_files + nb_sockets * PATH_MAX, q_arg, PATH_MAX);
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
		return 0;

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
	"		--rx-retry [0|1] --mergeable [0|1] --stats [0-N]\n"
	"		--socket-file <path>\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--vm2vm [0|1|2]: disable/software(default)/hardware vm2vm comms\n"
	"		--rx-retry [0|1]: disable/enable(default) retries on Rx. Enable retry if destination queue is full\n"
	"		--rx-retry-delay [0-N]: timeout(in usecond) between retries on RX. This makes effect only if retries on rx enabled\n"
	"		--rx-retry-num [0-N]: the number of retries on rx. This makes effect only if retries on rx enabled\n"
	"		--mergeable [0|1]: disable(default)/enable RX mergeable buffers\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n"
	"		--socket-file: The path of the socket file.\n"
	"		--tx-csum [0|1]: disable/enable TX checksum offload.\n"
	"		--tso [0|1]: disable/enable TCP segment offload.\n"
	"		--client: register a vhost-user socket as client mode.\n"
	"		--dmas: register dma channel for specific vhost device.\n"
	"		--total-num-mbufs [0-N]: set the number of mbufs to be allocated in mbuf pools, the default value is 147456.\n"
	"		--builtin-net-driver: enable simple vhost-user net driver\n",
	       prgname);
}

enum {
#define OPT_VM2VM               "vm2vm"
	OPT_VM2VM_NUM = 256,
#define OPT_RX_RETRY            "rx-retry"
	OPT_RX_RETRY_NUM,
#define OPT_RX_RETRY_DELAY      "rx-retry-delay"
	OPT_RX_RETRY_DELAY_NUM,
#define OPT_RX_RETRY_NUMB       "rx-retry-num"
	OPT_RX_RETRY_NUMB_NUM,
#define OPT_MERGEABLE           "mergeable"
	OPT_MERGEABLE_NUM,
#define OPT_STATS               "stats"
	OPT_STATS_NUM,
#define OPT_SOCKET_FILE         "socket-file"
	OPT_SOCKET_FILE_NUM,
#define OPT_TX_CSUM             "tx-csum"
	OPT_TX_CSUM_NUM,
#define OPT_TSO                 "tso"
	OPT_TSO_NUM,
#define OPT_CLIENT              "client"
	OPT_CLIENT_NUM,
#define OPT_BUILTIN_NET_DRIVER  "builtin-net-driver"
	OPT_BUILTIN_NET_DRIVER_NUM,
#define OPT_DMAS                "dmas"
	OPT_DMAS_NUM,
#define OPT_NUM_MBUFS           "total-num-mbufs"
	OPT_NUM_MBUFS_NUM,
};

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
		{OPT_VM2VM, required_argument,
				NULL, OPT_VM2VM_NUM},
		{OPT_RX_RETRY, required_argument,
				NULL, OPT_RX_RETRY_NUM},
		{OPT_RX_RETRY_DELAY, required_argument,
				NULL, OPT_RX_RETRY_DELAY_NUM},
		{OPT_RX_RETRY_NUMB, required_argument,
				NULL, OPT_RX_RETRY_NUMB_NUM},
		{OPT_MERGEABLE, required_argument,
				NULL, OPT_MERGEABLE_NUM},
		{OPT_STATS, required_argument,
				NULL, OPT_STATS_NUM},
		{OPT_SOCKET_FILE, required_argument,
				NULL, OPT_SOCKET_FILE_NUM},
		{OPT_TX_CSUM, required_argument,
				NULL, OPT_TX_CSUM_NUM},
		{OPT_TSO, required_argument,
				NULL, OPT_TSO_NUM},
		{OPT_CLIENT, no_argument,
				NULL, OPT_CLIENT_NUM},
		{OPT_BUILTIN_NET_DRIVER, no_argument,
				NULL, OPT_BUILTIN_NET_DRIVER_NUM},
		{OPT_DMAS, required_argument,
				NULL, OPT_DMAS_NUM},
		{OPT_NUM_MBUFS, required_argument,
				NULL, OPT_NUM_MBUFS_NUM},
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
				RTE_ETH_VMDQ_ACCEPT_BROADCAST |
				RTE_ETH_VMDQ_ACCEPT_MULTICAST;
			break;

		case OPT_VM2VM_NUM:
			ret = parse_num_opt(optarg, (VM2VM_LAST - 1));
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for "
					"vm2vm [0|1|2]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			vm2vm_mode = (vm2vm_type)ret;
			break;

		case OPT_RX_RETRY_NUM:
			ret = parse_num_opt(optarg, 1);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry [0|1]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			enable_retry = ret;
			break;

		case OPT_TX_CSUM_NUM:
			ret = parse_num_opt(optarg, 1);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for tx-csum [0|1]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			enable_tx_csum = ret;
			break;

		case OPT_TSO_NUM:
			ret = parse_num_opt(optarg, 1);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for tso [0|1]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			enable_tso = ret;
			break;

		case OPT_RX_RETRY_DELAY_NUM:
			ret = parse_num_opt(optarg, INT32_MAX);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-delay [0-N]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			burst_rx_delay_time = ret;
			break;

		case OPT_RX_RETRY_NUMB_NUM:
			ret = parse_num_opt(optarg, INT32_MAX);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-num [0-N]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			burst_rx_retry_num = ret;
			break;

		case OPT_MERGEABLE_NUM:
			ret = parse_num_opt(optarg, 1);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for mergeable [0|1]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			mergeable = !!ret;
			break;

		case OPT_STATS_NUM:
			ret = parse_num_opt(optarg, INT32_MAX);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for stats [0..N]\n");
				us_vhost_usage(prgname);
				return -1;
			}
			enable_stats = ret;
			break;

		/* Set socket file path. */
		case OPT_SOCKET_FILE_NUM:
			if (us_vhost_parse_socket_path(optarg) == -1) {
				RTE_LOG(INFO, VHOST_CONFIG,
				"Invalid argument for socket name (Max %d characters)\n",
				PATH_MAX);
				us_vhost_usage(prgname);
				return -1;
			}
			break;

		case OPT_DMAS_NUM:
			if (open_dma(optarg) == -1) {
				RTE_LOG(INFO, VHOST_CONFIG,
					"Wrong DMA args\n");
				us_vhost_usage(prgname);
				return -1;
			}
			break;

		case OPT_NUM_MBUFS_NUM:
			ret = parse_num_opt(optarg, INT32_MAX);
			if (ret == -1) {
				RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for total-num-mbufs [0..N]\n");
				us_vhost_usage(prgname);
				return -1;
			}

			if (total_num_mbufs < ret)
				total_num_mbufs = ret;
			break;

		case OPT_CLIENT_NUM:
			client_mode = 1;
			break;

		case OPT_BUILTIN_NET_DRIVER_NUM:
			builtin_net_driver = 1;
			break;

		/* Invalid option - print options. */
		default:
			us_vhost_usage(prgname);
			return -1;
		}
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (enabled_port_mask & (1 << i))
			ports[num_ports++] = i;
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
		if (!rte_eth_dev_is_valid_port(ports[portid])) {
			RTE_LOG(INFO, VHOST_PORT,
				"\nSpecified port ID(%u) is not valid\n",
				ports[portid]);
			ports[portid] = INVALID_PORT_ID;
			valid_num_ports--;
		}
	}
	return valid_num_ports;
}

static __rte_always_inline struct vhost_dev *
find_vhost_dev(struct rte_ether_addr *mac)
{
	struct vhost_dev *vdev;

	TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
		if (vdev->ready == DEVICE_RX &&
		    rte_is_same_ether_addr(mac, &vdev->mac_address))
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
	struct rte_ether_hdr *pkt_hdr;
	int i, ret;

	/* Learn MAC address of guest device from packet */
	pkt_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	if (find_vhost_dev(&pkt_hdr->src_addr)) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) device is using a registered MAC!\n",
			vdev->vid);
		return -1;
	}

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		vdev->mac_address.addr_bytes[i] =
			pkt_hdr->src_addr.addr_bytes[i];

	/* vlan_tag currently uses the device_id. */
	vdev->vlan_tag = vlan_tags[vdev->vid];

	/* Print out VMDQ registration info. */
	RTE_LOG(INFO, VHOST_DATA,
		"(%d) mac " RTE_ETHER_ADDR_PRT_FMT " and vlan %d registered\n",
		vdev->vid, RTE_ETHER_ADDR_BYTES(&vdev->mac_address),
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

static inline void
free_pkts(struct rte_mbuf **pkts, uint16_t n)
{
	while (n--)
		rte_pktmbuf_free(pkts[n]);
}

static __rte_always_inline void
complete_async_pkts(struct vhost_dev *vdev)
{
	struct rte_mbuf *p_cpl[MAX_PKT_BURST];
	uint16_t complete_count;
	int16_t dma_id = dma_bind[vid2socketid[vdev->vid]].dmas[VIRTIO_RXQ].dev_id;

	complete_count = rte_vhost_poll_enqueue_completed(vdev->vid,
					VIRTIO_RXQ, p_cpl, MAX_PKT_BURST, dma_id, 0);
	if (complete_count)
		free_pkts(p_cpl, complete_count);

}

static __rte_always_inline void
sync_virtio_xmit(struct vhost_dev *dst_vdev, struct vhost_dev *src_vdev,
	    struct rte_mbuf *m)
{
	uint16_t ret;

	if (builtin_net_driver) {
		ret = vs_enqueue_pkts(dst_vdev, VIRTIO_RXQ, &m, 1);
	} else {
		ret = rte_vhost_enqueue_burst(dst_vdev->vid, VIRTIO_RXQ, &m, 1);
	}

	if (enable_stats) {
		__atomic_fetch_add(&dst_vdev->stats.rx_total_atomic, 1,
				__ATOMIC_SEQ_CST);
		__atomic_fetch_add(&dst_vdev->stats.rx_atomic, ret,
				__ATOMIC_SEQ_CST);
		src_vdev->stats.tx_total++;
		src_vdev->stats.tx += ret;
	}
}

static __rte_always_inline void
drain_vhost(struct vhost_dev *vdev)
{
	uint16_t ret;
	uint32_t buff_idx = rte_lcore_id() * RTE_MAX_VHOST_DEVICE + vdev->vid;
	uint16_t nr_xmit = vhost_txbuff[buff_idx]->len;
	struct rte_mbuf **m = vhost_txbuff[buff_idx]->m_table;

	ret = vdev_queue_ops[vdev->vid].enqueue_pkt_burst(vdev, VIRTIO_RXQ, m, nr_xmit);

	if (enable_stats) {
		__atomic_fetch_add(&vdev->stats.rx_total_atomic, nr_xmit,
				__ATOMIC_SEQ_CST);
		__atomic_fetch_add(&vdev->stats.rx_atomic, ret,
				__ATOMIC_SEQ_CST);
	}

	if (!dma_bind[vid2socketid[vdev->vid]].dmas[VIRTIO_RXQ].async_enabled) {
		free_pkts(m, nr_xmit);
	} else {
		uint16_t enqueue_fail = nr_xmit - ret;
		if (enqueue_fail > 0)
			free_pkts(&m[ret], enqueue_fail);
	}
}

static __rte_always_inline void
drain_vhost_table(void)
{
	uint16_t lcore_id = rte_lcore_id();
	struct vhost_bufftable *vhost_txq;
	struct vhost_dev *vdev;
	uint64_t cur_tsc;

	TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
		if (unlikely(vdev->remove == 1))
			continue;

		vhost_txq = vhost_txbuff[lcore_id * RTE_MAX_VHOST_DEVICE + vdev->vid];

		cur_tsc = rte_rdtsc();
		if (unlikely(cur_tsc - vhost_txq->pre_tsc
				> MBUF_TABLE_DRAIN_TSC)) {
			RTE_LOG_DP(DEBUG, VHOST_DATA,
				"Vhost TX queue drained after timeout with burst size %u\n",
				vhost_txq->len);
			drain_vhost(vdev);
			vhost_txq->len = 0;
			vhost_txq->pre_tsc = cur_tsc;
		}
	}
}

/*
 * Check if the packet destination MAC address is for a local device. If so then put
 * the packet on that devices RX queue. If not then return.
 */
static __rte_always_inline int
virtio_tx_local(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct rte_ether_hdr *pkt_hdr;
	struct vhost_dev *dst_vdev;
	struct vhost_bufftable *vhost_txq;
	uint16_t lcore_id = rte_lcore_id();
	pkt_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	dst_vdev = find_vhost_dev(&pkt_hdr->dst_addr);
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

	vhost_txq = vhost_txbuff[lcore_id * RTE_MAX_VHOST_DEVICE + dst_vdev->vid];
	vhost_txq->m_table[vhost_txq->len++] = m;

	if (enable_stats) {
		vdev->stats.tx_total++;
		vdev->stats.tx++;
	}

	if (unlikely(vhost_txq->len == MAX_PKT_BURST)) {
		drain_vhost(dst_vdev);
		vhost_txq->len = 0;
		vhost_txq->pre_tsc = rte_rdtsc();
	}
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
	struct rte_ether_hdr *pkt_hdr =
		rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	dst_vdev = find_vhost_dev(&pkt_hdr->dst_addr);
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
	*offset  = RTE_VLAN_HLEN;
	*vlan_tag = vlan_tags[vdev->vid];

	RTE_LOG_DP(DEBUG, VHOST_DATA,
		"(%d) TX: pkt to local VM device id: (%d), vlan tag: %u.\n",
		vdev->vid, dst_vdev->vid, *vlan_tag);

	return 0;
}

static void virtio_tx_offload(struct rte_mbuf *m)
{
	struct rte_net_hdr_lens hdr_lens;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	uint32_t ptype;
	void *l3_hdr;

	ptype = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);
	m->l2_len = hdr_lens.l2_len;
	m->l3_len = hdr_lens.l3_len;
	m->l4_len = hdr_lens.l4_len;

	l3_hdr = rte_pktmbuf_mtod_offset(m, void *, m->l2_len);
	tcp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_tcp_hdr *,
		m->l2_len + m->l3_len);

	m->ol_flags |= RTE_MBUF_F_TX_TCP_SEG;
	if ((ptype & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV4) {
		m->ol_flags |= RTE_MBUF_F_TX_IPV4;
		m->ol_flags |= RTE_MBUF_F_TX_IP_CKSUM;
		ipv4_hdr = l3_hdr;
		ipv4_hdr->hdr_checksum = 0;
		tcp_hdr->cksum = rte_ipv4_phdr_cksum(l3_hdr, m->ol_flags);
	} else { /* assume ethertype == RTE_ETHER_TYPE_IPV6 */
		m->ol_flags |= RTE_MBUF_F_TX_IPV6;
		tcp_hdr->cksum = rte_ipv6_phdr_cksum(l3_hdr, m->ol_flags);
	}
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
	struct rte_ether_hdr *nh;


	nh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (unlikely(rte_is_broadcast_ether_addr(&nh->dst_addr))) {
		struct vhost_dev *vdev2;

		TAILQ_FOREACH(vdev2, &vhost_dev_list, global_vdev_entry) {
			if (vdev2 != vdev)
				sync_virtio_xmit(vdev2, vdev, m);
		}
		goto queue2nic;
	}

	/*check if destination is local VM*/
	if ((vm2vm_mode == VM2VM_SOFTWARE) && (virtio_tx_local(vdev, m) == 0))
		return;

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

	nh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (unlikely(nh->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))) {
		/* Guest has inserted the vlan tag. */
		struct rte_vlan_hdr *vh = (struct rte_vlan_hdr *) (nh + 1);
		uint16_t vlan_tag_be = rte_cpu_to_be_16(vlan_tag);
		if ((vm2vm_mode == VM2VM_HARDWARE) &&
			(vh->vlan_tci != vlan_tag_be))
			vh->vlan_tci = vlan_tag_be;
	} else {
		m->ol_flags |= RTE_MBUF_F_TX_VLAN;

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

	if (m->ol_flags & RTE_MBUF_F_RX_LRO)
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

uint16_t
async_enqueue_pkts(struct vhost_dev *dev, uint16_t queue_id,
		struct rte_mbuf **pkts, uint32_t rx_count)
{
	uint16_t enqueue_count;
	uint16_t dma_id = dma_bind[vid2socketid[dev->vid]].dmas[VIRTIO_RXQ].dev_id;

	complete_async_pkts(dev);
	enqueue_count = rte_vhost_submit_enqueue_burst(dev->vid, queue_id,
					pkts, rx_count, dma_id, 0);

	return enqueue_count;
}

uint16_t
sync_enqueue_pkts(struct vhost_dev *dev, uint16_t queue_id,
		struct rte_mbuf **pkts, uint32_t rx_count)
{
	return rte_vhost_enqueue_burst(dev->vid, queue_id, pkts, rx_count);
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

	enqueue_count = vdev_queue_ops[vdev->vid].enqueue_pkt_burst(vdev,
						VIRTIO_RXQ, pkts, rx_count);

	/* Retry if necessary */
	if (enable_retry && unlikely(enqueue_count < rx_count)) {
		uint32_t retry = 0;

		while (enqueue_count < rx_count && retry++ < burst_rx_retry_num) {
			rte_delay_us(burst_rx_delay_time);
			enqueue_count += vdev_queue_ops[vdev->vid].enqueue_pkt_burst(vdev,
							VIRTIO_RXQ, &pkts[enqueue_count],
							rx_count - enqueue_count);
		}
	}

	if (enable_stats) {
		__atomic_fetch_add(&vdev->stats.rx_total_atomic, rx_count,
				__ATOMIC_SEQ_CST);
		__atomic_fetch_add(&vdev->stats.rx_atomic, enqueue_count,
				__ATOMIC_SEQ_CST);
	}

	if (!dma_bind[vid2socketid[vdev->vid]].dmas[VIRTIO_RXQ].async_enabled) {
		free_pkts(pkts, rx_count);
	} else {
		uint16_t enqueue_fail = rx_count - enqueue_count;
		if (enqueue_fail > 0)
			free_pkts(&pkts[enqueue_count], enqueue_fail);
	}
}

uint16_t async_dequeue_pkts(struct vhost_dev *dev, uint16_t queue_id,
			    struct rte_mempool *mbuf_pool,
			    struct rte_mbuf **pkts, uint16_t count)
{
	int nr_inflight;
	uint16_t dequeue_count;
	int16_t dma_id = dma_bind[vid2socketid[dev->vid]].dmas[VIRTIO_TXQ].dev_id;

	dequeue_count = rte_vhost_async_try_dequeue_burst(dev->vid, queue_id,
			mbuf_pool, pkts, count, &nr_inflight, dma_id, 0);

	return dequeue_count;
}

uint16_t sync_dequeue_pkts(struct vhost_dev *dev, uint16_t queue_id,
			   struct rte_mempool *mbuf_pool,
			   struct rte_mbuf **pkts, uint16_t count)
{
	return rte_vhost_dequeue_burst(dev->vid, queue_id, mbuf_pool, pkts, count);
}

static __rte_always_inline void
drain_virtio_tx(struct vhost_dev *vdev)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	uint16_t count;
	uint16_t i;

	count = vdev_queue_ops[vdev->vid].dequeue_pkt_burst(vdev,
				VIRTIO_TXQ, mbuf_pool, pkts, MAX_PKT_BURST);

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

	RTE_LOG(INFO, VHOST_DATA, "Processing on Core %u started\n", lcore_id);

	tx_q = &lcore_tx_queue[lcore_id];
	for (i = 0; i < rte_lcore_count(); i++) {
		if (lcore_ids[i] == lcore_id) {
			tx_q->txq_id = i;
			break;
		}
	}

	while(1) {
		drain_mbuf_table(tx_q);
		drain_vhost_table();
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

static void
vhost_clear_queue_thread_unsafe(struct vhost_dev *vdev, uint16_t queue_id)
{
	uint16_t n_pkt = 0;
	int pkts_inflight;

	int16_t dma_id = dma_bind[vid2socketid[vdev->vid]].dmas[queue_id].dev_id;
	pkts_inflight = rte_vhost_async_get_inflight_thread_unsafe(vdev->vid, queue_id);

	struct rte_mbuf *m_cpl[pkts_inflight];

	while (pkts_inflight) {
		n_pkt = rte_vhost_clear_queue_thread_unsafe(vdev->vid, queue_id, m_cpl,
							pkts_inflight, dma_id, 0);
		free_pkts(m_cpl, n_pkt);
		pkts_inflight = rte_vhost_async_get_inflight_thread_unsafe(vdev->vid,
									queue_id);
	}
}

static void
vhost_clear_queue(struct vhost_dev *vdev, uint16_t queue_id)
{
	uint16_t n_pkt = 0;
	int pkts_inflight;

	int16_t dma_id = dma_bind[vid2socketid[vdev->vid]].dmas[queue_id].dev_id;
	pkts_inflight = rte_vhost_async_get_inflight(vdev->vid, queue_id);

	struct rte_mbuf *m_cpl[pkts_inflight];

	while (pkts_inflight) {
		n_pkt = rte_vhost_clear_queue(vdev->vid, queue_id, m_cpl,
						pkts_inflight, dma_id, 0);
		free_pkts(m_cpl, n_pkt);
		pkts_inflight = rte_vhost_async_get_inflight(vdev->vid, queue_id);
	}
}

/*
 * Remove a device from the specific data core linked list and from the
 * main linked list. Synchronization  occurs through the use of the
 * lcore dev_removal_flag. Device is made volatile here to avoid re-ordering
 * of dev->remove=1 which can cause an infinite loop in the rte_pause loop.
 */
static void
destroy_device(int vid)
{
	struct vhost_dev *vdev = NULL;
	int lcore;
	uint16_t i;

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

	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_free(vhost_txbuff[i * RTE_MAX_VHOST_DEVICE + vid]);

	if (builtin_net_driver)
		vs_vhost_net_remove(vdev);

	TAILQ_REMOVE(&lcore_info[vdev->coreid].vdev_list, vdev,
		     lcore_vdev_entry);
	TAILQ_REMOVE(&vhost_dev_list, vdev, global_vdev_entry);


	/* Set the dev_removal_flag on each lcore. */
	RTE_LCORE_FOREACH_WORKER(lcore)
		lcore_info[lcore].dev_removal_flag = REQUEST_DEV_REMOVAL;

	/*
	 * Once each core has set the dev_removal_flag to ACK_DEV_REMOVAL
	 * we can be sure that they can no longer access the device removed
	 * from the linked lists and that the devices are no longer in use.
	 */
	RTE_LCORE_FOREACH_WORKER(lcore) {
		while (lcore_info[lcore].dev_removal_flag != ACK_DEV_REMOVAL)
			rte_pause();
	}

	lcore_info[vdev->coreid].device_num--;

	RTE_LOG(INFO, VHOST_DATA,
		"(%d) device has been removed from data core\n",
		vdev->vid);

	if (dma_bind[vid].dmas[VIRTIO_RXQ].async_enabled) {
		vhost_clear_queue(vdev, VIRTIO_RXQ);
		rte_vhost_async_channel_unregister(vid, VIRTIO_RXQ);
		dma_bind[vid].dmas[VIRTIO_RXQ].async_enabled = false;
	}

	if (dma_bind[vid].dmas[VIRTIO_TXQ].async_enabled) {
		vhost_clear_queue(vdev, VIRTIO_TXQ);
		rte_vhost_async_channel_unregister(vid, VIRTIO_TXQ);
		dma_bind[vid].dmas[VIRTIO_TXQ].async_enabled = false;
	}

	rte_free(vdev);
}

static inline int
get_socketid_by_vid(int vid)
{
	int i;
	char ifname[PATH_MAX];
	rte_vhost_get_ifname(vid, ifname, sizeof(ifname));

	for (i = 0; i < nb_sockets; i++) {
		char *file = socket_files + i * PATH_MAX;
		if (strcmp(file, ifname) == 0)
			return i;
	}

	return -1;
}

static int
init_vhost_queue_ops(int vid)
{
	if (builtin_net_driver) {
		vdev_queue_ops[vid].enqueue_pkt_burst = builtin_enqueue_pkts;
		vdev_queue_ops[vid].dequeue_pkt_burst = builtin_dequeue_pkts;
	} else {
		if (dma_bind[vid2socketid[vid]].dmas[VIRTIO_RXQ].async_enabled)
			vdev_queue_ops[vid].enqueue_pkt_burst = async_enqueue_pkts;
		else
			vdev_queue_ops[vid].enqueue_pkt_burst = sync_enqueue_pkts;

		if (dma_bind[vid2socketid[vid]].dmas[VIRTIO_TXQ].async_enabled)
			vdev_queue_ops[vid].dequeue_pkt_burst = async_dequeue_pkts;
		else
			vdev_queue_ops[vid].dequeue_pkt_burst = sync_dequeue_pkts;
	}

	return 0;
}

static inline int
vhost_async_channel_register(int vid)
{
	int rx_ret = 0, tx_ret = 0;

	if (dma_bind[vid2socketid[vid]].dmas[VIRTIO_RXQ].dev_id != INVALID_DMA_ID) {
		rx_ret = rte_vhost_async_channel_register(vid, VIRTIO_RXQ);
		if (rx_ret == 0)
			dma_bind[vid2socketid[vid]].dmas[VIRTIO_RXQ].async_enabled = true;
	}

	if (dma_bind[vid2socketid[vid]].dmas[VIRTIO_TXQ].dev_id != INVALID_DMA_ID) {
		tx_ret = rte_vhost_async_channel_register(vid, VIRTIO_TXQ);
		if (tx_ret == 0)
			dma_bind[vid2socketid[vid]].dmas[VIRTIO_TXQ].async_enabled = true;
	}

	return rx_ret | tx_ret;
}



/*
 * A new device is added to a data core. First the device is added to the main linked list
 * and then allocated to a specific data core.
 */
static int
new_device(int vid)
{
	int lcore, core_add = 0;
	uint16_t i;
	uint32_t device_num_min = num_devices;
	struct vhost_dev *vdev;
	int ret;

	vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
	if (vdev == NULL) {
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) couldn't allocate memory for vhost dev\n",
			vid);
		return -1;
	}
	vdev->vid = vid;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		vhost_txbuff[i * RTE_MAX_VHOST_DEVICE + vid]
			= rte_zmalloc("vhost bufftable",
				sizeof(struct vhost_bufftable),
				RTE_CACHE_LINE_SIZE);

		if (vhost_txbuff[i * RTE_MAX_VHOST_DEVICE + vid] == NULL) {
			RTE_LOG(INFO, VHOST_DATA,
			  "(%d) couldn't allocate memory for vhost TX\n", vid);
			return -1;
		}
	}

	int socketid = get_socketid_by_vid(vid);
	if (socketid == -1)
		return -1;

	init_vid2socketid_array(vid, socketid);

	ret =  vhost_async_channel_register(vid);

	if (init_vhost_queue_ops(vid) != 0)
		return -1;

	if (builtin_net_driver)
		vs_vhost_net_setup(vdev);

	TAILQ_INSERT_TAIL(&vhost_dev_list, vdev, global_vdev_entry);
	vdev->vmdq_rx_q = vid * queues_per_pool + vmdq_queue_base;

	/*reset ready flag*/
	vdev->ready = DEVICE_MAC_LEARNING;
	vdev->remove = 0;

	/* Find a suitable lcore to add the device. */
	RTE_LCORE_FOREACH_WORKER(lcore) {
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

	return ret;
}

static int
vring_state_changed(int vid, uint16_t queue_id, int enable)
{
	struct vhost_dev *vdev = NULL;

	TAILQ_FOREACH(vdev, &vhost_dev_list, global_vdev_entry) {
		if (vdev->vid == vid)
			break;
	}
	if (!vdev)
		return -1;

	if (dma_bind[vid2socketid[vid]].dmas[queue_id].async_enabled) {
		if (!enable)
			vhost_clear_queue_thread_unsafe(vdev, queue_id);
	}

	return 0;
}

/*
 * These callback allow devices to be added to the data core when configuration
 * has been fully complete.
 */
static const struct rte_vhost_device_ops virtio_net_device_ops =
{
	.new_device =  new_device,
	.destroy_device = destroy_device,
	.vring_state_changed = vring_state_changed,
};

/*
 * This is a thread will wake up after a period to print stats if the user has
 * enabled them.
 */
static uint32_t
print_stats(__rte_unused void *arg)
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

			rx_total = __atomic_load_n(&vdev->stats.rx_total_atomic,
				__ATOMIC_SEQ_CST);
			rx         = __atomic_load_n(&vdev->stats.rx_atomic,
				__ATOMIC_SEQ_CST);
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

		fflush(stdout);
	}

	return 0;
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

static void
reset_dma(void)
{
	int i;

	for (i = 0; i < RTE_MAX_VHOST_DEVICE; i++) {
		int j;

		for (j = 0; j < RTE_MAX_QUEUES_PER_PORT * 2; j++) {
			dma_bind[i].dmas[j].dev_id = INVALID_DMA_ID;
			dma_bind[i].dmas[j].async_enabled = false;
		}
	}

	for (i = 0; i < RTE_DMADEV_DEFAULT_MAX; i++)
		dmas_id[i] = INVALID_DMA_ID;
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
	uint16_t portid;
	rte_thread_t tid;
	uint64_t flags = RTE_VHOST_USER_NET_COMPLIANT_OL_FLAGS;

	signal(SIGINT, sigint_handler);

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* initialize dma structures */
	reset_dma();

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
	nb_ports = rte_eth_dev_count_avail();

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
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", total_num_mbufs,
					    MBUF_CACHE_SIZE, 0, MBUF_DATA_SIZE,
					    rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	if (vm2vm_mode == VM2VM_HARDWARE) {
		/* Enable VT loop back to let L2 switch to do it. */
		vmdq_conf_default.rx_adv_conf.vmdq_rx_conf.enable_loop_back = 1;
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"Enable loop back for L2 switch in vmdq.\n");
	}

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
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
		ret = rte_thread_create_control(&tid, "dpdk-vhost-stat",
					print_stats, NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Cannot create dpdk-vhost-stat thread\n");
	}

	/* Launch all data cores. */
	RTE_LCORE_FOREACH_WORKER(lcore_id)
		rte_eal_remote_launch(switch_worker, NULL, lcore_id);

	if (client_mode)
		flags |= RTE_VHOST_USER_CLIENT;

	for (i = 0; i < dma_count; i++) {
		if (rte_vhost_async_dma_configure(dmas_id[i], 0) < 0) {
			RTE_LOG(ERR, VHOST_PORT, "Failed to configure DMA in vhost.\n");
			rte_exit(EXIT_FAILURE, "Cannot use given DMA device\n");
		}
	}

	/* Register vhost user driver to handle vhost messages. */
	for (i = 0; i < nb_sockets; i++) {
		char *file = socket_files + i * PATH_MAX;

		if (dma_count && get_async_flag_by_socketid(i) != 0)
			flags = flags | RTE_VHOST_USER_ASYNC_COPY;

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

	RTE_LCORE_FOREACH_WORKER(lcore_id)
		rte_eal_wait_lcore(lcore_id);

	for (i = 0; i < dma_count; i++) {
		if (rte_vhost_async_dma_unconfigure(dmas_id[i], 0) < 0) {
			RTE_LOG(ERR, VHOST_PORT,
				"Failed to unconfigure DMA %d in vhost.\n", dmas_id[i]);
			rte_exit(EXIT_FAILURE, "Cannot use given DMA device\n");
		}
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
