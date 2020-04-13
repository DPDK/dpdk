# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ena.a
CFLAGS += $(WERROR_FLAGS) -O2
INCLUDES :=-I$(SRCDIR) -I$(SRCDIR)/base/ena_defs -I$(SRCDIR)/base

EXPORT_MAP := rte_pmd_ena_version.map

VPATH += $(SRCDIR)/base
#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ENA_PMD) += ena_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENA_PMD) += ena_com.c
SRCS-$(CONFIG_RTE_LIBRTE_ENA_PMD) += ena_eth_com.c

CFLAGS += $(INCLUDES)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_timer

include $(RTE_SDK)/mk/rte.lib.mk
