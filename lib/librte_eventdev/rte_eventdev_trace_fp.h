/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_EVENTDEV_TRACE_FP_H_
#define _RTE_EVENTDEV_TRACE_FP_H_

/**
 * @file
 *
 * API for ethdev trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_deq_burst,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id, void *ev_table,
		uint16_t nb_events),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_ptr(ev_table);
	rte_trace_point_emit_u16(nb_events);
)

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_enq_burst,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		const void *ev_table, uint16_t nb_events, void *enq_mode_cb),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_ptr(ev_table);
	rte_trace_point_emit_u16(nb_events);
	rte_trace_point_emit_ptr(enq_mode_cb);
)

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_eth_tx_adapter_enqueue,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id, void *ev_table,
		uint16_t nb_events, const uint8_t flags),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_ptr(ev_table);
	rte_trace_point_emit_u16(nb_events);
	rte_trace_point_emit_u8(flags);
)

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_timer_arm_burst,
	RTE_TRACE_POINT_ARGS(const void *adapter, void **evtims_table,
		uint16_t nb_evtims),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(evtims_table);
	rte_trace_point_emit_u16(nb_evtims);
)

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_timer_arm_tmo_tick_burst,
	RTE_TRACE_POINT_ARGS(const void *adapter, const uint64_t timeout_ticks,
		void **evtims_table, const uint16_t nb_evtims),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_u64(timeout_ticks);
	rte_trace_point_emit_ptr(evtims_table);
	rte_trace_point_emit_u16(nb_evtims);
)

RTE_TRACE_POINT_FP(
	rte_eventdev_trace_timer_cancel_burst,
	RTE_TRACE_POINT_ARGS(const void *adapter, void **evtims_table,
		uint16_t nb_evtims),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(evtims_table);
	rte_trace_point_emit_u16(nb_evtims);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_TRACE_FP_H_ */
