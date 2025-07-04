/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"

#include "cnxk_tim_evdev.h"
#include "cnxk_tim_worker.h"

static inline int
cnxk_tim_arm_checks(const struct cnxk_tim_ring *const tim_ring,
		    struct rte_event_timer *const tim)
{
	if (unlikely(tim->state)) {
		tim->state = RTE_EVENT_TIMER_ERROR;
		rte_errno = EALREADY;
		goto fail;
	}

	if (unlikely(!tim->timeout_ticks ||
		     tim->timeout_ticks > tim_ring->nb_bkts)) {
		tim->state = tim->timeout_ticks ?
					   RTE_EVENT_TIMER_ERROR_TOOLATE :
					   RTE_EVENT_TIMER_ERROR_TOOEARLY;
		rte_errno = EINVAL;
		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static __rte_always_inline uint16_t
cnxk_tim_timer_arm_burst(const struct rte_event_timer_adapter *adptr,
			 struct rte_event_timer **tim, const uint16_t nb_timers,
			 const uint8_t flags)
{
	struct cnxk_tim_ring *tim_ring = adptr->data->adapter_priv;
	struct cnxk_tim_ent entry;
	uint16_t index;
	int ret = 0;

	for (index = 0; index < nb_timers; index++) {
		if (cnxk_tim_arm_checks(tim_ring, tim[index]))
			break;

		cnxk_tim_format_event(tim[index], &entry);
		if (flags & CNXK_TIM_SP)
			ret = cnxk_tim_add_entry_sp(tim_ring,
						    tim[index]->timeout_ticks,
						    tim[index], &entry, flags);
		if (flags & CNXK_TIM_MP)
			ret = cnxk_tim_add_entry_mp(tim_ring,
						    tim[index]->timeout_ticks,
						    tim[index], &entry, flags);

		if (unlikely(ret)) {
			rte_errno = -ret;
			break;
		}
	}

	if (flags & CNXK_TIM_ENA_STATS)
		rte_atomic_fetch_add_explicit(&tim_ring->arm_cnt, index, rte_memory_order_relaxed);

	return index;
}

uint16_t
cnxk_tim_arm_burst_hwwqe(const struct rte_event_timer_adapter *adptr, struct rte_event_timer **tim,
			 const uint16_t nb_timers)
{
	struct cnxk_tim_ring *tim_ring = adptr->data->adapter_priv;
	uint16_t index;

	for (index = 0; index < nb_timers; index++) {
		if (cnxk_tim_arm_checks(tim_ring, tim[index]))
			break;

		if (cnxk_tim_add_entry_hwwqe(tim_ring, tim[index]))
			break;
	}

	return index;
}

#define FP(_name, _f3, _f2, _f1, _flags)                                       \
	uint16_t __rte_noinline cnxk_tim_arm_burst_##_name(                    \
		const struct rte_event_timer_adapter *adptr,                   \
		struct rte_event_timer **tim, const uint16_t nb_timers)        \
	{                                                                      \
		return cnxk_tim_timer_arm_burst(adptr, tim, nb_timers,         \
						_flags);                       \
	}
TIM_ARM_FASTPATH_MODES
#undef FP

static __rte_always_inline uint16_t
cnxk_tim_timer_arm_tmo_brst(const struct rte_event_timer_adapter *adptr,
			    struct rte_event_timer **tim,
			    const uint64_t timeout_tick,
			    const uint16_t nb_timers, const uint8_t flags)
{
	alignas(RTE_CACHE_LINE_SIZE) struct cnxk_tim_ent entry[CNXK_TIM_MAX_BURST];
	struct cnxk_tim_ring *tim_ring = adptr->data->adapter_priv;
	uint16_t set_timers = 0;
	uint16_t arr_idx = 0;
	uint16_t idx;
	int ret;

	if (unlikely(!timeout_tick || timeout_tick > tim_ring->nb_bkts)) {
		const enum rte_event_timer_state state =
			timeout_tick ? RTE_EVENT_TIMER_ERROR_TOOLATE :
					     RTE_EVENT_TIMER_ERROR_TOOEARLY;
		for (idx = 0; idx < nb_timers; idx++)
			tim[idx]->state = state;

		rte_errno = EINVAL;
		return 0;
	}

	while (arr_idx < nb_timers) {
		for (idx = 0; idx < CNXK_TIM_MAX_BURST && (arr_idx < nb_timers);
		     idx++, arr_idx++) {
			cnxk_tim_format_event(tim[arr_idx], &entry[idx]);
		}
		ret = cnxk_tim_add_entry_brst(tim_ring, timeout_tick,
					      &tim[set_timers], entry, idx,
					      flags);
		set_timers += ret;
		if (ret != idx)
			break;
	}

	if (flags & CNXK_TIM_ENA_STATS)
		rte_atomic_fetch_add_explicit(&tim_ring->arm_cnt, set_timers,
					      rte_memory_order_relaxed);

	return set_timers;
}

uint16_t
cnxk_tim_arm_tmo_burst_hwwqe(const struct rte_event_timer_adapter *adptr,
			     struct rte_event_timer **tim, const uint64_t timeout_tick,
			     const uint16_t nb_timers)
{
	struct cnxk_tim_ring *tim_ring = adptr->data->adapter_priv;
	uint16_t idx;

	if (unlikely(!timeout_tick || timeout_tick > tim_ring->nb_bkts)) {
		const enum rte_event_timer_state state = timeout_tick ?
								 RTE_EVENT_TIMER_ERROR_TOOLATE :
								 RTE_EVENT_TIMER_ERROR_TOOEARLY;
		for (idx = 0; idx < nb_timers; idx++)
			tim[idx]->state = state;

		rte_errno = EINVAL;
		return 0;
	}

	return cnxk_tim_add_entry_tmo_hwwqe(tim_ring, tim, timeout_tick * tim_ring->tck_int,
					    nb_timers);
}

#define FP(_name, _f2, _f1, _flags)                                            \
	uint16_t __rte_noinline cnxk_tim_arm_tmo_tick_burst_##_name(           \
		const struct rte_event_timer_adapter *adptr,                   \
		struct rte_event_timer **tim, const uint64_t timeout_tick,     \
		const uint16_t nb_timers)                                      \
	{                                                                      \
		return cnxk_tim_timer_arm_tmo_brst(adptr, tim, timeout_tick,   \
						   nb_timers, _flags);         \
	}
TIM_ARM_TMO_FASTPATH_MODES
#undef FP

uint16_t
cnxk_tim_timer_cancel_burst(const struct rte_event_timer_adapter *adptr,
			    struct rte_event_timer **tim,
			    const uint16_t nb_timers)
{
	uint16_t index;
	int ret;

	RTE_SET_USED(adptr);
	rte_atomic_thread_fence(rte_memory_order_acquire);
	for (index = 0; index < nb_timers; index++) {
		if (tim[index]->state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		}

		if (tim[index]->state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}
		ret = cnxk_tim_rm_entry(tim[index]);
		if (ret) {
			rte_errno = -ret;
			break;
		}
	}

	return index;
}

uint16_t
cnxk_tim_timer_cancel_burst_hwwqe(const struct rte_event_timer_adapter *adptr,
				  struct rte_event_timer **tim, const uint16_t nb_timers)
{
	uint64_t __rte_atomic *status;
	uint16_t i;

	RTE_SET_USED(adptr);
	for (i = 0; i < nb_timers; i++) {
		if (tim[i]->state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		}

		if (tim[i]->state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}

		status = (uint64_t __rte_atomic *)&tim[i]->impl_opaque[1];
		if (!rte_atomic_compare_exchange_strong_explicit(status, (uint64_t *)&tim[i], 0,
								 rte_memory_order_release,
								 rte_memory_order_relaxed)) {
			rte_errno = ENOENT;
			break;
		}
		tim[i]->state = RTE_EVENT_TIMER_CANCELED;
	}

	return i;
}

int
cnxk_tim_remaining_ticks_get(const struct rte_event_timer_adapter *adapter,
			     const struct rte_event_timer *evtim, uint64_t *ticks_remaining)
{
	struct cnxk_tim_ring *tim_ring = adapter->data->adapter_priv;
	struct cnxk_tim_bkt *bkt, *current_bkt;
	struct cnxk_tim_ent *entry;
	uint64_t bkt_cyc, bucket;
	uint64_t sema;

	if (evtim->impl_opaque[1] == 0 || evtim->impl_opaque[0] == 0)
		return -ENOENT;

	entry = (struct cnxk_tim_ent *)(uintptr_t)evtim->impl_opaque[0];
	if (entry->wqe != evtim->ev.u64)
		return -ENOENT;

	if (evtim->state != RTE_EVENT_TIMER_ARMED)
		return -ENOENT;

	bkt = (struct cnxk_tim_bkt *)evtim->impl_opaque[1];
	sema = rte_atomic_load_explicit(&bkt->w1, rte_memory_order_acquire);
	if (cnxk_tim_bkt_get_hbt(sema) || !cnxk_tim_bkt_get_nent(sema))
		return -ENOENT;

	bkt_cyc = tim_ring->tick_fn(tim_ring->tbase) - tim_ring->ring_start_cyc;
	bucket = rte_reciprocal_divide_u64(bkt_cyc, &tim_ring->fast_div);
	current_bkt = &tim_ring->bkt[bucket];

	*ticks_remaining = RTE_MAX(bkt, current_bkt) - RTE_MIN(bkt, current_bkt);
	/* Assume that the current bucket is yet to expire */
	*ticks_remaining += 1;
	return 0;
}
