# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

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
else

# binary name
APP = vhost-crypto

# all source are stored in SRCS-y
SRCS-y := main.c

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O2 -D_FILE_OFFSET_BITS=64
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -D_GNU_SOURCE

include $(RTE_SDK)/mk/rte.extapp.mk

endif
