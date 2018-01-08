# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_skeleton_event.a

CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_eventdev
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_skeleton_event_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_EVENTDEV) += skeleton_eventdev.c

include $(RTE_SDK)/mk/rte.lib.mk
