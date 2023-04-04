/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_alarm.h>
#include <rte_spinlock.h>

#include <eal_trace_internal.h>
#include "eal_windows.h"

enum alarm_state {
	ALARM_ARMED,
	ALARM_TRIGGERED,
	ALARM_CANCELLED
};

struct alarm_entry {
	LIST_ENTRY(alarm_entry) next;
	rte_eal_alarm_callback cb_fn;
	void *cb_arg;
	HANDLE timer;
	atomic_uint state;
};

static LIST_HEAD(alarm_list, alarm_entry) alarm_list = LIST_HEAD_INITIALIZER();

static rte_spinlock_t alarm_lock = RTE_SPINLOCK_INITIALIZER;

static int intr_thread_exec_sync(void (*func)(void *arg), void *arg);

static void
alarm_remove_unsafe(struct alarm_entry *ap)
{
	LIST_REMOVE(ap, next);
	CloseHandle(ap->timer);
	free(ap);
}

static void
alarm_callback(void *arg, DWORD low __rte_unused, DWORD high __rte_unused)
{
	struct alarm_entry *ap = arg;
	unsigned int state = ALARM_ARMED;

	if (!atomic_compare_exchange_strong(
			&ap->state, &state, ALARM_TRIGGERED))
		return;

	ap->cb_fn(ap->cb_arg);

	rte_spinlock_lock(&alarm_lock);
	alarm_remove_unsafe(ap);
	rte_spinlock_unlock(&alarm_lock);
}

static int
alarm_set(struct alarm_entry *entry, LARGE_INTEGER deadline)
{
	BOOL ret = SetWaitableTimer(
		entry->timer, &deadline, 0, alarm_callback, entry, FALSE);
	if (!ret) {
		RTE_LOG_WIN32_ERR("SetWaitableTimer");
		return -1;
	}
	return 0;
}

struct alarm_task {
	struct alarm_entry *entry;
	LARGE_INTEGER deadline;
	int ret;
};

static void
alarm_task_exec(void *arg)
{
	struct alarm_task *task = arg;
	task->ret = alarm_set(task->entry, task->deadline);
}

int
rte_eal_alarm_set(uint64_t us, rte_eal_alarm_callback cb_fn, void *cb_arg)
{
	struct alarm_entry *ap;
	HANDLE timer;
	FILETIME ft;
	LARGE_INTEGER deadline;
	int ret;

	if (cb_fn == NULL) {
		RTE_LOG(ERR, EAL, "NULL callback\n");
		ret = -EINVAL;
		goto exit;
	}

	/* Calculate deadline ASAP, unit of measure = 100ns. */
	GetSystemTimePreciseAsFileTime(&ft);
	deadline.LowPart = ft.dwLowDateTime;
	deadline.HighPart = ft.dwHighDateTime;
	deadline.QuadPart += 10 * us;

	ap = calloc(1, sizeof(*ap));
	if (ap == NULL) {
		RTE_LOG(ERR, EAL, "Cannot allocate alarm entry\n");
		ret = -ENOMEM;
		goto exit;
	}

	timer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (timer == NULL) {
		RTE_LOG_WIN32_ERR("CreateWaitableTimer()");
		ret = -EINVAL;
		goto fail;
	}

	ap->timer = timer;
	ap->cb_fn = cb_fn;
	ap->cb_arg = cb_arg;

	/* Waitable timer must be set in the same thread that will
	 * do an alertable wait for the alarm to trigger, that is,
	 * in the interrupt thread.
	 */
	if (rte_thread_is_intr()) {
		/* Directly schedule callback execution. */
		ret = alarm_set(ap, deadline);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Cannot setup alarm\n");
			goto fail;
		}
	} else {
		/* Dispatch a task to set alarm into the interrupt thread.
		 * Execute it synchronously, because it can fail.
		 */
		struct alarm_task task = {
			.entry = ap,
			.deadline = deadline,
		};

		ret = intr_thread_exec_sync(alarm_task_exec, &task);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Cannot setup alarm in interrupt thread\n");
			goto fail;
		}

		ret = task.ret;
		if (ret < 0)
			goto fail;
	}

	rte_spinlock_lock(&alarm_lock);
	LIST_INSERT_HEAD(&alarm_list, ap, next);
	rte_spinlock_unlock(&alarm_lock);

	goto exit;

fail:
	if (timer != NULL)
		CloseHandle(timer);
	free(ap);

exit:
	rte_eal_trace_alarm_set(us, cb_fn, cb_arg, ret);
	return ret;
}

static bool
alarm_matches(const struct alarm_entry *ap,
	rte_eal_alarm_callback cb_fn, void *cb_arg)
{
	bool any_arg = cb_arg == (void *)(-1);
	return (ap->cb_fn == cb_fn) && (any_arg || ap->cb_arg == cb_arg);
}

int
rte_eal_alarm_cancel(rte_eal_alarm_callback cb_fn, void *cb_arg)
{
	struct alarm_entry *ap;
	unsigned int state;
	int removed;
	bool executing;

	removed = 0;

	if (cb_fn == NULL) {
		RTE_LOG(ERR, EAL, "NULL callback\n");
		return -EINVAL;
	}

	do {
		executing = false;

		rte_spinlock_lock(&alarm_lock);

		LIST_FOREACH(ap, &alarm_list, next) {
			if (!alarm_matches(ap, cb_fn, cb_arg))
				continue;

			state = ALARM_ARMED;
			if (atomic_compare_exchange_strong(
					&ap->state, &state, ALARM_CANCELLED)) {
				alarm_remove_unsafe(ap);
				removed++;
			} else if (state == ALARM_TRIGGERED)
				executing = true;
		}

		rte_spinlock_unlock(&alarm_lock);
	} while (executing);

	rte_eal_trace_alarm_cancel(cb_fn, cb_arg, removed);
	return removed;
}

struct intr_task {
	void (*func)(void *arg);
	void *arg;
	rte_spinlock_t lock; /* unlocked at task completion */
};

static void
intr_thread_entry(void *arg)
	__rte_no_thread_safety_analysis
{
	struct intr_task *task = arg;
	task->func(task->arg);
	rte_spinlock_unlock(&task->lock);
}

static int
intr_thread_exec_sync(void (*func)(void *arg), void *arg)
	__rte_no_thread_safety_analysis
{
	struct intr_task task;
	int ret;

	task.func = func;
	task.arg = arg;
	rte_spinlock_init(&task.lock);

	/* Make timers more precise by synchronizing in userspace. */
	rte_spinlock_lock(&task.lock);
	ret = eal_intr_thread_schedule(intr_thread_entry, &task);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Cannot schedule task to interrupt thread\n");
		return -EINVAL;
	}

	/* Wait for the task to complete. */
	rte_spinlock_lock(&task.lock);
	return 0;
}
