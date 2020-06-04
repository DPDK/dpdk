# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_TEST_FLOW_PERF),y)

#
# library name
#
APP = dpdk-test-flow-perf

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-y += actions_gen.c
SRCS-y += flow_gen.c
SRCS-y += items_gen.c
SRCS-y += main.c

include $(RTE_SDK)/mk/rte.app.mk

endif
