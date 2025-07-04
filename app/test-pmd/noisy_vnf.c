/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Red Hat Corp.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_memcpy.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>

#include "testpmd.h"
#include "5tswap.h"
#include "macfwd.h"
#if defined(RTE_ARCH_X86)
#include "macswap_sse.h"
#elif defined(__ARM_NEON)
#include "macswap_neon.h"
#else
#include "macswap.h"
#endif

#define NOISY_STRSIZE 256
#define NOISY_RING "noisy_ring_%d\n"

struct noisy_config {
	struct rte_ring *f;
	uint64_t prev_time;
	char *vnf_mem;
	bool do_buffering;
	bool do_flush;
	bool do_sim;
};

struct noisy_config *noisy_cfg[RTE_MAX_ETHPORTS];

static inline void
do_write(char *vnf_mem)
{
	uint64_t i = rte_rand();
	uint64_t w = rte_rand();

	vnf_mem[i % ((noisy_lkup_mem_sz * 1024 * 1024) /
			RTE_CACHE_LINE_SIZE)] = w;
}

static inline void
do_read(char *vnf_mem)
{
	uint64_t r __rte_unused;
	uint64_t i = rte_rand();

	r = vnf_mem[i % ((noisy_lkup_mem_sz * 1024 * 1024) /
			RTE_CACHE_LINE_SIZE)];
	r++;
}

static inline void
do_readwrite(char *vnf_mem)
{
	do_read(vnf_mem);
	do_write(vnf_mem);
}

/*
 * Simulate route lookups as defined by commandline parameters
 */
static void
sim_memory_lookups(struct noisy_config *ncf, uint16_t nb_pkts)
{
	uint16_t i, j;

	for (i = 0; i < nb_pkts; i++) {
		for (j = 0; j < noisy_lkup_num_writes; j++)
			do_write(ncf->vnf_mem);
		for (j = 0; j < noisy_lkup_num_reads; j++)
			do_read(ncf->vnf_mem);
		for (j = 0; j < noisy_lkup_num_reads_writes; j++)
			do_readwrite(ncf->vnf_mem);
	}
}

/*
 * Forwarding of packets in noisy VNF mode.  Forward packets but perform
 * memory operations first as specified on cmdline.
 *
 * Depending on which commandline parameters are specified we have
 * different cases to handle:
 *
 * 1. No FIFO size was given, so we don't do buffering of incoming
 *    packets.  This case is pretty much what iofwd does but in this case
 *    we also do simulation of memory accesses (depending on which
 *    parameters were specified for it).
 * 2. User wants do buffer packets in a FIFO and sent out overflowing
 *    packets.
 * 3. User wants a FIFO and specifies a time in ms to flush all packets
 *    out of the FIFO
 * 4. Cases 2 and 3 combined
 */
static uint16_t
noisy_eth_tx_burst(struct fwd_stream *fs, uint16_t nb_rx, struct rte_mbuf **pkts_burst)
{
	const uint64_t freq_khz = rte_get_timer_hz() / 1000;
	struct noisy_config *ncf = noisy_cfg[fs->rx_port];
	struct rte_mbuf *tmp_pkts[MAX_PKT_BURST];
	uint16_t nb_deqd = 0;
	uint16_t nb_tx = 0;
	uint16_t nb_enqd;
	unsigned int fifo_free;
	uint64_t delta_ms;
	bool needs_flush = false;
	uint64_t now;

	if (unlikely(nb_rx == 0)) {
		if (!ncf->do_buffering)
			goto end;
		else
			goto flush;
	}

	if (!ncf->do_buffering) {
		if (ncf->do_sim)
			sim_memory_lookups(ncf, nb_rx);
		nb_tx = common_fwd_stream_transmit(fs, pkts_burst, nb_rx);
		goto end;
	}

	fifo_free = rte_ring_free_count(ncf->f);
	if (fifo_free >= nb_rx) {
		nb_enqd = rte_ring_enqueue_burst(ncf->f, (void **) pkts_burst, nb_rx, NULL);
		if (nb_enqd < nb_rx) {
			fs->fwd_dropped += nb_rx - nb_enqd;
			rte_pktmbuf_free_bulk(&pkts_burst[nb_enqd], nb_rx - nb_enqd);
		}
	} else {
		nb_deqd = rte_ring_dequeue_burst(ncf->f, (void **) tmp_pkts, nb_rx, NULL);
		nb_enqd = rte_ring_enqueue_burst(ncf->f, (void **) pkts_burst, nb_deqd, NULL);
		if (nb_deqd > 0)
			nb_tx = common_fwd_stream_transmit(fs, tmp_pkts, nb_deqd);
	}

	if (ncf->do_sim)
		sim_memory_lookups(ncf, nb_enqd);

flush:
	if (ncf->do_flush) {
		if (!ncf->prev_time)
			now = ncf->prev_time = rte_get_timer_cycles();
		else
			now = rte_get_timer_cycles();
		delta_ms = (now - ncf->prev_time) / freq_khz;
		needs_flush = delta_ms >= noisy_tx_sw_buf_flush_time &&
				noisy_tx_sw_buf_flush_time > 0 && !nb_tx;
	}
	while (needs_flush && !rte_ring_empty(ncf->f)) {
		nb_deqd = rte_ring_dequeue_burst(ncf->f, (void **)tmp_pkts,
				MAX_PKT_BURST, NULL);
		nb_tx += common_fwd_stream_transmit(fs, tmp_pkts, nb_deqd);
		ncf->prev_time = rte_get_timer_cycles();
	}
end:
	return nb_tx;
}

static bool
pkt_burst_io(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;
	uint16_t nb_tx;

	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	nb_tx = noisy_eth_tx_burst(fs, nb_rx, pkts_burst);

	return nb_rx > 0 || nb_tx > 0;
}

static bool
pkt_burst_mac(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;
	uint16_t nb_tx;

	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (likely(nb_rx != 0))
		do_macfwd(pkts_burst, nb_rx, fs);
	nb_tx = noisy_eth_tx_burst(fs, nb_rx, pkts_burst);

	return nb_rx > 0 || nb_tx > 0;
}

static bool
pkt_burst_macswap(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;
	uint16_t nb_tx;

	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (likely(nb_rx != 0))
		do_macswap(pkts_burst, nb_rx, &ports[fs->tx_port]);
	nb_tx = noisy_eth_tx_burst(fs, nb_rx, pkts_burst);

	return nb_rx > 0 || nb_tx > 0;
}

static bool
pkt_burst_5tswap(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;
	uint16_t nb_tx;

	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (likely(nb_rx != 0))
		do_5tswap(pkts_burst, nb_rx, fs);
	nb_tx = noisy_eth_tx_burst(fs, nb_rx, pkts_burst);

	return nb_rx > 0 || nb_tx > 0;
}

static void
noisy_fwd_end(portid_t pi)
{
	rte_ring_free(noisy_cfg[pi]->f);
	rte_free(noisy_cfg[pi]->vnf_mem);
	rte_free(noisy_cfg[pi]);
}

static int
noisy_fwd_begin(portid_t pi)
{
	struct noisy_config *n;
	char name[NOISY_STRSIZE];

	noisy_cfg[pi] = rte_zmalloc("testpmd noisy fifo and timers",
				sizeof(struct noisy_config),
				RTE_CACHE_LINE_SIZE);
	if (noisy_cfg[pi] == NULL) {
		rte_exit(EXIT_FAILURE,
			 "rte_zmalloc(%d) struct noisy_config) failed\n",
			 (int) pi);
	}
	n = noisy_cfg[pi];
	n->do_buffering = noisy_tx_sw_bufsz > 0;
	n->do_sim = noisy_lkup_num_writes + noisy_lkup_num_reads +
		    noisy_lkup_num_reads_writes;
	n->do_flush = noisy_tx_sw_buf_flush_time > 0;

	if (n->do_buffering) {
		snprintf(name, NOISY_STRSIZE, NOISY_RING, pi);
		n->f = rte_ring_create(name, noisy_tx_sw_bufsz,
				rte_socket_id(), 0);
		if (!n->f)
			rte_exit(EXIT_FAILURE,
				 "rte_ring_create(%d), size %d) failed\n",
				 (int) pi,
				 noisy_tx_sw_bufsz);
	}
	if (noisy_lkup_mem_sz > 0) {
		n->vnf_mem = (char *) rte_zmalloc("vnf sim memory",
				 noisy_lkup_mem_sz * 1024 * 1024,
				 RTE_CACHE_LINE_SIZE);
		if (!n->vnf_mem)
			rte_exit(EXIT_FAILURE,
			   "rte_zmalloc(%" PRIu64 ") for vnf memory) failed\n",
			   noisy_lkup_mem_sz);
	} else if (n->do_sim) {
		rte_exit(EXIT_FAILURE,
			 "--noisy-lkup-memory-size must be > 0\n");
	}

	if (noisy_fwd_mode == NOISY_FWD_MODE_IO)
		noisy_vnf_engine.packet_fwd = pkt_burst_io;
	else if (noisy_fwd_mode == NOISY_FWD_MODE_MAC)
		noisy_vnf_engine.packet_fwd = pkt_burst_mac;
	else if (noisy_fwd_mode == NOISY_FWD_MODE_MACSWAP)
		noisy_vnf_engine.packet_fwd = pkt_burst_macswap;
	else if (noisy_fwd_mode == NOISY_FWD_MODE_5TSWAP)
		noisy_vnf_engine.packet_fwd = pkt_burst_5tswap;
	else
		rte_exit(EXIT_FAILURE,
			 " Invalid noisy_fwd_mode specified\n");

	noisy_vnf_engine.status = noisy_fwd_mode_desc[noisy_fwd_mode];

	return 0;
}

struct fwd_engine noisy_vnf_engine = {
	.fwd_mode_name  = "noisy",
	.port_fwd_begin = noisy_fwd_begin,
	.port_fwd_end   = noisy_fwd_end,
	.stream_init	= common_fwd_stream_init,
	.packet_fwd     = pkt_burst_io,
};
