/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2010-2011 Dmitry Vyukov
 */

#ifndef LTHREAD_POOL_H_
#define LTHREAD_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_malloc.h>
#include <rte_per_lcore.h>
#include <rte_log.h>

#include "lthread_int.h"
#include "lthread_diag.h"

/*
 * This file implements pool of queue nodes used by the queue implemented
 * in lthread_queue.h.
 *
 * The pool is an intrusive lock free MPSC queue.
 *
 * The pool is created empty and populated lazily, i.e. on first attempt to
 * allocate a the pool.
 *
 * Whenever the pool is empty more nodes are added to the pool
 * The number of nodes preallocated in this way is a parameter of
 * _qnode_pool_create. Freeing an object returns it to the pool.
 *
 * Each lthread scheduler maintains its own pool of nodes. L-threads must always
 * allocate from this local pool ( because it is a single consumer queue ).
 * L-threads can free nodes to any pool (because it is a multi producer queue)
 * This enables threads that have affined to a different scheduler to free
 * nodes safely.
 */

struct qnode;
struct qnode_cache;

/*
 * define intermediate node
 */
struct qnode {
	struct qnode *next;
	void *data;
	struct qnode_pool *pool;
} __rte_cache_aligned;

/*
 * a pool structure
 */
struct qnode_pool {
	struct qnode *head;
	struct qnode *stub;
	struct qnode *fast_alloc;
	struct qnode *tail __rte_cache_aligned;
	int pre_alloc;
	char name[LT_MAX_NAME_SIZE];

	DIAG_COUNT_DEFINE(rd);
	DIAG_COUNT_DEFINE(wr);
	DIAG_COUNT_DEFINE(available);
	DIAG_COUNT_DEFINE(prealloc);
	DIAG_COUNT_DEFINE(capacity);
} __rte_cache_aligned;

/*
 * Create a pool of qnodes
 */

static inline struct qnode_pool *
_qnode_pool_create(const char *name, int prealloc_size) {

	struct qnode_pool *p = rte_malloc_socket(NULL,
					sizeof(struct qnode_pool),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());

	RTE_ASSERT(p);

	p->stub = rte_malloc_socket(NULL,
				sizeof(struct qnode),
				RTE_CACHE_LINE_SIZE,
				rte_socket_id());

	RTE_ASSERT(p->stub);

	if (name != NULL)
		strncpy(p->name, name, LT_MAX_NAME_SIZE);
	p->name[sizeof(p->name)-1] = 0;

	p->stub->pool = p;
	p->stub->next = NULL;
	p->tail = p->stub;
	p->head = p->stub;
	p->pre_alloc = prealloc_size;

	DIAG_COUNT_INIT(p, rd);
	DIAG_COUNT_INIT(p, wr);
	DIAG_COUNT_INIT(p, available);
	DIAG_COUNT_INIT(p, prealloc);
	DIAG_COUNT_INIT(p, capacity);

	return p;
}


/*
 * Insert a node into the pool
 */
static __rte_always_inline void
_qnode_pool_insert(struct qnode_pool *p, struct qnode *n)
{
	n->next = NULL;
	struct qnode *prev = n;
	/* We insert at the head */
	prev = (struct qnode *) __sync_lock_test_and_set((uint64_t *)&p->head,
						(uint64_t) prev);
	/* there is a window of inconsistency until prev next is set */
	/* which is why remove must retry */
	prev->next = (n);
}

/*
 * Remove a node from the pool
 *
 * There is a race with _qnode_pool_insert() whereby the queue could appear
 * empty during a concurrent insert, this is handled by retrying
 *
 * The queue uses a stub node, which must be swung as the queue becomes
 * empty, this requires an insert of the stub, which means that removing the
 * last item from the queue incurs the penalty of an atomic exchange. Since the
 * pool is maintained with a bulk pre-allocation the cost of this is amortised.
 */
static __rte_always_inline struct qnode *
_pool_remove(struct qnode_pool *p)
{
	struct qnode *head;
	struct qnode *tail = p->tail;
	struct qnode *next = tail->next;

	/* we remove from the tail */
	if (tail == p->stub) {
		if (next == NULL)
			return NULL;
		/* advance the tail */
		p->tail = next;
		tail = next;
		next = next->next;
	}
	if (likely(next != NULL)) {
		p->tail = next;
		return tail;
	}

	head = p->head;
	if (tail == head)
		return NULL;

	/* swing stub node */
	_qnode_pool_insert(p, p->stub);

	next = tail->next;
	if (next) {
		p->tail = next;
		return tail;
	}
	return NULL;
}


/*
 * This adds a retry to the _pool_remove function
 * defined above
 */
static __rte_always_inline struct qnode *
_qnode_pool_remove(struct qnode_pool *p)
{
	struct qnode *n;

	do {
		n = _pool_remove(p);
		if (likely(n != NULL))
			return n;

		rte_compiler_barrier();
	}  while ((p->head != p->tail) &&
			(p->tail != p->stub));
	return NULL;
}

/*
 * Allocate a node from the pool
 * If the pool is empty add mode nodes
 */
static __rte_always_inline struct qnode *
_qnode_alloc(void)
{
	struct qnode_pool *p = (THIS_SCHED)->qnode_pool;
	int prealloc_size = p->pre_alloc;
	struct qnode *n;
	int i;

	if (likely(p->fast_alloc != NULL)) {
		n = p->fast_alloc;
		p->fast_alloc = NULL;
		return n;
	}

	n = _qnode_pool_remove(p);

	if (unlikely(n == NULL)) {
		DIAG_COUNT_INC(p, prealloc);
		for (i = 0; i < prealloc_size; i++) {
			n = rte_malloc_socket(NULL,
					sizeof(struct qnode),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());
			if (n == NULL)
				return NULL;

			DIAG_COUNT_INC(p, available);
			DIAG_COUNT_INC(p, capacity);

			n->pool = p;
			_qnode_pool_insert(p, n);
		}
		n = _qnode_pool_remove(p);
	}
	n->pool = p;
	DIAG_COUNT_INC(p, rd);
	DIAG_COUNT_DEC(p, available);
	return n;
}



/*
* free a queue node to the per scheduler pool from which it came
*/
static __rte_always_inline void
_qnode_free(struct qnode *n)
{
	struct qnode_pool *p = n->pool;


	if (unlikely(p->fast_alloc != NULL) ||
			unlikely(n->pool != (THIS_SCHED)->qnode_pool)) {
		DIAG_COUNT_INC(p, wr);
		DIAG_COUNT_INC(p, available);
		_qnode_pool_insert(p, n);
		return;
	}
	p->fast_alloc = n;
}

/*
 * Destroy an qnode pool
 * queue must be empty when this is called
 */
static inline int
_qnode_pool_destroy(struct qnode_pool *p)
{
	rte_free(p->stub);
	rte_free(p);
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_POOL_H_ */
