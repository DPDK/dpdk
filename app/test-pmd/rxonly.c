/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_net.h>
#include <rte_flow.h>

#include "testpmd.h"

/*
 * Received a burst of packets.
 */
static void
pkt_burst_receive(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;
	uint16_t i;
	uint64_t start_tsc = 0;

	get_start_cycles(&start_tsc);

	/*
	 * Receive a burst of packets.
	 */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	inc_rx_burst_stats(fs, nb_rx);
	if (unlikely(nb_rx == 0))
		return;

	fs->rx_packets += nb_rx;
	for (i = 0; i < nb_rx; i++)
		rte_pktmbuf_free(pkts_burst[i]);

	get_end_cycles(fs, start_tsc);
}

static void
stream_init_receive(struct fwd_stream *fs)
{
	fs->disabled = ports[fs->rx_port].rxq[fs->rx_queue].state ==
						RTE_ETH_QUEUE_STATE_STOPPED;
}

struct fwd_engine rx_only_engine = {
	.fwd_mode_name  = "rxonly",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.stream_init    = stream_init_receive,
	.packet_fwd     = pkt_burst_receive,
};
