/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_spinlock.h>
#include <rte_random.h>
#include <rte_pause.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "rte_timer.h"

/**
 * Per-lcore info for timers.
 */
struct priv_timer {
	struct rte_timer pending_head;  /**< dummy timer instance to head up list */
	rte_spinlock_t list_lock;       /**< lock to protect list access */

	/** per-core variable that true if a timer was updated on this
	 *  core since last reset of the variable */
	int updated;

	/** track the current depth of the skiplist */
	unsigned curr_skiplist_depth;

	unsigned prev_lcore;              /**< used for lcore round robin */

	/** running timer on this lcore now */
	struct rte_timer *running_tim;

#ifdef RTE_LIBRTE_TIMER_DEBUG
	/** per-lcore statistics */
	struct rte_timer_debug_stats stats;
#endif
} __rte_cache_aligned;

#define FL_ALLOCATED	(1 << 0)
struct rte_timer_data {
	struct priv_timer priv_timer[RTE_MAX_LCORE];
	uint8_t internal_flags;
};

#define RTE_MAX_DATA_ELS 64
static const struct rte_memzone *rte_timer_data_mz;
static int *volatile rte_timer_mz_refcnt;
static struct rte_timer_data *rte_timer_data_arr;
static const uint32_t default_data_id;
static uint32_t rte_timer_subsystem_initialized;

/* when debug is enabled, store some statistics */
#ifdef RTE_LIBRTE_TIMER_DEBUG
#define __TIMER_STAT_ADD(priv_timer, name, n) do {			\
		unsigned __lcore_id = rte_lcore_id();			\
		if (__lcore_id < RTE_MAX_LCORE)				\
			priv_timer[__lcore_id].stats.name += (n);	\
	} while(0)
#else
#define __TIMER_STAT_ADD(priv_timer, name, n) do {} while (0)
#endif

static inline int
timer_data_valid(uint32_t id)
{
	return rte_timer_data_arr &&
		(rte_timer_data_arr[id].internal_flags & FL_ALLOCATED);
}

/* validate ID and retrieve timer data pointer, or return error value */
#define TIMER_DATA_VALID_GET_OR_ERR_RET(id, timer_data, retval) do {	\
	if (id >= RTE_MAX_DATA_ELS || !timer_data_valid(id))		\
		return retval;						\
	timer_data = &rte_timer_data_arr[id];				\
} while (0)

int
rte_timer_data_alloc(uint32_t *id_ptr)
{
	int i;
	struct rte_timer_data *data;

	if (!rte_timer_subsystem_initialized)
		return -ENOMEM;

	for (i = 0; i < RTE_MAX_DATA_ELS; i++) {
		data = &rte_timer_data_arr[i];
		if (!(data->internal_flags & FL_ALLOCATED)) {
			data->internal_flags |= FL_ALLOCATED;

			if (id_ptr)
				*id_ptr = i;

			return 0;
		}
	}

	return -ENOSPC;
}

int
rte_timer_data_dealloc(uint32_t id)
{
	struct rte_timer_data *timer_data;
	TIMER_DATA_VALID_GET_OR_ERR_RET(id, timer_data, -EINVAL);

	timer_data->internal_flags &= ~(FL_ALLOCATED);

	return 0;
}

/* Init the timer library. Allocate an array of timer data structs in shared
 * memory, and allocate the zeroth entry for use with original timer
 * APIs. Since the intersection of the sets of lcore ids in primary and
 * secondary processes should be empty, the zeroth entry can be shared by
 * multiple processes.
 */
int
rte_timer_subsystem_init(void)
{
	const struct rte_memzone *mz;
	struct rte_timer_data *data;
	int i, lcore_id;
	static const char *mz_name = "rte_timer_mz";
	const size_t data_arr_size =
			RTE_MAX_DATA_ELS * sizeof(*rte_timer_data_arr);
	const size_t mem_size = data_arr_size + sizeof(*rte_timer_mz_refcnt);
	bool do_full_init = true;

	rte_mcfg_timer_lock();

	if (rte_timer_subsystem_initialized) {
		rte_mcfg_timer_unlock();
		return -EALREADY;
	}

	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL) {
		mz = rte_memzone_reserve_aligned(mz_name, mem_size,
				SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);
		if (mz == NULL) {
			rte_mcfg_timer_unlock();
			return -ENOMEM;
		}
		do_full_init = true;
	} else
		do_full_init = false;

	rte_timer_data_mz = mz;
	rte_timer_data_arr = mz->addr;
	rte_timer_mz_refcnt = (void *)((char *)mz->addr + data_arr_size);

	if (do_full_init) {
		for (i = 0; i < RTE_MAX_DATA_ELS; i++) {
			data = &rte_timer_data_arr[i];

			for (lcore_id = 0; lcore_id < RTE_MAX_LCORE;
			     lcore_id++) {
				rte_spinlock_init(
					&data->priv_timer[lcore_id].list_lock);
				data->priv_timer[lcore_id].prev_lcore =
					lcore_id;
			}
		}
	}

	rte_timer_data_arr[default_data_id].internal_flags |= FL_ALLOCATED;
	(*rte_timer_mz_refcnt)++;

	rte_timer_subsystem_initialized = 1;

	rte_mcfg_timer_unlock();

	return 0;
}

void
rte_timer_subsystem_finalize(void)
{
	rte_mcfg_timer_lock();

	if (!rte_timer_subsystem_initialized) {
		rte_mcfg_timer_unlock();
		return;
	}

	if (--(*rte_timer_mz_refcnt) == 0)
		rte_memzone_free(rte_timer_data_mz);

	rte_timer_subsystem_initialized = 0;

	rte_mcfg_timer_unlock();
}

/* Initialize the timer handle tim for use */
void
rte_timer_init(struct rte_timer *tim)
{
	union rte_timer_status status;

	status.state = RTE_TIMER_STOP;
	status.owner = RTE_TIMER_NO_OWNER;
	__atomic_store_n(&tim->status.u32, status.u32, __ATOMIC_RELAXED);
}

/*
 * if timer is pending or stopped (or running on the same core than
 * us), mark timer as configuring, and on success return the previous
 * status of the timer
 */
static int
timer_set_config_state(struct rte_timer *tim,
		       union rte_timer_status *ret_prev_status,
		       struct priv_timer *priv_timer)
{
	union rte_timer_status prev_status, status;
	int success = 0;
	unsigned lcore_id;

	lcore_id = rte_lcore_id();

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	prev_status.u32 = __atomic_load_n(&tim->status.u32, __ATOMIC_RELAXED);

	while (success == 0) {
		/* timer is running on another core
		 * or ready to run on local core, exit
		 */
		if (prev_status.state == RTE_TIMER_RUNNING &&
		    (prev_status.owner != (uint16_t)lcore_id ||
		     tim != priv_timer[lcore_id].running_tim))
			return -1;

		/* timer is being configured on another core */
		if (prev_status.state == RTE_TIMER_CONFIG)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as being configured */
		status.state = RTE_TIMER_CONFIG;
		status.owner = (int16_t)lcore_id;
		/* CONFIG states are acting as locked states. If the
		 * timer is in CONFIG state, the state cannot be changed
		 * by other threads. So, we should use ACQUIRE here.
		 */
		success = __atomic_compare_exchange_n(&tim->status.u32,
					      &prev_status.u32,
					      status.u32, 0,
					      __ATOMIC_ACQUIRE,
					      __ATOMIC_RELAXED);
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
	prev_status.u32 = __atomic_load_n(&tim->status.u32, __ATOMIC_RELAXED);

	while (success == 0) {
		/* timer is not pending anymore */
		if (prev_status.state != RTE_TIMER_PENDING)
			return -1;

		/* we know that the timer will be pending at this point
		 * mark it atomically as being running
		 */
		status.state = RTE_TIMER_RUNNING;
		status.owner = (int16_t)lcore_id;
		/* RUNNING states are acting as locked states. If the
		 * timer is in RUNNING state, the state cannot be changed
		 * by other threads. So, we should use ACQUIRE here.
		 */
		success = __atomic_compare_exchange_n(&tim->status.u32,
					      &prev_status.u32,
					      status.u32, 0,
					      __ATOMIC_ACQUIRE,
					      __ATOMIC_RELAXED);
	}

	return 0;
}

/*
 * Return a skiplist level for a new entry.
 * This probabilistically gives a level with p=1/4 that an entry at level n
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
		       struct rte_timer **prev, struct priv_timer *priv_timer)
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
				struct rte_timer **prev,
				struct priv_timer *priv_timer)
{
	int i;

	/* to get a specific entry in the list, look for just lower than the time
	 * values, and then increment on each level individually if necessary
	 */
	timer_get_prev_entries(tim->expire - 1, tim_lcore, prev, priv_timer);
	for (i = priv_timer[tim_lcore].curr_skiplist_depth - 1; i >= 0; i--) {
		while (prev[i]->sl_next[i] != NULL &&
				prev[i]->sl_next[i] != tim &&
				prev[i]->sl_next[i]->expire <= tim->expire)
			prev[i] = prev[i]->sl_next[i];
	}
}

/* call with lock held as necessary
 * add in list
 * timer must be in config state
 * timer must not be in a list
 */
static void
timer_add(struct rte_timer *tim, unsigned int tim_lcore,
	  struct priv_timer *priv_timer)
{
	unsigned lvl;
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH+1];

	/* find where exactly this element goes in the list of elements
	 * for each depth. */
	timer_get_prev_entries(tim->expire, tim_lcore, prev, priv_timer);

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
}

/*
 * del from list, lock if needed
 * timer must be in config state
 * timer must be in a list
 */
static void
timer_del(struct rte_timer *tim, union rte_timer_status prev_status,
	  int local_is_locked, struct priv_timer *priv_timer)
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
	timer_get_prev_entries_for_node(tim, prev_owner, prev, priv_timer);
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
		  int local_is_locked,
		  struct rte_timer_data *timer_data)
{
	union rte_timer_status prev_status, status;
	int ret;
	unsigned lcore_id = rte_lcore_id();
	struct priv_timer *priv_timer = timer_data->priv_timer;

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
	ret = timer_set_config_state(tim, &prev_status, priv_timer);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(priv_timer, reset, 1);
	if (prev_status.state == RTE_TIMER_RUNNING &&
	    lcore_id < RTE_MAX_LCORE) {
		priv_timer[lcore_id].updated = 1;
	}

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING) {
		timer_del(tim, prev_status, local_is_locked, priv_timer);
		__TIMER_STAT_ADD(priv_timer, pending, -1);
	}

	tim->period = period;
	tim->expire = expire;
	tim->f = fct;
	tim->arg = arg;

	/* if timer needs to be scheduled on another core, we need to
	 * lock the destination list; if it is on local core, we need to lock if
	 * we are not called from rte_timer_manage()
	 */
	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_lock(&priv_timer[tim_lcore].list_lock);

	__TIMER_STAT_ADD(priv_timer, pending, 1);
	timer_add(tim, tim_lcore, priv_timer);

	/* update state: as we are in CONFIG state, only us can modify
	 * the state so we don't need to use cmpset() here */
	status.state = RTE_TIMER_PENDING;
	status.owner = (int16_t)tim_lcore;
	/* The "RELEASE" ordering guarantees the memory operations above
	 * the status update are observed before the update by all threads
	 */
	__atomic_store_n(&tim->status.u32, status.u32, __ATOMIC_RELEASE);

	if (tim_lcore != lcore_id || !local_is_locked)
		rte_spinlock_unlock(&priv_timer[tim_lcore].list_lock);

	return 0;
}

/* Reset and start the timer associated with the timer handle tim */
int
rte_timer_reset(struct rte_timer *tim, uint64_t ticks,
		      enum rte_timer_type type, unsigned int tim_lcore,
		      rte_timer_cb_t fct, void *arg)
{
	return rte_timer_alt_reset(default_data_id, tim, ticks, type,
				   tim_lcore, fct, arg);
}

int
rte_timer_alt_reset(uint32_t timer_data_id, struct rte_timer *tim,
		    uint64_t ticks, enum rte_timer_type type,
		    unsigned int tim_lcore, rte_timer_cb_t fct, void *arg)
{
	uint64_t cur_time = rte_get_timer_cycles();
	uint64_t period;
	struct rte_timer_data *timer_data;

	TIMER_DATA_VALID_GET_OR_ERR_RET(timer_data_id, timer_data, -EINVAL);

	if (type == PERIODICAL)
		period = ticks;
	else
		period = 0;

	return __rte_timer_reset(tim,  cur_time + ticks, period, tim_lcore,
				 fct, arg, 0, timer_data);
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

static int
__rte_timer_stop(struct rte_timer *tim, int local_is_locked,
		 struct rte_timer_data *timer_data)
{
	union rte_timer_status prev_status, status;
	unsigned lcore_id = rte_lcore_id();
	int ret;
	struct priv_timer *priv_timer = timer_data->priv_timer;

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	ret = timer_set_config_state(tim, &prev_status, priv_timer);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(priv_timer, stop, 1);
	if (prev_status.state == RTE_TIMER_RUNNING &&
	    lcore_id < RTE_MAX_LCORE) {
		priv_timer[lcore_id].updated = 1;
	}

	/* remove it from list */
	if (prev_status.state == RTE_TIMER_PENDING) {
		timer_del(tim, prev_status, local_is_locked, priv_timer);
		__TIMER_STAT_ADD(priv_timer, pending, -1);
	}

	/* mark timer as stopped */
	status.state = RTE_TIMER_STOP;
	status.owner = RTE_TIMER_NO_OWNER;
	/* The "RELEASE" ordering guarantees the memory operations above
	 * the status update are observed before the update by all threads
	 */
	__atomic_store_n(&tim->status.u32, status.u32, __ATOMIC_RELEASE);

	return 0;
}

/* Stop the timer associated with the timer handle tim */
int
rte_timer_stop(struct rte_timer *tim)
{
	return rte_timer_alt_stop(default_data_id, tim);
}

int
rte_timer_alt_stop(uint32_t timer_data_id, struct rte_timer *tim)
{
	struct rte_timer_data *timer_data;

	TIMER_DATA_VALID_GET_OR_ERR_RET(timer_data_id, timer_data, -EINVAL);

	return __rte_timer_stop(tim, 0, timer_data);
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
	return __atomic_load_n(&tim->status.state,
				__ATOMIC_RELAXED) == RTE_TIMER_PENDING;
}

/* must be called periodically, run all timer that expired */
static void
__rte_timer_manage(struct rte_timer_data *timer_data)
{
	union rte_timer_status status;
	struct rte_timer *tim, *next_tim;
	struct rte_timer *run_first_tim, **pprev;
	unsigned lcore_id = rte_lcore_id();
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH + 1];
	uint64_t cur_time;
	int i, ret;
	struct priv_timer *priv_timer = timer_data->priv_timer;

	/* timer manager only runs on EAL thread with valid lcore_id */
	assert(lcore_id < RTE_MAX_LCORE);

	__TIMER_STAT_ADD(priv_timer, manage, 1);
	/* optimize for the case where per-cpu list is empty */
	if (priv_timer[lcore_id].pending_head.sl_next[0] == NULL)
		return;
	cur_time = rte_get_timer_cycles();

#ifdef RTE_ARCH_64
	/* on 64-bit the value cached in the pending_head.expired will be
	 * updated atomically, so we can consult that for a quick check here
	 * outside the lock */
	if (likely(priv_timer[lcore_id].pending_head.expire > cur_time))
		return;
#endif

	/* browse ordered list, add expired timers in 'expired' list */
	rte_spinlock_lock(&priv_timer[lcore_id].list_lock);

	/* if nothing to do just unlock and return */
	if (priv_timer[lcore_id].pending_head.sl_next[0] == NULL ||
	    priv_timer[lcore_id].pending_head.sl_next[0]->expire > cur_time) {
		rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);
		return;
	}

	/* save start of list of expired timers */
	tim = priv_timer[lcore_id].pending_head.sl_next[0];

	/* break the existing list at current time point */
	timer_get_prev_entries(cur_time, lcore_id, prev, priv_timer);
	for (i = priv_timer[lcore_id].curr_skiplist_depth -1; i >= 0; i--) {
		if (prev[i] == &priv_timer[lcore_id].pending_head)
			continue;
		priv_timer[lcore_id].pending_head.sl_next[i] =
		    prev[i]->sl_next[i];
		if (prev[i]->sl_next[i] == NULL)
			priv_timer[lcore_id].curr_skiplist_depth--;
		prev[i] ->sl_next[i] = NULL;
	}

	/* transition run-list from PENDING to RUNNING */
	run_first_tim = tim;
	pprev = &run_first_tim;

	for ( ; tim != NULL; tim = next_tim) {
		next_tim = tim->sl_next[0];

		ret = timer_set_running_state(tim);
		if (likely(ret == 0)) {
			pprev = &tim->sl_next[0];
		} else {
			/* another core is trying to re-config this one,
			 * remove it from local expired list
			 */
			*pprev = next_tim;
		}
	}

	/* update the next to expire timer value */
	priv_timer[lcore_id].pending_head.expire =
	    (priv_timer[lcore_id].pending_head.sl_next[0] == NULL) ? 0 :
		priv_timer[lcore_id].pending_head.sl_next[0]->expire;

	rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);

	/* now scan expired list and call callbacks */
	for (tim = run_first_tim; tim != NULL; tim = next_tim) {
		next_tim = tim->sl_next[0];
		priv_timer[lcore_id].updated = 0;
		priv_timer[lcore_id].running_tim = tim;

		/* execute callback function with list unlocked */
		tim->f(tim, tim->arg);

		__TIMER_STAT_ADD(priv_timer, pending, -1);
		/* the timer was stopped or reloaded by the callback
		 * function, we have nothing to do here */
		if (priv_timer[lcore_id].updated == 1)
			continue;

		if (tim->period == 0) {
			/* remove from done list and mark timer as stopped */
			status.state = RTE_TIMER_STOP;
			status.owner = RTE_TIMER_NO_OWNER;
			/* The "RELEASE" ordering guarantees the memory
			 * operations above the status update are observed
			 * before the update by all threads
			 */
			__atomic_store_n(&tim->status.u32, status.u32,
				__ATOMIC_RELEASE);
		}
		else {
			/* keep it in list and mark timer as pending */
			rte_spinlock_lock(&priv_timer[lcore_id].list_lock);
			status.state = RTE_TIMER_PENDING;
			__TIMER_STAT_ADD(priv_timer, pending, 1);
			status.owner = (int16_t)lcore_id;
			/* The "RELEASE" ordering guarantees the memory
			 * operations above the status update are observed
			 * before the update by all threads
			 */
			__atomic_store_n(&tim->status.u32, status.u32,
				__ATOMIC_RELEASE);
			__rte_timer_reset(tim, tim->expire + tim->period,
				tim->period, lcore_id, tim->f, tim->arg, 1,
				timer_data);
			rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);
		}
	}
	priv_timer[lcore_id].running_tim = NULL;
}

int
rte_timer_manage(void)
{
	struct rte_timer_data *timer_data;

	TIMER_DATA_VALID_GET_OR_ERR_RET(default_data_id, timer_data, -EINVAL);

	__rte_timer_manage(timer_data);

	return 0;
}

int
rte_timer_alt_manage(uint32_t timer_data_id,
		     unsigned int *poll_lcores,
		     int nb_poll_lcores,
		     rte_timer_alt_manage_cb_t f)
{
	unsigned int default_poll_lcores[] = {rte_lcore_id()};
	union rte_timer_status status;
	struct rte_timer *tim, *next_tim, **pprev;
	struct rte_timer *run_first_tims[RTE_MAX_LCORE];
	unsigned int this_lcore = rte_lcore_id();
	struct rte_timer *prev[MAX_SKIPLIST_DEPTH + 1];
	uint64_t cur_time;
	int i, j, ret;
	int nb_runlists = 0;
	struct rte_timer_data *data;
	struct priv_timer *privp;
	uint32_t poll_lcore;

	TIMER_DATA_VALID_GET_OR_ERR_RET(timer_data_id, data, -EINVAL);

	/* timer manager only runs on EAL thread with valid lcore_id */
	assert(this_lcore < RTE_MAX_LCORE);

	__TIMER_STAT_ADD(data->priv_timer, manage, 1);

	if (poll_lcores == NULL) {
		poll_lcores = default_poll_lcores;
		nb_poll_lcores = RTE_DIM(default_poll_lcores);
	}

	for (i = 0; i < nb_poll_lcores; i++) {
		poll_lcore = poll_lcores[i];
		privp = &data->priv_timer[poll_lcore];

		/* optimize for the case where per-cpu list is empty */
		if (privp->pending_head.sl_next[0] == NULL)
			continue;
		cur_time = rte_get_timer_cycles();

#ifdef RTE_ARCH_64
		/* on 64-bit the value cached in the pending_head.expired will
		 * be updated atomically, so we can consult that for a quick
		 * check here outside the lock
		 */
		if (likely(privp->pending_head.expire > cur_time))
			continue;
#endif

		/* browse ordered list, add expired timers in 'expired' list */
		rte_spinlock_lock(&privp->list_lock);

		/* if nothing to do just unlock and return */
		if (privp->pending_head.sl_next[0] == NULL ||
		    privp->pending_head.sl_next[0]->expire > cur_time) {
			rte_spinlock_unlock(&privp->list_lock);
			continue;
		}

		/* save start of list of expired timers */
		tim = privp->pending_head.sl_next[0];

		/* break the existing list at current time point */
		timer_get_prev_entries(cur_time, poll_lcore, prev,
				       data->priv_timer);
		for (j = privp->curr_skiplist_depth - 1; j >= 0; j--) {
			if (prev[j] == &privp->pending_head)
				continue;
			privp->pending_head.sl_next[j] =
				prev[j]->sl_next[j];
			if (prev[j]->sl_next[j] == NULL)
				privp->curr_skiplist_depth--;

			prev[j]->sl_next[j] = NULL;
		}

		/* transition run-list from PENDING to RUNNING */
		run_first_tims[nb_runlists] = tim;
		pprev = &run_first_tims[nb_runlists];
		nb_runlists++;

		for ( ; tim != NULL; tim = next_tim) {
			next_tim = tim->sl_next[0];

			ret = timer_set_running_state(tim);
			if (likely(ret == 0)) {
				pprev = &tim->sl_next[0];
			} else {
				/* another core is trying to re-config this one,
				 * remove it from local expired list
				 */
				*pprev = next_tim;
			}
		}

		/* update the next to expire timer value */
		privp->pending_head.expire =
		    (privp->pending_head.sl_next[0] == NULL) ? 0 :
			privp->pending_head.sl_next[0]->expire;

		rte_spinlock_unlock(&privp->list_lock);
	}

	/* Now process the run lists */
	while (1) {
		bool done = true;
		uint64_t min_expire = UINT64_MAX;
		int min_idx = 0;

		/* Find the next oldest timer to process */
		for (i = 0; i < nb_runlists; i++) {
			tim = run_first_tims[i];

			if (tim != NULL && tim->expire < min_expire) {
				min_expire = tim->expire;
				min_idx = i;
				done = false;
			}
		}

		if (done)
			break;

		tim = run_first_tims[min_idx];

		/* Move down the runlist from which we picked a timer to
		 * execute
		 */
		run_first_tims[min_idx] = run_first_tims[min_idx]->sl_next[0];

		data->priv_timer[this_lcore].updated = 0;
		data->priv_timer[this_lcore].running_tim = tim;

		/* Call the provided callback function */
		f(tim);

		__TIMER_STAT_ADD(data->priv_timer, pending, -1);

		/* the timer was stopped or reloaded by the callback
		 * function, we have nothing to do here
		 */
		if (data->priv_timer[this_lcore].updated == 1)
			continue;

		if (tim->period == 0) {
			/* remove from done list and mark timer as stopped */
			status.state = RTE_TIMER_STOP;
			status.owner = RTE_TIMER_NO_OWNER;
			/* The "RELEASE" ordering guarantees the memory
			 * operations above the status update are observed
			 * before the update by all threads
			 */
			__atomic_store_n(&tim->status.u32, status.u32,
				__ATOMIC_RELEASE);
		} else {
			/* keep it in list and mark timer as pending */
			rte_spinlock_lock(
				&data->priv_timer[this_lcore].list_lock);
			status.state = RTE_TIMER_PENDING;
			__TIMER_STAT_ADD(data->priv_timer, pending, 1);
			status.owner = (int16_t)this_lcore;
			/* The "RELEASE" ordering guarantees the memory
			 * operations above the status update are observed
			 * before the update by all threads
			 */
			__atomic_store_n(&tim->status.u32, status.u32,
				__ATOMIC_RELEASE);
			__rte_timer_reset(tim, tim->expire + tim->period,
				tim->period, this_lcore, tim->f, tim->arg, 1,
				data);
			rte_spinlock_unlock(
				&data->priv_timer[this_lcore].list_lock);
		}

		data->priv_timer[this_lcore].running_tim = NULL;
	}

	return 0;
}

/* Walk pending lists, stopping timers and calling user-specified function */
int
rte_timer_stop_all(uint32_t timer_data_id, unsigned int *walk_lcores,
		   int nb_walk_lcores,
		   rte_timer_stop_all_cb_t f, void *f_arg)
{
	int i;
	struct priv_timer *priv_timer;
	uint32_t walk_lcore;
	struct rte_timer *tim, *next_tim;
	struct rte_timer_data *timer_data;

	TIMER_DATA_VALID_GET_OR_ERR_RET(timer_data_id, timer_data, -EINVAL);

	for (i = 0; i < nb_walk_lcores; i++) {
		walk_lcore = walk_lcores[i];
		priv_timer = &timer_data->priv_timer[walk_lcore];

		rte_spinlock_lock(&priv_timer->list_lock);

		for (tim = priv_timer->pending_head.sl_next[0];
		     tim != NULL;
		     tim = next_tim) {
			next_tim = tim->sl_next[0];

			/* Call timer_stop with lock held */
			__rte_timer_stop(tim, 1, timer_data);

			if (f)
				f(tim, f_arg);
		}

		rte_spinlock_unlock(&priv_timer->list_lock);
	}

	return 0;
}

int64_t
rte_timer_next_ticks(void)
{
	unsigned int lcore_id = rte_lcore_id();
	struct rte_timer_data *timer_data;
	struct priv_timer *priv_timer;
	const struct rte_timer *tm;
	uint64_t cur_time;
	int64_t left = -ENOENT;

	TIMER_DATA_VALID_GET_OR_ERR_RET(default_data_id, timer_data, -EINVAL);

	priv_timer = timer_data->priv_timer;
	cur_time = rte_get_timer_cycles();

	rte_spinlock_lock(&priv_timer[lcore_id].list_lock);
	tm = priv_timer[lcore_id].pending_head.sl_next[0];
	if (tm) {
		left = tm->expire - cur_time;
		if (left < 0)
			left = 0;
	}
	rte_spinlock_unlock(&priv_timer[lcore_id].list_lock);

	return left;
}

/* dump statistics about timers */
static void
__rte_timer_dump_stats(struct rte_timer_data *timer_data __rte_unused, FILE *f)
{
#ifdef RTE_LIBRTE_TIMER_DEBUG
	struct rte_timer_debug_stats sum;
	unsigned lcore_id;
	struct priv_timer *priv_timer = timer_data->priv_timer;

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

int
rte_timer_dump_stats(FILE *f)
{
	return rte_timer_alt_dump_stats(default_data_id, f);
}

int
rte_timer_alt_dump_stats(uint32_t timer_data_id __rte_unused, FILE *f)
{
	struct rte_timer_data *timer_data;

	TIMER_DATA_VALID_GET_OR_ERR_RET(timer_data_id, timer_data, -EINVAL);

	__rte_timer_dump_stats(timer_data, f);

	return 0;
}
