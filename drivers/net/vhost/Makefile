# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_vhost.a

LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_vhost
LDLIBS += -lrte_bus_vdev

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_vhost_version.map

LIBABIVER := 2

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_VHOST) += rte_eth_vhost.c

#
# Export include files
#
SYMLINK-y-include += rte_eth_vhost.h

include $(RTE_SDK)/mk/rte.lib.mk
