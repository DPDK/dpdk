# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_net.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_mbuf -lrte_eal -lrte_mempool

EXPORT_MAP := rte_net_version.map
LIBABIVER := 1

SRCS-$(CONFIG_RTE_LIBRTE_NET) := rte_net.c
SRCS-$(CONFIG_RTE_LIBRTE_NET) += rte_net_crc.c
SRCS-$(CONFIG_RTE_LIBRTE_NET) += rte_arp.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_NET)-include := rte_ip.h rte_tcp.h rte_udp.h rte_esp.h
SYMLINK-$(CONFIG_RTE_LIBRTE_NET)-include += rte_sctp.h rte_icmp.h rte_arp.h
SYMLINK-$(CONFIG_RTE_LIBRTE_NET)-include += rte_ether.h rte_gre.h rte_net.h
SYMLINK-$(CONFIG_RTE_LIBRTE_NET)-include += rte_net_crc.h

include $(RTE_SDK)/mk/rte.lib.mk
