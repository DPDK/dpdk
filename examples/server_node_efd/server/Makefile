# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
$(error This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# binary name
APP = server

# all source are stored in SRCS-y
SRCS-y := main.c init.c args.c

INC := $(sort $(wildcard *.h))

CFLAGS += $(WERROR_FLAGS) -O3
CFLAGS += -I$(SRCDIR)/../shared

include $(RTE_SDK)/mk/rte.extapp.mk
