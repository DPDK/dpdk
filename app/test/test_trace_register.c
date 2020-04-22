/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */
#define RTE_TRACE_POINT_REGISTER_SELECT

#include "test_trace.h"

/* Define trace points */
RTE_TRACE_POINT_DEFINE(app_dpdk_test_tp);
RTE_TRACE_POINT_DEFINE(app_dpdk_test_fp);

RTE_INIT(register_valid_trace_points)
{
	RTE_TRACE_POINT_REGISTER(app_dpdk_test_tp, app.dpdk.test.tp);
	RTE_TRACE_POINT_REGISTER(app_dpdk_test_fp, app.dpdk.test.fp);
}

