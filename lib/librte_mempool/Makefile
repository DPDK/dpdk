# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_mempool.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal -lrte_ring

EXPORT_MAP := rte_mempool_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  rte_mempool_ops_default.c
SRCS-$(CONFIG_RTE_LIBRTE_MEMPOOL) +=  mempool_trace_points.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMPOOL)-include := rte_mempool.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMPOOL)-include += rte_mempool_trace.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMPOOL)-include += rte_mempool_trace_fp.h

include $(RTE_SDK)/mk/rte.lib.mk
