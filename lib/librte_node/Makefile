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
LDLIBS += -lrte_eal -lrte_graph -lrte_mbuf -lrte_lpm -lrte_ethdev -lrte_mempool

EXPORT_MAP := rte_node_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += null.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += log.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ethdev_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ethdev_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ethdev_ctrl.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ip4_lookup.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += ip4_rewrite.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += pkt_cls.c
SRCS-$(CONFIG_RTE_LIBRTE_NODE) += pkt_drop.c

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_NODE)-include += rte_node_ip4_api.h
SYMLINK-$(CONFIG_RTE_LIBRTE_NODE)-include += rte_node_eth_api.h

include $(RTE_SDK)/mk/rte.lib.mk
