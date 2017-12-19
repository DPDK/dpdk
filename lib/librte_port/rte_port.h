/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_H__
#define __INCLUDE_RTE_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port
 *
 * This tool is part of the DPDK Packet Framework tool suite and provides
 * a standard interface to implement different types of packet ports.
 *
 ***/

#include <stdint.h>
#include <rte_mbuf.h>

/**@{
 * Macros to allow accessing metadata stored in the mbuf headroom
 * just beyond the end of the mbuf data structure returned by a port
 */
#define RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset)          \
	(&((uint8_t *)(mbuf))[offset])
#define RTE_MBUF_METADATA_UINT16_PTR(mbuf, offset)         \
	((uint16_t *) RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset))
#define RTE_MBUF_METADATA_UINT32_PTR(mbuf, offset)         \
	((uint32_t *) RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset))
#define RTE_MBUF_METADATA_UINT64_PTR(mbuf, offset)         \
	((uint64_t *) RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset))

#define RTE_MBUF_METADATA_UINT8(mbuf, offset)              \
	(*RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset))
#define RTE_MBUF_METADATA_UINT16(mbuf, offset)             \
	(*RTE_MBUF_METADATA_UINT16_PTR(mbuf, offset))
#define RTE_MBUF_METADATA_UINT32(mbuf, offset)             \
	(*RTE_MBUF_METADATA_UINT32_PTR(mbuf, offset))
#define RTE_MBUF_METADATA_UINT64(mbuf, offset)             \
	(*RTE_MBUF_METADATA_UINT64_PTR(mbuf, offset))
/**@}*/

/*
 * Port IN
 *
 */
/** Maximum number of packets read from any input port in a single burst.
Cannot be changed. */
#define RTE_PORT_IN_BURST_SIZE_MAX                         64

/** Input port statistics */
struct rte_port_in_stats {
	uint64_t n_pkts_in;
	uint64_t n_pkts_drop;
};

/**
 * Input port create
 *
 * @param params
 *   Parameters for input port creation
 * @param socket_id
 *   CPU socket ID (e.g. for memory allocation purpose)
 * @return
 *   Handle to input port instance
 */
typedef void* (*rte_port_in_op_create)(void *params, int socket_id);

/**
 * Input port free
 *
 * @param port
 *   Handle to input port instance
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_in_op_free)(void *port);

/**
 * Input port packet burst RX
 *
 * @param port
 *   Handle to input port instance
 * @param pkts
 *   Burst of input packets
 * @param n_pkts
 *   Number of packets in the input burst
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_in_op_rx)(
	void *port,
	struct rte_mbuf **pkts,
	uint32_t n_pkts);

/**
 * Input port stats get
 *
 * @param port
 *   Handle to output port instance
 * @param stats
 *   Handle to port_in stats struct to copy data
 * @param clear
 *   Flag indicating that stats should be cleared after read
 *
 * @return
 *   Error code or 0 on success.
 */
typedef int (*rte_port_in_op_stats_read)(
		void *port,
		struct rte_port_in_stats *stats,
		int clear);

/** Input port interface defining the input port operation */
struct rte_port_in_ops {
	rte_port_in_op_create f_create;      /**< Create */
	rte_port_in_op_free f_free;          /**< Free */
	rte_port_in_op_rx f_rx;              /**< Packet RX (packet burst) */
	rte_port_in_op_stats_read f_stats;   /**< Stats */
};

/*
 * Port OUT
 *
 */
/** Output port statistics */
struct rte_port_out_stats {
	uint64_t n_pkts_in;
	uint64_t n_pkts_drop;
};

/**
 * Output port create
 *
 * @param params
 *   Parameters for output port creation
 * @param socket_id
 *   CPU socket ID (e.g. for memory allocation purpose)
 * @return
 *   Handle to output port instance
 */
typedef void* (*rte_port_out_op_create)(void *params, int socket_id);

/**
 * Output port free
 *
 * @param port
 *   Handle to output port instance
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_out_op_free)(void *port);

/**
 * Output port single packet TX
 *
 * @param port
 *   Handle to output port instance
 * @param pkt
 *   Input packet
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_out_op_tx)(
	void *port,
	struct rte_mbuf *pkt);

/**
 * Output port packet burst TX
 *
 * @param port
 *   Handle to output port instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param pkts_mask
 *   64-bit bitmask specifying which packets in the input burst are valid. When
 *   pkts_mask bit n is set, then element n of pkts array is pointing to a
 *   valid packet. Otherwise, element n of pkts array will not be accessed.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_out_op_tx_bulk)(
	void *port,
	struct rte_mbuf **pkt,
	uint64_t pkts_mask);

/**
 * Output port flush
 *
 * @param port
 *   Handle to output port instance
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_port_out_op_flush)(void *port);

/**
 * Output port stats read
 *
 * @param port
 *   Handle to output port instance
 * @param stats
 *   Handle to port_out stats struct to copy data
 * @param clear
 *   Flag indicating that stats should be cleared after read
 *
 * @return
 *   Error code or 0 on success.
 */
typedef int (*rte_port_out_op_stats_read)(
		void *port,
		struct rte_port_out_stats *stats,
		int clear);

/** Output port interface defining the output port operation */
struct rte_port_out_ops {
	rte_port_out_op_create f_create;      /**< Create */
	rte_port_out_op_free f_free;          /**< Free */
	rte_port_out_op_tx f_tx;              /**< Packet TX (single packet) */
	rte_port_out_op_tx_bulk f_tx_bulk;    /**< Packet TX (packet burst) */
	rte_port_out_op_flush f_flush;        /**< Flush */
	rte_port_out_op_stats_read f_stats;   /**< Stats */
};

#ifdef __cplusplus
}
#endif

#endif
