# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_rawdev_ntb.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_rawdev

EXPORT_MAP := rte_rawdev_ntb_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NTB_RAWDEV) += ntb.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NTB_RAWDEV) += ntb_hw_intel.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_PMD_NTB_RAWDEV)-include := rte_pmd_ntb.h

include $(RTE_SDK)/mk/rte.lib.mk
