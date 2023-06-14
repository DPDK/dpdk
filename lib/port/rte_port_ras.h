/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
 */

#include <stdint.h>

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
