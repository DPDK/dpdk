/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef VRB_TRACE_H_
#define VRB_TRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

RTE_TRACE_POINT(
	rte_bbdev_vrb_trace_error,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const char *op_string, const char *err_string),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_string(op_string);
	rte_trace_point_emit_string(err_string);
)

RTE_TRACE_POINT(
	rte_bbdev_vrb_trace_queue_error,
	RTE_TRACE_POINT_ARGS(uint8_t qg_id, uint8_t aq_id, const char *str),
	rte_trace_point_emit_u8(qg_id);
	rte_trace_point_emit_u8(aq_id);
	rte_trace_point_emit_string(str);
)


#ifdef __cplusplus
}
#endif

#endif /* VRB_TRACE_H_ */
