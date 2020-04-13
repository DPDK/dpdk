# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_octeontx_zip.a

# build flags
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -O3
CFLAGS += -I$(RTE_SDK)/drivers/compress/octeontx/include

# external library include paths
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_compressdev
LDLIBS += -lrte_pci -lrte_bus_pci

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_ZIPVF) += otx_zip_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_ZIPVF) += otx_zip.c

# versioning export map
EXPORT_MAP := rte_pmd_octeontx_compress_version.map

include $(RTE_SDK)/mk/rte.lib.mk
