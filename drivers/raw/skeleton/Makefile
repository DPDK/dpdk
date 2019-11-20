# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_rawdev_skeleton.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_kvargs

EXPORT_MAP := rte_rawdev_skeleton_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += skeleton_rawdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += skeleton_rawdev_test.c

include $(RTE_SDK)/mk/rte.lib.mk
