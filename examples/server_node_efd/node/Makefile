# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = node

# all source are stored in SRCS-y
SRCS-y := node.c

CFLAGS += $(WERROR_FLAGS) -O3
CFLAGS += -I$(SRCDIR)/../shared

include $(RTE_SDK)/mk/rte.extapp.mk
