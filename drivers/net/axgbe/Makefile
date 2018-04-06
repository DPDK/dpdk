#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_axgbe.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_axgbe_version.map

LIBABIVER := 1

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_ethdev

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_mdio.c
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_phy_impl.c
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_i2c.c
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_rxtx.c
ifeq ($(CONFIG_RTE_ARCH_X86),y)
SRCS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD) += axgbe_rxtx_vec_sse.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
