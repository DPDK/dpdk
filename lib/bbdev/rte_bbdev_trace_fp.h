/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Intel Corporation
 */

#ifndef _RTE_BBDEV_TRACE_FP_H_
#define _RTE_BBDEV_TRACE_FP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

RTE_TRACE_POINT_FP(
	rte_bbdev_trace_enqueue,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, void **ops,
		uint16_t nb_ops, const char *op_string),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
	rte_trace_point_emit_string(op_string);
)

RTE_TRACE_POINT_FP(
	rte_bbdev_trace_dequeue,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, void **ops,
		uint16_t nb_ops, uint16_t nb_ops_deq, const char *op_string),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
	rte_trace_point_emit_u16(nb_ops_deq);
	rte_trace_point_emit_string(op_string);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BBDEV_TRACE_FP_H_ */
