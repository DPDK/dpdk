# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_bbdev.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf

# library source files
SRCS-y += rte_bbdev.c

# export include files
SYMLINK-y-include += rte_bbdev_op.h
SYMLINK-y-include += rte_bbdev.h
SYMLINK-y-include += rte_bbdev_pmd.h

# versioning export map
EXPORT_MAP := rte_bbdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
