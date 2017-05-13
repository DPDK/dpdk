/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015  Intel Corporation. All rights reserved.
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
 * Some portions of this software is derived from the
 * https://github.com/halayli/lthread which carrys the following license.
 *
 * Copyright (C) 2012, Hasan Alayli <halayli@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
