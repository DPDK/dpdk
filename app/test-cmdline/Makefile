# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_LIBRTE_CMDLINE),y)

#
# library name
#
APP = cmdline_test

#
# all sources are stored in SRCS-y
#
SRCS-y += cmdline_test.c
SRCS-y += commands.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.app.mk

endif
