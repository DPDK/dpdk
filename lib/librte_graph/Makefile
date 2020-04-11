# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_graph.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal

EXPORT_MAP := rte_graph_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_GRAPH) += node.c
SRCS-$(CONFIG_RTE_LIBRTE_GRAPH) += graph.c
SRCS-$(CONFIG_RTE_LIBRTE_GRAPH) += graph_debug.c

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_GRAPH)-include += rte_graph.h

include $(RTE_SDK)/mk/rte.lib.mk
