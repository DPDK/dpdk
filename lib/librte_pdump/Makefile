# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pdump.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
CFLAGS += -D_GNU_SOURCE
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_ethdev

EXPORT_MAP := rte_pdump_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_PDUMP) := rte_pdump.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_PDUMP)-include := rte_pdump.h

include $(RTE_SDK)/mk/rte.lib.mk
