/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 HiSilicon Limited
 */

#ifndef RTE_DMADEV_TRACE_H
#define RTE_DMADEV_TRACE_H

/**
 * @file
 *
 * API for dmadev trace support.
 */

#include <rte_trace_point.h>

#include "rte_dmadev.h"

#ifdef __cplusplus
extern "C" {
#endif

RTE_TRACE_POINT(
	rte_dma_trace_info_get,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, struct rte_dma_info *dev_info),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_string(dev_info->dev_name);
	rte_trace_point_emit_u64(dev_info->dev_capa);
	rte_trace_point_emit_u16(dev_info->max_vchans);
	rte_trace_point_emit_u16(dev_info->max_desc);
	rte_trace_point_emit_u16(dev_info->min_desc);
	rte_trace_point_emit_u16(dev_info->max_sges);
	rte_trace_point_emit_i16(dev_info->numa_node);
	rte_trace_point_emit_u16(dev_info->nb_vchans);
	rte_trace_point_emit_u16(dev_info->nb_priorities);
)

RTE_TRACE_POINT(
	rte_dma_trace_configure,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, const struct rte_dma_conf *dev_conf,
			     int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(dev_conf->nb_vchans);
	rte_trace_point_emit_u16(dev_conf->priority);
	rte_trace_point_emit_u64(dev_conf->flags);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_start,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_stop,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_close,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_vchan_setup,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan,
			     const struct rte_dma_vchan_conf *conf, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_int(conf->direction);
	rte_trace_point_emit_u16(conf->nb_desc);
	rte_trace_point_emit_int(conf->src_port.port_type);
	rte_trace_point_emit_u64(conf->src_port.pcie.val);
	rte_trace_point_emit_int(conf->dst_port.port_type);
	rte_trace_point_emit_u64(conf->dst_port.pcie.val);
	rte_trace_point_emit_ptr(conf->auto_free.m2d.pool);
	rte_trace_point_emit_int(conf->domain.type);
	rte_trace_point_emit_u16(conf->domain.src_handler);
	rte_trace_point_emit_u16(conf->domain.dst_handler);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_stats_reset,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, uint16_t vchan, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_u16(vchan);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_dma_trace_dump,
	RTE_TRACE_POINT_ARGS(int16_t dev_id, FILE *f, int ret),
	rte_trace_point_emit_i16(dev_id);
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_int(ret);
)

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_TRACE_H */
