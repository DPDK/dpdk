# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 6WIND S.A.

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_bus_pci.a
LIBABIVER := 2
EXPORT_MAP := rte_bus_pci_version.map

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),)
SYSTEM := linux
endif
ifneq ($(CONFIG_RTE_EXEC_ENV_FREEBSD),)
SYSTEM := bsd
endif

CFLAGS += -I$(RTE_SDK)/drivers/bus/pci/$(SYSTEM)
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common

# memseg walk is not part of stable API yet
CFLAGS += -DALLOW_EXPERIMENTAL_API

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_pci -lrte_kvargs

include $(RTE_SDK)/drivers/bus/pci/$(SYSTEM)/Makefile
SRCS-$(CONFIG_RTE_LIBRTE_PCI_BUS) := $(addprefix $(SYSTEM)/,$(SRCS))
SRCS-$(CONFIG_RTE_LIBRTE_PCI_BUS) += pci_params.c
SRCS-$(CONFIG_RTE_LIBRTE_PCI_BUS) += pci_common.c
SRCS-$(CONFIG_RTE_LIBRTE_PCI_BUS) += pci_common_uio.c

SYMLINK-$(CONFIG_RTE_LIBRTE_PCI_BUS)-include += rte_bus_pci.h

include $(RTE_SDK)/mk/rte.lib.mk
