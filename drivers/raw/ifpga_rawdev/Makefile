# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ifpga_rawdev.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/ifpga
CFLAGS += -I$(RTE_SDK)/drivers/raw/ifpga_rawdev
CFLAGS += -I$(RTE_SDK)/drivers/net/ipn3ke
LDLIBS += -lrte_eal
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_bus_ifpga

EXPORT_MAP := rte_pmd_ifpga_rawdev_version.map

LIBABIVER := 1

VPATH += $(SRCDIR)/base

include $(RTE_SDK)/drivers/raw/ifpga_rawdev/base/Makefile

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV) += ifpga_rawdev.c

include $(RTE_SDK)/mk/rte.lib.mk
