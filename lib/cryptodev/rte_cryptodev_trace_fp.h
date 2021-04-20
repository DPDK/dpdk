/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_CRYPTODEV_TRACE_FP_H_
#define _RTE_CRYPTODEV_TRACE_FP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

RTE_TRACE_POINT_FP(
	rte_cryptodev_trace_enqueue_burst,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, void **ops,
		uint16_t nb_ops),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
)

RTE_TRACE_POINT_FP(
	rte_cryptodev_trace_dequeue_burst,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, void **ops,
		uint16_t nb_ops),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_TRACE_FP_H_ */
