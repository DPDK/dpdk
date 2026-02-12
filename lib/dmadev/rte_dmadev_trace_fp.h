/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#ifndef RTE_DMADEV_TRACE_FP_H
#define RTE_DMADEV_TRACE_FP_H

/**
 * @file
 *
 * API for dmadev fastpath trace support
 */

#include <rte_trace_point.h>

#ifdef __cplusplus
extern "C" {
#endif

RTE_TRACE_POINT_FP(
	rte_dma_trace_stats_get,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     struct rte_dma_stats *stats, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u64(stats->submitted);
	rte_trace_point_emit_u64(stats->completed);
	rte_trace_point_emit_u64(stats->errors);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_vchan_status,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     enum rte_dma_vchan_status *status, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_int_ptr(status);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_copy,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, rte_iova_t src,
			     rte_iova_t dst, uint32_t length, uint64_t flags,
			     int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u64(src);
	rte_trace_point_emit_u64(dst);
	rte_trace_point_emit_u32(length);
	rte_trace_point_emit_u64(flags);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_copy_sg,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     struct rte_dma_sge *src, struct rte_dma_sge *dst,
			     uint16_t nb_src, uint16_t nb_dst, uint64_t flags,
			     int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_ptr(src);
	rte_trace_point_emit_ptr(dst);
	rte_trace_point_emit_u16(nb_src);
	rte_trace_point_emit_u16(nb_dst);
	rte_trace_point_emit_u64(flags);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_fill,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, uint64_t pattern,
			     rte_iova_t dst, uint32_t length, uint64_t flags,
			     int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u64(pattern);
	rte_trace_point_emit_u64(dst);
	rte_trace_point_emit_u32(length);
	rte_trace_point_emit_u64(flags);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_submit,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_completed,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     const uint16_t nb_cpls, uint16_t *last_idx,
			     bool *has_error, uint16_t ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u16(nb_cpls);
	rte_trace_point_emit_u16_ptr(last_idx);
	rte_trace_point_emit_u8_ptr(has_error);
	rte_trace_point_emit_u16(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_completed_status,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     const uint16_t nb_cpls, uint16_t *last_idx,
			     enum rte_dma_status_code *status, uint16_t ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u16(nb_cpls);
	rte_trace_point_emit_u16_ptr(last_idx);
	rte_trace_point_emit_ptr(status);
	rte_trace_point_emit_u16(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_burst_capacity,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, uint16_t ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_u16(ret);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_enqueue_ops,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, void **ops,
			     uint16_t nb_ops),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
)

RTE_TRACE_POINT_FP(
	rte_dma_trace_dequeue_ops,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, void **ops,
			     uint16_t nb_ops),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_ptr(ops);
	rte_trace_point_emit_u16(nb_ops);
)

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_TRACE_FP_H */
