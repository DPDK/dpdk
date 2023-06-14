/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_EVENTDEV_H__
#define __INCLUDE_RTE_PORT_EVENTDEV_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Eventdev Interface
 *
 * eventdev_reader: input port built on top of pre-initialized eventdev
 * interface
 * eventdev_writer: output port built on top of pre-initialized eventdev
 * interface
 */

#include <stdint.h>
#include <rte_eventdev.h>

#include "rte_port.h"

/** Eventdev_reader port parameters */
struct rte_port_eventdev_reader_params {
	/** Eventdev Device ID */
	uint8_t eventdev_id;

	/** Eventdev Port ID */
	uint8_t port_id;
};

/** Eventdev_reader port operations. */
extern struct rte_port_in_ops rte_port_eventdev_reader_ops;

/** Eventdev_writer port parameters. */
struct rte_port_eventdev_writer_params {
	/** Eventdev Device ID. */
	uint8_t eventdev_id;

	/** Eventdev Port ID. */
	uint8_t port_id;

	/** Eventdev Queue ID. */
	uint8_t queue_id;

	/** Burst size to eventdev interface. */
	uint32_t enq_burst_sz;

	/** Scheduler synchronization type (RTE_SCHED_TYPE_*)*/
	uint8_t sched_type;

	/** The type of eventdev enqueue operation - new/forward/release */
	uint8_t evt_op;
};

/** Eventdev_writer port operations. */
extern struct rte_port_out_ops rte_port_eventdev_writer_ops;

/** Event_writer_nodrop port parameters. */
struct rte_port_eventdev_writer_nodrop_params {
	/** Eventdev Device ID. */
	uint8_t eventdev_id;

	/** Eventdev Port ID. */
	uint16_t port_id;

	/** Eventdev Queue ID. */
	uint16_t queue_id;

	/** Burst size to eventdev interface. */
	uint32_t enq_burst_sz;

	/** Scheduler synchronization type (RTE_SCHED_TYPE_*)*/
	uint8_t sched_type;

	/** The type of eventdev enqueue operation - new/forward/release */
	uint8_t evt_op;

	/** Maximum number of retries, 0 for no limit. */
	uint32_t n_retries;
};

/** Eventdev_writer_nodrop port operations. */
extern struct rte_port_out_ops rte_port_eventdev_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
