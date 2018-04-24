# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_latencystats.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lm
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_metrics -lrte_ethdev -lrte_mbuf

EXPORT_MAP := rte_latencystats_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_LATENCY_STATS) := rte_latencystats.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_LATENCY_STATS)-include := rte_latencystats.h

include $(RTE_SDK)/mk/rte.lib.mk
