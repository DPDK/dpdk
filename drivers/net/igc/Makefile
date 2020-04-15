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

EXPORT_MAP := rte_pmd_igc_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_logs.c
SRCS-$(CONFIG_RTE_LIBRTE_IGC_PMD) += igc_ethdev.c

include $(RTE_SDK)/mk/rte.lib.mk
