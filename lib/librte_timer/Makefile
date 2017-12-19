# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_timer.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal

EXPORT_MAP := rte_timer_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_TIMER) := rte_timer.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_TIMER)-include := rte_timer.h

include $(RTE_SDK)/mk/rte.lib.mk
