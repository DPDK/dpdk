# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation.
# Copyright(c) 2014 6WIND S.A.
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_pcap.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lpcap
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_pcap_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_PCAP) += rte_eth_pcap.c

#
# Export include files
#
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
