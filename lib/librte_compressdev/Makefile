# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_compressdev.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_kvargs

# library source files
SRCS-y += rte_compressdev.c rte_compressdev_pmd.c rte_comp.c

# export include files
SYMLINK-y-include += rte_comp.h
SYMLINK-y-include += rte_compressdev.h
# export include files (for PMDs)
SYMLINK-y-include += rte_compressdev_pmd.h
SYMLINK-y-include += rte_compressdev_internal.h

# versioning export map
EXPORT_MAP := rte_compressdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
