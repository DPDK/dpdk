# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_iavf.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -Wno-cast-qual

EXPORT_MAP := rte_common_iavf_version.map

#
# all source are stored in SRCS-y
#
SRCS-y += iavf_adminq.c
SRCS-y += iavf_common.c
SRCS-y += iavf_impl.c

LDLIBS += -lrte_eal

include $(RTE_SDK)/mk/rte.lib.mk
