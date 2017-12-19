# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV),"linuxapp")
$(info This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
all:
clean:
else

# binary name
APP = qos_sched

# all source are stored in SRCS-y
SRCS-y := main.c args.c init.c app_thread.c cfg_file.c cmdline.c stats.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS_args.o := -D_GNU_SOURCE
CFLAGS_cfg_file.o := -D_GNU_SOURCE

include $(RTE_SDK)/mk/rte.extapp.mk

endif
