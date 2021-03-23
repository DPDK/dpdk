/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef __INCLUDE_RTE_SWX_PORT_FD_H__
#define __INCLUDE_RTE_SWX_PORT_FD_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX FD Input and Output Ports
 *
 ***/
#include <stdint.h>

#include <rte_mempool.h>

#include "rte_swx_port.h"

/** fd_reader port parameters */
struct rte_swx_port_fd_reader_params {
	/** File descriptor. Must be valid and opened in non-blocking mode. */
	int fd;

	/** Maximum Transfer Unit (MTU) */
	uint32_t mtu;

	/** Pre-initialized buffer pool */
	struct rte_mempool *mempool;

	/** RX burst size */
	uint32_t burst_size;
};

/** fd_reader port operations */
extern struct rte_swx_port_in_ops rte_swx_port_fd_reader_ops;

/** fd_writer port parameters */
struct rte_swx_port_fd_writer_params {
	/** File descriptor. Must be valid and opened in non-blocking mode. */
	int fd;

	/** TX burst size */
	uint32_t burst_size;
};

/** fd_writer port operations */
extern struct rte_swx_port_out_ops rte_swx_port_fd_writer_ops;

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SWX_PORT_FD_H__ */
