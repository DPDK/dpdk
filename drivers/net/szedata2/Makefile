# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 CESNET

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_szedata2.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lsze2
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_szedata2_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2) += rte_eth_szedata2.c

#
# Export include files
#
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
