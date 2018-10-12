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

LIBABIVER := 1

LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev -lrte_net
LDLIBS += -lrte_bus_pci

VPATH += $(SRCDIR)/hw_atl

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += atl_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += atl_hw_regs.c
SRCS-$(CONFIG_RTE_LIBRTE_ATLANTIC_PMD) += hw_atl_llh.c

include $(RTE_SDK)/mk/rte.lib.mk
