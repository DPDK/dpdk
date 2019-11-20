# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_member.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += $(WERROR_FLAGS) -O3

LDLIBS += -lm
LDLIBS += -lrte_eal -lrte_hash

EXPORT_MAP := rte_member_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MEMBER) +=  rte_member.c rte_member_ht.c rte_member_vbf.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MEMBER)-include := rte_member.h

include $(RTE_SDK)/mk/rte.lib.mk
