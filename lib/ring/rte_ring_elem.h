/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019 Arm Limited
 * Copyright (c) 2010-2017 Intel Corporation
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_ELEM_H_
#define _RTE_RING_ELEM_H_

/**
 * @file
 * RTE Ring with user defined element size
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ring_core.h>
#include <rte_ring_elem_pvt.h>

/**
 * Calculate the memory size needed for a ring with given element size
 *
 * This function returns the number of bytes needed for a ring, given
 * the number of elements in it and the size of the element. This value
 * is the sum of the size of the structure rte_ring and the size of the
 * memory needed for storing the elements. The value is aligned to a cache
 * line size.
 *
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 * @param count
 *   The number of elements in the ring (must be a power of 2).
 * @return
 *   - The memory size needed for the ring on success.
 *   - -EINVAL - esize is not a multiple of 4 or count provided is not a
 *		 power of 2.
 */
ssize_t rte_ring_get_memsize_elem(unsigned int esize, unsigned int count);

/**
 * Create a new ring named *name* that stores elements with given size.
 *
 * This function uses ``memzone_reserve()`` to allocate memory. Then it
 * calls rte_ring_init() to initialize an empty ring.
 *
 * The new ring size is set to *count*, which must be a power of
 * two. Water marking is disabled by default. The real usable ring size
 * is *count-1* instead of *count* to differentiate a full ring from an
 * empty ring.
 *
 * The ring is added in RTE_TAILQ_RING list.
 *
 * @param name
 *   The name of the ring.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 * @param count
 *   The number of elements in the ring (must be a power of 2).
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   An OR of the following:
 *   - One of mutually exclusive flags that define producer behavior:
 *      - RING_F_SP_ENQ: If this flag is set, the default behavior when
 *        using ``rte_ring_enqueue()`` or ``rte_ring_enqueue_bulk()``
 *        is "single-producer".
 *      - RING_F_MP_RTS_ENQ: If this flag is set, the default behavior when
 *        using ``rte_ring_enqueue()`` or ``rte_ring_enqueue_bulk()``
 *        is "multi-producer RTS mode".
 *      - RING_F_MP_HTS_ENQ: If this flag is set, the default behavior when
 *        using ``rte_ring_enqueue()`` or ``rte_ring_enqueue_bulk()``
 *        is "multi-producer HTS mode".
 *     If none of these flags is set, then default "multi-producer"
 *     behavior is selected.
 *   - One of mutually exclusive flags that define consumer behavior:
 *      - RING_F_SC_DEQ: If this flag is set, the default behavior when
 *        using ``rte_ring_dequeue()`` or ``rte_ring_dequeue_bulk()``
 *        is "single-consumer". Otherwise, it is "multi-consumers".
 *      - RING_F_MC_RTS_DEQ: If this flag is set, the default behavior when
 *        using ``rte_ring_dequeue()`` or ``rte_ring_dequeue_bulk()``
 *        is "multi-consumer RTS mode".
 *      - RING_F_MC_HTS_DEQ: If this flag is set, the default behavior when
 *        using ``rte_ring_dequeue()`` or ``rte_ring_dequeue_bulk()``
 *        is "multi-consumer HTS mode".
 *     If none of these flags is set, then default "multi-consumer"
 *     behavior is selected.
 * @return
 *   On success, the pointer to the new allocated ring. NULL on error with
 *    rte_errno set appropriately. Possible errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - EINVAL - esize is not a multiple of 4 or count provided is not a
 *		 power of 2.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_ring *rte_ring_create_elem(const char *name, unsigned int esize,
			unsigned int count, int socket_id, unsigned int flags);

/**
 * Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_mp_enqueue_bulk_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __rte_ring_do_enqueue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_FIXED, RTE_RING_SYNC_MT, free_space);
}

/**
 * Enqueue several objects on a ring
 *
 * @warning This API is NOT multi-producers safe
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_sp_enqueue_bulk_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __rte_ring_do_enqueue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_FIXED, RTE_RING_SYNC_ST, free_space);
}

#include <rte_ring_hts.h>
#include <rte_ring_rts.h>

/**
 * Enqueue several objects on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_enqueue_bulk_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	switch (r->prod.sync_type) {
	case RTE_RING_SYNC_MT:
		return rte_ring_mp_enqueue_bulk_elem(r, obj_table, esize, n,
			free_space);
	case RTE_RING_SYNC_ST:
		return rte_ring_sp_enqueue_bulk_elem(r, obj_table, esize, n,
			free_space);
	case RTE_RING_SYNC_MT_RTS:
		return rte_ring_mp_rts_enqueue_bulk_elem(r, obj_table, esize, n,
			free_space);
	case RTE_RING_SYNC_MT_HTS:
		return rte_ring_mp_hts_enqueue_bulk_elem(r, obj_table, esize, n,
			free_space);
	}

	/* valid ring should never reach this point */
	RTE_ASSERT(0);
	if (free_space != NULL)
		*free_space = 0;
	return 0;
}

/**
 * Enqueue one object on a ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_mp_enqueue_elem(struct rte_ring *r, void *obj, unsigned int esize)
{
	return rte_ring_mp_enqueue_bulk_elem(r, obj, esize, 1, NULL) ? 0 :
								-ENOBUFS;
}

/**
 * Enqueue one object on a ring
 *
 * @warning This API is NOT multi-producers safe
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_sp_enqueue_elem(struct rte_ring *r, void *obj, unsigned int esize)
{
	return rte_ring_sp_enqueue_bulk_elem(r, obj, esize, 1, NULL) ? 0 :
								-ENOBUFS;
}

/**
 * Enqueue one object on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_enqueue_elem(struct rte_ring *r, void *obj, unsigned int esize)
{
	return rte_ring_enqueue_bulk_elem(r, obj, esize, 1, NULL) ? 0 :
								-ENOBUFS;
}

/**
 * Dequeue several objects from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_mc_dequeue_bulk_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	return __rte_ring_do_dequeue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_FIXED, RTE_RING_SYNC_MT, available);
}

/**
 * Dequeue several objects from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table,
 *   must be strictly positive.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_sc_dequeue_bulk_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	return __rte_ring_do_dequeue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_FIXED, RTE_RING_SYNC_ST, available);
}

/**
 * Dequeue several objects from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_dequeue_bulk_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	switch (r->cons.sync_type) {
	case RTE_RING_SYNC_MT:
		return rte_ring_mc_dequeue_bulk_elem(r, obj_table, esize, n,
			available);
	case RTE_RING_SYNC_ST:
		return rte_ring_sc_dequeue_bulk_elem(r, obj_table, esize, n,
			available);
	case RTE_RING_SYNC_MT_RTS:
		return rte_ring_mc_rts_dequeue_bulk_elem(r, obj_table, esize,
			n, available);
	case RTE_RING_SYNC_MT_HTS:
		return rte_ring_mc_hts_dequeue_bulk_elem(r, obj_table, esize,
			n, available);
	}

	/* valid ring should never reach this point */
	RTE_ASSERT(0);
	if (available != NULL)
		*available = 0;
	return 0;
}

/**
 * Dequeue one object from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to the object that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_mc_dequeue_elem(struct rte_ring *r, void *obj_p,
				unsigned int esize)
{
	return rte_ring_mc_dequeue_bulk_elem(r, obj_p, esize, 1, NULL)  ? 0 :
								-ENOENT;
}

/**
 * Dequeue one object from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to the object that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_sc_dequeue_elem(struct rte_ring *r, void *obj_p,
				unsigned int esize)
{
	return rte_ring_sc_dequeue_bulk_elem(r, obj_p, esize, 1, NULL) ? 0 :
								-ENOENT;
}

/**
 * Dequeue one object from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to the object that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @return
 *   - 0: Success, objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_dequeue_elem(struct rte_ring *r, void *obj_p, unsigned int esize)
{
	return rte_ring_dequeue_bulk_elem(r, obj_p, esize, 1, NULL) ? 0 :
								-ENOENT;
}

/**
 * Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static __rte_always_inline unsigned int
rte_ring_mp_enqueue_burst_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __rte_ring_do_enqueue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_VARIABLE, RTE_RING_SYNC_MT, free_space);
}

/**
 * Enqueue several objects on a ring
 *
 * @warning This API is NOT multi-producers safe
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static __rte_always_inline unsigned int
rte_ring_sp_enqueue_burst_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	return __rte_ring_do_enqueue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_VARIABLE, RTE_RING_SYNC_ST, free_space);
}

/**
 * Enqueue several objects on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static __rte_always_inline unsigned int
rte_ring_enqueue_burst_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *free_space)
{
	switch (r->prod.sync_type) {
	case RTE_RING_SYNC_MT:
		return rte_ring_mp_enqueue_burst_elem(r, obj_table, esize, n,
			free_space);
	case RTE_RING_SYNC_ST:
		return rte_ring_sp_enqueue_burst_elem(r, obj_table, esize, n,
			free_space);
	case RTE_RING_SYNC_MT_RTS:
		return rte_ring_mp_rts_enqueue_burst_elem(r, obj_table, esize,
			n, free_space);
	case RTE_RING_SYNC_MT_HTS:
		return rte_ring_mp_hts_enqueue_burst_elem(r, obj_table, esize,
			n, free_space);
	}

	/* valid ring should never reach this point */
	RTE_ASSERT(0);
	if (free_space != NULL)
		*free_space = 0;
	return 0;
}

/**
 * Dequeue several objects from a ring (multi-consumers safe). When the request
 * objects are more than the available objects, only dequeue the actual number
 * of objects
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static __rte_always_inline unsigned int
rte_ring_mc_dequeue_burst_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	return __rte_ring_do_dequeue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_VARIABLE, RTE_RING_SYNC_MT, available);
}

/**
 * Dequeue several objects from a ring (NOT multi-consumers safe).When the
 * request objects are more than the available objects, only dequeue the
 * actual number of objects
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static __rte_always_inline unsigned int
rte_ring_sc_dequeue_burst_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	return __rte_ring_do_dequeue_elem(r, obj_table, esize, n,
			RTE_RING_QUEUE_VARIABLE, RTE_RING_SYNC_ST, available);
}

/**
 * Dequeue multiple objects from a ring up to a maximum number.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects that will be filled.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   - Number of objects dequeued
 */
static __rte_always_inline unsigned int
rte_ring_dequeue_burst_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n, unsigned int *available)
{
	switch (r->cons.sync_type) {
	case RTE_RING_SYNC_MT:
		return rte_ring_mc_dequeue_burst_elem(r, obj_table, esize, n,
			available);
	case RTE_RING_SYNC_ST:
		return rte_ring_sc_dequeue_burst_elem(r, obj_table, esize, n,
			available);
	case RTE_RING_SYNC_MT_RTS:
		return rte_ring_mc_rts_dequeue_burst_elem(r, obj_table, esize,
			n, available);
	case RTE_RING_SYNC_MT_HTS:
		return rte_ring_mc_hts_dequeue_burst_elem(r, obj_table, esize,
			n, available);
	}

	/* valid ring should never reach this point */
	RTE_ASSERT(0);
	if (available != NULL)
		*available = 0;
	return 0;
}

#include <rte_ring_peek.h>
#include <rte_ring_peek_zc.h>

#include <rte_ring.h>

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RING_ELEM_H_ */
