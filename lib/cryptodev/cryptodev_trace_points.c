/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_trace_point_register.h>

#include "rte_cryptodev_trace.h"

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_configure,
	lib.cryptodev.configure)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_start,
	lib.cryptodev.start)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_stop,
	lib.cryptodev.stop)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_close,
	lib.cryptodev.close)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_queue_pair_setup,
	lib.cryptodev.queue.pair.setup)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_pool_create,
	lib.cryptodev.sym.pool.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_pool_create,
	lib.cryptodev.asym.pool.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_create,
	lib.cryptodev.sym.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_create,
	lib.cryptodev.asym.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_free,
	lib.cryptodev.sym.free)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_free,
	lib.cryptodev.asym.free)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_init,
	lib.cryptodev.sym.init)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_clear,
	lib.cryptodev.sym.clear)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_enqueue_burst,
	lib.cryptodev.enq.burst)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_dequeue_burst,
	lib.cryptodev.deq.burst)
