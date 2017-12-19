/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#ifndef LTHREAD_DIAG_H_
#define LTHREAD_DIAG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <inttypes.h>

#include <rte_log.h>
#include <rte_common.h>

#include "lthread_api.h"
#include "lthread_diag_api.h"

extern diag_callback diag_cb;

extern const char *diag_event_text[];
extern uint64_t diag_mask;

/* max size of name strings */
#define LT_MAX_NAME_SIZE 64

#if LTHREAD_DIAG
#define DISPLAY_OBJCACHE_QUEUES 1

/*
 * Generate a diagnostic trace or event in the case where an object is created.
 *
 * The value returned by the callback is stored in the object.
 *
 * @ param obj
 *  pointer to the object that was created
 * @ param ev
 *  the event code
 *
 */
#define DIAG_CREATE_EVENT(obj, ev) do {					\
	struct lthread *ct = RTE_PER_LCORE(this_sched)->current_lthread;\
	if ((BIT(ev) & diag_mask) && (ev < LT_DIAG_EVENT_MAX)) {	\
		(obj)->diag_ref = (diag_cb)(rte_rdtsc(),		\
					ct,				\
					(ev),				\
					0,				\
					diag_event_text[(ev)],		\
					(uint64_t)obj,			\
					0);				\
	}								\
} while (0)

/*
 * Generate a diagnostic trace event.
 *
 * @ param obj
 *  pointer to the lthread, cond or mutex object
 * @ param ev
 *  the event code
 * @ param p1
 *  object specific value ( see lthread_diag_api.h )
 * @ param p2
 *  object specific value ( see lthread_diag_api.h )
 */
#define DIAG_EVENT(obj, ev, p1, p2) do {				\
	struct lthread *ct = RTE_PER_LCORE(this_sched)->current_lthread;\
	if ((BIT(ev) & diag_mask) && (ev < LT_DIAG_EVENT_MAX)) {	\
		(diag_cb)(rte_rdtsc(),					\
				ct,					\
				ev,					\
				(obj)->diag_ref,			\
				diag_event_text[(ev)],			\
				(uint64_t)(p1),				\
				(uint64_t)(p2));			\
	}								\
} while (0)

#define DIAG_COUNT_DEFINE(x) rte_atomic64_t count_##x
#define DIAG_COUNT_INIT(o, x) rte_atomic64_init(&((o)->count_##x))
#define DIAG_COUNT_INC(o, x) rte_atomic64_inc(&((o)->count_##x))
#define DIAG_COUNT_DEC(o, x) rte_atomic64_dec(&((o)->count_##x))
#define DIAG_COUNT(o, x) rte_atomic64_read(&((o)->count_##x))

#define DIAG_USED

#else

/* no diagnostics configured */

#define DISPLAY_OBJCACHE_QUEUES 0

#define DIAG_CREATE_EVENT(obj, ev)
#define DIAG_EVENT(obj, ev, p1, p)

#define DIAG_COUNT_DEFINE(x)
#define DIAG_COUNT_INIT(o, x) do {} while (0)
#define DIAG_COUNT_INC(o, x) do {} while (0)
#define DIAG_COUNT_DEC(o, x) do {} while (0)
#define DIAG_COUNT(o, x) 0

#define DIAG_USED __rte_unused

#endif				/* LTHREAD_DIAG */

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_DIAG_H_ */
