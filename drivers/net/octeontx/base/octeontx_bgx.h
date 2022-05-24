/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_BGX_H__
#define __OCTEONTX_BGX_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <octeontx_mbox.h>

#define OCTEONTX_BGX_RSVD_RX_FIFOBYTES	0x40

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
#define MBOX_BGX_PORT_SET_MTU		14
#define MBOX_BGX_PORT_ADD_MACADDR	15
#define MBOX_BGX_PORT_DEL_MACADDR	16
#define MBOX_BGX_PORT_GET_MACADDR_ENTRIES 17
#define MBOX_BGX_PORT_GET_FIFO_CFG	18
#define MBOX_BGX_PORT_FLOW_CTRL_CFG	19
#define MBOX_BGX_PORT_SET_LINK_STATE	20
#define MBOX_BGX_PORT_CHANGE_MODE	21

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
	uint8_t duplex;
	uint32_t speed;
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
	uint64_t rx_pause_packets;
	uint64_t tx_pause_packets;
} octeontx_mbox_bgx_port_stats_t;

struct octeontx_mbox_bgx_port_mac_filter {
	uint8_t mac_addr[6];
	int index;
};

/* BGX port fifo config: */
typedef struct octeontx_mbox_bgx_port_fifo_cfg {
	uint32_t rx_fifosz; /* in Bytes */
} octeontx_mbox_bgx_port_fifo_cfg_t;

typedef enum {
	BGX_PORT_FC_CFG_GET = 0,
	BGX_PORT_FC_CFG_SET = 1
} bgx_port_fc_t;

/* BGX port flow control config: */
typedef struct octeontx_mbox_bgx_port_fc_cfg {
	/* BP on/off threshold levels in Bytes, must be a multiple of 16 */
	uint16_t high_water;
	uint16_t low_water;
	uint8_t rx_pause; /* rx_pause = 1/0 to enable/disable fc on Tx */
	uint8_t tx_pause; /* tx_pause = 1/0 to enable/disable fc on Rx */
	bgx_port_fc_t fc_cfg;
} octeontx_mbox_bgx_port_fc_cfg_t;

/* BGX change mode  */
typedef struct octeontx_mbox_bgx_port_change_mode {
	uint16_t padding;
	uint8_t  qlm_mode;
	bool	 autoneg;
	uint8_t  duplex;
	uint32_t speed;
} octeontx_mbox_bgx_port_change_mode_t;

int octeontx_bgx_port_open(int port, octeontx_mbox_bgx_port_conf_t *conf);
int octeontx_bgx_port_close(int port);
int octeontx_bgx_port_start(int port);
int octeontx_bgx_port_stop(int port);
int octeontx_bgx_port_get_config(int port, octeontx_mbox_bgx_port_conf_t *conf);
int octeontx_bgx_port_status(int port, octeontx_mbox_bgx_port_status_t *stat);
int octeontx_bgx_port_stats(int port, octeontx_mbox_bgx_port_stats_t *stats);
int octeontx_bgx_port_stats_clr(int port);
int octeontx_bgx_port_xstats(int port, octeontx_mbox_bgx_port_stats_t *stats);
int octeontx_bgx_port_link_status(int port);
int octeontx_bgx_port_promisc_set(int port, int en);
int octeontx_bgx_port_multicast_set(int port, int en);
int octeontx_bgx_port_mac_set(int port, uint8_t *mac_addr);
int octeontx_bgx_port_mac_add(int port, uint8_t *mac_addr, int index);
int octeontx_bgx_port_mac_del(int port, uint32_t index);
int octeontx_bgx_port_mac_entries_get(int port);
int octeontx_bgx_port_mtu_set(int port, int mtu);
int octeontx_bgx_port_set_link_state(int port, bool en);
int octeontx_bgx_port_get_fifo_cfg(int port,
				   octeontx_mbox_bgx_port_fifo_cfg_t *cfg);
int octeontx_bgx_port_flow_ctrl_cfg(int port,
				    octeontx_mbox_bgx_port_fc_cfg_t *cfg);
int octeontx_bgx_port_change_mode(int port,
				  octeontx_mbox_bgx_port_change_mode_t *cfg);

#endif	/* __OCTEONTX_BGX_H__ */

