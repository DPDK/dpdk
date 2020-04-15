# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_igc.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_mbuf
LDLIBS += -lrte_mempool

EXPORT_MAP := rte_pmd_igc_version.map

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_api.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_base.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_i225.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_manage.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_nvm.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_osdep.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_logs.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_txrx.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_flow.c

include $(RTE_SDK)/mk/rte.lib.mk
