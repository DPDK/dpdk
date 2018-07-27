# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2014 - 2018 Cavium Inc.
# All rights reserved.
# www.cavium.com
include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_bnx2x.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DZLIB_CONST
LDLIBS += -lz
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_bnx2x_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_stats.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += ecore_sp.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += elink.c
SRCS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x_vfpf.c

include $(RTE_SDK)/mk/rte.lib.mk
