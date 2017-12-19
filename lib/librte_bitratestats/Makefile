# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_bitratestats.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal -lrte_metrics -lrte_ethdev

EXPORT_MAP := rte_bitratestats_version.map

LIBABIVER := 2

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_BITRATE) := rte_bitrate.c

# Install header file
SYMLINK-$(CONFIG_RTE_LIBRTE_BITRATE)-include += rte_bitrate.h

include $(RTE_SDK)/mk/rte.lib.mk
