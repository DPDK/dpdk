# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium Networks

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_zlib.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_pmd_zlib_version.map

# external library dependencies
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring -lz
LDLIBS += -lrte_compressdev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ZLIB) += zlib_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ZLIB) += zlib_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
