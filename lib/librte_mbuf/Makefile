# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_mbuf.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
CFLAGS += -DALLOW_EXPERIMENTAL_API

LDLIBS += -lrte_eal -lrte_mempool

EXPORT_MAP := rte_mbuf_version.map

LIBABIVER := 5

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MBUF) := rte_mbuf.c rte_mbuf_ptype.c rte_mbuf_pool_ops.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MBUF)-include := rte_mbuf.h rte_mbuf_ptype.h rte_mbuf_pool_ops.h

include $(RTE_SDK)/mk/rte.lib.mk
