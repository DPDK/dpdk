/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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


#include <rte_common.h>
#include <rte_branch_prediction.h>

#include "ssovf_evdev.h"

enum {
	SSO_SYNC_ORDERED,
	SSO_SYNC_ATOMIC,
	SSO_SYNC_UNTAGGED,
	SSO_SYNC_EMPTY
};

#ifndef __hot
#define __hot	__attribute__((hot))
#endif

/* SSO Operations */

static __rte_always_inline uint16_t
ssows_get_work(struct ssows *ws, struct rte_event *ev)
{
	uint64_t get_work0, get_work1;
	uint64_t sched_type_queue;

	ssovf_load_pair(get_work0, get_work1, ws->getwork);

	sched_type_queue = (get_work0 >> 32) & 0xfff;
	ws->cur_tt = sched_type_queue & 0x3;
	ws->cur_grp = sched_type_queue >> 2;
	sched_type_queue = sched_type_queue << 38;

	ev->event = sched_type_queue | (get_work0 & 0xffffffff);
	ev->u64 = get_work1;
	return !!get_work1;
}

static __rte_always_inline void
ssows_add_work(struct ssows *ws, const uint64_t event_ptr, const uint32_t tag,
			const uint8_t new_tt, const uint8_t grp)
{
	uint64_t add_work0;

	add_work0 = tag | ((uint64_t)(new_tt) << 32);
	ssovf_store_pair(add_work0, event_ptr, ws->grps[grp]);
}

static __rte_always_inline void
ssows_swtag_full(struct ssows *ws, const uint64_t event_ptr, const uint32_t tag,
			const uint8_t new_tt, const uint8_t grp)
{
	uint64_t swtag_full0;

	swtag_full0 = tag | ((uint64_t)(new_tt & 0x3) << 32) |
				((uint64_t)grp << 34);
	ssovf_store_pair(swtag_full0, event_ptr, (ws->base +
				SSOW_VHWS_OP_SWTAG_FULL0));
}

static __rte_always_inline void
ssows_swtag_desched(struct ssows *ws, uint32_t tag, uint8_t new_tt, uint8_t grp)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32) | ((uint64_t)grp << 34);
	ssovf_write64(val, ws->base + SSOW_VHWS_OP_SWTAG_DESCHED);
}

static __rte_always_inline void
ssows_swtag_norm(struct ssows *ws, uint32_t tag, uint8_t new_tt)
{
	uint64_t val;

	val = tag | ((uint64_t)(new_tt & 0x3) << 32);
	ssovf_write64(val, ws->base + SSOW_VHWS_OP_SWTAG_NORM);
}

static __rte_always_inline void
ssows_swtag_untag(struct ssows *ws)
{
	ssovf_write64(0, ws->base + SSOW_VHWS_OP_SWTAG_UNTAG);
	ws->cur_tt = SSO_SYNC_UNTAGGED;
}

static __rte_always_inline void
ssows_upd_wqp(struct ssows *ws, uint8_t grp, uint64_t event_ptr)
{
	ssovf_store_pair((uint64_t)grp << 34, event_ptr, (ws->base +
				SSOW_VHWS_OP_UPD_WQP_GRP0));
}

static __rte_always_inline void
ssows_desched(struct ssows *ws)
{
	ssovf_write64(0, ws->base + SSOW_VHWS_OP_DESCHED);
}

static __rte_always_inline void
ssows_swtag_wait(struct ssows *ws)
{
	/* Wait for the SWTAG/SWTAG_FULL operation */
	while (ssovf_read64(ws->base + SSOW_VHWS_SWTP))
	;
}
