/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <sys/mman.h>
#endif
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_string_fns.h>
#ifdef RTE_NET_IXGBE
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_LIB_PDUMP
#include <rte_pdump.h>
#endif
#include <rte_flow.h>
#ifdef RTE_LIB_METRICS
#include <rte_metrics.h>
#endif
#ifdef RTE_LIB_BITRATESTATS
#include <rte_bitrate.h>
#endif
#ifdef RTE_LIB_LATENCYSTATS
#include <rte_latencystats.h>
#endif
#ifdef RTE_EXEC_ENV_WINDOWS
#include <process.h>
#endif
#ifdef RTE_NET_BOND
#include <rte_eth_bond.h>
#endif

#include "testpmd.h"

#ifndef MAP_HUGETLB
/* FreeBSD may not have MAP_HUGETLB (in fact, it probably doesn't) */
#define HUGE_FLAG (0x40000)
#else
#define HUGE_FLAG MAP_HUGETLB
#endif

#ifndef MAP_HUGE_SHIFT
/* older kernels (or FreeBSD) will not have this define */
#define HUGE_SHIFT (26)
#else
#define HUGE_SHIFT MAP_HUGE_SHIFT
#endif

#define EXTMEM_HEAP_NAME "extmem"
/*
 * Zone size with the malloc overhead (max of debug and release variants)
 * must fit into the smallest supported hugepage size (2M),
 * so that an IOVA-contiguous zone of this size can always be allocated
 * if there are free 2M hugepages.
 */
#define EXTBUF_ZONE_SIZE (RTE_PGSIZE_2M - 4 * RTE_CACHE_LINE_SIZE)

uint16_t verbose_level = 0; /**< Silent by default. */
int testpmd_logtype; /**< Log type for testpmd logs */

/* use main core for command line ? */
uint8_t interactive = 0;
uint8_t auto_start = 0;
uint8_t tx_first;
char cmdline_filename[PATH_MAX] = {0};

/*
 * NUMA support configuration.
 * When set, the NUMA support attempts to dispatch the allocation of the
 * RX and TX memory rings, and of the DMA memory buffers (mbufs) for the
 * probed ports among the CPU sockets 0 and 1.
 * Otherwise, all memory is allocated from CPU socket 0.
 */
uint8_t numa_support = 1; /**< numa enabled by default */

/*
 * In UMA mode,all memory is allocated from socket 0 if --socket-num is
 * not configured.
 */
uint8_t socket_num = UMA_NO_CONFIG;

/*
 * Select mempool allocation type:
 * - native: use regular DPDK memory
 * - anon: use regular DPDK memory to create mempool, but populate using
 *         anonymous memory (may not be IOVA-contiguous)
 * - xmem: use externally allocated hugepage memory
 */
uint8_t mp_alloc_type = MP_ALLOC_NATIVE;

/*
 * Store specified sockets on which memory pool to be used by ports
 * is allocated.
 */
uint8_t port_numa[RTE_MAX_ETHPORTS];

/*
 * Store specified sockets on which RX ring to be used by ports
 * is allocated.
 */
uint8_t rxring_numa[RTE_MAX_ETHPORTS];

/*
 * Store specified sockets on which TX ring to be used by ports
 * is allocated.
 */
uint8_t txring_numa[RTE_MAX_ETHPORTS];

/*
 * Record the Ethernet address of peer target ports to which packets are
 * forwarded.
 * Must be instantiated with the ethernet addresses of peer traffic generator
 * ports.
 */
struct rte_ether_addr peer_eth_addrs[RTE_MAX_ETHPORTS];
portid_t nb_peer_eth_addrs = 0;

/*
 * Probed Target Environment.
 */
struct rte_port *ports;	       /**< For all probed ethernet ports. */
portid_t nb_ports;             /**< Number of probed ethernet ports. */
struct fwd_lcore **fwd_lcores; /**< For all probed logical cores. */
lcoreid_t nb_lcores;           /**< Number of probed logical cores. */

portid_t ports_ids[RTE_MAX_ETHPORTS]; /**< Store all port ids. */

/*
 * Test Forwarding Configuration.
 *    nb_fwd_lcores <= nb_cfg_lcores <= nb_lcores
 *    nb_fwd_ports  <= nb_cfg_ports  <= nb_ports
 */
lcoreid_t nb_cfg_lcores; /**< Number of configured logical cores. */
lcoreid_t nb_fwd_lcores; /**< Number of forwarding logical cores. */
portid_t  nb_cfg_ports;  /**< Number of configured ports. */
portid_t  nb_fwd_ports;  /**< Number of forwarding ports. */

unsigned int fwd_lcores_cpuids[RTE_MAX_LCORE]; /**< CPU ids configuration. */
portid_t fwd_ports_ids[RTE_MAX_ETHPORTS];      /**< Port ids configuration. */

struct fwd_stream **fwd_streams; /**< For each RX queue of each port. */
streamid_t nb_fwd_streams;       /**< Is equal to (nb_ports * nb_rxq). */

/*
 * Forwarding engines.
 */
struct fwd_engine * fwd_engines[] = {
	&io_fwd_engine,
	&mac_fwd_engine,
	&mac_swap_engine,
	&flow_gen_engine,
	&rx_only_engine,
	&tx_only_engine,
	&csum_fwd_engine,
	&icmp_echo_engine,
	&noisy_vnf_engine,
	&five_tuple_swap_fwd_engine,
#ifdef RTE_LIBRTE_IEEE1588
	&ieee1588_fwd_engine,
#endif
	&shared_rxq_engine,
	NULL,
};

struct rte_mempool *mempools[RTE_MAX_NUMA_NODES * MAX_SEGS_BUFFER_SPLIT];
uint16_t mempool_flags;

struct fwd_config cur_fwd_config;
struct fwd_engine *cur_fwd_eng = &io_fwd_engine; /**< IO mode by default. */
uint32_t retry_enabled;
uint32_t burst_tx_delay_time = BURST_TX_WAIT_US;
uint32_t burst_tx_retry_num = BURST_TX_RETRIES;

uint32_t mbuf_data_size_n = 1; /* Number of specified mbuf sizes. */
uint16_t mbuf_data_size[MAX_SEGS_BUFFER_SPLIT] = {
	DEFAULT_MBUF_DATA_SIZE
}; /**< Mbuf data space size. */
uint32_t param_total_num_mbufs = 0;  /**< number of mbufs in all pools - if
                                      * specified on command-line. */
uint16_t stats_period; /**< Period to show statistics (disabled by default) */

/** Extended statistics to show. */
struct rte_eth_xstat_name *xstats_display;

unsigned int xstats_display_num; /**< Size of extended statistics to show */

/*
 * In container, it cannot terminate the process which running with 'stats-period'
 * option. Set flag to exit stats period loop after received SIGINT/SIGTERM.
 */
uint8_t f_quit;
uint8_t cl_quit; /* Quit testpmd from cmdline. */

/*
 * Max Rx frame size, set by '--max-pkt-len' parameter.
 */
uint32_t max_rx_pkt_len;

/*
 * Configuration of packet segments used to scatter received packets
 * if some of split features is configured.
 */
uint16_t rx_pkt_seg_lengths[MAX_SEGS_BUFFER_SPLIT];
uint8_t  rx_pkt_nb_segs; /**< Number of segments to split */
uint16_t rx_pkt_seg_offsets[MAX_SEGS_BUFFER_SPLIT];
uint8_t  rx_pkt_nb_offs; /**< Number of specified offsets */

/*
 * Configuration of packet segments used by the "txonly" processing engine.
 */
uint16_t tx_pkt_length = TXONLY_DEF_PACKET_LEN; /**< TXONLY packet length. */
uint16_t tx_pkt_seg_lengths[RTE_MAX_SEGS_PER_PKT] = {
	TXONLY_DEF_PACKET_LEN,
};
uint8_t  tx_pkt_nb_segs = 1; /**< Number of segments in TXONLY packets */

enum tx_pkt_split tx_pkt_split = TX_PKT_SPLIT_OFF;
/**< Split policy for packets to TX. */

uint8_t txonly_multi_flow;
/**< Whether multiple flows are generated in TXONLY mode. */

uint32_t tx_pkt_times_inter;
/**< Timings for send scheduling in TXONLY mode, time between bursts. */

uint32_t tx_pkt_times_intra;
/**< Timings for send scheduling in TXONLY mode, time between packets. */

uint16_t nb_pkt_per_burst = DEF_PKT_BURST; /**< Number of packets per burst. */
uint16_t nb_pkt_flowgen_clones; /**< Number of Tx packet clones to send in flowgen mode. */
int nb_flows_flowgen = 1024; /**< Number of flows in flowgen mode. */
uint16_t mb_mempool_cache = DEF_MBUF_CACHE; /**< Size of mbuf mempool cache. */

/* current configuration is in DCB or not,0 means it is not in DCB mode */
uint8_t dcb_config = 0;

/*
 * Configurable number of RX/TX queues.
 */
queueid_t nb_hairpinq; /**< Number of hairpin queues per port. */
queueid_t nb_rxq = 1; /**< Number of RX queues per port. */
queueid_t nb_txq = 1; /**< Number of TX queues per port. */

/*
 * Configurable number of RX/TX ring descriptors.
 * Defaults are supplied by drivers via ethdev.
 */
#define RTE_TEST_RX_DESC_DEFAULT 0
#define RTE_TEST_TX_DESC_DEFAULT 0
uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT; /**< Number of RX descriptors. */
uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT; /**< Number of TX descriptors. */

#define RTE_PMD_PARAM_UNSET -1
/*
 * Configurable values of RX and TX ring threshold registers.
 */

int8_t rx_pthresh = RTE_PMD_PARAM_UNSET;
int8_t rx_hthresh = RTE_PMD_PARAM_UNSET;
int8_t rx_wthresh = RTE_PMD_PARAM_UNSET;

int8_t tx_pthresh = RTE_PMD_PARAM_UNSET;
int8_t tx_hthresh = RTE_PMD_PARAM_UNSET;
int8_t tx_wthresh = RTE_PMD_PARAM_UNSET;

/*
 * Configurable value of RX free threshold.
 */
int16_t rx_free_thresh = RTE_PMD_PARAM_UNSET;

/*
 * Configurable value of RX drop enable.
 */
int8_t rx_drop_en = RTE_PMD_PARAM_UNSET;

/*
 * Configurable value of TX free threshold.
 */
int16_t tx_free_thresh = RTE_PMD_PARAM_UNSET;

/*
 * Configurable value of TX RS bit threshold.
 */
int16_t tx_rs_thresh = RTE_PMD_PARAM_UNSET;

/*
 * Configurable value of buffered packets before sending.
 */
uint16_t noisy_tx_sw_bufsz;

/*
 * Configurable value of packet buffer timeout.
 */
uint16_t noisy_tx_sw_buf_flush_time;

/*
 * Configurable value for size of VNF internal memory area
 * used for simulating noisy neighbour behaviour
 */
uint64_t noisy_lkup_mem_sz;

/*
 * Configurable value of number of random writes done in
 * VNF simulation memory area.
 */
uint64_t noisy_lkup_num_writes;

/*
 * Configurable value of number of random reads done in
 * VNF simulation memory area.
 */
uint64_t noisy_lkup_num_reads;

/*
 * Configurable value of number of random reads/writes done in
 * VNF simulation memory area.
 */
uint64_t noisy_lkup_num_reads_writes;

/*
 * Receive Side Scaling (RSS) configuration.
 */
uint64_t rss_hf = RTE_ETH_RSS_IP; /* RSS IP by default. */

/*
 * Port topology configuration
 */
uint16_t port_topology = PORT_TOPOLOGY_PAIRED; /* Ports are paired by default */

/*
 * Avoids to flush all the RX streams before starts forwarding.
 */
uint8_t no_flush_rx = 0; /* flush by default */

/*
 * Flow API isolated mode.
 */
uint8_t flow_isolate_all;

/*
 * Avoids to check link status when starting/stopping a port.
 */
uint8_t no_link_check = 0; /* check by default */

/*
 * Don't automatically start all ports in interactive mode.
 */
uint8_t no_device_start = 0;

/*
 * Enable link status change notification
 */
uint8_t lsc_interrupt = 1; /* enabled by default */

/*
 * Enable device removal notification.
 */
uint8_t rmv_interrupt = 1; /* enabled by default */

uint8_t hot_plug = 0; /**< hotplug disabled by default. */

/* After attach, port setup is called on event or by iterator */
bool setup_on_probe_event = true;

/* Clear ptypes on port initialization. */
uint8_t clear_ptypes = true;

/* Hairpin ports configuration mode. */
uint16_t hairpin_mode;

/* Pretty printing of ethdev events */
static const char * const eth_event_desc[] = {
	[RTE_ETH_EVENT_UNKNOWN] = "unknown",
	[RTE_ETH_EVENT_INTR_LSC] = "link state change",
	[RTE_ETH_EVENT_QUEUE_STATE] = "queue state",
	[RTE_ETH_EVENT_INTR_RESET] = "reset",
	[RTE_ETH_EVENT_VF_MBOX] = "VF mbox",
	[RTE_ETH_EVENT_IPSEC] = "IPsec",
	[RTE_ETH_EVENT_MACSEC] = "MACsec",
	[RTE_ETH_EVENT_INTR_RMV] = "device removal",
	[RTE_ETH_EVENT_NEW] = "device probed",
	[RTE_ETH_EVENT_DESTROY] = "device released",
	[RTE_ETH_EVENT_FLOW_AGED] = "flow aged",
	[RTE_ETH_EVENT_MAX] = NULL,
};

/*
 * Display or mask ether events
 * Default to all events except VF_MBOX
 */
uint32_t event_print_mask = (UINT32_C(1) << RTE_ETH_EVENT_UNKNOWN) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_LSC) |
			    (UINT32_C(1) << RTE_ETH_EVENT_QUEUE_STATE) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_RESET) |
			    (UINT32_C(1) << RTE_ETH_EVENT_IPSEC) |
			    (UINT32_C(1) << RTE_ETH_EVENT_MACSEC) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_RMV) |
			    (UINT32_C(1) << RTE_ETH_EVENT_FLOW_AGED);
/*
 * Decide if all memory are locked for performance.
 */
int do_mlockall = 0;

/*
 * NIC bypass mode configuration options.
 */

#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
/* The NIC bypass watchdog timeout. */
uint32_t bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_OFF;
#endif


#ifdef RTE_LIB_LATENCYSTATS

/*
 * Set when latency stats is enabled in the commandline
 */
uint8_t latencystats_enabled;

/*
 * Lcore ID to service latency statistics.
 */
lcoreid_t latencystats_lcore_id = -1;

#endif

/*
 * Ethernet device configuration.
 */
struct rte_eth_rxmode rx_mode;

struct rte_eth_txmode tx_mode = {
	.offloads = RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE,
};

struct rte_eth_fdir_conf fdir_conf = {
	.mode = RTE_FDIR_MODE_NONE,
	.pballoc = RTE_ETH_FDIR_PBALLOC_64K,
	.status = RTE_FDIR_REPORT_STATUS,
	.mask = {
		.vlan_tci_mask = 0xFFEF,
		.ipv4_mask     = {
			.src_ip = 0xFFFFFFFF,
			.dst_ip = 0xFFFFFFFF,
		},
		.ipv6_mask     = {
			.src_ip = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
			.dst_ip = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		},
		.src_port_mask = 0xFFFF,
		.dst_port_mask = 0xFFFF,
		.mac_addr_byte_mask = 0xFF,
		.tunnel_type_mask = 1,
		.tunnel_id_mask = 0xFFFFFFFF,
	},
	.drop_queue = 127,
};

volatile int test_done = 1; /* stop packet forwarding when set to 1. */

/*
 * Display zero values by default for xstats
 */
uint8_t xstats_hide_zero;

/*
 * Measure of CPU cycles disabled by default
 */
uint8_t record_core_cycles;

/*
 * Display of RX and TX bursts disabled by default
 */
uint8_t record_burst_stats;

/*
 * Number of ports per shared Rx queue group, 0 disable.
 */
uint32_t rxq_share;

unsigned int num_sockets = 0;
unsigned int socket_ids[RTE_MAX_NUMA_NODES];

#ifdef RTE_LIB_BITRATESTATS
/* Bitrate statistics */
struct rte_stats_bitrates *bitrate_data;
lcoreid_t bitrate_lcore_id;
uint8_t bitrate_enabled;
#endif

#ifdef RTE_LIB_GRO
struct gro_status gro_ports[RTE_MAX_ETHPORTS];
uint8_t gro_flush_cycles = GRO_DEFAULT_FLUSH_CYCLES;
#endif

/*
 * hexadecimal bitmask of RX mq mode can be enabled.
 */
enum rte_eth_rx_mq_mode rx_mq_mode = RTE_ETH_MQ_RX_VMDQ_DCB_RSS;

/*
 * Used to set forced link speed
 */
uint32_t eth_link_speed;

/*
 * ID of the current process in multi-process, used to
 * configure the queues to be polled.
 */
int proc_id;

/*
 * Number of processes in multi-process, used to
 * configure the queues to be polled.
 */
unsigned int num_procs = 1;

static void
eth_rx_metadata_negotiate_mp(uint16_t port_id)
{
	uint64_t rx_meta_features = 0;
	int ret;

	if (!is_proc_primary())
		return;

	rx_meta_features |= RTE_ETH_RX_METADATA_USER_FLAG;
	rx_meta_features |= RTE_ETH_RX_METADATA_USER_MARK;
	rx_meta_features |= RTE_ETH_RX_METADATA_TUNNEL_ID;

	ret = rte_eth_rx_metadata_negotiate(port_id, &rx_meta_features);
	if (ret == 0) {
		if (!(rx_meta_features & RTE_ETH_RX_METADATA_USER_FLAG)) {
			TESTPMD_LOG(DEBUG, "Flow action FLAG will not affect Rx mbufs on port %u\n",
				    port_id);
		}

		if (!(rx_meta_features & RTE_ETH_RX_METADATA_USER_MARK)) {
			TESTPMD_LOG(DEBUG, "Flow action MARK will not affect Rx mbufs on port %u\n",
				    port_id);
		}

		if (!(rx_meta_features & RTE_ETH_RX_METADATA_TUNNEL_ID)) {
			TESTPMD_LOG(DEBUG, "Flow tunnel offload support might be limited or unavailable on port %u\n",
				    port_id);
		}
	} else if (ret != -ENOTSUP) {
		rte_exit(EXIT_FAILURE, "Error when negotiating Rx meta features on port %u: %s\n",
			 port_id, rte_strerror(-ret));
	}
}

static int
eth_dev_configure_mp(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	if (is_proc_primary())
		return rte_eth_dev_configure(port_id, nb_rx_q, nb_tx_q,
					dev_conf);
	return 0;
}

static int
change_bonding_slave_port_status(portid_t bond_pid, bool is_stop)
{
#ifdef RTE_NET_BOND

	portid_t slave_pids[RTE_MAX_ETHPORTS];
	struct rte_port *port;
	int num_slaves;
	portid_t slave_pid;
	int i;

	num_slaves = rte_eth_bond_slaves_get(bond_pid, slave_pids,
						RTE_MAX_ETHPORTS);
	if (num_slaves < 0) {
		fprintf(stderr, "Failed to get slave list for port = %u\n",
			bond_pid);
		return num_slaves;
	}

	for (i = 0; i < num_slaves; i++) {
		slave_pid = slave_pids[i];
		port = &ports[slave_pid];
		port->port_status =
			is_stop ? RTE_PORT_STOPPED : RTE_PORT_STARTED;
	}
#else
	RTE_SET_USED(bond_pid);
	RTE_SET_USED(is_stop);
#endif
	return 0;
}

static int
eth_dev_start_mp(uint16_t port_id)
{
	int ret;

	if (is_proc_primary()) {
		ret = rte_eth_dev_start(port_id);
		if (ret != 0)
			return ret;

		struct rte_port *port = &ports[port_id];

		/*
		 * Starting a bonded port also starts all slaves under the bonded
		 * device. So if this port is bond device, we need to modify the
		 * port status of these slaves.
		 */
		if (port->bond_flag == 1)
			return change_bonding_slave_port_status(port_id, false);
	}

	return 0;
}

static int
eth_dev_stop_mp(uint16_t port_id)
{
	int ret;

	if (is_proc_primary()) {
		ret = rte_eth_dev_stop(port_id);
		if (ret != 0)
			return ret;

		struct rte_port *port = &ports[port_id];

		/*
		 * Stopping a bonded port also stops all slaves under the bonded
		 * device. So if this port is bond device, we need to modify the
		 * port status of these slaves.
		 */
		if (port->bond_flag == 1)
			return change_bonding_slave_port_status(port_id, true);
	}

	return 0;
}

static void
mempool_free_mp(struct rte_mempool *mp)
{
	if (is_proc_primary())
		rte_mempool_free(mp);
}

static int
eth_dev_set_mtu_mp(uint16_t port_id, uint16_t mtu)
{
	if (is_proc_primary())
		return rte_eth_dev_set_mtu(port_id, mtu);

	return 0;
}

/* Forward function declarations */
static void setup_attached_port(portid_t pi);
static void check_all_ports_link_status(uint32_t port_mask);
static int eth_event_callback(portid_t port_id,
			      enum rte_eth_event_type type,
			      void *param, void *ret_param);
static void dev_event_callback(const char *device_name,
				enum rte_dev_event_type type,
				void *param);
static void fill_xstats_display_info(void);

/*
 * Check if all the ports are started.
 * If yes, return positive value. If not, return zero.
 */
static int all_ports_started(void);

#ifdef RTE_LIB_GSO
struct gso_status gso_ports[RTE_MAX_ETHPORTS];
uint16_t gso_max_segment_size = RTE_ETHER_MAX_LEN - RTE_ETHER_CRC_LEN;
#endif

/* Holds the registered mbuf dynamic flags names. */
char dynf_names[64][RTE_MBUF_DYN_NAMESIZE];


/*
 * Helper function to check if socket is already discovered.
 * If yes, return positive value. If not, return zero.
 */
int
new_socket_id(unsigned int socket_id)
{
	unsigned int i;

	for (i = 0; i < num_sockets; i++) {
		if (socket_ids[i] == socket_id)
			return 0;
	}
	return 1;
}

/*
 * Setup default configuration.
 */
static void
set_default_fwd_lcores_config(void)
{
	unsigned int i;
	unsigned int nb_lc;
	unsigned int sock_num;

	nb_lc = 0;
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (!rte_lcore_is_enabled(i))
			continue;
		sock_num = rte_lcore_to_socket_id(i);
		if (new_socket_id(sock_num)) {
			if (num_sockets >= RTE_MAX_NUMA_NODES) {
				rte_exit(EXIT_FAILURE,
					 "Total sockets greater than %u\n",
					 RTE_MAX_NUMA_NODES);
			}
			socket_ids[num_sockets++] = sock_num;
		}
		if (i == rte_get_main_lcore())
			continue;
		fwd_lcores_cpuids[nb_lc++] = i;
	}
	nb_lcores = (lcoreid_t) nb_lc;
	nb_cfg_lcores = nb_lcores;
	nb_fwd_lcores = 1;
}

static void
set_def_peer_eth_addrs(void)
{
	portid_t i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		peer_eth_addrs[i].addr_bytes[0] = RTE_ETHER_LOCAL_ADMIN_ADDR;
		peer_eth_addrs[i].addr_bytes[5] = i;
	}
}

static void
set_default_fwd_ports_config(void)
{
	portid_t pt_id;
	int i = 0;

	RTE_ETH_FOREACH_DEV(pt_id) {
		fwd_ports_ids[i++] = pt_id;

		/* Update sockets info according to the attached device */
		int socket_id = rte_eth_dev_socket_id(pt_id);
		if (socket_id >= 0 && new_socket_id(socket_id)) {
			if (num_sockets >= RTE_MAX_NUMA_NODES) {
				rte_exit(EXIT_FAILURE,
					 "Total sockets greater than %u\n",
					 RTE_MAX_NUMA_NODES);
			}
			socket_ids[num_sockets++] = socket_id;
		}
	}

	nb_cfg_ports = nb_ports;
	nb_fwd_ports = nb_ports;
}

void
set_def_fwd_config(void)
{
	set_default_fwd_lcores_config();
	set_def_peer_eth_addrs();
	set_default_fwd_ports_config();
}

#ifndef RTE_EXEC_ENV_WINDOWS
/* extremely pessimistic estimation of memory required to create a mempool */
static int
calc_mem_size(uint32_t nb_mbufs, uint32_t mbuf_sz, size_t pgsz, size_t *out)
{
	unsigned int n_pages, mbuf_per_pg, leftover;
	uint64_t total_mem, mbuf_mem, obj_sz;

	/* there is no good way to predict how much space the mempool will
	 * occupy because it will allocate chunks on the fly, and some of those
	 * will come from default DPDK memory while some will come from our
	 * external memory, so just assume 128MB will be enough for everyone.
	 */
	uint64_t hdr_mem = 128 << 20;

	/* account for possible non-contiguousness */
	obj_sz = rte_mempool_calc_obj_size(mbuf_sz, 0, NULL);
	if (obj_sz > pgsz) {
		TESTPMD_LOG(ERR, "Object size is bigger than page size\n");
		return -1;
	}

	mbuf_per_pg = pgsz / obj_sz;
	leftover = (nb_mbufs % mbuf_per_pg) > 0;
	n_pages = (nb_mbufs / mbuf_per_pg) + leftover;

	mbuf_mem = n_pages * pgsz;

	total_mem = RTE_ALIGN(hdr_mem + mbuf_mem, pgsz);

	if (total_mem > SIZE_MAX) {
		TESTPMD_LOG(ERR, "Memory size too big\n");
		return -1;
	}
	*out = (size_t)total_mem;

	return 0;
}

static int
pagesz_flags(uint64_t page_sz)
{
	/* as per mmap() manpage, all page sizes are log2 of page size
	 * shifted by MAP_HUGE_SHIFT
	 */
	int log2 = rte_log2_u64(page_sz);

	return (log2 << HUGE_SHIFT);
}

static void *
alloc_mem(size_t memsz, size_t pgsz, bool huge)
{
	void *addr;
	int flags;

	/* allocate anonymous hugepages */
	flags = MAP_ANONYMOUS | MAP_PRIVATE;
	if (huge)
		flags |= HUGE_FLAG | pagesz_flags(pgsz);

	addr = mmap(NULL, memsz, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (addr == MAP_FAILED)
		return NULL;

	return addr;
}

struct extmem_param {
	void *addr;
	size_t len;
	size_t pgsz;
	rte_iova_t *iova_table;
	unsigned int iova_table_len;
};

static int
create_extmem(uint32_t nb_mbufs, uint32_t mbuf_sz, struct extmem_param *param,
		bool huge)
{
	uint64_t pgsizes[] = {RTE_PGSIZE_2M, RTE_PGSIZE_1G, /* x86_64, ARM */
			RTE_PGSIZE_16M, RTE_PGSIZE_16G};    /* POWER */
	unsigned int cur_page, n_pages, pgsz_idx;
	size_t mem_sz, cur_pgsz;
	rte_iova_t *iovas = NULL;
	void *addr;
	int ret;

	for (pgsz_idx = 0; pgsz_idx < RTE_DIM(pgsizes); pgsz_idx++) {
		/* skip anything that is too big */
		if (pgsizes[pgsz_idx] > SIZE_MAX)
			continue;

		cur_pgsz = pgsizes[pgsz_idx];

		/* if we were told not to allocate hugepages, override */
		if (!huge)
			cur_pgsz = sysconf(_SC_PAGESIZE);

		ret = calc_mem_size(nb_mbufs, mbuf_sz, cur_pgsz, &mem_sz);
		if (ret < 0) {
			TESTPMD_LOG(ERR, "Cannot calculate memory size\n");
			return -1;
		}

		/* allocate our memory */
		addr = alloc_mem(mem_sz, cur_pgsz, huge);

		/* if we couldn't allocate memory with a specified page size,
		 * that doesn't mean we can't do it with other page sizes, so
		 * try another one.
		 */
		if (addr == NULL)
			continue;

		/* store IOVA addresses for every page in this memory area */
		n_pages = mem_sz / cur_pgsz;

		iovas = malloc(sizeof(*iovas) * n_pages);

		if (iovas == NULL) {
			TESTPMD_LOG(ERR, "Cannot allocate memory for iova addresses\n");
			goto fail;
		}
		/* lock memory if it's not huge pages */
		if (!huge)
			mlock(addr, mem_sz);

		/* populate IOVA addresses */
		for (cur_page = 0; cur_page < n_pages; cur_page++) {
			rte_iova_t iova;
			size_t offset;
			void *cur;

			offset = cur_pgsz * cur_page;
			cur = RTE_PTR_ADD(addr, offset);

			/* touch the page before getting its IOVA */
			*(volatile char *)cur = 0;

			iova = rte_mem_virt2iova(cur);

			iovas[cur_page] = iova;
		}

		break;
	}
	/* if we couldn't allocate anything */
	if (iovas == NULL)
		return -1;

	param->addr = addr;
	param->len = mem_sz;
	param->pgsz = cur_pgsz;
	param->iova_table = iovas;
	param->iova_table_len = n_pages;

	return 0;
fail:
	if (iovas)
		free(iovas);
	if (addr)
		munmap(addr, mem_sz);

	return -1;
}

static int
setup_extmem(uint32_t nb_mbufs, uint32_t mbuf_sz, bool huge)
{
	struct extmem_param param;
	int socket_id, ret;

	memset(&param, 0, sizeof(param));

	/* check if our heap exists */
	socket_id = rte_malloc_heap_get_socket(EXTMEM_HEAP_NAME);
	if (socket_id < 0) {
		/* create our heap */
		ret = rte_malloc_heap_create(EXTMEM_HEAP_NAME);
		if (ret < 0) {
			TESTPMD_LOG(ERR, "Cannot create heap\n");
			return -1;
		}
	}

	ret = create_extmem(nb_mbufs, mbuf_sz, &param, huge);
	if (ret < 0) {
		TESTPMD_LOG(ERR, "Cannot create memory area\n");
		return -1;
	}

	/* we now have a valid memory area, so add it to heap */
	ret = rte_malloc_heap_memory_add(EXTMEM_HEAP_NAME,
			param.addr, param.len, param.iova_table,
			param.iova_table_len, param.pgsz);

	/* when using VFIO, memory is automatically mapped for DMA by EAL */

	/* not needed any more */
	free(param.iova_table);

	if (ret < 0) {
		TESTPMD_LOG(ERR, "Cannot add memory to heap\n");
		munmap(param.addr, param.len);
		return -1;
	}

	/* success */

	TESTPMD_LOG(DEBUG, "Allocated %zuMB of external memory\n",
			param.len >> 20);

	return 0;
}
static void
dma_unmap_cb(struct rte_mempool *mp __rte_unused, void *opaque __rte_unused,
	     struct rte_mempool_memhdr *memhdr, unsigned mem_idx __rte_unused)
{
	uint16_t pid = 0;
	int ret;

	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;

		ret = eth_dev_info_get_print_err(pid, &dev_info);
		if (ret != 0) {
			TESTPMD_LOG(DEBUG,
				    "unable to get device info for port %d on addr 0x%p,"
				    "mempool unmapping will not be performed\n",
				    pid, memhdr->addr);
			continue;
		}

		ret = rte_dev_dma_unmap(dev_info.device, memhdr->addr, 0, memhdr->len);
		if (ret) {
			TESTPMD_LOG(DEBUG,
				    "unable to DMA unmap addr 0x%p "
				    "for device %s\n",
				    memhdr->addr, dev_info.device->name);
		}
	}
	ret = rte_extmem_unregister(memhdr->addr, memhdr->len);
	if (ret) {
		TESTPMD_LOG(DEBUG,
			    "unable to un-register addr 0x%p\n", memhdr->addr);
	}
}

static void
dma_map_cb(struct rte_mempool *mp __rte_unused, void *opaque __rte_unused,
	   struct rte_mempool_memhdr *memhdr, unsigned mem_idx __rte_unused)
{
	uint16_t pid = 0;
	size_t page_size = sysconf(_SC_PAGESIZE);
	int ret;

	ret = rte_extmem_register(memhdr->addr, memhdr->len, NULL, 0,
				  page_size);
	if (ret) {
		TESTPMD_LOG(DEBUG,
			    "unable to register addr 0x%p\n", memhdr->addr);
		return;
	}
	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;

		ret = eth_dev_info_get_print_err(pid, &dev_info);
		if (ret != 0) {
			TESTPMD_LOG(DEBUG,
				    "unable to get device info for port %d on addr 0x%p,"
				    "mempool mapping will not be performed\n",
				    pid, memhdr->addr);
			continue;
		}
		ret = rte_dev_dma_map(dev_info.device, memhdr->addr, 0, memhdr->len);
		if (ret) {
			TESTPMD_LOG(DEBUG,
				    "unable to DMA map addr 0x%p "
				    "for device %s\n",
				    memhdr->addr, dev_info.device->name);
		}
	}
}
#endif

static unsigned int
setup_extbuf(uint32_t nb_mbufs, uint16_t mbuf_sz, unsigned int socket_id,
	    char *pool_name, struct rte_pktmbuf_extmem **ext_mem)
{
	struct rte_pktmbuf_extmem *xmem;
	unsigned int ext_num, zone_num, elt_num;
	uint16_t elt_size;

	elt_size = RTE_ALIGN_CEIL(mbuf_sz, RTE_CACHE_LINE_SIZE);
	elt_num = EXTBUF_ZONE_SIZE / elt_size;
	zone_num = (nb_mbufs + elt_num - 1) / elt_num;

	xmem = malloc(sizeof(struct rte_pktmbuf_extmem) * zone_num);
	if (xmem == NULL) {
		TESTPMD_LOG(ERR, "Cannot allocate memory for "
				 "external buffer descriptors\n");
		*ext_mem = NULL;
		return 0;
	}
	for (ext_num = 0; ext_num < zone_num; ext_num++) {
		struct rte_pktmbuf_extmem *xseg = xmem + ext_num;
		const struct rte_memzone *mz;
		char mz_name[RTE_MEMZONE_NAMESIZE];
		int ret;

		ret = snprintf(mz_name, sizeof(mz_name),
			RTE_MEMPOOL_MZ_FORMAT "_xb_%u", pool_name, ext_num);
		if (ret < 0 || ret >= (int)sizeof(mz_name)) {
			errno = ENAMETOOLONG;
			ext_num = 0;
			break;
		}
		mz = rte_memzone_reserve(mz_name, EXTBUF_ZONE_SIZE,
					 socket_id,
					 RTE_MEMZONE_IOVA_CONTIG |
					 RTE_MEMZONE_1GB |
					 RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL) {
			/*
			 * The caller exits on external buffer creation
			 * error, so there is no need to free memzones.
			 */
			errno = ENOMEM;
			ext_num = 0;
			break;
		}
		xseg->buf_ptr = mz->addr;
		xseg->buf_iova = mz->iova;
		xseg->buf_len = EXTBUF_ZONE_SIZE;
		xseg->elt_size = elt_size;
	}
	if (ext_num == 0 && xmem != NULL) {
		free(xmem);
		xmem = NULL;
	}
	*ext_mem = xmem;
	return ext_num;
}

/*
 * Configuration initialisation done once at init time.
 */
static struct rte_mempool *
mbuf_pool_create(uint16_t mbuf_seg_size, unsigned nb_mbuf,
		 unsigned int socket_id, uint16_t size_idx)
{
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *rte_mp = NULL;
#ifndef RTE_EXEC_ENV_WINDOWS
	uint32_t mb_size;

	mb_size = sizeof(struct rte_mbuf) + mbuf_seg_size;
#endif
	mbuf_poolname_build(socket_id, pool_name, sizeof(pool_name), size_idx);
	if (!is_proc_primary()) {
		rte_mp = rte_mempool_lookup(pool_name);
		if (rte_mp == NULL)
			rte_exit(EXIT_FAILURE,
				"Get mbuf pool for socket %u failed: %s\n",
				socket_id, rte_strerror(rte_errno));
		return rte_mp;
	}

	TESTPMD_LOG(INFO,
		"create a new mbuf pool <%s>: n=%u, size=%u, socket=%u\n",
		pool_name, nb_mbuf, mbuf_seg_size, socket_id);

	switch (mp_alloc_type) {
	case MP_ALLOC_NATIVE:
		{
			/* wrapper to rte_mempool_create() */
			TESTPMD_LOG(INFO, "preferred mempool ops selected: %s\n",
					rte_mbuf_best_mempool_ops());
			rte_mp = rte_pktmbuf_pool_create(pool_name, nb_mbuf,
				mb_mempool_cache, 0, mbuf_seg_size, socket_id);
			break;
		}
#ifndef RTE_EXEC_ENV_WINDOWS
	case MP_ALLOC_ANON:
		{
			rte_mp = rte_mempool_create_empty(pool_name, nb_mbuf,
				mb_size, (unsigned int) mb_mempool_cache,
				sizeof(struct rte_pktmbuf_pool_private),
				socket_id, mempool_flags);
			if (rte_mp == NULL)
				goto err;

			if (rte_mempool_populate_anon(rte_mp) == 0) {
				rte_mempool_free(rte_mp);
				rte_mp = NULL;
				goto err;
			}
			rte_pktmbuf_pool_init(rte_mp, NULL);
			rte_mempool_obj_iter(rte_mp, rte_pktmbuf_init, NULL);
			rte_mempool_mem_iter(rte_mp, dma_map_cb, NULL);
			break;
		}
	case MP_ALLOC_XMEM:
	case MP_ALLOC_XMEM_HUGE:
		{
			int heap_socket;
			bool huge = mp_alloc_type == MP_ALLOC_XMEM_HUGE;

			if (setup_extmem(nb_mbuf, mbuf_seg_size, huge) < 0)
				rte_exit(EXIT_FAILURE, "Could not create external memory\n");

			heap_socket =
				rte_malloc_heap_get_socket(EXTMEM_HEAP_NAME);
			if (heap_socket < 0)
				rte_exit(EXIT_FAILURE, "Could not get external memory socket ID\n");

			TESTPMD_LOG(INFO, "preferred mempool ops selected: %s\n",
					rte_mbuf_best_mempool_ops());
			rte_mp = rte_pktmbuf_pool_create(pool_name, nb_mbuf,
					mb_mempool_cache, 0, mbuf_seg_size,
					heap_socket);
			break;
		}
#endif
	case MP_ALLOC_XBUF:
		{
			struct rte_pktmbuf_extmem *ext_mem;
			unsigned int ext_num;

			ext_num = setup_extbuf(nb_mbuf,	mbuf_seg_size,
					       socket_id, pool_name, &ext_mem);
			if (ext_num == 0)
				rte_exit(EXIT_FAILURE,
					 "Can't create pinned data buffers\n");

			TESTPMD_LOG(INFO, "preferred mempool ops selected: %s\n",
					rte_mbuf_best_mempool_ops());
			rte_mp = rte_pktmbuf_pool_create_extbuf
					(pool_name, nb_mbuf, mb_mempool_cache,
					 0, mbuf_seg_size, socket_id,
					 ext_mem, ext_num);
			free(ext_mem);
			break;
		}
	default:
		{
			rte_exit(EXIT_FAILURE, "Invalid mempool creation mode\n");
		}
	}

#ifndef RTE_EXEC_ENV_WINDOWS
err:
#endif
	if (rte_mp == NULL) {
		rte_exit(EXIT_FAILURE,
			"Creation of mbuf pool for socket %u failed: %s\n",
			socket_id, rte_strerror(rte_errno));
	} else if (verbose_level > 0) {
		rte_mempool_dump(stdout, rte_mp);
	}
	return rte_mp;
}

/*
 * Check given socket id is valid or not with NUMA mode,
 * if valid, return 0, else return -1
 */
static int
check_socket_id(const unsigned int socket_id)
{
	static int warning_once = 0;

	if (new_socket_id(socket_id)) {
		if (!warning_once && numa_support)
			fprintf(stderr,
				"Warning: NUMA should be configured manually by using --port-numa-config and --ring-numa-config parameters along with --numa.\n");
		warning_once = 1;
		return -1;
	}
	return 0;
}

/*
 * Get the allowed maximum number of RX queues.
 * *pid return the port id which has minimal value of
 * max_rx_queues in all ports.
 */
queueid_t
get_allowed_max_nb_rxq(portid_t *pid)
{
	queueid_t allowed_max_rxq = RTE_MAX_QUEUES_PER_PORT;
	bool max_rxq_valid = false;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		max_rxq_valid = true;
		if (dev_info.max_rx_queues < allowed_max_rxq) {
			allowed_max_rxq = dev_info.max_rx_queues;
			*pid = pi;
		}
	}
	return max_rxq_valid ? allowed_max_rxq : 0;
}

/*
 * Check input rxq is valid or not.
 * If input rxq is not greater than any of maximum number
 * of RX queues of all ports, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_rxq(queueid_t rxq)
{
	queueid_t allowed_max_rxq;
	portid_t pid = 0;

	allowed_max_rxq = get_allowed_max_nb_rxq(&pid);
	if (rxq > allowed_max_rxq) {
		fprintf(stderr,
			"Fail: input rxq (%u) can't be greater than max_rx_queues (%u) of port %u\n",
			rxq, allowed_max_rxq, pid);
		return -1;
	}
	return 0;
}

/*
 * Get the allowed maximum number of TX queues.
 * *pid return the port id which has minimal value of
 * max_tx_queues in all ports.
 */
queueid_t
get_allowed_max_nb_txq(portid_t *pid)
{
	queueid_t allowed_max_txq = RTE_MAX_QUEUES_PER_PORT;
	bool max_txq_valid = false;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		max_txq_valid = true;
		if (dev_info.max_tx_queues < allowed_max_txq) {
			allowed_max_txq = dev_info.max_tx_queues;
			*pid = pi;
		}
	}
	return max_txq_valid ? allowed_max_txq : 0;
}

/*
 * Check input txq is valid or not.
 * If input txq is not greater than any of maximum number
 * of TX queues of all ports, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_txq(queueid_t txq)
{
	queueid_t allowed_max_txq;
	portid_t pid = 0;

	allowed_max_txq = get_allowed_max_nb_txq(&pid);
	if (txq > allowed_max_txq) {
		fprintf(stderr,
			"Fail: input txq (%u) can't be greater than max_tx_queues (%u) of port %u\n",
			txq, allowed_max_txq, pid);
		return -1;
	}
	return 0;
}

/*
 * Get the allowed maximum number of RXDs of every rx queue.
 * *pid return the port id which has minimal value of
 * max_rxd in all queues of all ports.
 */
static uint16_t
get_allowed_max_nb_rxd(portid_t *pid)
{
	uint16_t allowed_max_rxd = UINT16_MAX;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		if (dev_info.rx_desc_lim.nb_max < allowed_max_rxd) {
			allowed_max_rxd = dev_info.rx_desc_lim.nb_max;
			*pid = pi;
		}
	}
	return allowed_max_rxd;
}

/*
 * Get the allowed minimal number of RXDs of every rx queue.
 * *pid return the port id which has minimal value of
 * min_rxd in all queues of all ports.
 */
static uint16_t
get_allowed_min_nb_rxd(portid_t *pid)
{
	uint16_t allowed_min_rxd = 0;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		if (dev_info.rx_desc_lim.nb_min > allowed_min_rxd) {
			allowed_min_rxd = dev_info.rx_desc_lim.nb_min;
			*pid = pi;
		}
	}

	return allowed_min_rxd;
}

/*
 * Check input rxd is valid or not.
 * If input rxd is not greater than any of maximum number
 * of RXDs of every Rx queues and is not less than any of
 * minimal number of RXDs of every Rx queues, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_rxd(queueid_t rxd)
{
	uint16_t allowed_max_rxd;
	uint16_t allowed_min_rxd;
	portid_t pid = 0;

	allowed_max_rxd = get_allowed_max_nb_rxd(&pid);
	if (rxd > allowed_max_rxd) {
		fprintf(stderr,
			"Fail: input rxd (%u) can't be greater than max_rxds (%u) of port %u\n",
			rxd, allowed_max_rxd, pid);
		return -1;
	}

	allowed_min_rxd = get_allowed_min_nb_rxd(&pid);
	if (rxd < allowed_min_rxd) {
		fprintf(stderr,
			"Fail: input rxd (%u) can't be less than min_rxds (%u) of port %u\n",
			rxd, allowed_min_rxd, pid);
		return -1;
	}

	return 0;
}

/*
 * Get the allowed maximum number of TXDs of every rx queues.
 * *pid return the port id which has minimal value of
 * max_txd in every tx queue.
 */
static uint16_t
get_allowed_max_nb_txd(portid_t *pid)
{
	uint16_t allowed_max_txd = UINT16_MAX;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		if (dev_info.tx_desc_lim.nb_max < allowed_max_txd) {
			allowed_max_txd = dev_info.tx_desc_lim.nb_max;
			*pid = pi;
		}
	}
	return allowed_max_txd;
}

/*
 * Get the allowed maximum number of TXDs of every tx queues.
 * *pid return the port id which has minimal value of
 * min_txd in every tx queue.
 */
static uint16_t
get_allowed_min_nb_txd(portid_t *pid)
{
	uint16_t allowed_min_txd = 0;
	portid_t pi;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_FOREACH_DEV(pi) {
		if (eth_dev_info_get_print_err(pi, &dev_info) != 0)
			continue;

		if (dev_info.tx_desc_lim.nb_min > allowed_min_txd) {
			allowed_min_txd = dev_info.tx_desc_lim.nb_min;
			*pid = pi;
		}
	}

	return allowed_min_txd;
}

/*
 * Check input txd is valid or not.
 * If input txd is not greater than any of maximum number
 * of TXDs of every Rx queues, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_txd(queueid_t txd)
{
	uint16_t allowed_max_txd;
	uint16_t allowed_min_txd;
	portid_t pid = 0;

	allowed_max_txd = get_allowed_max_nb_txd(&pid);
	if (txd > allowed_max_txd) {
		fprintf(stderr,
			"Fail: input txd (%u) can't be greater than max_txds (%u) of port %u\n",
			txd, allowed_max_txd, pid);
		return -1;
	}

	allowed_min_txd = get_allowed_min_nb_txd(&pid);
	if (txd < allowed_min_txd) {
		fprintf(stderr,
			"Fail: input txd (%u) can't be less than min_txds (%u) of port %u\n",
			txd, allowed_min_txd, pid);
		return -1;
	}
	return 0;
}


/*
 * Get the allowed maximum number of hairpin queues.
 * *pid return the port id which has minimal value of
 * max_hairpin_queues in all ports.
 */
queueid_t
get_allowed_max_nb_hairpinq(portid_t *pid)
{
	queueid_t allowed_max_hairpinq = RTE_MAX_QUEUES_PER_PORT;
	portid_t pi;
	struct rte_eth_hairpin_cap cap;

	RTE_ETH_FOREACH_DEV(pi) {
		if (rte_eth_dev_hairpin_capability_get(pi, &cap) != 0) {
			*pid = pi;
			return 0;
		}
		if (cap.max_nb_queues < allowed_max_hairpinq) {
			allowed_max_hairpinq = cap.max_nb_queues;
			*pid = pi;
		}
	}
	return allowed_max_hairpinq;
}

/*
 * Check input hairpin is valid or not.
 * If input hairpin is not greater than any of maximum number
 * of hairpin queues of all ports, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_hairpinq(queueid_t hairpinq)
{
	queueid_t allowed_max_hairpinq;
	portid_t pid = 0;

	allowed_max_hairpinq = get_allowed_max_nb_hairpinq(&pid);
	if (hairpinq > allowed_max_hairpinq) {
		fprintf(stderr,
			"Fail: input hairpin (%u) can't be greater than max_hairpin_queues (%u) of port %u\n",
			hairpinq, allowed_max_hairpinq, pid);
		return -1;
	}
	return 0;
}

static int
get_eth_overhead(struct rte_eth_dev_info *dev_info)
{
	uint32_t eth_overhead;

	if (dev_info->max_mtu != UINT16_MAX &&
	    dev_info->max_rx_pktlen > dev_info->max_mtu)
		eth_overhead = dev_info->max_rx_pktlen - dev_info->max_mtu;
	else
		eth_overhead = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	return eth_overhead;
}

static void
init_config_port_offloads(portid_t pid, uint32_t socket_id)
{
	struct rte_port *port = &ports[pid];
	int ret;
	int i;

	eth_rx_metadata_negotiate_mp(pid);

	port->dev_conf.txmode = tx_mode;
	port->dev_conf.rxmode = rx_mode;

	ret = eth_dev_info_get_print_err(pid, &port->dev_info);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "rte_eth_dev_info_get() failed\n");

	if (!(port->dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE))
		port->dev_conf.txmode.offloads &=
			~RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Apply Rx offloads configuration */
	for (i = 0; i < port->dev_info.max_rx_queues; i++)
		port->rxq[i].conf.offloads = port->dev_conf.rxmode.offloads;
	/* Apply Tx offloads configuration */
	for (i = 0; i < port->dev_info.max_tx_queues; i++)
		port->txq[i].conf.offloads = port->dev_conf.txmode.offloads;

	if (eth_link_speed)
		port->dev_conf.link_speeds = eth_link_speed;

	if (max_rx_pkt_len)
		port->dev_conf.rxmode.mtu = max_rx_pkt_len -
			get_eth_overhead(&port->dev_info);

	/* set flag to initialize port/queue */
	port->need_reconfig = 1;
	port->need_reconfig_queues = 1;
	port->socket_id = socket_id;
	port->tx_metadata = 0;

	/*
	 * Check for maximum number of segments per MTU.
	 * Accordingly update the mbuf data size.
	 */
	if (port->dev_info.rx_desc_lim.nb_mtu_seg_max != UINT16_MAX &&
	    port->dev_info.rx_desc_lim.nb_mtu_seg_max != 0) {
		uint32_t eth_overhead = get_eth_overhead(&port->dev_info);
		uint16_t mtu;

		if (rte_eth_dev_get_mtu(pid, &mtu) == 0) {
			uint16_t data_size = (mtu + eth_overhead) /
				port->dev_info.rx_desc_lim.nb_mtu_seg_max;
			uint16_t buffer_size = data_size + RTE_PKTMBUF_HEADROOM;

			if (buffer_size > mbuf_data_size[0]) {
				mbuf_data_size[0] = buffer_size;
				TESTPMD_LOG(WARNING,
					"Configured mbuf size of the first segment %hu\n",
					mbuf_data_size[0]);
			}
		}
	}
}

static void
init_config(void)
{
	portid_t pid;
	struct rte_mempool *mbp;
	unsigned int nb_mbuf_per_pool;
	lcoreid_t  lc_id;
#ifdef RTE_LIB_GRO
	struct rte_gro_param gro_param;
#endif
#ifdef RTE_LIB_GSO
	uint32_t gso_types;
#endif

	/* Configuration of logical cores. */
	fwd_lcores = rte_zmalloc("testpmd: fwd_lcores",
				sizeof(struct fwd_lcore *) * nb_lcores,
				RTE_CACHE_LINE_SIZE);
	if (fwd_lcores == NULL) {
		rte_exit(EXIT_FAILURE, "rte_zmalloc(%d (struct fwd_lcore *)) "
							"failed\n", nb_lcores);
	}
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		fwd_lcores[lc_id] = rte_zmalloc("testpmd: struct fwd_lcore",
					       sizeof(struct fwd_lcore),
					       RTE_CACHE_LINE_SIZE);
		if (fwd_lcores[lc_id] == NULL) {
			rte_exit(EXIT_FAILURE, "rte_zmalloc(struct fwd_lcore) "
								"failed\n");
		}
		fwd_lcores[lc_id]->cpuid_idx = lc_id;
	}

	RTE_ETH_FOREACH_DEV(pid) {
		uint32_t socket_id;

		if (numa_support) {
			socket_id = port_numa[pid];
			if (port_numa[pid] == NUMA_NO_CONFIG) {
				socket_id = rte_eth_dev_socket_id(pid);

				/*
				 * if socket_id is invalid,
				 * set to the first available socket.
				 */
				if (check_socket_id(socket_id) < 0)
					socket_id = socket_ids[0];
			}
		} else {
			socket_id = (socket_num == UMA_NO_CONFIG) ?
				    0 : socket_num;
		}
		/* Apply default TxRx configuration for all ports */
		init_config_port_offloads(pid, socket_id);
	}
	/*
	 * Create pools of mbuf.
	 * If NUMA support is disabled, create a single pool of mbuf in
	 * socket 0 memory by default.
	 * Otherwise, create a pool of mbuf in the memory of sockets 0 and 1.
	 *
	 * Use the maximum value of nb_rxd and nb_txd here, then nb_rxd and
	 * nb_txd can be configured at run time.
	 */
	if (param_total_num_mbufs)
		nb_mbuf_per_pool = param_total_num_mbufs;
	else {
		nb_mbuf_per_pool = RTE_TEST_RX_DESC_MAX +
			(nb_lcores * mb_mempool_cache) +
			RTE_TEST_TX_DESC_MAX + MAX_PKT_BURST;
		nb_mbuf_per_pool *= RTE_MAX_ETHPORTS;
	}

	if (numa_support) {
		uint8_t i, j;

		for (i = 0; i < num_sockets; i++)
			for (j = 0; j < mbuf_data_size_n; j++)
				mempools[i * MAX_SEGS_BUFFER_SPLIT + j] =
					mbuf_pool_create(mbuf_data_size[j],
							  nb_mbuf_per_pool,
							  socket_ids[i], j);
	} else {
		uint8_t i;

		for (i = 0; i < mbuf_data_size_n; i++)
			mempools[i] = mbuf_pool_create
					(mbuf_data_size[i],
					 nb_mbuf_per_pool,
					 socket_num == UMA_NO_CONFIG ?
					 0 : socket_num, i);
	}

	init_port_config();

#ifdef RTE_LIB_GSO
	gso_types = RTE_ETH_TX_OFFLOAD_TCP_TSO | RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO | RTE_ETH_TX_OFFLOAD_UDP_TSO;
#endif
	/*
	 * Records which Mbuf pool to use by each logical core, if needed.
	 */
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		mbp = mbuf_pool_find(
			rte_lcore_to_socket_id(fwd_lcores_cpuids[lc_id]), 0);

		if (mbp == NULL)
			mbp = mbuf_pool_find(0, 0);
		fwd_lcores[lc_id]->mbp = mbp;
#ifdef RTE_LIB_GSO
		/* initialize GSO context */
		fwd_lcores[lc_id]->gso_ctx.direct_pool = mbp;
		fwd_lcores[lc_id]->gso_ctx.indirect_pool = mbp;
		fwd_lcores[lc_id]->gso_ctx.gso_types = gso_types;
		fwd_lcores[lc_id]->gso_ctx.gso_size = RTE_ETHER_MAX_LEN -
			RTE_ETHER_CRC_LEN;
		fwd_lcores[lc_id]->gso_ctx.flag = 0;
#endif
	}

	fwd_config_setup();

#ifdef RTE_LIB_GRO
	/* create a gro context for each lcore */
	gro_param.gro_types = RTE_GRO_TCP_IPV4;
	gro_param.max_flow_num = GRO_MAX_FLUSH_CYCLES;
	gro_param.max_item_per_flow = MAX_PKT_BURST;
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		gro_param.socket_id = rte_lcore_to_socket_id(
				fwd_lcores_cpuids[lc_id]);
		fwd_lcores[lc_id]->gro_ctx = rte_gro_ctx_create(&gro_param);
		if (fwd_lcores[lc_id]->gro_ctx == NULL) {
			rte_exit(EXIT_FAILURE,
					"rte_gro_ctx_create() failed\n");
		}
	}
#endif
}


void
reconfig(portid_t new_port_id, unsigned socket_id)
{
	/* Reconfiguration of Ethernet ports. */
	init_config_port_offloads(new_port_id, socket_id);
	init_port_config();
}

int
init_fwd_streams(void)
{
	portid_t pid;
	struct rte_port *port;
	streamid_t sm_id, nb_fwd_streams_new;
	queueid_t q;

	/* set socket id according to numa or not */
	RTE_ETH_FOREACH_DEV(pid) {
		port = &ports[pid];
		if (nb_rxq > port->dev_info.max_rx_queues) {
			fprintf(stderr,
				"Fail: nb_rxq(%d) is greater than max_rx_queues(%d)\n",
				nb_rxq, port->dev_info.max_rx_queues);
			return -1;
		}
		if (nb_txq > port->dev_info.max_tx_queues) {
			fprintf(stderr,
				"Fail: nb_txq(%d) is greater than max_tx_queues(%d)\n",
				nb_txq, port->dev_info.max_tx_queues);
			return -1;
		}
		if (numa_support) {
			if (port_numa[pid] != NUMA_NO_CONFIG)
				port->socket_id = port_numa[pid];
			else {
				port->socket_id = rte_eth_dev_socket_id(pid);

				/*
				 * if socket_id is invalid,
				 * set to the first available socket.
				 */
				if (check_socket_id(port->socket_id) < 0)
					port->socket_id = socket_ids[0];
			}
		}
		else {
			if (socket_num == UMA_NO_CONFIG)
				port->socket_id = 0;
			else
				port->socket_id = socket_num;
		}
	}

	q = RTE_MAX(nb_rxq, nb_txq);
	if (q == 0) {
		fprintf(stderr,
			"Fail: Cannot allocate fwd streams as number of queues is 0\n");
		return -1;
	}
	nb_fwd_streams_new = (streamid_t)(nb_ports * q);
	if (nb_fwd_streams_new == nb_fwd_streams)
		return 0;
	/* clear the old */
	if (fwd_streams != NULL) {
		for (sm_id = 0; sm_id < nb_fwd_streams; sm_id++) {
			if (fwd_streams[sm_id] == NULL)
				continue;
			rte_free(fwd_streams[sm_id]);
			fwd_streams[sm_id] = NULL;
		}
		rte_free(fwd_streams);
		fwd_streams = NULL;
	}

	/* init new */
	nb_fwd_streams = nb_fwd_streams_new;
	if (nb_fwd_streams) {
		fwd_streams = rte_zmalloc("testpmd: fwd_streams",
			sizeof(struct fwd_stream *) * nb_fwd_streams,
			RTE_CACHE_LINE_SIZE);
		if (fwd_streams == NULL)
			rte_exit(EXIT_FAILURE, "rte_zmalloc(%d"
				 " (struct fwd_stream *)) failed\n",
				 nb_fwd_streams);

		for (sm_id = 0; sm_id < nb_fwd_streams; sm_id++) {
			fwd_streams[sm_id] = rte_zmalloc("testpmd:"
				" struct fwd_stream", sizeof(struct fwd_stream),
				RTE_CACHE_LINE_SIZE);
			if (fwd_streams[sm_id] == NULL)
				rte_exit(EXIT_FAILURE, "rte_zmalloc"
					 "(struct fwd_stream) failed\n");
		}
	}

	return 0;
}

static void
pkt_burst_stats_display(const char *rx_tx, struct pkt_burst_stats *pbs)
{
	uint64_t total_burst, sburst;
	uint64_t nb_burst;
	uint64_t burst_stats[4];
	uint16_t pktnb_stats[4];
	uint16_t nb_pkt;
	int burst_percent[4], sburstp;
	int i;

	/*
	 * First compute the total number of packet bursts and the
	 * two highest numbers of bursts of the same number of packets.
	 */
	memset(&burst_stats, 0x0, sizeof(burst_stats));
	memset(&pktnb_stats, 0x0, sizeof(pktnb_stats));

	/* Show stats for 0 burst size always */
	total_burst = pbs->pkt_burst_spread[0];
	burst_stats[0] = pbs->pkt_burst_spread[0];
	pktnb_stats[0] = 0;

	/* Find the next 2 burst sizes with highest occurrences. */
	for (nb_pkt = 1; nb_pkt < MAX_PKT_BURST + 1; nb_pkt++) {
		nb_burst = pbs->pkt_burst_spread[nb_pkt];

		if (nb_burst == 0)
			continue;

		total_burst += nb_burst;

		if (nb_burst > burst_stats[1]) {
			burst_stats[2] = burst_stats[1];
			pktnb_stats[2] = pktnb_stats[1];
			burst_stats[1] = nb_burst;
			pktnb_stats[1] = nb_pkt;
		} else if (nb_burst > burst_stats[2]) {
			burst_stats[2] = nb_burst;
			pktnb_stats[2] = nb_pkt;
		}
	}
	if (total_burst == 0)
		return;

	printf("  %s-bursts : %"PRIu64" [", rx_tx, total_burst);
	for (i = 0, sburst = 0, sburstp = 0; i < 4; i++) {
		if (i == 3) {
			printf("%d%% of other]\n", 100 - sburstp);
			return;
		}

		sburst += burst_stats[i];
		if (sburst == total_burst) {
			printf("%d%% of %d pkts]\n",
				100 - sburstp, (int) pktnb_stats[i]);
			return;
		}

		burst_percent[i] =
			(double)burst_stats[i] / total_burst * 100;
		printf("%d%% of %d pkts + ",
			burst_percent[i], (int) pktnb_stats[i]);
		sburstp += burst_percent[i];
	}
}

static void
fwd_stream_stats_display(streamid_t stream_id)
{
	struct fwd_stream *fs;
	static const char *fwd_top_stats_border = "-------";

	fs = fwd_streams[stream_id];
	if ((fs->rx_packets == 0) && (fs->tx_packets == 0) &&
	    (fs->fwd_dropped == 0))
		return;
	printf("\n  %s Forward Stats for RX Port=%2d/Queue=%2d -> "
	       "TX Port=%2d/Queue=%2d %s\n",
	       fwd_top_stats_border, fs->rx_port, fs->rx_queue,
	       fs->tx_port, fs->tx_queue, fwd_top_stats_border);
	printf("  RX-packets: %-14"PRIu64" TX-packets: %-14"PRIu64
	       " TX-dropped: %-14"PRIu64,
	       fs->rx_packets, fs->tx_packets, fs->fwd_dropped);

	/* if checksum mode */
	if (cur_fwd_eng == &csum_fwd_engine) {
		printf("  RX- bad IP checksum: %-14"PRIu64
		       "  Rx- bad L4 checksum: %-14"PRIu64
		       " Rx- bad outer L4 checksum: %-14"PRIu64"\n",
			fs->rx_bad_ip_csum, fs->rx_bad_l4_csum,
			fs->rx_bad_outer_l4_csum);
		printf(" RX- bad outer IP checksum: %-14"PRIu64"\n",
			fs->rx_bad_outer_ip_csum);
	} else {
		printf("\n");
	}

	if (record_burst_stats) {
		pkt_burst_stats_display("RX", &fs->rx_burst_stats);
		pkt_burst_stats_display("TX", &fs->tx_burst_stats);
	}
}

void
fwd_stats_display(void)
{
	static const char *fwd_stats_border = "----------------------";
	static const char *acc_stats_border = "+++++++++++++++";
	struct {
		struct fwd_stream *rx_stream;
		struct fwd_stream *tx_stream;
		uint64_t tx_dropped;
		uint64_t rx_bad_ip_csum;
		uint64_t rx_bad_l4_csum;
		uint64_t rx_bad_outer_l4_csum;
		uint64_t rx_bad_outer_ip_csum;
	} ports_stats[RTE_MAX_ETHPORTS];
	uint64_t total_rx_dropped = 0;
	uint64_t total_tx_dropped = 0;
	uint64_t total_rx_nombuf = 0;
	struct rte_eth_stats stats;
	uint64_t fwd_cycles = 0;
	uint64_t total_recv = 0;
	uint64_t total_xmit = 0;
	struct rte_port *port;
	streamid_t sm_id;
	portid_t pt_id;
	int ret;
	int i;

	memset(ports_stats, 0, sizeof(ports_stats));

	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		struct fwd_stream *fs = fwd_streams[sm_id];

		if (cur_fwd_config.nb_fwd_streams >
		    cur_fwd_config.nb_fwd_ports) {
			fwd_stream_stats_display(sm_id);
		} else {
			ports_stats[fs->tx_port].tx_stream = fs;
			ports_stats[fs->rx_port].rx_stream = fs;
		}

		ports_stats[fs->tx_port].tx_dropped += fs->fwd_dropped;

		ports_stats[fs->rx_port].rx_bad_ip_csum += fs->rx_bad_ip_csum;
		ports_stats[fs->rx_port].rx_bad_l4_csum += fs->rx_bad_l4_csum;
		ports_stats[fs->rx_port].rx_bad_outer_l4_csum +=
				fs->rx_bad_outer_l4_csum;
		ports_stats[fs->rx_port].rx_bad_outer_ip_csum +=
				fs->rx_bad_outer_ip_csum;

		if (record_core_cycles)
			fwd_cycles += fs->core_cycles;
	}
	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		pt_id = fwd_ports_ids[i];
		port = &ports[pt_id];

		ret = rte_eth_stats_get(pt_id, &stats);
		if (ret != 0) {
			fprintf(stderr,
				"%s: Error: failed to get stats (port %u): %d",
				__func__, pt_id, ret);
			continue;
		}
		stats.ipackets -= port->stats.ipackets;
		stats.opackets -= port->stats.opackets;
		stats.ibytes -= port->stats.ibytes;
		stats.obytes -= port->stats.obytes;
		stats.imissed -= port->stats.imissed;
		stats.oerrors -= port->stats.oerrors;
		stats.rx_nombuf -= port->stats.rx_nombuf;

		total_recv += stats.ipackets;
		total_xmit += stats.opackets;
		total_rx_dropped += stats.imissed;
		total_tx_dropped += ports_stats[pt_id].tx_dropped;
		total_tx_dropped += stats.oerrors;
		total_rx_nombuf  += stats.rx_nombuf;

		printf("\n  %s Forward statistics for port %-2d %s\n",
		       fwd_stats_border, pt_id, fwd_stats_border);

		printf("  RX-packets: %-14"PRIu64" RX-dropped: %-14"PRIu64
		       "RX-total: %-"PRIu64"\n", stats.ipackets, stats.imissed,
		       stats.ipackets + stats.imissed);

		if (cur_fwd_eng == &csum_fwd_engine) {
			printf("  Bad-ipcsum: %-14"PRIu64
			       " Bad-l4csum: %-14"PRIu64
			       "Bad-outer-l4csum: %-14"PRIu64"\n",
			       ports_stats[pt_id].rx_bad_ip_csum,
			       ports_stats[pt_id].rx_bad_l4_csum,
			       ports_stats[pt_id].rx_bad_outer_l4_csum);
			printf("  Bad-outer-ipcsum: %-14"PRIu64"\n",
			       ports_stats[pt_id].rx_bad_outer_ip_csum);
		}
		if (stats.ierrors + stats.rx_nombuf > 0) {
			printf("  RX-error: %-"PRIu64"\n", stats.ierrors);
			printf("  RX-nombufs: %-14"PRIu64"\n", stats.rx_nombuf);
		}

		printf("  TX-packets: %-14"PRIu64" TX-dropped: %-14"PRIu64
		       "TX-total: %-"PRIu64"\n",
		       stats.opackets, ports_stats[pt_id].tx_dropped,
		       stats.opackets + ports_stats[pt_id].tx_dropped);

		if (record_burst_stats) {
			if (ports_stats[pt_id].rx_stream)
				pkt_burst_stats_display("RX",
					&ports_stats[pt_id].rx_stream->rx_burst_stats);
			if (ports_stats[pt_id].tx_stream)
				pkt_burst_stats_display("TX",
				&ports_stats[pt_id].tx_stream->tx_burst_stats);
		}

		printf("  %s--------------------------------%s\n",
		       fwd_stats_border, fwd_stats_border);
	}

	printf("\n  %s Accumulated forward statistics for all ports"
	       "%s\n",
	       acc_stats_border, acc_stats_border);
	printf("  RX-packets: %-14"PRIu64" RX-dropped: %-14"PRIu64"RX-total: "
	       "%-"PRIu64"\n"
	       "  TX-packets: %-14"PRIu64" TX-dropped: %-14"PRIu64"TX-total: "
	       "%-"PRIu64"\n",
	       total_recv, total_rx_dropped, total_recv + total_rx_dropped,
	       total_xmit, total_tx_dropped, total_xmit + total_tx_dropped);
	if (total_rx_nombuf > 0)
		printf("  RX-nombufs: %-14"PRIu64"\n", total_rx_nombuf);
	printf("  %s++++++++++++++++++++++++++++++++++++++++++++++"
	       "%s\n",
	       acc_stats_border, acc_stats_border);
	if (record_core_cycles) {
#define CYC_PER_MHZ 1E6
		if (total_recv > 0 || total_xmit > 0) {
			uint64_t total_pkts = 0;
			if (strcmp(cur_fwd_eng->fwd_mode_name, "txonly") == 0 ||
			    strcmp(cur_fwd_eng->fwd_mode_name, "flowgen") == 0)
				total_pkts = total_xmit;
			else
				total_pkts = total_recv;

			printf("\n  CPU cycles/packet=%.2F (total cycles="
			       "%"PRIu64" / total %s packets=%"PRIu64") at %"PRIu64
			       " MHz Clock\n",
			       (double) fwd_cycles / total_pkts,
			       fwd_cycles, cur_fwd_eng->fwd_mode_name, total_pkts,
			       (uint64_t)(rte_get_tsc_hz() / CYC_PER_MHZ));
		}
	}
}

void
fwd_stats_reset(void)
{
	streamid_t sm_id;
	portid_t pt_id;
	int ret;
	int i;

	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		pt_id = fwd_ports_ids[i];
		ret = rte_eth_stats_get(pt_id, &ports[pt_id].stats);
		if (ret != 0)
			fprintf(stderr,
				"%s: Error: failed to clear stats (port %u):%d",
				__func__, pt_id, ret);
	}
	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		struct fwd_stream *fs = fwd_streams[sm_id];

		fs->rx_packets = 0;
		fs->tx_packets = 0;
		fs->fwd_dropped = 0;
		fs->rx_bad_ip_csum = 0;
		fs->rx_bad_l4_csum = 0;
		fs->rx_bad_outer_l4_csum = 0;
		fs->rx_bad_outer_ip_csum = 0;

		memset(&fs->rx_burst_stats, 0, sizeof(fs->rx_burst_stats));
		memset(&fs->tx_burst_stats, 0, sizeof(fs->tx_burst_stats));
		fs->core_cycles = 0;
	}
}

static void
flush_fwd_rx_queues(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	portid_t  rxp;
	portid_t port_id;
	queueid_t rxq;
	uint16_t  nb_rx;
	uint16_t  i;
	uint8_t   j;
	uint64_t prev_tsc = 0, diff_tsc, cur_tsc, timer_tsc = 0;
	uint64_t timer_period;

	if (num_procs > 1) {
		printf("multi-process not support for flushing fwd Rx queues, skip the below lines and return.\n");
		return;
	}

	/* convert to number of cycles */
	timer_period = rte_get_timer_hz(); /* 1 second timeout */

	for (j = 0; j < 2; j++) {
		for (rxp = 0; rxp < cur_fwd_config.nb_fwd_ports; rxp++) {
			for (rxq = 0; rxq < nb_rxq; rxq++) {
				port_id = fwd_ports_ids[rxp];

				/* Polling stopped queues is prohibited. */
				if (ports[port_id].rxq[rxq].state ==
				    RTE_ETH_QUEUE_STATE_STOPPED)
					continue;

				/**
				* testpmd can stuck in the below do while loop
				* if rte_eth_rx_burst() always returns nonzero
				* packets. So timer is added to exit this loop
				* after 1sec timer expiry.
				*/
				prev_tsc = rte_rdtsc();
				do {
					nb_rx = rte_eth_rx_burst(port_id, rxq,
						pkts_burst, MAX_PKT_BURST);
					for (i = 0; i < nb_rx; i++)
						rte_pktmbuf_free(pkts_burst[i]);

					cur_tsc = rte_rdtsc();
					diff_tsc = cur_tsc - prev_tsc;
					timer_tsc += diff_tsc;
				} while ((nb_rx > 0) &&
					(timer_tsc < timer_period));
				timer_tsc = 0;
			}
		}
		rte_delay_ms(10); /* wait 10 milli-seconds before retrying */
	}
}

static void
run_pkt_fwd_on_lcore(struct fwd_lcore *fc, packet_fwd_t pkt_fwd)
{
	struct fwd_stream **fsm;
	streamid_t nb_fs;
	streamid_t sm_id;
#ifdef RTE_LIB_BITRATESTATS
	uint64_t tics_per_1sec;
	uint64_t tics_datum;
	uint64_t tics_current;
	uint16_t i, cnt_ports;

	cnt_ports = nb_ports;
	tics_datum = rte_rdtsc();
	tics_per_1sec = rte_get_timer_hz();
#endif
	fsm = &fwd_streams[fc->stream_idx];
	nb_fs = fc->stream_nb;
	do {
		for (sm_id = 0; sm_id < nb_fs; sm_id++)
			if (!fsm[sm_id]->disabled)
				(*pkt_fwd)(fsm[sm_id]);
#ifdef RTE_LIB_BITRATESTATS
		if (bitrate_enabled != 0 &&
				bitrate_lcore_id == rte_lcore_id()) {
			tics_current = rte_rdtsc();
			if (tics_current - tics_datum >= tics_per_1sec) {
				/* Periodic bitrate calculation */
				for (i = 0; i < cnt_ports; i++)
					rte_stats_bitrate_calc(bitrate_data,
						ports_ids[i]);
				tics_datum = tics_current;
			}
		}
#endif
#ifdef RTE_LIB_LATENCYSTATS
		if (latencystats_enabled != 0 &&
				latencystats_lcore_id == rte_lcore_id())
			rte_latencystats_update();
#endif

	} while (! fc->stopped);
}

static int
start_pkt_forward_on_core(void *fwd_arg)
{
	run_pkt_fwd_on_lcore((struct fwd_lcore *) fwd_arg,
			     cur_fwd_config.fwd_eng->packet_fwd);
	return 0;
}

/*
 * Run the TXONLY packet forwarding engine to send a single burst of packets.
 * Used to start communication flows in network loopback test configurations.
 */
static int
run_one_txonly_burst_on_core(void *fwd_arg)
{
	struct fwd_lcore *fwd_lc;
	struct fwd_lcore tmp_lcore;

	fwd_lc = (struct fwd_lcore *) fwd_arg;
	tmp_lcore = *fwd_lc;
	tmp_lcore.stopped = 1;
	run_pkt_fwd_on_lcore(&tmp_lcore, tx_only_engine.packet_fwd);
	return 0;
}

/*
 * Launch packet forwarding:
 *     - Setup per-port forwarding context.
 *     - launch logical cores with their forwarding configuration.
 */
static void
launch_packet_forwarding(lcore_function_t *pkt_fwd_on_lcore)
{
	unsigned int i;
	unsigned int lc_id;
	int diag;

	for (i = 0; i < cur_fwd_config.nb_fwd_lcores; i++) {
		lc_id = fwd_lcores_cpuids[i];
		if ((interactive == 0) || (lc_id != rte_lcore_id())) {
			fwd_lcores[i]->stopped = 0;
			diag = rte_eal_remote_launch(pkt_fwd_on_lcore,
						     fwd_lcores[i], lc_id);
			if (diag != 0)
				fprintf(stderr,
					"launch lcore %u failed - diag=%d\n",
					lc_id, diag);
		}
	}
}

/*
 * Launch packet forwarding configuration.
 */
void
start_packet_forwarding(int with_tx_first)
{
	port_fwd_begin_t port_fwd_begin;
	port_fwd_end_t  port_fwd_end;
	stream_init_t stream_init = cur_fwd_eng->stream_init;
	unsigned int i;

	if (strcmp(cur_fwd_eng->fwd_mode_name, "rxonly") == 0 && !nb_rxq)
		rte_exit(EXIT_FAILURE, "rxq are 0, cannot use rxonly fwd mode\n");

	if (strcmp(cur_fwd_eng->fwd_mode_name, "txonly") == 0 && !nb_txq)
		rte_exit(EXIT_FAILURE, "txq are 0, cannot use txonly fwd mode\n");

	if ((strcmp(cur_fwd_eng->fwd_mode_name, "rxonly") != 0 &&
		strcmp(cur_fwd_eng->fwd_mode_name, "txonly") != 0) &&
		(!nb_rxq || !nb_txq))
		rte_exit(EXIT_FAILURE,
			"Either rxq or txq are 0, cannot use %s fwd mode\n",
			cur_fwd_eng->fwd_mode_name);

	if (all_ports_started() == 0) {
		fprintf(stderr, "Not all ports were started\n");
		return;
	}
	if (test_done == 0) {
		fprintf(stderr, "Packet forwarding already started\n");
		return;
	}

	fwd_config_setup();

	pkt_fwd_config_display(&cur_fwd_config);
	if (!pkt_fwd_shared_rxq_check())
		return;

	if (stream_init != NULL)
		for (i = 0; i < cur_fwd_config.nb_fwd_streams; i++)
			stream_init(fwd_streams[i]);

	port_fwd_begin = cur_fwd_config.fwd_eng->port_fwd_begin;
	if (port_fwd_begin != NULL) {
		for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
			if (port_fwd_begin(fwd_ports_ids[i])) {
				fprintf(stderr,
					"Packet forwarding is not ready\n");
				return;
			}
		}
	}

	if (with_tx_first) {
		port_fwd_begin = tx_only_engine.port_fwd_begin;
		if (port_fwd_begin != NULL) {
			for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
				if (port_fwd_begin(fwd_ports_ids[i])) {
					fprintf(stderr,
						"Packet forwarding is not ready\n");
					return;
				}
			}
		}
	}

	test_done = 0;

	if(!no_flush_rx)
		flush_fwd_rx_queues();

	rxtx_config_display();

	fwd_stats_reset();
	if (with_tx_first) {
		while (with_tx_first--) {
			launch_packet_forwarding(
					run_one_txonly_burst_on_core);
			rte_eal_mp_wait_lcore();
		}
		port_fwd_end = tx_only_engine.port_fwd_end;
		if (port_fwd_end != NULL) {
			for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++)
				(*port_fwd_end)(fwd_ports_ids[i]);
		}
	}
	launch_packet_forwarding(start_pkt_forward_on_core);
}

void
stop_packet_forwarding(void)
{
	port_fwd_end_t port_fwd_end;
	lcoreid_t lc_id;
	portid_t pt_id;
	int i;

	if (test_done) {
		fprintf(stderr, "Packet forwarding not started\n");
		return;
	}
	printf("Telling cores to stop...");
	for (lc_id = 0; lc_id < cur_fwd_config.nb_fwd_lcores; lc_id++)
		fwd_lcores[lc_id]->stopped = 1;
	printf("\nWaiting for lcores to finish...\n");
	rte_eal_mp_wait_lcore();
	port_fwd_end = cur_fwd_config.fwd_eng->port_fwd_end;
	if (port_fwd_end != NULL) {
		for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
			pt_id = fwd_ports_ids[i];
			(*port_fwd_end)(pt_id);
		}
	}

	fwd_stats_display();

	printf("\nDone.\n");
	test_done = 1;
}

void
dev_set_link_up(portid_t pid)
{
	if (rte_eth_dev_set_link_up(pid) < 0)
		fprintf(stderr, "\nSet link up fail.\n");
}

void
dev_set_link_down(portid_t pid)
{
	if (rte_eth_dev_set_link_down(pid) < 0)
		fprintf(stderr, "\nSet link down fail.\n");
}

static int
all_ports_started(void)
{
	portid_t pi;
	struct rte_port *port;

	RTE_ETH_FOREACH_DEV(pi) {
		port = &ports[pi];
		/* Check if there is a port which is not started */
		if ((port->port_status != RTE_PORT_STARTED) &&
			(port->slave_flag == 0))
			return 0;
	}

	/* No port is not started */
	return 1;
}

int
port_is_stopped(portid_t port_id)
{
	struct rte_port *port = &ports[port_id];

	if ((port->port_status != RTE_PORT_STOPPED) &&
	    (port->slave_flag == 0))
		return 0;
	return 1;
}

int
all_ports_stopped(void)
{
	portid_t pi;

	RTE_ETH_FOREACH_DEV(pi) {
		if (!port_is_stopped(pi))
			return 0;
	}

	return 1;
}

int
port_is_started(portid_t port_id)
{
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 0;

	if (ports[port_id].port_status != RTE_PORT_STARTED)
		return 0;

	return 1;
}

/* Configure the Rx and Tx hairpin queues for the selected port. */
static int
setup_hairpin_queues(portid_t pi, portid_t p_pi, uint16_t cnt_pi)
{
	queueid_t qi;
	struct rte_eth_hairpin_conf hairpin_conf = {
		.peer_count = 1,
	};
	int i;
	int diag;
	struct rte_port *port = &ports[pi];
	uint16_t peer_rx_port = pi;
	uint16_t peer_tx_port = pi;
	uint32_t manual = 1;
	uint32_t tx_exp = hairpin_mode & 0x10;

	if (!(hairpin_mode & 0xf)) {
		peer_rx_port = pi;
		peer_tx_port = pi;
		manual = 0;
	} else if (hairpin_mode & 0x1) {
		peer_tx_port = rte_eth_find_next_owned_by(pi + 1,
						       RTE_ETH_DEV_NO_OWNER);
		if (peer_tx_port >= RTE_MAX_ETHPORTS)
			peer_tx_port = rte_eth_find_next_owned_by(0,
						RTE_ETH_DEV_NO_OWNER);
		if (p_pi != RTE_MAX_ETHPORTS) {
			peer_rx_port = p_pi;
		} else {
			uint16_t next_pi;

			/* Last port will be the peer RX port of the first. */
			RTE_ETH_FOREACH_DEV(next_pi)
				peer_rx_port = next_pi;
		}
		manual = 1;
	} else if (hairpin_mode & 0x2) {
		if (cnt_pi & 0x1) {
			peer_rx_port = p_pi;
		} else {
			peer_rx_port = rte_eth_find_next_owned_by(pi + 1,
						RTE_ETH_DEV_NO_OWNER);
			if (peer_rx_port >= RTE_MAX_ETHPORTS)
				peer_rx_port = pi;
		}
		peer_tx_port = peer_rx_port;
		manual = 1;
	}

	for (qi = nb_txq, i = 0; qi < nb_hairpinq + nb_txq; qi++) {
		hairpin_conf.peers[0].port = peer_rx_port;
		hairpin_conf.peers[0].queue = i + nb_rxq;
		hairpin_conf.manual_bind = !!manual;
		hairpin_conf.tx_explicit = !!tx_exp;
		diag = rte_eth_tx_hairpin_queue_setup
			(pi, qi, nb_txd, &hairpin_conf);
		i++;
		if (diag == 0)
			continue;

		/* Fail to setup rx queue, return */
		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STOPPED;
		else
			fprintf(stderr,
				"Port %d can not be set back to stopped\n", pi);
		fprintf(stderr, "Fail to configure port %d hairpin queues\n",
			pi);
		/* try to reconfigure queues next time */
		port->need_reconfig_queues = 1;
		return -1;
	}
	for (qi = nb_rxq, i = 0; qi < nb_hairpinq + nb_rxq; qi++) {
		hairpin_conf.peers[0].port = peer_tx_port;
		hairpin_conf.peers[0].queue = i + nb_txq;
		hairpin_conf.manual_bind = !!manual;
		hairpin_conf.tx_explicit = !!tx_exp;
		diag = rte_eth_rx_hairpin_queue_setup
			(pi, qi, nb_rxd, &hairpin_conf);
		i++;
		if (diag == 0)
			continue;

		/* Fail to setup rx queue, return */
		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STOPPED;
		else
			fprintf(stderr,
				"Port %d can not be set back to stopped\n", pi);
		fprintf(stderr, "Fail to configure port %d hairpin queues\n",
			pi);
		/* try to reconfigure queues next time */
		port->need_reconfig_queues = 1;
		return -1;
	}
	return 0;
}

/* Configure the Rx with optional split. */
int
rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
	       uint16_t nb_rx_desc, unsigned int socket_id,
	       struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp)
{
	union rte_eth_rxseg rx_useg[MAX_SEGS_BUFFER_SPLIT] = {};
	unsigned int i, mp_n;
	int ret;

	if (rx_pkt_nb_segs <= 1 ||
	    (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) == 0) {
		rx_conf->rx_seg = NULL;
		rx_conf->rx_nseg = 0;
		ret = rte_eth_rx_queue_setup(port_id, rx_queue_id,
					     nb_rx_desc, socket_id,
					     rx_conf, mp);
		goto exit;
	}
	for (i = 0; i < rx_pkt_nb_segs; i++) {
		struct rte_eth_rxseg_split *rx_seg = &rx_useg[i].split;
		struct rte_mempool *mpx;
		/*
		 * Use last valid pool for the segments with number
		 * exceeding the pool index.
		 */
		mp_n = (i >= mbuf_data_size_n) ? mbuf_data_size_n - 1 : i;
		mpx = mbuf_pool_find(socket_id, mp_n);
		/* Handle zero as mbuf data buffer size. */
		rx_seg->length = rx_pkt_seg_lengths[i] ?
				   rx_pkt_seg_lengths[i] :
				   mbuf_data_size[mp_n];
		rx_seg->offset = i < rx_pkt_nb_offs ?
				   rx_pkt_seg_offsets[i] : 0;
		rx_seg->mp = mpx ? mpx : mp;
	}
	rx_conf->rx_nseg = rx_pkt_nb_segs;
	rx_conf->rx_seg = rx_useg;
	ret = rte_eth_rx_queue_setup(port_id, rx_queue_id, nb_rx_desc,
				    socket_id, rx_conf, NULL);
	rx_conf->rx_seg = NULL;
	rx_conf->rx_nseg = 0;
exit:
	ports[port_id].rxq[rx_queue_id].state = rx_conf->rx_deferred_start ?
						RTE_ETH_QUEUE_STATE_STOPPED :
						RTE_ETH_QUEUE_STATE_STARTED;
	return ret;
}

static int
alloc_xstats_display_info(portid_t pi)
{
	uint64_t **ids_supp = &ports[pi].xstats_info.ids_supp;
	uint64_t **prev_values = &ports[pi].xstats_info.prev_values;
	uint64_t **curr_values = &ports[pi].xstats_info.curr_values;

	if (xstats_display_num == 0)
		return 0;

	*ids_supp = calloc(xstats_display_num, sizeof(**ids_supp));
	if (*ids_supp == NULL)
		goto fail_ids_supp;

	*prev_values = calloc(xstats_display_num,
			      sizeof(**prev_values));
	if (*prev_values == NULL)
		goto fail_prev_values;

	*curr_values = calloc(xstats_display_num,
			      sizeof(**curr_values));
	if (*curr_values == NULL)
		goto fail_curr_values;

	ports[pi].xstats_info.allocated = true;

	return 0;

fail_curr_values:
	free(*prev_values);
fail_prev_values:
	free(*ids_supp);
fail_ids_supp:
	return -ENOMEM;
}

static void
free_xstats_display_info(portid_t pi)
{
	if (!ports[pi].xstats_info.allocated)
		return;
	free(ports[pi].xstats_info.ids_supp);
	free(ports[pi].xstats_info.prev_values);
	free(ports[pi].xstats_info.curr_values);
	ports[pi].xstats_info.allocated = false;
}

/** Fill helper structures for specified port to show extended statistics. */
static void
fill_xstats_display_info_for_port(portid_t pi)
{
	unsigned int stat, stat_supp;
	const char *xstat_name;
	struct rte_port *port;
	uint64_t *ids_supp;
	int rc;

	if (xstats_display_num == 0)
		return;

	if (pi == (portid_t)RTE_PORT_ALL) {
		fill_xstats_display_info();
		return;
	}

	port = &ports[pi];
	if (port->port_status != RTE_PORT_STARTED)
		return;

	if (!port->xstats_info.allocated && alloc_xstats_display_info(pi) != 0)
		rte_exit(EXIT_FAILURE,
			 "Failed to allocate xstats display memory\n");

	ids_supp = port->xstats_info.ids_supp;
	for (stat = stat_supp = 0; stat < xstats_display_num; stat++) {
		xstat_name = xstats_display[stat].name;
		rc = rte_eth_xstats_get_id_by_name(pi, xstat_name,
						   ids_supp + stat_supp);
		if (rc != 0) {
			fprintf(stderr, "No xstat '%s' on port %u - skip it %u\n",
				xstat_name, pi, stat);
			continue;
		}
		stat_supp++;
	}

	port->xstats_info.ids_supp_sz = stat_supp;
}

/** Fill helper structures for all ports to show extended statistics. */
static void
fill_xstats_display_info(void)
{
	portid_t pi;

	if (xstats_display_num == 0)
		return;

	RTE_ETH_FOREACH_DEV(pi)
		fill_xstats_display_info_for_port(pi);
}

int
start_port(portid_t pid)
{
	int diag, need_check_link_status = -1;
	portid_t pi;
	portid_t p_pi = RTE_MAX_ETHPORTS;
	portid_t pl[RTE_MAX_ETHPORTS];
	portid_t peer_pl[RTE_MAX_ETHPORTS];
	uint16_t cnt_pi = 0;
	uint16_t cfg_pi = 0;
	int peer_pi;
	queueid_t qi;
	struct rte_port *port;
	struct rte_eth_hairpin_cap cap;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return 0;

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_bonding_slave(pi)) {
			fprintf(stderr,
				"Please remove port %d from bonded device.\n",
				pi);
			continue;
		}

		need_check_link_status = 0;
		port = &ports[pi];
		if (port->port_status == RTE_PORT_STOPPED)
			port->port_status = RTE_PORT_HANDLING;
		else {
			fprintf(stderr, "Port %d is now not stopped\n", pi);
			continue;
		}

		if (port->need_reconfig > 0) {
			struct rte_eth_conf dev_conf;
			int k;

			port->need_reconfig = 0;

			if (flow_isolate_all) {
				int ret = port_flow_isolate(pi, 1);
				if (ret) {
					fprintf(stderr,
						"Failed to apply isolated mode on port %d\n",
						pi);
					return -1;
				}
			}
			configure_rxtx_dump_callbacks(0);
			printf("Configuring Port %d (socket %u)\n", pi,
					port->socket_id);
			if (nb_hairpinq > 0 &&
			    rte_eth_dev_hairpin_capability_get(pi, &cap)) {
				fprintf(stderr,
					"Port %d doesn't support hairpin queues\n",
					pi);
				return -1;
			}

			/* configure port */
			diag = eth_dev_configure_mp(pi, nb_rxq + nb_hairpinq,
						     nb_txq + nb_hairpinq,
						     &(port->dev_conf));
			if (diag != 0) {
				if (port->port_status == RTE_PORT_HANDLING)
					port->port_status = RTE_PORT_STOPPED;
				else
					fprintf(stderr,
						"Port %d can not be set back to stopped\n",
						pi);
				fprintf(stderr, "Fail to configure port %d\n",
					pi);
				/* try to reconfigure port next time */
				port->need_reconfig = 1;
				return -1;
			}
			/* get device configuration*/
			if (0 !=
				eth_dev_conf_get_print_err(pi, &dev_conf)) {
				fprintf(stderr,
					"port %d can not get device configuration\n",
					pi);
				return -1;
			}
			/* Apply Rx offloads configuration */
			if (dev_conf.rxmode.offloads !=
			    port->dev_conf.rxmode.offloads) {
				port->dev_conf.rxmode.offloads |=
					dev_conf.rxmode.offloads;
				for (k = 0;
				     k < port->dev_info.max_rx_queues;
				     k++)
					port->rxq[k].conf.offloads |=
						dev_conf.rxmode.offloads;
			}
			/* Apply Tx offloads configuration */
			if (dev_conf.txmode.offloads !=
			    port->dev_conf.txmode.offloads) {
				port->dev_conf.txmode.offloads |=
					dev_conf.txmode.offloads;
				for (k = 0;
				     k < port->dev_info.max_tx_queues;
				     k++)
					port->txq[k].conf.offloads |=
						dev_conf.txmode.offloads;
			}
		}
		if (port->need_reconfig_queues > 0 && is_proc_primary()) {
			port->need_reconfig_queues = 0;
			/* setup tx queues */
			for (qi = 0; qi < nb_txq; qi++) {
				struct rte_eth_txconf *conf =
							&port->txq[qi].conf;

				if ((numa_support) &&
					(txring_numa[pi] != NUMA_NO_CONFIG))
					diag = rte_eth_tx_queue_setup(pi, qi,
						port->nb_tx_desc[qi],
						txring_numa[pi],
						&(port->txq[qi].conf));
				else
					diag = rte_eth_tx_queue_setup(pi, qi,
						port->nb_tx_desc[qi],
						port->socket_id,
						&(port->txq[qi].conf));

				if (diag == 0) {
					port->txq[qi].state =
						conf->tx_deferred_start ?
						RTE_ETH_QUEUE_STATE_STOPPED :
						RTE_ETH_QUEUE_STATE_STARTED;
					continue;
				}

				/* Fail to setup tx queue, return */
				if (port->port_status == RTE_PORT_HANDLING)
					port->port_status = RTE_PORT_STOPPED;
				else
					fprintf(stderr,
						"Port %d can not be set back to stopped\n",
						pi);
				fprintf(stderr,
					"Fail to configure port %d tx queues\n",
					pi);
				/* try to reconfigure queues next time */
				port->need_reconfig_queues = 1;
				return -1;
			}
			for (qi = 0; qi < nb_rxq; qi++) {
				/* setup rx queues */
				if ((numa_support) &&
					(rxring_numa[pi] != NUMA_NO_CONFIG)) {
					struct rte_mempool * mp =
						mbuf_pool_find
							(rxring_numa[pi], 0);
					if (mp == NULL) {
						fprintf(stderr,
							"Failed to setup RX queue: No mempool allocation on the socket %d\n",
							rxring_numa[pi]);
						return -1;
					}

					diag = rx_queue_setup(pi, qi,
					     port->nb_rx_desc[qi],
					     rxring_numa[pi],
					     &(port->rxq[qi].conf),
					     mp);
				} else {
					struct rte_mempool *mp =
						mbuf_pool_find
							(port->socket_id, 0);
					if (mp == NULL) {
						fprintf(stderr,
							"Failed to setup RX queue: No mempool allocation on the socket %d\n",
							port->socket_id);
						return -1;
					}
					diag = rx_queue_setup(pi, qi,
					     port->nb_rx_desc[qi],
					     port->socket_id,
					     &(port->rxq[qi].conf),
					     mp);
				}
				if (diag == 0)
					continue;

				/* Fail to setup rx queue, return */
				if (port->port_status == RTE_PORT_HANDLING)
					port->port_status = RTE_PORT_STOPPED;
				else
					fprintf(stderr,
						"Port %d can not be set back to stopped\n",
						pi);
				fprintf(stderr,
					"Fail to configure port %d rx queues\n",
					pi);
				/* try to reconfigure queues next time */
				port->need_reconfig_queues = 1;
				return -1;
			}
			/* setup hairpin queues */
			if (setup_hairpin_queues(pi, p_pi, cnt_pi) != 0)
				return -1;
		}
		configure_rxtx_dump_callbacks(verbose_level);
		if (clear_ptypes) {
			diag = rte_eth_dev_set_ptypes(pi, RTE_PTYPE_UNKNOWN,
					NULL, 0);
			if (diag < 0)
				fprintf(stderr,
					"Port %d: Failed to disable Ptype parsing\n",
					pi);
		}

		p_pi = pi;
		cnt_pi++;

		/* start port */
		diag = eth_dev_start_mp(pi);
		if (diag < 0) {
			fprintf(stderr, "Fail to start port %d: %s\n",
				pi, rte_strerror(-diag));

			/* Fail to setup rx queue, return */
			if (port->port_status == RTE_PORT_HANDLING)
				port->port_status = RTE_PORT_STOPPED;
			else
				fprintf(stderr,
					"Port %d can not be set back to stopped\n",
					pi);
			continue;
		}

		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STARTED;
		else
			fprintf(stderr, "Port %d can not be set into started\n",
				pi);

		if (eth_macaddr_get_print_err(pi, &port->eth_addr) == 0)
			printf("Port %d: " RTE_ETHER_ADDR_PRT_FMT "\n", pi,
					RTE_ETHER_ADDR_BYTES(&port->eth_addr));

		/* at least one port started, need checking link status */
		need_check_link_status = 1;

		pl[cfg_pi++] = pi;
	}

	if (need_check_link_status == 1 && !no_link_check)
		check_all_ports_link_status(RTE_PORT_ALL);
	else if (need_check_link_status == 0)
		fprintf(stderr, "Please stop the ports first\n");

	if (hairpin_mode & 0xf) {
		uint16_t i;
		int j;

		/* bind all started hairpin ports */
		for (i = 0; i < cfg_pi; i++) {
			pi = pl[i];
			/* bind current Tx to all peer Rx */
			peer_pi = rte_eth_hairpin_get_peer_ports(pi, peer_pl,
							RTE_MAX_ETHPORTS, 1);
			if (peer_pi < 0)
				return peer_pi;
			for (j = 0; j < peer_pi; j++) {
				if (!port_is_started(peer_pl[j]))
					continue;
				diag = rte_eth_hairpin_bind(pi, peer_pl[j]);
				if (diag < 0) {
					fprintf(stderr,
						"Error during binding hairpin Tx port %u to %u: %s\n",
						pi, peer_pl[j],
						rte_strerror(-diag));
					return -1;
				}
			}
			/* bind all peer Tx to current Rx */
			peer_pi = rte_eth_hairpin_get_peer_ports(pi, peer_pl,
							RTE_MAX_ETHPORTS, 0);
			if (peer_pi < 0)
				return peer_pi;
			for (j = 0; j < peer_pi; j++) {
				if (!port_is_started(peer_pl[j]))
					continue;
				diag = rte_eth_hairpin_bind(peer_pl[j], pi);
				if (diag < 0) {
					fprintf(stderr,
						"Error during binding hairpin Tx port %u to %u: %s\n",
						peer_pl[j], pi,
						rte_strerror(-diag));
					return -1;
				}
			}
		}
	}

	fill_xstats_display_info_for_port(pid);

	printf("Done\n");
	return 0;
}

void
stop_port(portid_t pid)
{
	portid_t pi;
	struct rte_port *port;
	int need_check_link_status = 0;
	portid_t peer_pl[RTE_MAX_ETHPORTS];
	int peer_pi;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return;

	printf("Stopping ports...\n");

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_forwarding(pi) != 0 && test_done == 0) {
			fprintf(stderr,
				"Please remove port %d from forwarding configuration.\n",
				pi);
			continue;
		}

		if (port_is_bonding_slave(pi)) {
			fprintf(stderr,
				"Please remove port %d from bonded device.\n",
				pi);
			continue;
		}

		port = &ports[pi];
		if (port->port_status == RTE_PORT_STARTED)
			port->port_status = RTE_PORT_HANDLING;
		else
			continue;

		if (hairpin_mode & 0xf) {
			int j;

			rte_eth_hairpin_unbind(pi, RTE_MAX_ETHPORTS);
			/* unbind all peer Tx from current Rx */
			peer_pi = rte_eth_hairpin_get_peer_ports(pi, peer_pl,
							RTE_MAX_ETHPORTS, 0);
			if (peer_pi < 0)
				continue;
			for (j = 0; j < peer_pi; j++) {
				if (!port_is_started(peer_pl[j]))
					continue;
				rte_eth_hairpin_unbind(peer_pl[j], pi);
			}
		}

		if (port->flow_list)
			port_flow_flush(pi);

		if (eth_dev_stop_mp(pi) != 0)
			RTE_LOG(ERR, EAL, "rte_eth_dev_stop failed for port %u\n",
				pi);

		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STOPPED;
		else
			fprintf(stderr, "Port %d can not be set into stopped\n",
				pi);
		need_check_link_status = 1;
	}
	if (need_check_link_status && !no_link_check)
		check_all_ports_link_status(RTE_PORT_ALL);

	printf("Done\n");
}

static void
remove_invalid_ports_in(portid_t *array, portid_t *total)
{
	portid_t i;
	portid_t new_total = 0;

	for (i = 0; i < *total; i++)
		if (!port_id_is_invalid(array[i], DISABLED_WARN)) {
			array[new_total] = array[i];
			new_total++;
		}
	*total = new_total;
}

static void
remove_invalid_ports(void)
{
	remove_invalid_ports_in(ports_ids, &nb_ports);
	remove_invalid_ports_in(fwd_ports_ids, &nb_fwd_ports);
	nb_cfg_ports = nb_fwd_ports;
}

static void
flush_port_owned_resources(portid_t pi)
{
	mcast_addr_pool_destroy(pi);
	port_flow_flush(pi);
	port_flex_item_flush(pi);
	port_action_handle_flush(pi);
}

static void
clear_bonding_slave_device(portid_t *slave_pids, uint16_t num_slaves)
{
	struct rte_port *port;
	portid_t slave_pid;
	uint16_t i;

	for (i = 0; i < num_slaves; i++) {
		slave_pid = slave_pids[i];
		if (port_is_started(slave_pid) == 1) {
			if (rte_eth_dev_stop(slave_pid) != 0)
				fprintf(stderr, "rte_eth_dev_stop failed for port %u\n",
					slave_pid);

			port = &ports[slave_pid];
			port->port_status = RTE_PORT_STOPPED;
		}

		clear_port_slave_flag(slave_pid);

		/* Close slave device when testpmd quit or is killed. */
		if (cl_quit == 1 || f_quit == 1)
			rte_eth_dev_close(slave_pid);
	}
}

void
close_port(portid_t pid)
{
	portid_t pi;
	struct rte_port *port;
	portid_t slave_pids[RTE_MAX_ETHPORTS];
	int num_slaves = 0;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return;

	printf("Closing ports...\n");

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_forwarding(pi) != 0 && test_done == 0) {
			fprintf(stderr,
				"Please remove port %d from forwarding configuration.\n",
				pi);
			continue;
		}

		if (port_is_bonding_slave(pi)) {
			fprintf(stderr,
				"Please remove port %d from bonded device.\n",
				pi);
			continue;
		}

		port = &ports[pi];
		if (port->port_status == RTE_PORT_CLOSED) {
			fprintf(stderr, "Port %d is already closed\n", pi);
			continue;
		}

		if (is_proc_primary()) {
			flush_port_owned_resources(pi);
#ifdef RTE_NET_BOND
			if (port->bond_flag == 1)
				num_slaves = rte_eth_bond_slaves_get(pi,
						slave_pids, RTE_MAX_ETHPORTS);
#endif
			rte_eth_dev_close(pi);
			/*
			 * If this port is bonded device, all slaves under the
			 * device need to be removed or closed.
			 */
			if (port->bond_flag == 1 && num_slaves > 0)
				clear_bonding_slave_device(slave_pids,
							num_slaves);
		}

		free_xstats_display_info(pi);
	}

	remove_invalid_ports();
	printf("Done\n");
}

void
reset_port(portid_t pid)
{
	int diag;
	portid_t pi;
	struct rte_port *port;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return;

	if ((pid == (portid_t)RTE_PORT_ALL && !all_ports_stopped()) ||
		(pid != (portid_t)RTE_PORT_ALL && !port_is_stopped(pid))) {
		fprintf(stderr,
			"Can not reset port(s), please stop port(s) first.\n");
		return;
	}

	printf("Resetting ports...\n");

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_forwarding(pi) != 0 && test_done == 0) {
			fprintf(stderr,
				"Please remove port %d from forwarding configuration.\n",
				pi);
			continue;
		}

		if (port_is_bonding_slave(pi)) {
			fprintf(stderr,
				"Please remove port %d from bonded device.\n",
				pi);
			continue;
		}

		diag = rte_eth_dev_reset(pi);
		if (diag == 0) {
			port = &ports[pi];
			port->need_reconfig = 1;
			port->need_reconfig_queues = 1;
		} else {
			fprintf(stderr, "Failed to reset port %d. diag=%d\n",
				pi, diag);
		}
	}

	printf("Done\n");
}

void
attach_port(char *identifier)
{
	portid_t pi;
	struct rte_dev_iterator iterator;

	printf("Attaching a new port...\n");

	if (identifier == NULL) {
		fprintf(stderr, "Invalid parameters are specified\n");
		return;
	}

	if (rte_dev_probe(identifier) < 0) {
		TESTPMD_LOG(ERR, "Failed to attach port %s\n", identifier);
		return;
	}

	/* first attach mode: event */
	if (setup_on_probe_event) {
		/* new ports are detected on RTE_ETH_EVENT_NEW event */
		for (pi = 0; pi < RTE_MAX_ETHPORTS; pi++)
			if (ports[pi].port_status == RTE_PORT_HANDLING &&
					ports[pi].need_setup != 0)
				setup_attached_port(pi);
		return;
	}

	/* second attach mode: iterator */
	RTE_ETH_FOREACH_MATCHING_DEV(pi, identifier, &iterator) {
		/* setup ports matching the devargs used for probing */
		if (port_is_forwarding(pi))
			continue; /* port was already attached before */
		setup_attached_port(pi);
	}
}

static void
setup_attached_port(portid_t pi)
{
	unsigned int socket_id;
	int ret;

	socket_id = (unsigned)rte_eth_dev_socket_id(pi);
	/* if socket_id is invalid, set to the first available socket. */
	if (check_socket_id(socket_id) < 0)
		socket_id = socket_ids[0];
	reconfig(pi, socket_id);
	ret = rte_eth_promiscuous_enable(pi);
	if (ret != 0)
		fprintf(stderr,
			"Error during enabling promiscuous mode for port %u: %s - ignore\n",
			pi, rte_strerror(-ret));

	ports_ids[nb_ports++] = pi;
	fwd_ports_ids[nb_fwd_ports++] = pi;
	nb_cfg_ports = nb_fwd_ports;
	ports[pi].need_setup = 0;
	ports[pi].port_status = RTE_PORT_STOPPED;

	printf("Port %d is attached. Now total ports is %d\n", pi, nb_ports);
	printf("Done\n");
}

static void
detach_device(struct rte_device *dev)
{
	portid_t sibling;

	if (dev == NULL) {
		fprintf(stderr, "Device already removed\n");
		return;
	}

	printf("Removing a device...\n");

	RTE_ETH_FOREACH_DEV_OF(sibling, dev) {
		if (ports[sibling].port_status != RTE_PORT_CLOSED) {
			if (ports[sibling].port_status != RTE_PORT_STOPPED) {
				fprintf(stderr, "Port %u not stopped\n",
					sibling);
				return;
			}
			flush_port_owned_resources(sibling);
		}
	}

	if (rte_dev_remove(dev) < 0) {
		TESTPMD_LOG(ERR, "Failed to detach device %s\n", dev->name);
		return;
	}
	remove_invalid_ports();

	printf("Device is detached\n");
	printf("Now total ports is %d\n", nb_ports);
	printf("Done\n");
	return;
}

void
detach_port_device(portid_t port_id)
{
	int ret;
	struct rte_eth_dev_info dev_info;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	if (ports[port_id].port_status != RTE_PORT_CLOSED) {
		if (ports[port_id].port_status != RTE_PORT_STOPPED) {
			fprintf(stderr, "Port not stopped\n");
			return;
		}
		fprintf(stderr, "Port was not closed\n");
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0) {
		TESTPMD_LOG(ERR,
			"Failed to get device info for port %d, not detaching\n",
			port_id);
		return;
	}
	detach_device(dev_info.device);
}

void
detach_devargs(char *identifier)
{
	struct rte_dev_iterator iterator;
	struct rte_devargs da;
	portid_t port_id;

	printf("Removing a device...\n");

	memset(&da, 0, sizeof(da));
	if (rte_devargs_parsef(&da, "%s", identifier)) {
		fprintf(stderr, "cannot parse identifier\n");
		return;
	}

	RTE_ETH_FOREACH_MATCHING_DEV(port_id, identifier, &iterator) {
		if (ports[port_id].port_status != RTE_PORT_CLOSED) {
			if (ports[port_id].port_status != RTE_PORT_STOPPED) {
				fprintf(stderr, "Port %u not stopped\n",
					port_id);
				rte_eth_iterator_cleanup(&iterator);
				rte_devargs_reset(&da);
				return;
			}
			flush_port_owned_resources(port_id);
		}
	}

	if (rte_eal_hotplug_remove(da.bus->name, da.name) != 0) {
		TESTPMD_LOG(ERR, "Failed to detach device %s(%s)\n",
			    da.name, da.bus->name);
		rte_devargs_reset(&da);
		return;
	}

	remove_invalid_ports();

	printf("Device %s is detached\n", identifier);
	printf("Now total ports is %d\n", nb_ports);
	printf("Done\n");
	rte_devargs_reset(&da);
}

void
pmd_test_exit(void)
{
	portid_t pt_id;
	unsigned int i;
	int ret;

	if (test_done == 0)
		stop_packet_forwarding();

#ifndef RTE_EXEC_ENV_WINDOWS
	for (i = 0 ; i < RTE_DIM(mempools) ; i++) {
		if (mempools[i]) {
			if (mp_alloc_type == MP_ALLOC_ANON)
				rte_mempool_mem_iter(mempools[i], dma_unmap_cb,
						     NULL);
		}
	}
#endif
	if (ports != NULL) {
		no_link_check = 1;
		RTE_ETH_FOREACH_DEV(pt_id) {
			printf("\nStopping port %d...\n", pt_id);
			fflush(stdout);
			stop_port(pt_id);
		}
		RTE_ETH_FOREACH_DEV(pt_id) {
			printf("\nShutting down port %d...\n", pt_id);
			fflush(stdout);
			close_port(pt_id);
		}
	}

	if (hot_plug) {
		ret = rte_dev_event_monitor_stop();
		if (ret) {
			RTE_LOG(ERR, EAL,
				"fail to stop device event monitor.");
			return;
		}

		ret = rte_dev_event_callback_unregister(NULL,
			dev_event_callback, NULL);
		if (ret < 0) {
			RTE_LOG(ERR, EAL,
				"fail to unregister device event callback.\n");
			return;
		}

		ret = rte_dev_hotplug_handle_disable();
		if (ret) {
			RTE_LOG(ERR, EAL,
				"fail to disable hotplug handling.\n");
			return;
		}
	}
	for (i = 0 ; i < RTE_DIM(mempools) ; i++) {
		if (mempools[i])
			mempool_free_mp(mempools[i]);
	}
	free(xstats_display);

	printf("\nBye...\n");
}

typedef void (*cmd_func_t)(void);
struct pmd_test_command {
	const char *cmd_name;
	cmd_func_t cmd_func;
};

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	portid_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status[RTE_ETH_LINK_MAX_STR_LEN];

	printf("Checking link statuses...\n");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					fprintf(stderr,
						"Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status,
					sizeof(link_status), &link);
				printf("Port %d %s\n", portid, link_status);
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
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
		}

		if (lsc_interrupt)
			break;
	}
}

static void
rmv_port_callback(void *arg)
{
	int need_to_start = 0;
	int org_no_link_check = no_link_check;
	portid_t port_id = (intptr_t)arg;
	struct rte_eth_dev_info dev_info;
	int ret;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);

	if (!test_done && port_is_forwarding(port_id)) {
		need_to_start = 1;
		stop_packet_forwarding();
	}
	no_link_check = 1;
	stop_port(port_id);
	no_link_check = org_no_link_check;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		TESTPMD_LOG(ERR,
			"Failed to get device info for port %d, not detaching\n",
			port_id);
	else {
		struct rte_device *device = dev_info.device;
		close_port(port_id);
		detach_device(device); /* might be already removed or have more ports */
	}
	if (need_to_start)
		start_packet_forwarding(0);
}

/* This function is used by the interrupt thread */
static int
eth_event_callback(portid_t port_id, enum rte_eth_event_type type, void *param,
		  void *ret_param)
{
	RTE_SET_USED(param);
	RTE_SET_USED(ret_param);

	if (type >= RTE_ETH_EVENT_MAX) {
		fprintf(stderr,
			"\nPort %" PRIu16 ": %s called upon invalid event %d\n",
			port_id, __func__, type);
		fflush(stderr);
	} else if (event_print_mask & (UINT32_C(1) << type)) {
		printf("\nPort %" PRIu16 ": %s event\n", port_id,
			eth_event_desc[type]);
		fflush(stdout);
	}

	switch (type) {
	case RTE_ETH_EVENT_NEW:
		ports[port_id].need_setup = 1;
		ports[port_id].port_status = RTE_PORT_HANDLING;
		break;
	case RTE_ETH_EVENT_INTR_RMV:
		if (port_id_is_invalid(port_id, DISABLED_WARN))
			break;
		if (rte_eal_alarm_set(100000,
				rmv_port_callback, (void *)(intptr_t)port_id))
			fprintf(stderr,
				"Could not set up deferred device removal\n");
		break;
	case RTE_ETH_EVENT_DESTROY:
		ports[port_id].port_status = RTE_PORT_CLOSED;
		printf("Port %u is closed\n", port_id);
		break;
	default:
		break;
	}
	return 0;
}

static int
register_eth_event_callback(void)
{
	int ret;
	enum rte_eth_event_type event;

	for (event = RTE_ETH_EVENT_UNKNOWN;
			event < RTE_ETH_EVENT_MAX; event++) {
		ret = rte_eth_dev_callback_register(RTE_ETH_ALL,
				event,
				eth_event_callback,
				NULL);
		if (ret != 0) {
			TESTPMD_LOG(ERR, "Failed to register callback for "
					"%s event\n", eth_event_desc[event]);
			return -1;
		}
	}

	return 0;
}

/* This function is used by the interrupt thread */
static void
dev_event_callback(const char *device_name, enum rte_dev_event_type type,
			     __rte_unused void *arg)
{
	uint16_t port_id;
	int ret;

	if (type >= RTE_DEV_EVENT_MAX) {
		fprintf(stderr, "%s called upon invalid event %d\n",
			__func__, type);
		fflush(stderr);
	}

	switch (type) {
	case RTE_DEV_EVENT_REMOVE:
		RTE_LOG(DEBUG, EAL, "The device: %s has been removed!\n",
			device_name);
		ret = rte_eth_dev_get_port_by_name(device_name, &port_id);
		if (ret) {
			RTE_LOG(ERR, EAL, "can not get port by device %s!\n",
				device_name);
			return;
		}
		/*
		 * Because the user's callback is invoked in eal interrupt
		 * callback, the interrupt callback need to be finished before
		 * it can be unregistered when detaching device. So finish
		 * callback soon and use a deferred removal to detach device
		 * is need. It is a workaround, once the device detaching be
		 * moved into the eal in the future, the deferred removal could
		 * be deleted.
		 */
		if (rte_eal_alarm_set(100000,
				rmv_port_callback, (void *)(intptr_t)port_id))
			RTE_LOG(ERR, EAL,
				"Could not set up deferred device removal\n");
		break;
	case RTE_DEV_EVENT_ADD:
		RTE_LOG(ERR, EAL, "The device: %s has been added!\n",
			device_name);
		/* TODO: After finish kernel driver binding,
		 * begin to attach port.
		 */
		break;
	default:
		break;
	}
}

static void
rxtx_port_config(portid_t pid)
{
	uint16_t qid;
	uint64_t offloads;
	struct rte_port *port = &ports[pid];

	for (qid = 0; qid < nb_rxq; qid++) {
		offloads = port->rxq[qid].conf.offloads;
		port->rxq[qid].conf = port->dev_info.default_rxconf;

		if (rxq_share > 0 &&
		    (port->dev_info.dev_capa & RTE_ETH_DEV_CAPA_RXQ_SHARE)) {
			/* Non-zero share group to enable RxQ share. */
			port->rxq[qid].conf.share_group = pid / rxq_share + 1;
			port->rxq[qid].conf.share_qid = qid; /* Equal mapping. */
		}

		if (offloads != 0)
			port->rxq[qid].conf.offloads = offloads;

		/* Check if any Rx parameters have been passed */
		if (rx_pthresh != RTE_PMD_PARAM_UNSET)
			port->rxq[qid].conf.rx_thresh.pthresh = rx_pthresh;

		if (rx_hthresh != RTE_PMD_PARAM_UNSET)
			port->rxq[qid].conf.rx_thresh.hthresh = rx_hthresh;

		if (rx_wthresh != RTE_PMD_PARAM_UNSET)
			port->rxq[qid].conf.rx_thresh.wthresh = rx_wthresh;

		if (rx_free_thresh != RTE_PMD_PARAM_UNSET)
			port->rxq[qid].conf.rx_free_thresh = rx_free_thresh;

		if (rx_drop_en != RTE_PMD_PARAM_UNSET)
			port->rxq[qid].conf.rx_drop_en = rx_drop_en;

		port->nb_rx_desc[qid] = nb_rxd;
	}

	for (qid = 0; qid < nb_txq; qid++) {
		offloads = port->txq[qid].conf.offloads;
		port->txq[qid].conf = port->dev_info.default_txconf;
		if (offloads != 0)
			port->txq[qid].conf.offloads = offloads;

		/* Check if any Tx parameters have been passed */
		if (tx_pthresh != RTE_PMD_PARAM_UNSET)
			port->txq[qid].conf.tx_thresh.pthresh = tx_pthresh;

		if (tx_hthresh != RTE_PMD_PARAM_UNSET)
			port->txq[qid].conf.tx_thresh.hthresh = tx_hthresh;

		if (tx_wthresh != RTE_PMD_PARAM_UNSET)
			port->txq[qid].conf.tx_thresh.wthresh = tx_wthresh;

		if (tx_rs_thresh != RTE_PMD_PARAM_UNSET)
			port->txq[qid].conf.tx_rs_thresh = tx_rs_thresh;

		if (tx_free_thresh != RTE_PMD_PARAM_UNSET)
			port->txq[qid].conf.tx_free_thresh = tx_free_thresh;

		port->nb_tx_desc[qid] = nb_txd;
	}
}

/*
 * Helper function to set MTU from frame size
 *
 * port->dev_info should be set before calling this function.
 *
 * return 0 on success, negative on error
 */
int
update_mtu_from_frame_size(portid_t portid, uint32_t max_rx_pktlen)
{
	struct rte_port *port = &ports[portid];
	uint32_t eth_overhead;
	uint16_t mtu, new_mtu;

	eth_overhead = get_eth_overhead(&port->dev_info);

	if (rte_eth_dev_get_mtu(portid, &mtu) != 0) {
		printf("Failed to get MTU for port %u\n", portid);
		return -1;
	}

	new_mtu = max_rx_pktlen - eth_overhead;

	if (mtu == new_mtu)
		return 0;

	if (eth_dev_set_mtu_mp(portid, new_mtu) != 0) {
		fprintf(stderr,
			"Failed to set MTU to %u for port %u\n",
			new_mtu, portid);
		return -1;
	}

	port->dev_conf.rxmode.mtu = new_mtu;

	return 0;
}

void
init_port_config(void)
{
	portid_t pid;
	struct rte_port *port;
	int ret, i;

	RTE_ETH_FOREACH_DEV(pid) {
		port = &ports[pid];
		port->dev_conf.fdir_conf = fdir_conf;

		ret = eth_dev_info_get_print_err(pid, &port->dev_info);
		if (ret != 0)
			return;

		if (nb_rxq > 1) {
			port->dev_conf.rx_adv_conf.rss_conf.rss_key = NULL;
			port->dev_conf.rx_adv_conf.rss_conf.rss_hf =
				rss_hf & port->dev_info.flow_type_rss_offloads;
		} else {
			port->dev_conf.rx_adv_conf.rss_conf.rss_key = NULL;
			port->dev_conf.rx_adv_conf.rss_conf.rss_hf = 0;
		}

		if (port->dcb_flag == 0) {
			if (port->dev_conf.rx_adv_conf.rss_conf.rss_hf != 0) {
				port->dev_conf.rxmode.mq_mode =
					(enum rte_eth_rx_mq_mode)
						(rx_mq_mode & RTE_ETH_MQ_RX_RSS);
			} else {
				port->dev_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
				port->dev_conf.rxmode.offloads &=
						~RTE_ETH_RX_OFFLOAD_RSS_HASH;

				for (i = 0;
				     i < port->dev_info.nb_rx_queues;
				     i++)
					port->rxq[i].conf.offloads &=
						~RTE_ETH_RX_OFFLOAD_RSS_HASH;
			}
		}

		rxtx_port_config(pid);

		ret = eth_macaddr_get_print_err(pid, &port->eth_addr);
		if (ret != 0)
			return;

#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
		rte_pmd_ixgbe_bypass_init(pid);
#endif

		if (lsc_interrupt && (*port->dev_info.dev_flags & RTE_ETH_DEV_INTR_LSC))
			port->dev_conf.intr_conf.lsc = 1;
		if (rmv_interrupt && (*port->dev_info.dev_flags & RTE_ETH_DEV_INTR_RMV))
			port->dev_conf.intr_conf.rmv = 1;
	}
}

void set_port_slave_flag(portid_t slave_pid)
{
	struct rte_port *port;

	port = &ports[slave_pid];
	port->slave_flag = 1;
}

void clear_port_slave_flag(portid_t slave_pid)
{
	struct rte_port *port;

	port = &ports[slave_pid];
	port->slave_flag = 0;
}

uint8_t port_is_bonding_slave(portid_t slave_pid)
{
	struct rte_port *port;
	struct rte_eth_dev_info dev_info;
	int ret;

	port = &ports[slave_pid];
	ret = eth_dev_info_get_print_err(slave_pid, &dev_info);
	if (ret != 0) {
		TESTPMD_LOG(ERR,
			"Failed to get device info for port id %d,"
			"cannot determine if the port is a bonded slave",
			slave_pid);
		return 0;
	}
	if ((*dev_info.dev_flags & RTE_ETH_DEV_BONDED_SLAVE) || (port->slave_flag == 1))
		return 1;
	return 0;
}

const uint16_t vlan_tags[] = {
		0,  1,  2,  3,  4,  5,  6,  7,
		8,  9, 10, 11,  12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31
};

static  int
get_eth_dcb_conf(portid_t pid, struct rte_eth_conf *eth_conf,
		 enum dcb_mode_enable dcb_mode,
		 enum rte_eth_nb_tcs num_tcs,
		 uint8_t pfc_en)
{
	uint8_t i;
	int32_t rc;
	struct rte_eth_rss_conf rss_conf;

	/*
	 * Builds up the correct configuration for dcb+vt based on the vlan tags array
	 * given above, and the number of traffic classes available for use.
	 */
	if (dcb_mode == DCB_VT_ENABLED) {
		struct rte_eth_vmdq_dcb_conf *vmdq_rx_conf =
				&eth_conf->rx_adv_conf.vmdq_dcb_conf;
		struct rte_eth_vmdq_dcb_tx_conf *vmdq_tx_conf =
				&eth_conf->tx_adv_conf.vmdq_dcb_tx_conf;

		/* VMDQ+DCB RX and TX configurations */
		vmdq_rx_conf->enable_default_pool = 0;
		vmdq_rx_conf->default_pool = 0;
		vmdq_rx_conf->nb_queue_pools =
			(num_tcs ==  RTE_ETH_4_TCS ? RTE_ETH_32_POOLS : RTE_ETH_16_POOLS);
		vmdq_tx_conf->nb_queue_pools =
			(num_tcs ==  RTE_ETH_4_TCS ? RTE_ETH_32_POOLS : RTE_ETH_16_POOLS);

		vmdq_rx_conf->nb_pool_maps = vmdq_rx_conf->nb_queue_pools;
		for (i = 0; i < vmdq_rx_conf->nb_pool_maps; i++) {
			vmdq_rx_conf->pool_map[i].vlan_id = vlan_tags[i];
			vmdq_rx_conf->pool_map[i].pools =
				1 << (i % vmdq_rx_conf->nb_queue_pools);
		}
		for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
			vmdq_rx_conf->dcb_tc[i] = i % num_tcs;
			vmdq_tx_conf->dcb_tc[i] = i % num_tcs;
		}

		/* set DCB mode of RX and TX of multiple queues */
		eth_conf->rxmode.mq_mode =
				(enum rte_eth_rx_mq_mode)
					(rx_mq_mode & RTE_ETH_MQ_RX_VMDQ_DCB);
		eth_conf->txmode.mq_mode = RTE_ETH_MQ_TX_VMDQ_DCB;
	} else {
		struct rte_eth_dcb_rx_conf *rx_conf =
				&eth_conf->rx_adv_conf.dcb_rx_conf;
		struct rte_eth_dcb_tx_conf *tx_conf =
				&eth_conf->tx_adv_conf.dcb_tx_conf;

		memset(&rss_conf, 0, sizeof(struct rte_eth_rss_conf));

		rc = rte_eth_dev_rss_hash_conf_get(pid, &rss_conf);
		if (rc != 0)
			return rc;

		rx_conf->nb_tcs = num_tcs;
		tx_conf->nb_tcs = num_tcs;

		for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
			rx_conf->dcb_tc[i] = i % num_tcs;
			tx_conf->dcb_tc[i] = i % num_tcs;
		}

		eth_conf->rxmode.mq_mode =
				(enum rte_eth_rx_mq_mode)
					(rx_mq_mode & RTE_ETH_MQ_RX_DCB_RSS);
		eth_conf->rx_adv_conf.rss_conf = rss_conf;
		eth_conf->txmode.mq_mode = RTE_ETH_MQ_TX_DCB;
	}

	if (pfc_en)
		eth_conf->dcb_capability_en =
				RTE_ETH_DCB_PG_SUPPORT | RTE_ETH_DCB_PFC_SUPPORT;
	else
		eth_conf->dcb_capability_en = RTE_ETH_DCB_PG_SUPPORT;

	return 0;
}

int
init_port_dcb_config(portid_t pid,
		     enum dcb_mode_enable dcb_mode,
		     enum rte_eth_nb_tcs num_tcs,
		     uint8_t pfc_en)
{
	struct rte_eth_conf port_conf;
	struct rte_port *rte_port;
	int retval;
	uint16_t i;

	if (num_procs > 1) {
		printf("The multi-process feature doesn't support dcb.\n");
		return -ENOTSUP;
	}
	rte_port = &ports[pid];

	/* retain the original device configuration. */
	memcpy(&port_conf, &rte_port->dev_conf, sizeof(struct rte_eth_conf));

	/*set configuration of DCB in vt mode and DCB in non-vt mode*/
	retval = get_eth_dcb_conf(pid, &port_conf, dcb_mode, num_tcs, pfc_en);
	if (retval < 0)
		return retval;
	port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
	/* remove RSS HASH offload for DCB in vt mode */
	if (port_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_VMDQ_DCB) {
		port_conf.rxmode.offloads &= ~RTE_ETH_RX_OFFLOAD_RSS_HASH;
		for (i = 0; i < nb_rxq; i++)
			rte_port->rxq[i].conf.offloads &=
				~RTE_ETH_RX_OFFLOAD_RSS_HASH;
	}

	/* re-configure the device . */
	retval = rte_eth_dev_configure(pid, nb_rxq, nb_rxq, &port_conf);
	if (retval < 0)
		return retval;

	retval = eth_dev_info_get_print_err(pid, &rte_port->dev_info);
	if (retval != 0)
		return retval;

	/* If dev_info.vmdq_pool_base is greater than 0,
	 * the queue id of vmdq pools is started after pf queues.
	 */
	if (dcb_mode == DCB_VT_ENABLED &&
	    rte_port->dev_info.vmdq_pool_base > 0) {
		fprintf(stderr,
			"VMDQ_DCB multi-queue mode is nonsensical for port %d.\n",
			pid);
		return -1;
	}

	/* Assume the ports in testpmd have the same dcb capability
	 * and has the same number of rxq and txq in dcb mode
	 */
	if (dcb_mode == DCB_VT_ENABLED) {
		if (rte_port->dev_info.max_vfs > 0) {
			nb_rxq = rte_port->dev_info.nb_rx_queues;
			nb_txq = rte_port->dev_info.nb_tx_queues;
		} else {
			nb_rxq = rte_port->dev_info.max_rx_queues;
			nb_txq = rte_port->dev_info.max_tx_queues;
		}
	} else {
		/*if vt is disabled, use all pf queues */
		if (rte_port->dev_info.vmdq_pool_base == 0) {
			nb_rxq = rte_port->dev_info.max_rx_queues;
			nb_txq = rte_port->dev_info.max_tx_queues;
		} else {
			nb_rxq = (queueid_t)num_tcs;
			nb_txq = (queueid_t)num_tcs;

		}
	}
	rx_free_thresh = 64;

	memcpy(&rte_port->dev_conf, &port_conf, sizeof(struct rte_eth_conf));

	rxtx_port_config(pid);
	/* VLAN filter */
	rte_port->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
	for (i = 0; i < RTE_DIM(vlan_tags); i++)
		rx_vft_set(pid, vlan_tags[i], 1);

	retval = eth_macaddr_get_print_err(pid, &rte_port->eth_addr);
	if (retval != 0)
		return retval;

	rte_port->dcb_flag = 1;

	/* Enter DCB configuration status */
	dcb_config = 1;

	return 0;
}

static void
init_port(void)
{
	int i;

	/* Configuration of Ethernet ports. */
	ports = rte_zmalloc("testpmd: ports",
			    sizeof(struct rte_port) * RTE_MAX_ETHPORTS,
			    RTE_CACHE_LINE_SIZE);
	if (ports == NULL) {
		rte_exit(EXIT_FAILURE,
				"rte_zmalloc(%d struct rte_port) failed\n",
				RTE_MAX_ETHPORTS);
	}
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		ports[i].xstats_info.allocated = false;
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		LIST_INIT(&ports[i].flow_tunnel_list);
	/* Initialize ports NUMA structures */
	memset(port_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
	memset(rxring_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
	memset(txring_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
}

static void
force_quit(void)
{
	pmd_test_exit();
	prompt_exit();
}

static void
print_stats(void)
{
	uint8_t i;
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char top_left[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	/* Clear screen and move to top left */
	printf("%s%s", clr, top_left);

	printf("\nPort statistics ====================================");
	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++)
		nic_stats_display(fwd_ports_ids[i]);

	fflush(stdout);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		fprintf(stderr, "\nSignal %d received, preparing to exit...\n",
			signum);
#ifdef RTE_LIB_PDUMP
		/* uninitialize packet capture framework */
		rte_pdump_uninit();
#endif
#ifdef RTE_LIB_LATENCYSTATS
		if (latencystats_enabled != 0)
			rte_latencystats_uninit();
#endif
		force_quit();
		/* Set flag to indicate the force termination. */
		f_quit = 1;
		/* exit with the expected status */
#ifndef RTE_EXEC_ENV_WINDOWS
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
#endif
	}
}

int
main(int argc, char** argv)
{
	int diag;
	portid_t port_id;
	uint16_t count;
	int ret;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	testpmd_logtype = rte_log_register("testpmd");
	if (testpmd_logtype < 0)
		rte_exit(EXIT_FAILURE, "Cannot register log type");
	rte_log_set_level(testpmd_logtype, RTE_LOG_DEBUG);

	diag = rte_eal_init(argc, argv);
	if (diag < 0)
		rte_exit(EXIT_FAILURE, "Cannot init EAL: %s\n",
			 rte_strerror(rte_errno));

	ret = register_eth_event_callback();
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "Cannot register for ethdev events");

#ifdef RTE_LIB_PDUMP
	/* initialize packet capture framework */
	rte_pdump_init();
#endif

	count = 0;
	RTE_ETH_FOREACH_DEV(port_id) {
		ports_ids[count] = port_id;
		count++;
	}
	nb_ports = (portid_t) count;
	if (nb_ports == 0)
		TESTPMD_LOG(WARNING, "No probed ethernet devices\n");

	/* allocate port structures, and init them */
	init_port();

	set_def_fwd_config();
	if (nb_lcores == 0)
		rte_exit(EXIT_FAILURE, "No cores defined for forwarding\n"
			 "Check the core mask argument\n");

	/* Bitrate/latency stats disabled by default */
#ifdef RTE_LIB_BITRATESTATS
	bitrate_enabled = 0;
#endif
#ifdef RTE_LIB_LATENCYSTATS
	latencystats_enabled = 0;
#endif

	/* on FreeBSD, mlockall() is disabled by default */
#ifdef RTE_EXEC_ENV_FREEBSD
	do_mlockall = 0;
#else
	do_mlockall = 1;
#endif

	argc -= diag;
	argv += diag;
	if (argc > 1)
		launch_args_parse(argc, argv);

#ifndef RTE_EXEC_ENV_WINDOWS
	if (do_mlockall && mlockall(MCL_CURRENT | MCL_FUTURE)) {
		TESTPMD_LOG(NOTICE, "mlockall() failed with error \"%s\"\n",
			strerror(errno));
	}
#endif

	if (tx_first && interactive)
		rte_exit(EXIT_FAILURE, "--tx-first cannot be used on "
				"interactive mode.\n");

	if (tx_first && lsc_interrupt) {
		fprintf(stderr,
			"Warning: lsc_interrupt needs to be off when using tx_first. Disabling.\n");
		lsc_interrupt = 0;
	}

	if (!nb_rxq && !nb_txq)
		fprintf(stderr,
			"Warning: Either rx or tx queues should be non-zero\n");

	if (nb_rxq > 1 && nb_rxq > nb_txq)
		fprintf(stderr,
			"Warning: nb_rxq=%d enables RSS configuration, but nb_txq=%d will prevent to fully test it.\n",
			nb_rxq, nb_txq);

	init_config();

	if (hot_plug) {
		ret = rte_dev_hotplug_handle_enable();
		if (ret) {
			RTE_LOG(ERR, EAL,
				"fail to enable hotplug handling.");
			return -1;
		}

		ret = rte_dev_event_monitor_start();
		if (ret) {
			RTE_LOG(ERR, EAL,
				"fail to start device event monitoring.");
			return -1;
		}

		ret = rte_dev_event_callback_register(NULL,
			dev_event_callback, NULL);
		if (ret) {
			RTE_LOG(ERR, EAL,
				"fail  to register device event callback\n");
			return -1;
		}
	}

	if (!no_device_start && start_port(RTE_PORT_ALL) != 0)
		rte_exit(EXIT_FAILURE, "Start ports failed\n");

	/* set all ports to promiscuous mode by default */
	RTE_ETH_FOREACH_DEV(port_id) {
		ret = rte_eth_promiscuous_enable(port_id);
		if (ret != 0)
			fprintf(stderr,
				"Error during enabling promiscuous mode for port %u: %s - ignore\n",
				port_id, rte_strerror(-ret));
	}

#ifdef RTE_LIB_METRICS
	/* Init metrics library */
	rte_metrics_init(rte_socket_id());
#endif

#ifdef RTE_LIB_LATENCYSTATS
	if (latencystats_enabled != 0) {
		int ret = rte_latencystats_init(1, NULL);
		if (ret)
			fprintf(stderr,
				"Warning: latencystats init() returned error %d\n",
				ret);
		fprintf(stderr, "Latencystats running on lcore %d\n",
			latencystats_lcore_id);
	}
#endif

	/* Setup bitrate stats */
#ifdef RTE_LIB_BITRATESTATS
	if (bitrate_enabled != 0) {
		bitrate_data = rte_stats_bitrate_create();
		if (bitrate_data == NULL)
			rte_exit(EXIT_FAILURE,
				"Could not allocate bitrate data.\n");
		rte_stats_bitrate_reg(bitrate_data);
	}
#endif
#ifdef RTE_LIB_CMDLINE
	if (strlen(cmdline_filename) != 0)
		cmdline_read_from_file(cmdline_filename);

	if (interactive == 1) {
		if (auto_start) {
			printf("Start automatic packet forwarding\n");
			start_packet_forwarding(0);
		}
		prompt();
		pmd_test_exit();
	} else
#endif
	{
		char c;
		int rc;

		f_quit = 0;

		printf("No commandline core given, start packet forwarding\n");
		start_packet_forwarding(tx_first);
		if (stats_period != 0) {
			uint64_t prev_time = 0, cur_time, diff_time = 0;
			uint64_t timer_period;

			/* Convert to number of cycles */
			timer_period = stats_period * rte_get_timer_hz();

			while (f_quit == 0) {
				cur_time = rte_get_timer_cycles();
				diff_time += cur_time - prev_time;

				if (diff_time >= timer_period) {
					print_stats();
					/* Reset the timer */
					diff_time = 0;
				}
				/* Sleep to avoid unnecessary checks */
				prev_time = cur_time;
				rte_delay_us_sleep(US_PER_S);
			}
		}

		printf("Press enter to exit\n");
		rc = read(0, &c, 1);
		pmd_test_exit();
		if (rc < 0)
			return 1;
	}

	ret = rte_eal_cleanup();
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			 "EAL cleanup failed: %s\n", strerror(-ret));

	return EXIT_SUCCESS;
}
