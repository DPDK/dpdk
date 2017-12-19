# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = ka-agent

# all source are stored in SRCS-y
SRCS-y := main.c

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)/../
LDFLAGS += -lrt
include $(RTE_SDK)/mk/rte.extapp.mk
