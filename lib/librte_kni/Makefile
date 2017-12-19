# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_kni.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3 -fno-strict-aliasing
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_ethdev

EXPORT_MAP := rte_kni_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_KNI) := rte_kni.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_KNI)-include := rte_kni.h

include $(RTE_SDK)/mk/rte.lib.mk
