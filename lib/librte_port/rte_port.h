/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef __INCLUDE_RTE_PORT_H__
#define __INCLUDE_RTE_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port
 *
 * This tool is part of the Intel DPDK Packet Framework tool suite and provides
 * a standard interface to implement different types of packet ports.
 *
 ***/

#include <stdint.h>
#include <rte_mbuf.h>

/**@{
 * Macros to allow accessing metadata stored in the mbuf headroom
 * just beyond the end of the mbuf data structure returned by a port
 */
#define RTE_MBUF_METADATA_UINT8(mbuf, offset)              \
	(((uint8_t *)&(mbuf)[1])[offset])
#define RTE_MBUF_METADATA_UINT16(mbuf, offset)             \
	(((uint16_t *)&(mbuf)[1])[offset/sizeof(uint16_t)])
#define RTE_MBUF_METADATA_UINT32(mbuf, offset)             \
	(((uint32_t *)&(mbuf)[1])[offset/sizeof(uint32_t)])
#define RTE_MBUF_METADATA_UINT64(mbuf, offset)             \
	(((uint64_t *)&(mbuf)[1])[offset/sizeof(uint64_t)])

#define RTE_MBUF_METADATA_UINT8_PTR(mbuf, offset)          \
	(&RTE_MBUF_METADATA_UINT8(mbuf, offset))
#define RTE_MBUF_METADATA_UINT16_PTR(mbuf, offset)         \
	(&RTE_MBUF_METADATA_UINT16(mbuf, offset))
#define RTE_MBUF_METADATA_UINT32_PTR(mbuf, offset)         \
	(&RTE_MBUF_METADATA_UINT32(mbuf, offset))
#define RTE_MBUF_METADATA_UINT64_PTR(mbuf, offset)         \
	(&RTE_MBUF_METADATA_UINT64(mbuf, offset))
/**@}*/

/*
 * Port IN
 *
 */
/** Maximum number of packets read from any input port in a single burst.
Cannot be changed. */
#define RTE_PORT_IN_BURST_SIZE_MAX                         64

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

/** Input port interface defining the input port operation */
struct rte_port_in_ops {
	rte_port_in_op_create f_create; /**< Create */
	rte_port_in_op_free f_free;     /**< Free */
	rte_port_in_op_rx f_rx;         /**< Packet RX (packet burst) */
};

/*
 * Port OUT
 *
 */
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

/** Output port interface defining the output port operation */
struct rte_port_out_ops {
	rte_port_out_op_create f_create;   /**< Create */
	rte_port_out_op_free f_free;       /**< Free */
	rte_port_out_op_tx f_tx;           /**< Packet TX (single packet) */
	rte_port_out_op_tx_bulk f_tx_bulk; /**< Packet TX (packet burst) */
	rte_port_out_op_flush f_flush;     /**< Flush */
};

#ifdef __cplusplus
}
#endif

#endif
