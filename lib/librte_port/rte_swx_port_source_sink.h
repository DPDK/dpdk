/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_PORT_SOURCE_SINK_H__
#define __INCLUDE_RTE_SWX_PORT_SOURCE_SINK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Source and Sink Ports
 */

#include "rte_swx_port.h"

/** Maximum number of packets to read from the PCAP file. */
#ifndef RTE_SWX_PORT_SOURCE_PKTS_MAX
#define RTE_SWX_PORT_SOURCE_PKTS_MAX 1024
#endif

/** Source port creation parameters. */
struct rte_swx_port_source_params {
	/** Buffer pool. Must be valid. */
	struct rte_mempool *pool;

	/** Name of a valid PCAP file to read the input packets from. */
	const char *file_name;

	/** Maximum number of packets to read from the PCAP file. When 0, it is
	 * internally set to RTE_SWX_PORT_SOURCE_PKTS_MAX. Once read from the
	 * PCAP file, the same packets are looped forever.
	 */
	uint32_t n_pkts_max;
};

/** Source port operations. */
extern struct rte_swx_port_in_ops rte_swx_port_source_ops;

/** Sink port creation parameters. */
struct rte_swx_port_sink_params {
	/** Name of a valid PCAP file to write the output packets to. When NULL,
	 * all the output packets are dropped instead of being saved to a PCAP
	 * file.
	 */
	const char *file_name;
};

/** Sink port operations. */
extern struct rte_swx_port_out_ops rte_swx_port_sink_ops;

#ifdef __cplusplus
}
#endif

#endif
