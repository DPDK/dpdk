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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

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
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_string_fns.h>
#ifdef RTE_LIBRTE_IXGBE_PMD
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_LIBRTE_PMD_XENVIRT
#include <rte_eth_xenvirt.h>
#endif
#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif
#include <rte_flow.h>
#include <rte_metrics.h>
#ifdef RTE_LIBRTE_BITRATE
#include <rte_bitrate.h>
#endif
#ifdef RTE_LIBRTE_LATENCY_STATS
#include <rte_latencystats.h>
#endif
#include <rte_gro.h>

#include "testpmd.h"

uint16_t verbose_level = 0; /**< Silent by default. */

/* use master core for command line ? */
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
 * Use ANONYMOUS mapped memory (might be not physically continuous) for mbufs.
 */
uint8_t mp_anon = 0;

/*
 * Record the Ethernet address of peer target ports to which packets are
 * forwarded.
 * Must be instantiated with the ethernet addresses of peer traffic generator
 * ports.
 */
struct ether_addr peer_eth_addrs[RTE_MAX_ETHPORTS];
portid_t nb_peer_eth_addrs = 0;

/*
 * Probed Target Environment.
 */
struct rte_port *ports;	       /**< For all probed ethernet ports. */
portid_t nb_ports;             /**< Number of probed ethernet ports. */
struct fwd_lcore **fwd_lcores; /**< For all probed logical cores. */
lcoreid_t nb_lcores;           /**< Number of probed logical cores. */

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
#ifdef RTE_LIBRTE_IEEE1588
	&ieee1588_fwd_engine,
#endif
	NULL,
};

struct fwd_config cur_fwd_config;
struct fwd_engine *cur_fwd_eng = &io_fwd_engine; /**< IO mode by default. */
uint32_t retry_enabled;
uint32_t burst_tx_delay_time = BURST_TX_WAIT_US;
uint32_t burst_tx_retry_num = BURST_TX_RETRIES;

uint16_t mbuf_data_size = DEFAULT_MBUF_DATA_SIZE; /**< Mbuf data space size. */
uint32_t param_total_num_mbufs = 0;  /**< number of mbufs in all pools - if
                                      * specified on command-line. */
uint16_t stats_period; /**< Period to show statistics (disabled by default) */
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

uint16_t nb_pkt_per_burst = DEF_PKT_BURST; /**< Number of packets per burst. */
uint16_t mb_mempool_cache = DEF_MBUF_CACHE; /**< Size of mbuf mempool cache. */

/* current configuration is in DCB or not,0 means it is not in DCB mode */
uint8_t dcb_config = 0;

/* Whether the dcb is in testing status */
uint8_t dcb_test = 0;

/*
 * Configurable number of RX/TX queues.
 */
queueid_t nb_rxq = 1; /**< Number of RX queues per port. */
queueid_t nb_txq = 1; /**< Number of TX queues per port. */

/*
 * Configurable number of RX/TX ring descriptors.
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
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
 * Configurable value of TX queue flags.
 */
int32_t txq_flags = RTE_PMD_PARAM_UNSET;

/*
 * Receive Side Scaling (RSS) configuration.
 */
uint64_t rss_hf = ETH_RSS_IP; /* RSS IP by default. */

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
 * Enable link status change notification
 */
uint8_t lsc_interrupt = 1; /* enabled by default */

/*
 * Enable device removal notification.
 */
uint8_t rmv_interrupt = 1; /* enabled by default */

/*
 * Display or mask ether events
 * Default to all events except VF_MBOX
 */
uint32_t event_print_mask = (UINT32_C(1) << RTE_ETH_EVENT_UNKNOWN) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_LSC) |
			    (UINT32_C(1) << RTE_ETH_EVENT_QUEUE_STATE) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_RESET) |
			    (UINT32_C(1) << RTE_ETH_EVENT_MACSEC) |
			    (UINT32_C(1) << RTE_ETH_EVENT_INTR_RMV);

/*
 * NIC bypass mode configuration options.
 */

#if defined RTE_LIBRTE_IXGBE_PMD && defined RTE_LIBRTE_IXGBE_BYPASS
/* The NIC bypass watchdog timeout. */
uint32_t bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_OFF;
#endif


#ifdef RTE_LIBRTE_LATENCY_STATS

/*
 * Set when latency stats is enabled in the commandline
 */
uint8_t latencystats_enabled;

/*
 * Lcore ID to serive latency statistics.
 */
lcoreid_t latencystats_lcore_id = -1;

#endif

/*
 * Ethernet device configuration.
 */
struct rte_eth_rxmode rx_mode = {
	.max_rx_pkt_len = ETHER_MAX_LEN, /**< Default maximum frame length. */
	.split_hdr_size = 0,
	.header_split   = 0, /**< Header Split disabled. */
	.hw_ip_checksum = 0, /**< IP checksum offload disabled. */
	.hw_vlan_filter = 1, /**< VLAN filtering enabled. */
	.hw_vlan_strip  = 1, /**< VLAN strip enabled. */
	.hw_vlan_extend = 0, /**< Extended VLAN disabled. */
	.jumbo_frame    = 0, /**< Jumbo Frame Support disabled. */
	.hw_strip_crc   = 1, /**< CRC stripping by hardware enabled. */
};

struct rte_fdir_conf fdir_conf = {
	.mode = RTE_FDIR_MODE_NONE,
	.pballoc = RTE_FDIR_PBALLOC_64K,
	.status = RTE_FDIR_REPORT_STATUS,
	.mask = {
		.vlan_tci_mask = 0x0,
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

struct queue_stats_mappings tx_queue_stats_mappings_array[MAX_TX_QUEUE_STATS_MAPPINGS];
struct queue_stats_mappings rx_queue_stats_mappings_array[MAX_RX_QUEUE_STATS_MAPPINGS];

struct queue_stats_mappings *tx_queue_stats_mappings = tx_queue_stats_mappings_array;
struct queue_stats_mappings *rx_queue_stats_mappings = rx_queue_stats_mappings_array;

uint16_t nb_tx_queue_stats_mappings = 0;
uint16_t nb_rx_queue_stats_mappings = 0;

unsigned int num_sockets = 0;
unsigned int socket_ids[RTE_MAX_NUMA_NODES];

#ifdef RTE_LIBRTE_BITRATE
/* Bitrate statistics */
struct rte_stats_bitrates *bitrate_data;
lcoreid_t bitrate_lcore_id;
uint8_t bitrate_enabled;
#endif

struct gro_status gro_ports[RTE_MAX_ETHPORTS];

/* Forward function declarations */
static void map_port_queue_stats_mapping_registers(uint8_t pi, struct rte_port *port);
static void check_all_ports_link_status(uint32_t port_mask);
static int eth_event_callback(uint8_t port_id,
			      enum rte_eth_event_type type,
			      void *param, void *ret_param);

/*
 * Check if all the ports are started.
 * If yes, return positive value. If not, return zero.
 */
static int all_ports_started(void);

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
		sock_num = rte_lcore_to_socket_id(i);
		if (new_socket_id(sock_num)) {
			if (num_sockets >= RTE_MAX_NUMA_NODES) {
				rte_exit(EXIT_FAILURE,
					 "Total sockets greater than %u\n",
					 RTE_MAX_NUMA_NODES);
			}
			socket_ids[num_sockets++] = sock_num;
		}
		if (!rte_lcore_is_enabled(i))
			continue;
		if (i == rte_get_master_lcore())
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
		peer_eth_addrs[i].addr_bytes[0] = ETHER_LOCAL_ADMIN_ADDR;
		peer_eth_addrs[i].addr_bytes[5] = i;
	}
}

static void
set_default_fwd_ports_config(void)
{
	portid_t pt_id;

	for (pt_id = 0; pt_id < nb_ports; pt_id++)
		fwd_ports_ids[pt_id] = pt_id;

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

/*
 * Configuration initialisation done once at init time.
 */
static void
mbuf_pool_create(uint16_t mbuf_seg_size, unsigned nb_mbuf,
		 unsigned int socket_id)
{
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *rte_mp = NULL;
	uint32_t mb_size;

	mb_size = sizeof(struct rte_mbuf) + mbuf_seg_size;
	mbuf_poolname_build(socket_id, pool_name, sizeof(pool_name));

	RTE_LOG(INFO, USER1,
		"create a new mbuf pool <%s>: n=%u, size=%u, socket=%u\n",
		pool_name, nb_mbuf, mbuf_seg_size, socket_id);

#ifdef RTE_LIBRTE_PMD_XENVIRT
	rte_mp = rte_mempool_gntalloc_create(pool_name, nb_mbuf, mb_size,
		(unsigned) mb_mempool_cache,
		sizeof(struct rte_pktmbuf_pool_private),
		rte_pktmbuf_pool_init, NULL,
		rte_pktmbuf_init, NULL,
		socket_id, 0);
#endif

	/* if the former XEN allocation failed fall back to normal allocation */
	if (rte_mp == NULL) {
		if (mp_anon != 0) {
			rte_mp = rte_mempool_create_empty(pool_name, nb_mbuf,
				mb_size, (unsigned) mb_mempool_cache,
				sizeof(struct rte_pktmbuf_pool_private),
				socket_id, 0);
			if (rte_mp == NULL)
				goto err;

			if (rte_mempool_populate_anon(rte_mp) == 0) {
				rte_mempool_free(rte_mp);
				rte_mp = NULL;
				goto err;
			}
			rte_pktmbuf_pool_init(rte_mp, NULL);
			rte_mempool_obj_iter(rte_mp, rte_pktmbuf_init, NULL);
		} else {
			/* wrapper to rte_mempool_create() */
			rte_mp = rte_pktmbuf_pool_create(pool_name, nb_mbuf,
				mb_mempool_cache, 0, mbuf_seg_size, socket_id);
		}
	}

err:
	if (rte_mp == NULL) {
		rte_exit(EXIT_FAILURE,
			"Creation of mbuf pool for socket %u failed: %s\n",
			socket_id, rte_strerror(rte_errno));
	} else if (verbose_level > 0) {
		rte_mempool_dump(stdout, rte_mp);
	}
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
			printf("Warning: NUMA should be configured manually by"
			       " using --port-numa-config and"
			       " --ring-numa-config parameters along with"
			       " --numa.\n");
		warning_once = 1;
		return -1;
	}
	return 0;
}

static void
init_config(void)
{
	portid_t pid;
	struct rte_port *port;
	struct rte_mempool *mbp;
	unsigned int nb_mbuf_per_pool;
	lcoreid_t  lc_id;
	uint8_t port_per_socket[RTE_MAX_NUMA_NODES];

	memset(port_per_socket,0,RTE_MAX_NUMA_NODES);

	if (numa_support) {
		memset(port_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
		memset(rxring_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
		memset(txring_numa, NUMA_NO_CONFIG, RTE_MAX_ETHPORTS);
	}

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
		port = &ports[pid];
		rte_eth_dev_info_get(pid, &port->dev_info);

		if (numa_support) {
			if (port_numa[pid] != NUMA_NO_CONFIG)
				port_per_socket[port_numa[pid]]++;
			else {
				uint32_t socket_id = rte_eth_dev_socket_id(pid);

				/* if socket_id is invalid, set to 0 */
				if (check_socket_id(socket_id) < 0)
					socket_id = 0;
				port_per_socket[socket_id]++;
			}
		}

		/* set flag to initialize port/queue */
		port->need_reconfig = 1;
		port->need_reconfig_queues = 1;
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
		uint8_t i;

		for (i = 0; i < num_sockets; i++)
			mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool,
					 socket_ids[i]);
	} else {
		if (socket_num == UMA_NO_CONFIG)
			mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool, 0);
		else
			mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool,
						 socket_num);
	}

	init_port_config();

	/*
	 * Records which Mbuf pool to use by each logical core, if needed.
	 */
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		mbp = mbuf_pool_find(
			rte_lcore_to_socket_id(fwd_lcores_cpuids[lc_id]));

		if (mbp == NULL)
			mbp = mbuf_pool_find(0);
		fwd_lcores[lc_id]->mbp = mbp;
	}

	/* Configuration of packet forwarding streams. */
	if (init_fwd_streams() < 0)
		rte_exit(EXIT_FAILURE, "FAIL from init_fwd_streams()\n");

	fwd_config_setup();
}


void
reconfig(portid_t new_port_id, unsigned socket_id)
{
	struct rte_port *port;

	/* Reconfiguration of Ethernet ports. */
	port = &ports[new_port_id];
	rte_eth_dev_info_get(new_port_id, &port->dev_info);

	/* set flag to initialize port/queue */
	port->need_reconfig = 1;
	port->need_reconfig_queues = 1;
	port->socket_id = socket_id;

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
			printf("Fail: nb_rxq(%d) is greater than "
				"max_rx_queues(%d)\n", nb_rxq,
				port->dev_info.max_rx_queues);
			return -1;
		}
		if (nb_txq > port->dev_info.max_tx_queues) {
			printf("Fail: nb_txq(%d) is greater than "
				"max_tx_queues(%d)\n", nb_txq,
				port->dev_info.max_tx_queues);
			return -1;
		}
		if (numa_support) {
			if (port_numa[pid] != NUMA_NO_CONFIG)
				port->socket_id = port_numa[pid];
			else {
				port->socket_id = rte_eth_dev_socket_id(pid);

				/* if socket_id is invalid, set to 0 */
				if (check_socket_id(port->socket_id) < 0)
					port->socket_id = 0;
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
		printf("Fail: Cannot allocate fwd streams as number of queues is 0\n");
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
	fwd_streams = rte_zmalloc("testpmd: fwd_streams",
		sizeof(struct fwd_stream *) * nb_fwd_streams, RTE_CACHE_LINE_SIZE);
	if (fwd_streams == NULL)
		rte_exit(EXIT_FAILURE, "rte_zmalloc(%d (struct fwd_stream *)) "
						"failed\n", nb_fwd_streams);

	for (sm_id = 0; sm_id < nb_fwd_streams; sm_id++) {
		fwd_streams[sm_id] = rte_zmalloc("testpmd: struct fwd_stream",
				sizeof(struct fwd_stream), RTE_CACHE_LINE_SIZE);
		if (fwd_streams[sm_id] == NULL)
			rte_exit(EXIT_FAILURE, "rte_zmalloc(struct fwd_stream)"
								" failed\n");
	}

	return 0;
}

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
static void
pkt_burst_stats_display(const char *rx_tx, struct pkt_burst_stats *pbs)
{
	unsigned int total_burst;
	unsigned int nb_burst;
	unsigned int burst_stats[3];
	uint16_t pktnb_stats[3];
	uint16_t nb_pkt;
	int burst_percent[3];

	/*
	 * First compute the total number of packet bursts and the
	 * two highest numbers of bursts of the same number of packets.
	 */
	total_burst = 0;
	burst_stats[0] = burst_stats[1] = burst_stats[2] = 0;
	pktnb_stats[0] = pktnb_stats[1] = pktnb_stats[2] = 0;
	for (nb_pkt = 0; nb_pkt < MAX_PKT_BURST; nb_pkt++) {
		nb_burst = pbs->pkt_burst_spread[nb_pkt];
		if (nb_burst == 0)
			continue;
		total_burst += nb_burst;
		if (nb_burst > burst_stats[0]) {
			burst_stats[1] = burst_stats[0];
			pktnb_stats[1] = pktnb_stats[0];
			burst_stats[0] = nb_burst;
			pktnb_stats[0] = nb_pkt;
		}
	}
	if (total_burst == 0)
		return;
	burst_percent[0] = (burst_stats[0] * 100) / total_burst;
	printf("  %s-bursts : %u [%d%% of %d pkts", rx_tx, total_burst,
	       burst_percent[0], (int) pktnb_stats[0]);
	if (burst_stats[0] == total_burst) {
		printf("]\n");
		return;
	}
	if (burst_stats[0] + burst_stats[1] == total_burst) {
		printf(" + %d%% of %d pkts]\n",
		       100 - burst_percent[0], pktnb_stats[1]);
		return;
	}
	burst_percent[1] = (burst_stats[1] * 100) / total_burst;
	burst_percent[2] = 100 - (burst_percent[0] + burst_percent[1]);
	if ((burst_percent[1] == 0) || (burst_percent[2] == 0)) {
		printf(" + %d%% of others]\n", 100 - burst_percent[0]);
		return;
	}
	printf(" + %d%% of %d pkts + %d%% of others]\n",
	       burst_percent[1], (int) pktnb_stats[1], burst_percent[2]);
}
#endif /* RTE_TEST_PMD_RECORD_BURST_STATS */

static void
fwd_port_stats_display(portid_t port_id, struct rte_eth_stats *stats)
{
	struct rte_port *port;
	uint8_t i;

	static const char *fwd_stats_border = "----------------------";

	port = &ports[port_id];
	printf("\n  %s Forward statistics for port %-2d %s\n",
	       fwd_stats_border, port_id, fwd_stats_border);

	if ((!port->rx_queue_stats_mapping_enabled) && (!port->tx_queue_stats_mapping_enabled)) {
		printf("  RX-packets: %-14"PRIu64" RX-dropped: %-14"PRIu64"RX-total: "
		       "%-"PRIu64"\n",
		       stats->ipackets, stats->imissed,
		       (uint64_t) (stats->ipackets + stats->imissed));

		if (cur_fwd_eng == &csum_fwd_engine)
			printf("  Bad-ipcsum: %-14"PRIu64" Bad-l4csum: %-14"PRIu64" \n",
			       port->rx_bad_ip_csum, port->rx_bad_l4_csum);
		if ((stats->ierrors + stats->rx_nombuf) > 0) {
			printf("  RX-error: %-"PRIu64"\n",  stats->ierrors);
			printf("  RX-nombufs: %-14"PRIu64"\n", stats->rx_nombuf);
		}

		printf("  TX-packets: %-14"PRIu64" TX-dropped: %-14"PRIu64"TX-total: "
		       "%-"PRIu64"\n",
		       stats->opackets, port->tx_dropped,
		       (uint64_t) (stats->opackets + port->tx_dropped));
	}
	else {
		printf("  RX-packets:             %14"PRIu64"    RX-dropped:%14"PRIu64"    RX-total:"
		       "%14"PRIu64"\n",
		       stats->ipackets, stats->imissed,
		       (uint64_t) (stats->ipackets + stats->imissed));

		if (cur_fwd_eng == &csum_fwd_engine)
			printf("  Bad-ipcsum:%14"PRIu64"    Bad-l4csum:%14"PRIu64"\n",
			       port->rx_bad_ip_csum, port->rx_bad_l4_csum);
		if ((stats->ierrors + stats->rx_nombuf) > 0) {
			printf("  RX-error:%"PRIu64"\n", stats->ierrors);
			printf("  RX-nombufs:             %14"PRIu64"\n",
			       stats->rx_nombuf);
		}

		printf("  TX-packets:             %14"PRIu64"    TX-dropped:%14"PRIu64"    TX-total:"
		       "%14"PRIu64"\n",
		       stats->opackets, port->tx_dropped,
		       (uint64_t) (stats->opackets + port->tx_dropped));
	}

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	if (port->rx_stream)
		pkt_burst_stats_display("RX",
			&port->rx_stream->rx_burst_stats);
	if (port->tx_stream)
		pkt_burst_stats_display("TX",
			&port->tx_stream->tx_burst_stats);
#endif

	if (port->rx_queue_stats_mapping_enabled) {
		printf("\n");
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d RX-packets:%14"PRIu64
			       "     RX-errors:%14"PRIu64
			       "    RX-bytes:%14"PRIu64"\n",
			       i, stats->q_ipackets[i], stats->q_errors[i], stats->q_ibytes[i]);
		}
		printf("\n");
	}
	if (port->tx_queue_stats_mapping_enabled) {
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d TX-packets:%14"PRIu64
			       "                                 TX-bytes:%14"PRIu64"\n",
			       i, stats->q_opackets[i], stats->q_obytes[i]);
		}
	}

	printf("  %s--------------------------------%s\n",
	       fwd_stats_border, fwd_stats_border);
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
	printf("  RX-packets: %-14u TX-packets: %-14u TX-dropped: %-14u",
	       fs->rx_packets, fs->tx_packets, fs->fwd_dropped);

	/* if checksum mode */
	if (cur_fwd_eng == &csum_fwd_engine) {
	       printf("  RX- bad IP checksum: %-14u  Rx- bad L4 checksum: "
			"%-14u\n", fs->rx_bad_ip_csum, fs->rx_bad_l4_csum);
	}

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	pkt_burst_stats_display("RX", &fs->rx_burst_stats);
	pkt_burst_stats_display("TX", &fs->tx_burst_stats);
#endif
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

	/* convert to number of cycles */
	timer_period = rte_get_timer_hz(); /* 1 second timeout */

	for (j = 0; j < 2; j++) {
		for (rxp = 0; rxp < cur_fwd_config.nb_fwd_ports; rxp++) {
			for (rxq = 0; rxq < nb_rxq; rxq++) {
				port_id = fwd_ports_ids[rxp];
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
#ifdef RTE_LIBRTE_BITRATE
	uint64_t tics_per_1sec;
	uint64_t tics_datum;
	uint64_t tics_current;
	uint8_t idx_port, cnt_ports;

	cnt_ports = rte_eth_dev_count();
	tics_datum = rte_rdtsc();
	tics_per_1sec = rte_get_timer_hz();
#endif
	fsm = &fwd_streams[fc->stream_idx];
	nb_fs = fc->stream_nb;
	do {
		for (sm_id = 0; sm_id < nb_fs; sm_id++)
			(*pkt_fwd)(fsm[sm_id]);
#ifdef RTE_LIBRTE_BITRATE
		if (bitrate_enabled != 0 &&
				bitrate_lcore_id == rte_lcore_id()) {
			tics_current = rte_rdtsc();
			if (tics_current - tics_datum >= tics_per_1sec) {
				/* Periodic bitrate calculation */
				for (idx_port = 0;
						idx_port < cnt_ports;
						idx_port++)
					rte_stats_bitrate_calc(bitrate_data,
						idx_port);
				tics_datum = tics_current;
			}
		}
#endif
#ifdef RTE_LIBRTE_LATENCY_STATS
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
	port_fwd_begin_t port_fwd_begin;
	unsigned int i;
	unsigned int lc_id;
	int diag;

	port_fwd_begin = cur_fwd_config.fwd_eng->port_fwd_begin;
	if (port_fwd_begin != NULL) {
		for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++)
			(*port_fwd_begin)(fwd_ports_ids[i]);
	}
	for (i = 0; i < cur_fwd_config.nb_fwd_lcores; i++) {
		lc_id = fwd_lcores_cpuids[i];
		if ((interactive == 0) || (lc_id != rte_lcore_id())) {
			fwd_lcores[i]->stopped = 0;
			diag = rte_eal_remote_launch(pkt_fwd_on_lcore,
						     fwd_lcores[i], lc_id);
			if (diag != 0)
				printf("launch lcore %u failed - diag=%d\n",
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
	struct rte_port *port;
	unsigned int i;
	portid_t   pt_id;
	streamid_t sm_id;

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
		printf("Not all ports were started\n");
		return;
	}
	if (test_done == 0) {
		printf("Packet forwarding already started\n");
		return;
	}

	if (init_fwd_streams() < 0) {
		printf("Fail from init_fwd_streams()\n");
		return;
	}

	if(dcb_test) {
		for (i = 0; i < nb_fwd_ports; i++) {
			pt_id = fwd_ports_ids[i];
			port = &ports[pt_id];
			if (!port->dcb_flag) {
				printf("In DCB mode, all forwarding ports must "
                                       "be configured in this mode.\n");
				return;
			}
		}
		if (nb_fwd_lcores == 1) {
			printf("In DCB mode,the nb forwarding cores "
                               "should be larger than 1.\n");
			return;
		}
	}
	test_done = 0;

	if(!no_flush_rx)
		flush_fwd_rx_queues();

	fwd_config_setup();
	pkt_fwd_config_display(&cur_fwd_config);
	rxtx_config_display();

	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		pt_id = fwd_ports_ids[i];
		port = &ports[pt_id];
		rte_eth_stats_get(pt_id, &port->stats);
		port->tx_dropped = 0;

		map_port_queue_stats_mapping_registers(pt_id, port);
	}
	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		fwd_streams[sm_id]->rx_packets = 0;
		fwd_streams[sm_id]->tx_packets = 0;
		fwd_streams[sm_id]->fwd_dropped = 0;
		fwd_streams[sm_id]->rx_bad_ip_csum = 0;
		fwd_streams[sm_id]->rx_bad_l4_csum = 0;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
		memset(&fwd_streams[sm_id]->rx_burst_stats, 0,
		       sizeof(fwd_streams[sm_id]->rx_burst_stats));
		memset(&fwd_streams[sm_id]->tx_burst_stats, 0,
		       sizeof(fwd_streams[sm_id]->tx_burst_stats));
#endif
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
		fwd_streams[sm_id]->core_cycles = 0;
#endif
	}
	if (with_tx_first) {
		port_fwd_begin = tx_only_engine.port_fwd_begin;
		if (port_fwd_begin != NULL) {
			for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++)
				(*port_fwd_begin)(fwd_ports_ids[i]);
		}
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
	struct rte_eth_stats stats;
	struct rte_port *port;
	port_fwd_end_t  port_fwd_end;
	int i;
	portid_t   pt_id;
	streamid_t sm_id;
	lcoreid_t  lc_id;
	uint64_t total_recv;
	uint64_t total_xmit;
	uint64_t total_rx_dropped;
	uint64_t total_tx_dropped;
	uint64_t total_rx_nombuf;
	uint64_t tx_dropped;
	uint64_t rx_bad_ip_csum;
	uint64_t rx_bad_l4_csum;
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t fwd_cycles;
#endif
	static const char *acc_stats_border = "+++++++++++++++";

	if (test_done) {
		printf("Packet forwarding not started\n");
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
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	fwd_cycles = 0;
#endif
	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		if (cur_fwd_config.nb_fwd_streams >
		    cur_fwd_config.nb_fwd_ports) {
			fwd_stream_stats_display(sm_id);
			ports[fwd_streams[sm_id]->tx_port].tx_stream = NULL;
			ports[fwd_streams[sm_id]->rx_port].rx_stream = NULL;
		} else {
			ports[fwd_streams[sm_id]->tx_port].tx_stream =
				fwd_streams[sm_id];
			ports[fwd_streams[sm_id]->rx_port].rx_stream =
				fwd_streams[sm_id];
		}
		tx_dropped = ports[fwd_streams[sm_id]->tx_port].tx_dropped;
		tx_dropped = (uint64_t) (tx_dropped +
					 fwd_streams[sm_id]->fwd_dropped);
		ports[fwd_streams[sm_id]->tx_port].tx_dropped = tx_dropped;

		rx_bad_ip_csum =
			ports[fwd_streams[sm_id]->rx_port].rx_bad_ip_csum;
		rx_bad_ip_csum = (uint64_t) (rx_bad_ip_csum +
					 fwd_streams[sm_id]->rx_bad_ip_csum);
		ports[fwd_streams[sm_id]->rx_port].rx_bad_ip_csum =
							rx_bad_ip_csum;

		rx_bad_l4_csum =
			ports[fwd_streams[sm_id]->rx_port].rx_bad_l4_csum;
		rx_bad_l4_csum = (uint64_t) (rx_bad_l4_csum +
					 fwd_streams[sm_id]->rx_bad_l4_csum);
		ports[fwd_streams[sm_id]->rx_port].rx_bad_l4_csum =
							rx_bad_l4_csum;

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
		fwd_cycles = (uint64_t) (fwd_cycles +
					 fwd_streams[sm_id]->core_cycles);
#endif
	}
	total_recv = 0;
	total_xmit = 0;
	total_rx_dropped = 0;
	total_tx_dropped = 0;
	total_rx_nombuf  = 0;
	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		pt_id = fwd_ports_ids[i];

		port = &ports[pt_id];
		rte_eth_stats_get(pt_id, &stats);
		stats.ipackets -= port->stats.ipackets;
		port->stats.ipackets = 0;
		stats.opackets -= port->stats.opackets;
		port->stats.opackets = 0;
		stats.ibytes   -= port->stats.ibytes;
		port->stats.ibytes = 0;
		stats.obytes   -= port->stats.obytes;
		port->stats.obytes = 0;
		stats.imissed  -= port->stats.imissed;
		port->stats.imissed = 0;
		stats.oerrors  -= port->stats.oerrors;
		port->stats.oerrors = 0;
		stats.rx_nombuf -= port->stats.rx_nombuf;
		port->stats.rx_nombuf = 0;

		total_recv += stats.ipackets;
		total_xmit += stats.opackets;
		total_rx_dropped += stats.imissed;
		total_tx_dropped += port->tx_dropped;
		total_rx_nombuf  += stats.rx_nombuf;

		fwd_port_stats_display(pt_id, &stats);
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
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	if (total_recv > 0)
		printf("\n  CPU cycles/packet=%u (total cycles="
		       "%"PRIu64" / total RX packets=%"PRIu64")\n",
		       (unsigned int)(fwd_cycles / total_recv),
		       fwd_cycles, total_recv);
#endif
	printf("\nDone.\n");
	test_done = 1;
}

void
dev_set_link_up(portid_t pid)
{
	if (rte_eth_dev_set_link_up((uint8_t)pid) < 0)
		printf("\nSet link up fail.\n");
}

void
dev_set_link_down(portid_t pid)
{
	if (rte_eth_dev_set_link_down((uint8_t)pid) < 0)
		printf("\nSet link down fail.\n");
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
all_ports_stopped(void)
{
	portid_t pi;
	struct rte_port *port;

	RTE_ETH_FOREACH_DEV(pi) {
		port = &ports[pi];
		if ((port->port_status != RTE_PORT_STOPPED) &&
			(port->slave_flag == 0))
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

static int
port_is_closed(portid_t port_id)
{
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 0;

	if (ports[port_id].port_status != RTE_PORT_CLOSED)
		return 0;

	return 1;
}

int
start_port(portid_t pid)
{
	int diag, need_check_link_status = -1;
	portid_t pi;
	queueid_t qi;
	struct rte_port *port;
	struct ether_addr mac_addr;
	enum rte_eth_event_type event_type;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return 0;

	if(dcb_config)
		dcb_test = 1;
	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		need_check_link_status = 0;
		port = &ports[pi];
		if (rte_atomic16_cmpset(&(port->port_status), RTE_PORT_STOPPED,
						 RTE_PORT_HANDLING) == 0) {
			printf("Port %d is now not stopped\n", pi);
			continue;
		}

		if (port->need_reconfig > 0) {
			port->need_reconfig = 0;

			if (flow_isolate_all) {
				int ret = port_flow_isolate(pi, 1);
				if (ret) {
					printf("Failed to apply isolated"
					       " mode on port %d\n", pi);
					return -1;
				}
			}

			printf("Configuring Port %d (socket %u)\n", pi,
					port->socket_id);
			/* configure port */
			diag = rte_eth_dev_configure(pi, nb_rxq, nb_txq,
						&(port->dev_conf));
			if (diag != 0) {
				if (rte_atomic16_cmpset(&(port->port_status),
				RTE_PORT_HANDLING, RTE_PORT_STOPPED) == 0)
					printf("Port %d can not be set back "
							"to stopped\n", pi);
				printf("Fail to configure port %d\n", pi);
				/* try to reconfigure port next time */
				port->need_reconfig = 1;
				return -1;
			}
		}
		if (port->need_reconfig_queues > 0) {
			port->need_reconfig_queues = 0;
			/* setup tx queues */
			for (qi = 0; qi < nb_txq; qi++) {
				if ((numa_support) &&
					(txring_numa[pi] != NUMA_NO_CONFIG))
					diag = rte_eth_tx_queue_setup(pi, qi,
						nb_txd,txring_numa[pi],
						&(port->tx_conf));
				else
					diag = rte_eth_tx_queue_setup(pi, qi,
						nb_txd,port->socket_id,
						&(port->tx_conf));

				if (diag == 0)
					continue;

				/* Fail to setup tx queue, return */
				if (rte_atomic16_cmpset(&(port->port_status),
							RTE_PORT_HANDLING,
							RTE_PORT_STOPPED) == 0)
					printf("Port %d can not be set back "
							"to stopped\n", pi);
				printf("Fail to configure port %d tx queues\n", pi);
				/* try to reconfigure queues next time */
				port->need_reconfig_queues = 1;
				return -1;
			}
			/* setup rx queues */
			for (qi = 0; qi < nb_rxq; qi++) {
				if ((numa_support) &&
					(rxring_numa[pi] != NUMA_NO_CONFIG)) {
					struct rte_mempool * mp =
						mbuf_pool_find(rxring_numa[pi]);
					if (mp == NULL) {
						printf("Failed to setup RX queue:"
							"No mempool allocation"
							" on the socket %d\n",
							rxring_numa[pi]);
						return -1;
					}

					diag = rte_eth_rx_queue_setup(pi, qi,
					     nb_rxd,rxring_numa[pi],
					     &(port->rx_conf),mp);
				} else {
					struct rte_mempool *mp =
						mbuf_pool_find(port->socket_id);
					if (mp == NULL) {
						printf("Failed to setup RX queue:"
							"No mempool allocation"
							" on the socket %d\n",
							port->socket_id);
						return -1;
					}
					diag = rte_eth_rx_queue_setup(pi, qi,
					     nb_rxd,port->socket_id,
					     &(port->rx_conf), mp);
				}
				if (diag == 0)
					continue;

				/* Fail to setup rx queue, return */
				if (rte_atomic16_cmpset(&(port->port_status),
							RTE_PORT_HANDLING,
							RTE_PORT_STOPPED) == 0)
					printf("Port %d can not be set back "
							"to stopped\n", pi);
				printf("Fail to configure port %d rx queues\n", pi);
				/* try to reconfigure queues next time */
				port->need_reconfig_queues = 1;
				return -1;
			}
		}

		for (event_type = RTE_ETH_EVENT_UNKNOWN;
		     event_type < RTE_ETH_EVENT_MAX;
		     event_type++) {
			diag = rte_eth_dev_callback_register(pi,
							event_type,
							eth_event_callback,
							NULL);
			if (diag) {
				printf("Failed to setup even callback for event %d\n",
					event_type);
				return -1;
			}
		}

		/* start port */
		if (rte_eth_dev_start(pi) < 0) {
			printf("Fail to start port %d\n", pi);

			/* Fail to setup rx queue, return */
			if (rte_atomic16_cmpset(&(port->port_status),
				RTE_PORT_HANDLING, RTE_PORT_STOPPED) == 0)
				printf("Port %d can not be set back to "
							"stopped\n", pi);
			continue;
		}

		if (rte_atomic16_cmpset(&(port->port_status),
			RTE_PORT_HANDLING, RTE_PORT_STARTED) == 0)
			printf("Port %d can not be set into started\n", pi);

		rte_eth_macaddr_get(pi, &mac_addr);
		printf("Port %d: %02X:%02X:%02X:%02X:%02X:%02X\n", pi,
				mac_addr.addr_bytes[0], mac_addr.addr_bytes[1],
				mac_addr.addr_bytes[2], mac_addr.addr_bytes[3],
				mac_addr.addr_bytes[4], mac_addr.addr_bytes[5]);

		/* at least one port started, need checking link status */
		need_check_link_status = 1;
	}

	if (need_check_link_status == 1 && !no_link_check)
		check_all_ports_link_status(RTE_PORT_ALL);
	else if (need_check_link_status == 0)
		printf("Please stop the ports first\n");

	printf("Done\n");
	return 0;
}

void
stop_port(portid_t pid)
{
	portid_t pi;
	struct rte_port *port;
	int need_check_link_status = 0;

	if (dcb_test) {
		dcb_test = 0;
		dcb_config = 0;
	}

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return;

	printf("Stopping ports...\n");

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_forwarding(pi) != 0 && test_done == 0) {
			printf("Please remove port %d from forwarding configuration.\n", pi);
			continue;
		}

		if (port_is_bonding_slave(pi)) {
			printf("Please remove port %d from bonded device.\n", pi);
			continue;
		}

		port = &ports[pi];
		if (rte_atomic16_cmpset(&(port->port_status), RTE_PORT_STARTED,
						RTE_PORT_HANDLING) == 0)
			continue;

		rte_eth_dev_stop(pi);

		if (rte_atomic16_cmpset(&(port->port_status),
			RTE_PORT_HANDLING, RTE_PORT_STOPPED) == 0)
			printf("Port %d can not be set into stopped\n", pi);
		need_check_link_status = 1;
	}
	if (need_check_link_status && !no_link_check)
		check_all_ports_link_status(RTE_PORT_ALL);

	printf("Done\n");
}

void
close_port(portid_t pid)
{
	portid_t pi;
	struct rte_port *port;

	if (port_id_is_invalid(pid, ENABLED_WARN))
		return;

	printf("Closing ports...\n");

	RTE_ETH_FOREACH_DEV(pi) {
		if (pid != pi && pid != (portid_t)RTE_PORT_ALL)
			continue;

		if (port_is_forwarding(pi) != 0 && test_done == 0) {
			printf("Please remove port %d from forwarding configuration.\n", pi);
			continue;
		}

		if (port_is_bonding_slave(pi)) {
			printf("Please remove port %d from bonded device.\n", pi);
			continue;
		}

		port = &ports[pi];
		if (rte_atomic16_cmpset(&(port->port_status),
			RTE_PORT_CLOSED, RTE_PORT_CLOSED) == 1) {
			printf("Port %d is already closed\n", pi);
			continue;
		}

		if (rte_atomic16_cmpset(&(port->port_status),
			RTE_PORT_STOPPED, RTE_PORT_HANDLING) == 0) {
			printf("Port %d is now not stopped\n", pi);
			continue;
		}

		if (port->flow_list)
			port_flow_flush(pi);
		rte_eth_dev_close(pi);

		if (rte_atomic16_cmpset(&(port->port_status),
			RTE_PORT_HANDLING, RTE_PORT_CLOSED) == 0)
			printf("Port %d cannot be set to closed\n", pi);
	}

	printf("Done\n");
}

void
attach_port(char *identifier)
{
	portid_t pi = 0;
	unsigned int socket_id;

	printf("Attaching a new port...\n");

	if (identifier == NULL) {
		printf("Invalid parameters are specified\n");
		return;
	}

	if (rte_eth_dev_attach(identifier, &pi))
		return;

	socket_id = (unsigned)rte_eth_dev_socket_id(pi);
	/* if socket_id is invalid, set to 0 */
	if (check_socket_id(socket_id) < 0)
		socket_id = 0;
	reconfig(pi, socket_id);
	rte_eth_promiscuous_enable(pi);

	nb_ports = rte_eth_dev_count();

	ports[pi].port_status = RTE_PORT_STOPPED;

	printf("Port %d is attached. Now total ports is %d\n", pi, nb_ports);
	printf("Done\n");
}

void
detach_port(uint8_t port_id)
{
	char name[RTE_ETH_NAME_MAX_LEN];

	printf("Detaching a port...\n");

	if (!port_is_closed(port_id)) {
		printf("Please close port first\n");
		return;
	}

	if (ports[port_id].flow_list)
		port_flow_flush(port_id);

	if (rte_eth_dev_detach(port_id, name)) {
		RTE_LOG(ERR, USER1, "Failed to detach port '%s'\n", name);
		return;
	}

	nb_ports = rte_eth_dev_count();

	printf("Port '%s' is detached. Now total ports is %d\n",
			name, nb_ports);
	printf("Done\n");
	return;
}

void
pmd_test_exit(void)
{
	portid_t pt_id;

	if (test_done == 0)
		stop_packet_forwarding();

	if (ports != NULL) {
		no_link_check = 1;
		RTE_ETH_FOREACH_DEV(pt_id) {
			printf("\nShutting down port %d...\n", pt_id);
			fflush(stdout);
			stop_port(pt_id);
			close_port(pt_id);
		}
	}
	printf("\nBye...\n");
}

typedef void (*cmd_func_t)(void);
struct pmd_test_command {
	const char *cmd_name;
	cmd_func_t cmd_func;
};

#define PMD_TEST_CMD_NB (sizeof(pmd_test_menu) / sizeof(pmd_test_menu[0]))

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("Checking link statuses...\n");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
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
rmv_event_callback(void *arg)
{
	struct rte_eth_dev *dev;
	uint8_t port_id = (intptr_t)arg;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	stop_port(port_id);
	close_port(port_id);
	printf("removing device %s\n", dev->device->name);
	if (rte_eal_dev_detach(dev->device))
		RTE_LOG(ERR, USER1, "Failed to detach device %s\n",
			dev->device->name);
}

/* This function is used by the interrupt thread */
static int
eth_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param,
		  void *ret_param)
{
	static const char * const event_desc[] = {
		[RTE_ETH_EVENT_UNKNOWN] = "Unknown",
		[RTE_ETH_EVENT_INTR_LSC] = "LSC",
		[RTE_ETH_EVENT_QUEUE_STATE] = "Queue state",
		[RTE_ETH_EVENT_INTR_RESET] = "Interrupt reset",
		[RTE_ETH_EVENT_VF_MBOX] = "VF Mbox",
		[RTE_ETH_EVENT_MACSEC] = "MACsec",
		[RTE_ETH_EVENT_INTR_RMV] = "device removal",
		[RTE_ETH_EVENT_MAX] = NULL,
	};

	RTE_SET_USED(param);
	RTE_SET_USED(ret_param);

	if (type >= RTE_ETH_EVENT_MAX) {
		fprintf(stderr, "\nPort %" PRIu8 ": %s called upon invalid event %d\n",
			port_id, __func__, type);
		fflush(stderr);
	} else if (event_print_mask & (UINT32_C(1) << type)) {
		printf("\nPort %" PRIu8 ": %s event\n", port_id,
			event_desc[type]);
		fflush(stdout);
	}

	switch (type) {
	case RTE_ETH_EVENT_INTR_RMV:
		if (rte_eal_alarm_set(100000,
				rmv_event_callback, (void *)(intptr_t)port_id))
			fprintf(stderr, "Could not set up deferred device removal\n");
		break;
	default:
		break;
	}
	return 0;
}

static int
set_tx_queue_stats_mapping_registers(uint8_t port_id, struct rte_port *port)
{
	uint16_t i;
	int diag;
	uint8_t mapping_found = 0;

	for (i = 0; i < nb_tx_queue_stats_mappings; i++) {
		if ((tx_queue_stats_mappings[i].port_id == port_id) &&
				(tx_queue_stats_mappings[i].queue_id < nb_txq )) {
			diag = rte_eth_dev_set_tx_queue_stats_mapping(port_id,
					tx_queue_stats_mappings[i].queue_id,
					tx_queue_stats_mappings[i].stats_counter_id);
			if (diag != 0)
				return diag;
			mapping_found = 1;
		}
	}
	if (mapping_found)
		port->tx_queue_stats_mapping_enabled = 1;
	return 0;
}

static int
set_rx_queue_stats_mapping_registers(uint8_t port_id, struct rte_port *port)
{
	uint16_t i;
	int diag;
	uint8_t mapping_found = 0;

	for (i = 0; i < nb_rx_queue_stats_mappings; i++) {
		if ((rx_queue_stats_mappings[i].port_id == port_id) &&
				(rx_queue_stats_mappings[i].queue_id < nb_rxq )) {
			diag = rte_eth_dev_set_rx_queue_stats_mapping(port_id,
					rx_queue_stats_mappings[i].queue_id,
					rx_queue_stats_mappings[i].stats_counter_id);
			if (diag != 0)
				return diag;
			mapping_found = 1;
		}
	}
	if (mapping_found)
		port->rx_queue_stats_mapping_enabled = 1;
	return 0;
}

static void
map_port_queue_stats_mapping_registers(uint8_t pi, struct rte_port *port)
{
	int diag = 0;

	diag = set_tx_queue_stats_mapping_registers(pi, port);
	if (diag != 0) {
		if (diag == -ENOTSUP) {
			port->tx_queue_stats_mapping_enabled = 0;
			printf("TX queue stats mapping not supported port id=%d\n", pi);
		}
		else
			rte_exit(EXIT_FAILURE,
					"set_tx_queue_stats_mapping_registers "
					"failed for port id=%d diag=%d\n",
					pi, diag);
	}

	diag = set_rx_queue_stats_mapping_registers(pi, port);
	if (diag != 0) {
		if (diag == -ENOTSUP) {
			port->rx_queue_stats_mapping_enabled = 0;
			printf("RX queue stats mapping not supported port id=%d\n", pi);
		}
		else
			rte_exit(EXIT_FAILURE,
					"set_rx_queue_stats_mapping_registers "
					"failed for port id=%d diag=%d\n",
					pi, diag);
	}
}

static void
rxtx_port_config(struct rte_port *port)
{
	port->rx_conf = port->dev_info.default_rxconf;
	port->tx_conf = port->dev_info.default_txconf;

	/* Check if any RX/TX parameters have been passed */
	if (rx_pthresh != RTE_PMD_PARAM_UNSET)
		port->rx_conf.rx_thresh.pthresh = rx_pthresh;

	if (rx_hthresh != RTE_PMD_PARAM_UNSET)
		port->rx_conf.rx_thresh.hthresh = rx_hthresh;

	if (rx_wthresh != RTE_PMD_PARAM_UNSET)
		port->rx_conf.rx_thresh.wthresh = rx_wthresh;

	if (rx_free_thresh != RTE_PMD_PARAM_UNSET)
		port->rx_conf.rx_free_thresh = rx_free_thresh;

	if (rx_drop_en != RTE_PMD_PARAM_UNSET)
		port->rx_conf.rx_drop_en = rx_drop_en;

	if (tx_pthresh != RTE_PMD_PARAM_UNSET)
		port->tx_conf.tx_thresh.pthresh = tx_pthresh;

	if (tx_hthresh != RTE_PMD_PARAM_UNSET)
		port->tx_conf.tx_thresh.hthresh = tx_hthresh;

	if (tx_wthresh != RTE_PMD_PARAM_UNSET)
		port->tx_conf.tx_thresh.wthresh = tx_wthresh;

	if (tx_rs_thresh != RTE_PMD_PARAM_UNSET)
		port->tx_conf.tx_rs_thresh = tx_rs_thresh;

	if (tx_free_thresh != RTE_PMD_PARAM_UNSET)
		port->tx_conf.tx_free_thresh = tx_free_thresh;

	if (txq_flags != RTE_PMD_PARAM_UNSET)
		port->tx_conf.txq_flags = txq_flags;
}

void
init_port_config(void)
{
	portid_t pid;
	struct rte_port *port;

	RTE_ETH_FOREACH_DEV(pid) {
		port = &ports[pid];
		port->dev_conf.rxmode = rx_mode;
		port->dev_conf.fdir_conf = fdir_conf;
		if (nb_rxq > 1) {
			port->dev_conf.rx_adv_conf.rss_conf.rss_key = NULL;
			port->dev_conf.rx_adv_conf.rss_conf.rss_hf = rss_hf;
		} else {
			port->dev_conf.rx_adv_conf.rss_conf.rss_key = NULL;
			port->dev_conf.rx_adv_conf.rss_conf.rss_hf = 0;
		}

		if (port->dcb_flag == 0) {
			if( port->dev_conf.rx_adv_conf.rss_conf.rss_hf != 0)
				port->dev_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
			else
				port->dev_conf.rxmode.mq_mode = ETH_MQ_RX_NONE;
		}

		rxtx_port_config(port);

		rte_eth_macaddr_get(pid, &port->eth_addr);

		map_port_queue_stats_mapping_registers(pid, port);
#if defined RTE_LIBRTE_IXGBE_PMD && defined RTE_LIBRTE_IXGBE_BYPASS
		rte_pmd_ixgbe_bypass_init(pid);
#endif

		if (lsc_interrupt &&
		    (rte_eth_devices[pid].data->dev_flags &
		     RTE_ETH_DEV_INTR_LSC))
			port->dev_conf.intr_conf.lsc = 1;
		if (rmv_interrupt &&
		    (rte_eth_devices[pid].data->dev_flags &
		     RTE_ETH_DEV_INTR_RMV))
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

	port = &ports[slave_pid];
	return port->slave_flag;
}

const uint16_t vlan_tags[] = {
		0,  1,  2,  3,  4,  5,  6,  7,
		8,  9, 10, 11,  12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31
};

static  int
get_eth_dcb_conf(struct rte_eth_conf *eth_conf,
		 enum dcb_mode_enable dcb_mode,
		 enum rte_eth_nb_tcs num_tcs,
		 uint8_t pfc_en)
{
	uint8_t i;

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
			(num_tcs ==  ETH_4_TCS ? ETH_32_POOLS : ETH_16_POOLS);
		vmdq_tx_conf->nb_queue_pools =
			(num_tcs ==  ETH_4_TCS ? ETH_32_POOLS : ETH_16_POOLS);

		vmdq_rx_conf->nb_pool_maps = vmdq_rx_conf->nb_queue_pools;
		for (i = 0; i < vmdq_rx_conf->nb_pool_maps; i++) {
			vmdq_rx_conf->pool_map[i].vlan_id = vlan_tags[i];
			vmdq_rx_conf->pool_map[i].pools =
				1 << (i % vmdq_rx_conf->nb_queue_pools);
		}
		for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
			vmdq_rx_conf->dcb_tc[i] = i;
			vmdq_tx_conf->dcb_tc[i] = i;
		}

		/* set DCB mode of RX and TX of multiple queues */
		eth_conf->rxmode.mq_mode = ETH_MQ_RX_VMDQ_DCB;
		eth_conf->txmode.mq_mode = ETH_MQ_TX_VMDQ_DCB;
	} else {
		struct rte_eth_dcb_rx_conf *rx_conf =
				&eth_conf->rx_adv_conf.dcb_rx_conf;
		struct rte_eth_dcb_tx_conf *tx_conf =
				&eth_conf->tx_adv_conf.dcb_tx_conf;

		rx_conf->nb_tcs = num_tcs;
		tx_conf->nb_tcs = num_tcs;

		for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
			rx_conf->dcb_tc[i] = i % num_tcs;
			tx_conf->dcb_tc[i] = i % num_tcs;
		}
		eth_conf->rxmode.mq_mode = ETH_MQ_RX_DCB_RSS;
		eth_conf->rx_adv_conf.rss_conf.rss_hf = rss_hf;
		eth_conf->txmode.mq_mode = ETH_MQ_TX_DCB;
	}

	if (pfc_en)
		eth_conf->dcb_capability_en =
				ETH_DCB_PG_SUPPORT | ETH_DCB_PFC_SUPPORT;
	else
		eth_conf->dcb_capability_en = ETH_DCB_PG_SUPPORT;

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

	rte_port = &ports[pid];

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));
	/* Enter DCB configuration status */
	dcb_config = 1;

	/*set configuration of DCB in vt mode and DCB in non-vt mode*/
	retval = get_eth_dcb_conf(&port_conf, dcb_mode, num_tcs, pfc_en);
	if (retval < 0)
		return retval;
	port_conf.rxmode.hw_vlan_filter = 1;

	/**
	 * Write the configuration into the device.
	 * Set the numbers of RX & TX queues to 0, so
	 * the RX & TX queues will not be setup.
	 */
	(void)rte_eth_dev_configure(pid, 0, 0, &port_conf);

	rte_eth_dev_info_get(pid, &rte_port->dev_info);

	/* If dev_info.vmdq_pool_base is greater than 0,
	 * the queue id of vmdq pools is started after pf queues.
	 */
	if (dcb_mode == DCB_VT_ENABLED &&
	    rte_port->dev_info.vmdq_pool_base > 0) {
		printf("VMDQ_DCB multi-queue mode is nonsensical"
			" for port %d.", pid);
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

	rxtx_port_config(rte_port);
	/* VLAN filter */
	rte_port->dev_conf.rxmode.hw_vlan_filter = 1;
	for (i = 0; i < RTE_DIM(vlan_tags); i++)
		rx_vft_set(pid, vlan_tags[i], 1);

	rte_eth_macaddr_get(pid, &rte_port->eth_addr);
	map_port_queue_stats_mapping_registers(pid, rte_port);

	rte_port->dcb_flag = 1;

	return 0;
}

static void
init_port(void)
{
	/* Configuration of Ethernet ports. */
	ports = rte_zmalloc("testpmd: ports",
			    sizeof(struct rte_port) * RTE_MAX_ETHPORTS,
			    RTE_CACHE_LINE_SIZE);
	if (ports == NULL) {
		rte_exit(EXIT_FAILURE,
				"rte_zmalloc(%d struct rte_port) failed\n",
				RTE_MAX_ETHPORTS);
	}
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
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\nSignal %d received, preparing to exit...\n",
				signum);
#ifdef RTE_LIBRTE_PDUMP
		/* uninitialize packet capture framework */
		rte_pdump_uninit();
#endif
#ifdef RTE_LIBRTE_LATENCY_STATS
		rte_latencystats_uninit();
#endif
		force_quit();
		/* exit with the expected status */
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}
}

int
main(int argc, char** argv)
{
	int  diag;
	uint8_t port_id;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	diag = rte_eal_init(argc, argv);
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

#ifdef RTE_LIBRTE_PDUMP
	/* initialize packet capture framework */
	rte_pdump_init(NULL);
#endif

	nb_ports = (portid_t) rte_eth_dev_count();
	if (nb_ports == 0)
		RTE_LOG(WARNING, EAL, "No probed ethernet devices\n");

	/* allocate port structures, and init them */
	init_port();

	set_def_fwd_config();
	if (nb_lcores == 0)
		rte_panic("Empty set of forwarding logical cores - check the "
			  "core mask supplied in the command parameters\n");

	/* Bitrate/latency stats disabled by default */
#ifdef RTE_LIBRTE_BITRATE
	bitrate_enabled = 0;
#endif
#ifdef RTE_LIBRTE_LATENCY_STATS
	latencystats_enabled = 0;
#endif

	argc -= diag;
	argv += diag;
	if (argc > 1)
		launch_args_parse(argc, argv);

	if (tx_first && interactive)
		rte_exit(EXIT_FAILURE, "--tx-first cannot be used on "
				"interactive mode.\n");

	if (tx_first && lsc_interrupt) {
		printf("Warning: lsc_interrupt needs to be off when "
				" using tx_first. Disabling.\n");
		lsc_interrupt = 0;
	}

	if (!nb_rxq && !nb_txq)
		printf("Warning: Either rx or tx queues should be non-zero\n");

	if (nb_rxq > 1 && nb_rxq > nb_txq)
		printf("Warning: nb_rxq=%d enables RSS configuration, "
		       "but nb_txq=%d will prevent to fully test it.\n",
		       nb_rxq, nb_txq);

	init_config();
	if (start_port(RTE_PORT_ALL) != 0)
		rte_exit(EXIT_FAILURE, "Start ports failed\n");

	/* set all ports to promiscuous mode by default */
	RTE_ETH_FOREACH_DEV(port_id)
		rte_eth_promiscuous_enable(port_id);

	/* Init metrics library */
	rte_metrics_init(rte_socket_id());

#ifdef RTE_LIBRTE_LATENCY_STATS
	if (latencystats_enabled != 0) {
		int ret = rte_latencystats_init(1, NULL);
		if (ret)
			printf("Warning: latencystats init()"
				" returned error %d\n",	ret);
		printf("Latencystats running on lcore %d\n",
			latencystats_lcore_id);
	}
#endif

	/* Setup bitrate stats */
#ifdef RTE_LIBRTE_BITRATE
	if (bitrate_enabled != 0) {
		bitrate_data = rte_stats_bitrate_create();
		if (bitrate_data == NULL)
			rte_exit(EXIT_FAILURE,
				"Could not allocate bitrate data.\n");
		rte_stats_bitrate_reg(bitrate_data);
	}
#endif

#ifdef RTE_LIBRTE_CMDLINE
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

		printf("No commandline core given, start packet forwarding\n");
		start_packet_forwarding(tx_first);
		if (stats_period != 0) {
			uint64_t prev_time = 0, cur_time, diff_time = 0;
			uint64_t timer_period;

			/* Convert to number of cycles */
			timer_period = stats_period * rte_get_timer_hz();

			while (1) {
				cur_time = rte_get_timer_cycles();
				diff_time += cur_time - prev_time;

				if (diff_time >= timer_period) {
					print_stats();
					/* Reset the timer */
					diff_time = 0;
				}
				/* Sleep to avoid unnecessary checks */
				prev_time = cur_time;
				sleep(1);
			}
		}

		printf("Press enter to exit\n");
		rc = read(0, &c, 1);
		pmd_test_exit();
		if (rc < 0)
			return 1;
	}

	return 0;
}
