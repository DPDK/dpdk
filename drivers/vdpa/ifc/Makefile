# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ifc.a

LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_pci -lrte_vhost -lrte_bus_pci
LDLIBS += -lrte_kvargs

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# Add extra flags for base driver source files to disable warnings in them
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))

VPATH += $(SRCDIR)/base

EXPORT_MAP := rte_pmd_ifc_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IFC_PMD) += ifcvf_vdpa.c
SRCS-$(CONFIG_RTE_LIBRTE_IFC_PMD) += ifcvf.c

include $(RTE_SDK)/mk/rte.lib.mk
