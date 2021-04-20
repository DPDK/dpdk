/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_ETHDEV_H__
#define __INCLUDE_RTE_PORT_ETHDEV_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Ethernet Device
 *
 * ethdev_reader: input port built on top of pre-initialized NIC RX queue
 * ethdev_writer: output port built on top of pre-initialized NIC TX queue
 *
 ***/

#include <stdint.h>

#include "rte_port.h"

/** ethdev_reader port parameters */
struct rte_port_ethdev_reader_params {
	/** NIC RX port ID */
	uint16_t port_id;

	/** NIC RX queue ID */
	uint16_t queue_id;
};

/** ethdev_reader port operations */
extern struct rte_port_in_ops rte_port_ethdev_reader_ops;

/** ethdev_writer port parameters */
struct rte_port_ethdev_writer_params {
	/** NIC RX port ID */
	uint16_t port_id;

	/** NIC RX queue ID */
	uint16_t queue_id;

	/** Recommended burst size to NIC TX queue. The actual burst size can be
	bigger or smaller than this value. */
	uint32_t tx_burst_sz;
};

/** ethdev_writer port operations */
extern struct rte_port_out_ops rte_port_ethdev_writer_ops;

/** ethdev_writer_nodrop port parameters */
struct rte_port_ethdev_writer_nodrop_params {
	/** NIC RX port ID */
	uint16_t port_id;

	/** NIC RX queue ID */
	uint16_t queue_id;

	/** Recommended burst size to NIC TX queue. The actual burst size can be
	bigger or smaller than this value. */
	uint32_t tx_burst_sz;

	/** Maximum number of retries, 0 for no limit */
	uint32_t n_retries;
};

/** ethdev_writer_nodrop port operations */
extern struct rte_port_out_ops rte_port_ethdev_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
