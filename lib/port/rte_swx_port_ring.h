/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef __INCLUDE_RTE_SWX_PORT_RING_H__
#define __INCLUDE_RTE_SWX_PORT_RING_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Ring Input and Output Ports
 ***/

#include <stdint.h>


#include "rte_swx_port.h"

/** Ring input port (reader) creation parameters. */
struct rte_swx_port_ring_reader_params {
	/** Name of valid RTE ring. */
	const char *name;

	/** Read burst size. */
	uint32_t burst_size;
};

/** Ring_reader operations. */
extern struct rte_swx_port_in_ops rte_swx_port_ring_reader_ops;

/** Ring output port (writer) creation parameters. */
struct rte_swx_port_ring_writer_params {
	/** Name of valid RTE ring. */
	const char *name;

	/** Read burst size. */
	uint32_t burst_size;
};

/** Ring writer operations. */
extern struct rte_swx_port_out_ops rte_swx_port_ring_writer_ops;

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SWX_PORT_RING_H__ */
