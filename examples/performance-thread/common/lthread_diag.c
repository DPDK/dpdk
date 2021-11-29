/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#include <rte_log.h>
#include <rte_common.h>

#include "lthread_diag.h"
#include "lthread_queue.h"
#include "lthread_pool.h"
#include "lthread_objcache.h"
#include "lthread_sched.h"
#include "lthread_diag_api.h"


/* dummy ref value of default diagnostic callback */
static uint64_t dummy_ref;

#define DIAG_SCHED_STATS_FORMAT \
"core %d\n%33s %12s %12s %12s %12s\n"

#define DIAG_CACHE_STATS_FORMAT \
"%20s %12lu %12lu %12lu %12lu %12lu\n"

#define DIAG_QUEUE_STATS_FORMAT \
"%20s %12lu %12lu %12lu\n"


/*
 * texts used in diagnostic events,
 * corresponding diagnostic mask bit positions are given as comment
 */
const char *diag_event_text[] = {
	"LTHREAD_CREATE     ",	/* 00 */
	"LTHREAD_EXIT       ",	/* 01 */
	"LTHREAD_JOIN       ",	/* 02 */
	"LTHREAD_CANCEL     ",	/* 03 */
	"LTHREAD_DETACH     ",	/* 04 */
	"LTHREAD_FREE       ",	/* 05 */
	"LTHREAD_SUSPENDED  ",	/* 06 */
	"LTHREAD_YIELD      ",	/* 07 */
	"LTHREAD_RESCHEDULED",	/* 08 */
	"LTHREAD_SLEEP      ",	/* 09 */
	"LTHREAD_RESUMED    ",	/* 10 */
	"LTHREAD_AFFINITY   ",	/* 11 */
	"LTHREAD_TMR_START  ",	/* 12 */
	"LTHREAD_TMR_DELETE ",	/* 13 */
	"LTHREAD_TMR_EXPIRED",	/* 14 */
	"COND_CREATE        ",	/* 15 */
	"COND_DESTROY       ",	/* 16 */
	"COND_WAIT          ",	/* 17 */
	"COND_SIGNAL        ",	/* 18 */
	"COND_BROADCAST     ",	/* 19 */
	"MUTEX_CREATE       ",	/* 20 */
	"MUTEX_DESTROY      ",	/* 21 */
	"MUTEX_LOCK         ",	/* 22 */
	"MUTEX_TRYLOCK      ",	/* 23 */
	"MUTEX_BLOCKED      ",	/* 24 */
	"MUTEX_UNLOCKED     ",	/* 25 */
	"SCHED_CREATE       ",	/* 26 */
	"SCHED_SHUTDOWN     "	/* 27 */
};


/*
 * set diagnostic ,ask
 */
void lthread_diagnostic_set_mask(DIAG_USED uint64_t mask)
{
#if LTHREAD_DIAG
	diag_mask = mask;
#else
	RTE_LOG(INFO, LTHREAD,
		"LTHREAD_DIAG is not set, see lthread_diag_api.h\n");
#endif
}


/*
 * Check consistency of the scheduler stats
 * Only sensible run after the schedulers are stopped
 * Count the number of objects lying in caches and queues
 * and available in the qnode pool.
 * This should be equal to the total capacity of all
 * qnode pools.
 */
void
_sched_stats_consistency_check(void);
void
_sched_stats_consistency_check(void)
{
#if LTHREAD_DIAG
	int i;
	struct lthread_sched *sched;
	uint64_t count = 0;
	uint64_t capacity = 0;

	for (i = 0; i < LTHREAD_MAX_LCORES; i++) {
		sched = schedcore[i];
		if (sched == NULL)
			continue;

		/* each of these queues consumes a stub node */
		count += 8;
		count += DIAG_COUNT(sched->ready, size);
		count += DIAG_COUNT(sched->pready, size);
		count += DIAG_COUNT(sched->lthread_cache, available);
		count += DIAG_COUNT(sched->stack_cache, available);
		count += DIAG_COUNT(sched->tls_cache, available);
		count += DIAG_COUNT(sched->per_lthread_cache, available);
		count += DIAG_COUNT(sched->cond_cache, available);
		count += DIAG_COUNT(sched->mutex_cache, available);

		/* the node pool does not consume a stub node */
		if (sched->qnode_pool->fast_alloc != NULL)
			count++;
		count += DIAG_COUNT(sched->qnode_pool, available);

		capacity += DIAG_COUNT(sched->qnode_pool, capacity);
	}
	if (count != capacity) {
		RTE_LOG(CRIT, LTHREAD,
			"Scheduler caches are inconsistent\n");
	} else {
		RTE_LOG(INFO, LTHREAD,
			"Scheduler caches are ok\n");
	}
#endif
}


#if LTHREAD_DIAG
/*
 * Display node pool stats
 */
static inline void
_qnode_pool_display(DIAG_USED struct qnode_pool *p)
{

	printf(DIAG_CACHE_STATS_FORMAT,
			p->name,
			DIAG_COUNT(p, rd),
			DIAG_COUNT(p, wr),
			DIAG_COUNT(p, available),
			DIAG_COUNT(p, prealloc),
			DIAG_COUNT(p, capacity));
	fflush(stdout);
}
#endif


#if LTHREAD_DIAG
/*
 * Display queue stats
 */
static inline void
_lthread_queue_display(DIAG_USED struct lthread_queue *q)
{
#if DISPLAY_OBJCACHE_QUEUES
	printf(DIAG_QUEUE_STATS_FORMAT,
			q->name,
			DIAG_COUNT(q, rd),
			DIAG_COUNT(q, wr),
			DIAG_COUNT(q, size));
	fflush(stdout);
#else
	printf("%s: queue stats disabled\n",
			q->name);

#endif
}
#endif

#if LTHREAD_DIAG
/*
 * Display objcache stats
 */
static inline void
_objcache_display(DIAG_USED struct lthread_objcache *c)
{

	printf(DIAG_CACHE_STATS_FORMAT,
			c->name,
			DIAG_COUNT(c, rd),
			DIAG_COUNT(c, wr),
			DIAG_COUNT(c, available),
			DIAG_COUNT(c, prealloc),
			DIAG_COUNT(c, capacity));
	_lthread_queue_display(c->q);
	fflush(stdout);
}
#endif

/*
 * Display sched stats
 */
void
lthread_sched_stats_display(void)
{
#if LTHREAD_DIAG
	int i;
	struct lthread_sched *sched;

	for (i = 0; i < LTHREAD_MAX_LCORES; i++) {
		sched = schedcore[i];
		if (sched != NULL) {
			printf(DIAG_SCHED_STATS_FORMAT,
					sched->lcore_id,
					"rd",
					"wr",
					"present",
					"nb preallocs",
					"capacity");
			_lthread_queue_display(sched->ready);
			_lthread_queue_display(sched->pready);
			_qnode_pool_display(sched->qnode_pool);
			_objcache_display(sched->lthread_cache);
			_objcache_display(sched->stack_cache);
			_objcache_display(sched->tls_cache);
			_objcache_display(sched->per_lthread_cache);
			_objcache_display(sched->cond_cache);
			_objcache_display(sched->mutex_cache);
		fflush(stdout);
		}
	}
	_sched_stats_consistency_check();
#else
	RTE_LOG(INFO, LTHREAD,
		"lthread diagnostics disabled\n"
		"hint - set LTHREAD_DIAG in lthread_diag_api.h\n");
#endif
}

/*
 * Default diagnostic callback
 */
static uint64_t
_lthread_diag_default_cb(uint64_t time, struct lthread *lt, int diag_event,
		uint64_t diag_ref, const char *text, uint64_t p1, uint64_t p2)
{
	uint64_t _p2;
	int lcore = (int) rte_lcore_id();

	switch (diag_event) {
	case LT_DIAG_LTHREAD_CREATE:
	case LT_DIAG_MUTEX_CREATE:
	case LT_DIAG_COND_CREATE:
		_p2 = dummy_ref;
		break;
	default:
		_p2 = p2;
		break;
	}

	printf("%"PRIu64" %d %8.8lx %8.8lx %s %8.8lx %8.8lx\n",
		time,
		lcore,
		(uint64_t) lt,
		diag_ref,
		text,
		p1,
		_p2);

	return dummy_ref++;
}

/*
 * plug in default diag callback with mask off
 */
RTE_INIT(_lthread_diag_ctor)
{
	diag_cb = _lthread_diag_default_cb;
	diag_mask = 0;
}


/*
 * enable diagnostics
 */
void lthread_diagnostic_enable(DIAG_USED diag_callback cb,
				DIAG_USED uint64_t mask)
{
#if LTHREAD_DIAG
	if (cb == NULL)
		diag_cb = _lthread_diag_default_cb;
	else
		diag_cb = cb;
	diag_mask = mask;
#else
	RTE_LOG(INFO, LTHREAD,
		"LTHREAD_DIAG is not set, see lthread_diag_api.h\n");
#endif
}
