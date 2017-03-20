/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#ifndef _RTE_DIST_PRIV_H_
#define _RTE_DIST_PRIV_H_

/**
 * @file
 * RTE distributor
 *
 * The distributor is a component which is designed to pass packets
 * one-at-a-time to workers, with dynamic load balancing.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NO_FLAGS 0
#define RTE_DISTRIB_PREFIX "DT_"

/*
 * We will use the bottom four bits of pointer for flags, shifting out
 * the top four bits to make room (since a 64-bit pointer actually only uses
 * 48 bits). An arithmetic-right-shift will then appropriately restore the
 * original pointer value with proper sign extension into the top bits.
 */
#define RTE_DISTRIB_FLAG_BITS 4
#define RTE_DISTRIB_FLAGS_MASK (0x0F)
#define RTE_DISTRIB_NO_BUF 0       /**< empty flags: no buffer requested */
#define RTE_DISTRIB_GET_BUF (1)    /**< worker requests a buffer, returns old */
#define RTE_DISTRIB_RETURN_BUF (2) /**< worker returns a buffer, no request */
#define RTE_DISTRIB_VALID_BUF (4)  /**< set if bufptr contains ptr */

#define RTE_DISTRIB_BACKLOG_SIZE 8
#define RTE_DISTRIB_BACKLOG_MASK (RTE_DISTRIB_BACKLOG_SIZE - 1)

#define RTE_DISTRIB_MAX_RETURNS 128
#define RTE_DISTRIB_RETURNS_MASK (RTE_DISTRIB_MAX_RETURNS - 1)

/**
 * Maximum number of workers allowed.
 * Be aware of increasing the limit, becaus it is limited by how we track
 * in-flight tags. See in_flight_bitmask and rte_distributor_process
 */
#define RTE_DISTRIB_MAX_WORKERS 64

#define RTE_DISTRIBUTOR_NAMESIZE 32 /**< Length of name for instance */

/**
 * Buffer structure used to pass the pointer data between cores. This is cache
 * line aligned, but to improve performance and prevent adjacent cache-line
 * prefetches of buffers for other workers, e.g. when worker 1's buffer is on
 * the next cache line to worker 0, we pad this out to three cache lines.
 * Only 64-bits of the memory is actually used though.
 */
union rte_distributor_buffer {
	volatile int64_t bufptr64;
	char pad[RTE_CACHE_LINE_SIZE*3];
} __rte_cache_aligned;

/*
 * Transfer up to 8 mbufs at a time to/from workers, and
 * flow matching algorithm optimised for 8 flow IDs at a time
 */
#define RTE_DIST_BURST_SIZE 8

struct rte_distributor_backlog {
	unsigned int start;
	unsigned int count;
	int64_t pkts[RTE_DIST_BURST_SIZE] __rte_cache_aligned;
	uint16_t *tags; /* will point to second cacheline of inflights */
} __rte_cache_aligned;


struct rte_distributor_returned_pkts {
	unsigned int start;
	unsigned int count;
	struct rte_mbuf *mbufs[RTE_DISTRIB_MAX_RETURNS];
};

struct rte_distributor {
	TAILQ_ENTRY(rte_distributor) next;    /**< Next in list. */

	char name[RTE_DISTRIBUTOR_NAMESIZE];  /**< Name of the ring. */
	unsigned int num_workers;             /**< Number of workers polling */

	uint32_t in_flight_tags[RTE_DISTRIB_MAX_WORKERS];
		/**< Tracks the tag being processed per core */
	uint64_t in_flight_bitmask;
		/**< on/off bits for in-flight tags.
		 * Note that if RTE_DISTRIB_MAX_WORKERS is larger than 64 then
		 * the bitmask has to expand.
		 */

	struct rte_distributor_backlog backlog[RTE_DISTRIB_MAX_WORKERS];

	union rte_distributor_buffer bufs[RTE_DISTRIB_MAX_WORKERS];

	struct rte_distributor_returned_pkts returns;
};

#ifdef __cplusplus
}
#endif

#endif
