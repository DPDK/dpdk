# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_bus_vdev.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_bus_vdev_version.map

SRCS-y += vdev.c
SRCS-y += vdev_params.c

LDLIBS += -lrte_eal -lrte_kvargs

#
# Export include files
#
SYMLINK-y-include += rte_bus_vdev.h

include $(RTE_SDK)/mk/rte.lib.mk
