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
#include <rte_vhost.h>
#include <rte_pause.h>

#include "main.h"
#include "vxlan.h"
#include "vxlan_setup.h"

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

/**
 * Calculate the number of buffers needed per port
 */
#define NUM_MBUFS_PER_PORT ((MAX_QUEUES * RTE_TEST_RX_DESC_DEFAULT) +\
				(nb_switching_cores * MAX_PKT_BURST) +\
				(nb_switching_cores * \
				RTE_TEST_TX_DESC_DEFAULT) +\
				(nb_switching_cores * MBUF_CACHE_SIZE))

#define MBUF_CACHE_SIZE 128
#define MBUF_DATA_SIZE RTE_MBUF_DEFAULT_BUF_SIZE

#define MAX_PKT_BURST 32	/* Max burst size for RX/TX */
#define BURST_TX_DRAIN_US 100	/* TX drain every ~100us */

/* Defines how long we wait between retries on RX */
#define BURST_RX_WAIT_US 15

#define BURST_RX_RETRIES 4	/* Number of retries on RX. */

#define JUMBO_FRAME_MAX_SIZE    0x2600

/* State of virtio device. */
#define DEVICE_MAC_LEARNING 0
#define DEVICE_RX	    1
#define DEVICE_SAFE_REMOVE  2

/* Config_core_flag status definitions. */
#define REQUEST_DEV_REMOVAL 1
#define ACK_DEV_REMOVAL     0

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 512

/* Get first 4 bytes in mbuf headroom. */
#define MBUF_HEADROOM_UINT32(mbuf) (*(uint32_t *)((uint8_t *)(mbuf) \
		+ sizeof(struct rte_mbuf)))

#define INVALID_PORT_ID 0xFF

/* Size of buffers used for snprintfs. */
#define MAX_PRINT_BUFF 6072

/* Maximum character device basename size. */
#define MAX_BASENAME_SZ 20

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64

/* Used to compare MAC addresses. */
#define MAC_ADDR_CMP 0xFFFFFFFFFFFFULL

#define CMD_LINE_OPT_NB_DEVICES "nb-devices"
#define CMD_LINE_OPT_UDP_PORT "udp-port"
#define CMD_LINE_OPT_TX_CHECKSUM "tx-checksum"
#define CMD_LINE_OPT_TSO_SEGSZ "tso-segsz"
#define CMD_LINE_OPT_FILTER_TYPE "filter-type"
#define CMD_LINE_OPT_ENCAP "encap"
#define CMD_LINE_OPT_DECAP "decap"
#define CMD_LINE_OPT_RX_RETRY "rx-retry"
#define CMD_LINE_OPT_RX_RETRY_DELAY "rx-retry-delay"
#define CMD_LINE_OPT_RX_RETRY_NUM "rx-retry-num"
#define CMD_LINE_OPT_STATS "stats"
#define CMD_LINE_OPT_DEV_BASENAME "dev-basename"

/* mask of enabled ports */
static uint32_t enabled_port_mask;

/*Number of switching cores enabled*/
static uint32_t nb_switching_cores;

/* number of devices/queues to support*/
uint16_t nb_devices = 2;

/* max ring descriptor, ixgbe, i40e, e1000 all are 4096. */
#define MAX_RING_DESC 4096

struct vpool {
	struct rte_mempool *pool;
	struct rte_ring *ring;
	uint32_t buf_size;
} vpool_array[MAX_QUEUES+MAX_QUEUES];

/* UDP tunneling port */
uint16_t udp_port = 4789;

/* enable/disable inner TX checksum */
uint8_t tx_checksum = 0;

/* TCP segment size */
uint16_t tso_segsz = 0;

/* enable/disable decapsulation */
uint8_t rx_decap = 1;

/* enable/disable encapsulation */
uint8_t tx_encap = 1;

/* RX filter type for tunneling packet */
uint8_t filter_idx = 1;

/* overlay packet operation */
struct ol_switch_ops overlay_options = {
	.port_configure = vxlan_port_init,
	.tunnel_setup = vxlan_link,
	.tunnel_destroy = vxlan_unlink,
	.tx_handle = vxlan_tx_pkts,
	.rx_handle = vxlan_rx_pkts,
	.param_handle = NULL,
};

/* Enable stats. */
uint32_t enable_stats = 0;
/* Enable retries on RX. */
static uint32_t enable_retry = 1;
/* Specify timeout (in useconds) between retries on RX. */
static uint32_t burst_rx_delay_time = BURST_RX_WAIT_US;
/* Specify the number of retries on RX. */
static uint32_t burst_rx_retry_num = BURST_RX_RETRIES;

/* Character device basename. Can be set by user. */
static char dev_basename[MAX_BASENAME_SZ] = "vhost-net";

static unsigned lcore_ids[RTE_MAX_LCORE];
uint8_t ports[RTE_MAX_ETHPORTS];

static unsigned nb_ports; /**< The number of ports specified in command line */

/* ethernet addresses of ports */
struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

/* heads for the main used and free linked lists for the data path. */
static struct virtio_net_data_ll *ll_root_used;
static struct virtio_net_data_ll *ll_root_free;

/**
 * Array of data core structures containing information on
 * individual core linked lists.
 */
static struct lcore_info lcore_info[RTE_MAX_LCORE];

/* Used for queueing bursts of TX packets. */
struct mbuf_table {
	unsigned len;
	unsigned txq_id;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

/* TX queue for each data core. */
struct mbuf_table lcore_tx_queue[RTE_MAX_LCORE];

struct device_statistics dev_statistics[MAX_DEVICES];

/**
 * Set character device basename.
 */
static int
us_vhost_parse_basename(const char *q_arg)
{
	/* parse number string */
	if (strlen(q_arg) >= MAX_BASENAME_SZ)
		return -1;
	else
		snprintf((char *)&dev_basename, MAX_BASENAME_SZ, "%s", q_arg);

	return 0;
}

/**
 * Parse the portmask provided at run time.
 */
static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

/**
 * Parse num options at run time.
 */
static int
parse_num_opt(const char *q_arg, uint32_t max_valid_value)
{
	char *end = NULL;
	unsigned long num;

	/* parse unsigned int string */
	num = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (num > max_valid_value)
		return -1;

	return num;
}

/**
 * Display usage
 */
static void
tep_termination_usage(const char *prgname)
{
	RTE_LOG(INFO, VHOST_CONFIG, "%s [EAL options] -- -p PORTMASK\n"
	"               --udp-port: UDP destination port for VXLAN packet\n"
	"		--nb-devices[1-64]: The number of virtIO device\n"
	"               --tx-checksum [0|1]: inner Tx checksum offload\n"
	"               --tso-segsz [0-N]: TCP segment size\n"
	"               --decap [0|1]: tunneling packet decapsulation\n"
	"               --encap [0|1]: tunneling packet encapsulation\n"
	"               --filter-type[1-3]: filter type for tunneling packet\n"
	"                   1: Inner MAC and tenent ID\n"
	"                   2: Inner MAC and VLAN, and tenent ID\n"
	"                   3: Outer MAC, Inner MAC and tenent ID\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--rx-retry [0|1]: disable/enable(default) retries on rx."
	"		 Enable retry if destintation queue is full\n"
	"		--rx-retry-delay [0-N]: timeout(in usecond) between retries on RX."
	"		 This makes effect only if retries on rx enabled\n"
	"		--rx-retry-num [0-N]: the number of retries on rx."
	"		 This makes effect only if retries on rx enabled\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n"
	"		--dev-basename: The basename to be used for the character device.\n",
	       prgname);
}

/**
 * Parse the arguments given in the command line of the application.
 */
static int
tep_termination_parse_args(int argc, char **argv)
{
	int opt, ret;
	int option_index;
	unsigned i;
	const char *prgname = argv[0];
	static struct option long_option[] = {
		{CMD_LINE_OPT_NB_DEVICES, required_argument, NULL, 0},
		{CMD_LINE_OPT_UDP_PORT, required_argument, NULL, 0},
		{CMD_LINE_OPT_TX_CHECKSUM, required_argument, NULL, 0},
		{CMD_LINE_OPT_TSO_SEGSZ, required_argument, NULL, 0},
		{CMD_LINE_OPT_DECAP, required_argument, NULL, 0},
		{CMD_LINE_OPT_ENCAP, required_argument, NULL, 0},
		{CMD_LINE_OPT_FILTER_TYPE, required_argument, NULL, 0},
		{CMD_LINE_OPT_RX_RETRY, required_argument, NULL, 0},
		{CMD_LINE_OPT_RX_RETRY_DELAY, required_argument, NULL, 0},
		{CMD_LINE_OPT_RX_RETRY_NUM, required_argument, NULL, 0},
		{CMD_LINE_OPT_STATS, required_argument, NULL, 0},
		{CMD_LINE_OPT_DEV_BASENAME, required_argument, NULL, 0},
		{NULL, 0, 0, 0},
	};

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		/* Portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid portmask\n");
				tep_termination_usage(prgname);
				return -1;
			}
			break;
		case 0:
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_NB_DEVICES,
				sizeof(CMD_LINE_OPT_NB_DEVICES))) {
				ret = parse_num_opt(optarg, MAX_DEVICES);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for nb-devices [0-%d]\n",
					MAX_DEVICES);
					tep_termination_usage(prgname);
					return -1;
				} else
					nb_devices = ret;
			}

			/* Enable/disable retries on RX. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_RX_RETRY,
				sizeof(CMD_LINE_OPT_RX_RETRY))) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for rx-retry [0|1]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					enable_retry = ret;
			}

			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_TSO_SEGSZ,
				sizeof(CMD_LINE_OPT_TSO_SEGSZ))) {
				ret = parse_num_opt(optarg, INT16_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for TCP segment size [0-N]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					tso_segsz = ret;
			}

			if (!strncmp(long_option[option_index].name,
					CMD_LINE_OPT_UDP_PORT,
					sizeof(CMD_LINE_OPT_UDP_PORT))) {
				ret = parse_num_opt(optarg, INT16_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for UDP port [0-N]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					udp_port = ret;
			}

			/* Specify the retries delay time (in useconds) on RX.*/
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_RX_RETRY_DELAY,
				sizeof(CMD_LINE_OPT_RX_RETRY_DELAY))) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for rx-retry-delay [0-N]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					burst_rx_delay_time = ret;
			}

			/* Specify the retries number on RX. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_RX_RETRY_NUM,
				sizeof(CMD_LINE_OPT_RX_RETRY_NUM))) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for rx-retry-num [0-N]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					burst_rx_retry_num = ret;
			}

			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_TX_CHECKSUM,
				sizeof(CMD_LINE_OPT_TX_CHECKSUM))) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for tx-checksum [0|1]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					tx_checksum = ret;
			}

			if (!strncmp(long_option[option_index].name,
					CMD_LINE_OPT_FILTER_TYPE,
					sizeof(CMD_LINE_OPT_FILTER_TYPE))) {
				ret = parse_num_opt(optarg, 3);
				if ((ret == -1) || (ret == 0)) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for filter type [1-3]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					filter_idx = ret - 1;
			}

			/* Enable/disable encapsulation on RX. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_DECAP,
				sizeof(CMD_LINE_OPT_DECAP))) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for decap [0|1]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					rx_decap = ret;
			}

			/* Enable/disable encapsulation on TX. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_ENCAP,
				sizeof(CMD_LINE_OPT_ENCAP))) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for encap [0|1]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					tx_encap = ret;
			}

			/* Enable/disable stats. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_STATS,
				sizeof(CMD_LINE_OPT_STATS))) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
							"Invalid argument for stats [0..N]\n");
					tep_termination_usage(prgname);
					return -1;
				} else
					enable_stats = ret;
			}

			/* Set character device basename. */
			if (!strncmp(long_option[option_index].name,
				CMD_LINE_OPT_DEV_BASENAME,
				sizeof(CMD_LINE_OPT_DEV_BASENAME))) {
				if (us_vhost_parse_basename(optarg) == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for character "
						"device basename (Max %d characters)\n",
						MAX_BASENAME_SZ);
					tep_termination_usage(prgname);
					return -1;
				}
			}

			break;

			/* Invalid option - print options. */
		default:
			tep_termination_usage(prgname);
			return -1;
		}
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (enabled_port_mask & (1 << i))
			ports[nb_ports++] = (uint8_t)i;
	}

	if ((nb_ports ==  0) || (nb_ports > MAX_SUP_PORTS)) {
		RTE_LOG(INFO, VHOST_PORT, "Current enabled port number is %u,"
			"but only %u port can be enabled\n", nb_ports,
			MAX_SUP_PORTS);
		return -1;
	}

	return 0;
}

/**
 * Update the global var NB_PORTS and array PORTS
 * according to system ports number and return valid ports number
 */
static unsigned
check_ports_num(unsigned max_nb_ports)
{
	unsigned valid_nb_ports = nb_ports;
	unsigned portid;

	if (nb_ports > max_nb_ports) {
		RTE_LOG(INFO, VHOST_PORT, "\nSpecified port number(%u) "
			" exceeds total system port number(%u)\n",
			nb_ports, max_nb_ports);
		nb_ports = max_nb_ports;
	}

	for (portid = 0; portid < nb_ports; portid++) {
		if (ports[portid] >= max_nb_ports) {
			RTE_LOG(INFO, VHOST_PORT,
				"\nSpecified port ID(%u) exceeds max "
				" system port ID(%u)\n",
				ports[portid], (max_nb_ports - 1));
			ports[portid] = INVALID_PORT_ID;
			valid_nb_ports--;
		}
	}
	return valid_nb_ports;
}

/**
 * This function routes the TX packet to the correct interface. This may be a local device
 * or the physical port.
 */
static __rte_always_inline void
virtio_tx_route(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	struct mbuf_table *tx_q;
	struct rte_mbuf **m_table;
	unsigned len, ret = 0;
	const uint16_t lcore_id = rte_lcore_id();

	RTE_LOG_DP(DEBUG, VHOST_DATA, "(%d) TX: MAC address is external\n",
		vdev->vid);

	/* Add packet to the port tx queue */
	tx_q = &lcore_tx_queue[lcore_id];
	len = tx_q->len;

	tx_q->m_table[len] = m;
	len++;
	if (enable_stats) {
		dev_statistics[vdev->vid].tx_total++;
		dev_statistics[vdev->vid].tx++;
	}

	if (unlikely(len == MAX_PKT_BURST)) {
		m_table = (struct rte_mbuf **)tx_q->m_table;
		ret = overlay_options.tx_handle(ports[0],
			(uint16_t)tx_q->txq_id, m_table,
			(uint16_t)tx_q->len);

		/* Free any buffers not handled by TX and update
		 * the port stats.
		 */
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

/**
 * This function is called by each data core. It handles all
 * RX/TX registered with the core. For TX the specific lcore
 * linked list is used. For RX, MAC addresses are compared
 * with all devices in the main linked list.
 */
static int
switch_worker(__rte_unused void *arg)
{
	struct rte_mempool *mbuf_pool = arg;
	struct vhost_dev *vdev = NULL;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct virtio_net_data_ll *dev_ll;
	struct mbuf_table *tx_q;
	volatile struct lcore_ll_info *lcore_ll;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1)
					/ US_PER_S * BURST_TX_DRAIN_US;
	uint64_t prev_tsc, diff_tsc, cur_tsc, ret_count = 0;
	unsigned i, ret = 0;
	const uint16_t lcore_id = rte_lcore_id();
	const uint16_t num_cores = (uint16_t)rte_lcore_count();
	uint16_t rx_count = 0;
	uint16_t tx_count;
	uint32_t retry = 0;

	RTE_LOG(INFO, VHOST_DATA, "Procesing on Core %u started\n", lcore_id);
	lcore_ll = lcore_info[lcore_id].lcore_ll;
	prev_tsc = 0;

	tx_q = &lcore_tx_queue[lcore_id];
	for (i = 0; i < num_cores; i++) {
		if (lcore_ids[i] == lcore_id) {
			tx_q->txq_id = i;
			break;
		}
	}

	while (1) {
		cur_tsc = rte_rdtsc();
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			if (tx_q->len) {
				RTE_LOG_DP(DEBUG, VHOST_DATA, "TX queue drained after "
					"timeout with burst size %u\n",
					tx_q->len);
				ret = overlay_options.tx_handle(ports[0],
					(uint16_t)tx_q->txq_id,
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

		/**
		 * Inform the configuration core that we have exited
		 * the linked list and that no devices are
		 * in use if requested.
		 */
		if (lcore_ll->dev_removal_flag == REQUEST_DEV_REMOVAL)
			lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;

		/*
		 * Process devices
		 */
		dev_ll = lcore_ll->ll_root_used;

		while (dev_ll != NULL) {
			vdev = dev_ll->vdev;

			if (unlikely(vdev->remove)) {
				dev_ll = dev_ll->next;
				overlay_options.tunnel_destroy(vdev);
				vdev->ready = DEVICE_SAFE_REMOVE;
				continue;
			}
			if (likely(vdev->ready == DEVICE_RX)) {
				/* Handle guest RX */
				rx_count = rte_eth_rx_burst(ports[0],
					vdev->rx_q, pkts_burst, MAX_PKT_BURST);

				if (rx_count) {
					/*
					* Retry is enabled and the queue is
					* full then we wait and retry to
					* avoid packet loss. Here MAX_PKT_BURST
					* must be less than virtio queue size
					*/
					if (enable_retry && unlikely(rx_count >
						rte_vhost_avail_entries(vdev->vid, VIRTIO_RXQ))) {
						for (retry = 0; retry < burst_rx_retry_num;
							retry++) {
							rte_delay_us(burst_rx_delay_time);
							if (rx_count <= rte_vhost_avail_entries(vdev->vid, VIRTIO_RXQ))
								break;
						}
					}

					ret_count = overlay_options.rx_handle(vdev->vid, pkts_burst, rx_count);
					if (enable_stats) {
						rte_atomic64_add(
						&dev_statistics[vdev->vid].rx_total_atomic,
						rx_count);
						rte_atomic64_add(
						&dev_statistics[vdev->vid].rx_atomic, ret_count);
					}
					while (likely(rx_count)) {
						rx_count--;
						rte_pktmbuf_free(pkts_burst[rx_count]);
					}

				}
			}

			if (likely(!vdev->remove)) {
				/* Handle guest TX*/
				tx_count = rte_vhost_dequeue_burst(vdev->vid,
						VIRTIO_TXQ, mbuf_pool,
						pkts_burst, MAX_PKT_BURST);
				/* If this is the first received packet we need to learn the MAC */
				if (unlikely(vdev->ready == DEVICE_MAC_LEARNING) && tx_count) {
					if (vdev->remove ||
						(overlay_options.tunnel_setup(vdev, pkts_burst[0]) == -1)) {
						while (tx_count)
							rte_pktmbuf_free(pkts_burst[--tx_count]);
					}
				}
				while (tx_count)
					virtio_tx_route(vdev, pkts_burst[--tx_count]);
			}

			/* move to the next device in the list */
			dev_ll = dev_ll->next;
		}
	}

	return 0;
}

/**
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
		while (ll->next != NULL)
			ll = ll->next;

		ll->next = ll_dev;
	} else {
		*ll_root_addr = ll_dev;
	}
}

/**
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
			RTE_LOG(ERR, VHOST_CONFIG,
				"Remove entry form ll failed.\n");
}

/**
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

/**
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

/**
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
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to allocate memory for ll_new.\n");
		return NULL;
	}

	for (i = 0; i < size - 1; i++) {
		ll_new[i].vdev = NULL;
		ll_new[i].next = &ll_new[i+1];
	}
	ll_new[i].next = NULL;

	return ll_new;
}

/**
 * Create the main linked list along with each individual cores
 * linked list. A used and a free list are created to manage entries.
 */
static int
init_data_ll(void)
{
	int lcore;

	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll =
			malloc(sizeof(struct lcore_ll_info));
		if (lcore_info[lcore].lcore_ll == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to allocate memory for lcore_ll.\n");
			return -1;
		}

		lcore_info[lcore].lcore_ll->device_num = 0;
		lcore_info[lcore].lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;
		lcore_info[lcore].lcore_ll->ll_root_used = NULL;
		if (nb_devices % nb_switching_cores)
			lcore_info[lcore].lcore_ll->ll_root_free =
				alloc_data_ll((nb_devices / nb_switching_cores)
						+ 1);
		else
			lcore_info[lcore].lcore_ll->ll_root_free =
				alloc_data_ll(nb_devices / nb_switching_cores);
	}

	/* Allocate devices up to a maximum of MAX_DEVICES. */
	ll_root_free = alloc_data_ll(MIN((nb_devices), MAX_DEVICES));

	return 0;
}

/**
 * Remove a device from the specific data core linked list and
 * from the main linked list. Synchonization occurs through the use
 * of the lcore dev_removal_flag.
 */
static void
destroy_device(int vid)
{
	struct virtio_net_data_ll *ll_lcore_dev_cur;
	struct virtio_net_data_ll *ll_main_dev_cur;
	struct virtio_net_data_ll *ll_lcore_dev_last = NULL;
	struct virtio_net_data_ll *ll_main_dev_last = NULL;
	struct vhost_dev *vdev = NULL;
	int lcore;

	ll_main_dev_cur = ll_root_used;
	while (ll_main_dev_cur != NULL) {
		if (ll_main_dev_cur->vdev->vid == vid) {
			vdev = ll_main_dev_cur->vdev;
			break;
		}
	}
	if (!vdev)
		return;

	/* set the remove flag. */
	vdev->remove = 1;
	while (vdev->ready != DEVICE_SAFE_REMOVE)
		rte_pause();

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
			"(%d) Failed to find the dev to be destroy.\n", vid);
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
	rm_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used,
			ll_lcore_dev_cur, ll_lcore_dev_last);
	rm_data_ll_entry(&ll_root_used, ll_main_dev_cur, ll_main_dev_last);

	/* Set the dev_removal_flag on each lcore. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll->dev_removal_flag =
			REQUEST_DEV_REMOVAL;
	}

	/*
	 * Once each core has set the dev_removal_flag to
	 * ACK_DEV_REMOVAL we can be sure that they can no longer access
	 * the device removed from the linked lists and that the devices
	 * are no longer in use.
	 */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		while (lcore_info[lcore].lcore_ll->dev_removal_flag
			!= ACK_DEV_REMOVAL)
			rte_pause();
	}

	/* Add the entries back to the lcore and main free ll.*/
	put_data_ll_free_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_free,
				ll_lcore_dev_cur);
	put_data_ll_free_entry(&ll_root_free, ll_main_dev_cur);

	/* Decrement number of device on the lcore. */
	lcore_info[vdev->coreid].lcore_ll->device_num--;

	RTE_LOG(INFO, VHOST_DATA, "(%d) Device has been removed "
		"from data core\n", vid);

	rte_free(vdev);

}

/**
 * A new device is added to a data core. First the device is added
 * to the main linked list and the allocated to a specific data core.
 */
static int
new_device(int vid)
{
	struct virtio_net_data_ll *ll_dev;
	int lcore, core_add = 0;
	uint32_t device_num_min = nb_devices;
	struct vhost_dev *vdev;

	vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
	if (vdev == NULL) {
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) Couldn't allocate memory for vhost dev\n", vid);
		return -1;
	}
	vdev->vid = vid;
	/* Add device to main ll */
	ll_dev = get_data_ll_free_entry(&ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%d) No free entry found in"
			" linked list Device limit of %d devices per core"
			" has been reached\n", vid, nb_devices);
		if (vdev->regions_hpa)
			rte_free(vdev->regions_hpa);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	add_data_ll_entry(&ll_root_used, ll_dev);
	vdev->rx_q = vid;

	/* reset ready flag */
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
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) Failed to add device to data core\n",
			vid);
		vdev->ready = DEVICE_SAFE_REMOVE;
		destroy_device(vid);
		rte_free(vdev->regions_hpa);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	vdev->coreid = core_add;

	add_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used,
			ll_dev);

	/* Initialize device stats */
	memset(&dev_statistics[vid], 0,
		sizeof(struct device_statistics));

	/* Disable notifications. */
	rte_vhost_enable_guest_notification(vid, VIRTIO_RXQ, 0);
	rte_vhost_enable_guest_notification(vid, VIRTIO_TXQ, 0);
	lcore_info[vdev->coreid].lcore_ll->device_num++;

	RTE_LOG(INFO, VHOST_DATA, "(%d) Device has been added to data core %d\n",
		vid, vdev->coreid);

	return 0;
}

/**
 * These callback allow devices to be added to the data core when configuration
 * has been fully complete.
 */
static const struct vhost_device_ops virtio_net_device_ops = {
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

/**
 * This is a thread will wake up after a period to print stats if the user has
 * enabled them.
 */
static void
print_stats(void)
{
	struct virtio_net_data_ll *dev_ll;
	uint64_t tx_dropped, rx_dropped;
	uint64_t tx, tx_total, rx, rx_total, rx_ip_csum, rx_l4_csum;
	int vid;
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char top_left[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	while (1) {
		sleep(enable_stats);

		/* Clear screen and move to top left */
		printf("%s%s", clr, top_left);

		printf("\nDevice statistics ================================");

		dev_ll = ll_root_used;
		while (dev_ll != NULL) {
			vid = dev_ll->vdev->vid;
			tx_total = dev_statistics[vid].tx_total;
			tx = dev_statistics[vid].tx;
			tx_dropped = tx_total - tx;

			rx_total = rte_atomic64_read(
				&dev_statistics[vid].rx_total_atomic);
			rx = rte_atomic64_read(
				&dev_statistics[vid].rx_atomic);
			rx_dropped = rx_total - rx;
			rx_ip_csum = rte_atomic64_read(
				&dev_statistics[vid].rx_bad_ip_csum);
			rx_l4_csum = rte_atomic64_read(
				&dev_statistics[vid].rx_bad_l4_csum);

			printf("\nStatistics for device %d ----------"
					"\nTX total:		%"PRIu64""
					"\nTX dropped:		%"PRIu64""
					"\nTX successful:		%"PRIu64""
					"\nRX total:		%"PRIu64""
					"\nRX bad IP csum:      %"PRIu64""
					"\nRX bad L4 csum:      %"PRIu64""
					"\nRX dropped:		%"PRIu64""
					"\nRX successful:		%"PRIu64"",
					vid,
					tx_total,
					tx_dropped,
					tx,
					rx_total,
					rx_ip_csum,
					rx_l4_csum,
					rx_dropped,
					rx);

			dev_ll = dev_ll->next;
		}
		printf("\n================================================\n");
	}
}

/**
 * Main function, does initialisation and calls the per-lcore functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool = NULL;
	unsigned lcore_id, core_id = 0;
	unsigned nb_ports, valid_nb_ports;
	int ret;
	uint8_t portid;
	uint16_t queue_id;
	static pthread_t tid;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* parse app arguments */
	ret = tep_termination_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid argument\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		if (rte_lcore_is_enabled(lcore_id))
			lcore_ids[core_id++] = lcore_id;

	/* set the number of swithcing cores available */
	nb_switching_cores = rte_lcore_count()-1;

	/* Get the number of physical ports. */
	nb_ports = rte_eth_dev_count();

	/*
	 * Update the global var NB_PORTS and global array PORTS
	 * and get value of var VALID_NB_PORTS according to system ports number
	 */
	valid_nb_ports = check_ports_num(nb_ports);

	if ((valid_nb_ports == 0) || (valid_nb_ports > MAX_SUP_PORTS)) {
		rte_exit(EXIT_FAILURE, "Current enabled port number is %u,"
			"but only %u port can be enabled\n", nb_ports,
			MAX_SUP_PORTS);
	}
	/* Create the mbuf pool. */
	mbuf_pool = rte_pktmbuf_pool_create(
			"MBUF_POOL",
			NUM_MBUFS_PER_PORT * valid_nb_ports,
			MBUF_CACHE_SIZE,
			0,
			MBUF_DATA_SIZE,
			rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	for (queue_id = 0; queue_id < MAX_QUEUES + 1; queue_id++)
		vpool_array[queue_id].pool = mbuf_pool;

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			RTE_LOG(INFO, VHOST_PORT,
				"Skipping disabled port %d\n", portid);
			continue;
		}
		if (overlay_options.port_configure(portid, mbuf_pool) != 0)
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
			rte_exit(EXIT_FAILURE, "Cannot create print-stats thread\n");
		snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "print-stats");
		ret = rte_thread_setname(tid, thread_name);
		if (ret != 0)
			RTE_LOG(DEBUG, VHOST_CONFIG, "Cannot set print-stats name\n");
	}

	/* Launch all data cores. */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(switch_worker,
			mbuf_pool, lcore_id);
	}

	ret = rte_vhost_driver_register((char *)&dev_basename, 0);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "failed to register vhost driver.\n");

	rte_vhost_driver_disable_features(dev_basename,
		1ULL << VIRTIO_NET_F_MRG_RXBUF);

	ret = rte_vhost_driver_callback_register(dev_basename,
		&virtio_net_device_ops);
	if (ret != 0) {
		rte_exit(EXIT_FAILURE,
			"failed to register vhost driver callbacks.\n");
	}

	if (rte_vhost_driver_start(dev_basename) < 0) {
		rte_exit(EXIT_FAILURE,
			"failed to start vhost driver.\n");
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id)
		rte_eal_wait_lcore(lcore_id);

	return 0;
}
