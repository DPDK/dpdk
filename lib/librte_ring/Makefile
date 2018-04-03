# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_ring.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal

EXPORT_MAP := rte_ring_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_RING) := rte_ring.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_RING)-include := rte_ring.h \
					rte_ring_generic.h \
					rte_ring_c11_mem.h

include $(RTE_SDK)/mk/rte.lib.mk
