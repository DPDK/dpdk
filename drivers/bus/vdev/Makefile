# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_bus_vdev.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

# versioning export map
EXPORT_MAP := rte_bus_vdev_version.map

# library version
LIBABIVER := 1

SRCS-y += vdev.c

LDLIBS += -lrte_eal

#
# Export include files
#
SYMLINK-y-include += rte_bus_vdev.h

include $(RTE_SDK)/mk/rte.lib.mk
