# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_kni.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_kni
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_kni_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_KNI) += rte_eth_kni.c

#
# Export include files
#
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
