/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <sched.h>
#include <dlfcn.h>

#include <rte_log.h>

#include "lthread_api.h"
#include "pthread_shim.h"

#define RTE_LOGTYPE_PTHREAD_SHIM RTE_LOGTYPE_USER3

#define POSIX_ERRNO(x)  (x)

/* some releases of FreeBSD 10, e.g. 10.0, don't have CPU_COUNT macro */
#ifndef CPU_COUNT
#define CPU_COUNT(x) __cpu_count(x)

static inline unsigned int
__cpu_count(const rte_cpuset_t *cpuset)
{
	unsigned int i, count = 0;
	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (CPU_ISSET(i, cpuset))
			count++;
	return count;
}
#endif

/*
 * this flag determines at run time if we override pthread
 * calls and map then to equivalent lthread calls
 * or of we call the standard pthread function
 */
static __thread int override;


/*
 * this structures contains function pointers that will be
 * initialised to the loaded address of the real
 * pthread library API functions
 */
struct pthread_lib_funcs {
int (*f_pthread_barrier_destroy)
	(pthread_barrier_t *);
int (*f_pthread_barrier_init)
	(pthread_barrier_t *, const pthread_barrierattr_t *, unsigned);
int (*f_pthread_barrier_wait)
	(pthread_barrier_t *);
int (*f_pthread_cond_broadcast)
	(pthread_cond_t *);
int (*f_pthread_cond_destroy)
	(pthread_cond_t *);
int (*f_pthread_cond_init)
	(pthread_cond_t *, const pthread_condattr_t *);
int (*f_pthread_cond_signal)
	(pthread_cond_t *);
int (*f_pthread_cond_timedwait)
	(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
int (*f_pthread_cond_wait)
	(pthread_cond_t *, pthread_mutex_t *);
int (*f_pthread_create)
	(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int (*f_pthread_detach)
	(pthread_t);
int (*f_pthread_equal)
	(pthread_t, pthread_t);
void (*f_pthread_exit)
	(void *);
void * (*f_pthread_getspecific)
	(pthread_key_t);
int (*f_pthread_getcpuclockid)
	(pthread_t, clockid_t *);
int (*f_pthread_join)
	(pthread_t, void **);
int (*f_pthread_key_create)
	(pthread_key_t *, void (*) (void *));
int (*f_pthread_key_delete)
	(pthread_key_t);
int (*f_pthread_mutex_destroy)
	(pthread_mutex_t *__mutex);
int (*f_pthread_mutex_init)
	(pthread_mutex_t *__mutex, const pthread_mutexattr_t *);
int (*f_pthread_mutex_lock)
	(pthread_mutex_t *__mutex);
int (*f_pthread_mutex_trylock)
	(pthread_mutex_t *__mutex);
int (*f_pthread_mutex_timedlock)
	(pthread_mutex_t *__mutex, const struct timespec *);
int (*f_pthread_mutex_unlock)
	(pthread_mutex_t *__mutex);
int (*f_pthread_once)
	(pthread_once_t *, void (*) (void));
int (*f_pthread_rwlock_destroy)
	(pthread_rwlock_t *__rwlock);
int (*f_pthread_rwlock_init)
	(pthread_rwlock_t *__rwlock, const pthread_rwlockattr_t *);
int (*f_pthread_rwlock_rdlock)
	(pthread_rwlock_t *__rwlock);
int (*f_pthread_rwlock_timedrdlock)
	(pthread_rwlock_t *__rwlock, const struct timespec *);
int (*f_pthread_rwlock_timedwrlock)
	(pthread_rwlock_t *__rwlock, const struct timespec *);
int (*f_pthread_rwlock_tryrdlock)
	(pthread_rwlock_t *__rwlock);
int (*f_pthread_rwlock_trywrlock)
	(pthread_rwlock_t *__rwlock);
int (*f_pthread_rwlock_unlock)
	(pthread_rwlock_t *__rwlock);
int (*f_pthread_rwlock_wrlock)
	(pthread_rwlock_t *__rwlock);
pthread_t (*f_pthread_self)
	(void);
int (*f_pthread_setspecific)
	(pthread_key_t, const void *);
int (*f_pthread_spin_init)
	(pthread_spinlock_t *__spin, int);
int (*f_pthread_spin_destroy)
	(pthread_spinlock_t *__spin);
int (*f_pthread_spin_lock)
	(pthread_spinlock_t *__spin);
int (*f_pthread_spin_trylock)
	(pthread_spinlock_t *__spin);
int (*f_pthread_spin_unlock)
	(pthread_spinlock_t *__spin);
int (*f_pthread_cancel)
	(pthread_t);
int (*f_pthread_setcancelstate)
	(int, int *);
int (*f_pthread_setcanceltype)
	(int, int *);
void (*f_pthread_testcancel)
	(void);
int (*f_pthread_getschedparam)
	(pthread_t pthread, int *, struct sched_param *);
int (*f_pthread_setschedparam)
	(pthread_t, int, const struct sched_param *);
int (*f_pthread_yield)
	(void);
int (*f_pthread_setaffinity_np)
	(pthread_t thread, size_t cpusetsize, const rte_cpuset_t *cpuset);
int (*f_nanosleep)
	(const struct timespec *req, struct timespec *rem);
} _sys_pthread_funcs = {
	.f_pthread_barrier_destroy = NULL,
};


/*
 * this macro obtains the loaded address of a library function
 * and saves it.
 */
static void *__libc_dl_handle = RTLD_NEXT;

#define get_addr_of_loaded_symbol(name) do {				\
	char *error_str;						\
	_sys_pthread_funcs.f_##name = dlsym(__libc_dl_handle, (#name));	\
	error_str = dlerror();						\
	if (error_str != NULL) {					\
		fprintf(stderr, "%s\n", error_str);			\
	}								\
} while (0)


/*
 * The constructor function initialises the
 * function pointers for pthread library functions
 */
RTE_INIT(pthread_intercept_ctor)
{
	override = 0;
	/*
	 * Get the original functions
	 */
	get_addr_of_loaded_symbol(pthread_barrier_destroy);
	get_addr_of_loaded_symbol(pthread_barrier_init);
	get_addr_of_loaded_symbol(pthread_barrier_wait);
	get_addr_of_loaded_symbol(pthread_cond_broadcast);
	get_addr_of_loaded_symbol(pthread_cond_destroy);
	get_addr_of_loaded_symbol(pthread_cond_init);
	get_addr_of_loaded_symbol(pthread_cond_signal);
	get_addr_of_loaded_symbol(pthread_cond_timedwait);
	get_addr_of_loaded_symbol(pthread_cond_wait);
	get_addr_of_loaded_symbol(pthread_create);
	get_addr_of_loaded_symbol(pthread_detach);
	get_addr_of_loaded_symbol(pthread_equal);
	get_addr_of_loaded_symbol(pthread_exit);
	get_addr_of_loaded_symbol(pthread_getspecific);
	get_addr_of_loaded_symbol(pthread_getcpuclockid);
	get_addr_of_loaded_symbol(pthread_join);
	get_addr_of_loaded_symbol(pthread_key_create);
	get_addr_of_loaded_symbol(pthread_key_delete);
	get_addr_of_loaded_symbol(pthread_mutex_destroy);
	get_addr_of_loaded_symbol(pthread_mutex_init);
	get_addr_of_loaded_symbol(pthread_mutex_lock);
	get_addr_of_loaded_symbol(pthread_mutex_trylock);
	get_addr_of_loaded_symbol(pthread_mutex_timedlock);
	get_addr_of_loaded_symbol(pthread_mutex_unlock);
	get_addr_of_loaded_symbol(pthread_once);
	get_addr_of_loaded_symbol(pthread_rwlock_destroy);
	get_addr_of_loaded_symbol(pthread_rwlock_init);
	get_addr_of_loaded_symbol(pthread_rwlock_rdlock);
	get_addr_of_loaded_symbol(pthread_rwlock_timedrdlock);
	get_addr_of_loaded_symbol(pthread_rwlock_timedwrlock);
	get_addr_of_loaded_symbol(pthread_rwlock_tryrdlock);
	get_addr_of_loaded_symbol(pthread_rwlock_trywrlock);
	get_addr_of_loaded_symbol(pthread_rwlock_unlock);
	get_addr_of_loaded_symbol(pthread_rwlock_wrlock);
	get_addr_of_loaded_symbol(pthread_self);
	get_addr_of_loaded_symbol(pthread_setspecific);
	get_addr_of_loaded_symbol(pthread_spin_init);
	get_addr_of_loaded_symbol(pthread_spin_destroy);
	get_addr_of_loaded_symbol(pthread_spin_lock);
	get_addr_of_loaded_symbol(pthread_spin_trylock);
	get_addr_of_loaded_symbol(pthread_spin_unlock);
	get_addr_of_loaded_symbol(pthread_cancel);
	get_addr_of_loaded_symbol(pthread_setcancelstate);
	get_addr_of_loaded_symbol(pthread_setcanceltype);
	get_addr_of_loaded_symbol(pthread_testcancel);
	get_addr_of_loaded_symbol(pthread_getschedparam);
	get_addr_of_loaded_symbol(pthread_setschedparam);
	get_addr_of_loaded_symbol(pthread_yield);
	get_addr_of_loaded_symbol(pthread_setaffinity_np);
	get_addr_of_loaded_symbol(nanosleep);
}


/*
 * Enable/Disable pthread override
 * state
 *  0 disable
 *  1 enable
 */
void pthread_override_set(int state)
{
	override = state;
}


/*
 * Return pthread override state
 * return
 *  0 disable
 *  1 enable
 */
int pthread_override_get(void)
{
	return override;
}

/*
 * This macro is used to catch and log
 * invocation of stubs for unimplemented pthread
 * API functions.
 */
#define NOT_IMPLEMENTED do {				\
	if (override) {					\
		RTE_LOG(WARNING,			\
			PTHREAD_SHIM,			\
			"WARNING %s NOT IMPLEMENTED\n",	\
			__func__);			\
	}						\
} while (0)

/*
 * pthread API override functions follow
 * Note in this example code only a subset of functions are
 * implemented.
 *
 * The stub functions provided will issue a warning log
 * message if an unimplemented function is invoked
 *
 */

int pthread_barrier_destroy(pthread_barrier_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_barrier_destroy(a);
}

int
pthread_barrier_init(pthread_barrier_t *a,
		     const pthread_barrierattr_t *b, unsigned c)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_barrier_init(a, b, c);
}

int pthread_barrier_wait(pthread_barrier_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_barrier_wait(a);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	if (override) {

		lthread_cond_broadcast(*(struct lthread_cond **)cond);
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_cond_broadcast(cond);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (override)
		return lthread_mutex_destroy(*(struct lthread_mutex **)mutex);
	return _sys_pthread_funcs.f_pthread_mutex_destroy(mutex);
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	if (override)
		return lthread_cond_destroy(*(struct lthread_cond **)cond);
	return _sys_pthread_funcs.f_pthread_cond_destroy(cond);
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	if (override)
		return lthread_cond_init(NULL,
				(struct lthread_cond **)cond,
				(const struct lthread_condattr *) attr);
	return _sys_pthread_funcs.f_pthread_cond_init(cond, attr);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	if (override) {
		lthread_cond_signal(*(struct lthread_cond **)cond);
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_cond_signal(cond);
}

int
pthread_cond_timedwait(pthread_cond_t *__rte_restrict cond,
		       pthread_mutex_t *__rte_restrict mutex,
		       const struct timespec *__rte_restrict time)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_cond_timedwait(cond, mutex, time);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (override) {
		pthread_mutex_unlock(mutex);
		int rv = lthread_cond_wait(*(struct lthread_cond **)cond, 0);

		pthread_mutex_lock(mutex);
		return rv;
	}
	return _sys_pthread_funcs.f_pthread_cond_wait(cond, mutex);
}

int
pthread_create(pthread_t *__rte_restrict tid,
		const pthread_attr_t *__rte_restrict attr,
		lthread_func_t func,
	       void *__rte_restrict arg)
{
	if (override) {
		int lcore = -1;

		if (attr != NULL) {
			/* determine CPU being requested */
			rte_cpuset_t cpuset;

			CPU_ZERO(&cpuset);
			pthread_attr_getaffinity_np(attr,
						sizeof(rte_cpuset_t),
						&cpuset);

			if (CPU_COUNT(&cpuset) != 1)
				return POSIX_ERRNO(EINVAL);

			for (lcore = 0; lcore < LTHREAD_MAX_LCORES; lcore++) {
				if (!CPU_ISSET(lcore, &cpuset))
					continue;
				break;
			}
		}
		return lthread_create((struct lthread **)tid, lcore,
				      func, arg);
	}
	return _sys_pthread_funcs.f_pthread_create(tid, attr, func, arg);
}

int pthread_detach(pthread_t tid)
{
	if (override) {
		struct lthread *lt = (struct lthread *)tid;

		if (lt == lthread_current()) {
			lthread_detach();
			return 0;
		}
		NOT_IMPLEMENTED;
	}
	return _sys_pthread_funcs.f_pthread_detach(tid);
}

int pthread_equal(pthread_t a, pthread_t b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_equal(a, b);
}

void pthread_exit_override(void *v)
{
	if (override) {
		lthread_exit(v);
		return;
	}
	_sys_pthread_funcs.f_pthread_exit(v);
}

void
*pthread_getspecific(pthread_key_t key)
{
	if (override)
		return lthread_getspecific((unsigned int) key);
	return _sys_pthread_funcs.f_pthread_getspecific(key);
}

int pthread_getcpuclockid(pthread_t a, clockid_t *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_getcpuclockid(a, b);
}

int pthread_join(pthread_t tid, void **val)
{
	if (override)
		return lthread_join((struct lthread *)tid, val);
	return _sys_pthread_funcs.f_pthread_join(tid, val);
}

int pthread_key_create(pthread_key_t *keyptr, void (*dtor) (void *))
{
	if (override)
		return lthread_key_create((unsigned int *)keyptr, dtor);
	return _sys_pthread_funcs.f_pthread_key_create(keyptr, dtor);
}

int pthread_key_delete(pthread_key_t key)
{
	if (override) {
		lthread_key_delete((unsigned int) key);
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_key_delete(key);
}


int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	if (override)
		return lthread_mutex_init(NULL,
				(struct lthread_mutex **)mutex,
				(const struct lthread_mutexattr *)attr);
	return _sys_pthread_funcs.f_pthread_mutex_init(mutex, attr);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	if (override)
		return lthread_mutex_lock(*(struct lthread_mutex **)mutex);
	return _sys_pthread_funcs.f_pthread_mutex_lock(mutex);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if (override)
		return lthread_mutex_trylock(*(struct lthread_mutex **)mutex);
	return _sys_pthread_funcs.f_pthread_mutex_trylock(mutex);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_mutex_timedlock(mutex, b);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (override)
		return lthread_mutex_unlock(*(struct lthread_mutex **)mutex);
	return _sys_pthread_funcs.f_pthread_mutex_unlock(mutex);
}

int pthread_once(pthread_once_t *a, void (b) (void))
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_once(a, b);
}

int pthread_rwlock_destroy(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_destroy(a);
}

int pthread_rwlock_init(pthread_rwlock_t *a, const pthread_rwlockattr_t *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_init(a, b);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_rdlock(a);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *a, const struct timespec *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_timedrdlock(a, b);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *a, const struct timespec *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_timedwrlock(a, b);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_tryrdlock(a);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_trywrlock(a);
}

int pthread_rwlock_unlock(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_unlock(a);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_rwlock_wrlock(a);
}

#ifdef RTE_EXEC_ENV_LINUX
int
pthread_yield(void)
{
	if (override) {
		lthread_yield();
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_yield();
}
#else
void
pthread_yield(void)
{
	if (override)
		lthread_yield();
	else
		_sys_pthread_funcs.f_pthread_yield();
}
#endif

pthread_t pthread_self(void)
{
	if (override)
		return (pthread_t) lthread_current();
	return _sys_pthread_funcs.f_pthread_self();
}

int pthread_setspecific(pthread_key_t key, const void *data)
{
	if (override) {
		int rv =  lthread_setspecific((unsigned int)key, data);
		return rv;
	}
	return _sys_pthread_funcs.f_pthread_setspecific(key, data);
}

int pthread_spin_init(pthread_spinlock_t *a, int b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_spin_init(a, b);
}

int pthread_spin_destroy(pthread_spinlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_spin_destroy(a);
}

int pthread_spin_lock(pthread_spinlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_spin_lock(a);
}

int pthread_spin_trylock(pthread_spinlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_spin_trylock(a);
}

int pthread_spin_unlock(pthread_spinlock_t *a)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_spin_unlock(a);
}

int pthread_cancel(pthread_t tid)
{
	if (override) {
		lthread_cancel(*(struct lthread **)tid);
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_cancel(tid);
}

int pthread_setcancelstate(int a, int *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_setcancelstate(a, b);
}

int pthread_setcanceltype(int a, int *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_setcanceltype(a, b);
}

void pthread_testcancel(void)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_testcancel();
}


int pthread_getschedparam(pthread_t tid, int *a, struct sched_param *b)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_getschedparam(tid, a, b);
}

int pthread_setschedparam(pthread_t a, int b, const struct sched_param *c)
{
	NOT_IMPLEMENTED;
	return _sys_pthread_funcs.f_pthread_setschedparam(a, b, c);
}


int nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (override) {
		uint64_t ns = req->tv_sec * 1000000000 + req->tv_nsec;

		lthread_sleep(ns);
		return 0;
	}
	return _sys_pthread_funcs.f_nanosleep(req, rem);
}

int
pthread_setaffinity_np(pthread_t thread, size_t cpusetsize,
		       const rte_cpuset_t *cpuset)
{
	if (override) {
		/* we only allow affinity with a single CPU */
		if (CPU_COUNT(cpuset) != 1)
			return POSIX_ERRNO(EINVAL);

		/* we only allow the current thread to sets its own affinity */
		struct lthread *lt = (struct lthread *)thread;

		if (lthread_current() != lt)
			return POSIX_ERRNO(EINVAL);

		/* determine the CPU being requested */
		int i;

		for (i = 0; i < LTHREAD_MAX_LCORES; i++) {
			if (!CPU_ISSET(i, cpuset))
				continue;
			break;
		}
		/* check requested core is allowed */
		if (i == LTHREAD_MAX_LCORES)
			return POSIX_ERRNO(EINVAL);

		/* finally we can set affinity to the requested lcore */
		lthread_set_affinity(i);
		return 0;
	}
	return _sys_pthread_funcs.f_pthread_setaffinity_np(thread, cpusetsize,
							   cpuset);
}
