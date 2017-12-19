# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_efd.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_ring -lrte_hash

EXPORT_MAP := rte_efd_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_EFD) := rte_efd.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_EFD)-include := rte_efd.h

include $(RTE_SDK)/mk/rte.lib.mk
