# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
$(error This application can only operate in a linux environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# library name
LIB = librte_ethtool.a

LIBABIVER := 1

# all source are stored in SRC-Y
SRCS-y := rte_ethtool.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif
LDLIBS += -lrte_eal -lrte_ethdev

include $(RTE_SDK)/mk/rte.extlib.mk
