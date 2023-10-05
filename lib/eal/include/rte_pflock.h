/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Derived from Concurrency Kit
 * Copyright 2011-2015 Samy Al Bahra.
 */

#ifndef _RTE_PFLOCK_H_
#define _RTE_PFLOCK_H_

/**
 * @file
 *
 * Phase-fair locks
 *
 * This file defines an API for phase-fair reader writer locks,
 * which is a variant of typical reader-writer locks that prevent
 * starvation. In this type of lock, readers and writers alternate.
 * This significantly reduces the worst-case blocking for readers and writers.
 *
 * This is an implementation derived from FreeBSD
 * based on the work described in:
 *    Brandenburg, B. and Anderson, J. 2010. Spin-Based
 *    Reader-Writer Synchronization for Multiprocessor Real-Time Systems
 *
 * All locks must be initialised before use, and only initialised once.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_pause.h>
#include <rte_stdatomic.h>

/**
 * The rte_pflock_t type.
 */
struct rte_pflock {
	struct {
		RTE_ATOMIC(uint16_t) in;
		RTE_ATOMIC(uint16_t) out;
	} rd, wr;
};
typedef struct rte_pflock rte_pflock_t;

/*
 * Allocation of bits to reader
 *
 * 15                 4 3 2 1 0
 * +-------------------+---+-+-+
 * | rin: reads issued |x|x| | |
 * +-------------------+---+-+-+
 *                          ^ ^
 *                          | |
 * PRES: writer present ----/ |
 * PHID: writer phase id -----/
 *
 * 15                4 3 2 1 0
 * +------------------+------+
 * |rout:read complete|unused|
 * +------------------+------+
 *
 * The maximum number of readers is 4095
 */

/* Constants used to map the bits in reader counter */
#define RTE_PFLOCK_WBITS 0x3	/* Writer bits in reader. */
#define RTE_PFLOCK_PRES  0x2	/* Writer present bit. */
#define RTE_PFLOCK_PHID  0x1	/* Phase ID bit. */
#define RTE_PFLOCK_LSB   0xFFF0 /* reader bits. */
#define RTE_PFLOCK_RINC  0x10	/* Reader increment. */

/**
 * A static pflock initializer.
 */
#define RTE_PFLOCK_INITIALIZER {  }

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize the pflock to an unlocked state.
 *
 * @param pf
 *   A pointer to the pflock.
 */
__rte_experimental
static inline void
rte_pflock_init(struct rte_pflock *pf)
{
	pf->rd.in = 0;
	pf->rd.out = 0;
	pf->wr.in = 0;
	pf->wr.out = 0;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Take a pflock for read.
 *
 * @param pf
 *   A pointer to a pflock structure.
 */
__rte_experimental
static inline void
rte_pflock_read_lock(rte_pflock_t *pf)
{
	uint16_t w;

	/*
	 * If no writer is present, then the operation has completed
	 * successfully.
	 */
	w = rte_atomic_fetch_add_explicit(&pf->rd.in, RTE_PFLOCK_RINC, rte_memory_order_acquire)
		& RTE_PFLOCK_WBITS;
	if (w == 0)
		return;

	/* Wait for current write phase to complete. */
	RTE_WAIT_UNTIL_MASKED(&pf->rd.in, RTE_PFLOCK_WBITS, !=, w, rte_memory_order_acquire);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Release a pflock locked for reading.
 *
 * @param pf
 *   A pointer to the pflock structure.
 */
__rte_experimental
static inline void
rte_pflock_read_unlock(rte_pflock_t *pf)
{
	rte_atomic_fetch_add_explicit(&pf->rd.out, RTE_PFLOCK_RINC, rte_memory_order_release);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Take the pflock for write.
 *
 * @param pf
 *   A pointer to the pflock structure.
 */
__rte_experimental
static inline void
rte_pflock_write_lock(rte_pflock_t *pf)
{
	uint16_t ticket, w;

	/* Acquire ownership of write-phase.
	 * This is same as rte_ticketlock_lock().
	 */
	ticket = rte_atomic_fetch_add_explicit(&pf->wr.in, 1, rte_memory_order_relaxed);
	rte_wait_until_equal_16((uint16_t *)(uintptr_t)&pf->wr.out, ticket,
		rte_memory_order_acquire);

	/*
	 * Acquire ticket on read-side in order to allow them
	 * to flush. Indicates to any incoming reader that a
	 * write-phase is pending.
	 *
	 * The load of rd.out in wait loop could be executed
	 * speculatively.
	 */
	w = RTE_PFLOCK_PRES | (ticket & RTE_PFLOCK_PHID);
	ticket = rte_atomic_fetch_add_explicit(&pf->rd.in, w, rte_memory_order_relaxed);

	/* Wait for any pending readers to flush. */
	rte_wait_until_equal_16((uint16_t *)(uintptr_t)&pf->rd.out, ticket,
		rte_memory_order_acquire);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Release a pflock held for writing.
 *
 * @param pf
 *   A pointer to a pflock structure.
 */
__rte_experimental
static inline void
rte_pflock_write_unlock(rte_pflock_t *pf)
{
	/* Migrate from write phase to read phase. */
	rte_atomic_fetch_and_explicit(&pf->rd.in, RTE_PFLOCK_LSB, rte_memory_order_release);

	/* Allow other writers to continue. */
	rte_atomic_fetch_add_explicit(&pf->wr.out, 1, rte_memory_order_release);
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PFLOCK_H */
