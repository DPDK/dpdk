# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pipeline.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_table
LDLIBS += -lrte_port

EXPORT_MAP := rte_pipeline_version.map

LIBABIVER := 3

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := rte_pipeline.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_PIPELINE)-include += rte_pipeline.h

include $(RTE_SDK)/mk/rte.lib.mk
