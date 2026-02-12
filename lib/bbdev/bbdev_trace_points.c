/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Intel Corporation
 */

#include <eal_export.h>
#include <rte_trace_point_register.h>

#include "bbdev_trace.h"

RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_setup_queues,
	lib.bbdev.queue.setup)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_queue_configure,
	lib.bbdev.queue.configure)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_start,
	lib.bbdev.start)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_stop,
	lib.bbdev.stop)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_close,
	lib.bbdev.close)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_queue_start,
	lib.bbdev.queue.start)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_queue_stop,
	lib.bbdev.queue.stop)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_ldpc_dec,
	lib.bbdev.op.ldpc_dec);
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_ldpc_enc,
	lib.bbdev.op.ldpc_enc);
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_turbo_dec,
	lib.bbdev.op.turbo_dec);
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_turbo_enc,
	lib.bbdev.op.turbo_enc);
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_fft,
	lib.bbdev.op.fft);
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_op_mldts,
	lib.bbdev.op.mldts);

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_bbdev_trace_enqueue, 25.03)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_enqueue,
	lib.bbdev.enq)
RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_bbdev_trace_dequeue, 25.03)
RTE_TRACE_POINT_REGISTER(rte_bbdev_trace_dequeue,
	lib.bbdev.deq)
