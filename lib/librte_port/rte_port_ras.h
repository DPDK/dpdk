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

#ifndef __INCLUDE_RTE_PORT_RAS_H__
#define __INCLUDE_RTE_PORT_RAS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port for IPv4 Reassembly
 *
 * This port is built on top of pre-initialized single producer rte_ring. In
 * order to minimize the amount of packets stored in the ring at any given
 * time, the IP reassembly functionality is executed on ring write operation,
 * hence this port is implemented as an output port. A regular ring_reader port
 * can be created to read from the same ring.
 *
 * The packets written to the ring are either complete IP datagrams or IP
 * fragments. The packets read from the ring are all complete IP datagrams,
 * either jumbo frames (i.e. IP packets with length bigger than MTU) or not.
 * The complete IP datagrams written to the ring are not changed. The IP
 * fragments written to the ring are first reassembled and into complete IP
 * datagrams or dropped on error or IP reassembly time-out.
 *
 ***/

#include <stdint.h>

#include <rte_ring.h>

#include "rte_port.h"

/** ring_writer_ipv4_ras port parameters */
struct rte_port_ring_writer_ras_params {
	/** Underlying single consumer ring that has to be pre-initialized. */
	struct rte_ring *ring;

	/** Recommended burst size to ring. The actual burst size can be bigger
	or smaller than this value. */
	uint32_t tx_burst_sz;
};

#define rte_port_ring_writer_ipv4_ras_params rte_port_ring_writer_ras_params

#define rte_port_ring_writer_ipv6_ras_params rte_port_ring_writer_ras_params

/** ring_writer_ipv4_ras port operations */
extern struct rte_port_out_ops rte_port_ring_writer_ipv4_ras_ops;

/** ring_writer_ipv6_ras port operations */
extern struct rte_port_out_ops rte_port_ring_writer_ipv6_ras_ops;

#ifdef __cplusplus
}
#endif

#endif
