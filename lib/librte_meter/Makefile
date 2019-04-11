# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_meter.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lm
LDLIBS += -lrte_eal

EXPORT_MAP := rte_meter_version.map

LIBABIVER := 3

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_METER) := rte_meter.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_METER)-include := rte_meter.h

include $(RTE_SDK)/mk/rte.lib.mk
