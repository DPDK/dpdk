/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _DIST_PRIV_H_
#define _DIST_PRIV_H_

/**
 * @file
 * RTE distributor
 *
 * The distributor is a component which is designed to pass packets
 * one-at-a-time to workers, with dynamic load balancing.
 */

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
 * Be aware of increasing the limit, because it is limited by how we track
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
union rte_distributor_buffer_single {
	volatile RTE_ATOMIC(int64_t) bufptr64;
	char pad[RTE_CACHE_LINE_SIZE*3];
} __rte_cache_aligned;

/*
 * Transfer up to 8 mbufs at a time to/from workers, and
 * flow matching algorithm optimized for 8 flow IDs at a time
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

struct rte_distributor_single {
	TAILQ_ENTRY(rte_distributor_single) next;    /**< Next in list. */

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

	union rte_distributor_buffer_single bufs[RTE_DISTRIB_MAX_WORKERS];

	struct rte_distributor_returned_pkts returns;
};

/* All different signature compare functions */
enum rte_distributor_match_function {
	RTE_DIST_MATCH_SCALAR = 0,
	RTE_DIST_MATCH_VECTOR,
	RTE_DIST_NUM_MATCH_FNS
};

/**
 * Buffer structure used to pass the pointer data between cores. This is cache
 * line aligned, but to improve performance and prevent adjacent cache-line
 * prefetches of buffers for other workers, e.g. when worker 1's buffer is on
 * the next cache line to worker 0, we pad this out to two cache lines.
 * We can pass up to 8 mbufs at a time in one cacheline.
 * There is a separate cacheline for returns in the burst API.
 */
struct rte_distributor_buffer {
	volatile RTE_ATOMIC(int64_t) bufptr64[RTE_DIST_BURST_SIZE]
		__rte_cache_aligned; /* <= outgoing to worker */

	int64_t pad1 __rte_cache_aligned;    /* <= one cache line  */

	volatile RTE_ATOMIC(int64_t) retptr64[RTE_DIST_BURST_SIZE]
		__rte_cache_aligned; /* <= incoming from worker */

	int64_t pad2 __rte_cache_aligned;    /* <= one cache line  */

	int count __rte_cache_aligned;       /* <= number of current mbufs */
};

struct rte_distributor {
	TAILQ_ENTRY(rte_distributor) next;    /**< Next in list. */

	char name[RTE_DISTRIBUTOR_NAMESIZE];  /**< Name of the ring. */
	unsigned int num_workers;             /**< Number of workers polling */
	unsigned int alg_type;                /**< Number of alg types */

	/**>
	 * First cache line in the this array are the tags inflight
	 * on the worker core. Second cache line are the backlog
	 * that are going to go to the worker core.
	 */
	uint16_t in_flight_tags[RTE_DISTRIB_MAX_WORKERS][RTE_DIST_BURST_SIZE*2]
			__rte_cache_aligned;

	struct rte_distributor_backlog backlog[RTE_DISTRIB_MAX_WORKERS]
			__rte_cache_aligned;

	struct rte_distributor_buffer bufs[RTE_DISTRIB_MAX_WORKERS];

	struct rte_distributor_returned_pkts returns;

	enum rte_distributor_match_function dist_match_fn;

	struct rte_distributor_single *d_single;

	uint8_t active[RTE_DISTRIB_MAX_WORKERS];
	uint8_t activesum;
};

void
find_match_scalar(struct rte_distributor *d,
			uint16_t *data_ptr,
			uint16_t *output_ptr);

void
find_match_vec(struct rte_distributor *d,
			uint16_t *data_ptr,
			uint16_t *output_ptr);

#endif /* _DIST_PRIV_H_ */
