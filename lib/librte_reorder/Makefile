# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_reorder.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf

EXPORT_MAP := rte_reorder_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_REORDER) := rte_reorder.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_REORDER)-include := rte_reorder.h

include $(RTE_SDK)/mk/rte.lib.mk
