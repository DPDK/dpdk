/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/queue.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_spinlock.h>
#include <rte_random.h>

#include "rte_timer.h"

LIST_HEAD(rte_timer_list, rte_timer);

struct priv_timer {
	struct rte_timer pending_head;  /**< dummy timer instance to head up list */
	rte_spinlock_t list_lock;       /**< lock to protect list access */

	/** per-core variable that true if a timer was updated on this
	 *  core since last reset of the variable */
	int updated;

	/** track the current depth of the skiplist */
	unsigned curr_skiplist_depth;

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
#define __TIMER_STAT_ADD(name, n) do {					\
		unsigned __lcore_id = rte_lcore_id();			\
		if (__lcore_id < RTE_MAX_LCORE)				\
			priv_timer[__lcore_id].stats.name += (n);	\
	} while(0)
#else
#define __TIMER_STAT_ADD(name, n) do {} while(0)
#endif

/* Init the timer library. */
void
rte_timer_subsystem_init(void)
{
	unsigned lcore_id;

	/* since priv_timer is static, it's zeroed by default, so only init some
	 * fields.
	 */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id ++) {
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
	 * and mark it as being configured */
	while (success == 0) {
		prev_status.u32 = tim->status.u32;

		/* timer is running on another core, exit */
		if (prev_status.state == RTE_TIMER_RUNNING &&
		    prev_status.owner != (uint16_t)lcore_id)
			return -1;

		/* timer is being configured on another core */
		if (prev_status.state == RTE_TIMER_CONFIG)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as being configured */
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
 * Return a skiplist level for a new entry.
 * This probabalistically gives a level with p=1/4 that an entry at level n
 * will also appear at level n+1.
 */
static uint32_t
timer_get_skiplist_level(unsigned curr_depth)
{
#ifdef RTE_LIBRTE_TIMER_DEBUG
	static uint32_t i, count = 0;
	static uint32_t levels[MAX_SKIPLIST_DEPTH] = {0};
#endif

	/* probability value is 1/4, i.e. all at level 0, 1 in 4 is at level 1,
	 * 1 in 16 at level 2, 1 in 64 at level 3, etc. Calculated using lowest
	 * bit position of a (pseudo)random number.
	 */
	uint32_t rand = rte_rand() & (UINT32_MAX - 1);
	uint32_t level = rand == 0 ? MAX_SKIPLIST_DEPTH : (rte_bsf32(rand)-1) / 2;

	/* limit the levels used to one above our current level, so we don't,
	 * for instance, have a level 0 and a level 7 without anything between
	 */
	if (level > curr_depth)
		level = curr_depth;
	if (level >= MAX_SKIPLIST_DEPTH)
		level = MAX_SKIPLIST_DEPTH-1;
#ifdef RTE_LIBRTE_TIMER_DEBUG
	count ++;
	levels[level]++;
	if (count % 10000 == 0)
		for (i = 0; i < MAX_SKIPLIST_DEPTH; i++)
			printf("Level %u: %u\n", (unsigned)i, (unsigned)levels[i]);
#endif
	return level;
}

/*
 * For a given time value, get the entries at each level which
 * are <= that time value.
 */
static void
timer_get_prev_entries(uint64_t time_val, unsigned tim_lcore,
		struct rte_timer **prev)
{
	unsigned lvl = priv_timer[tim_lcore].curr_skiplist_depth;
	prev[lvl] = &priv_timer[tim_lcore].pending_head;
	while(lvl != 0) {
		lvl--;
		prev[lvl] = prev[lvl+1];
		while (prev[lvl]->sl_next[lvl] &&
				prev[lvl]->sl_next[lvl]->expire <= time_val)
			prev[lvl] = prev[lvl]->sl_next[lvl];
	}
}

/*
 * Given a timer node in the skiplist, find the previous entries for it at
 * all skiplist levels.
 */
static void
timer_get_prev_entries_for_node(struct rte_timer *tim, unsigned tim_lcore,
		struct rte_timer **prev)
{
	int i;
	/* to get a specific entry in the list, look for just lower than the time
	 * values, and then increment on each level individually if necessary
	 */
	timer_get_prev_entries(tim->expire - 1, tim_lcore, prev);
	for (i = priv_timer[tim_lcore].curr_skiplist_depth - 1; i >= 0; i--) {
		while (prev[i]->sl_next[i] != NULL &&
				prev[i]->sl_next[i] != tim &&
				prev[i]->sl_next[i]->expire <= tim->expire)
			prev[i] = prev[i]->sl_next[i];
	}
}

/*
 * add in list, lock if needed
 * timer must be in config state
 * timer must not be in a list
 */
static void
timer_add(struct rte_timer *tim, unsigned tim_lcore, int local_is_locked)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned lvl;
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH+1];

	/* if timer needs to be scheduled on another core, we need to
	 * lock the list; if it is on local core, we need to lock if
	 * we are not called from rte_timer_manage() */
	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_lock(&priv_timer[tim_lcore].list_lock);

	/* find where exactly this element goes in the list of elements
	 * for each depth. */
	timer_get_prev_entries(tim->expire, tim_lcore, prev);

	/* now assign it a new level and add at that level */
	const unsigned tim_level = timer_get_skiplist_level(
			priv_timer[tim_lcore].curr_skiplist_depth);
	if (tim_level == priv_timer[tim_lcore].curr_skiplist_depth)
		priv_timer[tim_lcore].curr_skiplist_depth++;

	lvl = tim_level;
	while (lvl > 0) {
		tim->sl_next[lvl] = prev[lvl]->sl_next[lvl];
		prev[lvl]->sl_next[lvl] = tim;
		lvl--;
	}
	tim->sl_next[0] = prev[0]->sl_next[0];
	prev[0]->sl_next[0] = tim;

	/* save the lowest list entry into the expire field of the dummy hdr
	 * NOTE: this is not atomic on 32-bit*/
	priv_timer[tim_lcore].pending_head.expire = priv_timer[tim_lcore].\
			pending_head.sl_next[0]->expire;

	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_unlock(&priv_timer[tim_lcore].list_lock);
}

/*
 * del from list, lock if needed
 * timer must be in config state
 * timer must be in a list
 */
static void
timer_del(struct rte_timer *tim, union rte_timer_status prev_status,
		int local_is_locked)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned prev_owner = prev_status.owner;
	int i;
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH+1];

	/* if timer needs is pending another core, we need to lock the
	 * list; if it is on local core, we need to lock if we are not
	 * called from rte_timer_manage() */
	if (prev_owner != lcore_id || !local_is_locked)
		rte_spinlock_lock(&priv_timer[prev_owner].list_lock);

	/* save the lowest list entry into the expire field of the dummy hdr.
	 * NOTE: this is not atomic on 32-bit */
	if (tim == priv_timer[prev_owner].pending_head.sl_next[0])
		priv_timer[prev_owner].pending_head.expire =
				((tim->sl_next[0] == NULL) ? 0 : tim->sl_next[0]->expire);

	/* adjust pointers from previous entries to point past this */
	timer_get_prev_entries_for_node(tim, prev_owner, prev);
	for (i = priv_timer[prev_owner].curr_skiplist_depth - 1; i >= 0; i--) {
		if (prev[i]->sl_next[i] == tim)
			prev[i]->sl_next[i] = tim->sl_next[i];
	}

	/* in case we deleted last entry at a level, adjust down max level */
	for (i = priv_timer[prev_owner].curr_skiplist_depth - 1; i >= 0; i--)
		if (priv_timer[prev_owner].pending_head.sl_next[i] == NULL)
			priv_timer[prev_owner].curr_skiplist_depth --;
		else
			break;

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
		if (lcore_id < RTE_MAX_LCORE) {
			/* EAL thread with valid lcore_id */
			tim_lcore = rte_get_next_lcore(
				priv_timer[lcore_id].prev_lcore,
				0, 1);
			priv_timer[lcore_id].prev_lcore = tim_lcore;
		} else
			/* non-EAL thread do not run rte_timer_manage(),
			 * so schedule the timer on the first enabled lcore. */
			tim_lcore = rte_get_next_lcore(LCORE_ID_ANY, 0, 1);
	}

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(reset, 1);
	if (prev_status.state == RTE_TIMER_RUNNING &&
	    lcore_id < RTE_MAX_LCORE) {
		priv_timer[lcore_id].updated = 1;
	}

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING) {
		timer_del(tim, prev_status, local_is_locked);
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
	uint64_t cur_time = rte_get_timer_cycles();
	uint64_t period;

	if (unlikely((tim_lcore != (unsigned)LCORE_ID_ANY) &&
			!rte_lcore_is_enabled(tim_lcore)))
		return -1;

	if (type == PERIODICAL)
		period = ticks;
	else
		period = 0;

	return __rte_timer_reset(tim,  cur_time + ticks, period, tim_lcore,
			  fct, arg, 0);
}

/* loop until rte_timer_reset() succeed */
void
rte_timer_reset_sync(struct rte_timer *tim, uint64_t ticks,
		     enum rte_timer_type type, unsigned tim_lcore,
		     rte_timer_cb_t fct, void *arg)
{
	while (rte_timer_reset(tim, ticks, type, tim_lcore,
			       fct, arg) != 0)
		rte_pause();
}

/* Stop the timer associated with the timer handle tim */
int
rte_timer_stop(struct rte_timer *tim)
{
	union rte_timer_status prev_status, status;
	unsigned lcore_id = rte_lcore_id();
	int ret;

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(stop, 1);
	if (prev_status.state == RTE_TIMER_RUNNING &&
	    lcore_id < RTE_MAX_LCORE) {
		priv_timer[lcore_id].updated = 1;
	}

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING) {
		timer_del(tim, prev_status, 0);
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
	while (rte_timer_stop(tim) != 0)
		rte_pause();
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
	struct rte_timer *tim, *next_tim;
	unsigned lcore_id = rte_lcore_id();
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH + 1];
	uint64_t cur_time;
	int i, ret;

	/* timer manager only runs on EAL thread with valid lcore_id */
	assert(lcore_id < RTE_MAX_LCORE);

	__TIMER_STAT_ADD(manage, 1);
	/* optimize for the case where per-cpu list is empty */
	if (priv_timer[lcore_id].pending_head.sl_next[0] == NULL)
		return;
	cur_time = rte_get_timer_cycles();

#ifdef RTE_ARCH_X86_64
	/* on 64-bit the value cached in the pending_head.expired will be updated
	 * atomically, so we can consult that for a quick check here outside the
	 * lock */
	if (likely(priv_timer[lcore_id].pending_head.expire > cur_time))
		return;
#endif

	/* browse ordered list, add expired timers in 'expired' list */
	rte_spinlock_lock(&priv_timer[lcore_id].list_lock);

	/* if nothing to do just unlock and return */
	if (priv_timer[lcore_id].pending_head.sl_next[0] == NULL ||
			priv_timer[lcore_id].pending_head.sl_next[0]->expire > cur_time)
		goto done;

	/* save start of list of expired timers */
	tim = priv_timer[lcore_id].pending_head.sl_next[0];

	/* break the existing list at current time point */
	timer_get_prev_entries(cur_time, lcore_id, prev);
	for (i = priv_timer[lcore_id].curr_skiplist_depth -1; i >= 0; i--) {
		priv_timer[lcore_id].pending_head.sl_next[i] = prev[i]->sl_next[i];
		if (prev[i]->sl_next[i] == NULL)
			priv_timer[lcore_id].curr_skiplist_depth--;
		prev[i] ->sl_next[i] = NULL;
	}

	/* now scan expired list and call callbacks */
	for ( ; tim != NULL; tim = next_tim) {
		next_tim = tim->sl_next[0];

		ret = timer_set_running_state(tim);

		/* this timer was not pending, continue */
		if (ret < 0)
			continue;

		rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);

		priv_timer[lcore_id].updated = 0;

		/* execute callback function with list unlocked */
		tim->f(tim, tim->arg);

		rte_spinlock_lock(&priv_timer[lcore_id].list_lock);
		__TIMER_STAT_ADD(pending, -1);
		/* the timer was stopped or reloaded by the callback
		 * function, we have nothing to do here */
		if (priv_timer[lcore_id].updated == 1)
			continue;

		if (tim->period == 0) {
			/* remove from done list and mark timer as stopped */
			status.state = RTE_TIMER_STOP;
			status.owner = RTE_TIMER_NO_OWNER;
			rte_wmb();
			tim->status.u32 = status.u32;
		}
		else {
			/* keep it in list and mark timer as pending */
			status.state = RTE_TIMER_PENDING;
			__TIMER_STAT_ADD(pending, 1);
			status.owner = (int16_t)lcore_id;
			rte_wmb();
			tim->status.u32 = status.u32;
			__rte_timer_reset(tim, cur_time + tim->period,
					tim->period, lcore_id, tim->f, tim->arg, 1);
		}
	}

	/* update the next to expire timer value */
	priv_timer[lcore_id].pending_head.expire =
			(priv_timer[lcore_id].pending_head.sl_next[0] == NULL) ? 0 :
					priv_timer[lcore_id].pending_head.sl_next[0]->expire;
done:
	/* job finished, unlock the list lock */
	rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);
}

/* dump statistics about timers */
void rte_timer_dump_stats(FILE *f)
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
	fprintf(f, "Timer statistics:\n");
	fprintf(f, "  reset = %"PRIu64"\n", sum.reset);
	fprintf(f, "  stop = %"PRIu64"\n", sum.stop);
	fprintf(f, "  manage = %"PRIu64"\n", sum.manage);
	fprintf(f, "  pending = %"PRIu64"\n", sum.pending);
#else
	fprintf(f, "No timer statistics, RTE_LIBRTE_TIMER_DEBUG is disabled\n");
#endif
}
