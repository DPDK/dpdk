# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_ip_frag.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_ethdev
LDLIBS += -lrte_hash

EXPORT_MAP := rte_ip_frag_version.map

LIBABIVER := 1

#source files
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += rte_ipv4_fragmentation.c
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += rte_ipv6_fragmentation.c
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += rte_ipv4_reassembly.c
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += rte_ipv6_reassembly.c
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += rte_ip_frag_common.c
SRCS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += ip_frag_internal.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_IP_FRAG)-include += rte_ip_frag.h

include $(RTE_SDK)/mk/rte.lib.mk
