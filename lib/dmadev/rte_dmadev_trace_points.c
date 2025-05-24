/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 HiSilicon Limited
 */

#include <eal_export.h>
#include <rte_trace_point_register.h>

#include "rte_dmadev_trace.h"

RTE_TRACE_POINT_REGISTER(rte_dma_trace_info_get,
	lib.dmadev.info_get)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_configure,
	lib.dmadev.configure)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_start,
	lib.dmadev.start)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_stop,
	lib.dmadev.stop)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_close,
	lib.dmadev.close)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_vchan_setup,
	lib.dmadev.vchan_setup)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_stats_get,
	lib.dmadev.stats_get)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_stats_reset,
	lib.dmadev.stats_reset)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_vchan_status,
	lib.dmadev.vchan_status)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_dump,
	lib.dmadev.dump)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_copy, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_copy,
	lib.dmadev.copy)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_copy_sg, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_copy_sg,
	lib.dmadev.copy_sg)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_fill, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_fill,
	lib.dmadev.fill)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_submit, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_submit,
	lib.dmadev.submit)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_completed, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_completed,
	lib.dmadev.completed)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_completed_status, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_completed_status,
	lib.dmadev.completed_status)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_burst_capacity, 24.03)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_burst_capacity,
	lib.dmadev.burst_capacity)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_enqueue_ops, 25.11)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_enqueue_ops,
	lib.dmadev.enqueue_ops)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_dma_trace_dequeue_ops, 25.11)
RTE_TRACE_POINT_REGISTER(rte_dma_trace_dequeue_ops,
	lib.dmadev.dequeue_ops)
