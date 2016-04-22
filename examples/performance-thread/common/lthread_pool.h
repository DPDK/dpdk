/*
 *-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

/*
 * Some portions of this software is derived from the producer
 * consumer queues described by Dmitry Vyukov and published  here
 * http://www.1024cores.net
 *
 * Copyright (c) 2010-2011 Dmitry Vyukov. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DMITRY VYUKOV OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Dmitry Vyukov.
 */

#ifndef LTHREAD_POOL_H_
#define LTHREAD_POOL_H_

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
static inline void __attribute__ ((always_inline))
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
static inline struct qnode *__attribute__ ((always_inline))
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
static inline struct qnode *__attribute__ ((always_inline))
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
static inline struct qnode *__attribute__ ((always_inline))
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
static inline void __attribute__ ((always_inline))
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


#endif				/* LTHREAD_POOL_H_ */
