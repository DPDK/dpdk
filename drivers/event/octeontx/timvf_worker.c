/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include "timvf_worker.h"

static inline int
timvf_timer_reg_checks(const struct timvf_ring * const timr,
		struct rte_event_timer * const tim)
{
	if (unlikely(tim->state)) {
		tim->state = RTE_EVENT_TIMER_ERROR;
		rte_errno = EALREADY;
		goto fail;
	}

	if (unlikely(!tim->timeout_ticks ||
				tim->timeout_ticks >= timr->nb_bkts)) {
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
timvf_format_event(const struct rte_event_timer * const tim,
		struct tim_mem_entry * const entry)
{
	entry->w0 = (tim->ev.event & 0xFFC000000000) >> 6 |
		(tim->ev.event & 0xFFFFFFFFF);
	entry->wqe = tim->ev.u64;
}

uint16_t
timvf_timer_cancel_burst(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers)
{
	RTE_SET_USED(adptr);
	int ret;
	uint16_t index;

	for (index = 0; index < nb_timers; index++) {
		if (tim[index]->state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		}

		if (tim[index]->state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}
		ret = timvf_rem_entry(tim[index]);
		if (ret) {
			rte_errno = -ret;
			break;
		}
	}
	return index;
}

uint16_t
timvf_timer_arm_burst_sp(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers)
{
	int ret;
	uint16_t index;
	struct tim_mem_entry entry;
	struct timvf_ring *timr = adptr->data->adapter_priv;
	for (index = 0; index < nb_timers; index++) {
		if (timvf_timer_reg_checks(timr, tim[index]))
			break;

		timvf_format_event(tim[index], &entry);
		ret = timvf_add_entry_sp(timr, tim[index]->timeout_ticks,
				tim[index], &entry);
		if (unlikely(ret)) {
			rte_errno = -ret;
			break;
		}
	}

	return index;
}

uint16_t
timvf_timer_arm_burst_sp_stats(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers)
{
	uint16_t ret;
	struct timvf_ring *timr = adptr->data->adapter_priv;

	ret = timvf_timer_arm_burst_sp(adptr, tim, nb_timers);
	timr->tim_arm_cnt += ret;

	return ret;
}

uint16_t
timvf_timer_arm_burst_mp(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers)
{
	int ret;
	uint16_t index;
	struct tim_mem_entry entry;
	struct timvf_ring *timr = adptr->data->adapter_priv;
	for (index = 0; index < nb_timers; index++) {
		if (timvf_timer_reg_checks(timr, tim[index]))
			break;
		timvf_format_event(tim[index], &entry);
		ret = timvf_add_entry_mp(timr, tim[index]->timeout_ticks,
				tim[index], &entry);
		if (unlikely(ret)) {
			rte_errno = -ret;
			break;
		}
	}

	return index;
}

uint16_t
timvf_timer_arm_burst_mp_stats(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint16_t nb_timers)
{
	uint16_t ret;
	struct timvf_ring *timr = adptr->data->adapter_priv;

	ret = timvf_timer_arm_burst_mp(adptr, tim, nb_timers);
	timr->tim_arm_cnt += ret;

	return ret;
}

uint16_t
timvf_timer_arm_tmo_brst(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint64_t timeout_tick,
		const uint16_t nb_timers)
{
	int ret;
	uint16_t set_timers = 0;
	uint16_t idx;
	uint16_t arr_idx = 0;
	struct timvf_ring *timr = adptr->data->adapter_priv;
	struct tim_mem_entry entry[TIMVF_MAX_BURST] __rte_cache_aligned;

	if (unlikely(!timeout_tick || timeout_tick >= timr->nb_bkts)) {
		const enum rte_event_timer_state state = timeout_tick ?
			RTE_EVENT_TIMER_ERROR_TOOLATE :
			RTE_EVENT_TIMER_ERROR_TOOEARLY;
		for (idx = 0; idx < nb_timers; idx++)
			tim[idx]->state = state;
		rte_errno = EINVAL;
		return 0;
	}

	while (arr_idx < nb_timers) {
		for (idx = 0; idx < TIMVF_MAX_BURST && (arr_idx < nb_timers);
				idx++, arr_idx++) {
			timvf_format_event(tim[arr_idx], &entry[idx]);
		}
		ret = timvf_add_entry_brst(timr, timeout_tick, &tim[set_timers],
				entry, idx);
		set_timers += ret;
		if (ret != idx)
			break;
	}

	return set_timers;
}


uint16_t
timvf_timer_arm_tmo_brst_stats(const struct rte_event_timer_adapter *adptr,
		struct rte_event_timer **tim, const uint64_t timeout_tick,
		const uint16_t nb_timers)
{
	uint16_t set_timers;
	struct timvf_ring *timr = adptr->data->adapter_priv;

	set_timers = timvf_timer_arm_tmo_brst(adptr, tim, timeout_tick,
			nb_timers);
	timr->tim_arm_cnt += set_timers;

	return set_timers;
}

void
timvf_set_chunk_refill(struct timvf_ring * const timr, uint8_t use_fpa)
{
	if (use_fpa)
		timr->refill_chunk = timvf_refill_chunk_fpa;
	else
		timr->refill_chunk = timvf_refill_chunk_generic;
}
