/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_ETHDEV_TRACE_FP_H_
#define _RTE_ETHDEV_TRACE_FP_H_

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
	rte_ethdev_trace_rx_burst,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id,
		void **pkt_tbl, uint16_t nb_rx),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_ptr(pkt_tbl);
	rte_trace_point_emit_u16(nb_rx);
)

RTE_TRACE_POINT_FP(
	rte_ethdev_trace_tx_burst,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id,
		void **pkts_tbl, uint16_t nb_pkts),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_ptr(pkts_tbl);
	rte_trace_point_emit_u16(nb_pkts);
)

RTE_TRACE_POINT_FP(
	rte_eth_trace_call_rx_callbacks,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id,
		void **rx_pkts, uint16_t nb_rx, uint16_t nb_pkts),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_ptr(rx_pkts);
	rte_trace_point_emit_u16(nb_rx);
	rte_trace_point_emit_u16(nb_pkts);
)

RTE_TRACE_POINT_FP(
	rte_eth_trace_call_tx_callbacks,
	RTE_TRACE_POINT_ARGS(uint16_t port_id, uint16_t queue_id,
		void **tx_pkts, uint16_t nb_pkts),
	rte_trace_point_emit_u16(port_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_ptr(tx_pkts);
	rte_trace_point_emit_u16(nb_pkts);
)

RTE_TRACE_POINT_FP(
	rte_eth_trace_tx_buffer_drop_callback,
	RTE_TRACE_POINT_ARGS(void **pkts, uint16_t unsent),
	rte_trace_point_emit_ptr(pkts);
	rte_trace_point_emit_u16(unsent);
)

RTE_TRACE_POINT_FP(
	rte_eth_trace_tx_buffer_count_callback,
	RTE_TRACE_POINT_ARGS(void **pkts, uint16_t unsent, uint64_t count),
	rte_trace_point_emit_ptr(pkts);
	rte_trace_point_emit_u16(unsent);
	rte_trace_point_emit_u64(count);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_TRACE_FP_H_ */
