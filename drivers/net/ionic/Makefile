# SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
# Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ionic.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_ionic_version.map

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net
LDLIBS += -lrte_bus_pci

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_mac_api.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rx_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_lif.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_main.c

include $(RTE_SDK)/mk/rte.lib.mk
