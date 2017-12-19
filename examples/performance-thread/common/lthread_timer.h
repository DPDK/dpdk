/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


#ifndef LTHREAD_TIMER_H_
#define LTHREAD_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_int.h"
#include "lthread_sched.h"


static inline uint64_t
_ns_to_clks(uint64_t ns)
{
	/*
	 * clkns needs to be divided by 1E9 to get ns clocks. However,
	 * dividing by this first would lose a lot of accuracy.
	 * Dividing after a multiply by ns, could cause overflow of
	 * uint64_t if ns is about 5 seconds [if we assume a max tsc
	 * rate of 4GHz]. Therefore we first divide by 1E4, then
	 * multiply and finally divide by 1E5. This allows ns to be
	 * values many hours long, without overflow, while still keeping
	 * reasonable accuracy.
	 */
	uint64_t clkns = rte_get_tsc_hz() / 1e4;

	clkns *= ns;
	clkns /= 1e5;

	return clkns;
}


static inline void
_timer_start(struct lthread *lt, uint64_t clks)
{
	if (clks > 0) {
		DIAG_EVENT(lt, LT_DIAG_LTHREAD_TMR_START, &lt->tim, clks);
		rte_timer_init(&lt->tim);
		rte_timer_reset(&lt->tim,
				clks,
				SINGLE,
				rte_lcore_id(),
				_sched_timer_cb,
				(void *)lt);
	}
}


static inline void
_timer_stop(struct lthread *lt)
{
	if (lt != NULL) {
		DIAG_EVENT(lt, LT_DIAG_LTHREAD_TMR_DELETE, &lt->tim, 0);
		rte_timer_stop(&lt->tim);
	}
}

#ifdef __cplusplus
}
#endif

#endif /* LTHREAD_TIMER_H_ */
