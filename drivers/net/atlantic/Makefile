# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Aquantia Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_atlantic.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_atlantic_version.map

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net
LDLIBS += -lrte_bus_pci

VPATH += $(SRCDIR)/hw_atl

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += atl_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += atl_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += atl_hw_regs.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += hw_atl_utils.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += hw_atl_llh.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += hw_atl_utils_fw2x.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += hw_atl_b0.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += rte_pmd_atlantic.c

include $(RTE_SDK)/mk/rte.lib.mk
