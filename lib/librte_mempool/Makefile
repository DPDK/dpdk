# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_mempool.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal -lrte_ring

EXPORT_MAP := rte_mempool_version.map

LIBABIVER := 3

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool_ops.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMPOOL)-include := rte_mempool.h

include $(RTE_SDK)/mk/rte.lib.mk
