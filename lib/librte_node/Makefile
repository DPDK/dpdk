# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_node.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
# Strict-aliasing rules are violated by uint8_t[] to context size casts.
CFLAGS += -fno-strict-aliasing
LDLIBS += -lrte_eal -lrte_graph -lrte_ethdev

EXPORT_MAP := rte_node_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += null.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += log.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ethdev_rx.c

include $(RTE_SDK)/mk/rte.lib.mk
