/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 HiSilicon Limited
 */

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

RTE_TRACE_POINT_REGISTER(rte_dma_trace_stats_reset,
	lib.dmadev.stats_reset)

RTE_TRACE_POINT_REGISTER(rte_dma_trace_dump,
	lib.dmadev.dump)
