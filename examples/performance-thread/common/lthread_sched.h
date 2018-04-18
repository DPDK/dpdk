/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2012 Hasan Alayli <halayli@gmail.com>
 */

#ifndef LTHREAD_SCHED_H_
#define LTHREAD_SCHED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_int.h"
#include "lthread_queue.h"
#include "lthread_objcache.h"
#include "lthread_diag.h"
#include "ctx.h"

/*
 * insert an lthread into a queue
 */
static inline void
_ready_queue_insert(struct lthread_sched *sched, struct lthread *lt)
{
	if (sched == THIS_SCHED)
		_lthread_queue_insert_sp((THIS_SCHED)->ready, lt);
	else
		_lthread_queue_insert_mp(sched->pready, lt);
}

/*
 * remove an lthread from a queue
 */
static inline struct lthread *_ready_queue_remove(struct lthread_queue *q)
{
	return _lthread_queue_remove(q);
}

/**
 * Return true if the ready queue is empty
 */
static inline int _ready_queue_empty(struct lthread_queue *q)
{
	return _lthread_queue_empty(q);
}

static inline uint64_t _sched_now(void)
{
	uint64_t now = rte_rdtsc();

	if (now > (THIS_SCHED)->birth)
		return now - (THIS_SCHED)->birth;
	if (now < (THIS_SCHED)->birth)
		return (THIS_SCHED)->birth - now;
	/* never return 0 because this means sleep forever */
	return 1;
}

static __rte_always_inline void
_affinitize(void);
static inline void
_affinitize(void)
{
	struct lthread *lt = THIS_LTHREAD;

	DIAG_EVENT(lt, LT_DIAG_LTHREAD_SUSPENDED, 0, 0);
	ctx_switch(&(THIS_SCHED)->ctx, &lt->ctx);
}

static __rte_always_inline void
_suspend(void);
static inline void
_suspend(void)
{
	struct lthread *lt = THIS_LTHREAD;

	(THIS_SCHED)->nb_blocked_threads++;
	DIAG_EVENT(lt, LT_DIAG_LTHREAD_SUSPENDED, 0, 0);
	ctx_switch(&(THIS_SCHED)->ctx, &lt->ctx);
	(THIS_SCHED)->nb_blocked_threads--;
}

static __rte_always_inline void
_reschedule(void);
static inline void
_reschedule(void)
{
	struct lthread *lt = THIS_LTHREAD;

	DIAG_EVENT(lt, LT_DIAG_LTHREAD_RESCHEDULED, 0, 0);
	_ready_queue_insert(THIS_SCHED, lt);
	ctx_switch(&(THIS_SCHED)->ctx, &lt->ctx);
}

extern struct lthread_sched *schedcore[];
void _sched_timer_cb(struct rte_timer *tim, void *arg);
void _sched_shutdown(__rte_unused void *arg);

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_SCHED_H_ */
