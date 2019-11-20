# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Cesnet
# Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_nfb.a

INCLUDES :=-I$(SRCDIR)


CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += $(shell command -v pkg-config > /dev/null 2>&1 && pkg-config --cflags netcope-common)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_kvargs
LDLIBS += -lrte_ethdev -lrte_net
LDLIBS += -lrte_bus_pci
LDLIBS += $(shell command -v pkg-config > /dev/null 2>&1 && pkg-config --libs netcope-common)

EXPORT_MAP := rte_pmd_nfb_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_NFB_PMD) += nfb_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_NFB_PMD) += nfb_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_NFB_PMD) += nfb_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_NFB_PMD) += nfb_stats.c
SRCS-$(CONFIG_RTE_LIBRTE_NFB_PMD) += nfb_rxmode.c

#
# Export include files
#
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
