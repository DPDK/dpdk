/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_FD_H__
#define __INCLUDE_RTE_PORT_FD_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port FD Device
 *
 * fd_reader: input port built on top of valid non-blocking file descriptor
 * fd_writer: output port built on top of valid non-blocking file descriptor
 *
 ***/

#include <stdint.h>

#include "rte_port.h"

/** fd_reader port parameters */
struct rte_port_fd_reader_params {
	/** File descriptor */
	int fd;

	/** Maximum Transfer Unit (MTU) */
	uint32_t mtu;

	/** Pre-initialized buffer pool */
	struct rte_mempool *mempool;
};

/** fd_reader port operations */
extern struct rte_port_in_ops rte_port_fd_reader_ops;

/** fd_writer port parameters */
struct rte_port_fd_writer_params {
	/** File descriptor */
	int fd;

	/**< Recommended write burst size. The actual burst size can be
	 * bigger or smaller than this value.
	 */
	uint32_t tx_burst_sz;
};

/** fd_writer port operations */
extern struct rte_port_out_ops rte_port_fd_writer_ops;

/** fd_writer_nodrop port parameters */
struct rte_port_fd_writer_nodrop_params {
	/** File descriptor */
	int fd;

	/**< Recommended write burst size. The actual burst size can be
	 * bigger or smaller than this value.
	 */
	uint32_t tx_burst_sz;

	/** Maximum number of retries, 0 for no limit */
	uint32_t n_retries;
};

/** fd_writer_nodrop port operations */
extern struct rte_port_out_ops rte_port_fd_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
