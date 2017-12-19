# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_metrics.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal

EXPORT_MAP := rte_metrics_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_METRICS) := rte_metrics.c

# Install header file
SYMLINK-$(CONFIG_RTE_LIBRTE_METRICS)-include += rte_metrics.h

include $(RTE_SDK)/mk/rte.lib.mk
