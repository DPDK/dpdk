# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_gro.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_ethdev -lrte_net

EXPORT_MAP := rte_gro_version.map

LIBABIVER := 1

# source files
SRCS-$(CONFIG_RTE_LIBRTE_GRO) += rte_gro.c
SRCS-$(CONFIG_RTE_LIBRTE_GRO) += gro_tcp4.c
SRCS-$(CONFIG_RTE_LIBRTE_GRO) += gro_vxlan_tcp4.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_GRO)-include += rte_gro.h

include $(RTE_SDK)/mk/rte.lib.mk
