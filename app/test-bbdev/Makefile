# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
APP = testbbdev

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all sources are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_TEST_BBDEV) += main.c
SRCS-$(CONFIG_RTE_TEST_BBDEV) += test_bbdev.c
SRCS-$(CONFIG_RTE_TEST_BBDEV) += test_bbdev_perf.c
SRCS-$(CONFIG_RTE_TEST_BBDEV) += test_bbdev_vector.c

LDLIBS += -lm

include $(RTE_SDK)/mk/rte.app.mk
