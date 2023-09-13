/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <eal_trace_internal.h>
#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_spinlock.h>

#include "eal_private.h"
#include "eal_alarm_private.h"

#define NS_PER_US 1000

#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE_ID CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE_ID CLOCK_MONOTONIC
#endif

struct alarm_entry {
	LIST_ENTRY(alarm_entry) next;
	struct timespec time;
	rte_eal_alarm_callback cb_fn;
	void *cb_arg;
	volatile uint8_t executing;
	volatile pthread_t executing_id;
};

static LIST_HEAD(alarm_list, alarm_entry) alarm_list = LIST_HEAD_INITIALIZER();
static rte_spinlock_t alarm_list_lk = RTE_SPINLOCK_INITIALIZER;

static struct rte_intr_handle *intr_handle;
static void eal_alarm_callback(void *arg);

void
rte_eal_alarm_cleanup(void)
{
	rte_intr_instance_free(intr_handle);
}

int
rte_eal_alarm_init(void)
{
	int fd;

	intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (intr_handle == NULL) {
		RTE_LOG(ERR, EAL, "Fail to allocate intr_handle\n");
		goto error;
	}

	if (rte_intr_type_set(intr_handle, RTE_INTR_HANDLE_ALARM))
		goto error;

	if (rte_intr_fd_set(intr_handle, -1))
		goto error;

	/* on FreeBSD, timers don't use fd's, and their identifiers are stored
	 * in separate namespace from fd's, so using any value is OK. however,
	 * EAL interrupts handler expects fd's to be unique, so use an actual fd
	 * to guarantee unique timer identifier.
	 */
	fd = open("/dev/zero", O_RDONLY);

	if (rte_intr_fd_set(intr_handle, fd))
		goto error;

	return 0;
error:
	rte_intr_instance_free(intr_handle);
	return -1;
}

static inline int
timespec_cmp(const struct timespec *now, const struct timespec *at)
{
	if (now->tv_sec < at->tv_sec)
		return -1;
	if (now->tv_sec > at->tv_sec)
		return 1;
	if (now->tv_nsec < at->tv_nsec)
		return -1;
	if (now->tv_nsec > at->tv_nsec)
		return 1;
	return 0;
}

static inline uint64_t
diff_ns(struct timespec *now, struct timespec *at)
{
	uint64_t now_ns, at_ns;

	if (timespec_cmp(now, at) >= 0)
		return 0;

	now_ns = now->tv_sec * NS_PER_S + now->tv_nsec;
	at_ns = at->tv_sec * NS_PER_S + at->tv_nsec;

	return at_ns - now_ns;
}

int
eal_alarm_get_timeout_ns(uint64_t *val)
{
	struct alarm_entry *ap;
	struct timespec now;

	if (clock_gettime(CLOCK_TYPE_ID, &now) < 0)
		return -1;

	if (LIST_EMPTY(&alarm_list))
		return -1;

	ap = LIST_FIRST(&alarm_list);

	*val = diff_ns(&now, &ap->time);

	return 0;
}

static int
unregister_current_callback(void)
{
	struct alarm_entry *ap;
	int ret = 0;

	if (!LIST_EMPTY(&alarm_list)) {
		ap = LIST_FIRST(&alarm_list);

		do {
			ret = rte_intr_callback_unregister(intr_handle,
				eal_alarm_callback, &ap->time);
		} while (ret == -EAGAIN);
	}

	return ret;
}

static int
register_first_callback(void)
{
	struct alarm_entry *ap;
	int ret = 0;

	if (!LIST_EMPTY(&alarm_list)) {
		ap = LIST_FIRST(&alarm_list);

		/* register a new callback */
		ret = rte_intr_callback_register(intr_handle,
				eal_alarm_callback, &ap->time);
	}
	return ret;
}

static void
eal_alarm_callback(void *arg __rte_unused)
{
	struct timespec now;
	struct alarm_entry *ap;

	if (clock_gettime(CLOCK_TYPE_ID, &now) < 0)
		return;

	rte_spinlock_lock(&alarm_list_lk);
	ap = LIST_FIRST(&alarm_list);

	while (ap != NULL && timespec_cmp(&now, &ap->time) >= 0) {
		ap->executing = 1;
		ap->executing_id = pthread_self();
		rte_spinlock_unlock(&alarm_list_lk);

		ap->cb_fn(ap->cb_arg);

		rte_spinlock_lock(&alarm_list_lk);

		LIST_REMOVE(ap, next);
		free(ap);

		ap = LIST_FIRST(&alarm_list);
	}

	/* timer has been deleted from the kqueue, so recreate it if needed */
	register_first_callback();

	rte_spinlock_unlock(&alarm_list_lk);
}


int
rte_eal_alarm_set(uint64_t us, rte_eal_alarm_callback cb_fn, void *cb_arg)
{
	struct alarm_entry *ap, *new_alarm;
	struct timespec now;
	uint64_t ns;
	int ret = 0;

	/* check parameters, also ensure us won't cause a uint64_t overflow */
	if (us < 1 || us > (UINT64_MAX - US_PER_S) || cb_fn == NULL)
		return -EINVAL;

	new_alarm = calloc(1, sizeof(*new_alarm));
	if (new_alarm == NULL)
		return -ENOMEM;

	/* use current time to calculate absolute time of alarm */
	clock_gettime(CLOCK_TYPE_ID, &now);

	ns = us * NS_PER_US;

	new_alarm->cb_fn = cb_fn;
	new_alarm->cb_arg = cb_arg;
	new_alarm->time.tv_nsec = (now.tv_nsec + ns) % NS_PER_S;
	new_alarm->time.tv_sec = now.tv_sec + ((now.tv_nsec + ns) / NS_PER_S);

	rte_spinlock_lock(&alarm_list_lk);

	if (LIST_EMPTY(&alarm_list))
		LIST_INSERT_HEAD(&alarm_list, new_alarm, next);
	else {
		LIST_FOREACH(ap, &alarm_list, next) {
			if (timespec_cmp(&new_alarm->time, &ap->time) < 0) {
				LIST_INSERT_BEFORE(ap, new_alarm, next);
				break;
			}
			if (LIST_NEXT(ap, next) == NULL) {
				LIST_INSERT_AFTER(ap, new_alarm, next);
				break;
			}
		}
	}

	/* re-register first callback just in case */
	register_first_callback();

	rte_spinlock_unlock(&alarm_list_lk);

	rte_eal_trace_alarm_set(us, cb_fn, cb_arg, ret);
	return ret;
}

int
rte_eal_alarm_cancel(rte_eal_alarm_callback cb_fn, void *cb_arg)
{
	struct alarm_entry *ap, *ap_prev;
	int count = 0;
	int err = 0;
	int executing;

	if (!cb_fn) {
		rte_errno = EINVAL;
		return -1;
	}

	do {
		executing = 0;
		rte_spinlock_lock(&alarm_list_lk);
		/* remove any matches at the start of the list */
		while (1) {
			ap = LIST_FIRST(&alarm_list);
			if (ap == NULL)
				break;
			if (cb_fn != ap->cb_fn)
				break;
			if (cb_arg != ap->cb_arg && cb_arg != (void *) -1)
				break;
			if (ap->executing == 0) {
				LIST_REMOVE(ap, next);
				free(ap);
				count++;
			} else {
				/* If calling from other context, mark that
				 * alarm is executing so loop can spin till it
				 * finish. Otherwise we are trying to cancel
				 * ourselves - mark it by EINPROGRESS.
				 */
				if (pthread_equal(ap->executing_id,
						pthread_self()) == 0)
					executing++;
				else
					err = EINPROGRESS;

				break;
			}
		}
		ap_prev = ap;

		/* now go through list, removing entries not at start */
		LIST_FOREACH(ap, &alarm_list, next) {
			/* this won't be true first time through */
			if (cb_fn == ap->cb_fn &&
					(cb_arg == (void *)-1 ||
					 cb_arg == ap->cb_arg)) {
				if (ap->executing == 0) {
					LIST_REMOVE(ap, next);
					free(ap);
					count++;
					ap = ap_prev;
				} else if (pthread_equal(ap->executing_id,
							 pthread_self()) == 0) {
					executing++;
				} else {
					err = EINPROGRESS;
				}
			}
			ap_prev = ap;
		}
		rte_spinlock_unlock(&alarm_list_lk);
	} while (executing != 0);

	if (count == 0 && err == 0)
		rte_errno = ENOENT;
	else if (err)
		rte_errno = err;

	rte_spinlock_lock(&alarm_list_lk);

	/* unregister if no alarms left, otherwise re-register first */
	if (LIST_EMPTY(&alarm_list))
		unregister_current_callback();
	else
		register_first_callback();

	rte_spinlock_unlock(&alarm_list_lk);

	rte_eal_trace_alarm_cancel(cb_fn, cb_arg, count);
	return count;
}
