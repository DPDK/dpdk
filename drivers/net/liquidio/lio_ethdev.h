/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _LIO_ETHDEV_H_
#define _LIO_ETHDEV_H_

#include <stdint.h>

#include "lio_struct.h"

/* timeout to check link state updates from firmware in us */
#define LIO_LSC_TIMEOUT		100000 /* 100000us (100ms) */
#define LIO_MAX_CMD_TIMEOUT     10000 /* 10000ms (10s) */

#define LIO_DEV(_eth_dev)		((_eth_dev)->data->dev_private)

/* LIO Response condition variable */
struct lio_dev_ctrl_cmd {
	struct rte_eth_dev *eth_dev;
	uint64_t cond;
};

enum lio_bus_speed {
	LIO_LINK_SPEED_UNKNOWN  = 0,
	LIO_LINK_SPEED_10000    = 10000,
	LIO_LINK_SPEED_25000    = 25000
};

struct octeon_if_cfg_info {
	uint64_t iqmask;	/** mask for IQs enabled for the port */
	uint64_t oqmask;	/** mask for OQs enabled for the port */
	struct octeon_link_info linfo; /** initial link information */
	char lio_firmware_version[LIO_FW_VERSION_LENGTH];
};

/** Stats for each NIC port in RX direction. */
struct octeon_rx_stats {
	/* link-level stats */
	uint64_t total_rcvd;
	uint64_t bytes_rcvd;
	uint64_t total_bcst;
	uint64_t total_mcst;
	uint64_t runts;
	uint64_t ctl_rcvd;
	uint64_t fifo_err; /* Accounts for over/under-run of buffers */
	uint64_t dmac_drop;
	uint64_t fcs_err;
	uint64_t jabber_err;
	uint64_t l2_err;
	uint64_t frame_err;

	/* firmware stats */
	uint64_t fw_total_rcvd;
	uint64_t fw_total_fwd;
	uint64_t fw_total_fwd_bytes;
	uint64_t fw_err_pko;
	uint64_t fw_err_link;
	uint64_t fw_err_drop;
	uint64_t fw_rx_vxlan;
	uint64_t fw_rx_vxlan_err;

	/* LRO */
	uint64_t fw_lro_pkts;   /* Number of packets that are LROed */
	uint64_t fw_lro_octs;   /* Number of octets that are LROed */
	uint64_t fw_total_lro;  /* Number of LRO packets formed */
	uint64_t fw_lro_aborts; /* Number of times lRO of packet aborted */
	uint64_t fw_lro_aborts_port;
	uint64_t fw_lro_aborts_seq;
	uint64_t fw_lro_aborts_tsval;
	uint64_t fw_lro_aborts_timer;
	/* intrmod: packet forward rate */
	uint64_t fwd_rate;
};

/** Stats for each NIC port in RX direction. */
struct octeon_tx_stats {
	/* link-level stats */
	uint64_t total_pkts_sent;
	uint64_t total_bytes_sent;
	uint64_t mcast_pkts_sent;
	uint64_t bcast_pkts_sent;
	uint64_t ctl_sent;
	uint64_t one_collision_sent;	/* Packets sent after one collision */
	/* Packets sent after multiple collision */
	uint64_t multi_collision_sent;
	/* Packets not sent due to max collisions */
	uint64_t max_collision_fail;
	/* Packets not sent due to max deferrals */
	uint64_t max_deferral_fail;
	/* Accounts for over/under-run of buffers */
	uint64_t fifo_err;
	uint64_t runts;
	uint64_t total_collisions; /* Total number of collisions detected */

	/* firmware stats */
	uint64_t fw_total_sent;
	uint64_t fw_total_fwd;
	uint64_t fw_total_fwd_bytes;
	uint64_t fw_err_pko;
	uint64_t fw_err_link;
	uint64_t fw_err_drop;
	uint64_t fw_err_tso;
	uint64_t fw_tso;     /* number of tso requests */
	uint64_t fw_tso_fwd; /* number of packets segmented in tso */
	uint64_t fw_tx_vxlan;
};

struct octeon_link_stats {
	struct octeon_rx_stats fromwire;
	struct octeon_tx_stats fromhost;
};

union lio_if_cfg {
	uint64_t if_cfg64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t base_queue : 16;
		uint64_t num_iqueues : 16;
		uint64_t num_oqueues : 16;
		uint64_t gmx_port_id : 8;
		uint64_t vf_id : 8;
#else
		uint64_t vf_id : 8;
		uint64_t gmx_port_id : 8;
		uint64_t num_oqueues : 16;
		uint64_t num_iqueues : 16;
		uint64_t base_queue : 16;
#endif
	} s;
};

struct lio_if_cfg_resp {
	uint64_t rh;
	struct octeon_if_cfg_info cfg_info;
	uint64_t status;
};

struct lio_link_stats_resp {
	uint64_t rh;
	struct octeon_link_stats link_stats;
	uint64_t status;
};

struct lio_link_status_resp {
	uint64_t rh;
	struct octeon_link_info link_info;
	uint64_t status;
};

struct lio_rss_set {
	struct param {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		uint64_t flags : 16;
		uint64_t hashinfo : 32;
		uint64_t itablesize : 16;
		uint64_t hashkeysize : 16;
		uint64_t reserved : 48;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t itablesize : 16;
		uint64_t hashinfo : 32;
		uint64_t flags : 16;
		uint64_t reserved : 48;
		uint64_t hashkeysize : 16;
#endif
	} param;

	uint8_t itable[LIO_RSS_MAX_TABLE_SZ];
	uint8_t key[LIO_RSS_MAX_KEY_SZ];
};

void lio_dev_rx_queue_release(void *rxq);

void lio_dev_tx_queue_release(void *txq);

#endif	/* _LIO_ETHDEV_H_ */
