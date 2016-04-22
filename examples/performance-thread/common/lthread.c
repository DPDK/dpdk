/*-
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

#define RTE_MEM 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <rte_log.h>
#include <ctx.h>

#include "lthread_api.h"
#include "lthread.h"
#include "lthread_timer.h"
#include "lthread_tls.h"
#include "lthread_objcache.h"
#include "lthread_diag.h"


/*
 * This function gets called after an lthread function has returned.
 */
void _lthread_exit_handler(struct lthread *lt)
{

	lt->state |= BIT(ST_LT_EXITED);

	if (!(lt->state & BIT(ST_LT_DETACH))) {
		/* thread is this not explicitly detached
		 * it must be joinable, so we call lthread_exit().
		 */
		lthread_exit(NULL);
	}

	/* if we get here the thread is detached so we can reschedule it,
	 * allowing the scheduler to free it
	 */
	_reschedule();
}


/*
 * Free resources allocated to an lthread
 */
void _lthread_free(struct lthread *lt)
{

	DIAG_EVENT(lt, LT_DIAG_LTHREAD_FREE, lt, 0);

	/* invoke any user TLS destructor functions */
	_lthread_tls_destroy(lt);

	/* free memory allocated for TLS defined using RTE_PER_LTHREAD macros */
	if (sizeof(void *) < (uint64_t)RTE_PER_LTHREAD_SECTION_SIZE)
		_lthread_objcache_free(lt->tls->root_sched->per_lthread_cache,
					lt->per_lthread_data);

	/* free pthread style TLS memory */
	_lthread_objcache_free(lt->tls->root_sched->tls_cache, lt->tls);

	/* free the stack */
	_lthread_objcache_free(lt->stack_container->root_sched->stack_cache,
				lt->stack_container);

	/* now free the thread */
	_lthread_objcache_free(lt->root_sched->lthread_cache, lt);

}

/*
 * Allocate a stack and maintain a cache of stacks
 */
struct lthread_stack *_stack_alloc(void)
{
	struct lthread_stack *s;

	s = _lthread_objcache_alloc((THIS_SCHED)->stack_cache);
	RTE_ASSERT(s != NULL);

	s->root_sched = THIS_SCHED;
	s->stack_size = LTHREAD_MAX_STACK_SIZE;
	return s;
}

/*
 * Execute a ctx by invoking the start function
 * On return call an exit handler if the user has provided one
 */
static void _lthread_exec(void *arg)
{
	struct lthread *lt = (struct lthread *)arg;

	/* invoke the contexts function */
	lt->fun(lt->arg);
	/* do exit handling */
	if (lt->exit_handler != NULL)
		lt->exit_handler(lt);
}

/*
 *	Initialize an lthread
 *	Set its function, args, and exit handler
 */
void
_lthread_init(struct lthread *lt,
	lthread_func_t fun, void *arg, lthread_exit_func exit_handler)
{

	/* set ctx func and args */
	lt->fun = fun;
	lt->arg = arg;
	lt->exit_handler = exit_handler;

	/* set initial state */
	lt->birth = _sched_now();
	lt->state = BIT(ST_LT_INIT);
	lt->join = LT_JOIN_INITIAL;
}

/*
 *	set the lthread stack
 */
void _lthread_set_stack(struct lthread *lt, void *stack, size_t stack_size)
{
	char *stack_top = (char *)stack + stack_size;
	void **s = (void **)stack_top;

	/* set stack */
	lt->stack = stack;
	lt->stack_size = stack_size;

	/* set initial context */
	s[-3] = NULL;
	s[-2] = (void *)lt;
	lt->ctx.rsp = (void *)(stack_top - (4 * sizeof(void *)));
	lt->ctx.rbp = (void *)(stack_top - (3 * sizeof(void *)));
	lt->ctx.rip = (void *)_lthread_exec;
}

/*
 * Create an lthread on the current scheduler
 * If there is no current scheduler on this pthread then first create one
 */
int
lthread_create(struct lthread **new_lt, int lcore_id,
		lthread_func_t fun, void *arg)
{
	if ((new_lt == NULL) || (fun == NULL))
		return POSIX_ERRNO(EINVAL);

	if (lcore_id < 0)
		lcore_id = rte_lcore_id();
	else if (lcore_id > LTHREAD_MAX_LCORES)
		return POSIX_ERRNO(EINVAL);

	struct lthread *lt = NULL;

	if (THIS_SCHED == NULL) {
		THIS_SCHED = _lthread_sched_create(0);
		if (THIS_SCHED == NULL) {
			perror("Failed to create scheduler");
			return POSIX_ERRNO(EAGAIN);
		}
	}

	/* allocate a thread structure */
	lt = _lthread_objcache_alloc((THIS_SCHED)->lthread_cache);
	if (lt == NULL)
		return POSIX_ERRNO(EAGAIN);

	bzero(lt, sizeof(struct lthread));
	lt->root_sched = THIS_SCHED;

	/* set the function args and exit handlder */
	_lthread_init(lt, fun, arg, _lthread_exit_handler);

	/* put it in the ready queue */
	*new_lt = lt;

	if (lcore_id < 0)
		lcore_id = rte_lcore_id();

	DIAG_CREATE_EVENT(lt, LT_DIAG_LTHREAD_CREATE);

	rte_wmb();
	_ready_queue_insert(_lthread_sched_get(lcore_id), lt);
	return 0;
}

/*
 * Schedules lthread to sleep for `nsecs`
 * setting the lthread state to LT_ST_SLEEPING.
 * lthread state is cleared upon resumption or expiry.
 */
static inline void _lthread_sched_sleep(struct lthread *lt, uint64_t nsecs)
{
	uint64_t state = lt->state;
	uint64_t clks = _ns_to_clks(nsecs);

	if (clks) {
		_timer_start(lt, clks);
		lt->state = state | BIT(ST_LT_SLEEPING);
	}
	DIAG_EVENT(lt, LT_DIAG_LTHREAD_SLEEP, clks, 0);
	_suspend();
}



/*
 * Cancels any running timer.
 * This can be called multiple times on the same lthread regardless if it was
 * sleeping or not.
 */
int _lthread_desched_sleep(struct lthread *lt)
{
	uint64_t state = lt->state;

	if (state & BIT(ST_LT_SLEEPING)) {
		_timer_stop(lt);
		state &= (CLEARBIT(ST_LT_SLEEPING) & CLEARBIT(ST_LT_EXPIRED));
		lt->state = state | BIT(ST_LT_READY);
		return 1;
	}
	return 0;
}

/*
 * set user data pointer in an lthread
 */
void lthread_set_data(void *data)
{
	if (sizeof(void *) == RTE_PER_LTHREAD_SECTION_SIZE)
		THIS_LTHREAD->per_lthread_data = data;
}

/*
 * Retrieve user data pointer from an lthread
 */
void *lthread_get_data(void)
{
	return THIS_LTHREAD->per_lthread_data;
}

/*
 * Return the current lthread handle
 */
struct lthread *lthread_current(void)
{
	struct lthread_sched *sched = THIS_SCHED;

	if (sched)
		return sched->current_lthread;
	return NULL;
}



/*
 * Tasklet to cancel a thread
 */
static void
_cancel(void *arg)
{
	struct lthread *lt = (struct lthread *) arg;

	lt->state |= BIT(ST_LT_CANCELLED);
	lthread_detach();
}


/*
 * Mark the specified as canceled
 */
int lthread_cancel(struct lthread *cancel_lt)
{
	struct lthread *lt;

	if ((cancel_lt == NULL) || (cancel_lt == THIS_LTHREAD))
		return POSIX_ERRNO(EINVAL);

	DIAG_EVENT(cancel_lt, LT_DIAG_LTHREAD_CANCEL, cancel_lt, 0);

	if (cancel_lt->sched != THIS_SCHED) {

		/* spawn task-let to cancel the thread */
		lthread_create(&lt,
				cancel_lt->sched->lcore_id,
				_cancel,
				cancel_lt);
		return 0;
	}
	cancel_lt->state |= BIT(ST_LT_CANCELLED);
	return 0;
}

/*
 * Suspend the current lthread for specified time
 */
void lthread_sleep(uint64_t nsecs)
{
	struct lthread *lt = THIS_LTHREAD;

	_lthread_sched_sleep(lt, nsecs);

}

/*
 * Suspend the current lthread for specified time
 */
void lthread_sleep_clks(uint64_t clks)
{
	struct lthread *lt = THIS_LTHREAD;
	uint64_t state = lt->state;

	if (clks) {
		_timer_start(lt, clks);
		lt->state = state | BIT(ST_LT_SLEEPING);
	}
	DIAG_EVENT(lt, LT_DIAG_LTHREAD_SLEEP, clks, 0);
	_suspend();
}

/*
 * Requeue the current thread to the back of the ready queue
 */
void lthread_yield(void)
{
	struct lthread *lt = THIS_LTHREAD;

	DIAG_EVENT(lt, LT_DIAG_LTHREAD_YIELD, 0, 0);

	_ready_queue_insert(THIS_SCHED, lt);
	ctx_switch(&(THIS_SCHED)->ctx, &lt->ctx);
}

/*
 * Exit the current lthread
 * If a thread is joining pass the user pointer to it
 */
void lthread_exit(void *ptr)
{
	struct lthread *lt = THIS_LTHREAD;

	/* if thread is detached (this is not valid) just exit */
	if (lt->state & BIT(ST_LT_DETACH))
		return;

	/* There is a race between lthread_join() and lthread_exit()
	 *  - if exit before join then we suspend and resume on join
	 *  - if join before exit then we resume the joining thread
	 */
	if ((lt->join == LT_JOIN_INITIAL)
	    && rte_atomic64_cmpset(&lt->join, LT_JOIN_INITIAL,
				   LT_JOIN_EXITING)) {

		DIAG_EVENT(lt, LT_DIAG_LTHREAD_EXIT, 1, 0);
		_suspend();
		/* set the exit value */
		if ((ptr != NULL) && (lt->lt_join->lt_exit_ptr != NULL))
			*(lt->lt_join->lt_exit_ptr) = ptr;

		/* let the joining thread know we have set the exit value */
		lt->join = LT_JOIN_EXIT_VAL_SET;
	} else {

		DIAG_EVENT(lt, LT_DIAG_LTHREAD_EXIT, 0, 0);
		/* set the exit value */
		if ((ptr != NULL) && (lt->lt_join->lt_exit_ptr != NULL))
			*(lt->lt_join->lt_exit_ptr) = ptr;
		/* let the joining thread know we have set the exit value */
		lt->join = LT_JOIN_EXIT_VAL_SET;
		_ready_queue_insert(lt->lt_join->sched,
				    (struct lthread *)lt->lt_join);
	}


	/* wait until the joinging thread has collected the exit value */
	while (lt->join != LT_JOIN_EXIT_VAL_READ)
		_reschedule();

	/* reset join state */
	lt->join = LT_JOIN_INITIAL;

	/* detach it so its resources can be released */
	lt->state |= (BIT(ST_LT_DETACH) | BIT(ST_LT_EXITED));
}

/*
 * Join an lthread
 * Suspend until the joined thread returns
 */
int lthread_join(struct lthread *lt, void **ptr)
{
	if (lt == NULL)
		return POSIX_ERRNO(EINVAL);

	struct lthread *current = THIS_LTHREAD;
	uint64_t lt_state = lt->state;

	/* invalid to join a detached thread, or a thread that is joined */
	if ((lt_state & BIT(ST_LT_DETACH)) || (lt->join == LT_JOIN_THREAD_SET))
		return POSIX_ERRNO(EINVAL);
	/* pointer to the joining thread and a poingter to return a value */
	lt->lt_join = current;
	current->lt_exit_ptr = ptr;
	/* There is a race between lthread_join() and lthread_exit()
	 *  - if join before exit we suspend and will resume when exit is called
	 *  - if exit before join we resume the exiting thread
	 */
	if ((lt->join == LT_JOIN_INITIAL)
	    && rte_atomic64_cmpset(&lt->join, LT_JOIN_INITIAL,
				   LT_JOIN_THREAD_SET)) {

		DIAG_EVENT(current, LT_DIAG_LTHREAD_JOIN, lt, 1);
		_suspend();
	} else {
		DIAG_EVENT(current, LT_DIAG_LTHREAD_JOIN, lt, 0);
		_ready_queue_insert(lt->sched, lt);
	}

	/* wait for exiting thread to set return value */
	while (lt->join != LT_JOIN_EXIT_VAL_SET)
		_reschedule();

	/* collect the return value */
	if (ptr != NULL)
		*ptr = *current->lt_exit_ptr;

	/* let the exiting thread proceed to exit */
	lt->join = LT_JOIN_EXIT_VAL_READ;
	return 0;
}


/*
 * Detach current lthread
 * A detached thread cannot be joined
 */
void lthread_detach(void)
{
	struct lthread *lt = THIS_LTHREAD;

	DIAG_EVENT(lt, LT_DIAG_LTHREAD_DETACH, 0, 0);

	uint64_t state = lt->state;

	lt->state = state | BIT(ST_LT_DETACH);
}

/*
 * Set function name of an lthread
 * this is a debug aid
 */
void lthread_set_funcname(const char *f)
{
	struct lthread *lt = THIS_LTHREAD;

	strncpy(lt->funcname, f, sizeof(lt->funcname));
	lt->funcname[sizeof(lt->funcname)-1] = 0;
}
