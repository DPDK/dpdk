/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PORT_ETHDEV_H__
#define __INCLUDE_RTE_SWX_PORT_ETHDEV_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Ethernet Device Input and Output Ports
 */

#include <stdint.h>

#include "rte_swx_port.h"

/** Ethernet device input port (reader) creation parameters. */
struct rte_swx_port_ethdev_reader_params {
	/** Name of a valid and fully configured Ethernet device. */
	const char *dev_name;

	/** Ethernet device receive queue ID. */
	uint16_t queue_id;

	/** Ethernet device receive burst size. */
	uint32_t burst_size;
};

/** Ethernet device reader operations. */
extern struct rte_swx_port_in_ops rte_swx_port_ethdev_reader_ops;

/** Ethernet device output port (writer) creation parameters. */
struct rte_swx_port_ethdev_writer_params {
	/** Name of a valid and fully configured Ethernet device. */
	const char *dev_name;

	/** Ethernet device transmit queue ID. */
	uint16_t queue_id;

	/** Ethernet device transmit burst size. */
	uint32_t burst_size;
};

/** Ethernet device writer operations. */
extern struct rte_swx_port_out_ops rte_swx_port_ethdev_writer_ops;

#ifdef __cplusplus
}
#endif

#endif
