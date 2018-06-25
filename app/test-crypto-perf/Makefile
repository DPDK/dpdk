# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk


APP = dpdk-test-crypto-perf

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3

# all source are stored in SRCS-y
SRCS-y := main.c
SRCS-y += cperf_ops.c
SRCS-y += cperf_options_parsing.c
SRCS-y += cperf_test_vectors.c
SRCS-y += cperf_test_throughput.c
SRCS-y += cperf_test_latency.c
SRCS-y += cperf_test_pmd_cyclecount.c
SRCS-y += cperf_test_verify.c
SRCS-y += cperf_test_vector_parsing.c
SRCS-y += cperf_test_common.c

ifeq ($(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER),y)
LDLIBS += -lrte_pmd_crypto_scheduler
endif

include $(RTE_SDK)/mk/rte.app.mk
