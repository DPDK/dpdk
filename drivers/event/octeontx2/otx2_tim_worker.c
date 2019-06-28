/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_tim_evdev.h"
#include "otx2_tim_worker.h"

static inline int
tim_arm_checks(const struct otx2_tim_ring * const tim_ring,
	       struct rte_event_timer * const tim)
{
	if (unlikely(tim->state)) {
		tim->state = RTE_EVENT_TIMER_ERROR;
		rte_errno = EALREADY;
		goto fail;
	}

	if (unlikely(!tim->timeout_ticks ||
		     tim->timeout_ticks >= tim_ring->nb_bkts)) {
		tim->state = tim->timeout_ticks ? RTE_EVENT_TIMER_ERROR_TOOLATE
			: RTE_EVENT_TIMER_ERROR_TOOEARLY;
		rte_errno = EINVAL;
		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static inline void
tim_format_event(const struct rte_event_timer * const tim,
		 struct otx2_tim_ent * const entry)
{
	entry->w0 = (tim->ev.event & 0xFFC000000000) >> 6 |
		(tim->ev.event & 0xFFFFFFFFF);
	entry->wqe = tim->ev.u64;
}

static __rte_always_inline uint16_t
tim_timer_arm_burst(const struct rte_event_timer_adapter *adptr,
		    struct rte_event_timer **tim,
		    const uint16_t nb_timers,
		    const uint8_t flags)
{
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;
	struct otx2_tim_ent entry;
	uint16_t index;
	int ret;

	for (index = 0; index < nb_timers; index++) {
		if (tim_arm_checks(tim_ring, tim[index]))
			break;

		tim_format_event(tim[index], &entry);
		if (flags & OTX2_TIM_SP)
			ret = tim_add_entry_sp(tim_ring,
					       tim[index]->timeout_ticks,
					       tim[index], &entry, flags);
		if (flags & OTX2_TIM_MP)
			ret = tim_add_entry_mp(tim_ring,
					       tim[index]->timeout_ticks,
					       tim[index], &entry, flags);

		if (unlikely(ret)) {
			rte_errno = -ret;
			break;
		}
	}

	return index;
}

#define FP(_name, _f3, _f2, _f1, _flags)				  \
uint16_t __rte_noinline							  \
otx2_tim_arm_burst_ ## _name(const struct rte_event_timer_adapter *adptr, \
			     struct rte_event_timer **tim,		  \
			     const uint16_t nb_timers)			  \
{									  \
	return tim_timer_arm_burst(adptr, tim, nb_timers, _flags);	  \
}
TIM_ARM_FASTPATH_MODES
#undef FP
