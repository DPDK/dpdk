# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_bus_ifpga.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_kvargs

# versioning export map
EXPORT_MAP := rte_bus_ifpga_version.map

# library version
LIBABIVER := 1

SRCS-$(CONFIG_RTE_LIBRTE_IFPGA_BUS) += ifpga_bus.c
SRCS-$(CONFIG_RTE_LIBRTE_IFPGA_BUS) += ifpga_common.c

#
# Export include files
#
SYMLINK-$(CONFIG_RTE_LIBRTE_IFPGA_BUS)-include += rte_bus_ifpga.h

include $(RTE_SDK)/mk/rte.lib.mk
