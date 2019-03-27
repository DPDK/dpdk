# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = ethtool

# all source are stored in SRCS-y
SRCS-y := main.c ethapp.c

CFLAGS += -O3 -pthread -I$(SRCDIR)/../lib
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -L$(subst ethtool-app,lib,$(RTE_OUTPUT))/lib
LDLIBS += -lrte_ethtool

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif

include $(RTE_SDK)/mk/rte.extapp.mk
