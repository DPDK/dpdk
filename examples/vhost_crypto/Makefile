# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
$(info This application can only operate in a linux environment, \
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

include $(RTE_SDK)/mk/rte.extapp.mk

endif
