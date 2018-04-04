# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_octeontx.a

CFLAGS += $(WERROR_FLAGS)
EXPORT_MAP := rte_common_octeontx_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-y += octeontx_mbox.c

LDLIBS += -lrte_eal

include $(RTE_SDK)/mk/rte.lib.mk
