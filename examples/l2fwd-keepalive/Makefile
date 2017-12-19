# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = l2fwd-keepalive

# all source are stored in SRCS-y
SRCS-y := main.c shm.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDFLAGS += -lrt

include $(RTE_SDK)/mk/rte.extapp.mk
