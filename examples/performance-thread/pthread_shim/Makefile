# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = lthread_pthread_shim

# all source are stored in SRCS-y
SRCS-y := main.c  pthread_shim.c
INCLUDES := -I$(RTE_SDK)/$(RTE_TARGET)/include -I$(SRCDIR)
include $(RTE_SDK)/examples/performance-thread/common/common.mk

CFLAGS += -g -O3 $(USER_FLAGS) $(INCLUDES)
CFLAGS += $(WERROR_FLAGS)

LDFLAGS += -lpthread

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

include $(RTE_SDK)/mk/rte.extapp.mk
