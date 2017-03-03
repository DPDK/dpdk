/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ssovf_worker.h"

static force_inline void
ssows_new_event(struct ssows *ws, const struct rte_event *ev)
{
	const uint64_t event_ptr = ev->u64;
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t new_tt = ev->sched_type;
	const uint8_t grp = ev->queue_id;

	ssows_add_work(ws, event_ptr, tag, new_tt, grp);
}

static force_inline void
ssows_fwd_swtag(struct ssows *ws, const struct rte_event *ev, const uint8_t grp)
{
	const uint8_t cur_tt = ws->cur_tt;
	const uint8_t new_tt = ev->sched_type;
	const uint32_t tag = (uint32_t)ev->event;
	/*
	 * cur_tt/new_tt     SSO_SYNC_ORDERED SSO_SYNC_ATOMIC SSO_SYNC_UNTAGGED
	 *
	 * SSO_SYNC_ORDERED        norm           norm             untag
	 * SSO_SYNC_ATOMIC         norm           norm		   untag
	 * SSO_SYNC_UNTAGGED       full           full             NOOP
	 */
	if (unlikely(cur_tt == SSO_SYNC_UNTAGGED)) {
		if (new_tt != SSO_SYNC_UNTAGGED) {
			ssows_swtag_full(ws, ev->u64, tag,
				new_tt, grp);
		}
	} else {
		if (likely(new_tt != SSO_SYNC_UNTAGGED))
			ssows_swtag_norm(ws, tag, new_tt);
		else
			ssows_swtag_untag(ws);
	}
	ws->swtag_req = 1;
}

#define OCT_EVENT_TYPE_GRP_FWD (RTE_EVENT_TYPE_MAX - 1)

static force_inline void
ssows_fwd_group(struct ssows *ws, const struct rte_event *ev, const uint8_t grp)
{
	const uint64_t event_ptr = ev->u64;
	const uint32_t tag = (uint32_t)ev->event;
	const uint8_t cur_tt = ws->cur_tt;
	const uint8_t new_tt = ev->sched_type;

	if (cur_tt == SSO_SYNC_ORDERED) {
		/* Create unique tag based on custom event type and new grp */
		uint32_t newtag = OCT_EVENT_TYPE_GRP_FWD << 28;

		newtag |= grp << 20;
		newtag |= tag;
		ssows_swtag_norm(ws, newtag, SSO_SYNC_ATOMIC);
		rte_smp_wmb();
		ssows_swtag_wait(ws);
	} else {
		rte_smp_wmb();
	}
	ssows_add_work(ws, event_ptr, tag, new_tt, grp);
}

static force_inline void
ssows_forward_event(struct ssows *ws, const struct rte_event *ev)
{
	const uint8_t grp = ev->queue_id;

	/* Group hasn't changed, Use SWTAG to forward the event */
	if (ws->cur_grp == grp)
		ssows_fwd_swtag(ws, ev, grp);
	else
	/*
	 * Group has been changed for group based work pipelining,
	 * Use deschedule/add_work operation to transfer the event to
	 * new group/core
	 */
		ssows_fwd_group(ws, ev, grp);
}

static force_inline void
ssows_release_event(struct ssows *ws)
{
	if (likely(ws->cur_tt != SSO_SYNC_UNTAGGED))
		ssows_swtag_untag(ws);
}

force_inline uint16_t __hot
ssows_enq(void *port, const struct rte_event *ev)
{
	struct ssows *ws = port;
	uint16_t ret = 1;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		ssows_new_event(ws, ev);
		break;
	case RTE_EVENT_OP_FORWARD:
		ssows_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		ssows_release_event(ws);
		break;
	default:
		ret = 0;
	}
	return ret;
}

uint16_t __hot
ssows_enq_burst(void *port, const struct rte_event ev[], uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return ssows_enq(port, ev);
}
