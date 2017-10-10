/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include "rte_eth_softnic.h"

#ifndef INTRUSIVE
#define INTRUSIVE					0
#endif

struct pmd_params {
	/** Parameters for the soft device (to be created) */
	struct {
		const char *name; /**< Name */
		uint32_t flags; /**< Flags */

		/** 0 = Access hard device though API only (potentially slower,
		 *      but safer);
		 *  1 = Access hard device private data structures is allowed
		 *      (potentially faster).
		 */
		int intrusive;
	} soft;

	/** Parameters for the hard device (existing) */
	struct {
		char *name; /**< Name */
		uint16_t tx_queue_id; /**< TX queue ID */
	} hard;
};

/**
 * Default Internals
 */

#ifndef DEFAULT_BURST_SIZE
#define DEFAULT_BURST_SIZE				32
#endif

#ifndef FLUSH_COUNT_THRESHOLD
#define FLUSH_COUNT_THRESHOLD			(1 << 17)
#endif

struct default_internals {
	struct rte_mbuf **pkts;
	uint32_t pkts_len;
	uint32_t txq_pos;
	uint32_t flush_count;
};

/**
 * PMD Internals
 */
struct pmd_internals {
	/** Params */
	struct pmd_params params;

	/** Soft device */
	struct {
		struct default_internals def; /**< Default */
	} soft;

	/** Hard device */
	struct {
		uint16_t port_id;
	} hard;
};

struct pmd_rx_queue {
	/** Hard device */
	struct {
		uint16_t port_id;
		uint16_t rx_queue_id;
	} hard;
};

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_INTERNALS_H__ */
