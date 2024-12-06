/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
 */

#ifndef _SORING_H_
#define _SORING_H_

/**
 * @file
 * This file contains internal structures of DPDK soring: Staged Ordered Ring.
 * Sort of extension of conventional DPDK ring.
 * Internal structure:
 * In addition to 'normal' ring of elems, it also has a ring of states of the
 * same size. Each state[] corresponds to exactly one elem[].
 * state[] will be used by acquire/release/dequeue functions to store internal
 * information and should not be accessed by the user directly.
 * For actual implementation details, please refer to soring.c
 */

#include <rte_soring.h>

/* logging stuff, register our own tag for SORING */
#include <rte_log.h>

extern int soring_logtype;
#define RTE_LOGTYPE_SORING soring_logtype
#define SORING_LOG(level, ...) \
	RTE_LOG_LINE(level, SORING, "" __VA_ARGS__)

/**
 * SORING internal state for each element
 */
union soring_state {
	alignas(sizeof(uint64_t)) RTE_ATOMIC(uint64_t) raw;
	struct {
		RTE_ATOMIC(uint32_t) ftoken;
		RTE_ATOMIC(uint32_t) stnum;
	};
};

/* upper 2 bits are used for status */
#define SORING_ST_START		RTE_SHIFT_VAL32(1, RTE_SORING_ST_BIT)
#define SORING_ST_FINISH	RTE_SHIFT_VAL32(2, RTE_SORING_ST_BIT)

#define SORING_ST_MASK	\
	RTE_GENMASK32(sizeof(uint32_t) * CHAR_BIT - 1, RTE_SORING_ST_BIT)

#define SORING_FTKN_MAKE(pos, stg)	((pos) + (stg))
#define SORING_FTKN_POS(ftk, stg)	((ftk) - (stg))

/**
 * SORING re-uses rte_ring internal structures and implementation
 * for enqueue/dequeue operations.
 */
struct __rte_ring_headtail {

	union __rte_cache_aligned {
		struct rte_ring_headtail ht;
		struct rte_ring_hts_headtail hts;
		struct rte_ring_rts_headtail rts;
	};

	RTE_CACHE_GUARD;
};

union soring_stage_tail {
	/** raw 8B value to read/write *sync* and *pos* as one atomic op */
	alignas(sizeof(uint64_t)) RTE_ATOMIC(uint64_t) raw;
	struct {
		RTE_ATOMIC(uint32_t) sync;
		RTE_ATOMIC(uint32_t) pos;
	};
};

struct soring_stage_headtail {
	volatile union soring_stage_tail tail;
	enum rte_ring_sync_type unused;  /**< unused */
	volatile RTE_ATOMIC(uint32_t) head;
};

/**
 * SORING stage head_tail structure is 'compatible' with rte_ring ones.
 * rte_ring internal functions for moving producer/consumer head
 * can work transparently with stage head_tail and visa-versa
 * (no modifications required).
 */
struct soring_stage {

	union __rte_cache_aligned {
		struct rte_ring_headtail ht;
		struct soring_stage_headtail sht;
	};

	RTE_CACHE_GUARD;
};

/**
 * soring internal structure.
 * As with rte_ring actual elements array supposed to be located directly
 * after the rte_soring structure.
 */
struct  __rte_cache_aligned rte_soring {
	uint32_t size;           /**< Size of ring. */
	uint32_t mask;           /**< Mask (size-1) of ring. */
	uint32_t capacity;       /**< Usable size of ring */
	uint32_t esize;
	/**< size of elements in the ring, must be a multiple of 4*/
	uint32_t msize;
	/**< size of metadata value for each elem, must be a multiple of 4 */

	/** Ring stages */
	struct soring_stage *stage;
	uint32_t nb_stage;

	/** Ring of states (one per element) */
	union soring_state *state;

	/** Pointer to the buffer where metadata values for each elements
	 * are stored. This is supplementary and optional information that
	 * user can attach to each element of the ring.
	 * While it is possible to incorporate this information inside
	 * user-defined element, in many cases it is plausible to maintain it
	 * as a separate array (mainly for performance reasons).
	 */
	void *meta;

	RTE_CACHE_GUARD;

	/** Ring producer status. */
	struct __rte_ring_headtail prod;

	/** Ring consumer status. */
	struct __rte_ring_headtail cons;

	char name[RTE_RING_NAMESIZE];  /**< Name of the ring. */
};

#endif /* _SORING_H_ */
