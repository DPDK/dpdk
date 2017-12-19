# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = ethtool

# all source are stored in SRCS-y
SRCS-y := main.c ethapp.c

CFLAGS += -O3 -D_GNU_SOURCE -pthread -I$(SRCDIR)/../lib
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -L$(subst ethtool-app,lib,$(RTE_OUTPUT))/lib
LDLIBS += -lrte_ethtool

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif

include $(RTE_SDK)/mk/rte.extapp.mk
