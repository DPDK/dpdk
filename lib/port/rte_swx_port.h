/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PORT_H__
#define __INCLUDE_RTE_SWX_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Port
 *
 * Packet I/O port interface.
 */

#include <stdint.h>

/** Packet. */
struct rte_swx_pkt {
	/** Opaque packet handle. */
	void *handle;

	/** Buffer where the packet is stored. */
	uint8_t *pkt;

	/** Packet buffer offset of the first packet byte. */
	uint32_t offset;

	/** Packet length in bytes. */
	uint32_t length;
};

/*
 * Input port
 */

/**
 * Input port create
 *
 * @param[in] args
 *   Arguments for input port creation. Format specific to each port type.
 * @return
 *   Handle to input port instance on success, NULL on error.
 */
typedef void *
(*rte_swx_port_in_create_t)(void *args);

/**
 * Input port free
 *
 * @param[in] port
 *   Input port handle.
 */
typedef void
(*rte_swx_port_in_free_t)(void *port);

/**
 * Input port packet receive
 *
 * @param[in] port
 *   Input port handle.
 * @param[out] pkt
 *   Received packet. Only valid when the function returns 1. Must point to
 *   valid memory.
 * @return
 *   0 when no packet was received, 1 when a packet was received. No other
 *   return values are allowed.
 */
typedef int
(*rte_swx_port_in_pkt_rx_t)(void *port,
			    struct rte_swx_pkt *pkt);

/** Input port statistics counters. */
struct rte_swx_port_in_stats {
	/** Number of packets. */
	uint64_t n_pkts;

	/** Number of bytes. */
	uint64_t n_bytes;

	/** Number of empty polls. */
	uint64_t n_empty;
};

/**
 * Input port statistics counters read
 *
 * @param[in] port
 *   Input port handle.
 * @param[out] stats
 *   Input port statistics counters. Must point to valid memory.
 */
typedef void
(*rte_swx_port_in_stats_read_t)(void *port,
				struct rte_swx_port_in_stats *stats);

/** Input port operations. */
struct rte_swx_port_in_ops {
	/** Create. Must be non-NULL. */
	rte_swx_port_in_create_t create;

	/** Free. Must be non-NULL. */
	rte_swx_port_in_free_t free;

	/** Packet reception. Must be non-NULL. */
	rte_swx_port_in_pkt_rx_t pkt_rx;

	/** Statistics counters read. Must be non-NULL. */
	rte_swx_port_in_stats_read_t stats_read;
};

/*
 * Output port
 */

/**
 * Output port create
 *
 * @param[in] args
 *   Arguments for output port creation. Format specific to each port type.
 * @return
 *   Handle to output port instance on success, NULL on error.
 */
typedef void *
(*rte_swx_port_out_create_t)(void *args);

/**
 * Output port free
 *
 * @param[in] port
 *   Output port handle.
 */
typedef void
(*rte_swx_port_out_free_t)(void *port);

/**
 * Output port packet transmit
 *
 * @param[in] port
 *   Output port handle.
 * @param[in] pkt
 *   Packet to be transmitted.
 */
typedef void
(*rte_swx_port_out_pkt_tx_t)(void *port,
			     struct rte_swx_pkt *pkt);

/**
 * Output port packet fast clone and transmit
 *
 * @param[in] port
 *   Output port handle.
 * @param[in] pkt
 *   Packet to be transmitted.
 */
typedef void
(*rte_swx_port_out_pkt_fast_clone_tx_t)(void *port,
					struct rte_swx_pkt *pkt);

/**
 * Output port packet clone and transmit
 *
 * @param[in] port
 *   Output port handle.
 * @param[in] pkt
 *   Packet to be transmitted.
 * @param[in] truncation_length
 *   Packet length to be cloned.
 */
typedef void
(*rte_swx_port_out_pkt_clone_tx_t)(void *port,
				   struct rte_swx_pkt *pkt,
				   uint32_t truncation_length);

/**
 * Output port flush
 *
 * @param[in] port
 *   Output port handle.
 */
typedef void
(*rte_swx_port_out_flush_t)(void *port);

/** Output port statistics counters. */
struct rte_swx_port_out_stats {
	/** Number of packets successfully transmitted. */
	uint64_t n_pkts;

	/** Number of bytes successfully transmitted. */
	uint64_t n_bytes;

	/** Number of packets dropped. */
	uint64_t n_pkts_drop;

	/** Number of bytes dropped. */
	uint64_t n_bytes_drop;

	/** Number of packets cloned successfully. */
	uint64_t n_pkts_clone;

	/** Number of packets with clone errors. */
	uint64_t n_pkts_clone_err;
};

/**
 * Output port statistics counters read
 *
 * @param[in] port
 *   Output port handle.
 * @param[out] stats
 *   Output port statistics counters. Must point to valid memory.
 */
typedef void
(*rte_swx_port_out_stats_read_t)(void *port,
				 struct rte_swx_port_out_stats *stats);

/** Output port operations. */
struct rte_swx_port_out_ops {
	/** Create. Must be non-NULL. */
	rte_swx_port_out_create_t create;

	/** Free. Must be non-NULL. */
	rte_swx_port_out_free_t free;

	/** Packet transmission. Must be non-NULL. */
	rte_swx_port_out_pkt_tx_t pkt_tx;

	/** Packet fast clone and transmission. Must be non-NULL. */
	rte_swx_port_out_pkt_fast_clone_tx_t pkt_fast_clone_tx;

	/** Packet clone and transmission. Must be non-NULL. */
	rte_swx_port_out_pkt_clone_tx_t pkt_clone_tx;

	/** Flush. May be NULL. */
	rte_swx_port_out_flush_t flush;

	/** Statistics counters read. Must be non-NULL. */
	rte_swx_port_out_stats_read_t stats_read;
};

#ifdef __cplusplus
}
#endif

#endif
