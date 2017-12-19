/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_RING_H__
#define __INCLUDE_RTE_PORT_RING_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Ring
 *
 * ring_reader:
 *      input port built on top of pre-initialized single consumer ring
 * ring_writer:
 *      output port built on top of pre-initialized single producer ring
 * ring_multi_reader:
 *      input port built on top of pre-initialized multi consumers ring
 * ring_multi_writer:
 *      output port built on top of pre-initialized multi producers ring
 *
 ***/

#include <stdint.h>

#include <rte_ring.h>

#include "rte_port.h"

/** ring_reader port parameters */
struct rte_port_ring_reader_params {
	/** Underlying consumer ring that has to be pre-initialized */
	struct rte_ring *ring;
};

/** ring_reader port operations */
extern struct rte_port_in_ops rte_port_ring_reader_ops;

/** ring_writer port parameters */
struct rte_port_ring_writer_params {
	/** Underlying producer ring that has to be pre-initialized */
	struct rte_ring *ring;

	/** Recommended burst size to ring. The actual burst size can be
		bigger or smaller than this value. */
	uint32_t tx_burst_sz;
};

/** ring_writer port operations */
extern struct rte_port_out_ops rte_port_ring_writer_ops;

/** ring_writer_nodrop port parameters */
struct rte_port_ring_writer_nodrop_params {
	/** Underlying producer ring that has to be pre-initialized */
	struct rte_ring *ring;

	/** Recommended burst size to ring. The actual burst size can be
		bigger or smaller than this value. */
	uint32_t tx_burst_sz;

	/** Maximum number of retries, 0 for no limit */
	uint32_t n_retries;
};

/** ring_writer_nodrop port operations */
extern struct rte_port_out_ops rte_port_ring_writer_nodrop_ops;

/** ring_multi_reader port parameters */
#define rte_port_ring_multi_reader_params rte_port_ring_reader_params

/** ring_multi_reader port operations */
extern struct rte_port_in_ops rte_port_ring_multi_reader_ops;

/** ring_multi_writer port parameters */
#define rte_port_ring_multi_writer_params rte_port_ring_writer_params

/** ring_multi_writer port operations */
extern struct rte_port_out_ops rte_port_ring_multi_writer_ops;

/** ring_multi_writer_nodrop port parameters */
#define rte_port_ring_multi_writer_nodrop_params \
	rte_port_ring_writer_nodrop_params

/** ring_multi_writer_nodrop port operations */
extern struct rte_port_out_ops rte_port_ring_multi_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
