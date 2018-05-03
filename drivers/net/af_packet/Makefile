# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 John W. Linville <linville@redhat.com>
# Copyright(c) 2010-2014 Intel Corporation.
# Copyright(c) 2014 6WIND S.A.
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_af_packet.a

EXPORT_MAP := rte_pmd_af_packet_version.map

LIBABIVER := 1

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vdev

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_AF_PACKET) += rte_eth_af_packet.c

include $(RTE_SDK)/mk/rte.lib.mk
