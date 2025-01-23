/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>
#include <rte_trace_point_register.h>
#include "vrb_trace.h"

RTE_LOG_REGISTER_SUFFIX(acc_common_logtype, common, INFO);

RTE_TRACE_POINT_REGISTER(rte_bbdev_vrb_trace_error,
	bbdev.vrb.device.error);

RTE_TRACE_POINT_REGISTER(rte_bbdev_vrb_trace_queue_error,
	bbdev.vrb.queue.error);
