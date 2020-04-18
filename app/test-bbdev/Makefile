# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
APP = testbbdev

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
ifeq ($(CONFIG_RTE_LIBRTE_PMD_BBDEV_FPGA_LTE_FEC),y)
LDLIBS += -lrte_pmd_bbdev_fpga_lte_fec
endif
ifeq ($(CONFIG_RTE_LIBRTE_PMD_BBDEV_FPGA_5GNR_FEC),y)
LDLIBS += -lrte_pmd_bbdev_fpga_5gnr_fec
endif

include $(RTE_SDK)/mk/rte.app.mk
