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
 *
 ***/

#include <stdint.h>

#include <rte_ring.h>

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
