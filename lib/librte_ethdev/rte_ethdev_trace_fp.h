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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_TRACE_FP_H_ */
