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
 * are met:
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

#ifndef LTHREAD_QUEUE_H_
#define LTHREAD_QUEUE_H_

#include <string.h>

#include <rte_prefetch.h>
#include <rte_per_lcore.h>

#include "lthread_int.h"
#include "lthread.h"
#include "lthread_diag.h"
#include "lthread_pool.h"

struct lthread_queue;

/*
 * This file implements an unbounded FIFO queue based on a lock free
 * linked list.
 *
 * The queue is non-intrusive in that it uses intermediate nodes, and does
 * not require these nodes to be inserted into the object being placed
 * in the queue.
 *
 * This is slightly more efficient than the very similar queue in lthread_pool
 * in that it does not have to swing a stub node as the queue becomes empty.
 *
 * The queue access functions allocate and free intermediate node
 * transparently from/to a per scheduler pool ( see lthread_pool.h ).
 *
 * The queue provides both MPSC and SPSC insert methods
 */

/*
 * define a queue of lthread nodes
 */
struct lthread_queue {
	struct qnode *head;
	struct qnode *tail __rte_cache_aligned;
	struct lthread_queue *p;
	char name[LT_MAX_NAME_SIZE];

	DIAG_COUNT_DEFINE(rd);
	DIAG_COUNT_DEFINE(wr);
	DIAG_COUNT_DEFINE(size);

} __rte_cache_aligned;



static inline struct lthread_queue *
_lthread_queue_create(const char *name)
{
	struct qnode *stub;
	struct lthread_queue *new_queue;

	new_queue = rte_malloc_socket(NULL, sizeof(struct lthread_queue),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());
	if (new_queue == NULL)
		return NULL;

	/* allocated stub node */
	stub = _qnode_alloc();
	RTE_ASSERT(stub);

	if (name != NULL)
		strncpy(new_queue->name, name, sizeof(new_queue->name));
	new_queue->name[sizeof(new_queue->name)-1] = 0;

	/* initialize queue as empty */
	stub->next = NULL;
	new_queue->head = stub;
	new_queue->tail = stub;

	DIAG_COUNT_INIT(new_queue, rd);
	DIAG_COUNT_INIT(new_queue, wr);
	DIAG_COUNT_INIT(new_queue, size);

	return new_queue;
}

/**
 * Return true if the queue is empty
 */
static inline int __attribute__ ((always_inline))
_lthread_queue_empty(struct lthread_queue *q)
{
	return q->tail == q->head;
}



/**
 * Destroy a queue
 * fail if queue is not empty
 */
static inline int _lthread_queue_destroy(struct lthread_queue *q)
{
	if (q == NULL)
		return -1;

	if (!_lthread_queue_empty(q))
		return -1;

	_qnode_free(q->head);
	rte_free(q);
	return 0;
}

RTE_DECLARE_PER_LCORE(struct lthread_sched *, this_sched);

/*
 * Insert a node into a queue
 * this implementation is multi producer safe
 */
static inline struct qnode *__attribute__ ((always_inline))
_lthread_queue_insert_mp(struct lthread_queue
							  *q, void *data)
{
	struct qnode *prev;
	struct qnode *n = _qnode_alloc();

	if (n == NULL)
		return NULL;

	/* set object in node */
	n->data = data;
	n->next = NULL;

	/* this is an MPSC method, perform a locked update */
	prev = n;
	prev =
	    (struct qnode *)__sync_lock_test_and_set((uint64_t *) &(q)->head,
					       (uint64_t) prev);
	/* there is a window of inconsistency until prev next is set,
	 * which is why remove must retry
	 */
	prev->next = n;

	DIAG_COUNT_INC(q, wr);
	DIAG_COUNT_INC(q, size);

	return n;
}

/*
 * Insert an node into a queue in single producer mode
 * this implementation is NOT mult producer safe
 */
static inline struct qnode *__attribute__ ((always_inline))
_lthread_queue_insert_sp(struct lthread_queue
							  *q, void *data)
{
	/* allocate a queue node */
	struct qnode *prev;
	struct qnode *n = _qnode_alloc();

	if (n == NULL)
		return NULL;

	/* set data in node */
	n->data = data;
	n->next = NULL;

	/* this is an SPSC method, no need for locked exchange operation */
	prev = q->head;
	prev->next = q->head = n;

	DIAG_COUNT_INC(q, wr);
	DIAG_COUNT_INC(q, size);

	return n;
}

/*
 * Remove a node from a queue
 */
static inline void *__attribute__ ((always_inline))
_lthread_queue_poll(struct lthread_queue *q)
{
	void *data = NULL;
	struct qnode *tail = q->tail;
	struct qnode *next = (struct qnode *)tail->next;
	/*
	 * There is a small window of inconsistency between producer and
	 * consumer whereby the queue may appear empty if consumer and
	 * producer access it at the same time.
	 * The consumer must handle this by retrying
	 */

	if (likely(next != NULL)) {
		q->tail = next;
		tail->data = next->data;
		data = tail->data;

		/* free the node */
		_qnode_free(tail);

		DIAG_COUNT_INC(q, rd);
		DIAG_COUNT_DEC(q, size);
		return data;
	}
	return NULL;
}

/*
 * Remove a node from a queue
 */
static inline void *__attribute__ ((always_inline))
_lthread_queue_remove(struct lthread_queue *q)
{
	void *data = NULL;

	/*
	 * There is a small window of inconsistency between producer and
	 * consumer whereby the queue may appear empty if consumer and
	 * producer access it at the same time. We handle this by retrying
	 */
	do {
		data = _lthread_queue_poll(q);

		if (likely(data != NULL)) {

			DIAG_COUNT_INC(q, rd);
			DIAG_COUNT_DEC(q, size);
			return data;
		}
		rte_compiler_barrier();
	} while (unlikely(!_lthread_queue_empty(q)));
	return NULL;
}


#endif				/* LTHREAD_QUEUE_H_ */
