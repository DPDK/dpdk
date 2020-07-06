# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
# Copyright 2020 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_regexdev.a

EXPORT_MAP := rte_regex_version.map

# library version
LIBABIVER := 1

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf

# library source files
# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_REGEXDEV) := rte_regexdev.c

# export include files
SYMLINK-$(CONFIG_RTE_LIBRTE_REGEXDEV)-include += rte_regexdev.h
SYMLINK-$(CONFIG_RTE_LIBRTE_REGEXDEV)-include += rte_regexdev_core.h
SYMLINK-$(CONFIG_RTE_LIBRTE_REGEXDEV)-include += rte_regexdev_driver.h

# versioning export map
EXPORT_MAP := rte_regexdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
