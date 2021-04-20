/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_SOURCE_SINK_H__
#define __INCLUDE_RTE_PORT_SOURCE_SINK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port Source/Sink
 *
 * source: input port that can be used to generate packets
 * sink: output port that drops all packets written to it
 *
 ***/

#include "rte_port.h"

/** source port parameters */
struct rte_port_source_params {
	/** Pre-initialized buffer pool */
	struct rte_mempool *mempool;

	/** The full path of the pcap file to read packets from */
	const char *file_name;
	/** The number of bytes to be read from each packet in the
	 *  pcap file. If this value is 0, the whole packet is read;
	 *  if it is bigger than packet size, the generated packets
	 *  will contain the whole packet */
	uint32_t n_bytes_per_pkt;
};

/** source port operations */
extern struct rte_port_in_ops rte_port_source_ops;

/** sink port parameters */
struct rte_port_sink_params {
	/** The full path of the pcap file to write the packets to */
	const char *file_name;
	/** The maximum number of packets write to the pcap file.
	 *  If this value is 0, the "infinite" write will be carried
	 *  out.
	 */
	uint32_t max_n_pkts;
};

/** sink port operations */
extern struct rte_port_out_ops rte_port_sink_ops;

#ifdef __cplusplus
}
#endif

#endif
