# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_jobstats.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal

EXPORT_MAP := rte_jobstats_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_JOBSTATS) := rte_jobstats.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_JOBSTATS)-include := rte_jobstats.h

include $(RTE_SDK)/mk/rte.lib.mk
