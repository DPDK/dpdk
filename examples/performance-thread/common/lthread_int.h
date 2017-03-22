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
 * Some portions of this software may have been derived from the
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
#ifndef LTHREAD_INT_H
#include <lthread_api.h>
#define LTHREAD_INT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <rte_memory.h>
#include <rte_cycles.h>
#include <rte_per_lcore.h>
#include <rte_timer.h>
#include <rte_atomic_64.h>
#include <rte_spinlock.h>
#include <ctx.h>

#include <lthread_api.h>
#include "lthread.h"
#include "lthread_diag.h"
#include "lthread_tls.h"

struct lthread;
struct lthread_sched;
struct lthread_cond;
struct lthread_mutex;
struct lthread_key;

struct key_pool;
struct qnode;
struct qnode_pool;
struct lthread_sched;
struct lthread_tls;


#define BIT(x) (1 << (x))
#define CLEARBIT(x) ~(1 << (x))

#define POSIX_ERRNO(x)  (x)

#define MAX_LTHREAD_NAME_SIZE 64

#define RTE_LOGTYPE_LTHREAD RTE_LOGTYPE_USER1


/* define some shorthand for current scheduler and current thread */
#define THIS_SCHED RTE_PER_LCORE(this_sched)
#define THIS_LTHREAD RTE_PER_LCORE(this_sched)->current_lthread

/*
 * Definition of an scheduler struct
 */
struct lthread_sched {
	struct ctx ctx;					/* cpu context */
	uint64_t birth;					/* time created */
	struct lthread *current_lthread;		/* running thread */
	unsigned lcore_id;				/* this sched lcore */
	int run_flag;					/* sched shutdown */
	uint64_t nb_blocked_threads;	/* blocked threads */
	struct lthread_queue *ready;			/* local ready queue */
	struct lthread_queue *pready;			/* peer ready queue */
	struct lthread_objcache *lthread_cache;		/* free lthreads */
	struct lthread_objcache *stack_cache;		/* free stacks */
	struct lthread_objcache *per_lthread_cache;	/* free per lthread */
	struct lthread_objcache *tls_cache;		/* free TLS */
	struct lthread_objcache *cond_cache;		/* free cond vars */
	struct lthread_objcache *mutex_cache;		/* free mutexes */
	struct qnode_pool *qnode_pool;		/* pool of queue nodes */
	struct key_pool *key_pool;		/* pool of free TLS keys */
	size_t stack_size;
	uint64_t diag_ref;				/* diag ref */
} __rte_cache_aligned;

RTE_DECLARE_PER_LCORE(struct lthread_sched *, this_sched);


/*
 * State for an lthread
 */
enum lthread_st {
	ST_LT_INIT,		/* initial state */
	ST_LT_READY,		/* lthread is ready to run */
	ST_LT_SLEEPING,		/* lthread is sleeping */
	ST_LT_EXPIRED,		/* lthread timeout has expired  */
	ST_LT_EXITED,		/* lthread has exited and needs cleanup */
	ST_LT_DETACH,		/* lthread frees on exit*/
	ST_LT_CANCELLED,	/* lthread has been cancelled */
};

/*
 * lthread sub states for exit/join
 */
enum join_st {
	LT_JOIN_INITIAL,	/* initial state */
	LT_JOIN_EXITING,	/* thread is exiting */
	LT_JOIN_THREAD_SET,	/* joining thread has been set */
	LT_JOIN_EXIT_VAL_SET,	/* exiting thread has set ret val */
	LT_JOIN_EXIT_VAL_READ,	/* joining thread has collected ret val */
};

/* defnition of an lthread stack object */
struct lthread_stack {
	uint8_t stack[LTHREAD_MAX_STACK_SIZE];
	size_t stack_size;
	struct lthread_sched *root_sched;
} __rte_cache_aligned;

/*
 * Definition of an lthread
 */
struct lthread {
	struct ctx ctx;				/* cpu context */

	uint64_t state;				/* current lthread state */

	struct lthread_sched *sched;		/* current scheduler */
	void *stack;				/* ptr to actual stack */
	size_t stack_size;			/* current stack_size */
	size_t last_stack_size;			/* last yield  stack_size */
	lthread_func_t fun;			/* func ctx is running */
	void *arg;				/* func args passed to func */
	void *per_lthread_data;			/* per lthread user data */
	lthread_exit_func exit_handler;		/* called when thread exits */
	uint64_t birth;				/* time lthread was born */
	struct lthread_queue *pending_wr_queue;	/* deferred  queue to write */
	struct lthread *lt_join;		/* lthread to join on */
	uint64_t join;				/* state for joining */
	void **lt_exit_ptr;			/* exit ptr for lthread_join */
	struct lthread_sched *root_sched;	/* thread was created here*/
	struct queue_node *qnode;		/* node when in a queue */
	struct rte_timer tim;			/* sleep timer */
	struct lthread_tls *tls;		/* keys in use by the thread */
	struct lthread_stack *stack_container;	/* stack */
	char funcname[MAX_LTHREAD_NAME_SIZE];	/* thread func name */
	uint64_t diag_ref;			/* ref to user diag data */
} __rte_cache_aligned;

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_INT_H */
