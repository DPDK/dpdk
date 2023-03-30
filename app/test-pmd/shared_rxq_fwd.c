/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <rte_ethdev.h>

#include "testpmd.h"

/*
 * Rx only sub-burst forwarding.
 */
static void
forward_rx_only(uint16_t nb_rx, struct rte_mbuf **pkts_burst)
{
	rte_pktmbuf_free_bulk(pkts_burst, nb_rx);
}

/**
 * Get packet source stream by source port and queue.
 * All streams of same shared Rx queue locates on same core.
 */
static struct fwd_stream *
forward_stream_get(struct fwd_stream *fs, uint16_t port)
{
	streamid_t sm_id;
	struct fwd_lcore *fc;
	struct fwd_stream **fsm;
	streamid_t nb_fs;

	fc = fs->lcore;
	fsm = &fwd_streams[fc->stream_idx];
	nb_fs = fc->stream_nb;
	for (sm_id = 0; sm_id < nb_fs; sm_id++) {
		if (fsm[sm_id]->rx_port == port &&
		    fsm[sm_id]->rx_queue == fs->rx_queue)
			return fsm[sm_id];
	}
	return NULL;
}

/**
 * Forward packet by source port and queue.
 */
static void
forward_sub_burst(struct fwd_stream *src_fs, uint16_t port, uint16_t nb_rx,
		  struct rte_mbuf **pkts)
{
	struct fwd_stream *fs = forward_stream_get(src_fs, port);

	if (fs != NULL) {
		fs->rx_packets += nb_rx;
		forward_rx_only(nb_rx, pkts);
	} else {
		/* Source stream not found, drop all packets. */
		src_fs->fwd_dropped += nb_rx;
		rte_pktmbuf_free_bulk(pkts, nb_rx);
	}
}

/**
 * Forward packets from shared Rx queue.
 *
 * Source port of packets are identified by mbuf->port.
 */
static void
forward_shared_rxq(struct fwd_stream *fs, uint16_t nb_rx,
		   struct rte_mbuf **pkts_burst)
{
	uint16_t i, nb_sub_burst, port, last_port;

	nb_sub_burst = 0;
	last_port = pkts_burst[0]->port;
	/* Locate sub-burst according to mbuf->port. */
	for (i = 0; i < nb_rx - 1; ++i) {
		rte_prefetch0(pkts_burst[i + 1]);
		port = pkts_burst[i]->port;
		if (i > 0 && last_port != port) {
			/* Forward packets with same source port. */
			forward_sub_burst(fs, last_port, nb_sub_burst,
					  &pkts_burst[i - nb_sub_burst]);
			nb_sub_burst = 0;
			last_port = port;
		}
		nb_sub_burst++;
	}
	/* Last sub-burst. */
	nb_sub_burst++;
	forward_sub_burst(fs, last_port, nb_sub_burst,
			  &pkts_burst[nb_rx - nb_sub_burst]);
}

static bool
shared_rxq_fwd(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[nb_pkt_per_burst];
	uint16_t nb_rx;

	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return false;
	forward_shared_rxq(fs, nb_rx, pkts_burst);

	return true;
}

static void
shared_rxq_stream_init(struct fwd_stream *fs)
{
	fs->disabled = ports[fs->rx_port].rxq[fs->rx_queue].state ==
						RTE_ETH_QUEUE_STATE_STOPPED;
}

struct fwd_engine shared_rxq_engine = {
	.fwd_mode_name  = "shared_rxq",
	.stream_init    = shared_rxq_stream_init,
	.packet_fwd     = shared_rxq_fwd,
};
