# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ring.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_ring_version.map

LIBABIVER := 2

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_RING) += rte_eth_ring.c

#
# Export include files
#
SYMLINK-y-include += rte_eth_ring.h

include $(RTE_SDK)/mk/rte.lib.mk
