/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Ericsson AB
 */

#ifndef _TEST_ATOMIC_COMMON_H
#define _TEST_ATOMIC_COMMON_H

#include <stdio.h>
#include <stdbool.h>

#include <rte_eventdev.h>

#include "evt_common.h"
#include "evt_options.h"
#include "evt_test.h"

#include "test_order_common.h"

#define IDLE_TIMEOUT 1

static inline uint32_t
get_lock_idx(int stage, flow_id_t flow, uint32_t nb_flows)
{
	return (stage * nb_flows) + flow;
}

static inline bool
atomic_spinlock_trylock(rte_spinlock_t atomic_locks[],
		uint32_t stage,
		uint32_t flow,
		uint32_t nb_flows)
{
	return rte_spinlock_trylock(&atomic_locks[get_lock_idx(stage, flow, nb_flows)]);
}

static inline void
atomic_spinlock_unlock(rte_spinlock_t atomic_locks[],
		uint32_t stage,
		uint32_t flow,
		uint32_t nb_flows)
{
	rte_spinlock_unlock(&atomic_locks[get_lock_idx(stage, flow, nb_flows)]);
}

static inline void
atomic_lock_verify(rte_spinlock_t atomic_locks[],
		uint32_t stage,
		uint32_t flow,
		uint32_t nb_flows,
		struct test_order *const t,
		uint32_t port)
{
	if (!atomic_spinlock_trylock(atomic_locks, stage, flow, nb_flows)) {

		evt_err("q=%u, flow=%x atomicity error: port %u tried to take held spinlock %p",
				stage, flow, port,
				&atomic_locks[get_lock_idx(stage, flow, nb_flows)]);
		t->err = true;
	}
}

static inline rte_spinlock_t *
atomic_init_locks(uint32_t nb_stages, uint32_t nb_flows)
{
	const uint32_t num_locks = nb_stages * nb_flows;

	rte_spinlock_t *atomic_locks = rte_calloc(NULL, num_locks, sizeof(rte_spinlock_t), 0);

	if (atomic_locks == NULL) {
		evt_err("Unable to allocate memory for spinlocks.");
		return NULL;
	}

	for (uint32_t i = 0; i < num_locks; i++)
		rte_spinlock_init(&atomic_locks[i]);

	return atomic_locks;
}

static inline flow_id_t *
order_mbuf_flow_id(struct test_order *t, struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, t->flow_id_dynfield_offset, flow_id_t *);
}

int atomic_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *));

#endif
