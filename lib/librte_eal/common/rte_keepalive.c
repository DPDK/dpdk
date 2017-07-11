/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <inttypes.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_keepalive.h>
#include <rte_malloc.h>

struct rte_keepalive {
	/** Core Liveness. */
	enum rte_keepalive_state __rte_cache_aligned state_flags[
		RTE_KEEPALIVE_MAXCORES];

	/** Last-seen-alive timestamps */
	uint64_t last_alive[RTE_KEEPALIVE_MAXCORES];

	/**
	 * Cores to check.
	 * Indexed by core id, non-zero if the core should be checked.
	 */
	uint8_t active_cores[RTE_KEEPALIVE_MAXCORES];

	/** Dead core handler. */
	rte_keepalive_failure_callback_t callback;

	/**
	 * Dead core handler app data.
	 * Pointer is passed to dead core handler.
	 */
	void *callback_data;
	uint64_t tsc_initial;
	uint64_t tsc_mhz;

	/** Core state relay handler. */
	rte_keepalive_relay_callback_t relay_callback;

	/**
	 * Core state relay handler app data.
	 * Pointer is passed to live core handler.
	 */
	void *relay_callback_data;
};

static void
print_trace(const char *msg, struct rte_keepalive *keepcfg, int idx_core)
{
	RTE_LOG(INFO, EAL, "%sLast seen %" PRId64 "ms ago.\n",
		msg,
		((rte_rdtsc() - keepcfg->last_alive[idx_core])*1000)
		/ rte_get_tsc_hz()
	      );
}

void
rte_keepalive_dispatch_pings(__rte_unused void *ptr_timer,
	void *ptr_data)
{
	struct rte_keepalive *keepcfg = ptr_data;
	int idx_core;

	for (idx_core = 0; idx_core < RTE_KEEPALIVE_MAXCORES; idx_core++) {
		if (keepcfg->active_cores[idx_core] == 0)
			continue;

		switch (keepcfg->state_flags[idx_core]) {
		case RTE_KA_STATE_UNUSED:
			break;
		case RTE_KA_STATE_ALIVE: /* Alive */
			keepcfg->state_flags[idx_core] = RTE_KA_STATE_MISSING;
			keepcfg->last_alive[idx_core] = rte_rdtsc();
			break;
		case RTE_KA_STATE_MISSING: /* MIA */
			print_trace("Core MIA. ", keepcfg, idx_core);
			keepcfg->state_flags[idx_core] = RTE_KA_STATE_DEAD;
			break;
		case RTE_KA_STATE_DEAD: /* Dead */
			keepcfg->state_flags[idx_core] = RTE_KA_STATE_GONE;
			print_trace("Core died. ", keepcfg, idx_core);
			if (keepcfg->callback)
				keepcfg->callback(
					keepcfg->callback_data,
					idx_core
					);
			break;
		case RTE_KA_STATE_GONE: /* Buried */
			break;
		case RTE_KA_STATE_DOZING: /* Core going idle */
			keepcfg->state_flags[idx_core] = RTE_KA_STATE_SLEEP;
			keepcfg->last_alive[idx_core] = rte_rdtsc();
			break;
		case RTE_KA_STATE_SLEEP: /* Idled core */
			break;
		}
		if (keepcfg->relay_callback)
			keepcfg->relay_callback(
				keepcfg->relay_callback_data,
				idx_core,
				keepcfg->state_flags[idx_core],
				keepcfg->last_alive[idx_core]
				);
	}
}

struct rte_keepalive *
rte_keepalive_create(rte_keepalive_failure_callback_t callback,
	void *data)
{
	struct rte_keepalive *keepcfg;

	keepcfg = rte_zmalloc("RTE_EAL_KEEPALIVE",
		sizeof(struct rte_keepalive),
		RTE_CACHE_LINE_SIZE);
	if (keepcfg != NULL) {
		keepcfg->callback = callback;
		keepcfg->callback_data = data;
		keepcfg->tsc_initial = rte_rdtsc();
		keepcfg->tsc_mhz = rte_get_tsc_hz() / 1000;
	}
	return keepcfg;
}

void rte_keepalive_register_relay_callback(struct rte_keepalive *keepcfg,
	rte_keepalive_relay_callback_t callback,
	void *data)
{
	keepcfg->relay_callback = callback;
	keepcfg->relay_callback_data = data;
}

void
rte_keepalive_register_core(struct rte_keepalive *keepcfg, const int id_core)
{
	if (id_core < RTE_KEEPALIVE_MAXCORES) {
		keepcfg->active_cores[id_core] = RTE_KA_STATE_ALIVE;
		keepcfg->last_alive[id_core] = rte_rdtsc();
	}
}

void
rte_keepalive_mark_alive(struct rte_keepalive *keepcfg)
{
	keepcfg->state_flags[rte_lcore_id()] = RTE_KA_STATE_ALIVE;
}

void
rte_keepalive_mark_sleep(struct rte_keepalive *keepcfg)
{
	keepcfg->state_flags[rte_lcore_id()] = RTE_KA_STATE_DOZING;
}
