/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
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

#include <rte_per_lcore.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include <rte_common.h>
#include <rte_string_fns.h>

#include "lthread_api.h"
#include "lthread_int.h"
#include "lthread_mutex.h"
#include "lthread_sched.h"
#include "lthread_queue.h"
#include "lthread_objcache.h"
#include "lthread_diag.h"

/*
 * Create a mutex
 */
int
lthread_mutex_init(char *name, struct lthread_mutex **mutex,
		   __rte_unused const struct lthread_mutexattr *attr)
{
	struct lthread_mutex *m;

	if (mutex == NULL)
		return POSIX_ERRNO(EINVAL);


	m = _lthread_objcache_alloc((THIS_SCHED)->mutex_cache);
	if (m == NULL)
		return POSIX_ERRNO(EAGAIN);

	m->blocked = _lthread_queue_create("blocked queue");
	if (m->blocked == NULL) {
		_lthread_objcache_free((THIS_SCHED)->mutex_cache, m);
		return POSIX_ERRNO(EAGAIN);
	}

	if (name == NULL)
		strlcpy(m->name, "no name", sizeof(m->name));
	else
		strlcpy(m->name, name, sizeof(m->name));

	m->root_sched = THIS_SCHED;
	m->owner = NULL;

	__atomic_store_n(&m->count, 0, __ATOMIC_RELAXED);

	DIAG_CREATE_EVENT(m, LT_DIAG_MUTEX_CREATE);
	/* success */
	(*mutex) = m;
	return 0;
}

/*
 * Destroy a mutex
 */
int lthread_mutex_destroy(struct lthread_mutex *m)
{
	if ((m == NULL) || (m->blocked == NULL)) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_DESTROY, m, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	if (m->owner == NULL) {
		/* try to delete the blocked queue */
		if (_lthread_queue_destroy(m->blocked) < 0) {
			DIAG_EVENT(m, LT_DIAG_MUTEX_DESTROY,
					m, POSIX_ERRNO(EBUSY));
			return POSIX_ERRNO(EBUSY);
		}

		/* free the mutex to cache */
		_lthread_objcache_free(m->root_sched->mutex_cache, m);
		DIAG_EVENT(m, LT_DIAG_MUTEX_DESTROY, m, 0);
		return 0;
	}
	/* can't do its still in use */
	DIAG_EVENT(m, LT_DIAG_MUTEX_DESTROY, m, POSIX_ERRNO(EBUSY));
	return POSIX_ERRNO(EBUSY);
}

/*
 * Try to obtain a mutex
 */
int lthread_mutex_lock(struct lthread_mutex *m)
{
	struct lthread *lt = THIS_LTHREAD;

	if ((m == NULL) || (m->blocked == NULL)) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_LOCK, m, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	/* allow no recursion */
	if (m->owner == lt) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_LOCK, m, POSIX_ERRNO(EDEADLK));
		return POSIX_ERRNO(EDEADLK);
	}

	for (;;) {
		__atomic_fetch_add(&m->count, 1, __ATOMIC_RELAXED);
		do {
			uint64_t lt_init = 0;
			if (__atomic_compare_exchange_n((uint64_t *) &m->owner, &lt_init,
				(uint64_t) lt, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
				/* happy days, we got the lock */
				DIAG_EVENT(m, LT_DIAG_MUTEX_LOCK, m, 0);
				return 0;
			}
			/* spin due to race with unlock when
			* nothing was blocked
			*/
		} while ((__atomic_load_n(&m->count, __ATOMIC_RELAXED) == 1) &&
				(m->owner == NULL));

		/* queue the current thread in the blocked queue
		 * we defer this to after we return to the scheduler
		 * to ensure that the current thread context is saved
		 * before unlock could result in it being dequeued and
		 * resumed
		 */
		DIAG_EVENT(m, LT_DIAG_MUTEX_BLOCKED, m, lt);
		lt->pending_wr_queue = m->blocked;
		/* now relinquish cpu */
		_suspend();
		/* resumed, must loop and compete for the lock again */
	}
	return 0;
}

/* try to lock a mutex but don't block */
int lthread_mutex_trylock(struct lthread_mutex *m)
{
	struct lthread *lt = THIS_LTHREAD;

	if ((m == NULL) || (m->blocked == NULL)) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_TRYLOCK, m, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	if (m->owner == lt) {
		/* no recursion */
		DIAG_EVENT(m, LT_DIAG_MUTEX_TRYLOCK, m, POSIX_ERRNO(EDEADLK));
		return POSIX_ERRNO(EDEADLK);
	}

	__atomic_fetch_add(&m->count, 1, __ATOMIC_RELAXED);
	uint64_t lt_init = 0;
	if (__atomic_compare_exchange_n((uint64_t *) &m->owner, &lt_init,
		(uint64_t) lt, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
		/* got the lock */
		DIAG_EVENT(m, LT_DIAG_MUTEX_TRYLOCK, m, 0);
		return 0;
	}

	/* failed so return busy */
	__atomic_fetch_sub(&m->count, 1, __ATOMIC_RELAXED);
	DIAG_EVENT(m, LT_DIAG_MUTEX_TRYLOCK, m, POSIX_ERRNO(EBUSY));
	return POSIX_ERRNO(EBUSY);
}

/*
 * Unlock a mutex
 */
int lthread_mutex_unlock(struct lthread_mutex *m)
{
	struct lthread *lt = THIS_LTHREAD;
	struct lthread *unblocked;

	if ((m == NULL) || (m->blocked == NULL)) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_UNLOCKED, m, POSIX_ERRNO(EINVAL));
		return POSIX_ERRNO(EINVAL);
	}

	/* fail if its owned */
	if (m->owner != lt || m->owner == NULL) {
		DIAG_EVENT(m, LT_DIAG_MUTEX_UNLOCKED, m, POSIX_ERRNO(EPERM));
		return POSIX_ERRNO(EPERM);
	}

	__atomic_fetch_sub(&m->count, 1, __ATOMIC_RELAXED);
	/* if there are blocked threads then make one ready */
	while (__atomic_load_n(&m->count, __ATOMIC_RELAXED) > 0) {
		unblocked = _lthread_queue_remove(m->blocked);

		if (unblocked != NULL) {
			__atomic_fetch_sub(&m->count, 1, __ATOMIC_RELAXED);
			DIAG_EVENT(m, LT_DIAG_MUTEX_UNLOCKED, m, unblocked);
			RTE_ASSERT(unblocked->sched != NULL);
			_ready_queue_insert((struct lthread_sched *)
					    unblocked->sched, unblocked);
			break;
		}
	}
	/* release the lock */
	m->owner = NULL;
	return 0;
}

/*
 * return the diagnostic ref val stored in a mutex
 */
uint64_t
lthread_mutex_diag_ref(struct lthread_mutex *m)
{
	if (m == NULL)
		return 0;
	return m->diag_ref;
}
