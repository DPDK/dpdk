# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_isal_comp.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

# external library dependencies
LDLIBS += -lisal
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_compressdev
LDLIBS += -lrte_bus_vdev

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_isal_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += isal_compress_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += isal_compress_pmd_ops.c

# export include files
include $(RTE_SDK)/mk/rte.lib.mk
