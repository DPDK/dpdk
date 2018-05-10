#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_ccp.a

# build flags
CFLAGS += -O3
CFLAGS += -I$(SRCDIR)
CFLAGS += $(WERROR_FLAGS)

# library version
LIBABIVER := 1

# external library include paths
LDLIBS += -lcrypto
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_kvargs

# versioning export map
EXPORT_MAP := rte_pmd_ccp_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CCP) += rte_ccp_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CCP) += ccp_crypto.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CCP) += ccp_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CCP) += ccp_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CCP) += ccp_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
