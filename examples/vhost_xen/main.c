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

#include "main.h"
#include "virtio-net.h"
#include "xen_vhost.h"

#define MAX_QUEUES 128

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

/*
 * Calculate the number of buffers needed per port
 */
#define NUM_MBUFS_PER_PORT ((MAX_QUEUES*RTE_TEST_RX_DESC_DEFAULT) +		\
							(num_switching_cores*MAX_PKT_BURST) +  			\
							(num_switching_cores*RTE_TEST_TX_DESC_DEFAULT) +\
							(num_switching_cores*MBUF_CACHE_SIZE))

#define MBUF_CACHE_SIZE 64

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /* Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /* Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /* Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /* Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /* Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /* Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST 32		/* Max burst size for RX/TX */
#define MAX_MRG_PKT_BURST 16	/* Max burst for merge buffers. Set to 1 due to performance issue. */
#define BURST_TX_DRAIN_US 100	/* TX drain every ~100us */

/* State of virtio device. */
#define DEVICE_NOT_READY     0
#define DEVICE_READY         1
#define DEVICE_SAFE_REMOVE   2

/* Config_core_flag status definitions. */
#define REQUEST_DEV_REMOVAL 1
#define ACK_DEV_REMOVAL 0

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512

#define INVALID_PORT_ID 0xFF

/* Max number of devices. Limited by vmdq. */
#define MAX_DEVICES 64

/* Size of buffers used for snprintfs. */
#define MAX_PRINT_BUFF 6072


/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64

/* Used to compare MAC addresses. */
#define MAC_ADDR_CMP 0xFFFFFFFFFFFF

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

/*Number of switching cores enabled*/
static uint32_t num_switching_cores = 0;

/* number of devices/queues to support*/
static uint32_t num_queues = 0;
uint32_t num_devices = 0;

/* Enable VM2VM communications. If this is disabled then the MAC address compare is skipped. */
static uint32_t enable_vm2vm = 1;
/* Enable stats. */
static uint32_t enable_stats = 0;

/* empty vmdq configuration structure. Filled in programatically */
static const struct rte_eth_conf vmdq_conf_default = {
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

/* Vlan header struct used to insert vlan tags on TX. */
struct vlan_ethhdr {
	unsigned char   h_dest[ETH_ALEN];
	unsigned char   h_source[ETH_ALEN];
	__be16          h_vlan_proto;
	__be16          h_vlan_TCI;
	__be16          h_vlan_encapsulated_proto;
};

/* Header lengths. */
#define VLAN_HLEN       4
#define VLAN_ETH_HLEN   18

/* Per-device statistics struct */
struct device_statistics {
	uint64_t tx_total;
	rte_atomic64_t rx_total;
	uint64_t tx;
	rte_atomic64_t rx;
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
	unsigned i;

	memset(&conf, 0, sizeof(conf));
	conf.nb_queue_pools = (enum rte_eth_nb_pools)num_devices;
	conf.nb_pool_maps = num_devices;

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
 * Validate the device number according to the max pool number gotten form dev_info
 * If the device number is invalid, give the error message and return -1.
 * Each device must have its own pool.
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
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_conf port_conf;
	uint16_t rx_rings, tx_rings = (uint16_t)rte_lcore_count();
	const uint16_t rx_ring_size = RTE_TEST_RX_DESC_DEFAULT, tx_ring_size = RTE_TEST_TX_DESC_DEFAULT;
	int retval;
	uint16_t q;

	/* The max pool number from dev_info will be used to validate the pool number specified in cmd line */
	rte_eth_dev_info_get (port, &dev_info);

	/*configure the number of supported virtio devices based on VMDQ limits */
	num_devices = dev_info.max_vmdq_pools;
	num_queues = dev_info.max_rx_queues;

	retval = validate_num_devices(MAX_DEVICES);
	if (retval < 0)
		return retval;

	/* Get port configuration. */
	retval = get_eth_conf(&port_conf, num_devices);
	if (retval < 0)
		return retval;

	if (port >= rte_eth_dev_count()) return -1;

	rx_rings = (uint16_t)num_queues,
	/* Configure ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	rte_eth_dev_info_get(port, &dev_info);
	rxconf = &dev_info.default_rxconf;
	rxconf->rx_drop_en = 1;
	/* Setup the queues. */
	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
						rte_eth_dev_socket_id(port), rxconf,
						mbuf_pool);
		if (retval < 0)
			return retval;
	}
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
						rte_eth_dev_socket_id(port),
						NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the device. */
	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

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
	RTE_LOG(INFO, VHOST_CONFIG, "%s [EAL options] -- -p PORTMASK --vm2vm [0|1] --stats [0-N] --nb-devices ND\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--vm2vm [0|1]: disable/enable(default) vm2vm comms\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n",
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
		{"stats", required_argument, NULL, 0},
		{NULL, 0, 0, 0}
	};

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:",long_option, &option_index)) != EOF) {
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

		case 0:
			/* Enable/disable vm2vm comms. */
			if (!strncmp(long_option[option_index].name, "vm2vm", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for vm2vm [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_vm2vm = ret;
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
 * Function to convert guest physical addresses to vhost virtual addresses. This
 * is used to convert virtio buffer addresses.
 */
static __rte_always_inline uint64_t
gpa_to_vva(struct virtio_net *dev, uint64_t guest_pa)
{
	struct virtio_memory_regions *region;
	uint32_t regionidx;
	uint64_t vhost_va = 0;

	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		region = &dev->mem->regions[regionidx];
		if ((guest_pa >= region->guest_phys_address) &&
			(guest_pa <= region->guest_phys_address_end)) {
			vhost_va = region->address_offset + guest_pa;
			break;
		}
	}
	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") GPA %p| VVA %p\n",
		dev->device_fh, (void*)(uintptr_t)guest_pa, (void*)(uintptr_t)vhost_va);

	return vhost_va;
}

/*
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtio device. A packet
 * count is returned to indicate the number of packets that were successfully
 * added to the RX queue.
 */
static __rte_always_inline uint32_t
virtio_dev_rx(struct virtio_net *dev, struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	struct vring_desc *desc;
	struct rte_mbuf *buff;
	/* The virtio_hdr is initialised to 0. */
	struct virtio_net_hdr_mrg_rxbuf virtio_hdr = {{0,0,0,0,0,0},0};
	uint64_t buff_addr = 0;
	uint64_t buff_hdr_addr = 0;
	uint32_t head[MAX_PKT_BURST], packet_len = 0;
	uint32_t head_idx, packet_success = 0;
	uint16_t avail_idx, res_cur_idx;
	uint16_t res_base_idx, res_end_idx;
	uint16_t free_entries;
	uint8_t success = 0;
	void *userdata;

	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") virtio_dev_rx()\n", dev->device_fh);
	vq = dev->virtqueue_rx;
	count = (count > MAX_PKT_BURST) ? MAX_PKT_BURST : count;
	/* As many data cores may want access to available buffers, they need to be reserved. */
	do {

		res_base_idx = vq->last_used_idx_res;

		avail_idx = *((volatile uint16_t *)&vq->avail->idx);

		free_entries = (avail_idx - res_base_idx);

		/*check that we have enough buffers*/
		if (unlikely(count > free_entries))
			count = free_entries;

		if (count == 0)
			return 0;

		res_end_idx = res_base_idx + count;
		/* vq->last_used_idx_res is atomically updated. */
		success = rte_atomic16_cmpset(&vq->last_used_idx_res, res_base_idx,
									res_end_idx);
	} while (unlikely(success == 0));
	res_cur_idx = res_base_idx;
	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") Current Index %d| End Index %d\n",
		dev->device_fh, res_cur_idx, res_end_idx);

	/* Prefetch available ring to retrieve indexes. */
	rte_prefetch0(&vq->avail->ring[res_cur_idx & (vq->size - 1)]);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (head_idx = 0; head_idx < count; head_idx++)
		head[head_idx] = vq->avail->ring[(res_cur_idx + head_idx) & (vq->size - 1)];

	/*Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[head[packet_success]]);

	while (res_cur_idx != res_end_idx) {
		/* Get descriptor from available ring */
		desc = &vq->desc[head[packet_success]];
		/* Prefetch descriptor address. */
		rte_prefetch0(desc);

		buff = pkts[packet_success];

		/* Convert from gpa to vva (guest physical addr -> vhost virtual addr) */
		buff_addr = gpa_to_vva(dev, desc->addr);
		/* Prefetch buffer address. */
		rte_prefetch0((void*)(uintptr_t)buff_addr);

		{
			/* Copy virtio_hdr to packet and increment buffer address */
			buff_hdr_addr = buff_addr;
			packet_len = rte_pktmbuf_data_len(buff) + vq->vhost_hlen;

			/*
			 * If the descriptors are chained the header and data are placed in
			 * separate buffers.
			 */
			if (desc->flags & VRING_DESC_F_NEXT) {
				desc->len = vq->vhost_hlen;
				desc = &vq->desc[desc->next];
				/* Buffer address translation. */
				buff_addr = gpa_to_vva(dev, desc->addr);
				desc->len = rte_pktmbuf_data_len(buff);
			} else {
				buff_addr += vq->vhost_hlen;
				desc->len = packet_len;
			}
		}

		/* Update used ring with desc information */
		vq->used->ring[res_cur_idx & (vq->size - 1)].id = head[packet_success];
		vq->used->ring[res_cur_idx & (vq->size - 1)].len = packet_len;

		/* Copy mbuf data to buffer */
		userdata = rte_pktmbuf_mtod(buff, void *);
		rte_memcpy((void *)(uintptr_t)buff_addr, userdata, rte_pktmbuf_data_len(buff));

		res_cur_idx++;
		packet_success++;

		/* mergeable is disabled then a header is required per buffer. */
		rte_memcpy((void *)(uintptr_t)buff_hdr_addr, (const void *)&virtio_hdr, vq->vhost_hlen);
		if (res_cur_idx < res_end_idx) {
			/* Prefetch descriptor index. */
			rte_prefetch0(&vq->desc[head[packet_success]]);
		}
	}

	rte_compiler_barrier();

	/* Wait until it's our turn to add our buffer to the used ring. */
	while (unlikely(vq->last_used_idx != res_base_idx))
		rte_pause();

	*(volatile uint16_t *)&vq->used->idx += count;

	vq->last_used_idx = res_end_idx;

	return count;
}

/*
 * Compares a packet destination MAC address to a device MAC address.
 */
static __rte_always_inline int
ether_addr_cmp(struct ether_addr *ea, struct ether_addr *eb)
{
	return ((*(uint64_t *)ea ^ *(uint64_t *)eb) & MAC_ADDR_CMP) == 0;
}

/*
 * This function registers mac along with a
 * vlan tag to a VMDQ.
 */
static int
link_vmdq(struct virtio_net *dev)
{
	int ret;
	struct virtio_net_data_ll *dev_ll;

	dev_ll = ll_root_used;

	while (dev_ll != NULL) {
		if ((dev != dev_ll->dev) && ether_addr_cmp(&dev->mac_address, &dev_ll->dev->mac_address)) {
			RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") WARNING: This device is using an existing MAC address and has not been registered.\n", dev->device_fh);
			return -1;
		}
		dev_ll = dev_ll->next;
	}

	/* vlan_tag currently uses the device_id. */
	dev->vlan_tag = vlan_tags[dev->device_fh];
	dev->vmdq_rx_q = dev->device_fh * (num_queues/num_devices);

	/* Print out VMDQ registration info. */
	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") MAC_ADDRESS %02x:%02x:%02x:%02x:%02x:%02x and VLAN_TAG %d registered\n",
		dev->device_fh,
		dev->mac_address.addr_bytes[0], dev->mac_address.addr_bytes[1],
		dev->mac_address.addr_bytes[2], dev->mac_address.addr_bytes[3],
		dev->mac_address.addr_bytes[4], dev->mac_address.addr_bytes[5],
		dev->vlan_tag);

	/* Register the MAC address. */
	ret = rte_eth_dev_mac_addr_add(ports[0], &dev->mac_address, (uint32_t)dev->device_fh);
	if (ret) {
		RTE_LOG(ERR, VHOST_DATA, "(%"PRIu64") Failed to add device MAC address to VMDQ\n",
										dev->device_fh);
		return -1;
	}

	/* Enable stripping of the vlan tag as we handle routing. */
	rte_eth_dev_set_vlan_strip_on_queue(ports[0], dev->vmdq_rx_q, 1);

	rte_compiler_barrier();
	/* Set device as ready for RX. */
	dev->ready = DEVICE_READY;

	return 0;
}

/*
 * Removes MAC address and vlan tag from VMDQ. Ensures that nothing is adding buffers to the RX
 * queue before disabling RX on the device.
 */
static inline void
unlink_vmdq(struct virtio_net *dev)
{
	unsigned i = 0;
	unsigned rx_count;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	if (dev->ready == DEVICE_READY) {
		/*clear MAC and VLAN settings*/
		rte_eth_dev_mac_addr_remove(ports[0], &dev->mac_address);
		for (i = 0; i < 6; i++)
			dev->mac_address.addr_bytes[i] = 0;

		dev->vlan_tag = 0;

		/*Clear out the receive buffers*/
		rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)dev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);

		while (rx_count) {
			for (i = 0; i < rx_count; i++)
				rte_pktmbuf_free(pkts_burst[i]);

			rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)dev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);
		}

		dev->ready = DEVICE_NOT_READY;
	}
}

/*
 * Check if the packet destination MAC address is for a local device. If so then put
 * the packet on that devices RX queue. If not then return.
 */
static __rte_always_inline unsigned
virtio_tx_local(struct virtio_net *dev, struct rte_mbuf *m)
{
	struct virtio_net_data_ll *dev_ll;
	struct ether_hdr *pkt_hdr;
	uint64_t ret = 0;

	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/*get the used devices list*/
	dev_ll = ll_root_used;

	while (dev_ll != NULL) {
		if (likely(dev_ll->dev->ready == DEVICE_READY) && ether_addr_cmp(&(pkt_hdr->d_addr),
				          &dev_ll->dev->mac_address)) {

			/* Drop the packet if the TX packet is destined for the TX device. */
			if (dev_ll->dev->device_fh == dev->device_fh) {
				RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
					"Source and destination MAC addresses are the same. "
					"Dropping packet.\n",
					dev_ll->dev->device_fh);
				return 0;
			}


			RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
				"MAC address is local\n", dev_ll->dev->device_fh);

			if (dev_ll->dev->remove) {
				/*drop the packet if the device is marked for removal*/
				RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") "
					"Device is marked for removal\n",
					dev_ll->dev->device_fh);
			} else {
				/*send the packet to the local virtio device*/
				ret = virtio_dev_rx(dev_ll->dev, &m, 1);
				if (enable_stats) {
					rte_atomic64_add(&dev_statistics[dev_ll->dev->device_fh].rx_total, 1);
					rte_atomic64_add(&dev_statistics[dev_ll->dev->device_fh].rx, ret);
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
 * This function routes the TX packet to the correct interface. This may be a local device
 * or the physical port.
 */
static __rte_always_inline void
virtio_tx_route(struct virtio_net* dev, struct rte_mbuf *m, struct rte_mempool *mbuf_pool, uint16_t vlan_tag)
{
	struct mbuf_table *tx_q;
	struct vlan_ethhdr *vlan_hdr;
	struct rte_mbuf **m_table;
	struct rte_mbuf *mbuf;
	unsigned len, ret;
	const uint16_t lcore_id = rte_lcore_id();

	/*check if destination is local VM*/
	if (enable_vm2vm && (virtio_tx_local(dev, m) == 0)) {
		return;
	}

	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") TX: "
		"MAC address is external\n", dev->device_fh);

	/*Add packet to the port tx queue*/
	tx_q = &lcore_tx_queue[lcore_id];
	len = tx_q->len;

	/* Allocate an mbuf and populate the structure. */
	mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if(!mbuf)
		return;

	mbuf->data_len = m->data_len + VLAN_HLEN;
	mbuf->pkt_len = mbuf->data_len;

	/* Copy ethernet header to mbuf. */
	rte_memcpy(rte_pktmbuf_mtod(mbuf, void*),
			rte_pktmbuf_mtod(m, const void*), ETH_HLEN);


	/* Setup vlan header. Bytes need to be re-ordered for network with htons()*/
	vlan_hdr = rte_pktmbuf_mtod(mbuf, struct vlan_ethhdr *);
	vlan_hdr->h_vlan_encapsulated_proto = vlan_hdr->h_vlan_proto;
	vlan_hdr->h_vlan_proto = htons(ETH_P_8021Q);
	vlan_hdr->h_vlan_TCI = htons(vlan_tag);

	/* Copy the remaining packet contents to the mbuf. */
	rte_memcpy(rte_pktmbuf_mtod_offset(mbuf, void *, VLAN_ETH_HLEN),
		rte_pktmbuf_mtod_offset(m, const void *, ETH_HLEN),
		(m->data_len - ETH_HLEN));
	tx_q->m_table[len] = mbuf;
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

static __rte_always_inline void
virtio_dev_tx(struct virtio_net* dev, struct rte_mempool *mbuf_pool)
{
	struct rte_mbuf m;
	struct vhost_virtqueue *vq;
	struct vring_desc *desc;
	uint64_t buff_addr = 0;
	uint32_t head[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i;
	uint16_t free_entries, packet_success = 0;
	uint16_t avail_idx;

	vq = dev->virtqueue_tx;
	avail_idx = *((volatile uint16_t *)&vq->avail->idx);

	/* If there are no available buffers then return. */
	if (vq->last_used_idx == avail_idx)
		return;

	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") virtio_dev_tx()\n",
		dev->device_fh);

	/* Prefetch available ring to retrieve head indexes. */
	rte_prefetch0(&vq->avail->ring[vq->last_used_idx & (vq->size - 1)]);

	/*get the number of free entries in the ring*/
	free_entries = avail_idx - vq->last_used_idx;
	free_entries = unlikely(free_entries < MAX_PKT_BURST) ? free_entries : MAX_PKT_BURST;

	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%" PRIu64 ") Buffers available %d\n",
		dev->device_fh, free_entries);
	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < free_entries; i++)
		head[i] = vq->avail->ring[(vq->last_used_idx + i) & (vq->size - 1)];

	/* Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[head[packet_success]]);

	while (packet_success < free_entries) {
		desc = &vq->desc[head[packet_success]];
		/* Prefetch descriptor address. */
		rte_prefetch0(desc);

		if (packet_success < (free_entries - 1)) {
			/* Prefetch descriptor index. */
			rte_prefetch0(&vq->desc[head[packet_success+1]]);
		}

		/* Update used index buffer information. */
		used_idx = vq->last_used_idx & (vq->size - 1);
		vq->used->ring[used_idx].id = head[packet_success];
		vq->used->ring[used_idx].len = 0;

		/* Discard first buffer as it is the virtio header */
		desc = &vq->desc[desc->next];

		/* Buffer address translation. */
		buff_addr = gpa_to_vva(dev, desc->addr);
		/* Prefetch buffer address. */
		rte_prefetch0((void*)(uintptr_t)buff_addr);

		/* Setup dummy mbuf. This is copied to a real mbuf if transmitted out the physical port. */
		m.data_len = desc->len;
		m.data_off = 0;
		m.nb_segs = 1;

		virtio_tx_route(dev, &m, mbuf_pool, 0);

		vq->last_used_idx++;
		packet_success++;
	}

	rte_compiler_barrier();
	vq->used->idx += packet_success;
	/* Kick guest if required. */
}

/*
 * This function is called by each data core. It handles all RX/TX registered with the
 * core. For TX the specific lcore linked list is used. For RX, MAC addresses are compared
 * with all devices in the main linked list.
 */
static int
switch_worker(__attribute__((unused)) void *arg)
{
	struct rte_mempool *mbuf_pool = arg;
	struct virtio_net *dev = NULL;
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

	RTE_LOG(INFO, VHOST_DATA, "Procesing on Core %u started \n", lcore_id);
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
				RTE_LOG_DP(DEBUG, VHOST_DATA,
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
			dev = dev_ll->dev;

			if (unlikely(dev->remove)) {
				dev_ll = dev_ll->next;
				unlink_vmdq(dev);
				dev->ready = DEVICE_SAFE_REMOVE;
				continue;
			}
			if (likely(dev->ready == DEVICE_READY)) {
				/*Handle guest RX*/
				rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)dev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);

				if (rx_count) {
					ret_count = virtio_dev_rx(dev, pkts_burst, rx_count);
					if (enable_stats) {
						rte_atomic64_add(&dev_statistics[dev_ll->dev->device_fh].rx_total, rx_count);
						rte_atomic64_add(&dev_statistics[dev_ll->dev->device_fh].rx, ret_count);
					}
					while (likely(rx_count)) {
						rx_count--;
						rte_pktmbuf_free_seg(pkts_burst[rx_count]);
					}

				}
			}

			if (likely(!dev->remove))
				/*Handle guest TX*/
				virtio_dev_tx(dev, mbuf_pool);

			/*move to the next device in the list*/
			dev_ll = dev_ll->next;
		}
	}

	return 0;
}

/*
 * Add an entry to a used linked list. A free entry must first be found in the free linked list
 * using get_data_ll_free_entry();
 */
static void
add_data_ll_entry(struct virtio_net_data_ll **ll_root_addr, struct virtio_net_data_ll *ll_dev)
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
 * Remove an entry from a used linked list. The entry must then be added to the free linked list
 * using put_data_ll_free_entry().
 */
static void
rm_data_ll_entry(struct virtio_net_data_ll **ll_root_addr, struct virtio_net_data_ll *ll_dev, struct virtio_net_data_ll *ll_dev_last)
{
	struct virtio_net_data_ll *ll = *ll_root_addr;

	if (ll_dev == ll)
		*ll_root_addr = ll_dev->next;
	else
		ll_dev_last->next = ll_dev->next;
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
put_data_ll_free_entry(struct virtio_net_data_ll **ll_root_addr, struct virtio_net_data_ll *ll_dev)
{
	struct virtio_net_data_ll *ll_free = *ll_root_addr;

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
		ll_new[i].dev = NULL;
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
 * Remove a device from the specific data core linked list and from the main linked list. The
 * rx/tx thread must be set the flag to indicate that it is safe to remove the device.
 * used.
 */
static void
destroy_device (volatile struct virtio_net *dev)
{
	struct virtio_net_data_ll *ll_lcore_dev_cur;
	struct virtio_net_data_ll *ll_main_dev_cur;
	struct virtio_net_data_ll *ll_lcore_dev_last = NULL;
	struct virtio_net_data_ll *ll_main_dev_last = NULL;
	int lcore;

	dev->flags &= ~VIRTIO_DEV_RUNNING;

	/*set the remove flag. */
	dev->remove = 1;

	while(dev->ready != DEVICE_SAFE_REMOVE) {
		rte_pause();
	}

	/* Search for entry to be removed from lcore ll */
	ll_lcore_dev_cur = lcore_info[dev->coreid].lcore_ll->ll_root_used;
	while (ll_lcore_dev_cur != NULL) {
		if (ll_lcore_dev_cur->dev == dev) {
			break;
		} else {
			ll_lcore_dev_last = ll_lcore_dev_cur;
			ll_lcore_dev_cur = ll_lcore_dev_cur->next;
		}
	}

	/* Search for entry to be removed from main ll */
	ll_main_dev_cur = ll_root_used;
	ll_main_dev_last = NULL;
	while (ll_main_dev_cur != NULL) {
		if (ll_main_dev_cur->dev == dev) {
			break;
		} else {
			ll_main_dev_last = ll_main_dev_cur;
			ll_main_dev_cur = ll_main_dev_cur->next;
		}
	}

	if (ll_lcore_dev_cur == NULL || ll_main_dev_cur == NULL) {
		RTE_LOG(ERR, XENHOST, "%s: could find device in per_cpu list or main_list\n", __func__);
		return;
	}

	/* Remove entries from the lcore and main ll. */
	rm_data_ll_entry(&lcore_info[ll_lcore_dev_cur->dev->coreid].lcore_ll->ll_root_used, ll_lcore_dev_cur, ll_lcore_dev_last);
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
	put_data_ll_free_entry(&lcore_info[ll_lcore_dev_cur->dev->coreid].lcore_ll->ll_root_free, ll_lcore_dev_cur);
	put_data_ll_free_entry(&ll_root_free, ll_main_dev_cur);

	/* Decrement number of device on the lcore. */
	lcore_info[ll_lcore_dev_cur->dev->coreid].lcore_ll->device_num--;

	RTE_LOG(INFO, VHOST_DATA, "  #####(%"PRIu64") Device has been removed from data core\n", dev->device_fh);
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

	/* Add device to main ll */
	ll_dev = get_data_ll_free_entry(&ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") No free entry found in linked list. Device limit "
			"of %d devices per core has been reached\n",
			dev->device_fh, num_devices);
		return -1;
	}
	ll_dev->dev = dev;
	add_data_ll_entry(&ll_root_used, ll_dev);

	/*reset ready flag*/
	dev->ready = DEVICE_NOT_READY;
	dev->remove = 0;

	/* Find a suitable lcore to add the device. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (lcore_info[lcore].lcore_ll->device_num < device_num_min) {
			device_num_min = lcore_info[lcore].lcore_ll->device_num;
			core_add = lcore;
		}
	}
	/* Add device to lcore ll */
	ll_dev->dev->coreid = core_add;
	ll_dev = get_data_ll_free_entry(&lcore_info[ll_dev->dev->coreid].lcore_ll->ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Failed to add device to data core\n", dev->device_fh);
		destroy_device(dev);
		return -1;
	}
	ll_dev->dev = dev;
	add_data_ll_entry(&lcore_info[ll_dev->dev->coreid].lcore_ll->ll_root_used, ll_dev);

	/* Initialize device stats */
	memset(&dev_statistics[dev->device_fh], 0, sizeof(struct device_statistics));

	lcore_info[ll_dev->dev->coreid].lcore_ll->device_num++;
	dev->flags |= VIRTIO_DEV_RUNNING;

	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been added to data core %d\n", dev->device_fh, dev->coreid);

	link_vmdq(dev);

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
			device_fh = (uint32_t)dev_ll->dev->device_fh;
			tx_total = dev_statistics[device_fh].tx_total;
			tx = dev_statistics[device_fh].tx;
			tx_dropped = tx_total - tx;
			rx_total = rte_atomic64_read(&dev_statistics[device_fh].rx_total);
			rx = rte_atomic64_read(&dev_statistics[device_fh].rx);
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


int init_virtio_net(struct virtio_net_device_ops const * const ops);

/*
 * Main function, does initialisation and calls the per-lcore functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned lcore_id, core_id = 0;
	unsigned nb_ports, valid_num_ports;
	int ret;
	uint8_t portid;
	static pthread_t tid;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

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
		NUM_MBUFS_PER_PORT * valid_num_ports, MBUF_CACHE_SIZE, 0,
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			RTE_LOG(INFO, VHOST_PORT, "Skipping disabled port %d\n", portid);
			continue;
		}
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize network ports\n");
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

		/* Set thread_name for aid in debugging. */
		snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "print-xen-stats");
		ret = rte_thread_setname(tid, thread_name);
		if (ret != 0)
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"Cannot set print-stats name\n");
	}

	/* Launch all data cores. */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(switch_worker, mbuf_pool, lcore_id);
	}

	init_virtio_xen(&virtio_net_device_ops);

	virtio_monitor_loop();
	return 0;
}
