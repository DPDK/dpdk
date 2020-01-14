/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_STATS_H_
#define _VNIC_STATS_H_

/* Tx statistics */
struct vnic_tx_stats {
	uint64_t tx_frames_ok;
	uint64_t tx_unicast_frames_ok;
	uint64_t tx_multicast_frames_ok;
	uint64_t tx_broadcast_frames_ok;
	uint64_t tx_bytes_ok;
	uint64_t tx_unicast_bytes_ok;
	uint64_t tx_multicast_bytes_ok;
	uint64_t tx_broadcast_bytes_ok;
	uint64_t tx_drops;
	uint64_t tx_errors;
	uint64_t tx_tso;
	uint64_t rsvd[16];
};

/* Rx statistics */
struct vnic_rx_stats {
	uint64_t rx_frames_ok;
	uint64_t rx_frames_total;
	uint64_t rx_unicast_frames_ok;
	uint64_t rx_multicast_frames_ok;
	uint64_t rx_broadcast_frames_ok;
	uint64_t rx_bytes_ok;
	uint64_t rx_unicast_bytes_ok;
	uint64_t rx_multicast_bytes_ok;
	uint64_t rx_broadcast_bytes_ok;
	uint64_t rx_drop;
	uint64_t rx_no_bufs;
	uint64_t rx_errors;
	uint64_t rx_rss;
	uint64_t rx_crc_errors;
	uint64_t rx_frames_64;
	uint64_t rx_frames_127;
	uint64_t rx_frames_255;
	uint64_t rx_frames_511;
	uint64_t rx_frames_1023;
	uint64_t rx_frames_1518;
	uint64_t rx_frames_to_max;
	uint64_t rsvd[16];
};

struct vnic_stats {
	struct vnic_tx_stats tx;
	struct vnic_rx_stats rx;
};

#endif /* _VNIC_STATS_H_ */
