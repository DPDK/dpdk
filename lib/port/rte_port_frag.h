/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_IP_FRAG_H__
#define __INCLUDE_RTE_PORT_IP_FRAG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port for IPv4 Fragmentation
 *
 * This port is built on top of pre-initialized single consumer rte_ring. In
 * order to minimize the amount of packets stored in the ring at any given
 * time, the IP fragmentation functionality is executed on ring read operation,
 * hence this port is implemented as an input port. A regular ring_writer port
 * can be created to write to the same ring.
 *
 * The packets written to the ring are either complete IP datagrams or jumbo
 * frames (i.e. IP packets with length bigger than provided MTU value). The
 * packets read from the ring are all non-jumbo frames. The complete IP
 * datagrams written to the ring are not changed. The jumbo frames are
 * fragmented into several IP packets with length less or equal to MTU.
 */

#include <stdint.h>


#include "rte_port.h"

/** ring_reader_ipv4_frag port parameters */
struct rte_port_ring_reader_frag_params {
	/** Underlying single consumer ring that has to be pre-initialized. */
	struct rte_ring *ring;

	/** Maximum Transfer Unit (MTU). Maximum IP packet size (in bytes). */
	uint32_t mtu;

	/** Size of application dependent meta-data stored per each input packet
	    that has to be copied to each of the fragments originating from the
	    same input IP datagram. */
	uint32_t metadata_size;

	/** Pre-initialized buffer pool used for allocating direct buffers for
	    the output fragments. */
	struct rte_mempool *pool_direct;

	/** Pre-initialized buffer pool used for allocating indirect buffers for
	    the output fragments. */
	struct rte_mempool *pool_indirect;
};

#define rte_port_ring_reader_ipv4_frag_params rte_port_ring_reader_frag_params

#define rte_port_ring_reader_ipv6_frag_params rte_port_ring_reader_frag_params

/** ring_reader_ipv4_frag port operations */
extern struct rte_port_in_ops rte_port_ring_reader_ipv4_frag_ops;

/** ring_reader_ipv6_frag port operations */
extern struct rte_port_in_ops rte_port_ring_reader_ipv6_frag_ops;

#ifdef __cplusplus
}
#endif

#endif
