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

static inline void
tim_sync_start_cyc(struct otx2_tim_ring *tim_ring)
{
	uint64_t cur_cyc = rte_rdtsc();
	uint32_t real_bkt;

	if (cur_cyc - tim_ring->last_updt_cyc > tim_ring->tot_int) {
		real_bkt = otx2_read64(tim_ring->base + TIM_LF_RING_REL) >> 44;
		cur_cyc = rte_rdtsc();

		tim_ring->ring_start_cyc = cur_cyc -
						(real_bkt * tim_ring->tck_int);
		tim_ring->last_updt_cyc = cur_cyc;
	}

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

	tim_sync_start_cyc(tim_ring);
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

	if (flags & OTX2_TIM_ENA_STATS)
		__atomic_fetch_add(&tim_ring->arm_cnt, index, __ATOMIC_RELAXED);

	return index;
}

static __rte_always_inline uint16_t
tim_timer_arm_tmo_brst(const struct rte_event_timer_adapter *adptr,
		       struct rte_event_timer **tim,
		       const uint64_t timeout_tick,
		       const uint16_t nb_timers, const uint8_t flags)
{
	struct otx2_tim_ent entry[OTX2_TIM_MAX_BURST] __rte_cache_aligned;
	struct otx2_tim_ring *tim_ring = adptr->data->adapter_priv;
	uint16_t set_timers = 0;
	uint16_t arr_idx = 0;
	uint16_t idx;
	int ret;

	if (unlikely(!timeout_tick || timeout_tick >= tim_ring->nb_bkts)) {
		const enum rte_event_timer_state state = timeout_tick ?
			RTE_EVENT_TIMER_ERROR_TOOLATE :
			RTE_EVENT_TIMER_ERROR_TOOEARLY;
		for (idx = 0; idx < nb_timers; idx++)
			tim[idx]->state = state;

		rte_errno = EINVAL;
		return 0;
	}

	tim_sync_start_cyc(tim_ring);
	while (arr_idx < nb_timers) {
		for (idx = 0; idx < OTX2_TIM_MAX_BURST && (arr_idx < nb_timers);
		     idx++, arr_idx++) {
			tim_format_event(tim[arr_idx], &entry[idx]);
		}
		ret = tim_add_entry_brst(tim_ring, timeout_tick,
					 &tim[set_timers], entry, idx, flags);
		set_timers += ret;
		if (ret != idx)
			break;
	}
	if (flags & OTX2_TIM_ENA_STATS)
		__atomic_fetch_add(&tim_ring->arm_cnt, set_timers,
				   __ATOMIC_RELAXED);

	return set_timers;
}

#define FP(_name, _f4, _f3, _f2, _f1, _flags)				\
uint16_t __rte_noinline							  \
otx2_tim_arm_burst_ ## _name(const struct rte_event_timer_adapter *adptr, \
			     struct rte_event_timer **tim,		  \
			     const uint16_t nb_timers)			  \
{									  \
	return tim_timer_arm_burst(adptr, tim, nb_timers, _flags);	  \
}
TIM_ARM_FASTPATH_MODES
#undef FP

#define FP(_name, _f3, _f2, _f1, _flags)				\
uint16_t __rte_noinline							\
otx2_tim_arm_tmo_tick_burst_ ## _name(					\
			const struct rte_event_timer_adapter *adptr,	\
				      struct rte_event_timer **tim,	\
				      const uint64_t timeout_tick,	\
				      const uint16_t nb_timers)		\
{									\
	return tim_timer_arm_tmo_brst(adptr, tim, timeout_tick,		\
			nb_timers, _flags);				\
}
TIM_ARM_TMO_FASTPATH_MODES
#undef FP

uint16_t
otx2_tim_timer_cancel_burst(const struct rte_event_timer_adapter *adptr,
			    struct rte_event_timer **tim,
			    const uint16_t nb_timers)
{
	uint16_t index;
	int ret;

	RTE_SET_USED(adptr);
	for (index = 0; index < nb_timers; index++) {
		if (tim[index]->state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		}

		if (tim[index]->state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}
		ret = tim_rm_entry(tim[index]);
		if (ret) {
			rte_errno = -ret;
			break;
		}
	}

	return index;
}
