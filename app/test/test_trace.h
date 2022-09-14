/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */
#include <rte_trace_point.h>

extern int app_dpdk_test_tp_count;
RTE_TRACE_POINT(
	app_dpdk_test_tp,
	RTE_TRACE_POINT_ARGS(const char *str),
	rte_trace_point_emit_string(str);
	app_dpdk_test_tp_count++;
)

RTE_TRACE_POINT_FP(
	app_dpdk_test_fp,
	RTE_TRACE_POINT_ARGS(void),
)
