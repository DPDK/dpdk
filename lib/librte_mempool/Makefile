# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_mempool.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
# Allow deprecated symbol to use deprecated rte_mempool_populate_iova_tab()
# from earlier deprecated rte_mempool_populate_phys_tab()
CFLAGS += -Wno-deprecated-declarations
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDLIBS += -lrte_eal -lrte_ring

EXPORT_MAP := rte_mempool_version.map

LIBABIVER := 4

# memseg walk is not yet part of stable API
CFLAGS += -DALLOW_EXPERIMENTAL_API

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool_ops_default.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMPOOL)-include := rte_mempool.h

include $(RTE_SDK)/mk/rte.lib.mk
