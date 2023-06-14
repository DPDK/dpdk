/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_SCHED_H__
#define __INCLUDE_RTE_PORT_SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Hierarchical Scheduler
 *
 * sched_reader: input port built on top of pre-initialized rte_sched_port
 * sched_writer: output port built on top of pre-initialized rte_sched_port
 */

#include <stdint.h>

#include <rte_sched.h>

#include "rte_port.h"

/** sched_reader port parameters */
struct rte_port_sched_reader_params {
	/** Underlying pre-initialized rte_sched_port */
	struct rte_sched_port *sched;
};

/** sched_reader port operations */
extern struct rte_port_in_ops rte_port_sched_reader_ops;

/** sched_writer port parameters */
struct rte_port_sched_writer_params {
	/** Underlying pre-initialized rte_sched_port */
	struct rte_sched_port *sched;

	/** Recommended burst size. The actual burst size can be bigger or
	smaller than this value. */
	uint32_t tx_burst_sz;
};

/** sched_writer port operations */
extern struct rte_port_out_ops rte_port_sched_writer_ops;

#ifdef __cplusplus
}
#endif

#endif
