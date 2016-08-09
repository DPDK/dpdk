/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
