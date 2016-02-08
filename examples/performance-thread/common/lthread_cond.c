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
#include <errno.h>

#include <rte_log.h>
#include <rte_common.h>

#include "lthread_api.h"
#include "lthread_diag_api.h"
#include "lthread_diag.h"
#include "lthread_int.h"
#include "lthread_sched.h"
#include "lthread_queue.h"
#include "lthread_objcache.h"
#include "lthread_timer.h"
#include "lthread_mutex.h"
#include "lthread_cond.h"

/*
 * Create a condition variable
 */
int
lthread_cond_init(char *name, struct lthread_cond **cond,
		  __rte_unused const struct lthread_condattr *attr)
{
	struct lthread_cond *c;

	if (cond == NULL)
		return POSIX_ERRNO(EINVAL);

	/* allocate a condition variable from cache */
	c = _lthread_objcache_alloc((THIS_SCHED)->cond_cache);

	if (c == NULL)
		return POSIX_ERRNO(EAGAIN);

	c->blocked = _lthread_queue_create("blocked");
	if (c->blocked == NULL) {
		_lthread_objcache_free((THIS_SCHED)->cond_cache, (void *)c);
		return POSIX_ERRNO(EAGAIN);
	}

	if (name == NULL)
		strncpy(c->name, "no name", sizeof(c->name));
	else
		strncpy(c->name, name, sizeof(c->name));
	c->name[sizeof(c->name)-1] = 0;

	c->root_sched = THIS_SCHED;

	(*cond) = c;
	DIAG_CREATE_EVENT((*cond), LT_DIAG_COND_CREATE);
	return 0;
}

/*
 * Destroy a condition variable
 */
int lthread_cond_destroy(struct lthread_cond *c)
{
	if (c == NULL) {
		DIAG_EVENT(c, LT_DIAG_COND_DESTROY, c, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	/* try to free it */
	if (_lthread_queue_destroy(c->blocked) < 0) {
		/* queue in use */
		DIAG_EVENT(c, LT_DIAG_COND_DESTROY, c, POSIX_ERRNO(EBUSY));
		return POSIX_ERRNO(EBUSY);
	}

	/* okay free it */
	_lthread_objcache_free(c->root_sched->cond_cache, c);
	DIAG_EVENT(c, LT_DIAG_COND_DESTROY, c, 0);
	return 0;
}

/*
 * Wait on a condition variable
 */
int lthread_cond_wait(struct lthread_cond *c, __rte_unused uint64_t reserved)
{
	struct lthread *lt = THIS_LTHREAD;

	if (c == NULL) {
		DIAG_EVENT(c, LT_DIAG_COND_WAIT, c, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}


	DIAG_EVENT(c, LT_DIAG_COND_WAIT, c, 0);

	/* queue the current thread in the blocked queue
	 * this will be written when we return to the scheduler
	 * to ensure that the current thread context is saved
	 * before any signal could result in it being dequeued and
	 * resumed
	 */
	lt->pending_wr_queue = c->blocked;
	_suspend();

	/* the condition happened */
	return 0;
}

/*
 * Signal a condition variable
 * attempt to resume any blocked thread
 */
int lthread_cond_signal(struct lthread_cond *c)
{
	struct lthread *lt;

	if (c == NULL) {
		DIAG_EVENT(c, LT_DIAG_COND_SIGNAL, c, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	lt = _lthread_queue_remove(c->blocked);

	if (lt != NULL) {
		/* okay wake up this thread */
		DIAG_EVENT(c, LT_DIAG_COND_SIGNAL, c, lt);
		_ready_queue_insert((struct lthread_sched *)lt->sched, lt);
	}
	return 0;
}

/*
 * Broadcast a condition variable
 */
int lthread_cond_broadcast(struct lthread_cond *c)
{
	struct lthread *lt;

	if (c == NULL) {
		DIAG_EVENT(c, LT_DIAG_COND_BROADCAST, c, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	DIAG_EVENT(c, LT_DIAG_COND_BROADCAST, c, 0);
	do {
		/* drain the queue waking everybody */
		lt = _lthread_queue_remove(c->blocked);

		if (lt != NULL) {
			DIAG_EVENT(c, LT_DIAG_COND_BROADCAST, c, lt);
			/* wake up */
			_ready_queue_insert((struct lthread_sched *)lt->sched,
					    lt);
		}
	} while (!_lthread_queue_empty(c->blocked));
	_reschedule();
	DIAG_EVENT(c, LT_DIAG_COND_BROADCAST, c, 0);
	return 0;
}

/*
 * return the diagnostic ref val stored in a condition var
 */
uint64_t
lthread_cond_diag_ref(struct lthread_cond *c)
{
	if (c == NULL)
		return 0;
	return c->diag_ref;
}
