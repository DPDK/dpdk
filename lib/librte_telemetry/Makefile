# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_telemetry.a

ARCH_DIR ?= $(RTE_ARCH)

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
CFLAGS += -I$(RTE_SDK)/lib/librte_metrics/
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/$(ARCH_DIR)/include
CFLAGS += -pthread

LDLIBS += -lpthread

EXPORT_MAP := rte_telemetry_version.map

# library source files
SRCS-y += telemetry.c
SRCS-y += telemetry_data.c
SRCS-y += telemetry_legacy.c

# export include files
SYMLINK-y-include := rte_telemetry.h

include $(RTE_SDK)/mk/rte.lib.mk
