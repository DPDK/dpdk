# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_rawdev_ioat.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal -lrte_rawdev
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_mbuf -lrte_mempool

# versioning export map
EXPORT_MAP := rte_rawdev_ioat_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_IOAT_RAWDEV) += ioat_rawdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_IOAT_RAWDEV) += ioat_rawdev_test.c

# export include files
SYMLINK-y-include += rte_ioat_rawdev.h
SYMLINK-y-include += rte_ioat_spec.h

include $(RTE_SDK)/mk/rte.lib.mk
