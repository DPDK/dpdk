# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_mbuf.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3

LDLIBS += -lrte_eal -lrte_mempool

EXPORT_MAP := rte_mbuf_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MBUF) := rte_mbuf.c rte_mbuf_ptype.c rte_mbuf_pool_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_MBUF) += rte_mbuf_dyn.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include := rte_mbuf.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include += rte_mbuf_core.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include += rte_mbuf_ptype.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include += rte_mbuf_pool_ops.h
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include += rte_mbuf_dyn.h

include $(RTE_SDK)/mk/rte.lib.mk
