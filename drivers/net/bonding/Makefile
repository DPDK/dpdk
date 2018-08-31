# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_bond.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_cmdline
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_bond_version.map

LIBABIVER := 2

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_api.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_args.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_8023ad.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_alb.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += rte_eth_bond_flow.c

#
# Export include files
#
SYMLINK-y-include += rte_eth_bond.h
SYMLINK-y-include += rte_eth_bond_8023ad.h

include $(RTE_SDK)/mk/rte.lib.mk
