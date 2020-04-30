# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_rawdev.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_telemetry

# library source files
SRCS-y += rte_rawdev.c

# export include files
SYMLINK-y-include += rte_rawdev.h
SYMLINK-y-include += rte_rawdev_pmd.h

# versioning export map
EXPORT_MAP := rte_rawdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
