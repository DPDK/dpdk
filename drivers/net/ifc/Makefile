# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_ifcvf_vdpa.a

LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_pci -lrte_vhost -lrte_bus_pci

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

#
# Add extra flags for base driver source files to disable warnings in them
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))

VPATH += $(SRCDIR)/base

EXPORT_MAP := rte_ifcvf_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IFCVF_VDPA_PMD) += ifcvf_vdpa.c
SRCS-$(CONFIG_RTE_LIBRTE_IFCVF_VDPA_PMD) += ifcvf.c

include $(RTE_SDK)/mk/rte.lib.mk
