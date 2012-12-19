/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
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
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "testpmd.h"

uint16_t verbose_level = 0; /**< Silent by default. */

/* use master core for command line ? */
uint8_t interactive = 0;

/*
 * NUMA support configuration.
 * When set, the NUMA support attempts to dispatch the allocation of the
 * RX and TX memory rings, and of the DMA memory buffers (mbufs) for the
 * probed ports among the CPU sockets 0 and 1.
 * Otherwise, all memory is allocated from CPU socket 0.
 */
uint8_t numa_support = 0; /**< No numa support by default */

/*
 * Record the Ethernet address of peer target ports to which packets are
 * forwarded.
 * Must be instanciated with the ethernet addresses of peer traffic generator
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
	&rx_only_engine,
	&tx_only_engine,
	&csum_fwd_engine,
#ifdef RTE_LIBRTE_IEEE1588
	&ieee1588_fwd_engine,
#endif
	NULL,
};

struct fwd_config cur_fwd_config;
struct fwd_engine *cur_fwd_eng = &io_fwd_engine; /**< IO mode by default. */

uint16_t mbuf_data_size = DEFAULT_MBUF_DATA_SIZE; /**< Mbuf data space size. */

/*
 * Configuration of packet segments used by the "txonly" processing engine.
 */
uint16_t tx_pkt_length = TXONLY_DEF_PACKET_LEN; /**< TXONLY packet length. */
uint16_t tx_pkt_seg_lengths[RTE_MAX_SEGS_PER_PKT] = {
	TXONLY_DEF_PACKET_LEN,
};
uint8_t  tx_pkt_nb_segs = 1; /**< Number of segments in TXONLY packets */

uint16_t nb_pkt_per_burst = DEF_PKT_BURST; /**< Number of packets per burst. */
uint16_t mb_mempool_cache = DEF_PKT_BURST; /**< Size of mbuf mempool cache. */

/*
 * Ethernet Ports Configuration.
 */
int promiscuous_on = 1; /**< Ports set in promiscuous mode by default. */

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

/*
 * Configurable values of RX and TX ring threshold registers.
 */
#define RX_PTHRESH 8 /**< Default value of RX prefetch threshold register. */
#define RX_HTHRESH 8 /**< Default value of RX host threshold register. */
#define RX_WTHRESH 4 /**< Default value of RX write-back threshold register. */

#define TX_PTHRESH 36 /**< Default value of TX prefetch threshold register. */
#define TX_HTHRESH 0 /**< Default value of TX host threshold register. */
#define TX_WTHRESH 0 /**< Default value of TX write-back threshold register. */

struct rte_eth_thresh rx_thresh = {
	.pthresh = RX_PTHRESH,
	.hthresh = RX_HTHRESH,
	.wthresh = RX_WTHRESH,
};

struct rte_eth_thresh tx_thresh = {
	.pthresh = TX_PTHRESH,
	.hthresh = TX_HTHRESH,
	.wthresh = TX_WTHRESH,
};

/*
 * Configurable value of RX free threshold.
 */
uint16_t rx_free_thresh = 0; /* Immediately free RX descriptors by default. */

/*
 * Configurable value of TX free threshold.
 */
uint16_t tx_free_thresh = 0; /* Use default values. */

/*
 * Configurable value of TX RS bit threshold.
 */
uint16_t tx_rs_thresh = 0; /* Use default values. */

/*
 * Receive Side Scaling (RSS) configuration.
 */
uint16_t rss_hf = ETH_RSS_IPV4 | ETH_RSS_IPV6; /* RSS IP by default. */

/*
 * Port topology configuration
 */
uint16_t port_topology = PORT_TOPOLOGY_PAIRED; /* Ports are paired by default */

/*
 * Ethernet device configuration.
 */
struct rte_eth_rxmode rx_mode = {
	.max_rx_pkt_len = ETHER_MAX_LEN, /**< Default maximum frame length. */
	.split_hdr_size = 0,
	.header_split   = 0, /**< Header Split disabled. */
	.hw_ip_checksum = 0, /**< IP checksum offload disabled. */
	.hw_vlan_filter = 1, /**< VLAN filtering enabled. */
	.jumbo_frame    = 0, /**< Jumbo Frame Support disabled. */
	.hw_strip_crc   = 0, /**< CRC stripping by hardware disabled. */
};

struct rte_fdir_conf fdir_conf = {
	.mode = RTE_FDIR_MODE_NONE,
	.pballoc = RTE_FDIR_PBALLOC_64K,
	.status = RTE_FDIR_REPORT_STATUS,
	.flexbytes_offset = 0x6,
	.drop_queue = 127,
};

static volatile int test_done = 1; /* stop packet forwarding when set to 1. */

/*
 * Setup default configuration.
 */
static void
set_default_fwd_lcores_config(void)
{
	unsigned int i;
	unsigned int nb_lc;

	nb_lc = 0;
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (! rte_lcore_is_enabled(i))
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
struct mbuf_ctor_arg {
	uint16_t seg_buf_offset; /**< offset of data in data segment of mbuf. */
	uint16_t seg_buf_size;   /**< size of data segment in mbuf. */
};

struct mbuf_pool_ctor_arg {
	uint16_t seg_buf_size; /**< size of data segment in mbuf. */
};

static void
testpmd_mbuf_ctor(struct rte_mempool *mp,
		  void *opaque_arg,
		  void *raw_mbuf,
		  __attribute__((unused)) unsigned i)
{
	struct mbuf_ctor_arg *mb_ctor_arg;
	struct rte_mbuf    *mb;

	mb_ctor_arg = (struct mbuf_ctor_arg *) opaque_arg;
	mb = (struct rte_mbuf *) raw_mbuf;

	mb->pool         = mp;
	mb->buf_addr     = (void *) ((char *)mb + mb_ctor_arg->seg_buf_offset);
	mb->buf_physaddr = (uint64_t) (rte_mempool_virt2phy(mp, mb) +
			mb_ctor_arg->seg_buf_offset);
	mb->buf_len      = mb_ctor_arg->seg_buf_size;
	mb->type         = RTE_MBUF_PKT;
	mb->ol_flags     = 0;
	mb->pkt.data     = (char *) mb->buf_addr + RTE_PKTMBUF_HEADROOM;
	mb->pkt.nb_segs  = 1;
	mb->pkt.vlan_macip.data = 0;
	mb->pkt.hash.rss = 0;
}

static void
testpmd_mbuf_pool_ctor(struct rte_mempool *mp,
		       void *opaque_arg)
{
	struct mbuf_pool_ctor_arg      *mbp_ctor_arg;
	struct rte_pktmbuf_pool_private *mbp_priv;

	if (mp->private_data_size < sizeof(struct rte_pktmbuf_pool_private)) {
		printf("%s(%s) private_data_size %d < %d\n",
		       __func__, mp->name, (int) mp->private_data_size,
		       (int) sizeof(struct rte_pktmbuf_pool_private));
		return;
	}
	mbp_ctor_arg = (struct mbuf_pool_ctor_arg *) opaque_arg;
	mbp_priv = (struct rte_pktmbuf_pool_private *)
		((char *)mp + sizeof(struct rte_mempool));
	mbp_priv->mbuf_data_room_size = mbp_ctor_arg->seg_buf_size;
}

static void
mbuf_pool_create(uint16_t mbuf_seg_size, unsigned nb_mbuf,
		 unsigned int socket_id)
{
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *rte_mp;
	struct mbuf_pool_ctor_arg mbp_ctor_arg;
	struct mbuf_ctor_arg mb_ctor_arg;
	uint32_t mb_size;

	mbp_ctor_arg.seg_buf_size = (uint16_t) (RTE_PKTMBUF_HEADROOM +
						mbuf_seg_size);
	mb_ctor_arg.seg_buf_offset =
		(uint16_t) CACHE_LINE_ROUNDUP(sizeof(struct rte_mbuf));
	mb_ctor_arg.seg_buf_size = mbp_ctor_arg.seg_buf_size;
	mb_size = mb_ctor_arg.seg_buf_offset + mb_ctor_arg.seg_buf_size;
	mbuf_poolname_build(socket_id, pool_name, sizeof(pool_name));
	rte_mp = rte_mempool_create(pool_name, nb_mbuf, (unsigned) mb_size,
				    (unsigned) mb_mempool_cache,
				    sizeof(struct rte_pktmbuf_pool_private),
				    testpmd_mbuf_pool_ctor, &mbp_ctor_arg,
				    testpmd_mbuf_ctor, &mb_ctor_arg,
				    socket_id, 0);
	if (rte_mp == NULL) {
		rte_exit(EXIT_FAILURE, "Creation of mbuf pool for socket %u failed\n",
		       socket_id);
	}
}

static void
init_config(void)
{
	struct rte_port *port;
	struct rte_mempool *mbp;
	unsigned int nb_mbuf_per_pool;
	streamid_t sm_id;
	lcoreid_t  lc_id;
	portid_t   pt_id;

	/* Configuration of logical cores. */
	fwd_lcores = rte_zmalloc("testpmd: fwd_lcores",
				sizeof(struct fwd_lcore *) * nb_lcores,
				CACHE_LINE_SIZE);
	if (fwd_lcores == NULL) {
		rte_exit(EXIT_FAILURE, "rte_zmalloc(%d (struct fwd_lcore *)) failed\n",
		       nb_lcores);
	}
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		fwd_lcores[lc_id] = rte_zmalloc("testpmd: struct fwd_lcore",
					       sizeof(struct fwd_lcore),
					       CACHE_LINE_SIZE);
		if (fwd_lcores[lc_id] == NULL) {
			rte_exit(EXIT_FAILURE, "rte_zmalloc(struct fwd_lcore) failed\n");
		}
		fwd_lcores[lc_id]->cpuid_idx = lc_id;
	}

	/*
	 * Create pools of mbuf.
	 * If NUMA support is disabled, create a single pool of mbuf in
	 * socket 0 memory.
	 * Otherwise, create a pool of mbuf in the memory of sockets 0 and 1.
	 */
	nb_mbuf_per_pool = nb_rxd + (nb_lcores * mb_mempool_cache) +
		nb_txd + MAX_PKT_BURST;
	if (numa_support) {
		nb_mbuf_per_pool = nb_mbuf_per_pool * (nb_ports >> 1);
		mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool, 0);
		mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool, 1);
	} else {
		nb_mbuf_per_pool = (nb_mbuf_per_pool * nb_ports);
		mbuf_pool_create(mbuf_data_size, nb_mbuf_per_pool, 0);
	}

	/*
	 * Records which Mbuf pool to use by each logical core, if needed.
	 */
	for (lc_id = 0; lc_id < nb_lcores; lc_id++) {
		mbp = mbuf_pool_find(rte_lcore_to_socket_id(lc_id));
		if (mbp == NULL)
			mbp = mbuf_pool_find(0);
		fwd_lcores[lc_id]->mbp = mbp;
	}

	/* Configuration of Ethernet ports. */
	ports = rte_zmalloc("testpmd: ports",
			    sizeof(struct rte_port) * nb_ports,
			    CACHE_LINE_SIZE);
	if (ports == NULL) {
		rte_exit(EXIT_FAILURE, "rte_zmalloc(%d struct rte_port) failed\n",
		       nb_ports);
	}
	port = ports;
	for (pt_id = 0; pt_id < nb_ports; pt_id++, port++) {
		rte_eth_dev_info_get(pt_id, &port->dev_info);
		if (nb_rxq > port->dev_info.max_rx_queues) {
			rte_exit(EXIT_FAILURE, "Port %d: max RX queues %d < nb_rxq %d\n",
			       (int) pt_id,
			       (int) port->dev_info.max_rx_queues,
			       (int) nb_rxq);
		}
		if (nb_txq > port->dev_info.max_tx_queues) {
			rte_exit(EXIT_FAILURE, "Port %d: max TX queues %d < nb_txq %d\n",
			       (int) pt_id,
			       (int) port->dev_info.max_tx_queues,
			       (int) nb_txq);
		}

		if (numa_support)
			port->socket_id = (pt_id < (nb_ports >> 1)) ? 0 : 1;
		else
			port->socket_id = 0;
	}

	/* Configuration of packet forwarding streams. */
	nb_fwd_streams = (streamid_t) (nb_ports * nb_rxq);
	fwd_streams = rte_zmalloc("testpmd: fwd_streams",
				  sizeof(struct fwd_stream *) * nb_fwd_streams,
				  CACHE_LINE_SIZE);
	if (fwd_streams == NULL) {
		rte_exit(EXIT_FAILURE, "rte_zmalloc(%d (struct fwd_stream *)) failed\n",
		       nb_fwd_streams);
	}
	for (sm_id = 0; sm_id < nb_fwd_streams; sm_id++) {
		fwd_streams[sm_id] = rte_zmalloc("testpmd: struct fwd_stream",
						 sizeof(struct fwd_stream),
						 CACHE_LINE_SIZE);
		if (fwd_streams[sm_id] == NULL) {
			rte_exit(EXIT_FAILURE, "rte_zmalloc(struct fwd_stream) failed\n");
		}
	}
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

	static const char *fwd_stats_border = "----------------------";

	port = &ports[port_id];
	printf("\n  %s Forward statistics for port %-2d %s\n",
		fwd_stats_border, port_id, fwd_stats_border);
	printf("  RX-packets: %-14"PRIu64" RX-dropped: %-14"PRIu64"RX-total: "
	       "%-"PRIu64"\n",
	       stats->ipackets, stats->ierrors,
	       (uint64_t) (stats->ipackets + stats->ierrors));

	if (cur_fwd_eng == &csum_fwd_engine)
		printf("  Bad-ipcsum: %-14"PRIu64" Bad-l4csum: %-14"PRIu64" \n",
				port->rx_bad_ip_csum, port->rx_bad_l4_csum);

	printf("  TX-packets: %-14"PRIu64" TX-dropped: %-14"PRIu64"TX-total: "
	       "%-"PRIu64"\n",
	       stats->opackets, port->tx_dropped,
	       (uint64_t) (stats->opackets + port->tx_dropped));

	if (stats->rx_nombuf > 0)
		printf("  RX-nombufs: %-14"PRIu64"\n", stats->rx_nombuf);
#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	if (port->rx_stream)
		pkt_burst_stats_display("RX", &port->rx_stream->rx_burst_stats);
	if (port->tx_stream)
		pkt_burst_stats_display("TX", &port->tx_stream->tx_burst_stats);
#endif
	/* stats fdir */
	if (fdir_conf.mode != RTE_FDIR_MODE_NONE)
		printf("  Fdirmiss: %-14"PRIu64"   Fdirmatch: %-14"PRIu64"\n",
		       stats->fdirmiss,
		       stats->fdirmatch);

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
	       printf("  RX- bad IP checksum: %-14u  Rx- bad L4 checksum: %-14u\n",
	       fs->rx_bad_ip_csum, fs->rx_bad_l4_csum);
	}

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	pkt_burst_stats_display("RX", &fs->rx_burst_stats);
	pkt_burst_stats_display("TX", &fs->tx_burst_stats);
#endif
}

static void
flush_all_rx_queues(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	portid_t  rxp;
	queueid_t rxq;
	uint16_t  nb_rx;
	uint16_t  i;
	uint8_t   j;

	for (j = 0; j < 2; j++) {
		for (rxp = 0; rxp < nb_ports; rxp++) {
			for (rxq = 0; rxq < nb_rxq; rxq++) {
				do {
					nb_rx = rte_eth_rx_burst(rxp, rxq,
								 pkts_burst,
								 MAX_PKT_BURST);
					for (i = 0; i < nb_rx; i++)
						rte_pktmbuf_free(pkts_burst[i]);
				} while (nb_rx > 0);
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

	fsm = &fwd_streams[fc->stream_idx];
	nb_fs = fc->stream_nb;
	do {
		for (sm_id = 0; sm_id < nb_fs; sm_id++)
			(*pkt_fwd)(fsm[sm_id]);
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

	if (test_done == 0) {
		printf("Packet forwarding already started\n");
		return;
	}
	test_done = 0;
	flush_all_rx_queues();
	fwd_config_setup();
	rxtx_config_display();

	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		pt_id = fwd_ports_ids[i];
		port = &ports[pt_id];
		rte_eth_stats_get(pt_id, &port->stats);
		port->tx_dropped = 0;
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
		launch_packet_forwarding(run_one_txonly_burst_on_core);
		rte_eal_mp_wait_lcore();
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

		rx_bad_ip_csum = ports[fwd_streams[sm_id]->rx_port].rx_bad_ip_csum;
		rx_bad_ip_csum = (uint64_t) (rx_bad_ip_csum +
					 fwd_streams[sm_id]->rx_bad_ip_csum);
		ports[fwd_streams[sm_id]->rx_port].rx_bad_ip_csum = rx_bad_ip_csum;

		rx_bad_l4_csum = ports[fwd_streams[sm_id]->rx_port].rx_bad_l4_csum;
		rx_bad_l4_csum = (uint64_t) (rx_bad_l4_csum +
					 fwd_streams[sm_id]->rx_bad_l4_csum);
		ports[fwd_streams[sm_id]->rx_port].rx_bad_l4_csum = rx_bad_l4_csum;

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
	for (i = 0; i < ((cur_fwd_config.nb_fwd_ports + 1) & ~0x1); i++) {
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
		stats.ierrors  -= port->stats.ierrors;
		port->stats.ierrors = 0;
		stats.oerrors  -= port->stats.oerrors;
		port->stats.oerrors = 0;
		stats.rx_nombuf -= port->stats.rx_nombuf;
		port->stats.rx_nombuf = 0;
		stats.fdirmatch -= port->stats.fdirmatch;
		port->stats.rx_nombuf = 0;
		stats.fdirmiss -= port->stats.fdirmiss;
		port->stats.rx_nombuf = 0;

		total_recv += stats.ipackets;
		total_xmit += stats.opackets;
		total_rx_dropped += stats.ierrors;
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
pmd_test_exit(void)
{
	portid_t pt_id;

	for (pt_id = 0; pt_id < nb_ports; pt_id++) {
		printf("Stopping port %d...", pt_id);
		fflush(stdout);
		rte_eth_dev_close(pt_id);
		printf("done\n");
	}
	printf("bye...\n");
}

typedef void (*cmd_func_t)(void);
struct pmd_test_command {
	const char *cmd_name;
	cmd_func_t cmd_func;
};

#define PMD_TEST_CMD_NB (sizeof(pmd_test_menu) / sizeof(pmd_test_menu[0]))

static void
fatal_init_error(const char *func_name, uint8_t port_id, int diag)
{
	rte_panic("%s(port_id=%d) failed - diag=%d\n",
		  func_name, port_id, diag);
}

static void
init_ports(void)
{
	struct rte_eth_link   link;
	struct rte_eth_conf   port_conf = {
		.intr_conf = {
			.lsc = 0,
		},
	};
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_txconf tx_conf;
	struct rte_port *port;
	unsigned int sock_id;
	portid_t  pi;
	queueid_t qi;
	int diag;

	port_conf.rxmode = rx_mode;
	port_conf.fdir_conf = fdir_conf;

	if (nb_rxq > 0) { /* configure RSS */
		port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
		/* use default hash key */
		port_conf.rx_adv_conf.rss_conf.rss_hf = rss_hf;
	} else
		port_conf.rx_adv_conf.rss_conf.rss_hf = 0;
	rx_conf.rx_thresh = rx_thresh;
	rx_conf.rx_free_thresh = rx_free_thresh;
	tx_conf.tx_thresh = tx_thresh;
	tx_conf.tx_rs_thresh = tx_rs_thresh;
	tx_conf.tx_free_thresh = tx_free_thresh;

	for (pi = 0; pi < nb_ports; pi++) {
		port = &ports[pi];
		memcpy(&port->dev_conf, &port_conf, sizeof(port_conf));
		sock_id = port->socket_id;
		printf("Initializing port %d... ", pi);
		fflush(stdout);
		diag = rte_eth_dev_configure(pi, nb_rxq, nb_txq, &port_conf);
		if (diag != 0) {
			fatal_init_error("rte_eth_dev_configure", pi, diag);
			/* NOT REACHED */
		}
		rte_eth_macaddr_get(pi, &port->eth_addr);
		for (qi = 0; qi < nb_txq; qi++) {
			diag = rte_eth_tx_queue_setup(pi, qi, nb_txd,
						      sock_id,
						      &tx_conf);
			if (diag != 0) {
				fatal_init_error("rte_eth_tx_queue_setup",
						 pi, diag);
				/* NOT REACHED */
			}
		}
		for (qi = 0; qi < nb_rxq; qi++) {
			diag = rte_eth_rx_queue_setup(pi, qi, nb_rxd, sock_id,
						      &rx_conf,
						      mbuf_pool_find(sock_id));
			if (diag != 0) {
				fatal_init_error("rte_eth_rx_queue_setup",
						 pi , diag);
				/* NOT REACHED */
			}
		}

		/* Start device */
		diag = rte_eth_dev_start(pi);
		if (diag != 0) {
			fatal_init_error("rte_eth_dev_start", pi, diag);
			/* NOT REACHED */
		}
		printf("done: ");
		rte_eth_link_get(pi, &link);
		if (link.link_status) {
			printf(" Link Up - speed %u Mbps - %s\n",
			       (unsigned) link.link_speed,
			       (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
			       ("full-duplex") : ("half-duplex\n"));
		} else {
			printf(" Link Down\n");
		}

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows the PMD test in IO forwarding mode to forward
		 * packets to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(pi);
	}
}

#ifdef RTE_EXEC_ENV_BAREMETAL
#define main _main
#endif

int
main(int argc, char** argv)
{
	int  diag;

	diag = rte_eal_init(argc, argv);
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

	if (rte_pmd_init_all())
		rte_panic("Cannot init PMD\n");

	if (rte_eal_pci_probe())
		rte_panic("Cannot probe PCI\n");

	nb_ports = (portid_t) rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No probed ethernet devices - check that "
			  "CONFIG_RTE_LIBRTE_IGB_PMD=y and that "
			  "CONFIG_RTE_LIBRTE_EM_PMD=y and that "
			  "CONFIG_RTE_LIBRTE_IXGBE_PMD=y in your "
			  "configuration file\n");

	set_def_fwd_config();
	if (nb_lcores == 0)
		rte_panic("Empty set of forwarding logical cores - check the "
			  "core mask supplied in the command parameters\n");

	argc -= diag;
	argv += diag;
	if (argc > 1)
		launch_args_parse(argc, argv);

	if (nb_rxq > nb_txq)
		printf("Warning: nb_rxq=%d enables RSS configuration, "
		       "but nb_txq=%d will prevent to fully test it.\n",
		       nb_rxq, nb_txq);

	init_config();

	init_ports();

	if (interactive == 1)
		prompt();
	else {
		char c;
		int rc;

		printf("No commandline core given, start packet forwarding\n");
		start_packet_forwarding(0);
		printf("Press enter to exit\n");
		rc = read(0, &c, 1);
		if (rc < 0)
			return 1;
	}

	return 0;
}
