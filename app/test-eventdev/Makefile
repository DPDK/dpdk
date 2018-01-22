# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

APP = dpdk-test-eventdev

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-y := evt_main.c
SRCS-y += evt_options.c
SRCS-y += evt_test.c
SRCS-y += parser.c

SRCS-y += test_order_common.c
SRCS-y += test_order_queue.c
SRCS-y += test_order_atq.c

SRCS-y += test_perf_common.c
SRCS-y += test_perf_queue.c
SRCS-y += test_perf_atq.c

SRCS-y += test_pipeline_common.c
SRCS-y += test_pipeline_queue.c
SRCS-y += test_pipeline_atq.c

include $(RTE_SDK)/mk/rte.app.mk
