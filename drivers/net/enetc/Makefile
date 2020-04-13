# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_enetc.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
EXPORT_MAP := rte_pmd_enetc_version.map
SRCS-$(CONFIG_RTE_LIBRTE_ENETC_PMD) += enetc_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENETC_PMD) += enetc_rxtx.c

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_ethdev -lrte_net
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_common_dpaax

include $(RTE_SDK)/mk/rte.lib.mk
