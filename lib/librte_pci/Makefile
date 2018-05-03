# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 6WIND S.A.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pci.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += $(WERROR_FLAGS) -O3
LDLIBS += -lrte_eal

EXPORT_MAP := rte_pci_version.map

LIBABIVER := 1

SRCS-$(CONFIG_RTE_LIBRTE_PCI) += rte_pci.c

SYMLINK-$(CONFIG_RTE_LIBRTE_PCI)-include += rte_pci.h

include $(RTE_SDK)/mk/rte.lib.mk
