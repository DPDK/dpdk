/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Intel Corporation
 */

#ifndef BBDEV_TRACE_H
#define BBDEV_TRACE_H

/**
 * @file
 *
 * API for bbdev trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

#include "rte_bbdev.h"

RTE_TRACE_POINT(
	rte_bbdev_trace_setup_queues,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t num_queues, int socket_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(num_queues);
	rte_trace_point_emit_int(socket_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_configure,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id, const char *op_str, uint8_t pri),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_string(op_str);
	rte_trace_point_emit_u8(pri);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_close,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
)

#ifdef __cplusplus
}
#endif

#endif /* BBDEV_TRACE_H */
