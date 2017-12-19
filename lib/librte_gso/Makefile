# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_gso.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal -lrte_mbuf -lrte_ethdev -lrte_net
LDLIBS += -lrte_mempool

EXPORT_MAP := rte_gso_version.map

LIBABIVER := 1

#source files
SRCS-$(CONFIG_RTE_LIBRTE_GSO) += rte_gso.c
SRCS-$(CONFIG_RTE_LIBRTE_GSO) += gso_common.c
SRCS-$(CONFIG_RTE_LIBRTE_GSO) += gso_tcp4.c
SRCS-$(CONFIG_RTE_LIBRTE_GSO) += gso_tunnel_tcp4.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_GSO)-include += rte_gso.h

include $(RTE_SDK)/mk/rte.lib.mk
