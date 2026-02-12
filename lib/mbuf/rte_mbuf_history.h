/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 NVIDIA Corporation & Affiliates
 */

#ifndef _RTE_MBUF_HISTORY_H_
#define _RTE_MBUF_HISTORY_H_

/**
 * @file
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * These functions allow to track history of mbuf objects using a dynamic field.
 *
 * It tracks the lifecycle of mbuf objects through the system
 * with a fixed set of predefined events to maintain performance.
 *
 * The history is stored as an atomic value (64-bit) in a dynamic field of the mbuf,
 * with each event encoded in 4 bits, allowing up to 16 events to be tracked.
 * Atomic operations ensure thread safety for cloned mbufs accessed by multiple lcores.
 *
 * After dumping the history in a file,
 * the script dpdk-mbuf-history-parser.py can be used for parsing.
 */

#include <rte_common.h>
#include <rte_debug.h>

#include <rte_mbuf_dyn.h>

/* Forward declaration to avoid circular dependency. */
struct rte_mbuf;
struct rte_mempool;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Number of bits for each history operation.
 */
#define RTE_MBUF_HISTORY_BITS 4

/**
 * Maximum number of history operations that can be stored.
 */
#define RTE_MBUF_HISTORY_MAX (sizeof(rte_mbuf_history_t) * 8 / RTE_MBUF_HISTORY_BITS)

/**
 * History operation types.
 */
enum rte_mbuf_history_op {
	RTE_MBUF_HISTORY_OP_NEVER     =  0, /**< Initial state - never allocated */
	RTE_MBUF_HISTORY_OP_LIB_FREE  =  1, /**< Freed back to pool */
	RTE_MBUF_HISTORY_OP_PMD_FREE  =  2, /**< Freed by PMD */
	RTE_MBUF_HISTORY_OP_APP_FREE  =  3, /**< Freed by application */
	/* With ops below, mbuf is considered allocated. */
	RTE_MBUF_HISTORY_OP_LIB_ALLOC =  4, /**< Allocation in mbuf library */
	RTE_MBUF_HISTORY_OP_PMD_ALLOC =  5, /**< Allocated by PMD for Rx */
	RTE_MBUF_HISTORY_OP_APP_ALLOC =  6, /**< Allocated by application */
	RTE_MBUF_HISTORY_OP_RX        =  7, /**< Received */
	RTE_MBUF_HISTORY_OP_TX        =  8, /**< Sent */
	RTE_MBUF_HISTORY_OP_TX_PREP   =  9, /**< Being prepared before Tx */
	RTE_MBUF_HISTORY_OP_TX_BUSY   = 10, /**< Returned due to Tx busy */
	RTE_MBUF_HISTORY_OP_ENQUEUE   = 11, /**< Enqueued for processing */
	RTE_MBUF_HISTORY_OP_DEQUEUE   = 12, /**< Dequeued for processing */
	/*                              13,      reserved for future */
	RTE_MBUF_HISTORY_OP_USR2      = 14, /**< Application-defined event 2 */
	RTE_MBUF_HISTORY_OP_USR1      = 15, /**< Application-defined event 1 */
	RTE_MBUF_HISTORY_OP_MAX       = 16, /**< Maximum number of operation types */
};

/**
 * Global offset for the history dynamic field (set during initialization).
 */
extern int rte_mbuf_history_field_offset;

/**
 * Initialize the mbuf history system.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * This function registers the dynamic field for mbuf history tracking.
 * It should be called once during application initialization.
 *
 * Note: This function is called by rte_pktmbuf_pool_create,
 * so explicit invocation is usually not required.
 */
__rte_experimental
void rte_mbuf_history_init(void);

/**
 * Mark an mbuf with a history event.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param m
 *   Pointer to the mbuf.
 * @param op
 *   The operation to record.
 */
static inline void rte_mbuf_history_mark(struct rte_mbuf *m, enum rte_mbuf_history_op op)
{
#ifndef RTE_MBUF_HISTORY_DEBUG
	RTE_SET_USED(m);
	RTE_SET_USED(op);
#else
	RTE_ASSERT(rte_mbuf_history_field_offset >= 0);
	RTE_ASSERT(op < RTE_MBUF_HISTORY_OP_MAX);
	if (unlikely(m == NULL))
		return;

	rte_mbuf_history_t *history_field = RTE_MBUF_DYNFIELD(m,
			rte_mbuf_history_field_offset, rte_mbuf_history_t *);
	uint64_t old_history = rte_atomic_load_explicit(history_field,
			rte_memory_order_acquire);
	uint64_t new_history;
	do {
		new_history = (old_history << RTE_MBUF_HISTORY_BITS) | op;
	} while (unlikely(!rte_atomic_compare_exchange_weak_explicit(history_field,
			&old_history, new_history,
			rte_memory_order_release, rte_memory_order_acquire)));
#endif
}

/**
 * Mark multiple mbufs with a history event.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param mbufs
 *   Array of mbuf pointers.
 * @param count
 *   Number of mbufs to mark.
 * @param op
 *   The operation to record.
 */
static inline void rte_mbuf_history_mark_bulk(struct rte_mbuf * const *mbufs,
		unsigned int count, enum rte_mbuf_history_op op)
{
#ifndef RTE_MBUF_HISTORY_DEBUG
	RTE_SET_USED(mbufs);
	RTE_SET_USED(count);
	RTE_SET_USED(op);
#else
	RTE_ASSERT(rte_mbuf_history_field_offset >= 0);
	RTE_ASSERT(op < RTE_MBUF_HISTORY_OP_MAX);
	if (unlikely(mbufs == NULL))
		return;

	while (count--)
		rte_mbuf_history_mark(*mbufs++, op);
#endif
}

/**
 * Dump mbuf history for a single mbuf to a file.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param f
 *   File pointer to write the history to.
 * @param m
 *   Pointer to the mbuf to dump history for.
 */
__rte_experimental
void rte_mbuf_history_dump(FILE *f, const struct rte_mbuf *m);

/**
 * Dump mbuf history statistics for a single mempool to a file.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param f
 *   File pointer to write the history statistics to.
 * @param mp
 *   Pointer to the mempool to dump history for.
 */
__rte_experimental
void rte_mbuf_history_dump_mempool(FILE *f, struct rte_mempool *mp);

/**
 * Dump mbuf history statistics for all mempools to a file.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param f
 *   File pointer to write the history statistics to.
 */
__rte_experimental
void rte_mbuf_history_dump_all(FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_HISTORY_H_ */
