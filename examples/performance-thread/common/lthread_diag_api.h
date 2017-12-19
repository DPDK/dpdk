/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */
#ifndef LTHREAD_DIAG_API_H_
#define LTHREAD_DIAG_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <inttypes.h>

/*
 * Enable diagnostics
 * 0 = conditionally compiled out
 * 1 = compiled in and maskable at run time, see below for details
 */
#define LTHREAD_DIAG 0

/**
 *
 * @file lthread_diag_api.h
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * lthread diagnostic interface
 *
 * If enabled via configuration file option ( tbd ) the lthread subsystem
 * can generate selected trace information, either RTE_LOG  (INFO) messages,
 * or else invoke a user supplied callback function when any of the events
 * listed below occur.
 *
 * Reporting of events can be selectively masked, the bit position in the
 * mask is determined by the corresponding event identifier listed below.
 *
 * Diagnostics are enabled by registering the callback function and mask
 * using the API lthread_diagnostic_enable().
 *
 * Various interesting parameters are passed to the callback, including the
 * time in cpu clks, the lthread id, the diagnostic event id, a user ref value,
 * event text string, object being traced, and two context dependent parameters
 * (p1 and p2). The meaning of the two parameters p1 and p2 depends on
 * the specific event.
 *
 * The events LT_DIAG_LTHREAD_CREATE, LT_DIAG_MUTEX_CREATE and
 * LT_DIAG_COND_CREATE are implicitly enabled if the event mask includes any of
 * the LT_DIAG_LTHREAD_XXX, LT_DIAG_MUTEX_XXX or LT_DIAG_COND_XXX events
 * respectively.
 *
 * These create events may also be included in the mask discreetly if it is
 * desired to monitor only create events.
 *
 * @param  time
 *  The time in cpu clks at which the event occurred
 *
 * @param  lthread
 *  The current lthread
 *
 * @param diag_event
 *  The diagnostic event id (bit position in the mask)
 *
 * @param  diag_ref
 *
 * For LT_DIAG_LTHREAD_CREATE, LT_DIAG_MUTEX_CREATE or LT_DIAG_COND_CREATE
 * this parameter is not used and set to 0.
 * All other events diag_ref contains the user ref value returned by the
 * callback function when lthread is created.
 *
 * The diag_ref values assigned to mutex and cond var can be retrieved
 * using the APIs lthread_mutex_diag_ref(), and lthread_cond_diag_ref()
 * respectively.
 *
 * @param p1
 *  see below
 *
 * @param p1
 *  see below
 *
 * @returns
 * For LT_DIAG_LTHREAD_CREATE, LT_DIAG_MUTEX_CREATE or LT_DIAG_COND_CREATE
 * expects a user diagnostic ref value that will be saved in the lthread, mutex
 * or cond var.
 *
 * For all other events return value is ignored.
 *
 *	LT_DIAG_SCHED_CREATE - Invoked when a scheduler is created
 *		p1 = the scheduler that was created
 *		p2 = not used
 *		return value will be ignored
 *
 *	LT_DIAG_SCHED_SHUTDOWN - Invoked when a shutdown request is received
 *		p1 = the scheduler to be shutdown
 *		p2 = not used
 *		return value will be ignored
 *
 *	LT_DIAG_LTHREAD_CREATE - Invoked when a thread is created
 *		p1 = the lthread that was created
 *		p2 = not used
 *		return value will be stored in the lthread
 *
 *	LT_DIAG_LTHREAD_EXIT - Invoked when a lthread exits
 *		p2 = 0 if the thread was already joined
 *		p2 = 1 if the thread was not already joined
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_JOIN - Invoked when a lthread exits
 *		p1 = the lthread that is being joined
 *		p2 = 0 if the thread was already exited
 *		p2 = 1 if the thread was not already exited
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_CANCELLED - Invoked when an lthread is cancelled
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_DETACH - Invoked when an lthread is detached
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_FREE - Invoked when an lthread is freed
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_SUSPENDED - Invoked when an lthread is suspended
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_YIELD - Invoked when an lthread explicitly yields
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_RESCHEDULED - Invoked when an lthread is rescheduled
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_RESUMED - Invoked when an lthread is resumed
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_AFFINITY - Invoked when an lthread is affinitised
 *		p1 = the destination lcore_id
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_TMR_START - Invoked when an lthread starts a timer
 *		p1 = address of timer node
 *		p2 = the timeout value
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_TMR_DELETE - Invoked when an lthread deletes a timer
 *		p1 = address of the timer node
 *		p2 = 0 the timer and the was successfully deleted
 *		p2 = not usee
 *		return val ignored
 *
 *	LT_DIAG_LTHREAD_TMR_EXPIRED - Invoked when an lthread timer expires
 *		p1 = address of scheduler the timer expired on
 *		p2 = the thread associated with the timer
 *		return val ignored
 *
 *	LT_DIAG_COND_CREATE - Invoked when a condition variable is created
 *		p1 = address of cond var that was created
 *		p2 = not used
 *		return diag ref value will be stored in the condition variable
 *
 *	LT_DIAG_COND_DESTROY - Invoked when a condition variable is destroyed
 *		p1 = not used
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_COND_WAIT - Invoked when an lthread waits on a cond var
 *		p1 = the address of the condition variable
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_COND_SIGNAL - Invoked when an lthread signals a cond var
 *		p1 = the address of the cond var
 *		p2 = the lthread that was signalled, or error code
 *		return val ignored
 *
 *	LT_DIAG_COND_BROADCAST - Invoked when an lthread broadcasts a cond var
 *		p1 = the address of the condition variable
 *		p2 = the lthread(s) that are signalled, or error code
 *
 *	LT_DIAG_MUTEX_CREATE - Invoked when a mutex is created
 *		p1 = address of muex
 *		p2 = not used
 *		return diag ref value will be stored in the mutex variable
 *
 *	LT_DIAG_MUTEX_DESTROY - Invoked when a mutex is destroyed
 *		p1 = address of mutex
 *		p2 = not used
 *		return val ignored
 *
 *	LT_DIAG_MUTEX_LOCK - Invoked when a mutex lock is obtained
 *		p1 = address of mutex
 *		p2 = function return value
 *		return val ignored
 *
 *	LT_DIAG_MUTEX_BLOCKED  - Invoked when an lthread blocks on a mutex
 *		p1 = address of mutex
 *		p2 = function return value
 *		return val ignored
 *
 *	LT_DIAG_MUTEX_TRYLOCK - Invoked when a mutex try lock is attempted
 *		p1 = address of mutex
 *		p2 = the function return value
 *		return val ignored
 *
 *	LT_DIAG_MUTEX_UNLOCKED - Invoked when a mutex is unlocked
 *		p1 = address of mutex
 *		p2 = the thread that was unlocked, or error code
 *		return val ignored
 */
typedef uint64_t (*diag_callback) (uint64_t time, struct lthread *lt,
				  int diag_event, uint64_t diag_ref,
				const char *text, uint64_t p1, uint64_t p2);

/*
 * Set user diagnostic callback and mask
 * If the callback function pointer is NULL the default
 * callback handler will be restored.
 */
void lthread_diagnostic_enable(diag_callback cb, uint64_t diag_mask);

/*
 * Set diagnostic mask
 */
void lthread_diagnostic_set_mask(uint64_t mask);

/*
 * lthread diagnostic callback
 */
enum lthread_diag_ev {
	/* bits 0 - 14 lthread flag group */
	LT_DIAG_LTHREAD_CREATE,		/* 00 mask 0x00000001 */
	LT_DIAG_LTHREAD_EXIT,		/* 01 mask 0x00000002 */
	LT_DIAG_LTHREAD_JOIN,		/* 02 mask 0x00000004 */
	LT_DIAG_LTHREAD_CANCEL,		/* 03 mask 0x00000008 */
	LT_DIAG_LTHREAD_DETACH,		/* 04 mask 0x00000010 */
	LT_DIAG_LTHREAD_FREE,		/* 05 mask 0x00000020 */
	LT_DIAG_LTHREAD_SUSPENDED,	/* 06 mask 0x00000040 */
	LT_DIAG_LTHREAD_YIELD,		/* 07 mask 0x00000080 */
	LT_DIAG_LTHREAD_RESCHEDULED,	/* 08 mask 0x00000100 */
	LT_DIAG_LTHREAD_SLEEP,		/* 09 mask 0x00000200 */
	LT_DIAG_LTHREAD_RESUMED,	/* 10 mask 0x00000400 */
	LT_DIAG_LTHREAD_AFFINITY,	/* 11 mask 0x00000800 */
	LT_DIAG_LTHREAD_TMR_START,	/* 12 mask 0x00001000 */
	LT_DIAG_LTHREAD_TMR_DELETE,	/* 13 mask 0x00002000 */
	LT_DIAG_LTHREAD_TMR_EXPIRED,	/* 14 mask 0x00004000 */
	/* bits 15 - 19 conditional variable flag group */
	LT_DIAG_COND_CREATE,		/* 15 mask 0x00008000 */
	LT_DIAG_COND_DESTROY,		/* 16 mask 0x00010000 */
	LT_DIAG_COND_WAIT,		/* 17 mask 0x00020000 */
	LT_DIAG_COND_SIGNAL,		/* 18 mask 0x00040000 */
	LT_DIAG_COND_BROADCAST,		/* 19 mask 0x00080000 */
	/* bits 20 - 25 mutex flag group */
	LT_DIAG_MUTEX_CREATE,		/* 20 mask 0x00100000 */
	LT_DIAG_MUTEX_DESTROY,		/* 21 mask 0x00200000 */
	LT_DIAG_MUTEX_LOCK,		/* 22 mask 0x00400000 */
	LT_DIAG_MUTEX_TRYLOCK,		/* 23 mask 0x00800000 */
	LT_DIAG_MUTEX_BLOCKED,		/* 24 mask 0x01000000 */
	LT_DIAG_MUTEX_UNLOCKED,		/* 25 mask 0x02000000 */
	/* bits 26 - 27 scheduler flag group - 8 bits */
	LT_DIAG_SCHED_CREATE,		/* 26 mask 0x04000000 */
	LT_DIAG_SCHED_SHUTDOWN,		/* 27 mask 0x08000000 */
	LT_DIAG_EVENT_MAX
};

#define LT_DIAG_ALL 0xffffffffffffffff


/*
 * Display scheduler stats
 */
void
lthread_sched_stats_display(void);

/*
 * return the diagnostic ref val stored in a condition var
 */
uint64_t
lthread_cond_diag_ref(struct lthread_cond *c);

/*
 * return the diagnostic ref val stored in a mutex
 */
uint64_t
lthread_mutex_diag_ref(struct lthread_mutex *m);

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_DIAG_API_H_ */
