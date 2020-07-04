/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_trace_point_register.h>

#include "rte_eventdev_trace.h"

/* Eventdev trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_configure,
	lib.eventdev.configure)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_queue_setup,
	lib.eventdev.queue.setup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_setup,
	lib.eventdev.port.setup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_link,
	lib.eventdev.port.link)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_unlink,
	lib.eventdev.port.unlink)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_start,
	lib.eventdev.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_stop,
	lib.eventdev.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_close,
	lib.eventdev.close)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_enq_burst,
	lib.eventdev.enq.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_deq_burst,
	lib.eventdev.deq.burst)

/* Eventdev Rx adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_create,
	lib.eventdev.rx.adapter.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_free,
	lib.eventdev.rx.adapter.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_add,
	lib.eventdev.rx.adapter.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_del,
	lib.eventdev.rx.adapter.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_start,
	lib.eventdev.rx.adapter.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_stop,
	lib.eventdev.rx.adapter.stop)

/* Eventdev Tx adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_create,
	lib.eventdev.tx.adapter.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_free,
	lib.eventdev.tx.adapter.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_add,
	lib.eventdev.tx.adapter.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_del,
	lib.eventdev.tx.adapter.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_start,
	lib.eventdev.tx.adapter.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_stop,
	lib.eventdev.tx.adapter.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_enqueue,
	lib.eventdev.tx.adapter.enq)

/* Eventdev Timer adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_create,
	lib.eventdev.timer.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_start,
	lib.eventdev.timer.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_stop,
	lib.eventdev.timer.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_free,
	lib.eventdev.timer.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_arm_burst,
	lib.eventdev.timer.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_arm_tmo_tick_burst,
	lib.eventdev.timer.tick.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_cancel_burst,
	lib.eventdev.timer.cancel)

/* Eventdev Crypto adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_create,
	lib.eventdev.crypto.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_free,
	lib.eventdev.crypto.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_queue_pair_add,
	lib.eventdev.crypto.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_queue_pair_del,
	lib.eventdev.crypto.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_start,
	lib.eventdev.crypto.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_stop,
	lib.eventdev.crypto.stop)
