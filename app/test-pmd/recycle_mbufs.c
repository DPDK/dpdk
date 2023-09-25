/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Arm Limited.
 */

#include "testpmd.h"

/*
 * Forwarding of packets in I/O mode.
 * Enable mbufs recycle mode to recycle txq used mbufs
 * for rxq mbuf ring. This can bypass mempool path and
 * save CPU cycles.
 */
static bool
pkt_burst_recycle_mbufs(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint16_t nb_rx;

	/* Recycle used mbufs from the txq, and move these mbufs into
	 * the rxq mbuf ring.
	 */
	rte_eth_recycle_mbufs(fs->rx_port, fs->rx_queue,
			fs->tx_port, fs->tx_queue, &(fs->recycle_rxq_info));

	/*
	 * Receive a burst of packets and forward them.
	 */
	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return false;

	common_fwd_stream_transmit(fs, pkts_burst, nb_rx);

	return true;
}

static void
recycle_mbufs_stream_init(struct fwd_stream *fs)
{
	int rc;

	/* Retrieve information about given ports's Rx queue
	 * for recycling mbufs.
	 */
	rc = rte_eth_recycle_rx_queue_info_get(fs->rx_port,
			fs->rx_queue, &(fs->recycle_rxq_info));
	if (rc != 0)
		TESTPMD_LOG(WARNING,
			"Failed to get rx queue mbufs recycle info\n");

	common_fwd_stream_init(fs);
}

struct fwd_engine recycle_mbufs_engine = {
	.fwd_mode_name  = "recycle_mbufs",
	.stream_init    = recycle_mbufs_stream_init,
	.packet_fwd     = pkt_burst_recycle_mbufs,
};
