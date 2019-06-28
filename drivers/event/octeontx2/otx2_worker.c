/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_worker.h"

static __rte_noinline uint8_t
otx2_ssogws_new_event(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint64_t event_ptr = ev->u64;
	const uint16_t grp = ev->queue_id;

	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	otx2_ssogws_add_work(ws, event_ptr, tag, new_tt, grp);

	return 1;
}

static __rte_always_inline void
otx2_ssogws_fwd_swtag(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t cur_tt = ws->cur_tt;

	/* 96XX model
	 * cur_tt/new_tt     SSO_SYNC_ORDERED SSO_SYNC_ATOMIC SSO_SYNC_UNTAGGED
	 *
	 * SSO_SYNC_ORDERED        norm           norm             untag
	 * SSO_SYNC_ATOMIC         norm           norm		   untag
	 * SSO_SYNC_UNTAGGED       norm           norm             NOOP
	 */

	if (new_tt == SSO_SYNC_UNTAGGED) {
		if (cur_tt != SSO_SYNC_UNTAGGED)
			otx2_ssogws_swtag_untag(ws);
	} else {
		otx2_ssogws_swtag_norm(ws, tag, new_tt);
	}

	ws->swtag_req = 1;
}

static __rte_always_inline void
otx2_ssogws_fwd_group(struct otx2_ssogws *ws, const struct rte_event *ev,
		      const uint16_t grp)
{
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;

	otx2_write64(ev->u64, OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op) +
		     SSOW_LF_GWS_OP_UPD_WQP_GRP1);
	rte_smp_wmb();
	otx2_ssogws_swtag_desched(ws, tag, new_tt, grp);
}

static __rte_always_inline void
otx2_ssogws_forward_event(struct otx2_ssogws *ws, const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (ws->cur_grp == grp)
		otx2_ssogws_fwd_swtag(ws, ev);
	else
	/*
	 * Group has been changed for group based work pipelining,
	 * Use deschedule/add_work operation to transfer the event to
	 * new group/core
	 */
		otx2_ssogws_fwd_group(ws, ev, grp);
}

static __rte_always_inline void
otx2_ssogws_release_event(struct otx2_ssogws *ws)
{
	otx2_ssogws_swtag_flush(ws);
}

uint16_t __hot
otx2_ssogws_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks)
{
	struct otx2_ssogws *ws = port;

	RTE_SET_USED(timeout_ticks);

	if (ws->swtag_req) {
		ws->swtag_req = 0;
		otx2_ssogws_swtag_wait(ws);
		return 1;
	}

	return otx2_ssogws_get_work(ws, ev);
}

uint16_t __hot
otx2_ssogws_deq_burst(void *port, struct rte_event ev[], uint16_t nb_events,
		      uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return otx2_ssogws_deq(port, ev, timeout_ticks);
}

uint16_t __hot
otx2_ssogws_deq_timeout(void *port, struct rte_event *ev,
			uint64_t timeout_ticks)
{
	struct otx2_ssogws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	if (ws->swtag_req) {
		ws->swtag_req = 0;
		otx2_ssogws_swtag_wait(ws);
		return ret;
	}

	ret = otx2_ssogws_get_work(ws, ev);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = otx2_ssogws_get_work(ws, ev);

	return ret;
}

uint16_t __hot
otx2_ssogws_deq_timeout_burst(void *port, struct rte_event ev[],
			      uint16_t nb_events, uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return otx2_ssogws_deq_timeout(port, ev, timeout_ticks);
}

uint16_t __hot
otx2_ssogws_enq(void *port, const struct rte_event *ev)
{
	struct otx2_ssogws *ws = port;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		rte_smp_mb();
		return otx2_ssogws_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		otx2_ssogws_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		otx2_ssogws_release_event(ws);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __hot
otx2_ssogws_enq_burst(void *port, const struct rte_event ev[],
		      uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return otx2_ssogws_enq(port, ev);
}

uint16_t __hot
otx2_ssogws_enq_new_burst(void *port, const struct rte_event ev[],
			  uint16_t nb_events)
{
	struct otx2_ssogws *ws = port;
	uint16_t i, rc = 1;

	rte_smp_mb();
	if (ws->xaq_lmt <= *ws->fc_mem)
		return 0;

	for (i = 0; i < nb_events && rc; i++)
		rc = otx2_ssogws_new_event(ws,  &ev[i]);

	return nb_events;
}

uint16_t __hot
otx2_ssogws_enq_fwd_burst(void *port, const struct rte_event ev[],
			  uint16_t nb_events)
{
	struct otx2_ssogws *ws = port;

	RTE_SET_USED(nb_events);
	otx2_ssogws_forward_event(ws,  ev);

	return 1;
}
