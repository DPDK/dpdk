# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_rawdev_ifpga.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/ifpga
CFLAGS += -I$(RTE_SDK)/drivers/raw/ifpga_rawdev
CFLAGS += -I$(RTE_SDK)/drivers/net/i40e
CFLAGS += -I$(RTE_SDK)/drivers/net/ipn3ke
LDLIBS += -lrte_eal
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_bus_ifpga
LDLIBS += -lpthread
LDLIBS += -lfdt
LDLIBS += -lrte_pmd_i40e
LDLIBS += -lrte_pmd_ipn3ke

EXPORT_MAP := rte_rawdev_ifpga_version.map

VPATH += $(SRCDIR)/base

include $(RTE_SDK)/drivers/raw/ifpga/base/Makefile

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV) += ifpga_rawdev.c

include $(RTE_SDK)/mk/rte.lib.mk
