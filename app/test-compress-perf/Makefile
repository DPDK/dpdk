# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

APP = dpdk-test-compress-perf

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -O3

# all source are stored in SRCS-y
SRCS-y := main.c
SRCS-y += comp_perf_options_parse.c
SRCS-y += comp_perf_test_verify.c
SRCS-y += comp_perf_test_throughput.c
SRCS-y += comp_perf_test_cyclecount.c
SRCS-y += comp_perf_test_common.c

include $(RTE_SDK)/mk/rte.app.mk
