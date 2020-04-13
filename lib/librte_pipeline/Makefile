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
LDLIBS += -lrte_port -lrte_meter -lrte_sched -lrte_cryptodev

EXPORT_MAP := rte_pipeline_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := rte_pipeline.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += rte_port_in_action.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += rte_table_action.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_PIPELINE)-include += rte_pipeline.h rte_port_in_action.h rte_table_action.h

include $(RTE_SDK)/mk/rte.lib.mk
