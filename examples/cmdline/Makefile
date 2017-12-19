# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = cmdline

# all source are stored in SRCS-y
SRCS-y := main.c commands.c parse_obj_list.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS_parse_obj_list.o := -D_GNU_SOURCE

include $(RTE_SDK)/mk/rte.extapp.mk
