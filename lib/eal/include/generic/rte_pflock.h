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

#include <rte_common.h>
#include <rte_pause.h>

/**
 * The rte_pflock_t type.
 */
struct rte_pflock {
	struct {
		uint16_t in;
		uint16_t out;
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
	w = __atomic_fetch_add(&pf->rd.in, RTE_PFLOCK_RINC, __ATOMIC_ACQUIRE)
		& RTE_PFLOCK_WBITS;
	if (w == 0)
		return;

	/* Wait for current write phase to complete. */
	RTE_WAIT_UNTIL_MASKED(&pf->rd.in, RTE_PFLOCK_WBITS, !=, w,
		__ATOMIC_ACQUIRE);
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
	__atomic_fetch_add(&pf->rd.out, RTE_PFLOCK_RINC, __ATOMIC_RELEASE);
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
	ticket = __atomic_fetch_add(&pf->wr.in, 1, __ATOMIC_RELAXED);
	rte_wait_until_equal_16(&pf->wr.out, ticket, __ATOMIC_ACQUIRE);

	/*
	 * Acquire ticket on read-side in order to allow them
	 * to flush. Indicates to any incoming reader that a
	 * write-phase is pending.
	 *
	 * The load of rd.out in wait loop could be executed
	 * speculatively.
	 */
	w = RTE_PFLOCK_PRES | (ticket & RTE_PFLOCK_PHID);
	ticket = __atomic_fetch_add(&pf->rd.in, w, __ATOMIC_RELAXED);

	/* Wait for any pending readers to flush. */
	rte_wait_until_equal_16(&pf->rd.out, ticket, __ATOMIC_ACQUIRE);
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
	__atomic_fetch_and(&pf->rd.in, RTE_PFLOCK_LSB, __ATOMIC_RELEASE);

	/* Allow other writers to continue. */
	__atomic_fetch_add(&pf->wr.out, 1, __ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PFLOCK_H */
