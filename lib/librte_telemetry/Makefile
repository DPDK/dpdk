# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_telemetry.a

CFLAGS += -O3
CFLAGS += -I$(SRCDIR)
CFLAGS += -DALLOW_EXPERIMENTAL_API

LDLIBS += -lrte_eal -lrte_ethdev
LDLIBS += -lrte_metrics
LDLIBS += -lpthread
LDLIBS += -ljansson

EXPORT_MAP := rte_telemetry_version.map

LIBABIVER := 1

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_TELEMETRY) := rte_telemetry.c
SRCS-$(CONFIG_RTE_LIBRTE_TELEMETRY) += rte_telemetry_parser.c
SRCS-$(CONFIG_RTE_LIBRTE_TELEMETRY) += rte_telemetry_parser_test.c

# export include files
SYMLINK-$(CONFIG_RTE_LIBRTE_TELEMETRY)-include := rte_telemetry.h

include $(RTE_SDK)/mk/rte.lib.mk
