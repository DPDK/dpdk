/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef __OCTEONTX_BGX_H__
#define __OCTEONTX_BGX_H__

#include <stddef.h>
#include <stdint.h>

#include <octeontx_mbox.h>

#define OCTEONTX_BGX_COPROC	        6

/* BGX messages */
#define MBOX_BGX_PORT_OPEN              0
#define MBOX_BGX_PORT_CLOSE             1
#define MBOX_BGX_PORT_START             2
#define MBOX_BGX_PORT_STOP              3
#define MBOX_BGX_PORT_GET_CONFIG        4
#define MBOX_BGX_PORT_GET_STATUS        5
#define MBOX_BGX_PORT_GET_STATS         6
#define MBOX_BGX_PORT_CLR_STATS         7
#define MBOX_BGX_PORT_GET_LINK_STATUS   8
#define MBOX_BGX_PORT_SET_PROMISC       9
#define MBOX_BGX_PORT_SET_MACADDR       10
#define MBOX_BGX_PORT_SET_BP            11
#define MBOX_BGX_PORT_SET_BCAST         12
#define MBOX_BGX_PORT_SET_MCAST         13

/* BGX port configuration parameters: */
typedef struct octeontx_mbox_bgx_port_conf {
	uint8_t enable;
	uint8_t promisc;
	uint8_t bpen;
	uint8_t macaddr[6]; /* MAC address.*/
	uint8_t fcs_strip;
	uint8_t bcast_mode;
	uint8_t mcast_mode;
	uint8_t node; /* CPU node */
	uint16_t base_chan;
	uint16_t num_chans;
	uint16_t mtu;
	uint8_t bgx;
	uint8_t lmac;
	uint8_t mode;
	uint8_t pkind;
} octeontx_mbox_bgx_port_conf_t;

/* BGX port status: */
typedef struct octeontx_mbox_bgx_port_status {
	uint8_t link_up;
	uint8_t bp;
} octeontx_mbox_bgx_port_status_t;

/* BGX port statistics: */
typedef struct octeontx_mbox_bgx_port_stats {
	uint64_t rx_packets;
	uint64_t tx_packets;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t rx_errors;
	uint64_t tx_errors;
	uint64_t rx_dropped;
	uint64_t tx_dropped;
	uint64_t multicast;
	uint64_t collisions;

	uint64_t rx_length_errors;
	uint64_t rx_over_errors;
	uint64_t rx_crc_errors;
	uint64_t rx_frame_errors;
	uint64_t rx_fifo_errors;
	uint64_t rx_missed_errors;

	/* Detailed transmit errors. */
	uint64_t tx_aborted_errors;
	uint64_t tx_carrier_errors;
	uint64_t tx_fifo_errors;
	uint64_t tx_heartbeat_errors;
	uint64_t tx_window_errors;

	/* Extended statistics based on RFC2819. */
	uint64_t rx_1_to_64_packets;
	uint64_t rx_65_to_127_packets;
	uint64_t rx_128_to_255_packets;
	uint64_t rx_256_to_511_packets;
	uint64_t rx_512_to_1023_packets;
	uint64_t rx_1024_to_1522_packets;
	uint64_t rx_1523_to_max_packets;

	uint64_t tx_1_to_64_packets;
	uint64_t tx_65_to_127_packets;
	uint64_t tx_128_to_255_packets;
	uint64_t tx_256_to_511_packets;
	uint64_t tx_512_to_1023_packets;
	uint64_t tx_1024_to_1522_packets;
	uint64_t tx_1523_to_max_packets;

	uint64_t tx_multicast_packets;
	uint64_t rx_broadcast_packets;
	uint64_t tx_broadcast_packets;
	uint64_t rx_undersized_errors;
	uint64_t rx_oversize_errors;
	uint64_t rx_fragmented_errors;
	uint64_t rx_jabber_errors;
} octeontx_mbox_bgx_port_stats_t;

int octeontx_bgx_port_open(int port, octeontx_mbox_bgx_port_conf_t *conf);
int octeontx_bgx_port_close(int port);
int octeontx_bgx_port_start(int port);
int octeontx_bgx_port_stop(int port);
int octeontx_bgx_port_get_config(int port, octeontx_mbox_bgx_port_conf_t *conf);
int octeontx_bgx_port_status(int port, octeontx_mbox_bgx_port_status_t *stat);
int octeontx_bgx_port_stats(int port, octeontx_mbox_bgx_port_stats_t *stats);
int octeontx_bgx_port_stats_clr(int port);
int octeontx_bgx_port_link_status(int port);
int octeontx_bgx_port_promisc_set(int port, int en);
int octeontx_bgx_port_mac_set(int port, uint8_t *mac_addr);

#endif	/* __OCTEONTX_BGX_H__ */

