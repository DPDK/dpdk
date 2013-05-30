/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>
#include <inttypes.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_spinlock.h>

#include "rte_timer.h"

LIST_HEAD(rte_timer_list, rte_timer);

struct priv_timer {
	struct rte_timer_list pending;  /**< list of pending timers */
	struct rte_timer_list expired;  /**< list of expired timers */
	struct rte_timer_list done;     /**< list of done timers */
	rte_spinlock_t list_lock;       /**< lock to protect list access */

	/** per-core variable that true if a timer was updated on this
	 *  core since last reset of the variable */
	int updated;

	unsigned prev_lcore;              /**< used for lcore round robin */

#ifdef RTE_LIBRTE_TIMER_DEBUG
	/** per-lcore statistics */
	struct rte_timer_debug_stats stats;
#endif
} __rte_cache_aligned;

/** per-lcore private info for timers */
static struct priv_timer priv_timer[RTE_MAX_LCORE];

/* when debug is enabled, store some statistics */
#ifdef RTE_LIBRTE_TIMER_DEBUG
#define __TIMER_STAT_ADD(name, n) do {				\
		unsigned __lcore_id = rte_lcore_id();		\
		priv_timer[__lcore_id].stats.name += (n);	\
	} while(0)
#else
#define __TIMER_STAT_ADD(name, n) do {} while(0)
#endif

/* this macro allow to modify var while browsing the list */
#define LIST_FOREACH_SAFE(var, var2, head, field)		       \
	for ((var) = ((head)->lh_first),			       \
		     (var2) = ((var) ? ((var)->field.le_next) : NULL); \
	     (var);						       \
	     (var) = (var2),					       \
		     (var2) = ((var) ? ((var)->field.le_next) : NULL))


/* Init the timer library. */
void
rte_timer_subsystem_init(void)
{
	unsigned lcore_id;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id ++) {
		LIST_INIT(&priv_timer[lcore_id].pending);
		LIST_INIT(&priv_timer[lcore_id].expired);
		LIST_INIT(&priv_timer[lcore_id].done);
		rte_spinlock_init(&priv_timer[lcore_id].list_lock);
		priv_timer[lcore_id].prev_lcore = lcore_id;
	}
}

/* Initialize the timer handle tim for use */
void
rte_timer_init(struct rte_timer *tim)
{
	union rte_timer_status status;

	status.state = RTE_TIMER_STOP;
	status.owner = RTE_TIMER_NO_OWNER;
	tim->status.u32 = status.u32;
}

/*
 * if timer is pending or stopped (or running on the same core than
 * us), mark timer as configuring, and on success return the previous
 * status of the timer
 */
static int
timer_set_config_state(struct rte_timer *tim,
		       union rte_timer_status *ret_prev_status)
{
	union rte_timer_status prev_status, status;
	int success = 0;
	unsigned lcore_id;

	lcore_id = rte_lcore_id();

	/* wait that the timer is in correct status before update,
	 * and mark it as beeing configured */
	while (success == 0) {
		prev_status.u32 = tim->status.u32;

		/* timer is running on another core, exit */
		if (prev_status.state == RTE_TIMER_RUNNING &&
		    (unsigned)prev_status.owner != lcore_id)
			return -1;

		/* timer is beeing configured on another core */
		if (prev_status.state == RTE_TIMER_CONFIG)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as beeing configured */
		status.state = RTE_TIMER_CONFIG;
		status.owner = (int16_t)lcore_id;
		success = rte_atomic32_cmpset(&tim->status.u32,
					      prev_status.u32,
					      status.u32);
	}

	ret_prev_status->u32 = prev_status.u32;
	return 0;
}

/*
 * if timer is pending, mark timer as running
 */
static int
timer_set_running_state(struct rte_timer *tim)
{
	union rte_timer_status prev_status, status;
	unsigned lcore_id = rte_lcore_id();
	int success = 0;

	/* wait that the timer is in correct status before update,
	 * and mark it as running */
	while (success == 0) {
		prev_status.u32 = tim->status.u32;

		/* timer is not pending anymore */
		if (prev_status.state != RTE_TIMER_PENDING)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as beeing configured */
		status.state = RTE_TIMER_RUNNING;
		status.owner = (int16_t)lcore_id;
		success = rte_atomic32_cmpset(&tim->status.u32,
					      prev_status.u32,
					      status.u32);
	}

	return 0;
}

/*
 * add in list, lock if needed
 * timer must be in config state
 * timer must not be in a list
 */
static void
timer_add(struct rte_timer *tim, unsigned tim_lcore, int local_is_locked)
{
	uint64_t cur_time = rte_get_hpet_cycles();
	unsigned lcore_id = rte_lcore_id();
	struct rte_timer *t, *t_prev;

	/* if timer needs to be scheduled on another core, we need to
	 * lock the list; if it is on local core, we need to lock if
	 * we are not called from rte_timer_manage() */
	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_lock(&priv_timer[tim_lcore].list_lock);

	t = LIST_FIRST(&priv_timer[tim_lcore].pending);

	/* list is empty or 'tim' will expire before 't' */
	if (t == NULL || ((int64_t)(tim->expire - cur_time) <
			  (int64_t)(t->expire - cur_time))) {
		LIST_INSERT_HEAD(&priv_timer[tim_lcore].pending, tim, next);
	}
	else {
		t_prev = t;

		/* find an element that will expire after 'tim' */
		LIST_FOREACH(t, &priv_timer[tim_lcore].pending, next) {
			if ((int64_t)(tim->expire - cur_time) <
			    (int64_t)(t->expire - cur_time)) {
				LIST_INSERT_BEFORE(t, tim, next);
				break;
			}
			t_prev = t;
		}

		/* not found, insert at the end of the list */
		if (t == NULL)
			LIST_INSERT_AFTER(t_prev, tim, next);
	}

	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_unlock(&priv_timer[tim_lcore].list_lock);
}

/*
 * del from list, lock if needed
 * timer must be in config state
 * timer must be in a list
 */
static void
timer_del(struct rte_timer *tim, unsigned prev_owner, int local_is_locked)
{
	unsigned lcore_id = rte_lcore_id();

	/* if timer needs is pending another core, we need to lock the
	 * list; if it is on local core, we need to lock if we are not
	 * called from rte_timer_manage() */
	if (prev_owner != lcore_id || !local_is_locked)
		rte_spinlock_lock(&priv_timer[prev_owner].list_lock);

	LIST_REMOVE(tim, next);

	if (prev_owner != lcore_id || !local_is_locked)
		rte_spinlock_unlock(&priv_timer[prev_owner].list_lock);
}

/* Reset and start the timer associated with the timer handle (private func) */
static int
__rte_timer_reset(struct rte_timer *tim, uint64_t expire,
		  uint64_t period, unsigned tim_lcore,
		  rte_timer_cb_t fct, void *arg,
		  int local_is_locked)
{
	union rte_timer_status prev_status, status;
	int ret;
	unsigned lcore_id = rte_lcore_id();

	/* round robin for tim_lcore */
	if (tim_lcore == (unsigned)LCORE_ID_ANY) {
		tim_lcore = rte_get_next_lcore(priv_timer[lcore_id].prev_lcore,
					       0, 1);
		priv_timer[lcore_id].prev_lcore = tim_lcore;
	}

	/* wait that the timer is in correct status before update,
	 * and mark it as beeing configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(reset, 1);
	priv_timer[lcore_id].updated = 1;

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING ||
	    prev_status.state == RTE_TIMER_RUNNING) {
		timer_del(tim, prev_status.owner, local_is_locked);
		__TIMER_STAT_ADD(pending, -1);
	}

	tim->period = period;
	tim->expire = expire;
	tim->f = fct;
	tim->arg = arg;

	__TIMER_STAT_ADD(pending, 1);
	timer_add(tim, tim_lcore, local_is_locked);

	/* update state: as we are in CONFIG state, only us can modify
	 * the state so we don't need to use cmpset() here */
	rte_wmb();
	status.state = RTE_TIMER_PENDING;
	status.owner = (int16_t)tim_lcore;
	tim->status.u32 = status.u32;

	return 0;
}

/* Reset and start the timer associated with the timer handle tim */
int
rte_timer_reset(struct rte_timer *tim, uint64_t ticks,
		enum rte_timer_type type, unsigned tim_lcore,
		rte_timer_cb_t fct, void *arg)
{
	uint64_t cur_time = rte_get_hpet_cycles();
	uint64_t period;

	if (unlikely((tim_lcore != (unsigned)LCORE_ID_ANY) &&
			!rte_lcore_is_enabled(tim_lcore)))
		return -1;

	if (type == PERIODICAL)
		period = ticks;
	else
		period = 0;

	__rte_timer_reset(tim,  cur_time + ticks, period, tim_lcore,
			  fct, arg, 0);

	return 0;
}

/* loop until rte_timer_reset() succeed */
void
rte_timer_reset_sync(struct rte_timer *tim, uint64_t ticks,
		     enum rte_timer_type type, unsigned tim_lcore,
		     rte_timer_cb_t fct, void *arg)
{
	while (rte_timer_reset(tim, ticks, type, tim_lcore,
			       fct, arg) != 0);
}

/* Stop the timer associated with the timer handle tim */
int
rte_timer_stop(struct rte_timer *tim)
{
	union rte_timer_status prev_status, status;
	unsigned lcore_id = rte_lcore_id();
	int ret;

	/* wait that the timer is in correct status before update,
	 * and mark it as beeing configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(stop, 1);
	priv_timer[lcore_id].updated = 1;

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING ||
	    prev_status.state == RTE_TIMER_RUNNING) {
		timer_del(tim, prev_status.owner, 0);
		__TIMER_STAT_ADD(pending, -1);
	}

	/* mark timer as stopped */
	rte_wmb();
	status.state = RTE_TIMER_STOP;
	status.owner = RTE_TIMER_NO_OWNER;
	tim->status.u32 = status.u32;

	return 0;
}

/* loop until rte_timer_stop() succeed */
void
rte_timer_stop_sync(struct rte_timer *tim)
{
	while (rte_timer_stop(tim) != 0);
}

/* Test the PENDING status of the timer handle tim */
int
rte_timer_pending(struct rte_timer *tim)
{
	return tim->status.state == RTE_TIMER_PENDING;
}

/* must be called periodically, run all timer that expired */
void rte_timer_manage(void)
{
	union rte_timer_status status;
	struct rte_timer *tim, *tim2;
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time;
	int ret;

	/* optimize for the case where per-cpu list is empty */
	if (LIST_EMPTY(&priv_timer[lcore_id].pending))
		return;

	cur_time = rte_get_hpet_cycles();
	__TIMER_STAT_ADD(manage, 1);

	/* browse ordered list, add expired timers in 'expired' list */
	rte_spinlock_lock(&priv_timer[lcore_id].list_lock);

	LIST_FOREACH_SAFE(tim, tim2, &priv_timer[lcore_id].pending, next) {
		if ((int64_t)(cur_time - tim->expire) < 0)
			break;

		LIST_REMOVE(tim, next);
		LIST_INSERT_HEAD(&priv_timer[lcore_id].expired, tim, next);
	}


	/* for each timer of 'expired' list, check state and execute callback */
	while ((tim = LIST_FIRST(&priv_timer[lcore_id].expired)) != NULL) {
		ret = timer_set_running_state(tim);

		/* remove from expired list, and add it in done list */
		LIST_REMOVE(tim, next);
		LIST_INSERT_HEAD(&priv_timer[lcore_id].done, tim, next);

		/* this timer was not pending, continue */
		if (ret < 0)
			continue;

		rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);

		priv_timer[lcore_id].updated = 0;

		/* execute callback function with list unlocked */
		tim->f(tim, tim->arg);

		rte_spinlock_lock(&priv_timer[lcore_id].list_lock);

		/* the timer was stopped or reloaded by the callback
		 * function, we have nothing to do here */
		if (priv_timer[lcore_id].updated == 1)
			continue;

		if (tim->period == 0) {
			/* remove from done list and mark timer as stopped */
			LIST_REMOVE(tim, next);
			__TIMER_STAT_ADD(pending, -1);
			status.state = RTE_TIMER_STOP;
			status.owner = RTE_TIMER_NO_OWNER;
			rte_wmb();
			tim->status.u32 = status.u32;
		}
		else {
			/* keep it in done list and mark timer as pending */
			status.state = RTE_TIMER_PENDING;
			status.owner = (int16_t)lcore_id;
			rte_wmb();
			tim->status.u32 = status.u32;
		}
	}

	/* finally, browse done list, some timer may have to be
	 * rescheduled automatically */
	LIST_FOREACH_SAFE(tim, tim2, &priv_timer[lcore_id].done, next) {

		/* reset may fail if timer is beeing modified, in this
		 * case the timer will remain in 'done' list until the
		 * core that is modifying it remove it */
		__rte_timer_reset(tim, cur_time + tim->period,
				  tim->period, lcore_id, tim->f,
				  tim->arg, 1);
	}

	/* job finished, unlock the list lock */
	rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);
}

/* dump statistics about timers */
void rte_timer_dump_stats(void)
{
#ifdef RTE_LIBRTE_TIMER_DEBUG
	struct rte_timer_debug_stats sum;
	unsigned lcore_id;

	memset(&sum, 0, sizeof(sum));
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		sum.reset += priv_timer[lcore_id].stats.reset;
		sum.stop += priv_timer[lcore_id].stats.stop;
		sum.manage += priv_timer[lcore_id].stats.manage;
		sum.pending += priv_timer[lcore_id].stats.pending;
	}
	printf("Timer statistics:\n");
	printf("  reset = %"PRIu64"\n", sum.reset);
	printf("  stop = %"PRIu64"\n", sum.stop);
	printf("  manage = %"PRIu64"\n", sum.manage);
	printf("  pending = %"PRIu64"\n", sum.pending);
#else
	printf("No timer statistics, RTE_LIBRTE_TIMER_DEBUG is disabled\n");
#endif
}
