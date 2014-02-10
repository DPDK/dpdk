/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef _RTE_ETH_PCAP_H_
#define _RTE_ETH_PCAP_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pcap/pcap.h>

#define RTE_ETH_PCAP_PARAM_NAME "eth_pcap"

int rte_eth_from_pcaps(pcap_t * const rx_queues[],
		const unsigned nb_rx_queues,
		pcap_t * const tx_queues[],
		const unsigned nb_tx_queues,
		const unsigned numa_node);

int rte_eth_from_pcaps_n_dumpers(pcap_t * const rx_queues[],
		const unsigned nb_rx_queues,
		pcap_dumper_t * const tx_queues[],
		const unsigned nb_tx_queues,
		const unsigned numa_node);

/**
 * For use by the EAL only. Called as part of EAL init to set up any dummy NICs
 * configured on command line.
 */
int rte_pmd_pcap_init(const char *name, const char *params);

#ifdef __cplusplus
}
#endif

#endif
