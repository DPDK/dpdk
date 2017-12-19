# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = l3fwd-thread

# all source are stored in SRCS-y
SRCS-y := main.c

include $(RTE_SDK)/examples/performance-thread/common/common.mk

CFLAGS += -O3 -g $(USER_FLAGS) $(INCLUDES) $(WERROR_FLAGS)

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
#ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
#endif

include $(RTE_SDK)/mk/rte.extapp.mk
