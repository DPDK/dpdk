# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_flow_classify.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)

EXPORT_MAP := rte_flow_classify_version.map

LDLIBS += -lrte_eal -lrte_ethdev -lrte_net -lrte_table -lrte_acl

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY) += rte_flow_classify.c
SRCS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY) += rte_flow_classify_parse.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY)-include := rte_flow_classify.h

include $(RTE_SDK)/mk/rte.lib.mk
